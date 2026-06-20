#include "gdclip_core.hpp"
#include <cmath>

namespace gdclip
{
	namespace
	{
		const double CLEAN_DISTANCE = CLEAN_DISTANCE_PX * SCALE;
		const double MIN_AREA = MIN_AREA_PX2 * SCALE * SCALE;

		// Clean a contour in place and report whether it is still worth keeping
		// (a simple polygon with at least a sliver of real area).
		bool KeepPath(ClipperLib::Path &path)
		{
			ClipperLib::CleanPolygon(path, CLEAN_DISTANCE);
			return path.size() >= 3 && std::fabs(ClipperLib::Area(path)) >= MIN_AREA;
		}

		// Turn one solid PolyTree node into a result piece: its contour is the
		// outline, its direct children are holes. A hole may in turn contain solid
		// "islands" (a chunk left floating where the matter around it was blown
		// away) - those become their own independent pieces, so the asteroid
		// really does break apart wherever it is severed.
		void EmitPiece(const ClipperLib::PolyNode *solid, std::vector<Piece> &out)
		{
			Piece piece;
			piece.outline = solid->Contour;
			const bool keep = KeepPath(piece.outline);

			for (int i = 0; i < solid->ChildCount(); ++i)
			{
				const ClipperLib::PolyNode *hole = solid->Childs[i];

				if (keep)
				{
					ClipperLib::Path hpath = hole->Contour;
					if (KeepPath(hpath))
						piece.holes.push_back(hpath);
				}

				// Islands floating inside this hole are separate fragments.
				for (int j = 0; j < hole->ChildCount(); ++j)
					EmitPiece(hole->Childs[j], out);
			}

			if (keep)
				out.push_back(piece);
		}
	}

	ClipperLib::cInt ToFixed(double v)
	{
		// Round (not truncate) after scaling so sub-pixel detail is preserved.
		return (ClipperLib::cInt)std::llround(v * SCALE);
	}

	double FromFixed(ClipperLib::cInt v)
	{
		return (double)v / SCALE;
	}

	std::vector<Piece> Difference(const ClipperLib::Paths &subject,
	                              const ClipperLib::Paths &clip)
	{
		ClipperLib::PolyTree tree;
		{
			ClipperLib::Clipper clipper;

			// Asteroid: nesting defines holes (outline > hole > island > ...).
			// Even-odd fill resolves that nesting at any depth without the caller
			// having to label or wind anything a particular way.
			for (size_t i = 0; i < subject.size(); ++i)
				if (subject[i].size() >= 3)
					clipper.AddPath(subject[i], ClipperLib::ptSubject, true);

			// Explosions: each contour is its own solid blast. They must be
			// unioned, not nested, so two overlapping blasts destroy their overlap
			// instead of leaving it intact. Force every blast to the same winding
			// and use non-zero fill, which is exactly the union of all of them.
			for (size_t i = 0; i < clip.size(); ++i)
			{
				if (clip[i].size() < 3)
					continue;
				ClipperLib::Path p = clip[i];
				if (!ClipperLib::Orientation(p))
					ClipperLib::ReversePath(p);
				clipper.AddPath(p, ClipperLib::ptClip, true);
			}

			// PolyTree (not Paths) so the outline/hole/island nesting of the
			// result is preserved for EmitPiece to walk.
			clipper.Execute(ClipperLib::ctDifference, tree,
				ClipperLib::pftEvenOdd, ClipperLib::pftNonZero);
		}

		std::vector<Piece> out;
		// Every top-level node of a Difference result is a solid outer contour.
		for (int i = 0; i < tree.ChildCount(); ++i)
			EmitPiece(tree.Childs[i], out);
		return out;
	}
}
