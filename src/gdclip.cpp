#include "gdclip.hpp"

#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/packed_vector2_array.hpp>
#include <godot_cpp/variant/vector2.hpp>

#include "gdclip_core.hpp"
#include "clipper/clipper.hpp"

using namespace godot;

namespace {

ClipperLib::Path path_from_packed(const PackedVector2Array &pa) {
	ClipperLib::Path p;
	const int64_t n = pa.size();
	p.reserve(n);
	for (int64_t i = 0; i < n; ++i) {
		const Vector2 v = pa[i];
		p.push_back(ClipperLib::IntPoint(gdclip::ToFixed(v.x), gdclip::ToFixed(v.y)));
	}
	return p;
}

// Pull every contour out of an operand. An operand is either a single
// PackedVector2Array or an Array of PackedVector2Array. The solid-vs-hole and
// winding decisions belong to the core, not here.
void collect_contours(const Variant &arg, ClipperLib::Paths &out) {
	if (arg.get_type() == Variant::ARRAY) {
		const Array arr = arg;
		for (int64_t i = 0; i < arr.size(); ++i) {
			const PackedVector2Array pa = arr[i];
			ClipperLib::Path p = path_from_packed(pa);
			if (p.size() >= 3) { // a line or a point bounds no area
				out.push_back(p);
			}
		}
	} else {
		const PackedVector2Array pa = arg;
		ClipperLib::Path p = path_from_packed(pa);
		if (p.size() >= 3) {
			out.push_back(p);
		}
	}
}

PackedVector2Array packed_from_path(const ClipperLib::Path &path) {
	PackedVector2Array out;
	out.resize((int64_t)path.size());
	for (size_t i = 0; i < path.size(); ++i) {
		out[(int64_t)i] = Vector2(
				(real_t)gdclip::FromFixed(path[i].X),
				(real_t)gdclip::FromFixed(path[i].Y));
	}
	return out;
}

} // namespace

void GDClip::_bind_methods() {
	ClassDB::bind_method(D_METHOD("diff", "subject", "clip"), &GDClip::diff);
}

Array GDClip::diff(const Variant &subject, const Variant &clip) const {
	ClipperLib::Paths subject_paths;
	ClipperLib::Paths clip_paths;
	collect_contours(subject, subject_paths);
	collect_contours(clip, clip_paths);

	const std::vector<gdclip::Piece> pieces = gdclip::Difference(subject_paths, clip_paths);

	Array result;
	for (size_t i = 0; i < pieces.size(); ++i) {
		const gdclip::Piece &p = pieces[i];

		Array piece; // [outline, hole, hole, ...]
		piece.push_back(packed_from_path(p.outline));
		for (size_t h = 0; h < p.holes.size(); ++h) {
			piece.push_back(packed_from_path(p.holes[h]));
		}
		result.push_back(piece);
	}
	return result;
}
