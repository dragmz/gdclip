// Unit tests for the gdclip geometry core (the asteroid-breaking maths).
//
// These call the real production entry point - gdclip::Difference - and its
// helpers, so the tests track the shipped behaviour rather than a copy of it.
// No Godot runtime is needed; only gdclip_core.cpp + clipper.cpp are linked.
//
// Build & run:  tests/run_tests.sh   (or the CMake target, or run_tests.bat)

#include <cstdio>
#include <cmath>
#include <vector>
#include <algorithm>
#include "gdclip_core.hpp"

using ClipperLib::Path;
using ClipperLib::Paths;

// ---------------------------------------------------------------------------
// Tiny test harness (no external dependency)
// ---------------------------------------------------------------------------

static int g_checks = 0;
static int g_failures = 0;
static const char *g_current = "";

#define CHECK(cond)                                                           \
	do {                                                                      \
		++g_checks;                                                           \
		if (!(cond)) {                                                        \
			++g_failures;                                                     \
			std::printf("    FAIL [%s] line %d: %s\n",                        \
				g_current, __LINE__, #cond);                                  \
		}                                                                     \
	} while (0)

#define CHECK_RANGE(value, lo, hi)                                            \
	do {                                                                      \
		++g_checks;                                                           \
		const double _v = (value);                                           \
		if (!(_v >= (lo) && _v <= (hi))) {                                    \
			++g_failures;                                                     \
			std::printf("    FAIL [%s] line %d: %s = %.3f not in [%.3f, %.3f]\n", \
				g_current, __LINE__, #value, _v, (double)(lo), (double)(hi)); \
		}                                                                     \
	} while (0)

typedef void (*TestFn)();

static void run(const char *name, TestFn fn)
{
	g_current = name;
	const int before = g_failures;
	fn();
	std::printf("  [%s] %s\n", before == g_failures ? "PASS" : "FAIL", name);
}

// ---------------------------------------------------------------------------
// Geometry helpers (build inputs / measure outputs in pixel space)
// ---------------------------------------------------------------------------

static Path circle(double cx, double cy, double r, int n = 32)
{
	Path p;
	for (int i = 0; i < n; ++i)
	{
		const double a = 2.0 * M_PI * i / n;
		p.push_back(ClipperLib::IntPoint(
			gdclip::ToFixed(cx + r * std::cos(a)),
			gdclip::ToFixed(cy + r * std::sin(a))));
	}
	return p;
}

static Path rect(double x0, double y0, double x1, double y1)
{
	Path p;
	p.push_back(ClipperLib::IntPoint(gdclip::ToFixed(x0), gdclip::ToFixed(y0)));
	p.push_back(ClipperLib::IntPoint(gdclip::ToFixed(x1), gdclip::ToFixed(y0)));
	p.push_back(ClipperLib::IntPoint(gdclip::ToFixed(x1), gdclip::ToFixed(y1)));
	p.push_back(ClipperLib::IntPoint(gdclip::ToFixed(x0), gdclip::ToFixed(y1)));
	return p;
}

// Area of a single contour in px^2.
static double polyArea(const Path &p)
{
	return std::fabs(ClipperLib::Area(p)) / (gdclip::SCALE * gdclip::SCALE);
}

// Solid area of a piece in px^2 (outline minus its holes).
static double pieceArea(const gdclip::Piece &p)
{
	double a = std::fabs(ClipperLib::Area(p.outline));
	for (size_t i = 0; i < p.holes.size(); ++i)
		a -= std::fabs(ClipperLib::Area(p.holes[i]));
	return a / (gdclip::SCALE * gdclip::SCALE);
}

static double totalArea(const std::vector<gdclip::Piece> &v)
{
	double a = 0;
	for (size_t i = 0; i < v.size(); ++i)
		a += pieceArea(v[i]);
	return a;
}

static int holeCount(const std::vector<gdclip::Piece> &v)
{
	int h = 0;
	for (size_t i = 0; i < v.size(); ++i)
		h += (int)v[i].holes.size();
	return h;
}

// A 50px-radius asteroid centred at (100, 100). Areas are measured from the
// actual (polygon-approximated) shapes rather than the ideal pi*r^2, so the
// expectations stay exact regardless of how finely the circles are tessellated.
static Paths asteroid() { return Paths{ circle(100, 100, 50) }; }
static double asteroidArea() { return polyArea(circle(100, 100, 50)); }

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

// Coordinates must be rounded *after* scaling. The original bug cast to int
// first ((long)x * SCALE), collapsing all sub-pixel detail.
static void test_precision_round_not_truncate()
{
	CHECK(gdclip::ToFixed(12.7) == 3251);          // llround(12.7 * 256)
	CHECK(gdclip::ToFixed(12.7) != 12 * 256);      // what truncate-first gave
	CHECK(gdclip::ToFixed(-3.5) == -896);          // rounds away from zero
	CHECK_RANGE(gdclip::FromFixed(gdclip::ToFixed(7.25)), 7.24, 7.26);
}

// A blast entirely inside the asteroid leaves a donut: one piece, one hole.
static void test_interior_hit_makes_hole()
{
	const Path blast = circle(100, 100, 15);
	auto r = gdclip::Difference(asteroid(), Paths{ blast });
	CHECK(r.size() == 1);
	CHECK(holeCount(r) == 1);
	const double expected = asteroidArea() - polyArea(blast);
	CHECK_RANGE(totalArea(r), expected - 5, expected + 5);
}

// A blast on the rim takes a bite: one piece, no hole.
static void test_edge_bite_no_hole()
{
	auto r = gdclip::Difference(asteroid(), Paths{ circle(150, 100, 25) });
	CHECK(r.size() == 1);
	CHECK(holeCount(r) == 0);
	CHECK(totalArea(r) < asteroidArea());
}

// A thin cut straight across splits the asteroid into two pieces.
static void test_slice_splits_into_two()
{
	auto r = gdclip::Difference(asteroid(), Paths{ rect(95, 40, 105, 160) });
	CHECK(r.size() == 2);
	CHECK(holeCount(r) == 0);
}

// A blast that swallows the whole asteroid destroys it: no pieces.
static void test_engulfed_is_destroyed()
{
	auto r = gdclip::Difference(asteroid(), Paths{ circle(100, 100, 80) });
	CHECK(r.size() == 0);
}

// A piece returned by a previous hit (here a donut: outline + hole) feeds
// straight back in as the subject, and cutting it still works.
static void test_rehit_holed_asteroid_splits()
{
	Paths donut{ circle(100, 100, 50), circle(100, 100, 15) };
	auto r = gdclip::Difference(donut, Paths{ rect(95, 40, 105, 160) });
	CHECK(r.size() == 2);
}

// EXPLOSIONS ARE POLYGONS: several blasts passed in one call each carve their
// own crater. Two far-apart bites leave the body connected (one piece) and
// remove area from both sites.
static void test_multiple_blasts_each_crater()
{
	Paths blasts{ circle(150, 100, 18), circle(50, 100, 18) };
	auto r = gdclip::Difference(asteroid(), blasts);
	CHECK(r.size() == 1);
	CHECK(holeCount(r) == 0);
	CHECK(totalArea(r) < asteroidArea() - 400); // both craters removed
}

// Overlapping blasts must UNION: their overlap is destroyed, not left intact.
// Two overlapping discs fully inside the asteroid carve exactly ONE connected
// hole whose area is the union of the discs (more than one disc, less than the
// sum). The earlier even-odd bug would have removed only the symmetric
// difference (~one disc), leaving the overlap as surviving matter.
static void test_overlapping_blasts_union()
{
	Paths blasts{ circle(100, 100, 20), circle(115, 100, 20) };
	auto r = gdclip::Difference(asteroid(), blasts);

	const double oneDisc = M_PI * 20 * 20;
	const double sumDiscs = 2.0 * oneDisc;
	const double removed = asteroidArea() - totalArea(r);

	CHECK(r.size() == 1);
	CHECK(holeCount(r) == 1); // a single connected crater, not a ring
	CHECK_RANGE(removed, oneDisc + 50, sumDiscs - 50);
}

// A ring of overlapping blasts carves an annulus, severing the core: the result
// is the outer rim (a piece with a hole) plus the inner core left floating as
// its own independent piece (an island).
static void test_severed_core_becomes_island()
{
	Paths blasts;
	const int n = 12;
	for (int i = 0; i < n; ++i)
	{
		const double a = 2.0 * M_PI * i / n;
		blasts.push_back(circle(100 + 25 * std::cos(a), 100 + 25 * std::sin(a), 12));
	}
	auto r = gdclip::Difference(asteroid(), blasts);

	CHECK(r.size() == 2);

	int withHole = 0, solid = 0;
	for (size_t i = 0; i < r.size(); ++i)
		(r[i].holes.empty() ? solid : withHole)++;
	CHECK(withHole == 1); // outer rim
	CHECK(solid == 1);    // inner island
}

// Sub-pixel precision is preserved end to end: nudging a blast by 0.3px changes
// the carved area measurably. With the old truncate-first scaling both blasts
// would have rounded to the same integer grid and removed identical area.
static void test_subpixel_shift_changes_result()
{
	auto a = gdclip::Difference(asteroid(), Paths{ circle(150.0, 100, 25) });
	auto b = gdclip::Difference(asteroid(), Paths{ circle(150.3, 100, 25) });
	CHECK(std::fabs(totalArea(a) - totalArea(b)) > 0.01);
}

// Degenerate / empty inputs must not crash and must give sensible results.
static void test_degenerate_inputs()
{
	// Empty subject -> nothing survives.
	CHECK(gdclip::Difference(Paths{}, Paths{ circle(100, 100, 10) }).size() == 0);
	// No explosion -> the asteroid passes through unchanged (one solid piece).
	auto r = gdclip::Difference(asteroid(), Paths{});
	CHECK(r.size() == 1);
	CHECK(holeCount(r) == 0);
	CHECK_RANGE(totalArea(r), asteroidArea() - 30, asteroidArea() + 30);
	// A two-point "polygon" bounds no area and is ignored.
	Path line; line.push_back(ClipperLib::IntPoint(0, 0));
	line.push_back(ClipperLib::IntPoint(gdclip::ToFixed(10), 0));
	CHECK(gdclip::Difference(asteroid(), Paths{ line }).size() == 1);
}

// Invariant: every emitted contour (each piece outline and each hole) is a
// real, simple polygon of at least MIN_AREA_PX2 - no dust, and in particular no
// phantom sub-threshold island leaking out of a parent that was itself dropped.
// Stress it with a messy mix of overlapping, ringed and tiny blasts.
static void test_no_subthreshold_fragments()
{
	Paths blasts;
	// A ring that severs the core (produces islands)...
	const int n = 16;
	for (int i = 0; i < n; ++i)
	{
		const double a = 2.0 * M_PI * i / n;
		blasts.push_back(circle(100 + 28 * std::cos(a), 100 + 28 * std::sin(a), 11));
	}
	// ...plus a scatter of tiny sub-pixel blasts that could spawn slivers.
	blasts.push_back(circle(70, 70, 0.4));
	blasts.push_back(circle(130, 130, 0.4));
	blasts.push_back(rect(99.9, 99.9, 100.1, 100.1));

	auto r = gdclip::Difference(asteroid(), blasts);
	CHECK(!r.empty());
	for (size_t i = 0; i < r.size(); ++i)
	{
		CHECK(r[i].outline.size() >= 3);
		CHECK(polyArea(r[i].outline) >= gdclip::MIN_AREA_PX2);
		for (size_t h = 0; h < r[i].holes.size(); ++h)
		{
			CHECK(r[i].holes[h].size() >= 3);
			CHECK(polyArea(r[i].holes[h]) >= gdclip::MIN_AREA_PX2);
		}
	}
}

int main()
{
	std::printf("gdclip unit tests\n");

	run("precision: round, not truncate", test_precision_round_not_truncate);
	run("interior hit -> donut (1 hole)", test_interior_hit_makes_hole);
	run("edge bite -> 1 solid piece", test_edge_bite_no_hole);
	run("slice -> 2 pieces", test_slice_splits_into_two);
	run("engulfed -> destroyed", test_engulfed_is_destroyed);
	run("re-hit holed asteroid splits", test_rehit_holed_asteroid_splits);
	run("multiple blasts each crater", test_multiple_blasts_each_crater);
	run("overlapping blasts union", test_overlapping_blasts_union);
	run("severed core -> island piece", test_severed_core_becomes_island);
	run("no sub-threshold dust fragments", test_no_subthreshold_fragments);
	run("sub-pixel shift changes result", test_subpixel_shift_changes_result);
	run("degenerate inputs are safe", test_degenerate_inputs);

	std::printf("\n%d checks, %d failure(s)\n", g_checks, g_failures);
	if (g_failures == 0)
		std::printf("ALL TESTS PASSED\n");
	else
		std::printf("TESTS FAILED\n");
	return g_failures == 0 ? 0 : 1;
}
