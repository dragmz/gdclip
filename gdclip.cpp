#include <gdnative_api_struct.gen.h>
#include <vector>
#include "clipper/clipper.hpp"
#include "gdclip_core.hpp"
#include "gdclip.h"

// Thin GDNative marshalling layer: convert Godot polygons <-> Clipper paths and
// delegate the actual geometry to the (unit-tested) gdclip core.
namespace
{
	ClipperLib::IntPoint ToPoint(const godot_vector2 *v)
	{
		return ClipperLib::IntPoint{
			gdclip::ToFixed(api->godot_vector2_get_x(v)),
			gdclip::ToFixed(api->godot_vector2_get_y(v)) };
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

	// Pull every contour out of an operand. An operand may be a single
	// PoolVector2Array (one polygon) or an Array of PoolVector2Array (several
	// polygons). No winding/solid-vs-hole decision is made here; that is the
	// core's job and depends on whether the operand is the asteroid or the
	// explosions (see gdclip::Difference).
	void CollectContours(const godot_variant *arg, ClipperLib::Paths &out)
	{
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
				if (p.size() >= 3) // a line or a point bounds no area
					out.push_back(p);

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
			if (p.size() >= 3)
				out.push_back(p);

			api->godot_pool_vector2_array_destroy(&pa);
		}
	}

	void EmitPath(const ClipperLib::Path &path, godot_pool_vector2_array &out)
	{
		api->godot_pool_vector2_array_resize(&out, (godot_int)path.size());
		for (size_t i = 0; i < path.size(); ++i)
		{
			godot_vector2 v;
			api->godot_vector2_new(&v,
				(godot_real)gdclip::FromFixed(path[i].X),
				(godot_real)gdclip::FromFixed(path[i].Y));
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

	void PushPathAsPool(godot_array &piece, const ClipperLib::Path &path)
	{
		godot_pool_vector2_array pool;
		api->godot_pool_vector2_array_new(&pool);
		EmitPath(path, pool);
		PushPool(piece, pool);
		api->godot_pool_vector2_array_destroy(&pool);
	}
}

godot_array Difference(const godot_variant *subject, const godot_variant *clip)
{
	ClipperLib::Paths subjectPaths;
	ClipperLib::Paths clipPaths;
	CollectContours(subject, subjectPaths);
	CollectContours(clip, clipPaths);

	const std::vector<gdclip::Piece> pieces = gdclip::Difference(subjectPaths, clipPaths);

	godot_array result;
	api->godot_array_new(&result);

	for (size_t i = 0; i < pieces.size(); ++i)
	{
		const gdclip::Piece &p = pieces[i];

		// Each piece is [outline, hole, hole, ...].
		godot_array piece;
		api->godot_array_new(&piece);

		PushPathAsPool(piece, p.outline);
		for (size_t h = 0; h < p.holes.size(); ++h)
			PushPathAsPool(piece, p.holes[h]);

		godot_variant pv;
		api->godot_variant_new_array(&pv, &piece);
		api->godot_array_push_back(&result, &pv);
		api->godot_variant_destroy(&pv);
		api->godot_array_destroy(&piece);
	}

	return result;
}
