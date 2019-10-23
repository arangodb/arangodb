// intersections.cpp
//
// Copyright (c) 2018
// Justinas V. Daugmaudis
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

//[intersections
/*`
    For the source of this example see
    [@boost://libs/random/example/intersections.cpp intersections.cpp].

    This example demonstrates generating quasi-randomly distributed chord
    entry and exit points on an S[sup 2] sphere.

    First we include the headers we need for __niederreiter_base2
    and __uniform_01 distribution.
 */

#include <boost/random/niederreiter_base2.hpp>
#include <boost/random/uniform_01.hpp>

#include <boost/math/constants/constants.hpp>

#include <boost/tuple/tuple.hpp>

/*`
  We use 4-dimensional __niederreiter_base2 as a source of randomness.
 */
boost::random::niederreiter_base2 gen(4);


int main()
{
  typedef boost::tuple<double, double, double> point_t;

  const std::size_t n_points = 100; // we will generate 100 points

  std::vector<point_t> points;
  points.reserve(n_points);

  /*<< __niederreiter_base2 produces integers in the range [0, 2[sup 64]-1].
  However, we want numbers in the range [0, 1). The distribution
  __uniform_01 performs this transformation.
  >>*/
  boost::random::uniform_01<double> dist;

  for (std::size_t i = 0; i != n_points; ++i)
  {
    /*`
      Using formula from J. Rovira et al., "Point sampling with uniformly distributed lines", 2005
      to compute uniformly distributed chord entry and exit points on the surface of a sphere.
    */
    double cos_theta = 1 - 2 * dist(gen);
    double sin_theta = std::sqrt(1 - cos_theta * cos_theta);
    double phi = boost::math::constants::two_pi<double>() * dist(gen);
    double sin_phi = std::sin(phi), cos_phi = std::cos(phi);

    point_t point_on_sphere(sin_theta*sin_phi, cos_theta, sin_theta*cos_phi);

    /*`
      Here we assume that our sphere is a unit sphere at origin. If your sphere was
      different then now would be the time to scale and translate the `point_on_sphere`.
    */

    points.push_back(point_on_sphere);
  }

  /*`
    Vector `points` now holds generated 3D points on a sphere.
  */

  return 0;
}

//]
