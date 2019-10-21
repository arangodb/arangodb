// Boost.Geometry

// Copyright (c) 2017-2018, Oracle and/or its affiliates.

// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

// Licensed under the Boost Software License version 1.0.
// http://www.boost.org/users/license.html

#ifndef BOOST_GEOMETRY_STRATEGIES_CARTESIAN_DENSIFY_HPP
#define BOOST_GEOMETRY_STRATEGIES_CARTESIAN_DENSIFY_HPP


#include <boost/geometry/algorithms/detail/convert_point_to_point.hpp>
#include <boost/geometry/algorithms/detail/signed_size_type.hpp>
#include <boost/geometry/arithmetic/arithmetic.hpp>
#include <boost/geometry/arithmetic/dot_product.hpp>
#include <boost/geometry/core/assert.hpp>
#include <boost/geometry/core/coordinate_dimension.hpp>
#include <boost/geometry/core/coordinate_type.hpp>
#include <boost/geometry/geometries/point.hpp>
#include <boost/geometry/strategies/densify.hpp>
#include <boost/geometry/util/math.hpp>
#include <boost/geometry/util/select_most_precise.hpp>


namespace boost { namespace geometry
{

namespace strategy { namespace densify
{


/*!
\brief Densification of cartesian segment.
\ingroup strategies
\tparam CalculationType \tparam_calculation

\qbk{
[heading See also]
[link geometry.reference.algorithms.densify.densify_4_with_strategy densify (with strategy)]
}
 */
template
<
    typename CalculationType = void
>
class cartesian
{
public:
    template <typename Point, typename AssignPolicy, typename T>
    static inline void apply(Point const& p0, Point const& p1, AssignPolicy & policy, T const& length_threshold)
    {
        typedef typename AssignPolicy::point_type out_point_t;
        typedef typename select_most_precise
            <
                typename coordinate_type<Point>::type,
                typename coordinate_type<out_point_t>::type,
                CalculationType
            >::type calc_t;

        typedef model::point<calc_t, geometry::dimension<Point>::value, cs::cartesian> calc_point_t;
        
        calc_point_t cp0, cp1;
        geometry::detail::conversion::convert_point_to_point(p0, cp0);
        geometry::detail::conversion::convert_point_to_point(p1, cp1);

        // dir01 = xy1 - xy0
        calc_point_t dir01 = cp1;
        geometry::subtract_point(dir01, cp0);
        calc_t const dot01 = geometry::dot_product(dir01, dir01);
        calc_t const len = math::sqrt(dot01);

        BOOST_GEOMETRY_ASSERT(length_threshold > T(0));

        signed_size_type n = signed_size_type(len / length_threshold);
        if (n <= 0)
        {
            return;
        }

        // NOTE: Normalization will not work for integral coordinates
        // normalize
        //geometry::divide_value(dir01, len);

        calc_t step = len / (n + 1);

        calc_t d = step;
        for (signed_size_type i = 0 ; i < n ; ++i, d += step)
        {
            // pd = xy0 + d * dir01
            calc_point_t pd = dir01;

            // without normalization
            geometry::multiply_value(pd, calc_t(i + 1));
            geometry::divide_value(pd, calc_t(n + 1));
            // with normalization
            //geometry::multiply_value(pd, d);
            
            geometry::add_point(pd, cp0);

            // NOTE: Only needed if types calc_point_t and out_point_t are different
            // otherwise pd could simply be passed into policy
            out_point_t p;
            assert_dimension_equal<calc_point_t, out_point_t>();
            geometry::detail::conversion::convert_point_to_point(pd, p);
            
            policy.apply(p);
        }
    }
};


#ifndef DOXYGEN_NO_STRATEGY_SPECIALIZATIONS
namespace services
{

template <>
struct default_strategy<cartesian_tag>
{
    typedef strategy::densify::cartesian<> type;
};


} // namespace services
#endif // DOXYGEN_NO_STRATEGY_SPECIALIZATIONS


}} // namespace strategy::densify


}} // namespace boost::geometry

#endif // BOOST_GEOMETRY_STRATEGIES_CARTESIAN_DENSIFY_HPP
