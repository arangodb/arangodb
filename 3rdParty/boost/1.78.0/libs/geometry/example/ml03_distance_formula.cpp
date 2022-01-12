// Boost.Geometry

// Copyright (c) 2018 Adeel Ahmad, Islamabad, Pakistan.

// Contributed and/or modified by Adeel Ahmad, as part of Google Summer of Code 2018 program.

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

// Formula example - Show how to use Karney's direct method.

#include <boost/geometry.hpp>
#include <boost/geometry/formulas/karney_direct.hpp>

using namespace boost::geometry;

int main()
{
    double lon1_deg = 0.;
    double lat1_deg = 73.114273316483;
    double distance_m = 19992866.6147806;
    double azi12_deg = 78.154765899661;

    // Create an alias of the formula.
    typedef formula::karney_direct<double, true, true, true, true, 8> karney_direct;

    // Structure to hold the resulting values.
    formula::result_direct<double> result;

    // WGS-84 spheroid.
    srs::spheroid<double> spheroid(6378137.0, 6356752.3142451793);

    result = karney_direct::apply(lon1_deg, lat1_deg, distance_m, azi12_deg, spheroid);

    return 0;
}
