/*=============================================================================
    Copyright (c) 2019 Nikita Kniazev

    Use, modification and distribution is subject to the Boost Software
    License, Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
    http://www.boost.org/LICENSE_1_0.txt)
=============================================================================*/

#include <boost/spirit/home/x3.hpp>

// Check that `BOOST_SPIRIT_INSTANTIATE` instantiates `parse_rule` with proper
// types when a rule has no attribute.

namespace unused_attr {

namespace x3 = boost::spirit::x3;

// skipper must has no attribute, checks `parse` and `skip_over`
using skipper_type = x3::rule<class skipper_r>;
extern const skipper_type skipper;
BOOST_SPIRIT_DECLARE(skipper_type)

// the `unused_type const` must have the same effect as no attribute
using skipper2_type = x3::rule<class skipper2_r, x3::unused_type const>;
extern const skipper2_type skipper2;
BOOST_SPIRIT_DECLARE(skipper2_type)

// grammar must has no attribute, checks `parse` and `phrase_parse`
using grammar_type = x3::rule<class grammar_r>;
extern const grammar_type grammar;
BOOST_SPIRIT_DECLARE(grammar_type)

}

// Check instantiation when rule has an attribute.

namespace used_attr {

namespace x3 = boost::spirit::x3;

using skipper_type = x3::rule<class skipper_r>;
extern const skipper_type skipper;
BOOST_SPIRIT_DECLARE(skipper_type)

using grammar_type = x3::rule<class grammar_r, int>;
extern const grammar_type grammar;
BOOST_SPIRIT_DECLARE(grammar_type)

}
