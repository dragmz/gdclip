#pragma once
#include <gdnative_api_struct.gen.h>

extern const godot_gdnative_core_api_struct *api;
extern const godot_gdnative_ext_nativescript_api_struct *nativescript_api;

// Subtract the `clip` region (an explosion) from the `subject` region (an
// asteroid) and return the resulting fragments.
//
// Each argument may be either:
//   * a PoolVector2Array            - a single solid outline, or
//   * an Array of PoolVector2Array  - an outline followed by its holes.
//
// The return value is an Array of "pieces". Each piece is itself an Array of
// PoolVector2Array whose first element is the outer outline and whose remaining
// elements (if any) are holes. A piece can be fed straight back in as `subject`
// for the next hit, so destruction accumulates correctly across multiple hits.
godot_array Difference(const godot_variant *subject, const godot_variant *clip);
