// Boost.Geometry

// Copyright (c) 2017 Barend Gehrels, Amsterdam, the Netherlands.

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_GEOMETRY_TEST_SETOP_CHECK_VALIDITY_HPP
#define BOOST_GEOMETRY_TEST_SETOP_CHECK_VALIDITY_HPP

#include <boost/foreach.hpp>

#include <boost/geometry/algorithms/is_valid.hpp>

template
<
    typename Geometry,
    typename Tag = typename bg::tag<Geometry>::type
>
struct check_validity
{
    static inline
    bool apply(Geometry const& geometry, std::string& message)
    {
        return bg::is_valid(geometry, message);
    }
};

// Specialization for vector of <geometry> (e.g. rings)
template <typename Geometry>
struct check_validity<Geometry, void>
{
    static inline
    bool apply(Geometry const& geometry, std::string& message)
    {
        typedef typename boost::range_value<Geometry>::type single_type;
        BOOST_FOREACH(single_type const& element, geometry)
        {
            if (! bg::is_valid(element, message))
            {
                return false;
            }
        }
        return true;
    }
};


#endif // BOOST_GEOMETRY_TEST_SETOP_CHECK_VALIDITY_HPP
