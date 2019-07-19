#pragma once
#include <gdnative_api_struct.gen.h>

extern const godot_gdnative_core_api_struct *api;
extern const godot_gdnative_ext_nativescript_api_struct *nativescript_api;

godot_array Difference(godot_pool_vector2_array subject, godot_pool_vector2_array clip);