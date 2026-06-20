extends SceneTree

# Integration tests for the gdclip GDExtension, run inside a real Godot 4 engine
# (headless). These exercise the marshalling layer that the engine-free C++ unit
# tests cannot reach: Variant dispatch (single PackedVector2Array vs Array of
# them), the float<->fixed round trip, and the nested [outline, holes...] result
# shape - plus every destruction behaviour, end to end through GDClip.diff().

var checks := 0
var failures := 0

func check(name: String, cond: bool) -> void:
	checks += 1
	if cond:
		print("  [PASS] ", name)
	else:
		failures += 1
		print("  [FAIL] ", name)

func circle(cx: float, cy: float, r: float, n: int = 32) -> PackedVector2Array:
	var p := PackedVector2Array()
	for i in range(n):
		var a := TAU * i / float(n)
		p.append(Vector2(cx + r * cos(a), cy + r * sin(a)))
	return p

func rect(x0: float, y0: float, x1: float, y1: float) -> PackedVector2Array:
	return PackedVector2Array([Vector2(x0, y0), Vector2(x1, y0), Vector2(x1, y1), Vector2(x0, y1)])

func poly_area(p: PackedVector2Array) -> float:
	var a := 0.0
	var n := p.size()
	for i in range(n):
		var q := p[(i + 1) % n]
		a += p[i].x * q.y - q.x * p[i].y
	return absf(a) * 0.5

func total_area(pieces: Array) -> float:
	var a := 0.0
	for piece in pieces:
		a += poly_area(piece[0])
		for h in range(1, piece.size()):
			a -= poly_area(piece[h])
	return a

func hole_count(pieces: Array) -> int:
	var h := 0
	for piece in pieces:
		h += piece.size() - 1
	return h

func is_packed_v2(v) -> bool:
	return typeof(v) == TYPE_PACKED_VECTOR2_ARRAY

func _initialize() -> void:
	print("gdclip GDExtension tests (Godot ", Engine.get_version_info().string, ")")

	check("GDClip class registered", ClassDB.class_exists("GDClip"))
	var g = ClassDB.instantiate("GDClip")
	check("GDClip instantiable", g != null)
	if g == null:
		quit(1)
		return

	var ast := circle(100, 100, 50)
	var ast_area := poly_area(ast)

	# --- result shape / marshalling (the reason this layer needs the engine) ---
	var r: Array = g.diff(ast, circle(100, 100, 15))
	check("result is Array", typeof(r) == TYPE_ARRAY)
	check("interior hit -> 1 piece", r.size() == 1)
	check("piece is Array", r.size() == 1 and typeof(r[0]) == TYPE_ARRAY)
	check("piece = outline + 1 hole", r.size() == 1 and r[0].size() == 2)
	check("outline is PackedVector2Array", r.size() == 1 and is_packed_v2(r[0][0]))
	check("hole is PackedVector2Array", r.size() == 1 and r[0].size() == 2 and is_packed_v2(r[0][1]))
	check("outline has >= 3 points", r.size() == 1 and r[0][0].size() >= 3)
	check("interior hole area ~ blast",
		r.size() == 1 and absf(poly_area(r[0][1]) - poly_area(circle(100, 100, 15))) < 5.0)
	check("interior outline area ~ asteroid",
		r.size() == 1 and absf(poly_area(r[0][0]) - ast_area) < 5.0)

	# --- destruction behaviours, end to end ---
	var edge: Array = g.diff(ast, circle(150, 100, 25))
	check("edge bite -> 1 solid piece", edge.size() == 1 and edge[0].size() == 1)

	check("slice -> 2 pieces", g.diff(ast, rect(95, 40, 105, 160)).size() == 2)
	check("cross blast -> 4 pieces", g.diff(ast, [rect(95, 40, 105, 160), rect(40, 95, 160, 105)]).size() == 4)
	check("engulfed -> destroyed", g.diff(ast, circle(100, 100, 80)).size() == 0)
	check("blast misses -> unchanged", g.diff(ast, circle(400, 400, 30)).size() == 1)

	# Array-form subject (outline + hole) fed back in
	check("re-feed holed asteroid (Array subject)",
		g.diff([ast, circle(100, 100, 15)], rect(95, 40, 105, 160)).size() == 2)

	# multiple / overlapping blasts as an Array clip
	check("two separate blasts -> 1 piece",
		g.diff(ast, [circle(150, 100, 18), circle(50, 100, 18)]).size() == 1)
	var overlap: Array = g.diff(ast, [circle(100, 100, 20), circle(115, 100, 20)])
	check("overlapping blasts -> 1 piece, 1 hole", overlap.size() == 1 and hole_count(overlap) == 1)
	var removed := ast_area - total_area(overlap)
	check("overlap union removed area in range",
		removed > PI * 20.0 * 20.0 + 50.0 and removed < 2.0 * PI * 20.0 * 20.0 - 50.0)

	# multiple disjoint bodies in one subject
	check("disjoint bodies stay independent",
		g.diff([circle(50, 100, 20), circle(150, 100, 20)], circle(50, 100, 8)).size() == 2)

	# empty / degenerate inputs must be safe
	check("empty clip -> asteroid unchanged", g.diff(ast, PackedVector2Array()).size() == 1)
	check("empty subject -> 0 pieces", g.diff(PackedVector2Array(), circle(100, 100, 10)).size() == 0)

	# winding independence (reverse every contour -> same outcome)
	var rev_ast := ast.duplicate()
	rev_ast.reverse()
	var rev_blast := circle(120, 100, 25)
	rev_blast.reverse()
	var normal_n: int = g.diff(ast, circle(120, 100, 25)).size()
	var reversed_n: int = g.diff(rev_ast, rev_blast).size()
	check("winding independence", reversed_n == normal_n)

	print("\n", checks, " checks, ", failures, " failure(s)")
	print("ALL TESTS PASSED" if failures == 0 else "TESTS FAILED")
	quit(failures)
