// Boost.Geometry (aka GGL, Generic Geometry Library)
// Unit Test

// Copyright (c) 2012-2015 Barend Gehrels, Amsterdam, the Netherlands.

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <test_buffer.hpp>


static std::string const simplex
    = "MULTIPOLYGON(((0 1,2 5,5 3,0 1)),((1 1,5 2,5 0,1 1)))";

static std::string const zonethru
    = "MULTIPOLYGON(((0 0,0 6,5 6,5 4,3 4,3 0,0 0)),((5 0,5 2,7 2,7 6,10 6,10 0,5 0)))";

static std::string const wrapped
    = "MULTIPOLYGON(((0 0,0 10,10 10,10 0,0 0),(2 2,8 2,8 8,2 8,2 2)),((4 4,4 6,6 6,6 4,4 4)))";

static std::string const nested
    = "MULTIPOLYGON(((0 0,0 14,14 14,14 0,0 0),(2 2,12 2,12 12,2 12,2 2)),((4 4,4 10,10 10,10 4,4 4),(6 6,8 6,8 8,6 8,6 6)))";

static std::string const triangles
    = "MULTIPOLYGON(((0 4,3 0,-2.5 -1,0 4)),((3 8,5.5 13,8 8,3 8)),((11 4,13.5 -1,8 0,11 4)))";

static std::string const degenerate0
    = "MULTIPOLYGON()";
static std::string const degenerate1
    = "MULTIPOLYGON(((5 5,5 5,5 5,5 5)),((6 6,6 6,6 6,6 6)))";
static std::string const degenerate2
    = "MULTIPOLYGON(((0 0,0 10,10 10,10 0,0 0),(5 5,5 5,5 5,5 5)),((11 5,11 5,11 5,11 5)))";

// From robustness tests (rt)

// Case with duplicate points (due to chained boxes) (round)
static std::string const rt_a
    = "MULTIPOLYGON(((2 7,2 8,3 8,3 7,2 7)),((5 4,5 5,6 5,6 4,5 4)),((5 8,6 8,6 7,6 6,5 6,5 7,4 7,4 8,5 8)),((3 5,4 5,4 4,3 4,2 4,2 5,3 5)))";

// Case with u-u (miter)
static std::string const rt_b
    = "MULTIPOLYGON(((8 4,8 5,9 5,9 4,8 4)),((6 2,6 3,7 3,7 2,6 2)),((8 0,8 1,9 1,9 0,8 0)),((9 7,9 8,10 8,10 7,9 7)))";

// Case with geometry::equals( turn.point(7.0000000000000000, 4.3368086899420177e-019), helper_segment(7.0000000000000000, 0.00000000000000000))) (round)
static std::string const rt_c
    = "MULTIPOLYGON(((6 1,6 2,7 2,7 1,6 1)),((8 0,8 1,9 1,9 0,8 0)))";

// Case with round corner on same perpendicular points (round)
static std::string const rt_d
    = "MULTIPOLYGON(((2 2,2 3,3 2,2 2)),((2 5,2 6,3 5,2 5)),((2 4,2 5,3 4,2 4)),((3 2,3 3,4 2,3 2)),((4 4,4 5,5 4,4 4)),((5 6,5 5,4 5,4 6,5 7,5 6)),((2 2,3 1,3 0,2 0,2 1,1 1,1 2,2 2)),((1 3,1 2,0 2,1 3)),((1 4,2 4,2 3,1 3,1 4)))";

// Case with missing turning point (miter) and many intersections (round, OK)
static std::string const rt_e
    = "MULTIPOLYGON(((0 6,0 7,1 6,0 6)),((3 7,3 8,4 8,4 7,3 7)),((4 6,4 7,5 7,4 6)),((3 6,3 7,4 6,3 6)),((1 9,2 10,2 9,1 9)),((1 9,1 8,0 8,0 9,1 9)),((3 5,3 4,2 4,2 5,2 6,3 5)))";

// Extract of e (miter)
static std::string const rt_f
    = "MULTIPOLYGON(((0 6,0 7,1 6,0 6)),((1 9,1 8,0 8,0 9,1 9)))";

// Robustness / turn problem (collinearity with turn after), solved in get_turn-info
static std::string const rt_g1
    = "MULTIPOLYGON(((3 8,3 9,4 9,3 8)),((7 5,7 6,8 5,7 5)),((1 8,1 9,2 9,1 8)),((1 6,1 7,2 7,1 6)))";

static std::string const rt_g2
    = "MULTIPOLYGON(((6 5,7 5,6 4,5 4,5 5,6 5)),((4 2,4 3,5 3,4 2)))";

static std::string const rt_g3
    = "MULTIPOLYGON(((4 2,5 3,5 2,4 2)),((2 0,3 1,3 0,2 0)))";


// IP on match of 3 lines
static std::string const rt_h
    = "MULTIPOLYGON(((4 7,4 8,5 7,4 7)),((4 8,5 9,5 8,4 8)),((9 1,10 2,10 1,9 1)),((4 1,4 2,5 2,4 1)),((2 9,2 10,3 10,2 9)),((7 7,8 8,8 7,7 7)),((3 4,4 4,3 3,2 3,2 4,3 4)))";

// r=1.16 (touching)
static std::string const rt_i
    = "MULTIPOLYGON(((2 1,2 2,3 2,2 1)),((3 2,3 3,4 3,3 2)))";

// r=1.16 (touching in the middle)
static std::string const rt_j
    = "MULTIPOLYGON(((2 4,2 5,3 5,2 4)),((5 3,5 4,6 4,5 3)),((9 4,9 5,10 5,10 4,9 4)),((0 2,0 3,1 3,0 2)))";

// Occupied (but not completely - due to duplicate point there)
static std::string const rt_k
    = "MULTIPOLYGON(((3 7,3 8,4 8,3 7)),((0 4,0 5,1 5,0 4)),((3 5,3 6,4 6,4 5,3 5)),((2 6,3 7,3 6,2 6)),((3 2,3 1,2 1,2 2,3 3,3 2)),((0 9,1 10,1 9,1 8,0 8,0 9)))";

// Segment-intersection problem (nearly collinear not reported as collinear), fixed
static std::string const rt_l
    = "MULTIPOLYGON(((2 5,2 6,3 5,2 5)),((6 1,6 2,7 1,6 1)))";

// Segment-intersection problem (missed touch because of robustness) (with 4), fixed
static std::string const rt_m1
    = "MULTIPOLYGON(((4 2,4 3,5 2,4 2)),((1 2,2 3,2 2,1 2)))";

// Same, with 2
static std::string const rt_m2
    = "MULTIPOLYGON(((0 3,1 4,1 3,0 3)),((3 6,4 7,4 6,4 5,3 5,3 6)))";

// Segment-intersection problem (disjoint nearly collinear segments were reported as intersecting), fixed.
static std::string const rt_n
    = "MULTIPOLYGON(((0 0,1 1,1 0,0 0)),((3 3,4 4,4 3,3 3)))";

// Segment intersection of 3 pieces in one point, plus all intersection points are within the other piece (due to precision)
static std::string const rt_o1
    = "MULTIPOLYGON(((8 4,8 5,9 5,8 4)),((9 4,10 5,10 4,9 4)),((6 2,6 3,7 3,6 2)))";

static std::string const rt_o2
    = "MULTIPOLYGON(((8 4,8 5,9 5,8 4)),((6 5,7 4,6 4,6 5)),((9 4,10 5,10 4,9 4)),((6 2,6 3,7 3,6 2)))";

static std::string const rt_o3
    = "MULTIPOLYGON(((8 4,8 5,9 5,8 4)),((6.5 5,7.5 4,6.5 4,6.5 5)),((8.5 4,9.5 5,9.5 4,8.5 4)),((6 2,6 3,7 3,6 2)),((10 4,11 5,11 4,10 4)))";

static std::string const rt_o4
    = "MULTIPOLYGON(((8 4,8 5,9 5,8 4)),((6.5 5,7.5 4,6.5 4,6.5 5)),((8.5 4,9.5 5,9.5 4,8.5 4)),((6 2,6 3,7 3,6 2)),((10 4,11 5,11 4,10 4)),((10 3,11 3,11 2,10 3)))";

// Occupied - intersection
static std::string const rt_p1
    = "MULTIPOLYGON(((5 2,5 3,6 3,6 2,5 2)),((8 0,8 1,9 0,8 0)),((8 2,9 3,9 2,8 2)))";

static std::string const rt_p2
    = "MULTIPOLYGON(((9 3,9 4,10 3,9 3)),((9 4,9 5,10 5,9 4)),((8 6,9 7,9 6,8 6)))";

static std::string const rt_p3
    = "MULTIPOLYGON(((3 8,3 9,4 9,3 8)),((3 7,3 8,4 8,3 7)),((0 8,0 9,1 8,0 8)))";

// Occupation map: robustness, nearly collinear, both incoming
static std::string const rt_p4
    = "MULTIPOLYGON(((8 8,9 9,9 8,8 8)),((5 8,5 9,6 9,5 8)),((6 5,6 6,7 6,6 5)),((4 7,4 8,5 8,4 7)))";

// Occupation map: Two collinear segments intersecting one segment
static std::string const rt_p5
    = "MULTIPOLYGON(((3 2,4 3,4 2,3 2)),((2 2,2 3,3 3,3 2,2 2)),((4 3,5 4,5 3,4 3)))";

// Occupied map: only two pieces involved so leave early
static std::string const rt_p6
    = "MULTIPOLYGON(((7 3,7 4,8 3,7 3)),((7 6,8 7,8 6,7 6)))";

// Occupation map: needing relaxed_less for map
static std::string const rt_p7
    = "MULTIPOLYGON(((6 6,7 7,7 6,6 6)),((3 4,3 5,4 4,3 4)),((2 6,3 7,3 6,2 6)))";

// Occupation map: needing relaxed_less PLUS relaxed_equals for map
static std::string const rt_p8
    = "MULTIPOLYGON(((4 7,4 8,5 7,4 7)),((5 3,6 4,6 3,5 3)),((8 5,8 6,9 6,8 5)))";

// Occupation map: needing go back for extra outgoing vectors too
static std::string const rt_p9
    = "MULTIPOLYGON(((1 6,1 7,2 6,1 6)),((4 3,4 4,5 3,4 3)),((3 4,2 3,2 4,3 5,3 4)))";

static std::string const rt_p10
    = "MULTIPOLYGON(((6 6,7 7,7 6,6 6)),((4 6,5 7,5 6,4 6)),((8 8,9 9,9 8,8 8)))";

// Occupation map, multiple back/forward cases and combinations
static std::string const rt_p11
    = "MULTIPOLYGON(((0 8,0 9,1 9,1 8,0 8)),((2 8,2 9,3 9,3 8,2 8)),((3 8,4 9,4 8,3 8)),((4 1,4 2,5 2,4 1)))";

// Occupation map - incoming angle, non-zero
static std::string const rt_p12
    = "MULTIPOLYGON(((8 4,9 5,9 4,8 4)),((5 5,5 6,6 6,6 5,5 5)),((8 4,8 3,7 3,8 4)))";

// Occupation map - outgoing angle, 1
static std::string const rt_p13
    = "MULTIPOLYGON(((5 2,6 3,6 2,5 2)),((3 0,4 1,4 0,3 0)),((3 1,3 2,4 2,4 1,3 1)))";

// Occupation map - touch which was originally in other segment-id
static std::string const rt_p14
    = "MULTIPOLYGON(((9 9,10 10,10 9,9 9)),((7 7,8 8,8 7,7 7)),((6 6,6 7,7 7,7 6,6 6)))";

// Occupation map - needing measuring sides from original point and not center point
static std::string const rt_p15
    = "MULTIPOLYGON(((5 2,5 3,6 3,6 2,5 2)),((4 1,5 2,5 1,4 1)),((8 5,9 6,9 5,8 5)))";

// Occupation map - needing other approach w.r.t. discarding (collinear) segments
static std::string const rt_p16
    = "MULTIPOLYGON(((5 7,5 8,6 7,5 7)),((9 3,9 4,10 3,9 3)),((6 7,7 8,7 7,6 7)))";

// Occupation map - outputting two valid turns (resulting in the wrong choice in the end)
static std::string const rt_p17
    = "MULTIPOLYGON(((4 8,5 9,5 8,4 8)),((1 8,2 9,2 8,1 8)),((2 6,3 7,3 6,2 6)))";

// Occupation map - outputting no valid turns (needing to take other turns into account)
static std::string const rt_p18
    = "MULTIPOLYGON(((7 6,8 7,8 6,7 6)),((7 3,7 4,8 3,7 3)),((5 4,6 5,6 4,5 4)))";

// Occupation map - showing wrong approach in p17/p18, now new approach with keep_indices
static std::string const rt_p19
    = "MULTIPOLYGON(((0 5,1 6,1 5,0 5)),((0 7,0 8,1 7,0 7)),((3 4,3 5,4 4,3 4)))";

// Occupation map: two non-collinear segments non-intersecting, needing relaxed_equal
static std::string const rt_p20
    = "MULTIPOLYGON(((2 3,2 4,3 4,3 3,2 3)),((0 5,0 6,1 6,0 5)),((2 7,2 8,3 8,2 7)))";

// Occupation map: turn more right should still be included
static std::string const rt_p21
    = "MULTIPOLYGON(((4 2,4 3,5 3,4 2)),((4 1,5 2,5 1,4 1)),((5 2,6 3,6 2,5 2)))";

static std::string const rt_p22
    = "MULTIPOLYGON(((4 8,5 9,5 8,4 8)),((5 9,6 10,6 9,5 9)),((1 7,1 8,2 8,2 7,1 7)),((2 6,3 7,3 6,2 6)))";

// Occupation map with a uu-turn
static std::string const rt_q1
    = "MULTIPOLYGON(((4 6,4 7,5 7,5 6,4 6)),((1 6,1 7,2 7,2 6,1 6)),((1 9,1 10,2 10,2 9,1 9)))";

// Occupation map with twice a uu-turn
static std::string const rt_q2
    = "MULTIPOLYGON(((0 6,0 7,1 6,0 6)),((2 6,2 7,3 6,2 6)),((4 5,3 5,3 6,4 6,5 6,5 5,4 4,4 5)))";

// Robustness issue related to collinear correction (meeting) and selecting the right axis (x/y)
static std::string const rt_r
    = "MULTIPOLYGON(((3 1,2 0,1 1,2 2,3 1)),((5 3,4 2,3 2,4 4,5 3)))";

// Robustness - flagged by "meeting"
static std::string const rt_s1
    = "MULTIPOLYGON(((4 1,5 2,5 1,4 1)),((5 2,6 3,6 2,5 2)),((7 1,7 2,8 1,7 1)))";

// Robustness - flagged by "disjoint"
static std::string const rt_s2
    = "MULTIPOLYGON(((0 0,1 1,1 0,0 0)),((2 4,2 5,3 4,2 4)),((3.5 3.5,4 4,4 3,3 3,3 4,3.5 3.5)))";

// Robustness issue in get_turn_info (touch, collinear, blocking q)
static std::string const rt_t
    = "MULTIPOLYGON(((1 3,1 4,2 3,1 3)),((1 4,0 3,0 4,0 5,1 4)))";

// Created traversed ring
static std::string const rt_u1
    = "MULTIPOLYGON(((7 6,8 7,8 6,7 6)),((8 9,9 10,9 9,8 9)),((2 8,3 9,3 8,2 8)),((6 5,6 6,7 6,6 5)),((6 7,7 7,6 6,5 6,6 7)),((7 7,7 8,8 7,7 7)),((6 7,6 8,7 8,6 7)),((8 7,8 8,9 8,9 7,8 7)))";

static std::string const rt_u2
    = "MULTIPOLYGON(((4 2,5 3,5 2,4 2)),((6 4,6 5,7 5,6 4)),((7 4,7 5,8 5,7 4)),((7 3,7 4,8 3,7 3)),((7 10,8 10,7 9,6 9,7 10)),((0 7,0 8,1 7,0 7)),((6 1,6 2,7 2,7 1,6 1)),((5 7,5 8,6 9,6 8,7 9,7 8,7 7,6 6,6 7,5 7)),((5 4,6 4,6 3,5 3,4 3,4 4,5 4)),((5 2,6 2,5 1,4 0,3 0,4 1,3 1,3 2,3 3,4 3,4 2,4.5 1.5,5 2)),((5 6,5 5,4 5,4 6,4 7,5 7,5 6)),((5 0,5 1,5.5 0.5,6 1,6 0,5 0)),((5 9,5 8,4 8,3 8,2 8,1 8,0 8,1 9,1 10,2 9,2 10,3 10,4 10,5 10,5 9)),((4 5,4 4,3 4,3 3,2 3,1 3,0 3,0 4,0 5,0 6,1 6,2 5,1 5,1 4,2 4,3 5,2 5,2 6,2.5 5.5,3 6,4 5)),((3 2,2 2,2 3,3 2)),((2 0,1 0,2 1,3 0,2 0)),((1 1,0 1,0 2,0 3,1 2,1.5 1.5,2 2,3 1,2 1,1 1)),((2 6,1 6,1 7,2 7,3 7,2 6)),((4 8,4 7,3 7,4 8)),((9 6,8 6,7 6,7 7,8 7,8.5 6.5,9 7,8 7,8 8,8.5 7.5,9 8,8 8,8 9,9 9,9 10,10 10,10 9,10 8,10 7,10 6,10 5,9 4,9 5,8 5,9 6)),((7 0,6 0,7 1,8 1,7 0)),((8 3,9 2,8 2,7 2,8 3)),((10 2,10 1,10 0,9 0,9 1,8 1,8 2,8.5 1.5,9 2,9 3,10 4,10 3,10 2)),((9 4,9 3,8 3,8 4,9 4)))";

static std::string const rt_u3
    = "MULTIPOLYGON(((3 0,4 1,4 0,3 0)),((2 0,2 1,3 1,2 0)),((6 7,7 8,7 7,6 7)),((1 2,2 3,2 2,1 2)),((9 1,10 2,10 1,9 1)),((0 2,0 3,1 3,1 2,0 2)),((5 0,5 1,6 1,6 0,5 0)),((1 1,2 2,2 1,1 1)),((0 7,0 8,1 7,0 7)),((9 0,10 1,10 0,9 0)),((0 5,0 6,1 6,1 5,0 5)),((1 6,2 7,2 6,1 6)),((7 0,8 1,8 0,7 0)),((8 6,9 7,9 6,8 6)),((9 6,10 7,10 6,9 6)),((1 4,1 5,2 5,1 4)),((5 6,4 5,4 6,5 7,5 6)),((5 4,4.5 3.5,5 3,4 3,4 2,3 1,3 2,3 3,2 3,2 4,3 4,4 4,5 5,5 4)),((4 7,3 7,3 8,4 9,5 10,6 10,6 9,5 9,4 8,5 8,4 7)),((6 3,5 3,5 4,5.5 3.5,6 4,7 4,7 3,8 3,8 2,8 1,7 1,6 1,6 2,6 3)),((3 5,2 4,2 5,2 6,3 5)),((1 1,1 0,0 0,0 1,1 2,1 1)),((2 9,2 8,1 8,0 8,1 9,1 10,1.5 9.5,2 10,2.5 9.5,3 10,3 9,2 9)),((6 5,6 6,7 7,8 7,8 6,7 6,7 5,6 4,6 5)),((7 8,6 8,6 9,7 9,8 8,7 8)),((9 10,10 10,10 9,10 8,9 7,9 8,8 8,9 9,8 9,8 10,9 10)))";

static std::string const rt_u4
    = "MULTIPOLYGON(((5 0,6 1,6 0,5 0)),((6 1,6 2,7 2,7 1,6 1)),((6 6,7 7,7 6,6 6)),((4 7,4 8,5 8,4 7)),((1 6,1 7,2 7,1 6)),((3 4,3 5,4 5,3 4)),((2 0,2 1,3 0,2 0)),((7 2,7 3,8 3,7 2)),((3 3,4 4,4 3,3 3)),((5 9,5 10,6 10,7 10,6 9,5 8,5 9)),((5 4,5 5,6 4,5 4)),((3 1,4 2,4 1,3 1)),((3 0,3 1,4 0,3 0)),((2 7,2 8,3 8,3 7,2 7)),((9 2,9 3,10 2,9 2)),((6 2,6 3,7 3,6 2)),((8 8,9 9,9 8,8 8)),((2 1,2 2,3 2,3 1,2 1)),((4 3,5 4,5 3,4 3)),((4 6,5 7,5 6,4 6)),((1 8,1 9,2 8,1 7,1 8)),((8 8,9 7,8 7,7 7,7 8,8 8)),((8 1,8 2,9 2,9 1,10 1,9 0,8 0,8 1)),((5 2,4 2,4 3,5 2)),((2 5,1 5,1 6,2 6,3 5,2 5)),((1 3,2 4,2 3,2 2,1 1,0 1,1 2,1 3)),((1 0,0 0,1 1,2 1,1.5 0.5,2 0,1 0)),((2 10,3 10,4 10,4 9,3 9,2 8,2 9,1 9,0 9,1 10,2 10)),((9 5,9 6,10 6,10 5,10 4,9 4,8.5 3.5,9 3,8 3,8 4,7 4,7 5,8 5,9 5)),((7 0,6 0,7 1,8 1,7 0)),((7 9,7 8,6 8,6 9,7 9)))";

static std::string const rt_u5
    = "MULTIPOLYGON(((4 3,4 4,5 4,5 3,4 3)),((6 5,6 6,7 6,6 5)),((5 4,6 5,6 4,5 4)),((4 0,4 1,5 1,5 0,4 0)),((7 8,8 9,8 8,7 8)),((8 2,8 3,9 3,8 2)),((2 1,2 2,3 1,2 1)),((5 7,5 8,6 8,5 7)),((4 5,4 6,5 5,4 5)),((7 0,8 1,8 0,7 0)),((7 2,8 1,7 1,6 1,7 2)),((3 3,4 3,4 2,3 2,3 3)),((3 9,3 10,4 9,3 9)),((1 2,2 3,2 2,1 2)),((2 4,2 5,3 4,2 3,2 4)),((3 7,4 7,3 6,2 6,3 7)))";

static std::string const rt_u6
    = "MULTIPOLYGON(((2 2,3 3,3 2,2 2)),((9 8,9 9,10 9,10 8,9 8)),((5 3,6 4,6 3,5 3)),((5 5,5 6,6 6,5 5)),((5 1,6 2,6 1,5 1)),((6 5,7 6,7 5,6 5)),((3 0,4 1,4 0,3 0)),((6 6,6 7,7 6,6 6)),((9 2,10 2,10 1,9 1,9 2)),((6 8,6 9,7 9,7 8,6 8)),((7 0,7 1,8 0,7 0)),((4 4,4 5,5 5,4 4)),((0 7,0 8,1 8,0 7)),((5 9,6 10,6 9,5 9)),((4 9,4 10,5 10,5 9,4 9)),((3 7,4 7,3 6,2 6,3 7)),((9 7,10 6,9 6,8 6,9 7)),((5 3,5 2,4 2,4 3,4 4,5 4,5 3)),((1 1,2 0,1 0,0 0,1 1)),((2 10,3 10,3 9,2 9,1 9,1 10,2 10)),((8.5 4.5,9 4,8 4,7 4,8 5,9 5,8.5 4.5)),((8 3,9 3,9 2,8 2,8 3)))";

// This case has colocated intersection points and is fixed by specific sorting in enrich_intersection_points
static std::string const rt_u7
    = "MULTIPOLYGON(((4 5,4 6,5 5,4 5)),((9 2,9 3,10 3,10 2,9 2)),((7 3,7 4,8 4,8 3,7 3)),((5 5,6 6,6 5,5 5)),((3 6,4 7,4 6,3 6)),((0 5,0 6,1 5,0 5)))";

// This case is also fixed by sorting order in enrich_intersection_points. It also has IP's between piece and original (due to rounding/rescaling pieces but not original) but that is not problematic
// anymore by the fix in sorting
static std::string const rt_u8
    = "MULTIPOLYGON(((4 3,4 4,5 4,5 3,4 3)),((6 3,6 4,7 4,7 3,6 3)),((9 0,9 1,10 0,9 0)),((9 3,9 4,10 4,10 3,9 3)),((8 2,9 2,9 1,8 1,8 2)),((8 4,8 5,9 4,8 4)),((9 1,10 2,10 1,9 1)),((6 7,7 8,7 7,6 7)),((4 6,4 7,5 7,4 6)),((8 8,8 9,9 9,8 8)),((3 2,3 3,4 3,4 2,3 2)),((7 1,8 1,8 0,7 0,6 0,6 1,7 1)))";

// Fixed by checking IP's between piece and original (using robust represention of original)
static std::string const rt_u9
    = "MULTIPOLYGON(((2 3,2 4,3 4,2 3)),((9 8,9 9,10 9,10 8,9 8)),((6 3,6 4,7 4,6 3)),((5 8,5 9,6 9,5 8)),((9 5,9 6,10 6,9 5)),((4 4,4 5,5 5,5 4,4 4)),((7 7,7 8,8 8,7 7)),((8 6,8 7,9 6,8 6)),((6 7,7 7,6 6,5 6,5 7,6 7)))";

// Fixed by fixing border check for IP's
static std::string const rt_u10
    = "MULTIPOLYGON(((7 5,8 6,8 5,7 5)),((4 5,5 6,5 5,5 4,4 4,4 5)),((9 5,10 5,10 4,9 4,8 4,9 5)),((1 2,1 1,0 1,1 2)),((6 6,6 7,7 7,7 6,6 6)),((3 6,3 7,4 6,3 6)),((8 6,8 7,9 6,8 6)),((1 3,1 4,2 3,1 3)),((9 9,10 8,9 8,9 7,8 7,8 8,9 9)),((9 2,10 3,10 2,9 2)),((2 5,2 6,3 6,2 5)),((1 9,1 10,2 10,2 9,1 9)),((4 6,4 7,5 7,5 6,4 6)),((6 0,6 1,7 1,7 0,6 0)),((0 9,0 10,1 10,0 9)),((3.5 5.5,4 6,4 5,3 5,3 6,3.5 5.5)),((9 3,9 4,10 3,9 3)),((9 9,10 10,10 9,9 9)),((1 6,2 7,2 6,1 6)),((4 2,4 3,5 2,4 2)),((5 10,4 9,4 10,5 10)),((0 3,1 2,0 2,0 3)),((5 8,4 8,5 9,6 10,6 9,7 9,7 8,6 8,5 7,5 8)),((6 2,5 2,5 3,5.5 2.5,6 3,6 4,7 4,8 4,8 3,7 3,7 2,6 2)),((4 0,3 0,3 1,2.5 0.5,3 0,2 0,2 1,2 2,3 3,3 2,4 2,4 1,5 0,4 0)),((3 5,4 4,3 4,2 4,3 5)),((0 6,0 7,1 7,1 6,1 5,0 5,0 6)),((2 8,3 8,3 7,2 7,1 7,1 8,2 8)),((9 1,9 0,8 0,8 1,8 2,9 1)),((7 10,8 10,8 9,7 9,7 10)))";

// Fixed by blocking IP's instead of discarding them
static std::string const rt_u11
    = "MULTIPOLYGON(((7 3,8 4,8 3,7 3)),((4 8,5 9,5 8,4 8)),((7 1,8 2,8 1,7 1)),((7 2,8 3,8 2,7 2)),((0 1,0 2,1 2,0 1)),((8 1,9 2,9 1,8 1)),((6 9,6 8,5 8,6 9)),((7 9,7 10,8 10,7 9)),((0 7,1 8,1 7,0 7)),((6 4,6 5,7 5,7 4,6 4)),((7 4,8 5,8 4,7 4)),((4 2,4 3,5 3,5 2,4 2)),((5 5,5 6,6 6,6 5,5 5)),((5 9,6 10,6 9,5 9)),((4 8,4 7,3 7,3 8,4 8)),((5 5,6 4,5 4,4 4,4 5,5 5)),((4.5 0.5,5 0,4 0,4 1,5 2,6 2,6 3,7 4,7 3,7 2,7 1,6 1,6 0,5 0,5 1,4.5 0.5)),((4 6,3 5,3 6,4 7,5 7,5 6,4 6)),((2 3,1 3,0 3,1 4,1 5,1 6,2 6,2 5,2 4,2 3)),((3 4,4 4,3 3,2 3,3 4)),((3 9,4 9,3 8,2 8,2 9,3 9)),((1 9,1 8,0 8,0 9,1 10,2 9,1 9)),((3 7,3 6,2 6,2 7,3 7)),((8 6,8 7,9 7,9 6,9 5,8 5,8 6)),((9 1,10 2,10 1,10 0,9 0,9 1)),((8 8,8 7,7 7,7 8,8 8)),((6 8,7 9,7 8,6 7,6 8)),((10 9,10 8,10 7,9 7,9 8,9 9,9 10,10 10,10 9)))";

static std::string const rt_u12
    = "MULTIPOLYGON(((9 0,9 1,10 0,9 0)),((1 0,1 1,2 1,2 0,1 0)),((4 0,4 1,5 0,4 0)),((1 2,1 3,2 2,1 2)),((3 7,4 8,4 7,3 7)),((3 6,4 7,4 6,3 6)),((6 9,7 10,7 9,6 9)),((5 6,5 7,6 7,5 6)),((7 6,7 7,8 7,7 6)),((7 3,7 4,8 3,7 3)),((8 7,8 8,9 7,8 6,8 7)),((8 5,8 6,9 6,8 5)),((6 4,6 5,7 5,6 4)),((2 9,2 10,3 10,2 9)),((1 4,2 5,2 4,1 4)),((2 2,2 3,3 3,2 2)),((3 2,3 3,4 3,3 2)),((9 5,10 6,10 5,9 5)),((4 4,5 5,5 4,4 4)),((5 10,6 10,6 9,5 9,4 9,4 10,5 10)),((5 3,5 4,6 4,6 3,6 2,6 1,5 1,5 2,4 2,4 3,5 3)),((0 6,1 6,1 5,0 5,0 6)),((3 5,4 6,4 5,4 4,3 4,3 5)),((2 2,3 2,4 2,4 1,3 1,2 1,2 2)),((2 8,1 8,2 9,3 9,2 8)),((2 6,1 6,2 7,3 6,2 6)),((0 9,1 10,1 9,0 8,0 9)),((1 8,1 7,0 7,1 8)),((9 5,10 4,9 4,8 4,8 5,9 5)),((6 1,7 1,6.5 0.5,7 0,6 0,6 1)),((8 1,9 1,8.5 0.5,9 0,8 0,8 1)),((6 7,7 7,6.5 6.5,7 6,6 6,6 7)),((6.5 8.5,7 9,7 8,6 8,6 9,6.5 8.5)),((9 9,10 10,10 9,9 8,9 9)))";

static std::string const rt_u13
    = "MULTIPOLYGON(((6 4,6 5,7 5,6 4)),((3 2,3 3,4 3,3 2)),((7 8,7 9,8 9,8 8,7 8)),((4 9,4 10,5 10,4 9)),((7 7,7 8,8 7,7 7)),((2 6,2 7,3 7,2 6)),((0 1,1 2,1 1,0 1)),((3 1,4 2,4 1,3 1)),((2 5,2 6,3 6,2 5)),((3 5,4 4,3 4,2 4,3 5)),((4 1,5 2,5 1,4 1)),((2 0,2 1,3 1,2 0)),((5 7,5 8,6 7,5 7)),((0 2,0 3,1 3,0 2)),((9 8,9 9,10 9,10 8,9 8)),((7 5,7 6,8 5,7 5)),((5 6,5 7,6 6,5 6)),((0 6,0 7,1 7,1 6,0 6)),((5 0,5 1,6 1,5 0)),((8 7,8 8,9 8,8 7)),((4.5 4.5,5 4,4 4,4 5,5 5,4.5 4.5)),((6 2,5 2,5 3,6 3,7 3,8 2,7 2,6 2)),((8 6,8 7,9 7,9 6,9 5,8 5,8 6)),((8 1,9 0,8 0,7 0,8 1)))";

static std::string const neighbouring
    = "MULTIPOLYGON(((0 0,0 10,10 10,10 0,0 0)),((10 10,10 20,20 20,20 10,10 10)))";

static std::string const neighbouring_with_holes
    = "MULTIPOLYGON(((0 0,0 10,10 10,10 0,0 0),(4 4,4 6,6 6,6 4,4 4)),((10 10,10 20,20 20,20 10,10 10),(14 14,14 16,16 16,16 14,14 14)))";


// MultiPolygons causing assertion failure
static std::string const mysql_report_2015_07_05_1
    = "MULTIPOLYGON(((44 25,2.68435e+08 1.75922e+13,-6.87195e+10 -6.87195e+10,44 25),(-9704 8028,1.01776e+308 4.42027e+307,1.34218e+08 7.20576e+16,-9704 8028),(27945 15972,13 24,18 34,27945 15972),(3.3444e+307 1.47467e+308,1.22536e+38 1.42547e+38,10 -88,-28578 24802,3.3444e+307 1.47467e+308)))";
static std::string const mysql_report_2015_07_05_2
    = "MULTIPOLYGON(((19777 -21893,3.22595e+307 6.86823e+307,-40 -13,19777 -21893)),((-1322 4851,8.49998e+307 3.94481e+307,75 -69,8.64636e+307 3.94909e+307,-1.15292e+18 7.20576e+16,-1322 4851)))";

template <bool Clockwise, typename P>
void test_all()
{
    typedef bg::model::polygon<P, Clockwise> polygon_type;
    typedef bg::model::multi_polygon<polygon_type> multi_polygon_type;

    bg::strategy::buffer::join_miter join_miter;
    bg::strategy::buffer::join_round join_round(100);
    bg::strategy::buffer::join_round join_round_rough(12);
    bg::strategy::buffer::end_flat end_flat;

    bg::strategy::buffer::join_round join_round32(32);
    bg::strategy::buffer::end_round end_round32(32);

    test_one<multi_polygon_type, polygon_type>("triangles424", triangles, join_miter, end_flat, 417.910, 4.24);
    test_one<multi_polygon_type, polygon_type>("triangles425", triangles, join_miter, end_flat, 418.918, 4.25);
    test_one<multi_polygon_type, polygon_type>("triangles426", triangles, join_miter, end_flat, 419.927, 4.26);
    test_one<multi_polygon_type, polygon_type>("zonethru_10", zonethru, join_miter, end_flat, 96.0000, 1.0);

    test_one<multi_polygon_type, polygon_type>("multi_simplex_05", simplex, join_round, end_flat, 23.7030, 0.5);
    test_one<multi_polygon_type, polygon_type>("multi_simplex_05", simplex, join_miter, end_flat, 24.5965, 0.5);
    test_one<multi_polygon_type, polygon_type>("multi_simplex_10", simplex, join_round, end_flat, 34.2532, 1.0);
    test_one<multi_polygon_type, polygon_type>("multi_simplex_10", simplex, join_miter, end_flat, 38.1379, 1.0);
    test_one<multi_polygon_type, polygon_type>("multi_simplex_20", simplex, join_round, end_flat, 59.9159, 2.0);
    test_one<multi_polygon_type, polygon_type>("multi_simplex_20", simplex, join_miter, end_flat, 77.7060, 2.0);
    test_one<multi_polygon_type, polygon_type>("multi_simplex_50", simplex, join_round, end_flat, 174.46, 5.0);
    test_one<multi_polygon_type, polygon_type>("multi_simplex_50", simplex, join_miter, end_flat, 298.797, 5.0);

    test_one<multi_polygon_type, polygon_type>("multi_simplex_01", simplex, join_round, end_flat, 9.7514, -0.1);
    test_one<multi_polygon_type, polygon_type>("multi_simplex_05", simplex, join_round, end_flat, 3.2019, -0.5);
    test_one<multi_polygon_type, polygon_type>("multi_simplex_10", simplex, join_round, end_flat, 0.2012, -1.0);
    test_one<multi_polygon_type, polygon_type>("multi_simplex_12", simplex, join_round, end_flat, 0.0, -1.2);

    test_one<multi_polygon_type, polygon_type>("zonethru_05", zonethru, join_round, end_flat, 67.4627, 0.5);
    test_one<multi_polygon_type, polygon_type>("zonethru_05", zonethru, join_miter, end_flat, 68.0000, 0.5);
    test_one<multi_polygon_type, polygon_type>("zonethru_10", zonethru, join_round, end_flat, 93.8508, 1.0);
    test_one<multi_polygon_type, polygon_type>("zonethru_10", zonethru, join_miter, end_flat, 96.0000, 1.0);
    test_one<multi_polygon_type, polygon_type>("zonethru_15", zonethru, join_round, end_flat, 114.584, 1.5);
    test_one<multi_polygon_type, polygon_type>("zonethru_15", zonethru, join_miter, end_flat, 117.000, 1.5);

    test_one<multi_polygon_type, polygon_type>("wrapped_05", wrapped, join_round, end_flat, 104.570, 0.5);
    test_one<multi_polygon_type, polygon_type>("wrapped_05", wrapped, join_miter, end_flat, 105.000, 0.5);
    test_one<multi_polygon_type, polygon_type>("wrapped_10", wrapped, join_round, end_flat, 142.281, 1.0);
    test_one<multi_polygon_type, polygon_type>("wrapped_10", wrapped, join_miter, end_flat, 144.000, 1.0);
    test_one<multi_polygon_type, polygon_type>("wrapped_15", wrapped, join_round, end_flat, 167.066, 1.5);
    test_one<multi_polygon_type, polygon_type>("wrapped_15", wrapped, join_miter, end_flat, 169.000, 1.5);

    test_one<multi_polygon_type, polygon_type>("wrapped_05", wrapped, join_round, end_flat, 33.215, -0.5);
    test_one<multi_polygon_type, polygon_type>("wrapped_05", wrapped, join_miter, end_flat, 33.000, -0.5);
    test_one<multi_polygon_type, polygon_type>("wrapped_15", wrapped, join_round, end_flat, 0.0, -1.5);
    test_one<multi_polygon_type, polygon_type>("wrapped_15", wrapped, join_miter, end_flat, 0.0, -1.5);
    test_one<multi_polygon_type, polygon_type>("wrapped_25", wrapped, join_round, end_flat, 0.0, -2.5);
    test_one<multi_polygon_type, polygon_type>("wrapped_25", wrapped, join_miter, end_flat, 0.0, -2.5);
    test_one<multi_polygon_type, polygon_type>("wrapped_50", wrapped, join_round, end_flat, 0.0, -5.0);
    test_one<multi_polygon_type, polygon_type>("wrapped_50", wrapped, join_miter, end_flat, 0.0, -5.0);

    test_one<multi_polygon_type, polygon_type>("nested_05", nested, join_round, end_flat, 191.570, 0.5);
    test_one<multi_polygon_type, polygon_type>("nested_05", nested, join_round, end_flat, 64.430, -0.5);
    test_one<multi_polygon_type, polygon_type>("nested_10", nested, join_round, end_flat, 254.279, 1.0);
    test_one<multi_polygon_type, polygon_type>("nested_10", nested, join_round, end_flat, 1.721, -1.0);
    test_one<multi_polygon_type, polygon_type>("nested_25", nested, join_round, end_flat, 355.622, 2.5);
    test_one<multi_polygon_type, polygon_type>("nested_25", nested, join_round, end_flat, 0, -2.5);
    // 3.0 is exactly touching (for the deflate case)
    test_one<multi_polygon_type, polygon_type>("nested_30", nested, join_round, end_flat, 392.256, 3.0);
    test_one<multi_polygon_type, polygon_type>("nested_30", nested, join_round, end_flat, 0, -3.0);
    test_one<multi_polygon_type, polygon_type>("nested_29", nested, join_round, end_flat, 384.803, 2.9);
    test_one<multi_polygon_type, polygon_type>("nested_29", nested, join_round, end_flat, 0, -2.9);
    test_one<multi_polygon_type, polygon_type>("nested_31", nested, join_round, end_flat, 399.771, 3.1);
    test_one<multi_polygon_type, polygon_type>("nested_31", nested, join_round, end_flat, 0, -3.1);

    test_one<multi_polygon_type, polygon_type>("degenerate0", degenerate0, join_round, end_flat, 0.0, 1.0);
    test_one<multi_polygon_type, polygon_type>("degenerate1", degenerate1, join_round, end_flat, 5.708, 1.0);
    test_one<multi_polygon_type, polygon_type>("degenerate2", degenerate2, join_round, end_flat, 133.0166, 0.75);

    test_one<multi_polygon_type, polygon_type>("rt_a", rt_a, join_round, end_flat, 34.5381, 1.0);
    test_one<multi_polygon_type, polygon_type>("rt_a", rt_a, join_miter, end_flat, 36, 1.0);
    test_one<multi_polygon_type, polygon_type>("rt_b", rt_b, join_round, end_flat, 31.4186, 1.0);
    test_one<multi_polygon_type, polygon_type>("rt_b", rt_b, join_miter, end_flat, 34, 1.0);
    test_one<multi_polygon_type, polygon_type>("rt_c", rt_c, join_round, end_flat, 14.7093, 1.0);
    test_one<multi_polygon_type, polygon_type>("rt_c", rt_c, join_miter, end_flat, 16, 1.0);
    test_one<multi_polygon_type, polygon_type>("rt_d", rt_d, join_round, end_flat, 18.8726, 0.3);
    test_one<multi_polygon_type, polygon_type>("rt_e", rt_e, join_round, end_flat, 14.1866, 0.3);

    test_one<multi_polygon_type, polygon_type>("rt_g1", rt_g1, join_round, end_flat, 24.719, 1.0);
    test_one<multi_polygon_type, polygon_type>("rt_g3", rt_g3, join_miter, end_flat, 16.5711, 1.0);

#if ! defined(BOOST_GEOMETRY_NO_ROBUSTNESS)
    test_one<multi_polygon_type, polygon_type>("rt_d", rt_d, join_miter, end_flat, 19.8823, 0.3);
    test_one<multi_polygon_type, polygon_type>("rt_e", rt_e, join_miter, end_flat, 15.1198, 0.3);
    test_one<multi_polygon_type, polygon_type>("rt_f", rt_f, join_miter, end_flat, 4.60853, 0.3);
    test_one<multi_polygon_type, polygon_type>("rt_g1", rt_g1, join_miter, end_flat, 30.3137, 1.0);
    test_one<multi_polygon_type, polygon_type>("rt_g2", rt_g2, join_miter, end_flat, 18.5711, 1.0);
#endif

    test_one<multi_polygon_type, polygon_type>("rt_h", rt_h, join_round, end_flat, 47.6012, 1.0);
    test_one<multi_polygon_type, polygon_type>("rt_h", rt_h, join_miter, end_flat, 61.7058, 1.0);
    test_one<multi_polygon_type, polygon_type>("rt_i", rt_i, join_round, end_flat, 10.7528, 1.0);
    test_one<multi_polygon_type, polygon_type>("rt_i", rt_i, join_miter, end_flat, 13.6569, 1.0);
    test_one<multi_polygon_type, polygon_type>("rt_j", rt_j, join_round, end_flat, 28.7309, 1.0);
    test_one<multi_polygon_type, polygon_type>("rt_j", rt_j, join_miter, end_flat, 35.1421, 1.0);
    test_one<multi_polygon_type, polygon_type>("rt_k", rt_k, join_round, end_flat, 42.0092, 1.0);
    test_one<multi_polygon_type, polygon_type>("rt_k", rt_k, join_miter, end_flat, 48.0563, 1.0);

#if ! defined(BOOST_GEOMETRY_NO_ROBUSTNESS)
    test_one<multi_polygon_type, polygon_type>("rt_l", rt_l, join_miter, end_flat, 19.3995, 1.0);
#endif

    test_one<multi_polygon_type, polygon_type>("rt_m1", rt_m1, join_round, end_flat, 14.1074, 1.0);
    test_one<multi_polygon_type, polygon_type>("rt_m1", rt_m1, join_miter, end_flat, 19.4853, 1.0);
    test_one<multi_polygon_type, polygon_type>("rt_m2", rt_m2, join_miter, end_flat, 21.4853, 1.0);

    test_one<multi_polygon_type, polygon_type>("rt_n", rt_n,  join_miter, end_flat, 18.4853, 1.0);

    test_one<multi_polygon_type, polygon_type>("rt_o1", rt_o1, join_round, end_flat, 17.536, 1.0);

#if ! defined(BOOST_GEOMETRY_NO_ROBUSTNESS)
    test_one<multi_polygon_type, polygon_type>("rt_o1", rt_o1, join_miter, end_flat, 20.9142, 1.0);
    test_one<multi_polygon_type, polygon_type>("rt_o2", rt_o2, join_miter, end_flat, 25.7426, 1.0);
    test_one<multi_polygon_type, polygon_type>("rt_o3", rt_o3, join_miter, end_flat, 28.8247, 1.0);
    test_one<multi_polygon_type, polygon_type>("rt_o4", rt_o4, join_miter, end_flat, 34.6532, 1.0);
#endif

    test_one<multi_polygon_type, polygon_type>("rt_p1", rt_p1, join_miter, end_flat, 24.8211, 1.0);
    test_one<multi_polygon_type, polygon_type>("rt_p2", rt_p2, join_miter, end_flat, 21.4853, 1.0);
    test_one<multi_polygon_type, polygon_type>("rt_p3", rt_p3, join_miter, end_flat, 22.3995, 1.0);
    test_one<multi_polygon_type, polygon_type>("rt_p4", rt_p4, join_miter, end_flat, 33.0563, 1.0);

#if ! defined(BOOST_GEOMETRY_NO_ROBUSTNESS)
    test_one<multi_polygon_type, polygon_type>("rt_p5", rt_p5, join_miter, end_flat, 17, 1.0);
#endif

    test_one<multi_polygon_type, polygon_type>("rt_p6", rt_p6, join_miter, end_flat, 18.4853, 1.0);
    test_one<multi_polygon_type, polygon_type>("rt_p7", rt_p7, join_miter, end_flat, 26.2279, 1.0);
    test_one<multi_polygon_type, polygon_type>("rt_p8", rt_p8, join_miter, end_flat, 29.0563, 1.0);
    test_one<multi_polygon_type, polygon_type>("rt_p9", rt_p9, join_miter, end_flat, 26.1421, 1.0);
    test_one<multi_polygon_type, polygon_type>("rt_p10", rt_p10, join_miter, end_flat, 23.3995, 1.0);

    test_one<multi_polygon_type, polygon_type>("rt_p11", rt_p11, join_miter, end_flat, 28.7426, 1.0);
    test_one<multi_polygon_type, polygon_type>("rt_p12", rt_p12, join_miter, end_flat, 22.5711, 1.0);
    test_one<multi_polygon_type, polygon_type>("rt_p13", rt_p13, join_miter, end_flat, 19.9142, 1.0);
    test_one<multi_polygon_type, polygon_type>("rt_p14", rt_p14, join_miter, end_flat, 20.8284, 1.0);
    test_one<multi_polygon_type, polygon_type>("rt_p15", rt_p15, join_miter, end_flat, 23.6569, 1.0);
    test_one<multi_polygon_type, polygon_type>("rt_p16", rt_p16, join_miter, end_flat, 23.4853, 1.0);

#if ! defined(BOOST_GEOMETRY_NO_ROBUSTNESS)
    test_one<multi_polygon_type, polygon_type>("rt_p17", rt_p17, join_miter, end_flat, 25.3137, 1.0);
#endif

    test_one<multi_polygon_type, polygon_type>("rt_p18", rt_p18, join_miter, end_flat, 23.3137, 1.0);
    test_one<multi_polygon_type, polygon_type>("rt_p19", rt_p19, join_miter, end_flat, 25.5637, 1.0);
    test_one<multi_polygon_type, polygon_type>("rt_p20", rt_p20, join_miter, end_flat, 25.4853, 1.0);

#if ! defined(BOOST_GEOMETRY_NO_ROBUSTNESS)
    test_one<multi_polygon_type, polygon_type>("rt_p21", rt_p21, join_miter, end_flat, 17.1716, 1.0);
#endif

    test_one<multi_polygon_type, polygon_type>("rt_p22", rt_p22, join_miter, end_flat, 26.5711, 1.0);

    test_one<multi_polygon_type, polygon_type>("rt_q1", rt_q1, join_miter, end_flat, 27, 1.0);
    test_one<multi_polygon_type, polygon_type>("rt_q2", rt_q2, join_miter, end_flat, 26.4853, 1.0);
    test_one<multi_polygon_type, polygon_type>("rt_q2", rt_q2, join_miter, end_flat, 0.9697, -0.25);

#if ! defined(BOOST_GEOMETRY_NO_ROBUSTNESS)
    test_one<multi_polygon_type, polygon_type>("rt_r", rt_r, join_miter, end_flat, 21.0761, 1.0);
#endif

    test_one<multi_polygon_type, polygon_type>("rt_s1", rt_s1, join_miter, end_flat, 20.4853, 1.0);
    test_one<multi_polygon_type, polygon_type>("rt_s2", rt_s2, join_miter, end_flat, 24.6495, 1.0);

#if ! defined(BOOST_GEOMETRY_NO_ROBUSTNESS)
    test_one<multi_polygon_type, polygon_type>("rt_t1", rt_t, join_miter, end_flat, 15.6569, 1.0);
#endif

    test_one<multi_polygon_type, polygon_type>("rt_t2", rt_t, join_miter, end_flat, 0.1679, -0.25);

    test_one<multi_polygon_type, polygon_type>("rt_u1", rt_u1, join_round, end_flat, 33.2032, 1.0);
    test_one<multi_polygon_type, polygon_type>("rt_u2", rt_u2, join_round, end_flat, 138.8001, 1.0);
    test_one<multi_polygon_type, polygon_type>("rt_u3", rt_u3, join_round, end_flat, 133.4526, 1.0);
    test_one<multi_polygon_type, polygon_type>("rt_u4", rt_u4, join_round, end_flat, 126.9268, 1.0);
    test_one<multi_polygon_type, polygon_type>("rt_u5", rt_u5, join_round, end_flat, 78.4906, 1.0);
    test_one<multi_polygon_type, polygon_type>("rt_u6", rt_u6, join_round, end_flat, 115.4461, 1.0);

    test_one<multi_polygon_type, polygon_type>("rt_u7", rt_u7, join_miter, end_flat, 42.6421, 1.0);
    test_one<multi_polygon_type, polygon_type>("rt_u7", rt_u7, join_round, end_flat, 35.6233, 1.0);
    test_one<multi_polygon_type, polygon_type>("rt_u7_rough", rt_u7, join_round_rough, end_flat, 35.1675, 1.0);

    test_one<multi_polygon_type, polygon_type>("rt_u8", rt_u8, join_miter, end_flat, 70.9142, 1.0);
    test_one<multi_polygon_type, polygon_type>("rt_u9", rt_u9, join_miter, end_flat, 59.3063, 1.0);
    test_one<multi_polygon_type, polygon_type>("rt_u10", rt_u10, join_miter, end_flat, 144.0858, 1.0);
    test_one<multi_polygon_type, polygon_type>("rt_u10_50", rt_u10, join_miter, end_flat, 0.2145, -0.50);
    test_one<multi_polygon_type, polygon_type>("rt_u10_25", rt_u10, join_miter, end_flat, 9.6682, -0.25);

    test_one<multi_polygon_type, polygon_type>("rt_u11", rt_u11, join_miter, end_flat, 131.3995, 1.0);
    test_one<multi_polygon_type, polygon_type>("rt_u11_50", rt_u11, join_miter, end_flat, 0.04289, -0.50);
    test_one<multi_polygon_type, polygon_type>("rt_u11_25", rt_u11, join_miter, end_flat, 10.1449, -0.25);

#if defined(BOOST_GEOMETRY_BUFFER_USE_SIDE_OF_INTERSECTION)
    test_one<multi_polygon_type, polygon_type>("rt_u12", rt_u12, join_miter, end_flat, 142.1348, 1.0);
    test_one<multi_polygon_type, polygon_type>("rt_u13", rt_u13, join_miter, end_flat, 115.4853, 1.0);
#endif

    test_one<multi_polygon_type, polygon_type>("neighbouring_small",
        neighbouring,
        join_round32, end_round32, 128, -1);
    test_one<multi_polygon_type, polygon_type>("neighbouring_with_holes_small",
        neighbouring_with_holes,
        join_round32, end_round32, 97.757, -1);
    test_one<multi_polygon_type, polygon_type>("neighbouring_large",
        neighbouring,
        join_round32, end_round32, 0, -10);
    test_one<multi_polygon_type, polygon_type>("neighbouring_with_holes_large",
        neighbouring_with_holes,
        join_round32, end_round32, 0, -10);

    test_one<multi_polygon_type, polygon_type>("mysql_report_2015_07_05_1",
        mysql_report_2015_07_05_1,
        join_round32, end_round32, 6.04454566324708726e+23, 5526,
        same_distance, false, 1e+020);
    test_one<multi_polygon_type, polygon_type>("mysql_report_2015_07_05_2",
        mysql_report_2015_07_05_2,
        join_round32, end_round32, 0, 948189399);
}

int test_main(int, char* [])
{
    test_all<true, bg::model::point<double, 2, bg::cs::cartesian> >();
    test_all<false, bg::model::point<double, 2, bg::cs::cartesian> >();
    //test_all<bg::model::point<ttmath_big, 2, bg::cs::cartesian> >();
    
    return 0;
}

// intersecting:
//= "MULTIPOLYGON(((8 6,9 7,9 6,8 6)),((0 6,0 7,1 6,0 6)),((1 6,1 7,2 7,2 6,1 6)),((2 0,2 1,3 0,2 0)),((9 2,10 3,10 2,9 2)),((7 0,7 1,8 1,7 0)),((4 6,5 7,5 6,4 6)),((7 1,7 2,8 2,7 1)),((0 8,0 9,1 8,0 8)),((0 1,0 2,1 2,0 1)),((7 2,7 3,8 3,7 2)),((9 8,9 9,10 8,9 8)),((5 2,5 3,6 3,6 2,5 2)),((1 1,1 2,2 2,2 1,1 1)),((5 3,4 2,4 3,5 4,6 5,6 6,6 7,7 7,7 6,8 6,7 5,8 4,7 4,6 4,5 3)),((3 4,2 4,2 5,2 6,3 6,3 5,4 5,3 4)),((3 7,2 7,2 8,3 8,4 8,5 8,4 7,3 7)))";
