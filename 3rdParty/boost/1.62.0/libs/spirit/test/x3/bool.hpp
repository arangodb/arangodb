/*=============================================================================
    Copyright (c) 2001-2011 Hartmut Kaiser
    Copyright (c) 2011      Bryce Lelbach

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
==============================================================================*/
#if !defined(BOOST_SPIRIT_TEST_QI_BOOL)
#define BOOST_SPIRIT_TEST_QI_BOOL

#include <boost/detail/lightweight_test.hpp>
#include <boost/spirit/home/x3.hpp>
#include "test.hpp"

///////////////////////////////////////////////////////////////////////////////
struct backwards_bool_policies : boost::spirit::x3::bool_policies<>
{
    // we want to interpret a 'true' spelled backwards as 'false'
    template <typename Iterator, typename Attribute, typename CaseCompare>
    static bool
    parse_false(Iterator& first, Iterator const& last, Attribute& attr, CaseCompare const& case_compare)
    {
        namespace spirit = boost::spirit;
        namespace x3 = boost::spirit::x3;
        if (x3::detail::string_parse("eurt", first, last, x3::unused, case_compare))
        {
            x3::traits::move_to(false, attr);   // result is false
            return true;
        }
        return false;
    }
};

///////////////////////////////////////////////////////////////////////////////
struct test_bool_type
{
    test_bool_type(bool b = false) : b(b) {}    // provide conversion
    bool b;
};

#endif
