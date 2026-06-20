# gdclip

A small **Godot 4 GDExtension** that wraps the [Clipper](http://www.angusj.com/delphi/clipper.php)
polygon-clipping library to do one job well: **subtract one polygon region from
another and return the resulting fragments.** It is built for destructible 2D
geometry — carving explosion craters out of asteroids and letting them break
into pieces.

Built with [godot-cpp](https://github.com/godotengine/godot-cpp) (vendored as a
submodule). The geometry itself lives in an engine-free C++ core
(`gdclip_core`), which is what makes it testable without Godot and trivial to
re-bind to other engines.

## The `GDClip.diff` method

```gdscript
var clipper := GDClip.new()
var pieces := clipper.diff(subject, clip)   # -> Array of pieces
```

`GDClip` is a `RefCounted`, so you do not need to free it manually.

### Inputs

Both `subject` (the asteroid) and `clip` (the explosions) accept either form:

* a `PackedVector2Array` — a single polygon, or
* an `Array` of `PackedVector2Array` — several polygons.

Winding order never matters. The two operands are interpreted differently
because they mean different things, which is the whole point:

* **`subject` — the asteroid.** Its polygons form one region where *nesting*
  defines holes: an outline contains a hole, a hole may contain a solid island,
  and so on to any depth. Pass a single outline, or `[outline, hole, hole, …]`.
  The contours should be *simple* (non-self-intersecting); a self-crossing
  outline would, under even-odd fill, sprout a phantom hole at the crossing.
  Pieces returned by `diff` are always simple, so re-feeding them is safe.
* **`clip` — the explosions.** Each polygon is an independent solid blast, and
  they are **unioned**. Two overlapping blasts therefore destroy their overlap
  (all matter in either blast is gone); they do *not* punch a hole in each
  other. Pass one blast, or `[blast, blast, …]` to apply many hits at once.

### Output

An `Array` of **pieces**. Each piece is itself an `Array` of `PackedVector2Array`:

* element `0` — the outer outline of the fragment
* elements `1..n` — holes inside that fragment (if any)

Because a piece has the exact same shape as the `Array`-form input, you can feed
any returned piece straight back in as the `subject` of the next hit, so damage
accumulates correctly across repeated explosions.

What you get back depends on the geometry, with no special-casing needed:

| Situation | Result |
| --- | --- |
| Explosion bites the edge | one piece, no hole |
| Explosion fully inside | one piece with a hole (a donut) |
| Explosion cuts clean through | two (or more) separate pieces |
| A chunk is severed all around | the chunk comes back as its own piece |
| Several blasts at once | each carves its own crater in one call |
| Overlapping blasts | their union is removed, overlap and all |
| Asteroid fully engulfed | empty array (destroyed) |

Pulverised slivers (area `< 1 px²`) are dropped, and the micro-fragments boolean
ops leave behind are cleaned up, so every returned piece is a usable collision
shape.

## Example (GDScript, Godot 4)

```gdscript
var clipper := GDClip.new()

# `asteroid` is an Array of PackedVector2Array: [outline, hole, hole, ...].
# `at` is the impact point in the asteroid's local space, `radius` the blast size.
func explode(asteroid: Array, at: Vector2, radius: float) -> Array:
    var blast := PackedVector2Array()
    var segments := 16
    for i in range(segments):
        var a := TAU * i / segments
        blast.append(at + Vector2(cos(a), sin(a)) * radius)

    # -> Array of pieces, each piece an Array of PackedVector2Array.
    return clipper.diff(asteroid, blast)

func _on_hit(asteroid_node: Node2D, impact_point: Vector2, radius: float) -> void:
    var pieces := explode(asteroid_node.rings, asteroid_node.to_local(impact_point), radius)
    asteroid_node.queue_free()
    for piece in pieces:
        spawn_asteroid(piece)   # piece[0] = outline, piece[1..] = holes

func spawn_asteroid(rings: Array) -> void:
    var poly := Polygon2D.new()
    poly.polygon = rings[0]            # outline
    if rings.size() > 1:
        # Polygon2D renders holes via the `polygons` index lists.
        var pts := PackedVector2Array(rings[0])
        var idx := []
        idx.append(range(rings[0].size()))
        for h in range(1, rings.size()):
            var start := pts.size()
            pts.append_array(rings[h])
            idx.append(range(start, pts.size()))
        poly.polygon = pts
        poly.polygons = idx
    # ... add a CollisionPolygon2D per ring for physics, give it velocity, etc.
```

## Coordinate precision

Clipper works on integers, so coordinates are scaled by `256` and **rounded**
(not truncated) on the way in, giving ~1/256 px precision. Coordinates up to
roughly ±4 million px stay on Clipper's fast arithmetic path (its `loRange` of
2^30 ÷ 256); larger play-fields still compute correctly but fall back to slower
128-bit math, and only become invalid past ~1.8e16 px. For an Asteroids-style
game none of this is a concern.

## Building

You need the godot-cpp submodule and SCons (the godot-cpp build system):

```sh
git submodule update --init --recursive
scons                          # template_debug -> demo/bin/, what the editor loads
scons target=template_release  # for an exported game
```

The shared library is written into `demo/bin/` with godot-cpp's platform/target
suffix, matching the paths in `demo/gdclip.gdextension`. Drop that
`.gdextension` file and the built library into your own project's `res://` to
use `GDClip` there.

Source layout:

* `gdclip_core.{hpp,cpp}` — the pure, engine-free geometry (the asteroid maths).
* `clipper/` — the vendored Clipper polygon library.
* `src/gdclip.{hpp,cpp}` — the `GDClip` GDExtension class: marshals Godot
  polygons to and from the core's Clipper paths.
* `src/register_types.cpp` — GDExtension entry point and class registration.
* `demo/` — a minimal Godot 4 project that loads the extension (used by the
  integration tests).

## Tests

Coverage comes in two layers.

**1. Engine-free unit tests** (`tests/test_gdclip.cpp`) drive the geometry core
(`gdclip::Difference`) directly — no Godot needed. They cover interior hits
(holes), edge bites, slicing into many pieces, multiple and overlapping blasts
(union), severed cores becoming islands, deep nesting, multiple holes, winding
independence, the round-not-truncate precision fix, the no-dust invariant, and
degenerate inputs.

```sh
tests/run_tests.sh                 # direct compile + run (Linux/macOS)
tests\run_tests.bat                # same, MSVC (Developer Command Prompt)

cmake -S tests -B tests/build      # or via CMake/CTest
cmake --build tests/build
ctest --test-dir tests/build --output-on-failure
```

**2. Integration tests** (`demo/tests/run_tests.gd`) call `GDClip.diff` through
a real headless Godot 4 engine, covering the marshalling layer the C++ tests
cannot reach: `Variant` dispatch (single `PackedVector2Array` vs `Array` of
them), the float↔fixed round trip, and the nested `[outline, holes…]` result
shape — plus every destruction behaviour end to end.

```sh
git submodule update --init --recursive
GODOT=/path/to/Godot_v4.3-stable_linux.x86_64 tests/run_integration_tests.sh
```
