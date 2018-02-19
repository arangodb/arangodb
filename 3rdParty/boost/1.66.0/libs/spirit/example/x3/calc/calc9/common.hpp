/*=============================================================================
    Copyright (c) 2001-2014 Joel de Guzman

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
=============================================================================*/
#if !defined(BOOST_SPIRIT_X3_CALC9_COMMON_HPP)
#define BOOST_SPIRIT_X3_CALC9_COMMON_HPP

#include <boost/spirit/home/x3.hpp>

namespace client { namespace parser
{
    using x3::raw;
    using x3::lexeme;
    using x3::alpha;
    using x3::alnum;

    struct identifier_class;
    typedef x3::rule<identifier_class, std::string> identifier_type;
    identifier_type const identifier = "identifier";

    auto const identifier_def = raw[lexeme[(alpha | '_') >> *(alnum | '_')]];

    BOOST_SPIRIT_DEFINE(identifier);
}}

#endif
