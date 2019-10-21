/*=============================================================================
    Copyright (c) 2019 Nikita Kniazev

    Use, modification and distribution is subject to the Boost Software
    License, Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
    http://www.boost.org/LICENSE_1_0.txt)
=============================================================================*/
#if !defined(BOOST_SPIRIT_TEST_X3_UTILS_HPP)
#define BOOST_SPIRIT_TEST_X3_UTILS_HPP

#include <boost/spirit/home/x3/core/parser.hpp>

struct move_only
{
    move_only() = default;
    move_only(move_only&&) = default;
    move_only& operator=(move_only&&) = default;
};


template <typename T>
struct synth_parser : boost::spirit::x3::parser<synth_parser<T>>
{
    typedef T attribute_type;

    static bool const has_attribute = true;
    static bool const handles_container = false;

    template <typename Iterator, typename Context,
        typename RuleContext, typename Attribute>
    bool parse(Iterator& iter, Iterator const& last, Context const&,
        RuleContext&, Attribute& attr) const
    {
        if (iter != last && *iter == 's') {
            ++iter;
            boost::spirit::x3::traits::move_to(attribute_type{}, attr);
            return true;
        }
        return false;
    }
};

template <typename T>
synth_parser<T> synth{};

synth_parser<move_only> const synth_move_only{};

#endif
