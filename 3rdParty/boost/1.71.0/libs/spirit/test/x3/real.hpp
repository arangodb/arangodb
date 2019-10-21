/*=============================================================================
    Copyright (c) 2001-2010 Joel de Guzman
    Copyright (c) 2001-2010 Hartmut Kaiser

    Use, modification and distribution is subject to the Boost Software
    License, Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
    http://www.boost.org/LICENSE_1_0.txt)
=============================================================================*/
#if !defined(BOOST_SPIRIT_TEST_X3_REAL_HPP)
#define BOOST_SPIRIT_TEST_X3_REAL_HPP

#include <climits>
#include <boost/math/concepts/real_concept.hpp>
#include <boost/detail/lightweight_test.hpp>
#include <boost/spirit/home/x3/char.hpp>
#include <boost/spirit/home/x3/numeric.hpp>
#include <boost/spirit/home/x3/operator.hpp>
#include <boost/math/special_functions/fpclassify.hpp>
#include <boost/math/special_functions/sign.hpp>

#include "test.hpp"

///////////////////////////////////////////////////////////////////////////////
//  These policies can be used to parse thousand separated
//  numbers with at most 2 decimal digits after the decimal
//  point. e.g. 123,456,789.01
///////////////////////////////////////////////////////////////////////////////
template <typename T>
struct ts_real_policies : boost::spirit::x3::ureal_policies<T>
{
    //  2 decimal places Max
    template <typename Iterator, typename Attribute>
    static bool
    parse_frac_n(Iterator& first, Iterator const& last, Attribute& attr)
    {
        namespace x3 = boost::spirit::x3;
        return boost::spirit::x3::extract_uint<T, 10, 1, 2, true>::call(first, last, attr);
    }

    //  No exponent
    template <typename Iterator>
    static bool
    parse_exp(Iterator&, Iterator const&)
    {
        return false;
    }

    //  No exponent
    template <typename Iterator, typename Attribute>
    static bool
    parse_exp_n(Iterator&, Iterator const&, Attribute&)
    {
        return false;
    }

    //  Thousands separated numbers
    template <typename Iterator, typename Accumulator>
    static bool
    parse_n(Iterator& first, Iterator const& last, Accumulator& result)
    {
        using boost::spirit::x3::uint_parser;
        namespace x3 = boost::spirit::x3;

        uint_parser<unsigned, 10, 1, 3> uint3;
        uint_parser<unsigned, 10, 3, 3> uint3_3;

        if (parse(first, last, uint3, result))
        {
            Accumulator n;
            Iterator iter = first;

            while (x3::parse(iter, last, ',') && x3::parse(iter, last, uint3_3, n))
            {
                result = result * 1000 + n;
                first = iter;
            }

            return true;
        }
        return false;
    }
};

template <typename T>
struct no_trailing_dot_policy : boost::spirit::x3::real_policies<T>
{
    static bool const allow_trailing_dot = false;
};

template <typename T>
struct no_leading_dot_policy : boost::spirit::x3::real_policies<T>
{
    static bool const allow_leading_dot = false;
};

template <typename T, typename T2>
bool
compare(T n, T2 expected)
{
    T const eps = std::pow(10.0, -std::numeric_limits<T>::digits10);
    T delta = n - expected;
    return (delta >= -eps) && (delta <= eps);
}

///////////////////////////////////////////////////////////////////////////////
// A custom real type
struct custom_real
{
    double n;
    custom_real() : n(0) {}
    custom_real(double n_) : n(n_) {}
    friend bool operator==(custom_real a, custom_real b)
        { return a.n == b.n; }
    friend custom_real operator*(custom_real a, custom_real b)
        { return custom_real(a.n * b.n); }
    friend custom_real operator+(custom_real a, custom_real b)
        { return custom_real(a.n + b.n); }
    friend custom_real operator-(custom_real a, custom_real b)
        { return custom_real(a.n - b.n); }
};

#endif
