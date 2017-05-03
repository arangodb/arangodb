/*=============================================================================
    Copyright (c) 2001-2015 Joel de Guzman

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
=============================================================================*/
#if !defined(BOOST_SPIRIT_X3_REPR_CONFIG_HPP)
#define BOOST_SPIRIT_X3_REPR_CONFIG_HPP

#include "error_handler.hpp"

#include <boost/spirit/home/x3.hpp>

namespace rexpr { namespace parser
{
    // Our Iterator Type
    typedef std::string::const_iterator iterator_type;

    // The Phrase Parse Context
    typedef
        x3::phrase_parse_context<x3::ascii::space_type>::type
    phrase_context_type;

    // Our Error Handler
    typedef error_handler<iterator_type> error_handler_type;

    // Combined Error Handler and Phrase Parse Context
    typedef x3::with_context<
        error_handler_tag
      , std::reference_wrapper<error_handler_type> const
      , phrase_context_type>::type
    context_type;
}}

#endif
