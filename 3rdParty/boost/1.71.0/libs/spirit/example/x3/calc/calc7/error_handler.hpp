/*=============================================================================
    Copyright (c) 2001-2014 Joel de Guzman

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
=============================================================================*/
#if !defined(BOOST_SPIRIT_X3_CALC7_ERROR_HANDLER_HPP)
#define BOOST_SPIRIT_X3_CALC7_ERROR_HANDLER_HPP

#include <boost/spirit/home/x3.hpp>
#include <iostream>

namespace client { namespace calculator_grammar
{
    ////////////////////////////////////////////////////////////////////////////
    //  Our error handler
    ////////////////////////////////////////////////////////////////////////////
    namespace x3 = boost::spirit::x3;

    struct error_handler
    {
        //  Our error handler
        template <typename Iterator, typename Exception, typename Context>
        x3::error_handler_result
        on_error(Iterator&, Iterator const& last, Exception const& x, Context const& context)
        {
            std::cout
                << "Error! Expecting: "
                << x.which()
                << " here: \""
                << std::string(x.where(), last)
                << "\""
                << std::endl
                ;
            return x3::error_handler_result::fail;
        }
    };
}}

#endif
