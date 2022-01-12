// Boost.Geometry (aka GGL, Generic Geometry Library)
// Unit Test

// Copyright (c) 2010-2012 Barend Gehrels, Amsterdam, the Netherlands.

// This file was modified by Oracle on 2016.
// Modifications copyright (c) 2016, Oracle and/or its affiliates.
// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_GEOMETRY_TEST_MULTI_OVERLAY_CASES_HPP
#define BOOST_GEOMETRY_TEST_MULTI_OVERLAY_CASES_HPP


#include <string>

// See powerpoint doc/other/test_cases/overlay_cases.ppt
// Note that there are some duplicates with single (80 and on)

static std::string case_multi_simplex[2] =
{
    "MULTIPOLYGON(((0 1,2 5,5 3,0 1)),((1 1,5 2,5 0,1 1)))",
    "MULTIPOLYGON(((3 0,0 3,4 5,3 0)))"
};

// To support exact behavior in integer coordinates (rectangular and diagonal)
static std::string case_multi_rectangular[2] =
{
    "MULTIPOLYGON(((100 100,100 200,200 200,200 100,100 100)),"
                 "((300 100,300 200,400 200,400 100,300 100),"
                   "(325 125,375 125,375 175,325 175,325 125)))",
    "MULTIPOLYGON(((150 50,150 150,350 150,350 50,150 50)))"
};

static std::string case_multi_diagonal[2] =
{
    "MULTIPOLYGON(((40 0,0 40,40 80,80 40,40 0),(50 20,60 30,50 40,40 30,50 20)))",
    "MULTIPOLYGON(((80 0,40 40,80 80,120 40,80 0),(80 30,90 40,80 50,70 40,80 30)))"
};

static std::string case_multi_hard[2] =
{
    "MULTIPOLYGON(((0 0,0 4,2 4,2 3,4 3,4 0,0 0)))",
    "MULTIPOLYGON(((2 7,4 7,4 2.99959993362426758,2 3,2 7)))"
};

// To mix multi/single
static std::string case_single_simplex = "POLYGON((3 0,0 3,4 5,3 0))";

static std::string case_multi_no_ip[2] =
{
    "MULTIPOLYGON(((4 1,0 7,7 9,4 1)),((8 1,6 3,10 4,8 1)),((12 6,10 7,13 8,12 6)))",
    "MULTIPOLYGON(((14 4,8 8,15 10,14 4)),((15 3,18 9,20 2,15 3)),((3 4,1 7,5 7,3 4)))"
};

static std::string case_multi_2[2] =
{
    "MULTIPOLYGON(((4 3,2 7,10 9,4 3)),((8 1,6 3,10 4,8 1)),((12 6,10 7,13 8,12 6)))",
    "MULTIPOLYGON(((14 4,8 8,15 10,14 4)),((15 3,18 9,20 2,15 3)),((5 5,4 7,7 7,5 5)))"
};

// Case 58, same as case_58 IET (single) but here the second polygon is inverted
// To check behaviour of difference, but in an intersection operation
static std::string case_58_multi[8] =
{
    /* a */ "MULTIPOLYGON(((3 3,3 4,4 4,4 3,3 3)))",
    /* b */ "MULTIPOLYGON(((0 2,0 5,4 4,5 0,0 2),(4 4,1 4,1 3,4 4),(4 4,2 3,2 2,4 4),(4 4,3 2,4 2,4 4)))",
    /* a inv */ "MULTIPOLYGON(((-1 -1,-1 6,6 6,6 -1,-1 -1),(3 3,4 3,4 4,3 4,3 3)))",
    /* b inv */ "MULTIPOLYGON(((6 6,6 0,5 0,4 4,0 5,0 6,6 6)),((4 4,1 3,1 4,4 4)),((4 4,2 2,2 3,4 4)),((4 4,4 2,3 2,4 4)))",

    // simpler versions of b
    /* b */ "MULTIPOLYGON(((0 2,0 5,4 4,5 0,0 2),(4 4,1 4,1 3,4 4)))",
    /* b */ "MULTIPOLYGON(((0 2,0 5,4 4,5 0,0 2),(4 4,3 2,4 2,4 4)))",
    /* b */ "MULTIPOLYGON(((0 2,0 5,4 4,5 0,0 2),(4 4,2 3,2 2,4 4)))",
    /* b */ "MULTIPOLYGON(((0 2,0 5,4 4,5 0,0 2),(4 4,2 3,2 2,4 4),(4 4,3 2,4 2,4 4)))",
};

static std::string case_61_multi[2] =
{
    // extracted from recursive boxes
    "MULTIPOLYGON(((1 1,1 2,2 2,2 1,1 1)),((2 2,2 3,3 3,3 2,2 2)))",
    "MULTIPOLYGON(((1 2,1 3,2 3,2 2,1 2)),((2 3,2 4,3 4,3 3,2 3)))"
};

static std::string case_62_multi[2] =
{
    // extracted from recursive boxes
    "MULTIPOLYGON(((1 2,1 3,2 3,2 2,1 2)))",
    "MULTIPOLYGON(((1 2,1 3,2 3,2 2,1 2)),((2 3,2 4,3 4,3 3,2 3)))"
};

static std::string case_63_multi[2] =
{
    // extracted from recursive boxes
    "MULTIPOLYGON(((1 2,1 3,2 3,2 2,1 2)))",
    "MULTIPOLYGON(((1 2,1 3,2 3,2 2,1 2)),((2 1,2 2,3 2,3 1,2 1)))"
};

static std::string case_64_multi[3] =
{
    // extracted from recursive boxes
    "MULTIPOLYGON(((1 1,1 2,2 2,2 1,1 1)),((2 2,2 3,3 3,3 2,2 2)))",
    "MULTIPOLYGON(((1 1,1 2,2 2,3 2,3 1,2 1,1 1)))" ,
    // same but omitting not-necessary form-points at x=2 (==simplified)
    "MULTIPOLYGON(((1 1,1 2,3 2,3 1,1 1)))"
};

static std::string case_65_multi[4] =
{
    "MULTIPOLYGON(((2 2,2 3,3 3,3 2,2 2)))",
    "MULTIPOLYGON(((1 1,1 2,2 2,2 1,1 1)),((2 2,2 3,3 3,3 2,2 2)),((3 1,3 2,5 2,5 1,3 1)))",

    // Inverse versions
    "MULTIPOLYGON(((0 0,0 4,6 4,6 0,0 0),(2 2,3 2,3 3,2 3,2 2)))",
    "MULTIPOLYGON(((0 0,0 4,6 4,6 0,0 0),(1 1,2 1,2 2,1 2,1 1),(2 2,3 2,3 3,2 3,2 2),(3 1,5 1,5 2,3 2,3 1)))"
};

static std::string case_66_multi[2] =
{
    "MULTIPOLYGON(((3 5,2 5,2 6,3 6,4 6,4 5,3 5)),((1 6,0 6,0 7,1 7,2 7,2 6,1 6)))",
    "MULTIPOLYGON(((1 4,1 5,2 5,2 4,1 4)),((1 7,2 7,2 6,1 6,1 7)),((0 8,0 9,1 9,1 8,1 7,0 7,0 8)))"
};

static std::string case_67_multi[2] =
{
    "MULTIPOLYGON(((1 2,1 3,2 3,2 2,1 2)),((2 1,2 2,3 2,3 1,2 1)))",
    "MULTIPOLYGON(((1 1,1 2,3 2,3 1,1 1)))"
};

static std::string case_68_multi[2] =
{
    "MULTIPOLYGON(((2 1,2 2,4 2,4 1,2 1)),((4 2,4 3,5 3,5 2,4 2)))",
    "MULTIPOLYGON(((1 2,1 3,2 3,2 2,1 2)),((2 1,2 2,3 2,3 1,2 1)),((3 2,3 3,5 3,5 2,3 2)))"
};

static std::string case_69_multi[2] =
{
    "MULTIPOLYGON(((1 1,1 2,2 2,2 1,1 1)),((3 2,3 3,4 3,4 2,3 2)))",
    "MULTIPOLYGON(((2 0,2 1,3 1,3 0,2 0)),((1 1,1 3,2 3,2 1,1 1)),((2 3,2 4,3 4,3 3,2 3)))"
};

static std::string case_71_multi[2] =
{
    "MULTIPOLYGON(((0 0,0 3,1 3,1 1,3 1,3 2,4 2,4 0,0 0)),((2 2,2 3,3 3,3 2,2 2)))",
    "MULTIPOLYGON(((0 2,0 3,3 3,3 2,0 2)))"
};

static std::string case_72_multi[3] =
{
    // cluster with ii, done by both traverse and assemble
    "MULTIPOLYGON(((0 3,4 4,3 0,3 3,0 3)),((3 3,2 1,1 2,3 3)))",
    "MULTIPOLYGON(((0 0,1 4,3 3,4 1,0 0)))",

    // Inverse version of a
    "MULTIPOLYGON(((-1 -1,-1 5,5 5,5 -1,-1 -1),(0 3,3 3,3 0,4 4,0 3),(3 3,1 2,2 1,3 3)))"
};

static std::string case_73_multi[2] =
{
    "MULTIPOLYGON(((2 2,2 3,3 3,3 2,2 2)),((1 1,1 2,2 2,2 1,1 1)))",
    "MULTIPOLYGON(((1 1,1 2,2 2,2 3,3 3,3 1,1 1)))"
};

static std::string case_74_multi[2] =
{
    "MULTIPOLYGON(((3 0,2 0,2 1,3 1,3 3,1 3,1 2,2 2,2 1,0 1,0 5,4 5,4 0,3 0)))",
    "MULTIPOLYGON(((0 2,0 3,1 3,1 1,2 1,2 0,0 0,0 2)),((2 3,1 3,1 4,2 4,2 3)))"
};

static std::string case_75_multi[2] =
{
    // cc/uu turns on all corners of second box
    "MULTIPOLYGON(((1 1,1 2,2 2,2 1,1 1)),((1 3,1 4,2 4,2 3,1 3)),((2 2,2 3,3 3,3 2,2 2)),((3 1,3 2,4 2,4 1,3 1)),((3 3,3 4,4 4,4 3,3 3)))",
    "MULTIPOLYGON(((2 2,2 3,3 3,3 2,2 2)))"
};

static std::string case_76_multi[2] =
{
    // cc/uu turns on all corners of second box, might generate TWO OVERLAPPING union polygons!
    // therefore, don't follow uu.
    "MULTIPOLYGON(((1 0,1 1,2 1,2 0,1 0)),((3 2,4 2,4 1,3 1,3 2)),((2 2,2 3,3 3,3 2,2 2)),((2 3,1 3,1 4,2 4,2 3)),((3 3,3 4,4 4,4 3,3 3)))",
    "MULTIPOLYGON(((0 2,0 3,1 3,1 2,2 2,2 0,1 0,1 1,0 1,0 2)),((2 2,2 3,3 3,3 2,2 2)))"
};

static std::string case_77_multi[2] =
{
    // with a point on interior-ring-border of enclosing
    // -> went wrong in the assemble phase for intersection (traversal is OK)
    // -> fixed
    "MULTIPOLYGON(((3 3,3 4,4 4,4 3,3 3)),((5 3,5 4,4 4,4 5,3 5,3 6,5 6,5 5,7 5,7 6,8 6,8 5,9 5,9 2,8 2,8 1,7 1,7 2,5 2,5 3),(6 3,8 3,8 4,6 4,6 3)))",
    "MULTIPOLYGON(((6 3,6 4,7 4,7 3,6 3)),((2 3,1 3,1 4,3 4,3 5,4 5,4 6,5 6,5 7,9 7,9 4,7 4,7 5,8 5,8 6,7 6,7 5,6 5,6 4,4 4,4 3,3 3,3 2,2 2,2 3)),((5 2,4 2,4 3,6 3,6 2,5 2)),((7 2,7 3,8 3,8 2,8 1,7 1,7 2)))"
};

static std::string case_78_multi[2] =
{
    "MULTIPOLYGON(((0 0,0 5,5 5,5 0,0 0),(2 2,4 2,4 3,2 3,2 2)))",
    "MULTIPOLYGON(((0 0,0 5,5 5,5 0,0 0),(3 2,4 2,4 3,3 3,3 2),(1 1,2 1,2 2,1 2,1 1)))"

};

static std::string case_80_multi[2] =
{
    // Many ux-clusters -> needs correct cluster-sorting
    // Fixed now
    "MULTIPOLYGON(((3 1,3 2,4 2,3 1)),((1 5,0 4,0 5,1 6,1 5)),((3 3,4 3,3 2,2 2,2 3,3 3)),((4 5,5 6,5 5,4 5)),((4 2,4 3,5 3,4 2)),((2.5 5.5,3 5,2 5,2 7,3 6,2.5 5.5)),((1 6,0 6,0 7,1 7,2 6,1 6)))",
    "MULTIPOLYGON(((3 5,3 6,4 6,4 5,3 5)),((4 4,5 5,5 4,4 4)),((3 3,4 4,4 3,3 3)),((1 5,1 6,2 6,2 5,1 5)),((0 6,1 7,1 6,0 6)),((1 4,1 3,0 3,0 4,1 4)),((3 5,4 4,3 4,3 3,2 3,2 5,3 5)))"
};

static std::string case_81_multi[2] =
{
    "MULTIPOLYGON(((1 1,2 2,2 1,1 1)),((2 2,2 3,3 2,2 2)),((3 1,4 2,4 1,3 1)))",
    "MULTIPOLYGON(((2 1,2 2,3 3,3 2,4 2,3 1,2 1)))"
};

static std::string case_82_multi[2] =
{
    "MULTIPOLYGON(((4 0,5 1,5 0,4 0)),((2 1,3 2,3 1,2 1)),((3 0,4 1,4 0,3 0)),((1 0,1 1,2 1,2 0,1 0)))",
    "MULTIPOLYGON(((3 2,4 3,4 2,3 2)),((3 1,3 2,4 1,3 1)),((0 0,1 1,1 0,0 0)),((5 1,5 0,4 0,4 1,5 1)))"
};

static std::string case_83_multi[2] =
{
    // iu/iu
    "MULTIPOLYGON(((1 0,1 1,2 1,1 0)),((0 1,0 4,1 4,1 1,0 1)),((2 1,2 2,3 2,3 1,2 1)),((2 3,3 4,3 3,2 3)))",
    "MULTIPOLYGON(((1 0,2 1,2 0,1 0)),((0 3,1 4,1 3,0 3)),((2 3,2 4,3 3,2 3)),((1 3,2 3,2 2,0 2,1 3)))"
};

static std::string case_84_multi[2] =
{
    // iu/ux
    "MULTIPOLYGON(((2 2,3 3,3 2,2 2)),((2 1,2 2,3 1,2 1)),((2 3,3 4,3 3,2 3)),((1 3,2 4,2 2,1 2,1 3)))",
    "MULTIPOLYGON(((2 3,3 3,3 1,2 1,2 2,1 2,1 3,2 3)))"
};

static std::string case_85_multi[2] =
{
    // iu/ux (and ux/ux)
    "MULTIPOLYGON(((0 1,0 2,1 2,0 1)),((1 1,1 2,2 1,1 1)),((0 3,1 3,0 2,0 3)))",
    "MULTIPOLYGON(((1 3,2 3,2 1,1 1,1 2,0 2,1 3)))"
};

static std::string case_86_multi[2] =
{
    // iu/ux
    "MULTIPOLYGON(((4 2,4 3,5 3,4 2)),((5 2,6 3,6 2,5 2)),((5 1,4 1,4 2,5 2,6 1,5 1)))",
    "MULTIPOLYGON(((5 1,5 2,6 2,6 1,5 1)),((4 2,5 3,5 2,4 2)),((3 2,4 3,4 2,3 2)))"
};

static std::string case_87_multi[2] =
{
    // iu/ux where iu crosses, no touch
    "MULTIPOLYGON(((5 0,5 1,6 0,5 0)),((6 2,7 3,7 2,6 2)),((5 1,5 3,6 3,6 1,5 1)))",
    "MULTIPOLYGON(((5 1,5 2,7 2,7 1,6 1,6 0,5 0,5 1)),((4 3,5 3,5 2,3 2,4 3)))"
};


static std::string case_88_multi[2] =
{
    "MULTIPOLYGON(((0 0,0 1,1 0,0 0)),((1 1,1 2,2 1,1 1)),((0 2,0 3,1 3,2 3,2 2,1 2,0 1,0 2)))",
    "MULTIPOLYGON(((0 0,0 1,1 0,0 0)),((0 1,1 2,1 1,0 1)),((0 2,0 3,1 3,1 2,0 2)))"
};

static std::string case_89_multi[2] =
{
    // Extract from rec.boxes_3
    "MULTIPOLYGON(((8 1,7 1,8 2,8 3,9 4,9 2,8.5 1.5,9 1,8 0,8 1)),((9 1,9 2,10 2,10 1,9 0,9 1)))",
    "MULTIPOLYGON(((8 3,9 4,9 3,8 3)),((7 0,7 1,8 1,8 0,7 0)),((9 2,9 1,8 1,8 3,8.5 2.5,9 3,9 2)))"
};

static std::string case_90_multi[2] =
{
    // iu/iu for Union; see ppt
    "MULTIPOLYGON(((1 8,0 8,0 10,1 10,1 9,2 8,2 7,1 7,1 8)),((2 9,2 10,4 10,4 9,3 9,3 8,2 8,2 9)))",
    "MULTIPOLYGON(((2 8,1 8,1 9,2 9,2 10,3 10,3 8,2 8)),((0 10,2 10,0 8,0 10)))"
};

static std::string case_91_multi[2] =
{
    // iu/xi for Intersection
    "MULTIPOLYGON(((3 3,3 4,4 4,3 3)),((2 2,1 2,1 4,2 4,2 3,3 3,2 2)))",
    "MULTIPOLYGON(((2 2,2 3,3 2,2 2)),((2 3,1 3,1 4,1.5 3.5,2 4,2.5 3.5,3 4,3 3,2 3)))"
};

static std::string case_92_multi[2] =
{
    // iu/iu all aligned (for union)
    "MULTIPOLYGON(((7 2,7 3,8 2,7 2)),((8 4,9 5,9 4,8 4)),((8 2,8 3,9 2,8 2)),((7 3,7 4,8 4,8 3,7 3)),((9 3,9 4,10 4,10 3,9 3)))",
    "MULTIPOLYGON(((9 2,8 2,8 3,9 3,10 2,9 2)),((7 5,8 5,9 6,9 4,8 4,7 3,6 3,6 4,6.5 3.5,7 4,6 4,7 5)))"
};

static std::string case_93_multi[2] =
{
    // iu/xi for intersection
    "MULTIPOLYGON(((6 2,7 2,7 1,5 1,6 2)),((7 3,8 3,7.5 2.5,8 2,7 2,7 3)))",
    "MULTIPOLYGON(((7 1,6 0,6 2,7 3,7 2,8 3,8 2,7 1)))"
};


static std::string case_94_multi[2] =
{
    // iu/iu for union
    "MULTIPOLYGON(((9 2,9 3,10 3,10 2,9 2)),((7 3,8 4,9 3,8 3,9 2,7 2,7 3)),((8 6,9 5,9 4,8 4,8 6)))",
    "MULTIPOLYGON(((6 2,6 3,7 3,8 2,6 2)),((9 3,10 3,9 2,9 1,8 0,7 0,8 1,8 3,8.5 2.5,9 3)),((7 4,7 5,8 5,9 6,9 4,8 4,8 3,7 3,7 4)))"
};

static std::string case_95_multi[2] =
{
    // iu/iu for union
    "MULTIPOLYGON(((0 8,1 8,1 7,0 7,0 8)),((2 8,2 9,2.5 8.5,3 9,3 7,2 7,2 8)),((1 9,1 10,2 9,1 8,1 9)))",
    "MULTIPOLYGON(((1 7,0 7,0 8,1 8,2 7,1 7)),((2 9,1 9,1 10,2 10,3 9,4 9,4 8,2 8,2 9)))"
};

static std::string case_96_multi[2] =
{
    // iu/iu all collinear, for intersection/union
    "MULTIPOLYGON(((8 2,9 3,9 2,8 2)),((8 1,9 2,9 1,10 1,10 0,8 0,8 1)))",
    "MULTIPOLYGON(((9 0,9 1,10 0,9 0)),((8 1,8 2,9 2,9 1,8 1)))"
};

static std::string case_97_multi[2] =
{
    // ux/ux for union
    "MULTIPOLYGON(((4 4,4 5,4.5 4.5,5 5,6 5,5 4,5 3,4 3,4 4)))",
    "MULTIPOLYGON(((5 3,5 4,6 3,5 3)),((6 5,7 5,6 4,5 4,6 5)))"
};


static std::string case_98_multi[2] =
{
    // ii/iu for intersection, solved by discarding iu (ordering not possible)
    "MULTIPOLYGON(((2 0,3 1,3 0,2 0)),((2 2,2 3,1 3,1 4,2 4,3 3,3 4,5 2,4 2,4 1,3 1,3 2,2.5 1.5,3 1,2 1,2 2)))",
    "MULTIPOLYGON(((4 2,4 3,5 2,4 2)),((1 0,0 0,0 2,4 2,4 1,2 1,2 0,1 0)),((3 3,4 4,4 3,3 2,3 3)))"
};

static std::string case_99_multi[2] =
{
    // iu/iu for intersection
    "MULTIPOLYGON(((1 0,2 1,2 0,1 0)),((1 2,2 2,1.5 1.5,2 1,1 1,1 0,0 0,0 1,1 2)))",
    "MULTIPOLYGON(((1 1,2 0,0 0,1 1)),((1 1,0 1,0 2,1 2,2 3,2 2,1 1)))"
};

static std::string case_100_multi[2] =
{
    // for intersection
    "MULTIPOLYGON(((0 0,0 1,1 0,0 0)),((2 2,2 1,0 1,0 2,1 2,2 3,2 2)))",
    "MULTIPOLYGON(((1 1,1 2,2 2,2 1,1 1)),((1 2,0 1,0 3,1 4,1 2)))"
};

static std::string case_101_multi[4] =
{
    // interior ring / union
    "MULTIPOLYGON(((7 2,7 3,8 2,7 2)),((9 3,9 4,10 3,9 3)),((10 1,10 0,8 0,8 1,9 2,10 2,10 1)),((9 3,9 2,8 2,8 3,7 3,7 4,8 4,9 3)),((8 4,8 7,9 6,9 4,8 4)))",
    "MULTIPOLYGON(((5 1,5 2,6 3,6 4,7 5,6 5,7 6,8 6,8 5,9 5,10 5,10 1,8 1,7 0,5 0,5 1),(9 5,8 4,9 4,9 5),(8 1,8 3,7 3,7 2,6 2,7 1,8 1),(5 1,5.5 0.5,6 1,5 1),(8.5 2.5,9 2,9 3,8.5 2.5)))",

    // inverse versions
    "MULTIPOLYGON(((4 -1,4 8,11 8,11 -1,4 -1),(7 2,8 2,7 3,7 2),(9 3,10 3,9 4,9 3),(10 1,10 2,9 2,8 1,8 0,10 0,10 1),(9 3,8 4,7 4,7 3,8 3,8 2,9 2,9 3),(8 4,9 4,9 6,8 7,8 4)))",
    "MULTIPOLYGON(((4 -1,4 8,11 8,11 -1,4 -1),(5 1,5 0,7 0,8 1,10 1,10 5,9 5,8 5,8 6,7 6,6 5,7 5,6 4,6 3,5 2,5 1)),((9 5,9 4,8 4,9 5)),((8 1,7 1,6 2,7 2,7 3,8 3,8 1)),((5 1,6 1,5.5 0.5,5 1)),((8.5 2.5,9 3,9 2,8.5 2.5)))"
};

static std::string case_102_multi[4] =
{
    // interior ring 'fit' / union
    "MULTIPOLYGON(((0 2,0 7,5 7,5 2,0 2),(4 3,4 6,1 6,2 5,1 5,1 4,3 4,4 3)),((3 4,3 5,4 5,3 4)),((2 5,3 6,3 5,2 4,2 5)))",
    "MULTIPOLYGON(((0 2,0 7,5 7,5 2,0 2),(2 4,3 5,2 5,2 4),(4 4,3 4,3 3,4 4),(4 5,4 6,3 6,4 5)))",

    // inverse versions (first was already having an interior, so outer ring is just removed
    "MULTIPOLYGON(((4 3,3 4,1 4,1 5,2 5,1 6,4 6,4 3),(3 4,4 5,3 5,3 4),(2 5,2 4,3 5,3 6,2 5)))",
    "MULTIPOLYGON(((-1 1,-1 8,6 8,6 1,-1 1),(0 2,5 2,5 7,0 7,0 2)),((2 4,2 5,3 5,2 4)),((4 4,3 3,3 4,4 4)),((4 5,3 6,4 6,4 5)))"
};

static std::string case_103_multi[2] =
{
    // interior ring 'fit' (ix) / union / assemble
    "MULTIPOLYGON(((0 0,0 5,5 5,5 0,2 0,2 1,1 0,0 0),(2 1,3 1,3 2,2 2,2 1),(2 2,2 3,1 2,2 2)))",
    "MULTIPOLYGON(((0 0,0 5,5 5,5 0,0 0),(2 1,2 2,1 1,2 1)))"
};

static std::string case_104_multi[2] =
{
    // interior ring 'fit' (ii) / union / assemble
    "MULTIPOLYGON(((1 0,1 1,0 1,0 5,5 5,5 0,2 0,2 1,1 0),(2 2,3 3,2 3,2 2)))",
    "MULTIPOLYGON(((0 0,0 5,5 5,5 0,0 0),(1 1,3 1,3 2,1 2,1 1)))"
};

static std::string case_105_multi[2] =
{
    // interior ring 'fit' () / union / assemble
    "MULTIPOLYGON(((0 0,0 5,5 5,5 0,0 0),(2 2,3 2,3 3,1 3,2 2)))",
    "MULTIPOLYGON(((0 0,0 5,5 5,5 0,0 0),(1 1,2 1,2 2,1 1),(2 1,3 1,3 2,2 1),(1 3,2 3,2 4,1 4,1 3),(2 3,3 3,3 4,2 3)))"
};

static std::string case_106_multi[2] =
{
    // interior ring 'fit' () / union / assemble
    // [1] is reported as invalid by BG, but not by postgis
    "MULTIPOLYGON(((0 0,0 3,0 5,5 5,5 0,0 0),(0 3,1 2,1 3,0 3),(1 3,2 3,3 4,1 4,1 3),(2 3,2 2,3 3,2 3),(2 2,2 1,3 2,2 2)))",
    "MULTIPOLYGON(((0 0,0 5,1 5,1 4,2 4,2 5,3 5,3 3,4 4,5 4,5 0,2 0,3 1,2 1,1 1,0 0),(2 1,2 2,1.5 1.5,2 1),(2 2,2 3,1 3,2 2)),((2 0,1 0,2 1,2 0)))"
};

static std::string case_107_multi[4] =
{
    // For CCW polygon reports a iu/iu problem.
    "MULTIPOLYGON(((6 8,7 9,7 7,8 7,7 6,6 6,6 8)),((6.5 9.5,7 10,7 9,6 9,6 10,6.5 9.5)))",
    "MULTIPOLYGON(((5 7,6 8,6 10,7 9,8 10,8 8,7 8,6 7,6 6,5 7)))",

    // inverse versions
    "MULTIPOLYGON(((5 5,5 11,9 11,9 5,5 5),(6 8,6 6,7 6,8 7,7 7,7 9,6 8),(6.5 9.5,6 10,6 9,7 9,7 10,6.5 9.5)))",
    "MULTIPOLYGON(((4 5,4 11,9 11,9 5,4 5),(5 7,6 6,6 7,7 8,8 8,8 10,7 9,6 10,6 8,5 7)))"
};

static std::string case_108_multi[2] =
{
    // Missing intersection point in [0] / [1], [0] / [2] is OK
    "MULTIPOLYGON(((1 2,0 1,0 6,1 6,2 5,2 4,3 4,4 4,4 2,4 1,1 1,1 2),(1 2,2 2,3 3,1 3,1 2),(2 2,2.5 1.5,3 2,2 2),(2 4,1 4,1.5 3.5,2 4)))",
    "MULTIPOLYGON(((1 2,2 3,2 4,1 3,1 4,1 5,2 6,3 6,3 5,5 5,5 0,4 0,4 1,1 1,1 2),(2 4,2.5 3.5,3 4,2 4),(3 4,4 3,4 4,3 4),(4 3,3 3,3 2,4 2,4 3)),((0 3,1 3,1 2,0 2,0 3)),((0 3,0 4,1 4,0 3)))"
};

static std::string case_109_multi[2] =
    {
        "MULTIPOLYGON(((0 0,0 40,40 40,40 0,0 0),(10 10,30 10,30 30,10 30,10 10)))",
        "MULTIPOLYGON(((10 10,10 20,20 10,10 10)),((20 10,30 20,30 10,20 10)),((10 20,10 30,20 20,10 20)),((20 20,30 30,30 20,20 20)))"
    };

static std::string case_110_multi[2] =
    {
        "MULTIPOLYGON(((0 0,0 40,40 40,40 0,0 0),(10 10,30 10,30 30,10 30,10 10)))",
        "MULTIPOLYGON(((15 10,10 15,10 17,15 10)),((15 10,10 20,10 22,15 10)),((15 10,10 25,10 27,15 10)),((25 10,30 17,30 15,25 10)),((25 10,30 22,30 20,25 10)),((25 10,30 27,30 25,25 10)),((18 10,20 30,19 10,18 10)),((21 10,20 30,22 10,21 10)))"
    };


// Cases 111 to 122 are for testing uu-cases, validity, touch, interior rings
static std::string case_111_multi[2] =
{
    "MULTIPOLYGON(((4 0,2 2,4 4,6 2,4 0)))",
    "MULTIPOLYGON(((4 4,2 6,4 8,6 6,4 4)))"
};

static std::string case_112_multi[2] =
{
    "MULTIPOLYGON(((0 0,0 2,2 4,4 2,6 4,8 2,8 0,0 0)))",
    "MULTIPOLYGON(((0 8,8 8,8 6,6 4,4 6,2 4,0 6,0 8)))"
};

// Provided by Menelaos (1)
static std::string case_113_multi[2] =
{
    "MULTIPOLYGON(((0 0,0 10,10 10,10 0,0 0)),((15 5,15 10,20 10,20 5,15 5)))",
    "MULTIPOLYGON(((10 0,15 5,15 0,10 0)),((10 5,10 10,15 10,15 5,10 5)))"
};

// Provided by Menelaos (2)
static std::string case_114_multi[2] =
{
    "MULTIPOLYGON(((0 0,0 10,10 10,10 0,0 0)),((15 5,15 10,20 10,20 5,15 5)))",
    "MULTIPOLYGON(((10 0,15 5,20 5,20 0,10 0)),((10 5,10 10,15 10,15 5,10 5)))"
};

// Mailed by Barend
static std::string case_115_multi[2] =
{
    "MULTIPOLYGON(((4 0,2 2,4 4,6 2,4 0)),((4 6,6 8,8 6,6 4,4 6)))",
    "MULTIPOLYGON(((4 4,2 6,4 8,6 6,4 4)),((4 2,7 6,8 3,4 2)))"
};

// Formerly referred to as a
// Should result in 1 polygon with 2 holes
// "POLYGON((4 9,4 10,6 10,6 12,8 12,8 11,10 11,10 9,11 9,11 2,3 2,3 9,4 9),(6 10,6 8,7 8,7 10,6 10),(6 8,5 8,5 3,9 3,9 7,8 7,8 6,6 6,6 8))"
static std::string case_116_multi[2] =
{
    "MULTIPOLYGON(((4 8,4 10,6 10,6 8,4 8)),((7 7,7 11,10 11,10 7,7 7)))",
    "MULTIPOLYGON(((6 6,6 8,8 8,8 6,6 6)),((6 10,6 12,8 12,8 10,6 10)),((9 9,11 9,11 2,3 2,3 9,5 9,5 3,9 3,9 9)))"
};

// Formerly referred to as b
// Should result in 2 polygons
// "MULTIPOLYGON(((4 8,4 10,6 10,6 8,4 8)),((7 8,7 10,6 10,6 12,8 12,8 11,10 11,10 7,8 7,8 6,6 6,6 8,7 8)))"
static std::string case_117_multi[2] =
{
    "MULTIPOLYGON(((4 8,4 10,6 10,6 8,4 8)),((7 7,7 11,10 11,10 7,7 7)))",
    "MULTIPOLYGON(((6 6,6 8,8 8,8 6,6 6)),((6 10,6 12,8 12,8 10,6 10)))"
};

// Formerly referred to as c
// Shoud result in 3 polygons:
// "MULTIPOLYGON(((4 8,4 10,6 10,6 8,4 8)),((8 8,8 10,10 10,10 8,8 8)),((7 12,7 13,13 13,13 5,7 5,7 6,6 6,6 8,8 8,8 7,11 7,11 11,8 11,8 10,6 10,6 12,7 12)))"
static std::string case_118_multi[2] =
{
    "MULTIPOLYGON(((4 8,4 10,6 10,6 8,4 8)),((8 8,8 10,10 10,10 8,8 8)),((7 11,7 13,13 13,13 5,7 5,7 7,11 7,11 11,7 11)))",
    "MULTIPOLYGON(((6 6,6 8,8 8,8 6,6 6)),((6 10,6 12,8 12,8 10,6 10)))"
};

// Formerly referred to as d
// Should result in 2 polygons:
// "MULTIPOLYGON(((2 4,2 6,3 6,3 7,7 7,7 6,8 6,8 4,6 4,6 5,4 5,4 4,2 4)),((1 0,1 2,0 2,0 4,2 4,2 3,8 3,8 4,10 4,10 2,9 2,9 0,1 0)))"
static std::string case_119_multi[2] =
{
    "MULTIPOLYGON(((2 4,2 6,4 6,4 4,2 4)),((6 4,6 6,8 6,8 4,6 4)),((1 0,1 3,9 3,9 0,1 0)))",
    "MULTIPOLYGON(((0 2,0 4,2 4,2 2,0 2)),((8 2,8 4,10 4,10 2,8 2)),((3 5,3 7,7 7,7 5,3 5)))"
};

// With a c/c turn
static std::string case_120_multi[2] =
{
    "MULTIPOLYGON(((6 4,6 9,9 9,9 6,11 6,11 4,6 4)),((10 7,10 10,12 10,12 7,10 7)))",
    "MULTIPOLYGON(((10 5,10 8,12 8,12 5,10 5)),((6 10,8 12,10 10,8 8,6 10)))"
};

// With c/c turns in both involved polygons
static std::string case_121_multi[2] =
{
    "MULTIPOLYGON(((7 4,7 8,9 8,9 6,11 6,11 4,7 4)),((10 7,10 10,12 10,12 7,10 7)))",
    "MULTIPOLYGON(((10 5,10 8,12 8,12 5,10 5)),((7 7,7 10,10 10,9 9,9 7,7 7)))"
};

// Same but here c/c not directly involved in the turns itself
// (This one breaks if continue is not checked in handle_touch)
static std::string case_122_multi[2] =
{
    "MULTIPOLYGON(((10 8,10 10,12 10,12 8,10 8)),((10 4,10 7,12 7,12 4,10 4)),((7 5,7 8,9 8,9 5,7 5)))",
    "MULTIPOLYGON(((7 3,7 6,9 6,9 5,11 5,11 3,7 3)),((10 6,10 9,12 9,12 6,10 6)),((7 7,7 10,10 10,9 9,9 7,7 7)))"
};

static std::string case_123_multi[2] =
{
    // Intersection: one cluster with 3 zones, intersection -> no holes
    "MULTIPOLYGON(((1 0,1 1,1.5 0.5,2 0.5,2 0,1 0)),((0 1,1 2,2 2,2 1,0 1)))",
    "MULTIPOLYGON(((1 0,1 2,2 2,2 0,1 0)),((0 1,0 2,1 1,0 1)))"
};

static std::string case_124_multi[2] =
{
    // Intersection: one cluster with 3 zones, intersection -> one hole
    "MULTIPOLYGON(((1 0,1 1,0 1,1 2,2 2,2 0,1 0),(1.5 0.5,1.75 1,1 1,1.5 0.5)))",
    "MULTIPOLYGON(((1 0,1 2,2 2,2 0,1 0)),((0 1,0 2,1 1,0 1)))"
};

static std::string case_125_multi[2] =
{
    // Intersection: one cluster with 3 zones, intersection -> one hole (filled with other polygon)
    "MULTIPOLYGON(((1 0,1 1,0 1,1 2,2 2,2 0,1 0),(1.5 0.5,1.75 1,1 1,1.5 0.5)),((1 1,1.5 0.9,1.25 0.8,1 1)))",
    "MULTIPOLYGON(((1 0,1 2,2 2,2 0,1 0)),((0 1,0 2,1 1,0 1)))"
};

static std::string case_126_multi[2] =
{
    // Intersection: should result in multi-polygon of 5 (needs self-intersections)
    "MULTIPOLYGON(((5 5,5 10,10 10,10 5,5 5),(9 8,7 9,5 8,7 7,9 8)),((3 3,3 5,5 5,5 3,3 3)))",
    "MULTIPOLYGON(((0 3,6 3,6 9,0 9,0 3),(2 6,4 7,6 6,4 5,2 6)),((6 9,6 11,8 11,8 9,6 9)))"
};

static std::string case_127_multi[2] =
{
    // Intersection/validity with ii at (4 4), and both have self intersections at (5 5)
    "MULTIPOLYGON(((0 0,0 5,3 5,3 4,4 4,4 5,5 5,5 0,0 0)),((5 5,5 6,6 6,6 5,5 5)))",
    "MULTIPOLYGON(((0 0,0 5,5 5,5 4,4 4,4 3,5 3,5 0,0 0)),((5 5,5 7,7 7,7 5,5 5)))"
};

static std::string case_128_multi[2] =
{
    // Contains isolated areas of two types
    "MULTIPOLYGON(((0 0,0 10,10 10,10 0,0 0),(4 8,6 7,8 8,6 9,4 8),(1 2,2 0,3 2,2 4,1 2)))",
    "MULTIPOLYGON(((0 0,0 10,11 10,11 0,0 0),(2 9,0 8,2 7,4 8,2 9),(5 5,5 0,10 5,5 5)))"
};

static std::string case_129_multi[2] =
{
    // Extract from rec.box #6
    "MULTIPOLYGON(((0 0,0 5,5 5,5 4,4 4,4 2,5 1,5 0,0 0),(3 4,3 3,4 4,3 4),(2 1,3 2,3 3,2 2,2 1)))",
    "MULTIPOLYGON(((0 0,0 5,5 5,5 0,0 0),(4 4,4 3,5 4,4 4),(2 1,3 1,3 3,2 2,2 1)))"
};

static std::string case_130_multi[2] =
{
    // For validity
    "MULTIPOLYGON(((0 0,0 7,7 7,7 0,0 0),(1 4,2 3,3 4,2 5,1 4),(5 4,6 3,7 4,6 5,5 4),(3 6,4 5,5 6,4 7,3 6)))",
    "MULTIPOLYGON(((0 0,0 7,7 7,7 0,0 0),(4 3,5 4,4 5,3 4,4 3),(1 2,2 1,3 2,2 3,1 2)))"
};

static std::string case_131_multi[2] =
{
    // For validity, interior ring connections
    "MULTIPOLYGON(((1 2,1 3,2 3,2 2,1 2)),((2 1,2 2,3 2,3 1,2 1)))",
    "MULTIPOLYGON(((0 0,0 4,4 4,4 0,0 0),(3 3,1 3,1 1,3 1,3 3)))"
};

static std::string case_132_multi[2] =
{
    // For validity, interior ring connections including cluster
    "MULTIPOLYGON(((2 4,2 6,4 6,4 4,2 4)),((4 2,4 4,6 4,6 2,4 2)))",
    "MULTIPOLYGON(((0 0,0 8,8 8,8 0,0 0),(6 6,2 6,2 2,6 2,6 6)),((2 2,2.5 3.5,4 4,3.5 2.5,2 2)),((4 4,4.5 5.5,6 6,5.5 4.5,4 4)))"
};

static std::string case_133_multi[2] =
{
    // Zoomed in version of case_recursive_boxes_49 with in both interiors an extra polygon (same for both)
    "MULTIPOLYGON(((0 0,0 4,2 4,2 6,4 6,4 8,10 8,10 4,8 4,8 0,0 0),(4 4,4 6,6 6,6 4,4 4)),((5 4.5,4 6,5.5 5, 5 4.5)))",
    "MULTIPOLYGON(((2 0,2 8,8 8,8 6,10 6,10 2,6 2,6 0,2 0),(6 6,6 4,4 4,4 6,6 6)),((5 4.5,4 6,5.5 5, 5 4.5)))"
};

static std::string case_134_multi[2] =
{
    // Zoomed in version of case_recursive_boxes_49 with two interior rings
    "MULTIPOLYGON(((0 0,0 4,2 4,2 6,4 6,4 8,10 8,10 4,8 4,8 0,0 0),(3 4,4 6,4 4,3 4),(6 6,4 6,6 7,6 6)))",
    "MULTIPOLYGON(((2 0,2 8,8 8,8 6,10 6,10 2,6 2,6 0,2 0),(3 4,4 6,4 4,3 4),(6 6,4 6,6 7,6 6)))"
};

static std::string case_135_multi[2] =
{
    // Contains two equal interior rings, both touching to their exterior rings
    // Needs detection of isolated interior ring pattern
    "MULTIPOLYGON(((5 8,4 8,4 7,3 7,3 6,2 6,2 7,1 7,1 10,3 10,3 9,5 9,5 8),(3 8,3 9,2 9,2 8,3 8)))",
    "MULTIPOLYGON(((5 4,4 4,3 4,3 7,1 7,1 8,0 8,0 9,1 9,1 10,3 10,3 9,4 9,4 8,6 8,6 9,7 9,7 5,6 5,6 4,5 4),(5 5,5 6,6 6,6 7,4 7,4 5,5 5),(3 8,3 9,2 9,2 8,3 8)))"
};

static std::string case_136_multi[2] =
{
    // Variant of 135, but approaching cluster is non equal
    "MULTIPOLYGON(((5 8,4 8,4 7,3 7,3 6,2 6,2 7,1 7,1 10,2 10,3 9,5 9,5 8),(3 8,3 9,2 9,2 8,3 8)))",
    "MULTIPOLYGON(((5 4,4 4,3 4,3 7,1 7,1 8,0 8,0 9,1 9,1 10,3 10,3 9,4 9,4 8,6 8,6 9,7 9,7 5,6 5,6 4,5 4),(5 5,5 6,6 6,6 7,4 7,4 5,5 5),(3 8,3 9,2 9,2 8,3 8)))"
};

static std::string case_137_multi[2] =
{
    // Variant of 135, but leaving cluster is non equal
    "MULTIPOLYGON(((5 8,4 8,4 7,3 7,3 6,2 6,2 7,1 7,1 10,3 10,3 9,5 9,5 8),(3 8,3 9,2 9,2 8,3 8)))",
    "MULTIPOLYGON(((5 4,4 4,3 4,3 7,1 7,1 8,0 8,0 9,1 9,1 10,3 10,3 9,4 8,6 8,6 9,7 9,7 5,6 5,6 4,5 4),(5 5,5 6,6 6,6 7,4 7,4 5,5 5),(3 8,3 9,2 9,2 8,3 8)))"
};

static std::string case_138_multi[2] =
{
    // Zoomed in version of case_recursive_boxes_49 with in both interiors an extra polygon (different for both)
    "MULTIPOLYGON(((0 0,0 4,2 4,2 6,4 6,4 8,10 8,10 4,8 4,8 0,0 0),(4 4,4 6,6 6,6 4,4 4)),((4.5 4.5,4 6,5.5 5.5,4.5 4.5)))",
    "MULTIPOLYGON(((2 0,2 8,8 8,8 6,10 6,10 2,6 2,6 0,2 0),(6 6,6 4,4 4,4 6,6 6)),((5 4.5,4 6,5.5 5, 5 4.5)))"
};

static std::string case_139_multi[2] =
{
    // Another zoomed in version of case_recursive_boxes_49 with different interior polygons
    "MULTIPOLYGON(((0 0,0 4,2 4,2 6,4 6,4 8,10 8,10 4,8 4,8 0,0 0),(4 4,4 6,6 6,6 4,4 4)),((4.5 4.5,4 6,5.5 5,4.5 4.5)))",
    "MULTIPOLYGON(((2 0,2 8,8 8,8 6,10 6,10 2,6 2,6 0,2 0),(6 6,6 4,4 4,4 6,6 6)),((5 4.5,4 6,5.5 5, 5 4.5)))"
};

static std::string case_140_multi[2] =
{
    // Another zoomed in version of case_recursive_boxes_49 with different interior polygons
    "MULTIPOLYGON(((0 0,0 4,2 4,2 6,4 6,4 8,10 8,10 4,8 4,8 0,0 0),(4 4,4 6,6 6,6 4,4 4)),((5 4.5,4 6,5.5 5.5,5 4.5)))",
    "MULTIPOLYGON(((2 0,2 8,8 8,8 6,10 6,10 2,6 2,6 0,2 0),(6 6,6 4,4 4,4 6,6 6)),((5 4.5,4 6,5.5 5, 5 4.5)))"
};

static std::string case_141_multi[2] =
{
    // Version to test more isolation/validity cases
    "MULTIPOLYGON(((0 0,0 10,10 10,10 0,0 0),(2 3,1 2,2 1,3 2,2 3),(2 7,3 8,2 9,1 8,2 7),(10 3,9 4,8 3,9 2,10 3),(7 10,6 9,7 8,8 9,7 10),(10 7,9 8,8 7,9 6,10 7)))",
    "MULTIPOLYGON(((0 0,0 10,10 10,10 0,0 0),(0 5,2 3,4 5,2 7),(3 2,4 1,5 2,4 3,3 2),(3 8,4 7,5 8,4 9,3 8),(7 2,8 1,9 2,8 2,8 3,7 2),(8 7,7 8,6 7,7 6,8 7)))"
};

static std::string case_recursive_boxes_1[2] =
{
    "MULTIPOLYGON(((1 0,0 0,0 1,1 1,1 2,0 2,0 4,2 4,2 5,3 5,3 6,1 6,1 5,0 5,0 10,9 10,9 9,7 9,7 8,6 8,6 7,8 7,8 6,9 6,9 4,8 4,8 3,10 3,10 0,6 0,6 1,5 1,5 0,1 0),(8 4,8 5,7 5,7 6,6 6,6 5,5 5,5 4,4 4,4 3,5 3,5 2,7 2,7 3,6 3,6 4,8 4),(8 1,9 1,9 2,8 2,8 1),(4 7,4 9,3 9,3 7,4 7)),((9 9,10 9,10 7,10 6,9 6,9 7,8 7,8 8,9 8,9 9)))",
    "MULTIPOLYGON(((5 6,5 7,3 7,3 9,2 9,2 8,1 8,1 10,4 10,4 9,6 9,6 10,10 10,10 9,9 9,9 8,10 8,10 6,9 6,9 5,10 5,10 3,7 3,7 4,6 4,6 6,5 6),(5 7,6 7,6 8,5 8,5 7),(7 7,7 6,8 6,8 7,7 7)),((1 0,0 0,0 7,2 7,2 6,5 6,5 5,4 5,4 4,5 4,5 3,7 3,7 2,6 2,6 0,1 0),(2 1,2 2,3 2,3 3,1 3,1 1,2 1)),((7 2,10 2,10 0,9 0,9 1,8 1,8 0,7 0,7 2)))"
};

static std::string case_recursive_boxes_2[2] =
{
    // Traversal problem; Many ii-cases -> formerly caused "Endless loop"
    // So it appears that there might be more decisions than intersection points
    "MULTIPOLYGON(((1 0,0 0,0 4,1 4,1 5,0 5,0 10,3 10,3 9,4 9,4 10,6 10,6 9,7 9,7 10,10 10,10 0,1 0),(6 9,5 9,5 8,6 8,6 9),(7 6,8 6,8 7,7 7,7 6),(8 7,9 7,9 8,8 8,8 7),(9 1,9 2,8 2,8 1,9 1)))",
    "MULTIPOLYGON(((0 0,0 10,10 10,10 0,8 0,8 1,7 1,7 0,0 0),(7 3,6 3,6 2,7 2,7 3),(6 7,7 7,7 8,6 8,6 7)))"
};


static std::string case_recursive_boxes_3[2] =
{
    // Previously a iu/ux problem causing union to fail.
    // For CCW polygon it also reports a iu/iu problem.
    "MULTIPOLYGON(((8 3,9 4,9 3,8 3)),((5 9,5 10,6 10,5 9)),((2 0,2 1,3 0,2 0)),((2 5,2 6,3 6,3 5,2 5)),((2 2,1 2,1 3,2 3,3 2,3 1,2 1,2 2)),((6 8,7 9,7 7,8 7,7 6,6 6,6 8)),((4 6,5 7,5 6,4 6)),((4 8,4 9,5 9,5 8,4 8)),((0 3,1 4,1 3,0 3)),((8 7,9 8,9 7,8 7)),((9 6,9 7,10 7,9 6)),((7 0,8 1,8 0,7 0)),((0 4,0 5,1 5,1 4,0 4)),((4 2,5 3,5 2,4 1,4 2)),((4 10,4 9,2 9,3 10,4 10)),((5 2,6 3,7 3,7 2,6 2,6 1,5 0,5 2)),((5 3,4 3,4 4,2 4,4 6,4 5,4.5 4.5,6 6,6 5,7 4,5 4,5 3)),((10 2,9 1,9 3,10 2)),((8 4,7 4,8 5,7 5,7 6,9 6,9 5,10 5,10 4,8 4)),((1 7,0 7,0 8,1 8,1 7)),((1 10,2 10,1 9,0 9,0 10,1 10)),((6.5 9.5,7 10,7 9,6 9,6 10,6.5 9.5)),((8 8,8 9,10 9,9 8,8 8)))",
    "MULTIPOLYGON(((7 3,7 4,8 5,8.5 4.5,9 5,9 4,10 3,8 3,7 3),(9 4,8 4,8.5 3.5,9 4)),((0 2,1 1,0 1,0 2)),((1 1,2 2,2 1,1 1)),((2 2,1 2,1 3,2 3,2 2)),((2 1,3 1,2 0,1 0,2 1)),((3 1,4 2,5 2,6 1,5 1,5 0,4 0,4 1,3 1)),((5 2,5 3,4 3,4 4,6 4,6 3,7 3,6 2,5 2)),((6 1,7 1,7 0,6 0,6 1)),((6 4,6 5,7 6,8 6,8 5,7 5,6 4)),((8 3,9 2,8 2,8 3)),((9 2,10 2,10 1,9 1,9 2)),((9 1,9 0,8 0,9 1)),((1 3,0 3,0 4,1 4,1 3)),((2 3,3 4,3 3,2 3)),((9 7,9 8,10 8,10 7,9 7)),((0 7,0 8,1 8,1 7,0 7)),((1 8,1 9,2 8,1 8)),((1 7,3 7,3 8,4 7,5 7,6 8,6 10,7 9,8 10,9 10,9 9,8 9,8 8,7 8,6 7,6 6,3 6,1 6,1 7)),((1 6,1 5,0 5,1 6)),((3 6,4 5,2 5,3 6)),((1 9,1 10,2 10,2 9,1 9)))"
};


static std::string case_recursive_boxes_4[2] =
{
    // Occurred after refactoring assemble
    "MULTIPOLYGON(((0 1,0 3,1 4,0 4,0 5,1 6,0 6,0 8,1 9,0 9,0 10,6 10,7 10,7.5 9.5,8 10,8 9,9 9,9.5 8.5,10 9,10 8,9.5 7.5,10 7,10 5,9 5,9 3,10 2,10 1,9 1,10 0,4 0,4 1,3 1,3 0,1 0,1 1,0 0,0 1),(1 9,1 8,2 9,1 9),(2 9,2 8,3 9,2 9),(2 8,2 7,3 8,2 8),(2 7,1.5 6.5,2 6,2 7),(2 6,2.5 5.5,3 6,2 6),(3 6,3 5,4 6,3 6),(6 10,5.5 9.5,6 9,6 10),(8 9,7 9,7 8,8 8,8 9),(7 8,6 8,6.5 7.5,7 8),(9 5,8 5,8 4,9 5),(8 4,7 3,7 2,8 3,8 4),(9 1,8 1,8.5 0.5,9 1),(6 4,6.5 3.5,7 4,6 4),(4 2,4.5 1.5,5 2,5 3,4 3,4 2),(5 3,5 4,4.5 3.5,5 3),(5 7,5 9,4 9,4 8,5 7),(3 3,4 4,3 4,3 3),(3 4,2 4,2.5 3.5,3 4)),((9 3,10 4,10 3,9 3)),((10 9,9 9,10 10,10 9)))",
    "MULTIPOLYGON(((1 0,0 0,0 3,1 3,1 4,0 4,0 8,1 7,1 9,0 9,0 10,7 10,6 9,6.5 8.5,7 9,8 9,9 8,8 8,9 7,9 6,10 7,10 5,10 0,7 0,8 1,7 1,6 0,3 0,3 1,2 1,1 1,1 0),(1 3,2 2,2 3,1 3),(1 4,2 4,2 5,1 4),(1 7,1 6,2 7,1 7),(10 5,9 5,9 4,10 5),(5 1,5.5 0.5,6 1,5 1),(6 1,6 2,5 2,6 1),(6 2,6.5 1.5,7 2,8 2,8 4,7 3,6 3,6 2),(6 5,7 6,7 7,6 7,6 5),(6 5,6.5 4.5,7 5,6 5),(4 4,5 4,5 5,4 4),(3 7,2 6,3 6,3 7),(3 7,4 6,4 7,3 7),(3.5 7.5,4 8,4 9,3 8,3.5 7.5)),((1 0,2 1,2 0,1 0)),((7 10,8 10,7 9,7 10)),((8 9,9 10,10 10,10 8,9 8,9 9,8 9)))"
};

static std::string case_recursive_boxes_5[2] =
{
    // Occurs after refactoring uu / handle_touch (not yet integrated)
    "MULTIPOLYGON(((0 9,0 10,1 10,1 9,0 9)),((9 0,9 1,10 1,10 0,9 0)),((5 6,5 7,6 7,6 6,7 6,7 4,6 4,6 5,5 5,5 6)),((5 3,7 3,7 2,4 2,4 3,5 3)),((5 8,5 9,7 9,7 8,5 8)),((4 0,1 0,1 1,5 1,5 0,4 0)),((3 5,3 4,4 4,4 3,2 3,2 2,1 2,1 3,0 3,0 4,2 4,2 5,1 5,1 6,4 6,4 5,3 5)),((0 2,1 2,1 1,0 1,0 2)),((4 10,4 7,1 7,1 6,0 6,0 8,1 8,1 9,2 9,2 10,4 10)),((9 4,9 3,8 3,8 5,9 5,9 4)),((7 2,8 2,8 0,7 0,7 2)),((8 7,10 7,10 6,7 6,7 8,8 8,8 7)))",
    "MULTIPOLYGON(((2 6,2 8,3 8,3 7,5 7,5 6,6 6,6 5,8 5,8 6,9 6,9 4,8 4,8 3,7 3,7 4,6 4,6 3,5 3,5 4,3 4,3 6,2 6),(5 6,4 6,4 5,5 5,5 6)),((1 1,2 1,2 0,1 0,1 1)),((2 1,2 2,3 2,3 1,2 1)),((2 2,1 2,1 3,2 3,2 2)),((1 3,0 3,0 5,1 5,1 4,1 3)),((1 5,1 6,2 6,2 5,1 5)),((2 8,0 8,0 10,3 10,4 10,4 9,2 9,2 8)),((8 6,7 6,7 7,8 7,8 6)),((8 7,8 10,9 10,9 9,10 9,10 8,9 8,9 7,8 7)),((9 7,10 7,10 6,9 6,9 7)),((9 4,10 4,10 3,10 1,9 1,9 2,8 2,8 3,9 3,9 4)),((8 2,8 1,7 1,7 2,8 2)),((8 1,9 1,9 0,8 0,8 1)),((7 1,7 0,5 0,3 0,3 1,4 1,4 2,6 2,6 1,7 1)),((6 2,6 3,7 3,7 2,6 2)),((3 4,3 3,2 3,2 4,3 4)),((6 8,6 9,7 9,7 8,6 8)))"
};

static std::string case_recursive_boxes_6[2] =
{
    // [1] is reported as invalid by BG, but not by postgis
    // Fixed by replacing handle_tangencies
    "MULTIPOLYGON(((0 3,1 3,1 4,2 4,2 5,5 5,5 0,2 0,0 0,0 3),(2 0,2 1,1 1,2 0),(2 1,2 2,1.5 1.5,2 1),(2 2,3 1,3 2,2 2),(3 2,3.5 1.5,4 2,3 2)),((0 3,0 5,1 5,2 5,1 4,0 3)))",
    "MULTIPOLYGON(((1 2,2 3,1 3,1 4,0 4,0 5,5 5,5 2,4 2,3 2,3 1,2 1,2 2,1 2),(4 2,4 3,3 3,4 2)),((1 2,2 1,3 0,2 0,0 0,0 3,1 3,1 2)),((3 0,3 1,4 2,4 1,5 1,5 0,4 0,3 0)))"
};

static std::string case_recursive_boxes_7[2] =
{
    "MULTIPOLYGON(((3 1,3 2,4 2,4 1,3 1)),((2.5 2.5,3 3,3 2,2 2,2 3,2.5 2.5)),((2 1,3 0,1 0,1 2,2 2,2 1)))",
    "MULTIPOLYGON(((0 0,1 1,1 0,0 0)),((0 1,0 2,1 2,0 1)),((3.5 2.5,4 3,4 2,3 2,3 3,3.5 2.5)),((3 2,4 1,1 1,1 2,3 2)))"
};

static std::string case_recursive_boxes_8[2] =
{
    // Having colocated IP halfway segment
    "MULTIPOLYGON(((3 4,3 3,2 3,2 2,0 2,0 3,1 3,1 4,1.5 3.5,2 4,3 4)),((2 5,2 4,1 4,0 3,0 5,2 5)))",
    "MULTIPOLYGON(((0 2,0 4,3 4,4 4,4 3,3 3,3 1,0 1,0 2),(0 2,1 2,1 3,0 2)))"
};

static std::string case_recursive_boxes_9[2] =
{
    // Needs ii turn skipping
    "MULTIPOLYGON(((2 2,3 2,3 0,2 0,2 1,1 1,2 2)),((1 1,1 0,0 0,0 3,0.5 2.5,1 3,2 2,1 2,1 1)))",
    "MULTIPOLYGON(((2 1,2 2,0 2,0 3,2 3,3 2,3 1,2 1)),((2.5 0.5,3 1,3 0,0 0,0 1,1 1,1 2,2.5 0.5)))"
};

static std::string case_recursive_boxes_10[4] =
{
    // Requires skipping ux for difference (a) and switching a->b
    "MULTIPOLYGON(((2 2,2 3,3 2,2 2)),((2 2,3 1,1 1,1 2,2 2)))",
    "MULTIPOLYGON(((3 2,2 1,2 3,3 3,3 2)))",

    // Inverse versions
    "MULTIPOLYGON(((0 0,0 4,4 4,4 0,0 0),(2 2,3 2,2 3,2 2),(2 2,1 2,1 1,3 1,2 2)))",
    "MULTIPOLYGON(((1 0,1 4,4 4,4 0,1 0),(3 2,3 3,2 3,2 1,3 2)))"
};

static std::string case_recursive_boxes_11[4] =
{
    // Requires switching a->b
    "MULTIPOLYGON(((3 2,5 2,5 1,4 1,4 0,3 0,3 1,2 1,3 2)))",
    "MULTIPOLYGON(((5 2,4 1,4 3,5 2)),((3 1,3 2,4 2,3 1)),((4 1,5 1,5 0,4 0,4 1)),((3 2,2 1,3 1,2 0,1 1,2 2,2 3,3 3,3 2)))",

    // Inverse versions
    "MULTIPOLYGON(((0 -1,0 4,6 4,6 -1,0 -1),(3 2,2 1,3 1,3 0,4 0,4 1,5 1,5 2,3 2)))",
    "MULTIPOLYGON(((0 -1,0 4,6 4,6 -1,0 -1),(5 2,4 3,4 1,5 2),(3 1,4 2,3 2,3 1),(4 1,4 0,5 0,5 1,4 1),(3 2,3 3,2 3,2 2,1 1,2 0,3 1,2 1,3 2)))"
};

static std::string case_recursive_boxes_12[2] =
{
    "MULTIPOLYGON(((0 3,1 3,0.5 2.5,1 2,0 2,0 3)),((1 2,2 2,2 1,1 1,1 2)),((2 1,3 2,3 1,2 1)),((2 2,2 3,3 3,2 2)),((0 0,0 1,1 0,0 0)))",
    "MULTIPOLYGON(((0 1,0 2,1 2,0 1)),((0 1,1 1,1.5 0.5,2 1,2 0,0 0,0 1)),((1 3,1 4,2 3,1 2,1 3)))"
};

static std::string case_recursive_boxes_13[2] =
{
    "MULTIPOLYGON(((1 3,1 5,2 5,2 4,1.5 3.5,2 3,1 3)),((1 3,2 2,0 2,1 3)),((2 2,3 2,3 1,2 1,2 2)),((3 2,3 3,4 3,4 2,3 2)))",
    "MULTIPOLYGON(((1 4,1 3,0 3,0 4,1 5,1 4)),((3 5,4 5,4 4,2 4,2 5,3 5)),((3 1,3 2,5 2,5 1,3 1)))"
};

static std::string case_recursive_boxes_14[2] =
{
    "MULTIPOLYGON(((2 2,2 3,3 2,2 2)),((2 3,3 4,3 3,2 3)),((2 3,1 3,1 4,2 4,2 3)))",
    "MULTIPOLYGON(((3 3,4 4,4 3,3 3)),((1 2,2 3,2 2,1 2)),((2 1,2 2,3 1,2 1)),((1 4,1 5,2 5,2 4,1 4)))"
};

static std::string case_recursive_boxes_12_invalid[2] =
{
    // One of them is invalid requiring discarding turns colocated with uu in these clusters
    "MULTIPOLYGON(((2 2,2 3,3 3,2 2)),((0 0,0 1,1 0,0 0)),((0 3,1 3,0.5 2.5,1 2,0 2,0 3)),((3 2,3 1,1 1,1 2,2 2,2 1,3 2)))",
    "MULTIPOLYGON(((0 1,0 2,1 2,0 1)),((0 1,1 1,1.5 0.5,2 1,2 0,0 0,0 1)),((1 3,1 4,2 3,1 2,1 3)))"
};

static std::string case_recursive_boxes_13_invalid[2] =
{
    // Strictly invalid, requires checking seg_id while considering skipping to next turn
    "MULTIPOLYGON(((2 1,2 2,3 2,3 1,2 1)),((3 2,3 3,4 3,4 2,3 2)),((2 4,1.5 3.5,2 3,1 3,2 2,0 2,1 3,1 5,2 5,2 4)))",
    "MULTIPOLYGON(((1 4,1 3,0 3,0 4,1 5,1 4)),((3 5,4 5,4 4,2 4,2 5,3 5)),((3 1,3 2,5 2,5 1,3 1)))"
};

static std::string case_recursive_boxes_14_invalid[2] =
{
    // Strictly invalid, requires skipping assignment of discarded turns for clusters
    "MULTIPOLYGON(((2 2,2 3,3 2,2 2)),((2 4,2 3,3 4,3 3,1 3,1 4,2 4)))",
    "MULTIPOLYGON(((3 3,4 4,4 3,3 3)),((1 2,2 3,2 2,1 2)),((2 1,2 2,3 1,2 1)),((1 4,1 5,2 5,2 4,1 4)))"
};


static std::string case_recursive_boxes_15[2] =
{
    // Requires inspecting blocked operations in traversing cluster
    "MULTIPOLYGON(((3 2,3 3,4 3,3 2)),((4 1,4 2,5 2,5 1,4 1)),((4 2,4 3,5 3,4 2)),((3 5,4 4,2 4,2 5,3 5)))",
    "MULTIPOLYGON(((3 4,4 3,3 2,3 3,2 3,2 4,3 4)),((4 3,4 4,5 4,5 3,4 3)))"
};

static std::string case_recursive_boxes_16[2] =
{
    // Requires inspecting if traverse is possible in selecting continue operation
    "MULTIPOLYGON(((2 4,1 3,0 3,0 5,3 5,3 4,2 4)),((3 4,4 4,4 5,5 5,5 4,4 3,2 3,3 4)),((2.5 1.5,3 1,2.5 0.5,3 0,0 0,0 2,3 2,2.5 1.5)),((3 1,3 2,4 2,4 1,5 2,5 0,3 0,3 1)))",
    "MULTIPOLYGON(((0 1,1 1,1 0,0 0,0 1)),((0 1,0 5,1 5,1 3,2 3,2 2,1 1,1 2,0 1)),((2 2,3 1,2 1,2 2)),((3 1,3 2,4 1,5 1,5 0,3 0,3 1)),((4 1,4 2,3 2,3 4,2 4,2 5,3 5,5 5,5 4,4 4,4 3,5 3,5 2,4 1)))"
};

static std::string case_recursive_boxes_17[2] =
{
    // Requires including uu turns, at least in clusters
    "MULTIPOLYGON(((0 4,0 5,1 5,0 4)),((1 5,1.5 4.5,2 5,2 4,1 3,1 5)),((2 4,3 4,3 3,2 2,1 2,1 3,2 3,2 4)),((1 2,1 1,2 1,3 0,0 0,0 2,1 2)),((2 1,2 2,2.5 1.5,4 3,5 3,5 1,4 1,4 0,3 0,3 1,2 1)),((4 0,5 1,5 0,4 0)),((3 4,3 5,4 5,5 5,5 4,3 4)))",
    "MULTIPOLYGON(((2 5,3 5,2 4,2 5)),((3 1,4 2,4 0,3 0,3 1)),((2 0,0 0,0 1,1 2,0 2,1 3,2 2,2 3,3 2,3 1,2 0)),((1 4,0.5 3.5,1 3,0 3,0 4,1 4)),((4 3,3 3,3 5,4 5,4 4,5 4,5 2,4 2,4 3)))"
};

static std::string case_recursive_boxes_18[2] =
{
    // Simple case having two colocated uu turns
    "MULTIPOLYGON(((2 1,3 0,2 0,2 1)),((2 1,1 1,1 2,2 1)))",
    "MULTIPOLYGON(((2 2,2 3,3 3,3 2,2 1,2 2)))"
};

static std::string case_recursive_boxes_19[2] =
{
    // Simple case having two colocated uu and ux turns
    "MULTIPOLYGON(((1 4,2 3,1 3,1 4)),((1 4,0 4,0 5,1 4)))",
    "MULTIPOLYGON(((3 4,1 4,2 5,3 4)),((1 4,1 3,0 3,1 4)))"
};

static std::string case_recursive_boxes_20[2] =
{
    // Simple case having two colocated uu and (discarded) cc turns
    "MULTIPOLYGON(((4 4,4 5,5 5,5 4,4 4)),((4 4,4 3,3 3,3 4,4 4)))",
    "MULTIPOLYGON(((4 4,4 3,3 3,4 4)),((4 4,4 5,5 4,4 4)))"
};

static std::string case_recursive_boxes_21[2] =
{
    // Having colocated uu/ux/cc turns requiring traversing through arcs to
    // find first open outgoing arc for union
    "MULTIPOLYGON(((3 1,3 2,4 1,3 1)),((3 1,3 0,2 0,2 1,3 1)))",
    "MULTIPOLYGON(((3 1,3 0,2 0,3 1)),((3 1,2 1,2 2,3 2,3 1)))"
};

static std::string case_recursive_boxes_22[2] =
{
    // Requires including ux turns for intersections to block paths
    "MULTIPOLYGON(((2 2,3 2,2.5 1.5,3 1,2 1,2 2)),((2 2,2 3,3 3,2 2)))",
    "MULTIPOLYGON(((1 2,0 2,0 3,1 2)),((1 2,2 3,2 1,1 1,1 2)))"
};

static std::string case_recursive_boxes_23[2] =
{
    // Requires discarding turns with uu for intersection/difference too
    "MULTIPOLYGON(((4 3,4 4,4.5 3.5,5 4,5 3,4 3)),((4 3,5 2,4 2,4 3)))",
    "MULTIPOLYGON(((4 3,5 4,5 3,4 3)),((3 3,3 4,4 3,3 3)))"
};

static std::string case_recursive_boxes_24[2] =
{
    // Requires including all combinations in clusters having uu
    "MULTIPOLYGON(((0 2,0 3,1 2,0 2)),((2 3,1 3,1 4,2 4,2 3)),((2 3,4 3,3 2,2 2,2 3)))",
    "MULTIPOLYGON(((3 2,4 1,2 1,3 2)),((3 2,2 2,2 3,3 2)),((2 2,2 1,1 1,1 2,2 2)))"
};

static std::string case_recursive_boxes_25[2] =
{
    // Requires startable flag per operation, assigned per cluster
    "MULTIPOLYGON(((4 1,4 2,5 2,5 1,4 0,4 1)),((3 2,3 3,4 3,4 2,3 2)),((3 2,3 1,2 1,3 2)))",
    "MULTIPOLYGON(((4 2,4 1,3 1,4 2)),((4 2,4 3,5 2,4 2)),((3 1,1 1,1 2,3 2,3 1)))"
};

static std::string case_recursive_boxes_26[2] =
{
    // Requires not including uu outside clusters (or travel through them)
    "MULTIPOLYGON(((2 4,3 4,3 3,0 3,1 4,1 5,2 5,2 4)),((1 3,1 2,0 2,1 3)))",
    "MULTIPOLYGON(((2 3,0 3,0 4,1 4,1 5,3 5,3 4,2 4,2 3)),((2 3,3 2,2 2,2 3)))"
};

static std::string case_recursive_boxes_27[2] =
{
    // Combination of lonely uu-turn (which is discarded) and a cluster containing it
    "MULTIPOLYGON(((2 2,3 1,3 0,2 0,2 2)),((2 2,1 2,1 3,2 2)))",
    "MULTIPOLYGON(((1 2,0 1,0 2,1 2)),((2 1,2 0,1 0,1 1,2 2,2 1)),((1 3,2 2,1 2,1 3)),((1 3,0 3,1 4,1 3)))"
};

static std::string case_recursive_boxes_28[2] =
{
    // Requires startable flag per operation, assigned per cluster (as #25 but in a different configuration)
    "MULTIPOLYGON(((5 1,5 0,4 0,4 2,5 3,5 1)),((4 2,3 2,3 3,4 3,4 2)))",
    "MULTIPOLYGON(((2 2,2 3,3 3,4 2,3 1,2 1,2 2)),((3 4,4 3,3 3,3 4)),((4 2,5 2,4 1,4 2)))"
};

static std::string case_recursive_boxes_29[2] =
{
    // Requires not discarding colocated cc turns
    "MULTIPOLYGON(((2 3,2 4,1 4,2 5,4 5,4 4,3 4,3 3,2 3)),((3 3,4 3,4 1,5 1,5 0,3 0,3 2,2 2,3 3)),((1 2,0 2,0 3,1 3,1 2)),((1 1,0 1,1 2,2 2,2 1,1 1)),((2 1,2 0,1 0,2 1)))",
    "MULTIPOLYGON(((0 4,0 5,1 4,0 4)),((2 3,2 4,4 4,4 3,2 3)),((2 2,1 2,1 3,2 3,2 2)),((1 2,0 2,0 3,1 3,0.5 2.5,1 2)),((1 0,0 0,1 1,4 1,4 0,1 0)),((4 0,5 1,5 0,4 0)))"
};

static std::string case_recursive_boxes_30[2] =
{
    // Requires not discarding turns colocated with uu/invalid polygons (now not necessary anymore because of startable)
    "MULTIPOLYGON(((2 2,2 3,4 3,4 4,4.5 3.5,5 4,5 0,3 0,3 1,4 1,4 2,2 2)),((1 5,3 5,4 4,0 4,0 5,1 5)))",
    "MULTIPOLYGON(((2 1,2 3,1 3,1 4,2 5,2 4,3 4,3 5,5 5,5 4,4 4,5 3,4.5 2.5,5 2,5 0,4 0,4 1,3 1,3 0,1 0,2 1),(4 4,3.5 3.5,4 3,4 4),(4 1,4 2,3 2,4 1),(3 2,3 3,2.5 2.5,3 2)))"
};

static std::string case_recursive_boxes_31[2] =
{
    // Requires allowing traverse through clusters having only uu/cc for intersection
    "MULTIPOLYGON(((1 4,1 1,0 1,0 4,1 4)),((1 1,2 1,2 0,1 0,1 1)),((2 2,1 2,2 3,2 2)))",
    "MULTIPOLYGON(((2 3,2 2,1 2,2 3)),((0 1,0 3,1 3,1 1,0 1)),((1 1,1 0,0 0,1 1)))"
};

static std::string case_recursive_boxes_32[2] =
{
    // Similar to #31 but here uu/ux/cc
    "MULTIPOLYGON(((1 3,2 3,2 2,1 1,1 3)),((2 2,3 1,3 0,2 0,2 2)),((1 1,2 1,1 0,0 0,0 1,1 1)))",
    "MULTIPOLYGON(((3 1,3 0,2 0,2 2,3 1)),((1 1,0 1,0 2,1 2,2 1,1 1)))"
};

static std::string case_recursive_boxes_33[2] =
{
    // Similar to #31 but here also a ui
    "MULTIPOLYGON(((4 3,5 3,5 2,3 2,4 3)),((4 2,5 1,3 1,4 2)),((2 1,3 1,3 0,2 0,2 1)),((3 2,1 2,1 3,2 3,3 2)))",
    "MULTIPOLYGON(((3 1,2 1,2 4,4 4,4 3,5 2,3 2,3 1)),((3 1,4 2,4 1,5 1,5 0,3 0,3 1)))"
};

static std::string case_recursive_boxes_34[2] =
{
    // Requires detecting finished arcs during cluster traversal
    "MULTIPOLYGON(((2 0,0 0,0 5,2 5,2 4,3 5,5 5,5 0,2 0)))",
    "MULTIPOLYGON(((1 0,0 0,0 5,1 4,1 5,4 5,5 4,5 1,4 1,4 3,3 3,2 3,2 2,3 2,3 1,2 0,1 0),(1 0,2 1,1 1,1 0),(4 5,3 4,3.5 3.5,4 4,4 5)),((4 1,3 0,3 1,4 1)))"
};

static std::string case_recursive_boxes_35[2] =
{
    // Requires detecting finished arcs during cluster traversal
    "MULTIPOLYGON(((0 2,0 5,4 5,4 4,5 4,5 1,4 1,5 0,3 0,0 0,0 2),(0 2,1 2,1 3,0 2),(3 0,3 1,2 1,3 0),(2.5 1.5,3 2,2 2,2.5 1.5),(2 4,1 4,2 3,2 4)))",
    "MULTIPOLYGON(((1 0,0 0,0 5,3 5,5 5,5 0,1 0),(1 0,2 1,1 1,1 0),(2 1,2 2,1.5 1.5,2 1),(3 5,2.5 4.5,3 4,3 5),(3 2,3 3,2.5 2.5,3 2),(2 3,2 4,1 4,1 3,2 3)))"
};

static std::string case_recursive_boxes_36[2] =
{
    // Requires increasing target rank while skipping finished arcs to avoid duplicate output
    "MULTIPOLYGON(((5 3,4 3,4 4,5 3)),((5 2,4 2,5 3,5 2)),((5 2,5 1,4 1,5 2)))",
    "MULTIPOLYGON(((4 2,4 3,5 3,5 2,4 2)),((4 2,4 1,3 1,3 2,4 2)))"
};

static std::string case_recursive_boxes_37[2] =
{
    // [1] is reported as invalid by BG, but not by postgis
    // Requires skipping arc for union too, to avoid duplicate hole
    "MULTIPOLYGON(((4 0,5 1,5 0,4 0)),((2 0,3 1,3 0,2 0)),((2 3,2 2,1 2,1 3,2 3)),((2 1,2 2,4 2,3 1,2 1)))",
    "MULTIPOLYGON(((3 1,4 2,4 3,5 3,5 0,3 0,3 1),(3 1,3.5 0.5,4 1,3 1)),((3 1,2 0,2 1,3 1)),((2 3,1 2,1 3,2 3)))"
};

static std::string case_recursive_boxes_38[2] =
{
    // Smaller version of 29, removing generated lower interior in a union
    "MULTIPOLYGON(((2 3,2 4,1 4,2 5,4 5,4 4,3 4,3 3,2 3)),((3 3,4 3,4 1,5 1,5 0,3 0,3 2,2 2,3 3)),((1 2,0 2,0 3,1 3,1 2)),((1 1,0 1,1 2,2 2,2 1,1 1)))",
    "MULTIPOLYGON(((2 2,4 2,4 1,2 1,2 2)),((0 4,0 5,1 4,0 4)),((2 3,2 4,4 4,4 3,2 3)),((2 2,1 2,1 3,2 3,2 2)),((1 2,0 2,0 3,1 3,0.5 2.5,1 2)))"
};

static std::string case_recursive_boxes_39[2] =
{
    // Needs check for back at start during traversal
    "MULTIPOLYGON(((3 8,2 8,2 9,3 9,3 8)),((4 8,4 9,5 9,5 8,4 8)),((6 9,6 10,7 10,7 9,6 9)),((5 6,4 6,4 7,6 7,6 5,5 5,5 6)),((4 7,3 7,3 8,4 8,4 7)),((7 8,8 8,8 7,6 7,6 8,7 8)))",
    "MULTIPOLYGON(((3 7,3 6,2 6,2 7,3 7)),((4 8,4 7,3 7,3 8,4 8)),((6 10,7 10,7 9,5 9,5 10,6 10)),((5 8,4 8,4 9,5 9,5 8)),((5 8,6 8,6 7,5 7,5 8)))"
};

static std::string case_recursive_boxes_40[2] =
{
    "MULTIPOLYGON(((8 7,9 7,9 6,7 6,7 7,8 7)),((6 4,7 4,7 3,6 3,6 4)),((6 4,5 4,5 5,6 5,6 4)),((3 6,3 7,4 7,4 6,3 6)),((3 6,3 5,2 5,2 6,3 6)),((1 0,1 1,2 1,2 0,1 0)),((7 9,8 9,8 8,6 8,6 9,7 9)),((6 8,6 7,5 7,5 8,6 8)),((5 1,5 0,3 0,3 1,5 1)),((5 1,5 2,6 2,6 1,5 1)),((4 3,4 4,5 4,5 3,4 3)),((4 3,4 2,3 2,3 3,4 3)))",
    "MULTIPOLYGON(((7 2,7 3,8 3,8 2,7 2)),((5 9,5 10,6 10,6 9,5 9)),((7 0,6 0,6 2,7 2,7 1,8 1,8 0,7 0)),((5 3,5 4,6 4,6 3,5 3)),((7 4,7 5,8 5,8 4,7 4)),((2 2,2 1,1 1,1 2,2 2)),((2 2,2 3,3 3,3 2,2 2)),((4 7,4 6,3 6,3 7,4 7)),((5 5,4 5,4 6,5 6,5 7,6 7,6 5,5 5)),((5 7,4 7,4 8,5 8,5 7)))"
};

static std::string case_recursive_boxes_41[2] =
{
    // Smaller version of 35 for validity checks
    "MULTIPOLYGON(((0 2,0 5,5 5,5 1,5 0,3 0,0 0,0 2),(0 2,1 2,1 3,0 2),(2 4,1 4,2 3,2 4)))",
    "MULTIPOLYGON(((0 0,0 5,5 5,5 0,0 0),(2 3,2 4,1 4,1 3,2 3)))"
};

static std::string case_recursive_boxes_42[2] =
{
    // Smaller version of 2 for validity checks
    "MULTIPOLYGON(((0 0,0 10,6 10,6 9,7 9,7 10,10 10,10 0,0 0),(6 9,5 9,5 8,6 8,6 9),(7 6,8 6,8 7,7 7,7 6),(8 7,9 7,9 8,8 8,8 7)))",
    "MULTIPOLYGON(((0 0,0 10,10 10,10 0,0 0),(6 7,7 7,7 8,6 8,6 7)))"
};

static std::string case_recursive_boxes_43[2] =
{
    // Smaller version of 35 for validity checks
    "MULTIPOLYGON(((0 2,0 5,4 5,4 4,5 4,5 1,4 1,5 0,3 0,0 0,0 2),(3 0,3 1,2 1,3 0)))",
    "MULTIPOLYGON(((1 0,0 0,0 5,3 5,5 5,5 0,1 0),(1 0,2 1,1 1,1 0)))"
};

static std::string case_recursive_boxes_44[2] =
{
    // Needs discarding uu turns in intersections, either globally or in aggregations
    "MULTIPOLYGON(((4 1,5 1,5 0,4 0,4 1)),((4 1,2 1,2 2,4 2,4 1)),((2 2,1 2,1 1,2 1,2 0,0 0,0 3,2 3,2 2)))",
    "MULTIPOLYGON(((2 1,2 2,3 2,3 1,2 1)),((1 0,0 0,0 1,2 1,2 0,1 0)))"
};

static std::string case_recursive_boxes_45[2] =
{
    // Needs discarding u/x turns in aggregations (for clusters for intersections)
    // This case did go wrong, don't make it smaller, it also depends on the order of turns
    "MULTIPOLYGON(((5 2,5 3,4 3,4 6,6 6,6 5,8 5,8 1,5 1,5 2)),((4 7,4 6,1 6,1 4,0 4,0 7,2 7,2 8,3 8,3 7,4 7)),((3 3,2 3,2 5,3 5,3 3)),((3 3,4 3,4 0,3 0,3 1,2 1,2 2,3 2,3 3)),((1 1,2 1,2 0,1 0,1 1)),((1 1,0 1,0 2,1 2,1 1)),((2 3,2 2,1 2,1 3,2 3)))",
    "MULTIPOLYGON(((5 1,4 1,4 2,7 2,7 1,5 1)),((7 6,7 4,6 4,6 3,4 3,4 4,3 4,3 5,6 5,6 6,3 6,3 8,2 8,2 9,7 9,7 6)),((3 4,3 2,1 2,1 1,0 1,0 4,1 4,1 3,2 3,2 4,3 4)),((4 1,4 0,2 0,2 1,4 1)),((2 4,1 4,1 5,2 5,2 4)))"
};

static std::string case_recursive_boxes_46[2] =
{
    "MULTIPOLYGON(((8 9,8 10,9 10,9 9,8 9)),((5 5,5 4,3 4,3 5,5 5)),((5 5,5 6,6 6,6 5,5 5)),((5 9,6 9,6 10,7 10,7 8,9 8,9 5,10 5,10 4,9 4,6 4,6 5,7 5,7 6,6 6,6 8,3 8,3 9,4 9,4 10,5 10,5 9),(7 6,8 6,8 7,7 7,7 6)))",
    "MULTIPOLYGON(((5 10,6 10,6 9,5 9,5 10)),((5 3,4 3,4 7,7 7,7 6,8 6,8 3,5 3),(5 5,5 4,7 4,7 6,6 6,6 5,5 5)),((9 7,9 6,8 6,8 7,9 7)),((9 7,9 9,8 9,8 10,10 10,10 7,9 7)),((8 9,8 8,7 8,7 9,8 9)))"
};

static std::string case_recursive_boxes_47[2] =
{
    // Needs open_count==0 in get_ring_turn_info to select_rings
    "MULTIPOLYGON(((5 4,6 4,6 3,4 3,4 5,5 5,5 4)),((5 5,5 6,6 6,6 5,5 5)),((5 6,3 6,3 7,4 7,4 8,7 8,7 6,6 6,6 7,5 7,5 6)),((6 4,6 5,7 5,7 4,6 4)))",
    "MULTIPOLYGON(((4 5,4 4,1 4,1 5,4 5)),((5 6,5 7,6 7,6 6,5 6)),((5 6,5 5,4 5,4 6,5 6)),((5 3,5 5,6 5,6 3,5 3)),((5 3,5 2,4 2,4 3,5 3)),((6 5,6 6,7 6,7 5,6 5)),((3 7,2 7,2 9,3 9,3 8,4 8,4 7,3 7)))"
};

static std::string case_recursive_boxes_48[2] =
{
    // Needs discarding self-ii turns not located within the other geometry
    "MULTIPOLYGON(((6 7,6 8,7 8,7 7,6 7)))",
    "MULTIPOLYGON(((9 9,10 9,10 7,9 7,9 5,8 5,8 6,7 6,7 7,6 7,6 8,7 8,7 9,8 9,8 10,9 10,9 9),(9 8,8 8,8 7,9 7,9 8)))"
};

static std::string case_recursive_boxes_49[2] =
{
    // Needs specific handling for cc interior rings touching one exterior ring
    "MULTIPOLYGON(((5 2,5 3,4 3,4 4,5 4,5 5,6 5,6 2,7 2,7 1,0 1,0 3,1 3,1 2,5 2)),((5 7,5 8,6 8,6 9,7 9,7 10,10 10,10 8,9 8,9 6,10 6,10 4,9 4,9 3,8 3,8 4,7 4,7 6,5 6,5 7),(7 6,8 6,8 7,7 7,7 6),(8 9,7 9,7 8,8 8,8 9)),((2 6,4 6,4 5,3 5,3 4,1 4,1 5,2 5,2 6)),((3 10,4 10,4 7,2 7,2 10,3 10)))",
    "MULTIPOLYGON(((5 1,4 1,4 3,3 3,3 6,2 6,2 9,5 9,5 7,4 7,4 4,5 4,5 3,6 3,6 2,8 2,8 1,9 1,9 0,5 0,5 1),(4 7,4 8,3 8,3 7,4 7)),((8 2,8 3,9 3,9 2,8 2)),((9 5,9 6,10 6,10 5,9 5)),((9 5,9 4,8 4,8 5,9 5)),((7 5,7 4,6 4,6 5,7 5)),((7 10,9 10,9 9,10 9,10 7,8 7,8 6,6 6,6 10,7 10),(7 9,7 8,8 8,8 9,7 9)))"
};

static std::string case_recursive_boxes_50[2] =
{
    // Same as 49 but needs the second variant
    "MULTIPOLYGON(((6 9,6 10,7 10,7 9,6 9)),((5 6,6 6,6 5,4 5,4 7,3 7,3 8,4 8,4 9,2 9,2 10,5 10,5 8,6 8,6 7,5 7,5 6)),((4 0,1 0,1 1,2 1,2 2,3 2,3 3,5 3,5 0,4 0)),((3 5,3 3,1 3,1 4,0 4,0 6,1 6,1 7,0 7,0 8,2 8,2 6,3 6,3 5),(2 5,1 5,1 4,2 4,2 5)),((1 1,0 1,0 2,1 2,1 1)),((6 5,7 5,7 4,6 4,6 5)),((10 5,8 5,8 6,9 6,9 7,10 7,10 5)),((6 0,6 1,8 1,8 2,10 2,10 0,6 0)),((9 7,7 7,7 8,8 8,8 9,9 9,9 7)),((8 2,6 2,6 3,8 3,8 2)),((9 9,9 10,10 10,10 9,9 9)))",
    "MULTIPOLYGON(((5 3,5 4,6 4,6 3,5 3)),((5 7,6 7,6 5,4 5,4 6,3 6,3 7,4 7,4 8,5 8,5 7)),((3 6,3 4,4 4,4 3,1 3,1 2,0 2,0 9,1 9,1 10,3 10,3 9,2 9,2 8,3 8,3 7,1 7,1 6,3 6),(1 5,1 4,2 4,2 5,1 5)),((4 0,0 0,0 1,1 1,1 2,3 2,3 1,5 1,5 0,4 0)),((7 6,7 7,8 7,8 8,9 8,9 6,8 6,8 5,7 5,7 6)),((9 0,7 0,7 1,9 1,9 2,10 2,10 0,9 0)),((7 9,5 9,5 10,7 10,7 9)),((7 9,8 9,8 8,7 8,7 9)),((8 3,8 2,7 2,7 3,8 3)),((8 3,8 4,9 4,9 3,8 3)),((8 9,8 10,9 10,9 9,8 9)),((9 8,9 9,10 9,10 8,9 8)))"
};

static std::string case_recursive_boxes_51[2] =
{
    // Needs keeping colocated turns with a xx turn to properly generate interior rings. It also needs self-turns for validity
    "MULTIPOLYGON(((0 4,0 5,1 5,1 4,0 4)),((2 1,3 1,3 0,2 0,2 1)),((3 3,4 3,4 2,3 2,3 3)),((5 4,6 4,6 3,5 3,5 4)),((2 3,2 4,3 4,3 3,2 3)),((5 6,4 6,4 5,3 5,3 10,4 10,4 9,5 9,5 8,4 8,4 7,6 7,6 9,7 9,7 10,8 10,8 8,7 8,7 7,8 7,8 4,7 4,7 5,6 5,6 6,5 6)),((5 0,4 0,4 1,5 1,5 2,6 2,6 3,7 3,7 1,8 1,8 0,5 0)),((0 2,1 2,1 1,0 1,0 2)),((1 2,1 3,2 3,2 2,1 2)),((1 10,2 10,2 8,1 8,1 9,0 9,0 10,1 10)),((1 8,1 7,0 7,0 8,1 8)),((10 1,10 0,9 0,9 1,8 1,8 4,9 4,9 6,10 6,10 1)))",
    "MULTIPOLYGON(((3 1,4 1,4 0,2 0,2 1,3 1)),((1 9,0 9,0 10,1 10,1 9)),((8 2,9 2,9 1,10 1,10 0,8 0,8 2)),((5 8,4 8,4 9,5 9,5 10,7 10,7 8,8 8,8 7,7 7,7 6,8 6,8 7,10 7,10 6,9 6,9 3,8 3,8 2,6 2,6 1,5 1,5 3,7 3,7 4,4 4,4 5,5 5,5 6,6 6,6 8,5 8),(7 5,7 4,8 4,8 5,7 5)),((4 4,4 3,2 3,2 4,1 4,1 5,0 5,0 7,1 7,1 9,2 9,2 8,4 8,4 6,3 6,3 4,4 4),(1 5,2 5,2 6,1 6,1 5)),((0 3,1 3,1 1,0 1,0 3)),((4 9,3 9,3 10,4 10,4 9)),((9 10,10 10,10 9,9 9,9 8,8 8,8 10,9 10)))"
};

static std::string case_recursive_boxes_52[2] =
{
    // Needs checking for isolated in handling of cc_ii clusters for intersection
    "MULTIPOLYGON(((0 0,0 1,1 1,1 0,0 0)),((5 1,2 1,2 3,1 3,1 4,0 4,0 8,1 8,1 9,2 9,2 8,5 8,5 5,6 5,6 7,7 7,7 9,9 9,9 10,10 10,10 7,9 7,9 6,10 6,10 5,9 5,9 1,8 1,8 2,7 2,7 3,6 3,6 4,5 4,5 2,6 2,6 0,5 0,5 1),(4 4,4 5,3 5,3 6,4 6,4 7,1 7,1 6,2 6,2 5,3 5,3 4,4 4),(7 5,7 4,8 4,8 6,7 6,7 5),(2 5,1 5,1 4,2 4,2 5)),((5 8,5 9,6 9,6 8,5 8)),((8 1,8 0,7 0,7 1,8 1)))",
    "MULTIPOLYGON(((1 1,2 1,2 0,1 0,1 1)),((9 10,10 10,10 9,9 9,9 10)),((3 9,3 10,4 10,4 9,3 9)),((1 10,2 10,2 9,0 9,0 10,1 10)),((5 4,4 4,4 3,3 3,3 2,1 2,1 1,0 1,0 3,2 3,2 4,1 4,1 5,5 5,5 7,6 7,6 6,7 6,7 8,9 8,9 7,8 7,8 6,9 6,9 4,8 4,8 3,10 3,10 2,8 2,8 1,7 1,7 0,6 0,6 2,7 2,7 3,6 3,6 4,5 4),(8 5,7 5,7 4,8 4,8 5)),((5 9,5 10,8 10,8 9,6 9,6 8,5 8,5 9)),((4 0,3 0,3 1,4 1,4 2,5 2,5 0,4 0)),((3 8,4 8,4 6,2 6,2 8,3 8)),((9 1,9 0,8 0,8 1,9 1)),((9 7,10 7,10 6,9 6,9 7)))"
};

static std::string case_recursive_boxes_53[2] =
{
    // Needs checking for intersection patterns intersection_pattern_common_interior2 with ii
    "MULTIPOLYGON(((2 0,0 0,0 5,5 5,4 4,5 4,5 0,2 0),(2.5 4.5,3 4,3 5,2.5 4.5),(3 1,4 0,4 1,3 1),(3 3,4 2,4 3,3 3)))",
    "MULTIPOLYGON(((2 0,0 0,0 5,1 5,0.5 4.5,1 4,2 5,5 5,5 0,2 0),(2 3,2 4,1 4,2 3),(3 1,3 2,2 2,2 1,3 1),(1 3,0 2,1 2,1 3),(1 0,1 1,0.5 0.5,1 0),(5 3,4 3,4 2,5 3),(4 1,3.5 0.5,4 0,4 1)))"
};

static std::string case_recursive_boxes_54[2] =
{
    // Needs including blocked from-ranks/operations in aggregations
    "MULTIPOLYGON(((2 2,2 3,1 3,1 5,2 4,2 5,5 5,5 4,4 3,5 3,5 2,4 2,4 1,5 0,2 0,2 1,1 1,1 2,2 2),(3 2,2.5 1.5,3 1,4 2,3 2),(3 2,3 3,2.5 2.5,3 2),(3 4,3 3,4 4,3 4)),((1 1,1 0,0 0,0 1,1 1)),((1 3,1 2,0 1,0 3,1 3)))",
    "MULTIPOLYGON(((2 2,3 3,2 3,2 5,5 5,5 3,4 3,3 2,5 2,5 1,4 1,3 0,1 0,1 1,0 1,0 2,2 2)),((0 4,0 5,1 5,1 4,0 3,0 4)))"
};

static std::string case_recursive_boxes_55[2] =
{
    // Needs correct handling for union clusters with 3 open spaces
    "MULTIPOLYGON(((3 4,3 5,4 5,3 4)),((0 4,1 4,1 3,0 3,0 4)),((2 1,1.5 0.5,2 0,1 0,1 1,0 1,0 2,2 2,2 1)),((3 2,3 3,4 2,4 3,5 2,4 1,2 1,3 2)),((2 2,2 3,3 3,2 2)))",
    "MULTIPOLYGON(((2 1,2 0,1 0,2 1)),((2 1,2 2,3 2,3 1,2 1)),((1 3,1 4,2 5,2 2,1 2,0 1,0 4,1 3)),((5 4,4 3,3 3,4 4,3 4,3 5,5 5,5 4)),((1 0,0 0,1 1,1 0)),((0 4,0 5,1 5,0 4)))"
};

static std::string case_recursive_boxes_56[2] =
{
    // Needs not discarding clustered self-turns if the cluster does not contain anything else
    "MULTIPOLYGON(((3 4,3 5,4 5,3 4)),((3 3,4 3,2 1,2 3,3 3)),((2 1,2 0,1 0,1 1,2 1)),((1 4,1 3,0 3,0 4,1 5,1 4)))",
    "MULTIPOLYGON(((2 4,2 5,3 4,2 4)),((1 5,1 4,0 4,0 5,1 5)),((1 2,0 2,0 3,1 2)),((2 2,1 2,2 3,2 2)),((2 2,2 1,1 1,1 2,1.5 1.5,2 2)))"
};

static std::string case_recursive_boxes_57[2] =
{
    // Needs handling of 3 open spaces in union to form interior ring
    "MULTIPOLYGON(((2 0,0 0,0 1,0.5 0.5,1 1,1 2,2 1,4 1,4 4,5 3,5 0,2 0)),((2 5,2 4,1 4,1 5,2 5)),((1 2,0 2,0 3,1 2)),((1 2,1 3,2 3,2 2,1 2)),((0 1,0 2,1 1,0 1)),((4 3,3 3,3 4,4 3)),((3 4,3 5,5 5,5 4,3 4)))",
    "MULTIPOLYGON(((3 3,2 3,2 5,3 4,3 5,5 5,5 4,4 4,4 3,3 3)),((3 3,4 2,2 2,3 3)),((0 0,0 4,1 4,1 3,2 3,2 2,1 2,1 0,0 0)),((3 1,2 1,2 2,3 1)),((1 5,0 4,0 5,1 5)),((1 5,2 5,1 4,1 5)),((5 2,5 1,4 1,4 3,5 4,5 2)),((4 1,4 0,3 0,3 1,4 1)),((2 1,2 0,1 0,2 1)))",
};

static std::string case_recursive_boxes_58[2] =
{
    // Needs correct handling for union clusters with 3 open spaces
    "MULTIPOLYGON(((0 0,1 1,2 1,1 0,0 0)),((1 1,1 2,2 2,1 1)),((3 1,4 2,4 1,3 1)),((3 2,3 3,4 3,3 2)))",
    "MULTIPOLYGON(((0 1,0 2,1 1,0 1)),((1 1,1 2,2 1,1 1)),((1 2,1 3,2 2,1 2)),((2 2,2 3,3 2,2 2)),((4 2,4 3,5 3,4 2)),((1 0,2 1,2 2,3 1,2 0,1 0)))",
};

static std::string case_recursive_boxes_59[2] =
{
    // Needs correct handling for union clusters with 3 open spaces
    "MULTIPOLYGON(((2 3,2 2,3 2,3 1,2 1,1.5 0.5,2 0,0 0,0 1,1 1,1 4,2 4,3 5,5 5,5 4,3 4,3 3,2 3)),((3 2,3 3,4 3,4 2,3 2)),((2 0,3 1,5 1,5 0,2 0)),((0 3,0 5,1 5,1 4,0 3)),((0 3,1 3,0 2,0 3)))",
    "MULTIPOLYGON(((3 5,3 4,4 4,4 3,5 3,5 2,4 2,4 1,3.5 0.5,4 0,1 0,1 1,2 2,1 2,1 4,0 4,1 5,3 5),(2 3,2 4,1 3,2 3),(2 2,3 2,3 3,2 2)),((1 1,0 1,0 3,1 2,1 1)))",
};

static std::string case_recursive_boxes_60[2] =
{
    // Needs checking left_count in union clusters
    "MULTIPOLYGON(((0 4,0 5,1 5,0 4)),((1 2,1 4,2 4,2 5,3 4,4 5,5 5,5 4,4 4,3.5 3.5,4 3,4 2,3 2,2 1,2 2,1 2)),((1 2,2 1,1 1,1 2)),((2 1,3 1,4 2,5 1,4 1,4 0,2 0,2 1)),((0 0,0 2,2 0,0 0)),((0 2,0 3,1 3,0 2)),((5 4,5 3,4 3,5 4)))",
    "MULTIPOLYGON(((2 3,1 2,1 4,0 4,0 5,3 5,3 3,2 3)),((1 2,3 2,3 3,4 4,5 4,5 3,4 3,3.5 2.5,4 2,4 0,0 0,0 1,1 1,1 2)),((4 2,5 3,5 2,4 2)))",
};


static std::string case_recursive_boxes_61[2] =
{
    // Needs pattern 4 for intersections
    "MULTIPOLYGON(((2 0,2 1,1 0,0 0,0 1,1 2,0 2,0 4,1 4,1 5,1.5 4.5,2 5,4 5,4.5 4.5,5 5,5 2,4.5 1.5,5 1,5 0,2 0),(3 1,3 2,2 2,2 1,3 1),(4 5,3.5 4.5,4 4,4 5)),((0 4,0 5,1 5,0 4)))",
    "MULTIPOLYGON(((3 5,5 5,5 0,2 0,2 1,1 1,1 0,0 0,0 2,1 2,1 3,0 3,0 5,3 5),(3 1,3 2,2 2,2 1,3 1)))",
};

static std::string case_recursive_boxes_62[2] =
{
    // Needs discarding self-intersections within other geometry
    "MULTIPOLYGON(((2.5 3.5,3 4,3 3,4 3,4 2,5 2,5 0,1 0,1 1,2 1,3 2,2 2,2 3,1 3,2 4,2.5 3.5),(3 1,3 0,4 1,3 1)),((1 0,0 0,0 1,1 0)),((2 4,2 5,3 4,2 4)),((2 4,1 4,1 5,2 4)),((3 4,3 5,4 5,4 4,3 4)),((0 1,0 3,0.5 2.5,1 3,1 1,0 1)),((1 3,0 3,0 4,1 4,1 3)),((4 4,5 4,5 3,4 3,4 4)))",
    "MULTIPOLYGON(((2 0,1 0,1 1,2 2,2 1,4 1,3 0,2 0)),((2 2,1 2,1 1,0.5 0.5,1 0,0 0,0 5,4 5,4 4,3 3,4 3,4 2,2 2),(1 3,1 4,0 4,1 3)),((4 2,5 1,4 1,4 2)))",
};

static std::string case_recursive_boxes_63[2] =
{
    // Derived from 62, needs not excluding startable points for checking rings for traversals
    "MULTIPOLYGON(((2 0,1 0,1 1,2 2,2 1,4 1,3 0,2 0)),((2 2,1 2,1 1,0.5 0.5,1 0,0 0,0 5,4 5,4 4,3 3,4 3,4 2,2 2),(1 3,1 4,0 4,1 3)),((4 2,5 1,4 1,4 2)))",
    "MULTIPOLYGON(((-1 -1, 6 -1, 6 6, -1 6, -1 -1), (0 0, 0 1, 0 3, 0 4, 1 4, 1 5, 2 4, 2 5, 3 4, 3 5, 4 5, 4 4, 5 4, 5 3, 4 3, 4 2, 5 2, 5 0, 3 0, 1 0, 0 0)),((2.5 3.5, 3 4, 2 4, 2.5 3.5)), ((3 3, 4 3, 4 4, 3 4, 3 3)), ((1 3, 2 4, 1 4, 1 3)), ((0.5 2.5, 1 3, 0 3, 0.5 2.5)), ((1 1, 2 1, 3 2, 2 2, 2 3, 1 3, 1 1)), ((3 0, 4 1, 3 1, 3 0)), ((1 0, 1 1, 0 1, 1 0)))",
};

static std::string case_recursive_boxes_64[2] =
{
    // Needs considering remaining_distance in clusters
    "MULTIPOLYGON(((3 4,3 5,5 5,5 4,4 4,3 3,3.5 2.5,4 3,5 3,5 1,4 0,0 0,0 5,2 5,2 4,3 4),(2 2,3 2,3 3,2 2),(2 3,2 4,1 4,2 3),(2 0,2 1,1 1,2 0),(1 2,0 2,0.5 1.5,1 2)))",
    "MULTIPOLYGON(((3 5,5 5,5 0,0 0,1 1,0 1,0 3,1 3,1 4,1.5 3.5,2 4,2 5,3 5),(2 2,1 2,2 1,2 2),(2 2,2.5 1.5,3 2,2 2),(4 4,4.5 3.5,5 4,4 4),(4 1,3 1,4 0,4 1)),((2 5,0 3,0 5,2 5)))"
};

static std::string case_recursive_boxes_65[2] =
{
    // Misses large hole in intersection
    "MULTIPOLYGON(((3 5,3.5 4.5,4 5,5 5,5 1,4 0,3 0,3 1,2 0,0 0,0 4,1 4,1 5,3 5),(2 4,2 3,3 4,2 4),(2 1,2 2,1.5 1.5,2 1),(3 3,4 2,4 3,3 3)))",
    "MULTIPOLYGON(((3 5,4 5,4 4,5 5,5 2,4 2,3.5 1.5,5 0,1 0,1 1,0 1,0 5,3 5),(2 2,3 2,4 3,1 3,1 2,2 2)),((1 0,0 0,0 1,1 0)),((4 1,4 2,5 1,4 1)))"
};

static std::string case_recursive_boxes_66[2] =
{
    // Needs self-turns startable, at least not determined using count left/right for self-turns
    "MULTIPOLYGON(((1 0,0 0,1 1,0 1,0 4,1 4,1 5,3 5,3 4,4 5,5 5,5 0,1 0),(3 3,4 2,4 3,3 3),(3 1,4 1,4 2,3 2,3 1),(3 3,3 4,2 3,3 3),(3 4,3.5 3.5,4 4,3 4)))",
    "MULTIPOLYGON(((2 0,0 0,0 1,1 1,1 2,0 2,0 5,1 5,2 4,2 5,5 5,5 0,2 0),(3 1,3 2,2.5 1.5,3 1),(2 3,1 2,2 1,2 2,3 2,4 3,2 3),(0 3,0.5 2.5,1 3,1 4,0 3)))"
};

static std::string case_recursive_boxes_67[2] =
{
    // Needs to avoid including any untraveled ring with blocked turns
    "MULTIPOLYGON(((2 2,3 3,3 2,2 2)),((2 2,1 2,1 3,2 3,2 2)),((1 1,2 1,2 0,0 0,0 1,1 1)),((2 4,1 4,0.5 3.5,1 3,0 3,0 5,1 5,2 4)),((4 2,4 3,5 2,4 2)),((4 2,4 1,3 1,4 2)),((3 3,3 4,4 4,4 3,3 3)),((4 4,4 5,5 5,5 4,4 4)))",
    "MULTIPOLYGON(((3 4,4 5,4 4,3 4)),((3 1,3 0,2 0,3 1)),((3 1,1 1,1 2,2 3,3 3,3 2,2 2,3 1)),((3 1,3 2,5 2,5 1,3 1)),((2 3,0 3,0 4,1 4,2 3)),((1 3,1 2,0 2,1 3)),((1 4,1 5,2 5,1 4)))"
};

static std::string case_recursive_boxes_68[2] =
{
    // Needs checking blocked turns in colocated turns
    "MULTIPOLYGON(((3 3,3 4,4 4,4 5,5 5,4.5 4.5,5 4,5 3,4.5 2.5,5 2,2 2,2 4,3 3)),((2 5,3 5,3 4,2 4,1 3,1 2,0 2,0 4,1 4,1 5,2 5)),((2 2,4 0,1 0,2 1,1 1,1 2,2 2)),((1 0,0 0,0 1,1 1,1 0)),((4 0,5 1,5 0,4 0)),((5 1,4 1,4 2,5 1)))",
    "MULTIPOLYGON(((2 0,2 1,1 1,2 2,3 2,3 1,4 2,5 2,5 0,2 0)),((2 0,1 0,1 1,2 0)),((2 2,2 3,3 4,3 3,2 2)),((3 2,3 3,4 4,5 4,5 3,4 2,3 2)),((3 4,3 5,4 4,3 4)),((2 4,2 3,1 2,1 3,0 3,0 5,1 5,1 4,2 4)),((1 2,1 1,0 1,0 2,1 2)),((4 4,4 5,5 5,4 4)))"
};

static std::string case_recursive_boxes_69[2] =
{
    // Needs checking left_count instead of is_closed for decision to block untraversed rings
    "MULTIPOLYGON(((3 4,3 5,4 5,3 4)),((3 4,3 2,2 2,3 1,1 1,1 2,0 2,1 3,0 3,1 4,3 4)),((3 1,4 1,4 0,3 0,3 1)),((1 1,2 0,0 0,0 1,1 1)))",
    "MULTIPOLYGON(((2 4,1 4,1 3,0 3,0 5,2 5,3 4,2 4)),((3 4,3 5,4 5,4 4,3 4)),((2 1,2 0,1 0,2 1)),((2 1,1 1,1 2,2 2,2 1)),((5 1,5 0,3 0,4 1,5 1)),((4 1,3 1,3 2,4 3,5 2,4 2,4 1)),((1 3,2 3,1 2,1 3)))"
};

static std::string case_recursive_boxes_70[2] =
{
    // Needs checking left_count instead of is_closed for decision to block untraversed rings
    "MULTIPOLYGON(((2 0,0 0,0 4,1 3,3 3,3 5,5 5,5 3,4.5 2.5,5 2,5 0,2 0),(0 1,0.5 0.5,1 1,0 1),(5 2,4 2,4.5 1.5,5 2),(4 2,3 2,3 1,4 2)),((3 4,2 3,2 4,3 4)),((0 4,0 5,1 5,1.5 4.5,2 5,2 4,0 4)))",
    "MULTIPOLYGON(((2 0,0 0,0 5,5 5,5 2,4.5 1.5,5 1,5 0,2 0),(2 1,3 0,3 1,2 1),(4 4,3 3,4 3,4 4),(1 1,1 2,0 2,1 1),(4 3,4.5 2.5,5 3,4 3)))"
};

static std::string case_recursive_boxes_71[2] =
{
    // Needs check for self-cluster within other geometry, in intersections
    "MULTIPOLYGON(((4 0,4 1,5 1,5 0,4 0)),((4 3,4 4,5 4,4 3)),((3 3,4 2,3 2,3 3)),((3 3,2 3,3 4,3 3)),((3 2,3 0,1 0,1 3,0 3,0 4,1 4,3 2),(3 2,2 2,2 1,3 2)),((1 4,1 5,2 5,2 4,1 4)))",
    "MULTIPOLYGON(((3 0,3 1,4 0,3 0)),((2 2,0 2,0 3,1 3,2 2)),((2 2,2 3,3 3,2 2)),((2 4,0 4,1 5,3 5,3 4,2 4)),((3 3,3 4,4 5,5 4,4 4,4 3,3 3)),((4 3,5 3,4 2,4 3)))"
};

static std::string case_recursive_boxes_72[2] =
{
    // Needs selection of ranked point in union (to finish the ring)
    "MULTIPOLYGON(((3 1,4 1,4 0,3 0,3 1)),((1 0,1 1,2 1,2 0,1 0)),((3 3,3 5,5 5,5 2,4 2,4 3,3 3)),((3 3,2 2,2 3,3 3)),((1 3,0 2,0 3,1 3)),((1 4,2 4,2 3,1 2,1 4)),((1 4,1 5,2 5,1 4)),((2 5,3 5,2 4,2 5)),((1 5,0 4,0 5,1 5)))",
    "MULTIPOLYGON(((4 0,4 1,5 0,4 0)),((4 4,4 5,5 5,5 4,4 4)),((2 4,2 3,1 3,1 4,2 4)),((2 4,2 5,3 5,2 4)),((2 2,2 3,3 3,2 2)),((2 2,3 2,3 1,2 1,2 2)),((3 3,4 2,3 2,3 3)),((3 1,3 0,2 0,3 1)),((1 3,2 2,1 1,1 3)),((1 2,0 2,0 3,1 2)),((1 4,0 4,0 5,1 5,1 4)),((4 2,4 3,5 2,4 2)))"
};

static std::string case_recursive_boxes_73[2] =
{
    // Needs handling connection points identical for clustered/non-clustered turns
    "MULTIPOLYGON(((3 5,5 5,5 4,4.5 3.5,5 3,5 2,3 2,3 1,4 0,0 0,0 5,1 4,2 4,2 5,3 5),(2 3,2 2,3 3,2 3),(1 2,0 2,1 1,1 2)),((1 4,1 5,2 5,1 4)),((4 0,4 1,5 1,5 0,4 0)))",
    "MULTIPOLYGON(((2 0,0 0,0 5,1 5,2 4,2 5,4 5,4.5 4.5,5 5,5 4,4 4,4 3,5 3,5 0,2 0),(0 2,0.5 1.5,1 2,0 2),(1 4,2 3,2 4,1 4)))"
};

static std::string case_recursive_boxes_74[2] =
{
    // Needs another method to find isolated regions (previous attempted used parents, that is dropped now)
    "MULTIPOLYGON(((3 5,5 5,5 0,0 0,0 5,3 5),(2 3,1.5 2.5,2 2,3 3,2 3),(3 3,4 2,4 3,3 3),(2 0,3 1,2 1,2 0)))",
    "MULTIPOLYGON(((2 0,0 0,0 5,1 5,2 4,2 5,5 5,5 0,2 0),(2 1,2.5 0.5,3 1,2 1),(2 3,3 3,3 4,2 4,2 3),(3 1,4 1,5 2,3 2,3 1)))"
};

static std::string case_recursive_boxes_75[2] =
{
    // Needs intersection pattern 6 (skip all isolated ranks in between)
    "MULTIPOLYGON(((3 1,2 1,1 0,0 0,0 5,5 5,5 3,4 2,5 2,5 0,3 0,3 1),(2 3,1 3,2 2,2 3),(3 4,3.5 3.5,4 4,3 4),(3 3,3 2,4 2,4 3,3 3)))",
    "MULTIPOLYGON(((3 5,4 5,4.5 4.5,5 5,5 0,4 0,4 1,2 1,2 2,1 2,1 3,0 2,0 5,1 5,0.5 4.5,1 4,2 5,3 5),(3 4,3 3,4 4,3 4),(3 3,4 2,4 3,3 3)),((2 1,2 0,1 0,1 1,2 1)),((0 2,1 2,0.5 1.5,1 1,0 1,0 2)),((1 0,0 0,0 1,1 0)))"
};

static std::string case_recursive_boxes_76[2] =
{
    // Needs considering ix/ix turns (opposite in both directions) as the same region
    "MULTIPOLYGON(((3 5,3 4,4 5,4 4,5 4,5 0,0 0,0 4,1 4,1 5,3 5),(3 1,2 1,2.5 0.5,3 1),(1 2,0 2,0.5 1.5,1 2)))",
    "MULTIPOLYGON(((2 0,0 0,0 3,1 4,0 4,0 5,2 5,2 4,3 4,4 5,5 5,5 0,2 0),(3 1,4 0,4 1,3 1),(3 2,2 1,3 1,3 2),(3 3,3 4,2 3,3 3),(2 1,1 1,1.5 0.5,2 1),(4 3,4 4,3 4,4 3)))"
};

static std::string case_recursive_boxes_77[2] =
{
    // Needs discarding self i/u turn traveling to itself (for intersection only)
    "MULTIPOLYGON(((1 2,2 3,2 2,1 1,1 2)),((1 4,1 5,2 5,2 4,1 4)),((1 4,0 3,0 4,1 4)),((4 2,4 1,3 1,3 3,4 2)),((3 1,3 0,1 0,2 1,2.5 0.5,3 1)),((3 3,3 5,4 5,4 3,3 3)))",
    "MULTIPOLYGON(((3 4,3 5,4 5,4 4,3 4)),((3 1,3 0,2 0,3 1)),((3 1,2 1,2 2,3 2,3 1)),((2 3,2 4,3 3,2 3)),((2 1,1 0,0 0,1 1,2 1)),((1 4,2 3,2 2,0 2,0 4,1 4),(2 3,1 3,1.5 2.5,2 3)),((1 4,1 5,2 5,2 4,1 4)),((4 1,5 0,4 0,4 1)),((4 1,4 2,5 1,4 1)),((4 2,5 3,5 2,4 2)))"
};

static std::string case_recursive_boxes_78[2] =
{
    // Needs checking intersection/right_count in overlay, as was already done for union
    "MULTIPOLYGON(((3 2,1 2,1 4,3 2)),((3 2,3 4,4 4,3.5 3.5,4.5 2.5,5 3,5 1,4 1,3 0,3 2),(4 2,3.5 1.5,4 1,4 2)),((1 2,1 1,0 0,0 2,1 2)),((0 4,1 3,0 3,0 4)),((0 4,1 5,1.5 4.5,2 5,3 5,3 4,0 4)),((1 1,2 1,1 0,1 1)),((4 1,5 0,4 0,4 1)),((4 3,4 5,5 4,5 3,4 3)))",
    "MULTIPOLYGON(((2 4,1 4,1 5,3 5,2 4)),((2 4,4 4,4 3,5 3,5 2,4 1,4 2,3 2,3 3,2 2,2 4)),((2 2,2 1,0 1,0 4,1 3,1 4,2 3,1 2,2 2)),((1 4,0 4,0 5,1 4)),((0 1,1 0,0 0,0 1)),((4 1,3 1,3 2,4 1)))"
};

static std::string case_recursive_boxes_79[2] =
{
    // Found by bug in discard_self_turns_which_loop, it needs only checking intersection and not union
    "MULTIPOLYGON(((2 3,2 2,1 2,1 4,4 4,4 3,2 3)),((2 0,0 0,0 2,1 2,1.5 1.5,2 2,2 1,1 1,2 0)),((2 0,2 1,3 2,4 2,3.5 1.5,4 1,4 0,2 0),(2 1,2.5 0.5,3 1,2 1)))",
    "MULTIPOLYGON(((2 0,1 0,1 1,0 1,0 3,1 4,4 4,4 0,2 0),(2 2,1 2,1 1,2 2),(4 3,3 3,3 2,4 3)))"
};

static std::string case_recursive_boxes_80[2] =
{
    // Creates very small interior ring (~0) for union. This is a robustness
    // problem, it should not be generated. The intersection point is a tiny
    // distance away from real IP, and therefore it generates a correct
    // interior ring, and is considered as valid. But if you combine this
    // resulting union later with other polygons, with another rescaling model,
    // it most probably will be invalid.
    // These cases are found with recursive_polygons and size=4.
    // For size=5 the scaling is such that it does not occur (so often)
    // It needs removing the rescaling.
    "MULTIPOLYGON(((3.5 2.5,4 3,4 2,3 2,3 3,3.5 2.5)))",
    "MULTIPOLYGON(((1 1,1 2,2 1,1 1)),((3 2,3 3,4 3,3 2)))"
};

static std::string case_recursive_boxes_81[2] =
{
    "MULTIPOLYGON(((3 4,2 4,2 5,3 4)),((3 3,2 3,2 4,3 3)),((3 3,3 4,4 5,5 4,5 3,3 3),(4 4,3.5 3.5,4 3,4 4)),((2 1,2 0,1 0,1 1,0 1,1 2,3 2,4 3,4 1,2 1)))",
    "MULTIPOLYGON(((2 4,2 2,1 2,0 1,0 3,1 3,1 4,2 4)),((2 4,2 5,3 4,2 4)),((3 4,5 4,5 3,3 3,3 4)),((1 4,0 4,0 5,1 5,1 4)),((2 1,2 0,0 0,0 1,2 1)),((4 2,5 2,4 1,3 1,4 2)),((4 1,5 1,4 0,4 1)))"
};

static std::string case_recursive_boxes_82[2] =
{
    // Contains two outgoing arcs on same ring causing current aggregation implementation to fail.
    // Fixed by greatly simplifying the code, skipping aggregations and using sbs directly (which is now possible, now that isolation-information is much better)
    "MULTIPOLYGON(((4 0,5 1,5 0,4 0)),((3 3,3 1,4 1,3 0,0 0,0 5,1 5,1 4,1.5 3.5,2 4,2 3,3 3),(2 2,2 1,3 2,2 2)),((2 4,3 5,4 5,3.5 4.5,5 3,3 3,3 4,2 4)),((4 4,4 5,5 5,4.5 4.5,5 4,4 4)))",
    "MULTIPOLYGON(((2 4,2 5,4 5,4 4,2 4)),((2 4,2 3,5 3,5 2,4 2,4 1,1 1,1 2,0 2,0 5,1 5,1 4,2 4),(2 2,1 2,1.5 1.5,2 2),(3 2,3.5 1.5,4 2,3 2)),((4 4,5 5,5 4,4 4)))"
};

static std::string case_recursive_boxes_83[2] =
{
    // Needs to select on operation in cluster
    "MULTIPOLYGON(((2 1,2 0,1 0,1 4,2 4,2 5,3 5,4 4,4 3,5 3,4 2,5 2,5 0,4 0,4 1,3 0,3 1,2 1),(2 3,1 3,2 2,2 3),(3 2,3 3,2 2,3 2)),((0 2,1 1,0 0,0 2)),((0 2,0 3,1 3,0 2)),((4 4,4 5,5 4,4 4)))",
    "MULTIPOLYGON(((2 0,1 0,1 2,2 2,2 5,3 4,3 5,4 5,4 4,5 5,5 3,4 3,4 1,5 1,5 0,2 0),(2 1,3 2,2 2,2 1)),((0 5,1 5,1 4,0 4,0 5)),((1 1,0 1,0 2,1 1)),((1 2,0 2,1 3,1 2)))"
};

static std::string case_recursive_boxes_84[2] =
{
    // Need to set some ii-turns non-startable
    "MULTIPOLYGON(((2 4,1 4,1 5,3 5,2 4)),((3 5,4 5,4 4,3 4,3 5)),((3 3,4 3,4 4,5 4,4.5 3.5,5 3,5 2,4 1,3 1,3 2,2 2,2 3,3 3),(4 2,3.5 1.5,4 1,4 2)),((0 3,0 4,1 4,1 3,0 3)))",
    "MULTIPOLYGON(((4 4,4 5,5 5,5 4,4 4)),((0 5,1 5,1 4,0 4,0 5)),((2 0,1 0,1 1,2 1,2 2,3 2,3 0,2 0)),((2 4,2 3,1 3,2 4)),((2 4,2 5,3 5,2 4)),((2 3,2 2,1 2,2 3)),((1 3,1 1,0 1,0 3,1 3)),((5 1,5 0,4 0,4 1,5 1)))"
};

static std::string case_recursive_boxes_85[2] =
{
    // Contains rescaling problem
    "MULTIPOLYGON(((5 2,5 3,6 3,6 2,5 2)),((6 5,6 6,7 6,7 5,6 5)),((3 6,1 6,2 7,3 7,3 6)),((3 6,3 5,2 5,2 6,2.5 5.5,3 6)))",
    "MULTIPOLYGON(((2 5,2 6,3 6,2 5)),((6 1,6 2,7 2,6 1)),((5 4,5 5,6 4,5 4)),((0 6,0 7,1 7,0 6)),((3 3,3 2,2 2,3 3)),((3 3,3 4,4 5,4 4,3 3)),((4 4,5 3,4 3,4 4)))"
};

static std::string case_recursive_boxes_86[2] =
{
    // Positive ring and negative ring have same area. For difference, this
    // needs to be handled correctly in assign_parents,
    // skipping the optimization for union
    "MULTIPOLYGON(((3 4,4 4,4 3,2 3,3 4)))",
    "MULTIPOLYGON(((4 1,3 1,3 2,4 1)),((4 1,5 1,5 0,4 0,4 1)))"
};

static std::string case_recursive_boxes_87[2] =
{
    // Needs to handle ii-turns for difference like done for intersection
    "MULTIPOLYGON(((5 2,5 3,6 3,5 2)),((3 4,3 3,2 3,3 4)),((3 4,2 4,3 5,3 4)),((2 4,2 3,1 3,2 4)))",
    "MULTIPOLYGON(((2 0,3 1,3 0,2 0)),((6 3,6 4,7 4,7 3,6 3)),((3 9,3 10,4 9,3 9)),((8 7,8 8,9 8,8 7)))"
};

static std::string case_recursive_boxes_88[2] =
{
    "MULTIPOLYGON(((1 4,1 5,2 5,2 4,1 4)),((0 2,0 3,1 3,0 2)),((3 5,4 5,4 4,3 4,3 5)),((2 3,4 3,4 2,3 1,2 1,2 3),(3 3,2.5 2.5,3 2,3 3)),((4 1,3 0,3 1,4 1)),((4 2,5 2,5 0,4 0,4 2)))",
    "MULTIPOLYGON(((4 0,4 1,5 1,5 0,4 0)),((3 4,3 3,1 3,1 5,3 5,2.5 4.5,3 4)),((3 4,3 5,4 5,4 4,3 4)),((1 1,2 0,1 0,1 1)),((1 1,1 2,2 2,2 1,1 1)),((1 2,0 2,0 3,1 3,1 2)),((4 4,5 4,5 2,4 2,4 4)))"
};

static std::string pie_21_7_21_0_3[2] =
{
    "MULTIPOLYGON(((2500 2500,2500 3875,2855 3828,3187 3690,3472 3472,3690 3187,3828 2855,3875 2500,3828 2144,3690 1812,3472 1527,3187 1309,2855 1171,2499 1125,2144 1171,1812 1309,1527 1527,1309 1812,1171 2144,1125 2499,1171 2855,1309 3187,2500 2500)))",
    "MULTIPOLYGON(((2500 2500,1704 3295,1937 3474,2208 3586,2499 3625,2791 3586,3062 3474,3295 3295,2500 2500)),((2500 2500,3586 2791,3625 2500,3586 2208,2500 2500)))"
};

static std::string pie_23_19_5_0_2[2] =
{
    "MULTIPOLYGON(((2500 2500,2500 3875,2855 3828,3187 3690,3472 3472,3690 3187,3828 2855,3875 2500,3828 2144,3690 1812,3472 1527,3187 1309,2855 1171,2499 1125,2144 1171,1812 1309,1527 1527,1309 1812,1171 2144,1125 2499,1171 2855,1309 3187,1527 3472,1812 3690,2500 2500)))",
    "MULTIPOLYGON(((2500 2500,3586 2791,3625 2500,3586 2208,3474 1937,3295 1704,3062 1525,2791 1413,2499 1375,2208 1413,1937 1525,1704 1704,1525 1937,1413 2208,1375 2500,1413 2791,1525 3062,1704 3295,1937 3474,2208 3586,2500 2500)),((2500 2500,2791 3586,3062 3474,2500 2500)))"
};

static std::string pie_7_14_5_0_7[2] =
{
    "MULTIPOLYGON(((2500 2500,2500 3875,2855 3828,3187 3690,3472 3472,3690 3187,3828 2855,3875 2500,2500 2500)))",
    "MULTIPOLYGON(((2500 2500,3586 2791,3625 2500,3586 2208,3474 1937,3295 1704,3062 1525,2791 1413,2499 1375,2208 1413,1937 1525,1704 1704,1525 1937,1413 2208,1375 2500,2500 2500)),((2500 2500,1525 3062,1704 3295,1937 3474,2208 3586,2499 3625,2791 3586,3062 3474,2500 2500)))"
};

static std::string pie_16_16_9_0_2[2] =
{
    "MULTIPOLYGON(((2500 2500,2500 3875,2855 3828,3187 3690,3472 3472,3690 3187,3828 2855,3875 2500,3828 2144,3690 1812,3472 1527,3187 1309,2855 1171,2499 1125,2144 1171,1812 1309,1527 1527,2500 2500)))",
    "MULTIPOLYGON(((2500 2500,3295 1704,3062 1525,2791 1413,2499 1375,2208 1413,1937 1525,1704 1704,1525 1937,1413 2208,1375 2500,1413 2791,1525 3062,1704 3295,1937 3474,2208 3586,2499 3625,2500 2500)),((2500 2500,3062 3474,3295 3295,2500 2500)))"
};

static std::string pie_7_2_1_0_15[2] =
{
    "MULTIPOLYGON(((2500 2500,2500 3875,2855 3828,3187 3690,3472 3472,3690 3187,3828 2855,3875 2500,2500 2500)))",
    "MULTIPOLYGON(((2500 2500,2791 3586,3062 3474,2500 2500)),((2500 2500,3474 3062,3586 2791,3625 2500,3586 2208,3474 1937,3295 1704,3062 1525,2791 1413,2499 1375,2208 1413,1937 1525,1704 1704,1525 1937,1413 2208,1375 2500,2500 2500)))"
};

static std::string case_precision_m1[2] =
{
    "MULTIPOLYGON(((0 0,0 4,2 4,2 3,4 3,4 0,0 0)))",
    "MULTIPOLYGON(((-1 -1,-1 8,2 8,2 7,2 3,4.0000005 2.9999995,4 7,4 8,8 8,8 -1,-1 -1)))"
};

static std::string case_precision_m2[2] =
{
    "MULTIPOLYGON(((0 0,0 4,2 4,2 3,4 3,4 0,0 0)),((3 6,3 7.5,4.5 7.5,4.5 6,3 6)))",
    "MULTIPOLYGON(((-1 -1,-1 8,8 8,8 -1,-1 -1),(2 7,2 3,4.0000005 2.9999995,4 7,2 7)))"
};

// Case, not literally on this list but derived, to mix polygon/multipolygon in call to difference
static std::string ggl_list_20111025_vd[4] =
    {
    "POLYGON((0 0,0 4,4 0,0 0))",
    "POLYGON((10 0,10 5,15 0,10 0))",
    "MULTIPOLYGON(((0 0,0 4,4 0,0 0)))",
    "MULTIPOLYGON(((10 0,10 5,15 0,10 0)))"
    };

// Same, mail with other case with text "Say the MP is the 2 squares below and P is the blue-ish rectangle."
static std::string ggl_list_20111025_vd_2[2] =
    {
    "POLYGON((5 0,5 4,8 4,8 0,5 0))",
    "MULTIPOLYGON(((0 0,0 2,2 2,2 0,0 0)),((4 0,4 2,6 2,6 0,4 0)))"
    };

// Mail of h2 indicating that reversed order (in second polygon) has ix/ix problems
static std::string ggl_list_20120915_h2[3] =
    {
        "MULTIPOLYGON(((-2 5, -1 5, 0 5, 2 5, 2 -2, 1 -2, 1 -1, 0 -1,0 0, -1 0, -2 0, -2 5)))",
        "MULTIPOLYGON(((0 0, 1 0, 1 -1, 0 -1, 0 0)), ((-1 5, 0 5, 0 0, -1 0, -1 5)))",
        "MULTIPOLYGON(((-1 5, 0 5, 0 0, -1 0, -1 5)), ((0 0, 1 0, 1 -1, 0 -1, 0 0)))"
    };

// Mail of volker, about another problem, but this specific example is causing two-point inner rings polygons which should be discarded
// (condition of num_points in detail/overlay/convert_ring.hpp)
static std::string ggl_list_20120221_volker[2] =
    {
        "MULTIPOLYGON(((1032 2130,1032 1764,2052 2712,1032 2130)),((3234 2580,2558 2690,3234 2532,3234 2580)),((2558 2690,2136 2790,2052 2712,2136 2760,2558 2690)))",
        "MULTIPOLYGON(((3232 2532.469945355191,2136 2790,1032 1764,1032 1458,1032 1212,2136 2328,3232 2220.196721311475,3232 1056,1031 1056,1031 2856,3232 2856,3232 2532.469945355191),(3232 2412.426229508197,2136 2646,3232 2412.426229508197)))"
    };

static std::string ggl_list_20140212_sybren[2] =
    {
        "MULTIPOLYGON(((0.494062 0.659354,0.471383 0.64654,0.446639 0.616561,0.47291 0.61171,0.495396 0.625263,0.494964 0.679709,0.494062 0.659354)))",
        "MULTIPOLYGON(((0.4951091661995328 0.6614133543986973,0.495396 0.625263,0.50092 0.6492750000000001,0.494964 0.679709,0.477258 0.698703,0.4951091661995328 0.6614133543986973)),((0.452167 0.706562,0.433379 0.696888,0.442673 0.65792,0.464729 0.671387,0.452167 0.706562)))"
    };

static std::string mail_2019_01_21_johan[4] =
    {
        // Contains a, b, both a and b (should have been merged), c (clip)
        "MULTIPOLYGON(((1.2036811113357544 0.7535473108291626,1.1699721813201904 0.7535473108291626,1.1699721813201904 0.7663263082504272,1.2033243179321289 0.7672826647758484,1.2036811113357544 0.7535473108291626)))",
        "MULTIPOLYGON(((1.2036811113357544 0.7535473108291626,1.2038091421127319 0.7486215233802795,1.1713759899139404 0.7495520114898682,1.1713759899139404 0.7535472512245178,1.2036811113357544 0.7535473108291626)))",
        "MULTIPOLYGON(((1.2036811113357544 0.7535473108291626,1.1699721813201904 0.7535473108291626,1.1699721813201904 0.7663263082504272,1.2033243179321289 0.7672826647758484,1.2036811113357544 0.7535473108291626)),"
                     "((1.2036811113357544 0.7535473108291626,1.2038091421127319 0.7486215233802795,1.1713759899139404 0.7495520114898682,1.1713759899139404 0.7535472512245178,1.2036811113357544 0.7535473108291626)))",
        "MULTIPOLYGON(((0 0,2 0,2 2,0 2,0 0)))"
    };

static std::string ticket_9081[2] =
    {
        "MULTIPOLYGON(((0.5489109414010371 0.5774835110050927,0.4099611282054447 0.4644351568071598,0.4294011278595494 0.4843224236729239,0.4205359995313906 0.5115225580860201,0.4441572412013468 0.5184999851878852,0.5489109414010371 0.5774835110050927)),((0.562085028126843 0.5882018328808966,0.5644349663154944 0.591180348361206,0.568218114394707 0.5970364466647042,0.5838690879677763 0.6212632646137447,0.5873787029417971 0.6412877041753083,0.468699602592386 0.5866280231830688,0.4171010902425981 0.5220616039851281,0.4059124592966251 0.5563907478354578,0.3909547828925878 0.6022841397455458,0.520859401226844 0.9508041627246925,0.8595233008819849 0.8301950132755517,0.562085028126843 0.5882018328808966)))",
        "MULTIPOLYGON(((0.2099392122251989 0.492066865490789,0.1124301889095737 0.5124668111209448,0.3306914939102383 0.6126684490171914,0.2099392122251989 0.492066865490789)),((0.5885369465145437 0.6478961722242873,0.5342320718598281 0.6686303269145104,0.5619623880692838 0.7033299168703926,0.5945761233023867 0.6823532655194001,0.5885369465145437 0.6478961722242873)),((0.5570738195183501 0.6001870087680015,0.5429714753344335 0.6231021858940831,0.5880357506342242 0.6450365518134291,0.5838690879677763 0.6212632646137447,0.568218114394707 0.5970364466647042,0.5570738195183501 0.6001870087680015)),((0.5498478321815098 0.5029279381860542,0.608691671498764 0.5163121433149205,0.5636607291345047 0.5894838094559455,0.8595233008819849 0.8301950132755517,0.8285440738598029 0.8412277162756114,0.9591357158116398 0.9011810663167211,0.8572649311807611 0.3566393017365032,0.5965816668471951 0.4111770689940296,0.5498478321815098 0.5029279381860542)),((0.3984249865018206 0.4526335964808558,0.3621206996557855 0.4602288471829723,0.4183516736935784 0.4730187483833363,0.4099611282054451 0.4644351568071601,0.3984249865018206 0.4526335964808558)))"
    };

// Integer, ccw, open, reported by Volker
static std::string ticket_9942[2] =
    {
        "MULTIPOLYGON(((2058 1761,1996 1700,1660 1370,1324 1040,982 1148,881 981,2644 981,2338 1982)),((1996 1760,2338 2078,2674 1010,3010 1160,3254 2085,3352 2522,3427 2562,3688 2930,3688 2924,3688 2702,3439 2568,3352 2444,3218 1926,3010 998,2959 981,3773 981,3773 3702,732 3702,732 981,770 981,982 1310,1324 1148,1660 1442,1697 1472,1660 1436,1698 1473,1697 1472)))",
        "MULTIPOLYGON(((646 932,646 788,870 1136)),((3032 1096,3010 1040,2981 988,3010 998)),((3688 2702,3352 2522,3032 1096,3352 1916,3688 2018)),((2981 988,2674 884,2338 1982,1996 1712,1660 1442,1324 1148,982 1310,870 1136,982 1238,1324 1136,1660 1304,1996 1460,2338 1610,2674 434)))",
    };

// Simplified version showing the generated 'spike' which actually has an area because of the necessary rounding
static std::string ticket_9942a[2] =
    {
        "MULTIPOLYGON(((2058 1761,1996 1700,1660 1370,1324 1040,2644 981,2338 1982)))",
        "MULTIPOLYGON(((2674 884,2338 1982,1996 1712,1660 1442,1324 1148,1324 1136,1660 1304,1996 1460,2338 1610,2674 434)))",
    };

// Integer, ccw, open
static std::string ticket_10661[3] =
    {
        /* A */ "MULTIPOLYGON(((1701 985,3501 985,3501 2785,1701 2785,1701 985)))",
        /* B */ "MULTIPOLYGON(((1698 1860,1698 1122,2598 1392,3492 1842,3492 32706,2598 2340,1698 1860)))",
        /* C=A-B, */
        /* D */ "MULTIPOLYGON(((1698 2772,1698 1860,2598 2340,3492 2412,3492 32743,1698 2772)))"
        // Reported problem was: validity of difference C-D
    };

// Integer, ccw, open
static std::string ticket_10803[2] =
    {
        "MULTIPOLYGON(((3174 1374,3174 2886,1374 2886,1374 2139,3174 1374)))",
        "MULTIPOLYGON(((1374 1092,1734 1092,3174 2526,3174 2886,1374 2886,1374 1092)))"
    };

// Intersection was reported as invalid
static std::string ticket_11018[2] =
    {
        "MULTIPOLYGON(((66.32861177 -32.36106001,66.32805654 -32.36294657,66.32805697 -32.36294666,66.2558847 -32.60687137,66.18259719 -32.85172659,66.18259756 -32.85172667,66.10923256 -33.09561483,66.10923294 -33.09561491,66.03574675 -33.34044236,65.96214887 -33.58529255,65.96214918 -33.58529261,65.94391227 -33.64484979,65.839119 -33.032507,66.32861177 -32.36106001)),((67.77272059 -31.62519092,67.77272315 -31.62518165,67.77272398 -31.62517863,67.84057793 -31.37975346,67.90737994 -31.13429944,67.92189506 -31.0817063,68.151987 -31.013051,69.236142 -30.678138,70.320296 -31.013051,71.411955 -31.338781,72.016906 -32.187175,72.633163 -33.032507,72.464921 -34.015608,72.292755 -34.998316,71.385165 -35.656005,70.46265 -36.306671,69.236142 -36.312964,68.009631 -36.306671,67.087119 -35.656005,66.69381557 -35.37099587,66.7156576 -35.29873723,66.78843343 -35.05443368,66.86117133 -34.80910208,66.86117388 -34.80909338,66.86117494 -34.8090898,66.93281945 -34.5647568,67.00441882 -34.31942369,67.00441834 -34.31942359,67.07592335 -34.07510168,67.14737287 -33.83073849,67.21777858 -33.58537517,67.28811831 -33.34001123,67.35838201 -33.09560549,67.42858934 -32.85025274,67.49773049 -32.6058299,67.56682212 -32.36043528,67.63585135 -32.11504002,67.63585413 -32.11503015,67.63585478 -32.1150278,67.70382724 -31.86961401,67.77272059 -31.62519092)))",
        "MULTIPOLYGON(((67.840578 -31.37975347,67.77272059 -31.62519092,66.61484414 -31.38427789,66.68556685 -31.13932219,67.840578 -31.37975347)))",
    };


// Integer, ccw, open
static std::string ticket_11674[2] =
    {
        "MULTIPOLYGON(((529 3217,529 998,5337 998,5337 1834,5070 2000,5337 2072,5337 3475,529 3475,529 3312,1734 2054,2934 1670,3230 1690,2934 1400,1734 1784,529 3217),(4140 2582,5071 2001,4140 1754,3231 1691,4140 2582)))",
        "MULTIPOLYGON(((528 3218,528 2498,1734 1406,2556 1522,1734 1784,528 3218)),((4610 2288,5340 1178,5340 1832,4609 2289,4140 3002,2934 1574,2555 1521,2934 1400,4140 2582,4610 2288)))",
    };

// Union was reported as invalid because of no generated interior rings, fixed
static std::string ticket_11984[2] =
    {
        "MULTIPOLYGON(((-104.025 72.5541,-95 67,-94 64,-95 60,-81.5804 61.9881,-75 52,-73.1862 50.0729,-75 48,-69.2 42.6,-96 56,-102 51,-119 49,-128 41,-126 33,-99 29,-61.9276 35.8291,-46 21,-30 10,-45 8,-104 6,-119 -8,-89 -18,-29 -10,-20 -9,-16 -11,-15 -13,-27 -29,-9 -13,-7 -14,-13.4737 -33.4211,-21 -34,-25 -36,-30 -40,-40 -49,-35 -50,-30 -51,-27 -53,-21 -52,-20.037 -52.642,-31.5314 -70.8413,-37.9815 -75.2274,-52.1689 -78.5148,-55 -78,-55 -79,-55 -79.1707,-112.692 -92.5385,-120 -94,-90.9143 -94.9695,-91 -95,-90 -99.5,-90 -103,-95 -114,-93 -116,-92 -117,-90 -119,-87.28 -119.68,-87 -120,-86.4444 -119.889,-82 -121,-80.821 -119.728,-77.6667 -119,-73 -119,-55 -114,-54 -114,-47 -114,-40 -121,-39.9327 -120.951,-39.3478 -121.609,-39 -123,-35.2182 -118.927,-24.536 -110.248,-22.875 -109.292,-22 -109,-22 -109,-16 -107,-9 -105,-7.83333 -104.833,-1.33333 -104.333,0.962934 -105.481,11.9682 -122.413,13 -129,20.1579 -120.053,22 -119,29.5 -115.25,30.2903 -114.968,39 -114,48 -112,47.156 -108.962,61 -117,54.9172 -103.176,65 -107,51.482 -95.3683,50.2426 -92.5515,53 -92,51.806 -91.0448,52 -91,49.1363 -88.9091,25.1429 -69.7143,22.733 -56.1314,24.9744 -49.3053,26.9174 -45.1157,27 -45,27.1481 -44.6543,33.4226 -34.0476,35.5919 -31.272,39 -29,39.4424 -26.3458,44.296 -20.136,46 -19,53 -14,53.3759 -13.4361,58.9297 -10.7874,115 13,110.286 13,114 14,112.414 14.7207,247.196 184.001,250 189,239.971 201.765,239 216,237.627 218.027,237.317 223.454,243 229)))",
        "MULTIPOLYGON(((-31 6,-51 220,-84 241,-120 249,-146 224,-67 56,-74 52,-95 60,-38 10,-38 9)))",
    };

static std::string ticket_12118[2] =
    {
        "MULTIPOLYGON(((13.08940410614013671875 -70.98416137695312500000,12.81384754180908203125 -67.55441284179687500000,12.60483169555664062500 -63.57923889160156250000,13.56438255310058593750 -54.91608428955078125000,13.80568027496337890625 -43.62073516845703125000,13.00057315826416015625 -33.85240554809570312500,9.29664993286132812500 -33.23409271240234375000,19.66869926452636718750 -14.42036247253417968750,-5.96064376831054687500 -17.19871711730957031250,-14.87041568756103515625 -6.99879980087280273438,-22.50806808471679687500 -27.92480468750000000000,-22.16161727905273437500 -45.15484619140625000000,-22.42436790466308593750 -54.01613616943359375000,-23.13828659057617187500 -59.28628540039062500000,-23.18314933776855468750 -68.01937866210937500000,-22.86939430236816406250 -72.78530883789062500000,-23.02970123291015625000 -72.76760864257812500000,-22.81921195983886718750 -73.54760742187500000000,-18.65677833557128906250 -73.25045776367187500000,3.16641521453857421875 -75.66014099121093750000,12.75282478332519531250 -76.71865844726562500000,13.08940410614013671875 -70.98416137695312500000)))",
        "MULTIPOLYGON(((3.16641521453857421875 -75.66014099121093750000,12.75282478332519531250 -76.71865844726562500000,12.95001888275146484375 -74.61856842041015625000,3.16641521453857421875 -75.66014099121093750000)),((-22.84768676757812500000 -78.42963409423828125000,-20.92837524414062500000 -78.22530364990234375000,3.16641521453857421875 -75.66014099121093750000,-23.02970123291015625000 -72.76760864257812500000,-22.84768676757812500000 -78.42963409423828125000)))",
    };

static std::string ticket_12125[2] =
    {
        "MULTIPOLYGON(((-5.96064376831054687500 -17.19871711730957031250,7.83307075500488281250 -32.98977279663085937500,8.81292819976806640625 -34.11151504516601562500,19.66869926452636718750 -14.42036247253417968750,-5.96064376831054687500 -17.19871711730957031250)),((-14.87041568756103515625 -6.99879980087280273438,-16.12161636352539062500 -18.30021858215332031250,-5.96064376831054687500 -17.19871711730957031250,-14.87041568756103515625 -6.99879980087280273438)))",
        "MULTIPOLYGON(((7.83307075500488281250 -32.98977279663085937500,8.81292819976806640625 -34.11151504516601562500,13.00057315826416015625 -33.85240554809570312500,7.83307075500488281250 -32.98977279663085937500)),((-22.50806808471679687500 -27.92480468750000000000,7.83307075500488281250 -32.98977279663085937500,-14.87041568756103515625 -6.99879980087280273438,-22.50806808471679687500 -27.92480468750000000000)))",
    };

static std::string ticket_12751[4] =
    {
        /* a */     "MULTIPOLYGON(((1920 1462,3720 1462,3720 3262,1920 3262,1920 1462)))",
        /* b */     "MULTIPOLYGON(((1918 1957,1918 1657,2218 2189,1918 1957)),((3718 1957,3360 2233,3718 1561,3718 1957)),((3360 2233,2818 3253,2218 2189,2818 2653,3360 2233)))",
        /* c=a-b */ "MULTIPOLYGON(((1920 1660,1920 1462,3720 1462,3720 3262,1920 3262,1920 1959,2218 2189,1920 1660),(3718 1561,3360 2233,3718 1957,3718 1561),(2818 2653,2218 2189,2818 3253,3360 2233,2818 2653)))",
        /* d */     "MULTIPOLYGON(((1918 2155,1918 1957,2818 2653,3718 1957,3718 2154,2818 3055,1918 2155)))",
    };

static std::string ticket_12752[2] =
    {
        "MULTIPOLYGON(((3232 2413,2136 2646,3232 2412,3232 2413)),((3232 2532,3232 2856,1031 2856,1031 1056,3232 1056,3232 2221,2136 2328,1032 1212,1032 1458,1032 1764,2136 2790,3232 2532)))",
        "MULTIPOLYGON(((1032 2130,1032 1764,2052 2712,1032 2130)),((3234 2580,2558 2690,3234 2532,3234 2580)),((2558 2690,2136 2790,2052 2712,2136 2760,2558 2690)))"
    };


// Ticket for validity, input is CCW
static std::string ticket_12503[2] =
    {
        "MULTIPOLYGON (((15 17, 12 23, 15 20, 15 17)), ((35 23, 34 23, 34 24, 35 25, 36 25, 35 23)), ((8 6, 7 24, 10 25, 12 25, 12 23, 11 23, 10 13, 15 15, 8 6)), ((12 27, 8 31, 6 32, 6 38, 13 34, 13 31, 11 31, 12 30, 11 30, 12 29, 12 27)), ((7 24, 7 26, 6 31, 9 26, 7 24)), ((18 44, 15 45, 15 48, 18 44)), ((26 34, 18 44, 38 39, 26 34)), ((15 33, 13 34, 15 45, 15 33)), ((15 32, 15 33, 17 32, 15 32)), ((19 30, 17 32, 18 32, 16 38, 21 31, 19 30)), ((15 29, 13 30, 13 31, 15 32, 15 29)), ((15 28, 15 29, 17 29, 15 28)), ((14 27, 12 29, 13 30, 15 28, 14 27)), ((30 24, 25 24, 24 26, 25 27, 24 27, 23 28, 19 28, 19 29, 17 29, 18 30, 19 30, 22 29, 21 31, 26 34, 31 27, 28 30, 26 27, 30 24)), ((15 26, 15 28, 17 26, 15 26)), ((27 27, 31 27, 34 27, 32 26, 27 27)), ((19 25, 17 26, 19 26, 19 25)), ((41 15, 33 18, 35 23, 41 15)), ((23 24, 20 25, 19 26, 24 27, 24 26, 23 24)), ((33 15, 46 5, 48 4, 49 1, 32 13, 33 15)), ((32 23, 31 24, 32 25, 32 26, 33 25, 32 23)), ((35 23, 43 23, 44 23, 44 22, 43 22, 42 15, 35 23)), ((43 23, 36 25, 38 31, 35 25, 33 25, 34 27, 39 34, 40 39, 38 39, 44 42, 43 23)), ((48 22, 44 23, 48 46, 48 22)), ((18 11, 23 2, 15 3, 18 11)), ((29 17, 26 20, 22 23, 23 24, 25 24, 27 21, 28 20, 30 19, 29 17)), ((22 19, 21 20, 21 21, 24 19, 22 19)), ((31 23, 34 23, 31 19, 30 19, 31 22, 27 21, 30 24, 31 24, 31 23)), ((21 18, 20 21, 21 20, 21 18)), ((14 27, 15 26, 16 25, 19 25, 20 24, 22 23, 21 21, 20 21, 17 17, 15 20, 15 25, 12 25, 12 26, 14 26, 14 27), (20 21, 20 22, 17 24, 20 21)), ((22 17, 22 19, 23 18, 22 17)), ((30 15, 32 13, 31 10, 28 13, 30 15)), ((16 16, 17 17, 18 17, 16 16)), ((15 15, 15 17, 16 16, 15 15)), ((30 15, 29 16, 29 17, 30 17, 30 15)), ((33 15, 30 17, 31 19, 33 18, 33 15)), ((43 14, 44 22, 48 22, 48 4, 47 7, 42 13, 43 14)), ((43 14, 42 14, 41 15, 42 15, 43 14)), ((27 10, 25 6, 23 13, 27 11, 49 1, 24 2, 27 10)), ((28 13, 23 18, 24 19, 29 16, 28 13)), ((15 11, 15 15, 16 15, 17 13, 15 11)), ((18 11, 17 13, 19 15, 18 17, 21 18, 21 17, 20 15, 22 17, 23 13, 20 14, 18 11)), ((8 6, 8 5, 15 11, 15 3, 5 3, 8 6)))",
        "MULTIPOLYGON(((13 18,18 18,18 23,13 23,13 18)))"
    };

static std::string issue_630_a[2] =
{
    "MULTIPOLYGON(((-0.3 -0.1475,-0.3 +0.1475,+0.3 +0.1475,+0.3 -0.1475,-0.3 -0.1475)))",
    "MULTIPOLYGON(((-0.605 +0.1575,+0.254777333596 +1.0172773336,+1.53436796127 -0.262313294074,+0.674590627671 -1.12209062767,-0.605 +0.1575)))"
};

static std::string issue_630_b[2] =
{
    "MULTIPOLYGON(((-0.3 -0.1475,-0.3 +0.1475,+0.3 +0.1475,+0.3 -0.1475,-0.3 -0.1475)))",
    "MULTIPOLYGON(((-1.215 +0.7675000000000001,-0.4962799075873666 +1.486220092412633,+0.665763075292561 +0.324177109532706,-0.05295701712007228 -0.3945429828799273,-1.215 +0.7675000000000001)))"
};

static std::string issue_630_c[2] =
{
    "MULTIPOLYGON(((-0.3 -0.1475,-0.3 +0.1475,+0.3 +0.1475,+0.3 -0.1475,-0.3 -0.1475)))",
    "MULTIPOLYGON(((-0.9099999999999999 +0.4625,-0.1912799075873667 +1.181220092412633,+0.9707630752925609 +0.01917710953270602,+0.2520429828799277 -0.6995429828799273,-0.9099999999999999 +0.4625)))"
};

static std::string issue_643[2] =
{
    "MULTIPOLYGON(((-0.420825839394717959862646239344 0.168094877926996288941552393226,5.29161113734201116187705338234 5.76881481261494233336861725547,12.2925110557019436896553088445 -1.37173140830596795858298264648,6.58007407896521545609402892296 -6.97245134299391278176472042105,-0.420825839394717959862646239344 0.168094877926996288941552393226)))",
    "MULTIPOLYGON(((4.75985680877701196159479187986e-16 -0.26112514220036736611874061964,0 0,0.72337592336357892097709054724 0.600648602980154100450249643472,1.223386680467822174023240222 1.19163154396839887638748223253,1.72339743757206620422550713556 2.1342216197599452875977021904,1.72339742847305732453833115869 -2.01889900623749607433410346857,4.75985680877701196159479187986e-16 -0.26112514220036736611874061964)))"
};

// For difference
static std::string issue_869_a[2] =
{
    "MULTIPOLYGON(((70 50,70 10,10 10,10 70,50 70,50 90,70 90,70 70,90 70,90 50,70 50)))",
    "MULTIPOLYGON(((50 50,50 70,70 70,70 50,50 50)),((50 50,50 30,30 30,30 50,50 50)))"
};

// For union
static std::string issue_869_b[2] =
{
    "MULTIPOLYGON(((10 10,10 70,50 70,50 50,70 50,70 10,10 10),(30 30,50 30,50 50,30 50,30 30)))",
    "MULTIPOLYGON(((50 70,50 90,70 90,70 70,50 70)),((70 70,90 70,90 50,70 50,70 70)))"
};

// For intersection
static std::string issue_869_c[2] =
{
    "MULTIPOLYGON(((10 10,10 90,90 90,90 10,10 10),(30 30,50 30,50 50,30 50,30 30),(50 50,70 50,70 70,50 70,50 50)))",
    "MULTIPOLYGON(((10 10,10 70,50 70,50 90,70 90,70 70,90 70,90 50,70 50,70 10,10 10)))"
};

// For difference
static std::string issue_888_34[2] =
{
    "MULTIPOLYGON(((0.450246 0.176853,0.476025 0.144909,0.468192 0.133251,0.448726 0.150373,0.450246 0.176853)),((0.719425 0.179933,0.778422 0.274563,0.76895 0.322197,0.791284 0.312445,0.791944 0.296253,0.799738 0.308754,0.959288 0.239088,0.958302 0.22776,0.745381 0.158712,0.719425 0.179933)),((0.701609 0.660841,0.666999 0.75257,0.681007 0.764449,0.701609 0.660841)),((0.594132 0.714429,0.606487 0.701251,0.594811 0.691349,0.594132 0.714429)),((0.224866 0.786223,0.382462 0.983231,0.466763 0.85994,0.47812 0.838165,0.39515 0.926659,0.476824 0.77908,0.50332 0.789847,0.525393 0.747525,0.505459 0.72734,0.547556 0.651273,0.482633 0.596213,0.577247 0.52647,0.570444 0.520834,0.600941 0.483052,0.603549 0.394417,0.517116 0.432156,0.563288 0.385417,0.492528 0.36544,0.577515 0.295956,0.550593 0.255888,0.452782 0.221035,0.459249 0.333709,0.441865 0.351136,0.370548 0.331001,0.385816 0.296593,0.394882 0.293488,0.387958 0.291767,0.423911 0.210747,0.392721 0.199633,0.310001 0.272391,0.236386 0.254095,0.304173 0.168082,0.296274 0.165267,0.207506 0.246917,0.0140798 0.198842,0.069792 0.24609,0.0159663 0.230894,0.0815688 0.362756,0.0156364 0.423401,0.0977468 0.395274,0.108341 0.416568,0.135246 0.382428,0.203132 0.359173,0.207309 0.362716,0.174164 0.39187,0.173365 0.391061,0.173401 0.392541,0.11983 0.439661,0.177364 0.555306,0.177972 0.580239,0.0906064 0.618386,0.127028 0.663916,0.0564116 0.721999,0.127196 0.664127,0.181673 0.732227,0.182236 0.755361,0.188638 0.740934,0.199131 0.754051,0.261029 0.691392,0.320648 0.715618,0.224866 0.786223),(0.216572 0.354569,0.27427 0.30382,0.323855 0.317819,0.216572 0.354569)),((0.224866 0.786223,0.199131 0.754051,0.013265 0.942202,0.224866 0.786223)),((0.598711 0.558841,0.547556 0.651273,0.566996 0.667759,0.59721 0.609827,0.598711 0.558841)),((0.598711 0.558841,0.604247 0.548838,0.59913 0.544599,0.598711 0.558841)),((0.604247 0.548838,0.621545 0.563169,0.680308 0.450499,0.643761 0.47744,0.604247 0.548838)),((0.643761 0.47744,0.668916 0.431986,0.656761 0.413896,0.600941 0.483052,0.60016 0.509579,0.643761 0.47744)))",
    "MULTIPOLYGON(((0.663637 0.851798,0.60596 0.926301,0.651077 0.91496,0.663637 0.851798)),((0.696266 0.142785,0.611653 0.00706631,0.573697 0.0404517,0.625869 0.119956,0.696266 0.142785)),((0.315687 0.454629,0.297299 0.496067,0.328117 0.465171,0.315687 0.454629)),((0.211225 0.286021,0.236386 0.254095,0.207506 0.246917,0.175851 0.276034,0.211225 0.286021)),((0.211225 0.286021,0.173427 0.333981,0.203132 0.359173,0.216572 0.354569,0.207309 0.362716,0.315687 0.454629,0.370548 0.331001,0.323855 0.317819,0.385816 0.296593,0.387958 0.291767,0.310001 0.272391,0.27427 0.30382,0.211225 0.286021)),((0.175851 0.276034,0.069792 0.24609,0.14191 0.307253,0.175851 0.276034)),((0.14191 0.307253,0.0815688 0.362756,0.0977468 0.395274,0.135246 0.382428,0.173427 0.333981,0.14191 0.307253)),((0.684151 0.208773,0.603476 0.274731,0.607034 0.275999,0.605233 0.337207,0.634572 0.380871,0.710003 0.347935,0.727272 0.326541,0.717162 0.344809,0.76089 0.325716,0.684151 0.208773)),((0.450246 0.176853,0.425156 0.207942,0.423911 0.210747,0.452782 0.221035,0.450246 0.176853)),((0.527613 0.0809854,0.521709 0.086179,0.523067 0.0866194,0.527613 0.0809854)))"
};

static std::string issue_888_37[2] =
{
    "MULTIPOLYGON(((0.573697 0.0404517,0.5677 0.0313133,0.527613 0.0809854,0.573697 0.0404517)),((0.663637 0.851798,0.711342 0.790176,0.681007 0.764449,0.663637 0.851798)),((0.328117 0.465171,0.348942 0.482832,0.492528 0.36544,0.441865 0.351136,0.328117 0.465171)),((0.59913 0.544599,0.60016 0.509579,0.577247 0.52647,0.59913 0.544599)),((0.780115 0.586441,0.779691 0.59685,0.748247 0.550053,0.740154 0.558685,0.710619 0.636962,0.961172 0.844534,0.780115 0.586441)),((0.791944 0.296253,0.796543 0.183435,0.778422 0.274563,0.791944 0.296253)),((0.76089 0.325716,0.766538 0.334324,0.76895 0.322197,0.76089 0.325716)),((0.36551 0.585627,0.420902 0.641717,0.482633 0.596213,0.413212 0.537338,0.36551 0.585627)),((0.36551 0.585627,0.305289 0.524648,0.289209 0.531669,0.301942 0.521258,0.291026 0.510204,0.297299 0.496067,0.287131 0.506261,0.174164 0.39187,0.173401 0.392541,0.177364 0.555306,0.187664 0.576007,0.177972 0.580239,0.178814 0.61485,0.142867 0.650888,0.127028 0.663916,0.127196 0.664127,0.178983 0.621787,0.181673 0.732227,0.188638 0.740934,0.226862 0.654796,0.241025 0.683264,0.261029 0.691392,0.36551 0.585627),(0.279648 0.535844,0.277112 0.541559,0.201344 0.603505,0.197613 0.596005,0.240627 0.552881,0.279648 0.535844)),((0.413212 0.537338,0.517116 0.432156,0.366654 0.497853,0.413212 0.537338)),((0.590866 0.825421,0.591197 0.814158,0.544737 0.767113,0.517876 0.795762,0.590866 0.825421)),((0.590866 0.825421,0.586751 0.965255,0.666999 0.75257,0.606487 0.701251,0.685935 0.616513,0.621545 0.563169,0.59721 0.609827,0.594811 0.691349,0.566996 0.667759,0.525393 0.747525,0.544737 0.767113,0.594132 0.714429,0.591197 0.814158,0.609997 0.833195,0.590866 0.825421)),((0.420902 0.641717,0.320648 0.715618,0.476824 0.77908,0.505459 0.72734,0.420902 0.641717)),((0.685935 0.616513,0.70696 0.633931,0.717016 0.583363,0.685935 0.616513)))",
    "MULTIPOLYGON(((0.594132 0.714429,0.606487 0.701251,0.594811 0.691349,0.594132 0.714429)),((0.663637 0.851798,0.60596 0.926301,0.651077 0.91496,0.663637 0.851798)),((0.550593 0.255888,0.603476 0.274731,0.684151 0.208773,0.625869 0.119956,0.523067 0.0866194,0.476025 0.144909,0.550593 0.255888)),((0.550593 0.255888,0.452782 0.221035,0.459249 0.333709,0.441865 0.351136,0.370548 0.331001,0.315687 0.454629,0.207309 0.362716,0.174164 0.39187,0.173365 0.391061,0.173401 0.392541,0.11983 0.439661,0.177364 0.555306,0.177972 0.580239,0.0906064 0.618386,0.127028 0.663916,0.0564116 0.721999,0.127196 0.664127,0.181673 0.732227,0.182236 0.755361,0.188638 0.740934,0.199131 0.754051,0.261029 0.691392,0.320648 0.715618,0.224866 0.786223,0.382462 0.983231,0.466763 0.85994,0.47812 0.838165,0.39515 0.926659,0.476824 0.77908,0.50332 0.789847,0.525393 0.747525,0.505459 0.72734,0.547556 0.651273,0.482633 0.596213,0.577247 0.52647,0.570444 0.520834,0.600941 0.483052,0.603549 0.394417,0.517116 0.432156,0.563288 0.385417,0.492528 0.36544,0.577515 0.295956,0.550593 0.255888),(0.366654 0.497853,0.305289 0.524648,0.301942 0.521258,0.348942 0.482832,0.366654 0.497853),(0.287131 0.506261,0.291026 0.510204,0.279648 0.535844,0.289209 0.531669,0.277112 0.541559,0.226862 0.654796,0.201344 0.603505,0.178983 0.621787,0.178814 0.61485,0.197613 0.596005,0.187664 0.576007,0.240627 0.552881,0.287131 0.506261),(0.315687 0.454629,0.328117 0.465171,0.297299 0.496067,0.315687 0.454629)),((0.50332 0.789847,0.47812 0.838165,0.517876 0.795762,0.50332 0.789847)),((0.476025 0.144909,0.468192 0.133251,0.448726 0.150373,0.450246 0.176853,0.476025 0.144909)),((0.521709 0.086179,0.399861 0.0466657,0.304173 0.168082,0.392721 0.199633,0.448726 0.150373,0.445833 0.0999749,0.468192 0.133251,0.521709 0.086179)),((0.521709 0.086179,0.523067 0.0866194,0.527613 0.0809854,0.521709 0.086179)),((0.577515 0.295956,0.605233 0.337207,0.607034 0.275999,0.603476 0.274731,0.577515 0.295956)),((0.304173 0.168082,0.296274 0.165267,0.207506 0.246917,0.236386 0.254095,0.304173 0.168082)),((0.392721 0.199633,0.310001 0.272391,0.387958 0.291767,0.423911 0.210747,0.392721 0.199633)),((0.108341 0.416568,0.0230624 0.524775,0.11983 0.439661,0.108341 0.416568)),((0.108341 0.416568,0.135246 0.382428,0.0977468 0.395274,0.108341 0.416568)),((0.668916 0.431986,0.729259 0.521793,0.766538 0.334324,0.788997 0.368549,0.791284 0.312445,0.76895 0.322197,0.778422 0.274563,0.719425 0.179933,0.684151 0.208773,0.76089 0.325716,0.717162 0.344809,0.668916 0.431986)),((0.668916 0.431986,0.656761 0.413896,0.600941 0.483052,0.60016 0.509579,0.643761 0.47744,0.668916 0.431986)),((0.603549 0.394417,0.634572 0.380871,0.605233 0.337207,0.603549 0.394417)),((0.625869 0.119956,0.696266 0.142785,0.611653 0.00706631,0.573697 0.0404517,0.625869 0.119956)),((0.656761 0.413896,0.710003 0.347935,0.634572 0.380871,0.656761 0.413896)),((0.696266 0.142785,0.719425 0.179933,0.745381 0.158712,0.696266 0.142785)),((0.701609 0.660841,0.710619 0.636962,0.70696 0.633931,0.701609 0.660841)),((0.701609 0.660841,0.666999 0.75257,0.681007 0.764449,0.701609 0.660841)),((0.788997 0.368549,0.783115 0.512864,0.842285 0.449754,0.788997 0.368549)),((0.959288 0.239088,0.799738 0.308754,0.992212 0.617482,0.959288 0.239088)),((0.959288 0.239088,0.973756 0.232771,0.958302 0.22776,0.959288 0.239088)),((0.717162 0.344809,0.727272 0.326541,0.710003 0.347935,0.717162 0.344809)),((0.783115 0.512864,0.751874 0.546184,0.780115 0.586441,0.783115 0.512864)),((0.745381 0.158712,0.958302 0.22776,0.938553 0.000778765,0.745381 0.158712)),((0.799738 0.308754,0.791944 0.296253,0.791284 0.312445,0.799738 0.308754)),((0.74515 0.545443,0.729259 0.521793,0.717016 0.583363,0.740154 0.558685,0.74515 0.545443)),((0.74515 0.545443,0.748247 0.550053,0.751874 0.546184,0.74732 0.539692,0.74515 0.545443)),((0.310001 0.272391,0.236386 0.254095,0.211225 0.286021,0.27427 0.30382,0.310001 0.272391)),((0.323855 0.317819,0.370548 0.331001,0.385816 0.296593,0.323855 0.317819)),((0.323855 0.317819,0.27427 0.30382,0.216572 0.354569,0.323855 0.317819)),((0.387958 0.291767,0.385816 0.296593,0.394882 0.293488,0.387958 0.291767)),((0.175851 0.276034,0.14191 0.307253,0.173427 0.333981,0.211225 0.286021,0.175851 0.276034)),((0.175851 0.276034,0.207506 0.246917,0.0140798 0.198842,0.069792 0.24609,0.175851 0.276034)),((0.173427 0.333981,0.135246 0.382428,0.203132 0.359173,0.173427 0.333981)),((0.203132 0.359173,0.207309 0.362716,0.216572 0.354569,0.203132 0.359173)),((0.069792 0.24609,0.0159663 0.230894,0.0815688 0.362756,0.14191 0.307253,0.069792 0.24609)),((0.0815688 0.362756,0.0156364 0.423401,0.0977468 0.395274,0.0815688 0.362756)),((0.224866 0.786223,0.199131 0.754051,0.013265 0.942202,0.224866 0.786223)),((0.643761 0.47744,0.604247 0.548838,0.621545 0.563169,0.680308 0.450499,0.643761 0.47744)),((0.604247 0.548838,0.59913 0.544599,0.598711 0.558841,0.604247 0.548838)),((0.452782 0.221035,0.450246 0.176853,0.425156 0.207942,0.423911 0.210747,0.452782 0.221035)),((0.547556 0.651273,0.566996 0.667759,0.59721 0.609827,0.598711 0.558841,0.547556 0.651273)))"
};

static std::string issue_888_53[2] =
{
    "MULTIPOLYGON(((0.101361 0.873249,0.301118 0.633252,0.285957 0.592644,0.135028 0.636275,0.344319 0.39052,0.0807768 0.389475,0.101361 0.873249)),((0.101361 0.873249,0.0361477 0.951599,0.103672 0.927553,0.101361 0.873249)),((0.301118 0.633252,0.315174 0.670899,0.33042 0.651962,0.330739 0.654302,0.315658 0.672196,0.323461 0.693095,0.334835 0.684373,0.341221 0.73125,0.338625 0.733712,0.34324 0.746072,0.344189 0.75304,0.324735 0.787732,0.287708 0.782026,0.152761 0.910072,0.226024 0.883982,0.215588 0.892864,0.277228 0.872453,0.241092 0.936893,0.327022 0.855964,0.37797 0.839093,0.425729 0.967012,0.529874 0.788791,0.558286 0.779383,0.556701 0.766224,0.65607 0.730838,0.692351 0.704562,0.697876 0.71595,0.702942 0.714146,0.707042 0.706142,0.711407 0.711132,0.799272 0.679842,0.779237 0.641637,0.833376 0.602428,0.780122 0.563447,0.810992 0.50317,0.757461 0.431674,0.767273 0.423323,0.781644 0.335916,0.749286 0.355358,0.709115 0.319338,0.732915 0.280433,0.713883 0.297463,0.704049 0.313171,0.706561 0.317049,0.703397 0.314212,0.686139 0.341775,0.674939 0.332312,0.670981 0.335853,0.658385 0.328037,0.657071 0.317215,0.623065 0.288482,0.644713 0.261593,0.628117 0.246712,0.6106 0.27795,0.602526 0.271128,0.590314 0.2858,0.571143 0.273905,0.578518 0.250844,0.426472 0.122377,0.364986 0.0297914,0.445637 0.196032,0.26697 0.0851721,0.292758 0.232593,0.387037 0.34036,0.344319 0.39052,0.424949 0.39084,0.282042 0.582159,0.285957 0.592644,0.320962 0.582524,0.324109 0.60563,0.301118 0.633252),(0.526427 0.362556,0.553002 0.330628,0.536289 0.382885,0.526427 0.362556),(0.510405 0.381806,0.511534 0.391183,0.502629 0.391148,0.510405 0.381806),(0.484614 0.50262,0.503361 0.485845,0.487873 0.534273,0.4628 0.541521,0.484614 0.50262),(0.632981 0.455744,0.625653 0.461363,0.632188 0.455163,0.632981 0.455744),(0.521031 0.470034,0.521942 0.477601,0.515424 0.475051,0.521031 0.470034),(0.664289 0.376674,0.665904 0.389973,0.664792 0.391791,0.654849 0.391752,0.664289 0.376674),(0.716114 0.391995,0.698833 0.391926,0.716805 0.374873,0.721777 0.371886,0.731559 0.380151,0.716114 0.391995),(0.476049 0.442104,0.497 0.445061,0.488199 0.455992,0.476049 0.442104),(0.417634 0.770625,0.48428 0.664179,0.524665 0.629807,0.541078 0.636494,0.54301 0.652544,0.417634 0.770625),(0.417634 0.770625,0.406699 0.78809,0.409979 0.777834,0.417634 0.770625),(0.562537 0.36366,0.602098 0.293112,0.613594 0.300245,0.562537 0.36366),(0.590531 0.494689,0.590473 0.49457,0.597976 0.482587,0.58827 0.49003,0.57156 0.455585,0.611365 0.461203,0.620502 0.446609,0.62793 0.452047,0.621457 0.462628,0.623608 0.462931,0.619207 0.466306,0.617496 0.469103,0.62395 0.462979,0.647385 0.466287,0.677877 0.488607,0.679066 0.498397,0.6847 0.493601,0.697077 0.50266,0.682592 0.527448,0.684251 0.541111,0.676403 0.53804,0.640975 0.598666,0.617678 0.550644,0.646318 0.526268,0.59637 0.506724,0.594746 0.503377,0.590761 0.504529,0.585522 0.502479,0.590205 0.494998,0.590531 0.494689),(0.544962 0.391316,0.54201 0.389156,0.562537 0.36366,0.547024 0.391324,0.608717 0.391569,0.580186 0.417099,0.546427 0.392389,0.543464 0.397674,0.561152 0.434132,0.541862 0.451393,0.518387 0.44808,0.517823 0.443399,0.515759 0.44708,0.51758 0.441384,0.515288 0.422347,0.529192 0.405077,0.533607 0.391271,0.540286 0.391297,0.540339 0.391232,0.540371 0.391298,0.544962 0.391316)),((0.152761 0.910072,0.103672 0.927553,0.104862 0.955521,0.152761 0.910072)),((0.590531 0.494689,0.594746 0.503377,0.596912 0.502751,0.617496 0.469103,0.590531 0.494689)),((0.645636 0.223015,0.645246 0.219802,0.644295 0.220945,0.645636 0.223015)),((0.645636 0.223015,0.649585 0.255543,0.644713 0.261593,0.651004 0.267233,0.657071 0.317215,0.674939 0.332312,0.699286 0.310525,0.703397 0.314212,0.704049 0.313171,0.701216 0.308798,0.713883 0.297463,0.756461 0.22946,0.79194 0.273302,0.808697 0.171384,0.794671 0.168431,0.81435 0.137,0.832141 0.0287975,0.730585 0.154935,0.462894 0.0985647,0.590598 0.213071,0.578518 0.250844,0.602526 0.271128,0.6251 0.244006,0.628117 0.246712,0.638996 0.227311,0.644295 0.220945,0.643368 0.219514,0.644883 0.216813,0.645246 0.219802,0.697673 0.156814,0.713413 0.176264,0.659074 0.243756,0.645636 0.223015)),((0.544962 0.391316,0.546427 0.392389,0.547024 0.391324,0.544962 0.391316)),((0.543464 0.397674,0.540371 0.391298,0.540286 0.391297,0.529192 0.405077,0.51758 0.441384,0.517823 0.443399,0.543464 0.397674)),((0.900875 0.327668,0.810992 0.50317,0.86705 0.57804,0.942565 0.52335,0.900875 0.327668)),((0.900875 0.327668,0.947768 0.236106,0.888903 0.271473,0.900875 0.327668)),((0.808697 0.171384,0.824352 0.174681,0.840911 0.300307,0.888903 0.271473,0.848599 0.0822989,0.81435 0.137,0.808697 0.171384)),((0.840911 0.300307,0.822665 0.31127,0.846188 0.340338,0.840911 0.300307)),((0.79194 0.273302,0.781644 0.335916,0.822665 0.31127,0.79194 0.273302)),((0.86705 0.57804,0.833376 0.602428,0.948285 0.686538,0.86705 0.57804)))",
    "MULTIPOLYGON(((0.659074 0.243756,0.649585 0.255543,0.651004 0.267233,0.699286 0.310525,0.701216 0.308798,0.659074 0.243756)),((0.65607 0.730838,0.614949 0.760619,0.558286 0.779383,0.569893 0.875763,0.926464 0.956953,0.711407 0.711132,0.702942 0.714146,0.699891 0.720104,0.697876 0.71595,0.65607 0.730838)),((0.6251 0.244006,0.638996 0.227311,0.643368 0.219514,0.606658 0.162855,0.590598 0.213071,0.6251 0.244006)),((0.338625 0.733712,0.323461 0.693095,0.220913 0.771732,0.287708 0.782026,0.338625 0.733712)),((0.315174 0.670899,0.28149 0.712736,0.315658 0.672196,0.315174 0.670899)),((0.625653 0.461363,0.623608 0.462931,0.62395 0.462979,0.625653 0.461363)),((0.794671 0.168431,0.730585 0.154935,0.713413 0.176264,0.756461 0.22946,0.794671 0.168431)),((0.597976 0.482587,0.619207 0.466306,0.621457 0.462628,0.611365 0.461203,0.597976 0.482587)),((0.679066 0.498397,0.646318 0.526268,0.676403 0.53804,0.682592 0.527448,0.679066 0.498397)),((0.402869 0.800065,0.401491 0.804374,0.540911 0.757512,0.402869 0.800065)),((0.402869 0.800065,0.406699 0.78809,0.398324 0.801466,0.402869 0.800065)),((0.719855 0.46368,0.757461 0.431674,0.746845 0.417494,0.719855 0.46368)),((0.719855 0.46368,0.696391 0.483651,0.701969 0.494288,0.719855 0.46368)),((0.746845 0.417494,0.77151 0.375285,0.749286 0.355358,0.736399 0.363101,0.750855 0.385413,0.725474 0.369665,0.721777 0.371886,0.701659 0.354888,0.691299 0.34846,0.665904 0.389973,0.666125 0.391796,0.698833 0.391926,0.669517 0.419742,0.670404 0.427047,0.716114 0.391995,0.727787 0.392041,0.746845 0.417494)),((0.708595 0.320188,0.709115 0.319338,0.706561 0.317049,0.708595 0.320188)),((0.708595 0.320188,0.692237 0.346927,0.701659 0.354888,0.725474 0.369665,0.736399 0.363101,0.708595 0.320188)),((0.473998 0.577658,0.465083 0.605533,0.471121 0.607993,0.530823 0.551344,0.52892 0.535542,0.473998 0.577658)),((0.473998 0.577658,0.487873 0.534273,0.527391 0.522848,0.524311 0.49727,0.509505 0.480347,0.503361 0.485845,0.506294 0.476676,0.501948 0.471709,0.484614 0.50262,0.430805 0.55077,0.4628 0.541521,0.434005 0.592872,0.447166 0.598234,0.473998 0.577658)),((0.509505 0.480347,0.515424 0.475051,0.507771 0.472056,0.506294 0.476676,0.509505 0.480347)),((0.502836 0.470125,0.507771 0.472056,0.515566 0.447682,0.515432 0.447663,0.502836 0.470125)),((0.502836 0.470125,0.499379 0.468772,0.501948 0.471709,0.502836 0.470125)),((0.502031 0.312271,0.526427 0.362556,0.518569 0.371997,0.535749 0.384573,0.533607 0.391271,0.511534 0.391183,0.515288 0.422347,0.497 0.445061,0.515432 0.447663,0.515759 0.44708,0.515566 0.447682,0.518387 0.44808,0.521031 0.470034,0.541862 0.451393,0.57156 0.455585,0.561152 0.434132,0.580186 0.417099,0.620502 0.446609,0.654849 0.391752,0.608717 0.391569,0.660475 0.345254,0.658385 0.328037,0.613594 0.300245,0.623065 0.288482,0.6106 0.27795,0.602098 0.293112,0.590314 0.2858,0.553002 0.330628,0.571143 0.273905,0.445637 0.196032,0.494788 0.297342,0.499477 0.291064,0.502031 0.312271),(0.537856 0.386115,0.535749 0.384573,0.536289 0.382885,0.537856 0.386115),(0.537856 0.386115,0.54201 0.389156,0.540339 0.391232,0.537856 0.386115)),((0.502031 0.312271,0.494788 0.297342,0.467142 0.334354,0.50832 0.364495,0.502031 0.312271)),((0.50832 0.364495,0.510405 0.381806,0.518569 0.371997,0.50832 0.364495)),((0.509635 0.623684,0.524665 0.629807,0.538821 0.617759,0.534711 0.583632,0.509635 0.623684)),((0.509635 0.623684,0.471121 0.607993,0.461323 0.617289,0.432133 0.708562,0.48428 0.664179,0.509635 0.623684)),((0.465083 0.605533,0.447166 0.598234,0.418801 0.619985,0.373678 0.700452,0.461323 0.617289,0.465083 0.605533)),((0.374931 0.830954,0.37797 0.839093,0.529874 0.788791,0.539477 0.772358,0.374931 0.830954)),((0.374931 0.830954,0.369376 0.816075,0.340592 0.843183,0.374931 0.830954)),((0.524311 0.49727,0.542793 0.518396,0.556414 0.514458,0.576592 0.498985,0.521942 0.477601,0.524311 0.49727)),((0.530823 0.551344,0.534711 0.583632,0.561889 0.540224,0.553113 0.530193,0.530823 0.551344)),((0.527391 0.522848,0.52892 0.535542,0.5462 0.522291,0.542793 0.518396,0.527391 0.522848)),((0.539477 0.772358,0.556701 0.766224,0.55437 0.746871,0.539477 0.772358)),((0.483053 0.462384,0.453094 0.450661,0.359867 0.562668,0.372228 0.567704,0.406154 0.557896,0.483053 0.462384)),((0.483053 0.462384,0.499379 0.468772,0.488199 0.455992,0.483053 0.462384)),((0.430805 0.55077,0.406154 0.557896,0.391829 0.575689,0.399476 0.578805,0.430805 0.55077)),((0.359867 0.562668,0.315812 0.54472,0.320962 0.582524,0.350431 0.574005,0.359867 0.562668)),((0.467142 0.334354,0.420948 0.300541,0.387037 0.34036,0.427828 0.386985,0.467142 0.334354)),((0.431222 0.390865,0.467936 0.43283,0.502629 0.391148,0.431222 0.390865)),((0.431222 0.390865,0.427828 0.386985,0.424949 0.39084,0.431222 0.390865)),((0.467936 0.43283,0.461881 0.440105,0.476049 0.442104,0.467936 0.43283)),((0.453094 0.450661,0.461881 0.440105,0.405934 0.432208,0.453094 0.450661)),((0.340592 0.843183,0.281925 0.864075,0.277228 0.872453,0.327022 0.855964,0.340592 0.843183)),((0.350431 0.574005,0.324109 0.60563,0.33042 0.651962,0.391829 0.575689,0.372228 0.567704,0.350431 0.574005)),((0.354048 0.77502,0.369376 0.816075,0.409979 0.777834,0.432133 0.708562,0.354048 0.77502)),((0.354048 0.77502,0.345181 0.751271,0.344189 0.75304,0.347896 0.780256,0.354048 0.77502)),((0.434005 0.592872,0.399476 0.578805,0.378706 0.59739,0.330739 0.654302,0.334835 0.684373,0.418801 0.619985,0.434005 0.592872)),((0.373678 0.700452,0.341221 0.73125,0.34324 0.746072,0.345181 0.751271,0.373678 0.700452)),((0.281925 0.864075,0.311609 0.81114,0.226024 0.883982,0.281925 0.864075)),((0.311609 0.81114,0.336908 0.789608,0.324735 0.787732,0.311609 0.81114)),((0.347896 0.780256,0.336908 0.789608,0.349433 0.791538,0.347896 0.780256)),((0.712142 0.513687,0.759426 0.603857,0.780122 0.563447,0.712142 0.513687)),((0.712142 0.513687,0.701969 0.494288,0.697077 0.50266,0.712142 0.513687)),((0.582796 0.506832,0.561889 0.540224,0.590925 0.573414,0.617678 0.550644,0.59637 0.506724,0.590761 0.504529,0.582796 0.506832)),((0.582796 0.506832,0.585522 0.502479,0.583256 0.501592,0.575516 0.508936,0.582796 0.506832)),((0.575516 0.508936,0.556414 0.514458,0.5462 0.522291,0.553113 0.530193,0.575516 0.508936)),((0.759426 0.603857,0.716989 0.686719,0.779237 0.641637,0.759426 0.603857)),((0.675652 0.470277,0.670404 0.427047,0.632981 0.455744,0.647385 0.466287,0.675652 0.470277)),((0.675652 0.470277,0.677877 0.488607,0.6847 0.493601,0.696391 0.483651,0.690475 0.472369,0.675652 0.470277)),((0.669517 0.419742,0.666125 0.391796,0.664792 0.391791,0.62793 0.452047,0.632188 0.455163,0.669517 0.419742)),((0.660475 0.345254,0.664289 0.376674,0.684569 0.344284,0.670981 0.335853,0.660475 0.345254)),((0.576592 0.498985,0.583256 0.501592,0.590205 0.494998,0.590473 0.49457,0.58827 0.49003,0.576592 0.498985)),((0.692237 0.346927,0.686139 0.341775,0.684569 0.344284,0.691299 0.34846,0.692237 0.346927)),((0.675782 0.67041,0.640975 0.598666,0.629779 0.617826,0.675782 0.67041)),((0.675782 0.67041,0.692351 0.704562,0.700498 0.698662,0.675782 0.67041)),((0.629779 0.617826,0.590925 0.573414,0.538821 0.617759,0.541078 0.636494,0.554323 0.64189,0.54301 0.652544,0.55437 0.746871,0.629779 0.617826)),((0.716989 0.686719,0.700498 0.698662,0.707042 0.706142,0.716989 0.686719)))"
};

static std::string bug_21155501[2] =
    {
        "MULTIPOLYGON(((-8.3935546875 27.449790329784214,4.9658203125 18.729501999072138,11.8212890625 23.563987128451217,9.7119140625 25.48295117535531,9.8876953125 31.728167146023935,8.3056640625 32.99023555965106,8.5693359375 37.16031654673677,-1.8896484375 35.60371874069731,-0.5712890625 32.02670629333614,-8.9208984375 29.458731185355344,-8.3935546875 27.449790329784214)))",
        "MULTIPOLYGON(((4.9658203125 18.729501999072138,-3.4868710311820115 24.246968623627644,8.3589904332912 33.833614418115445,8.3056640625 32.99023555965106,9.8876953125 31.728167146023935,9.7119140625 25.48295117535531,11.8212890625 23.563987128451217,4.9658203125 18.729501999072138)),((-3.88714525609152 24.508246314579743,-8.3935546875 27.449790329784214,-8.9208984375 29.458731185355344,-0.5712890625 32.02670629333614,-1.8896484375 35.60371874069731,8.5693359375 37.16031654673677,8.362166569827938 33.883846345901595,-3.88714525609152 24.508246314579743)))",
    };

static std::string mysql_21965285_b[2] =
    {
        "MULTIPOLYGON(((3 0, -19 -19, -7 3, -2 10, 15 0, 3 0)))",
        "MULTIPOLYGON(((1 1, 3 0, 19 -8, -4 -3, 1 1)),((3 0, -2 7, -3 16, 1 19, 8 12, 3 0)))"
    };

// formerly mysql_1
static std::string mysql_23023665_7[2] =
    {
        "MULTIPOLYGON(((4 5,12 11,-12 -3,4 5)))",
        "MULTIPOLYGON(((5 4,-14 0,1 0,5 4)),((1 6,13 0,10 12,1 6)))"
    };

static std::string mysql_23023665_8[2] =
    {
        "MULTIPOLYGON(((0 0,0 40,40 40,40 0,0 0),(10 10,30 10,30 30,10 30,10 10)))",
        "MULTIPOLYGON(((10 10,10 20,20 10,10 10)),((20 10,30 20,30 10,20 10)),((10 20,10 30,20 20,10 20)),((20 20,30 30,30 20,20 20)))"
    };

static std::string mysql_23023665_9[2] =
    {
        "MULTIPOLYGON(((0 0, 0 40, 40 40, 40 0, 0 0),(10 10, 30 10, 30 30, 10 30, 10 10)))",
        "MULTIPOLYGON(((15 10, 10 15, 10 17, 15 10)),((15 10, 10 20, 10 22, 15 10)),"
                     "((15 10, 10 25, 10 27, 15 10)),((25 10, 30 17, 30 15, 25 10)),"
                     "((25 10, 30 22, 30 20, 25 10)),((25 10, 30 27, 30 25, 25 10)),"
                     "((18 10, 20 30, 19 10, 18 10)),((21 10, 20 30, 22 10, 21 10)))"
    };

static std::string mysql_23023665_12[2] =
    {
        "MULTIPOLYGON(((6 7,18 14,-8 1,0 0,18 -8,6 7),(6 0,-4 3,5 3,6 0)))",
        "MULTIPOLYGON(((2 3,-3 5,-10 -1,2 3)))"
    };

static std::string mysql_regression_1_65_2017_08_31[2] =
    {
        "MULTIPOLYGON(((23.695652173913043 4.3478260869565215,23.333333333333336 4.166666666666667,25 0,23.695652173913043 4.3478260869565215)),((10 15,0 15,8.870967741935484 9.67741935483871,10.777777750841748 14.44444437710437,10 15)))",
        "MULTIPOLYGON(((10 15,20 15,15 25,10 15)),((10 15,0 15,7 10,5 0,15 5,15.90909090909091 4.545454545454546,17 10,10 15)),((23.695652173913043 4.3478260869565215,20 2.5,25 0,23.695652173913043 4.3478260869565215)))",
    };

#endif // BOOST_GEOMETRY_TEST_MULTI_OVERLAY_CASES_HPP
