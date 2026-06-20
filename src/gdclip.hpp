#pragma once

#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/variant.hpp>

namespace godot {

// GDExtension class exposed to Godot 4 as `GDClip`. It is the thin marshalling
// layer over the engine-free gdclip geometry core: convert Godot polygons to and
// from Clipper paths and delegate the actual asteroid-breaking to gdclip::Difference.
class GDClip : public RefCounted {
	GDCLASS(GDClip, RefCounted)

protected:
	static void _bind_methods();

public:
	// Subtract `clip` (the explosions) from `subject` (the asteroid).
	//
	// Each argument is either a PackedVector2Array (a single polygon) or an
	// Array of PackedVector2Array (several polygons). The asteroid's contours
	// define holes by nesting; the explosions are each a solid blast and are
	// unioned. Returns an Array of pieces; each piece is an Array whose first
	// element is the outer outline and whose remaining elements are holes. A
	// returned piece can be fed straight back in as `subject` for the next hit.
	Array diff(const Variant &subject, const Variant &clip) const;
};

} // namespace godot
