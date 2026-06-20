#include <gdnative_api_struct.gen.h>
#include <cmath>
#include "clipper/clipper.hpp"
#include "gdclip.h"

namespace
{
	// Clipper works on integers, so one Godot unit (pixel) is mapped to SCALE
	// integer units; 1/SCALE is the achievable sub-pixel precision. Kept modest
	// so coordinates stay inside Clipper's fast (non-Int128) range even for a
	// large play-field, while still being smooth enough for round explosions.
	constexpr double SCALE = 256.0;

	// After clipping, merge away edges shorter than this many pixels (and the
	// near-collinear vertices Clipper leaves on straight runs). This removes the
	// micro-slivers boolean ops produce, which would otherwise turn into
	// degenerate, self-intersecting collision shapes.
	constexpr double CLEAN_DISTANCE_PX = 0.05;

	// Fragments with an absolute area below this (in px^2) are discarded: they
	// are pulverised dust, not playable asteroid pieces.
	constexpr double MIN_AREA_PX2 = 1.0;

	const double CLEAN_DISTANCE = CLEAN_DISTANCE_PX * SCALE;
	const double MIN_AREA = MIN_AREA_PX2 * SCALE * SCALE;

	ClipperLib::IntPoint ToPoint(const godot_vector2 *v)
	{
		// Round (not truncate) after scaling so sub-pixel detail is preserved.
		const double x = api->godot_vector2_get_x(v);
		const double y = api->godot_vector2_get_y(v);
		return ClipperLib::IntPoint{
			(ClipperLib::cInt)std::llround(x * SCALE),
			(ClipperLib::cInt)std::llround(y * SCALE) };
	}

	void PathFromPool(const godot_pool_vector2_array *items, ClipperLib::Path &path)
	{
		const godot_int size = api->godot_pool_vector2_array_size(items);
		path.clear();
		path.reserve(size);
		for (godot_int i = 0; i < size; ++i)
		{
			const godot_vector2 item = api->godot_pool_vector2_array_get(items, i);
			path.push_back(ToPoint(&item));
		}
	}

	// Append the polygon(s) held by `arg` to `clipper` as one region. The first
	// path is the outer outline (forced to positive orientation), any further
	// paths are holes (forced to negative orientation). Forcing the winding makes
	// the region unambiguous regardless of what order the caller wound its
	// points in, and pairs with pftNonZero in Difference() to give exactly
	// "outline minus holes".
	void AddRegion(ClipperLib::Clipper &clipper, const godot_variant *arg, ClipperLib::PolyType type)
	{
		ClipperLib::Paths paths;

		if (api->godot_variant_get_type(arg) == GODOT_VARIANT_TYPE_ARRAY)
		{
			godot_array arr = api->godot_variant_as_array(arg);
			const godot_int n = api->godot_array_size(&arr);
			for (godot_int i = 0; i < n; ++i)
			{
				godot_variant e = api->godot_array_get(&arr, i);
				godot_pool_vector2_array pa = api->godot_variant_as_pool_vector2_array(&e);

				ClipperLib::Path p;
				PathFromPool(&pa, p);
				paths.push_back(p);

				api->godot_pool_vector2_array_destroy(&pa);
				api->godot_variant_destroy(&e);
			}
			api->godot_array_destroy(&arr);
		}
		else
		{
			godot_pool_vector2_array pa = api->godot_variant_as_pool_vector2_array(arg);

			ClipperLib::Path p;
			PathFromPool(&pa, p);
			paths.push_back(p);

			api->godot_pool_vector2_array_destroy(&pa);
		}

		for (size_t i = 0; i < paths.size(); ++i)
		{
			ClipperLib::Path &p = paths[i];
			if (p.size() < 3)
				continue; // a line or a point bounds no area

			const bool wantPositive = (i == 0); // outline solid, the rest holes
			if (ClipperLib::Orientation(p) != wantPositive)
				ClipperLib::ReversePath(p);

			clipper.AddPath(p, type, true);
		}
	}

	void EmitPath(const ClipperLib::Path &path, godot_pool_vector2_array &out)
	{
		api->godot_pool_vector2_array_resize(&out, (godot_int)path.size());
		for (size_t i = 0; i < path.size(); ++i)
		{
			godot_vector2 v;
			api->godot_vector2_new(&v,
				(godot_real)(path[i].X / SCALE),
				(godot_real)(path[i].Y / SCALE));
			api->godot_pool_vector2_array_set(&out, (godot_int)i, &v);
		}
	}

	void PushPool(godot_array &arr, godot_pool_vector2_array &pool)
	{
		godot_variant v;
		api->godot_variant_new_pool_vector2_array(&v, &pool);
		api->godot_array_push_back(&arr, &v);
		api->godot_variant_destroy(&v);
	}

	bool KeepPath(ClipperLib::Path &path)
	{
		ClipperLib::CleanPolygon(path, CLEAN_DISTANCE);
		return path.size() >= 3 && std::fabs(ClipperLib::Area(path)) >= MIN_AREA;
	}

	// Turn one solid PolyTree node into a result piece: its contour is the
	// outline, its direct children are holes. A hole may in turn contain solid
	// "islands" (a chunk left floating where the matter around it was blown
	// away) - those become their own independent pieces, so the asteroid really
	// does break apart wherever it is severed.
	void EmitPiece(const ClipperLib::PolyNode *solid, godot_array &result)
	{
		ClipperLib::Path outline = solid->Contour;
		const bool keep = KeepPath(outline);

		godot_array piece;
		if (keep)
		{
			api->godot_array_new(&piece);

			godot_pool_vector2_array outPool;
			api->godot_pool_vector2_array_new(&outPool);
			EmitPath(outline, outPool);
			PushPool(piece, outPool);
			api->godot_pool_vector2_array_destroy(&outPool);
		}

		for (int i = 0; i < solid->ChildCount(); ++i)
		{
			const ClipperLib::PolyNode *hole = solid->Childs[i];

			if (keep)
			{
				ClipperLib::Path hpath = hole->Contour;
				if (KeepPath(hpath))
				{
					godot_pool_vector2_array hPool;
					api->godot_pool_vector2_array_new(&hPool);
					EmitPath(hpath, hPool);
					PushPool(piece, hPool);
					api->godot_pool_vector2_array_destroy(&hPool);
				}
			}

			// Islands floating inside this hole are separate fragments.
			for (int j = 0; j < hole->ChildCount(); ++j)
				EmitPiece(hole->Childs[j], result);
		}

		if (keep)
		{
			godot_variant pv;
			api->godot_variant_new_array(&pv, &piece);
			api->godot_array_push_back(&result, &pv);
			api->godot_variant_destroy(&pv);
			api->godot_array_destroy(&piece);
		}
	}
}

godot_array Difference(const godot_variant *subject, const godot_variant *clip)
{
	godot_array result;
	api->godot_array_new(&result);

	ClipperLib::PolyTree tree;
	{
		ClipperLib::Clipper clipper;
		AddRegion(clipper, subject, ClipperLib::ptSubject);
		AddRegion(clipper, clip, ClipperLib::ptClip);

		// PolyTree (not Paths) so the outline/hole/island nesting is preserved;
		// pftNonZero so the forced winding from AddRegion means "outline minus
		// holes" for both operands.
		clipper.Execute(ClipperLib::ctDifference, tree,
			ClipperLib::pftNonZero, ClipperLib::pftNonZero);
	}

	// Every top-level node of a Difference result is a solid outer contour.
	for (int i = 0; i < tree.ChildCount(); ++i)
		EmitPiece(tree.Childs[i], result);

	return result;
}
