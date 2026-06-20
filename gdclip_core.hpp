#pragma once
#include <vector>
#include "clipper/clipper.hpp"

// Pure, Godot-free geometry core. This holds all the asteroid-breaking maths so
// it can be unit tested directly (see tests/), while gdclip.cpp keeps only the
// thin GDNative marshalling layer on top of it.
namespace gdclip
{
	// Clipper works on integers, so one pixel is mapped to SCALE integer units;
	// 1/SCALE is the achievable sub-pixel precision. Kept modest so coordinates
	// stay inside Clipper's fast (non-Int128) range even for a large play-field,
	// while still being smooth enough for round explosions.
	constexpr double SCALE = 256.0;

	// After clipping, merge away edges shorter than this many pixels (and the
	// near-collinear vertices Clipper leaves on straight runs). Removes the
	// micro-slivers boolean ops produce, which would otherwise become
	// degenerate, self-intersecting collision shapes.
	constexpr double CLEAN_DISTANCE_PX = 0.05;

	// Fragments with an absolute area below this (in px^2) are discarded: they
	// are pulverised dust, not playable asteroid pieces.
	constexpr double MIN_AREA_PX2 = 1.0;

	// A surviving fragment: an outer outline plus zero or more holes. Both are in
	// the fixed-point integer space (multiply pixels by SCALE / divide to undo).
	struct Piece
	{
		ClipperLib::Path outline;
		ClipperLib::Paths holes;
	};

	// Convert a pixel coordinate to/from the fixed-point integer grid. ToFixed
	// rounds (never truncates) so sub-pixel detail survives the round trip.
	ClipperLib::cInt ToFixed(double v);
	double FromFixed(ClipperLib::cInt v);

	// Subtract the explosions from the asteroid and return the surviving pieces.
	//
	//   subject : the asteroid - a set of contours whose nesting defines holes
	//             (outline > hole > island > ...), resolved with even-odd fill.
	//   clip    : the explosions - each contour an independent solid blast; they
	//             are unioned, so overlapping blasts destroy their overlap rather
	//             than leaving it intact.
	//
	// Slivers are cleaned and sub-MIN_AREA dust is dropped, so every returned
	// piece is a valid, simple polygon. A returned Piece can be turned straight
	// back into a `subject` for the next hit.
	std::vector<Piece> Difference(const ClipperLib::Paths &subject,
	                              const ClipperLib::Paths &clip);
}
