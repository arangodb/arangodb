// Boost.Geometry
// Unit Test

// Copyright (c) 2016 Oracle and/or its affiliates.

// Contributed and/or modified by Vissarion Fysikopoulos, on behalf of Oracle

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef LINESTRING_CASES_HPP
#define LINESTRING_CASES_HPP

#include <string>

static std::string Ls_data_geo[] = {"LINESTRING(0 90,1 80,1 70)",
                                    "LINESTRING(0 90,1 80,1 80,1 80,1 70,1 70)",
                                    "LINESTRING(0 90,1 80,1 79,1 78,1 77,1 76,1 75,1 74,\
                                                1 73,1 72,1 71,1 70)"};

static std::string Ls_data_sph[] = {"LINESTRING(0 0,180 0,180 180)",
                                    "LINESTRING(0 0,180 0,180 0,180 0,180 180,180 180)",
                                    "LINESTRING(0 0,180 0,180 10,180 20,180 30,180 40,180 50,180 60,\
                                                180 70,180 80,180 90,180 100,180 110,180 120,180 130,\
                                                180 140,180 150,180 160,180 170,180 180)"};

#endif // LINESTRING_CASES_HPP
