// Boost.Geometry

// Copyright (c) 2017 Barend Gehrels, Amsterdam, the Netherlands.

// This file was modified by Oracle on 2021.
// Modifications copyright (c) 2021, Oracle and/or its affiliates.
// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_GEOMETRY_TEST_CHECK_VALIDITY_HPP
#define BOOST_GEOMETRY_TEST_CHECK_VALIDITY_HPP

#include <boost/geometry/algorithms/is_valid.hpp>

template<typename Geometry>
inline bool input_is_valid(std::string const& case_id, std::string const& subcase,
                         Geometry const& geometry)
{
    std::string message;
    bool const result = bg::is_valid(geometry, message);
    if (! result)
    {
        std::cout << "WARNING: " << case_id << " Input [" << subcase
                  << "] is not considered as valid ("
                  << message << ") this can cause that output is invalid: "
                  << case_id << std::endl;
    }
    return result;
}


template<typename Geometry, typename G1, typename G2>
inline bool is_output_valid(Geometry const& geometry,
                            std::string const& case_id,
                            G1 const& g1, G2 const& g2,
                            bool ignore_validity_on_invalid_input,
                            std::string& message)
{
    bool result = bg::is_valid(geometry, message);
    if (! result && ignore_validity_on_invalid_input)
    {
        if (! input_is_valid(case_id, "a", g1)
            || ! input_is_valid(case_id, "b", g2))
        {
            // Because input is invalid, output validity is ignored
            result = true;
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
               std::string& message,
               bool ignore_validity_on_invalid_input = true)
    {
        return is_output_valid(geometry, case_id, g1, g2,
                               ignore_validity_on_invalid_input, message);
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
               std::string& message,
               bool ignore_validity_on_invalid_input = true)
    {
        typedef typename boost::range_value<Geometry>::type single_type;
        for (single_type const& element : geometry)
        {
            if (! is_output_valid(element, case_id, g1, g2,
                                  ignore_validity_on_invalid_input, message))
            {
                return false;
            }
        }
        return true;
    }
};


#endif // BOOST_GEOMETRY_TEST_CHECK_VALIDITY_HPP
