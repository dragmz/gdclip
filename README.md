# gdclip

A small Godot GDNative library that wraps the [Clipper](http://www.angusj.com/delphi/clipper.php)
polygon-clipping library to do one job well: **subtract one polygon from another
and return the resulting fragments.** It is built for destructible 2D geometry —
carving explosion craters out of asteroids and letting them break into pieces.

## The `diff` method

```
GDCLIP.diff(subject, clip) -> Array
```

### Inputs

Both `subject` (the asteroid) and `clip` (the explosions) accept either form:

* a `PoolVector2Array` — a single polygon, or
* an `Array` of `PoolVector2Array` — several polygons.

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

An `Array` of **pieces**. Each piece is itself an `Array` of `PoolVector2Array`:

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

## Example (GDScript)

```gdscript
var clipper = preload("res://GDCLIP.gdns").new()

# `asteroid` is an Array of PoolVector2Array: [outline, hole, hole, ...].
# `at` is the impact point in the asteroid's local space, `radius` the blast size.
func explode(asteroid: Array, at: Vector2, radius: float) -> Array:
    var blast := PoolVector2Array()
    var segments := 16
    for i in range(segments):
        var a := TAU * i / segments
        blast.append(at + Vector2(cos(a), sin(a)) * radius)

    # -> Array of pieces, each piece an Array of PoolVector2Array.
    return clipper.diff(asteroid, blast)

func _on_hit(asteroid_node, impact_point, radius):
    var pieces = explode(asteroid_node.rings, asteroid_node.to_local(impact_point), radius)
    asteroid_node.queue_free()
    for piece in pieces:
        spawn_asteroid(piece)   # piece[0] = outline, piece[1..] = holes

func spawn_asteroid(rings: Array):
    var poly := Polygon2D.new()
    poly.polygon = rings[0]            # outline
    if rings.size() > 1:
        # Polygon2D renders holes via the `polygons` index lists.
        var pts := PoolVector2Array(rings[0])
        var idx := []
        var outline_idx := range(rings[0].size())
        idx.append(outline_idx)
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

The repository ships a Visual Studio solution (`gdclip.sln`) targeting Windows.
The output `libgdclip.dll` is referenced by `libgdclip.gdnlib`. The source files
are:

* `gdclip_core.{hpp,cpp}` — the pure, Godot-free geometry (the asteroid maths).
* `gdclip.cpp` — the GDNative marshalling that converts Godot polygons to and
  from the core's Clipper paths.
* `gdclip_c.cpp` — GDNative class registration and the `diff` entry point.

## Tests

The geometry lives in `gdclip_core` precisely so it can be unit tested without a
Godot runtime. The tests in `tests/test_gdclip.cpp` call the real
`gdclip::Difference` and cover every destruction case: interior hits (holes),
edge bites, slicing into pieces, multiple and overlapping blasts (union),
severed cores becoming islands, re-hitting an already-holed asteroid, sub-pixel
precision, and degenerate inputs.

Run them any of these ways (no external test framework required):

```sh
tests/run_tests.sh                 # direct compile + run (Linux/macOS)
tests\run_tests.bat                # same, MSVC (Developer Command Prompt)

cmake -S tests -B tests/build      # or via CMake/CTest
cmake --build tests/build
ctest --test-dir tests/build --output-on-failure
```
