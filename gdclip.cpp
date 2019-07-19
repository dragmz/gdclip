#include <gdnative_api_struct.gen.h>
#include "clipper/clipper.hpp"
#include "gdclip.h"

static void ToClipper(godot_pool_vector2_array items, ClipperLib::Path& Path)
{
	const auto size = api->godot_pool_vector2_array_size(&items);
	Path.resize(size);

	for (godot_int i{}; i < size; ++i)
	{
		const auto item = api->godot_pool_vector2_array_get(&items, i);
		const auto x = api->godot_vector2_get_x(&item);
		const auto y = api->godot_vector2_get_y(&item);

		Path[i] = ClipperLib::IntPoint{ (ClipperLib::long64)(x * 10000), (ClipperLib::long64)(y * 10000) };
	}
}

static void FromClipper(ClipperLib::Path& Path, godot_pool_vector2_array& items)
{
	const auto tsize = Path.size();

	api->godot_pool_vector2_array_resize(&items, tsize);

	for (size_t j{}; j < tsize; ++j)
	{
		godot_vector2 v{};
		const auto pt = Path[j];
		api->godot_vector2_new(&v, pt.X / 10000.0f, pt.Y / 10000.0f);

		api->godot_pool_vector2_array_set(&items, j, &v);
	}
}

godot_array Difference(godot_pool_vector2_array subject, godot_pool_vector2_array clip)
{
	ClipperLib::PolyTree T{};
	{
		ClipperLib::Path S{};
		ClipperLib::Path C{};

		ToClipper(subject, S);
		ToClipper(clip, C);

		ClipperLib::Clipper Clipper{};
		Clipper.AddPath(S, ClipperLib::ptSubject, true);
		Clipper.AddPath(C, ClipperLib::ptClip, true);

		Clipper.Execute(ClipperLib::ctDifference, T);
	}


	const auto count = T.ChildCount();

	size_t holes{};
	for (int i{}; i < count; ++i)
		if (T.Childs[i]->IsHole())
			++holes;

	const auto nonholes = count - holes;

	godot_array result;
	api->godot_array_new(&result);
	api->godot_array_resize(&result, nonholes);

	godot_int nonhole{};
	for (size_t i{}; i < nonholes; ++i)
	{
		const auto ti = T.Childs[i];
		if (ti->IsHole())
			continue;

		godot_pool_vector2_array item{};
		api->godot_pool_vector2_array_new(&item);

		FromClipper(T.Childs[i]->Contour, item);

		godot_variant vitem{};

		api->godot_variant_new_pool_vector2_array(&vitem, &item);
		api->godot_array_set(&result, nonhole, &vitem);

		api->godot_variant_destroy(&vitem);
		api->godot_pool_vector2_array_destroy(&item);

		++nonhole;
	}

	return result;
}
