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

Both `subject` (the asteroid) and `clip` (the explosion) accept either form:

* a `PoolVector2Array` — a single solid outline, or
* an `Array` of `PoolVector2Array` — an outline followed by its holes.

Winding order does not matter; the library forces the first ring solid and the
rest into holes for you.

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
(not truncated) on the way in, giving ~1/256 px precision. Keep play-field
coordinates within roughly ±32 million units to stay inside Clipper's fast
arithmetic range.

## Building

The repository ships a Visual Studio solution (`gdclip.sln`) targeting Windows.
The output `libgdclip.dll` is referenced by `libgdclip.gdnlib`. The two source
files of interest are `gdclip.cpp` (the geometry) and `gdclip_c.cpp` (the
GDNative class registration and `diff` entry point).
