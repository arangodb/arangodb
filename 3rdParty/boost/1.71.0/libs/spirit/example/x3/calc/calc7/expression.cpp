/*=============================================================================
    Copyright (c) 2001-2014 Joel de Guzman

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
=============================================================================*/
#include "expression_def.hpp"

namespace client { namespace calculator_grammar
{
    typedef std::string::const_iterator iterator_type;
    typedef x3::phrase_parse_context<x3::ascii::space_type>::type context_type;

    BOOST_SPIRIT_INSTANTIATE(expression_type, iterator_type, context_type);
}}
