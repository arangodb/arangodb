/*=============================================================================
    Copyright (c) 2019 Nikita Kniazev

    Use, modification and distribution is subject to the Boost Software
    License, Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
    http://www.boost.org/LICENSE_1_0.txt)
=============================================================================*/

#include "rule_separate_tu_grammar.hpp"

#include <boost/spirit/home/x3.hpp>

namespace unused_attr {

const skipper_type skipper = "skipper";
const auto skipper_def = x3::lit('*');
BOOST_SPIRIT_DEFINE(skipper)
BOOST_SPIRIT_INSTANTIATE(skipper_type, char const*, x3::unused_type)

const skipper2_type skipper2 = "skipper2";
const auto skipper2_def = x3::lit('#');
BOOST_SPIRIT_DEFINE(skipper2)
BOOST_SPIRIT_INSTANTIATE(skipper2_type, char const*, x3::unused_type)

const grammar_type grammar = "grammar";
const auto grammar_def = *x3::lit('=');
BOOST_SPIRIT_DEFINE(grammar)
BOOST_SPIRIT_INSTANTIATE(grammar_type, char const*, x3::unused_type)
BOOST_SPIRIT_INSTANTIATE(grammar_type, char const*, x3::phrase_parse_context<skipper_type>::type)
BOOST_SPIRIT_INSTANTIATE(grammar_type, char const*, x3::phrase_parse_context<skipper2_type>::type)

}

namespace used_attr {

const skipper_type skipper = "skipper";
const auto skipper_def = x3::space;
BOOST_SPIRIT_DEFINE(skipper)
BOOST_SPIRIT_INSTANTIATE(skipper_type, char const*, x3::unused_type)

const grammar_type grammar = "grammar";
const auto grammar_def = x3::int_;
BOOST_SPIRIT_DEFINE(grammar)
BOOST_SPIRIT_INSTANTIATE(grammar_type, char const*, x3::unused_type)
BOOST_SPIRIT_INSTANTIATE(grammar_type, char const*, x3::phrase_parse_context<skipper_type>::type)

}
