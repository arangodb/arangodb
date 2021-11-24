// Boost.Geometry

// Copyright (c) 2017 Barend Gehrels, Amsterdam, the Netherlands.

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_GEOMETRY_TEST_CHECK_VALIDITY_HPP
#define BOOST_GEOMETRY_TEST_CHECK_VALIDITY_HPP

#include <boost/foreach.hpp>

#include <boost/geometry/algorithms/is_valid.hpp>

template<typename Geometry, typename G1, typename G2>
inline bool is_output_valid(Geometry const& geometry,
                            std::string const& case_id,
                            G1 const& g1, G2 const& g2,
                            std::string& message)
{
    bool const result = bg::is_valid(geometry, message);
    if (! result)
    {
        // Check if input was valid. If not, do not report output validity
        if (! bg::is_valid(g1) || ! bg::is_valid(g2))
        {
            std::cout << "WARNING: Input is not considered as valid; "
                      << "this can cause that output is invalid: " << case_id
                      << std::endl;
            return true;
        }
    }
    return result;
}

template
<
    typename Geometry,
    typename Tag = typename bg::tag<Geometry>::type
>
struct check_validity
{
    template <typename G1, typename G2>
    static inline
    bool apply(Geometry const& geometry,
               std::string const& case_id,
               G1 const& g1, G2 const& g2,
               std::string& message)
    {
        return is_output_valid(geometry, case_id, g1, g2, message);
    }
};

// Specialization for vector of <geometry> (e.g. rings)
template <typename Geometry>
struct check_validity<Geometry, void>
{
    template <typename G1, typename G2>
    static inline
    bool apply(Geometry const& geometry,
               std::string const& case_id,
               G1 const& g1, G2 const& g2,
               std::string& message)
    {
        typedef typename boost::range_value<Geometry>::type single_type;
        BOOST_FOREACH(single_type const& element, geometry)
        {
            if (! is_output_valid(element, case_id, g1, g2, message))
            {
                return false;
            }
        }
        return true;
    }
};


#endif // BOOST_GEOMETRY_TEST_CHECK_VALIDITY_HPP
