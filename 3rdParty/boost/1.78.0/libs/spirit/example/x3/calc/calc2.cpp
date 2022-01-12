/*=============================================================================
    Copyright (c) 2001-2014 Joel de Guzman

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
=============================================================================*/
///////////////////////////////////////////////////////////////////////////////
//
//  A Calculator example demonstrating the grammar and semantic actions
//  using lambda functions. The parser prints code suitable for a stack
//  based virtual machine.
//
//  [ JDG May 10, 2002 ]        spirit 1
//  [ JDG March 4, 2007 ]       spirit 2
//  [ JDG February 21, 2011 ]   spirit 2.5
//  [ JDG June 6, 2014 ]        spirit x3 (from qi calc2, but using lambda functions)
//
///////////////////////////////////////////////////////////////////////////////

#include <boost/spirit/home/x3.hpp>
#include <boost/spirit/home/x3/support/ast/variant.hpp>
#include <boost/fusion/include/adapt_struct.hpp>

#include <iostream>
#include <string>
#include <list>
#include <numeric>

namespace x3 = boost::spirit::x3;

namespace client
{
    ///////////////////////////////////////////////////////////////////////////////
    //  Semantic actions
    ////////////////////////////////////////////////////////1///////////////////////
    namespace
    {
        using x3::_attr;

        auto do_int     = [](auto& ctx) { std::cout << "push " << _attr(ctx) << std::endl; };
        auto do_add     = []{ std::cout << "add\n"; };
        auto do_subt    = []{ std::cout << "subtract\n"; };
        auto do_mult    = []{ std::cout << "mult\n"; };
        auto do_div     = []{ std::cout << "divide\n"; };
        auto do_neg     = []{ std::cout << "negate\n"; };
    }

    ///////////////////////////////////////////////////////////////////////////////
    //  The calculator grammar
    ///////////////////////////////////////////////////////////////////////////////
    namespace calculator_grammar
    {
        using x3::uint_;
        using x3::char_;

        x3::rule<class expression> const expression("expression");
        x3::rule<class term> const term("term");
        x3::rule<class factor> const factor("factor");

        auto const expression_def =
            term
            >> *(   ('+' >> term            [do_add])
                |   ('-' >> term            [do_subt])
                )
            ;

        auto const term_def =
            factor
            >> *(   ('*' >> factor          [do_mult])
                |   ('/' >> factor          [do_div])
                )
            ;

        auto const factor_def =
                uint_                       [do_int]
            |   '(' >> expression >> ')'
            |   ('-' >> factor              [do_neg])
            |   ('+' >> factor)
            ;

        BOOST_SPIRIT_DEFINE(
            expression
          , term
          , factor
        );

        auto calculator = expression;
    }

    using calculator_grammar::calculator;

}

///////////////////////////////////////////////////////////////////////////////
//  Main program
///////////////////////////////////////////////////////////////////////////////
int
main()
{
    std::cout << "/////////////////////////////////////////////////////////\n\n";
    std::cout << "Expression parser...\n\n";
    std::cout << "/////////////////////////////////////////////////////////\n\n";
    std::cout << "Type an expression...or [q or Q] to quit\n\n";

    typedef std::string::const_iterator iterator_type;

    std::string str;
    while (std::getline(std::cin, str))
    {
        if (str.empty() || str[0] == 'q' || str[0] == 'Q')
            break;

        auto& calc = client::calculator;    // Our grammar

        iterator_type iter = str.begin();
        iterator_type end = str.end();
        boost::spirit::x3::ascii::space_type space;
        bool r = phrase_parse(iter, end, calc, space);

        if (r && iter == end)
        {
            std::cout << "-------------------------\n";
            std::cout << "Parsing succeeded\n";
            std::cout << "-------------------------\n";
        }
        else
        {
            std::string rest(iter, end);
            std::cout << "-------------------------\n";
            std::cout << "Parsing failed\n";
            std::cout << "stopped at: \"" << rest << "\"\n";
            std::cout << "-------------------------\n";
        }
    }

    std::cout << "Bye... :-) \n\n";
    return 0;
}
