/*=============================================================================
    Copyright (c) 2002-2015 Joel de Guzman

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
=============================================================================*/
///////////////////////////////////////////////////////////////////////////////
//
//  A parser for summing a comma-separated list of numbers using phoenix.
//
//  [ JDG June 28, 2002 ]   spirit1
//  [ JDG March 24, 2007 ]  spirit2
//  [ JDG May 12, 2015 ]    spirit X3
//
///////////////////////////////////////////////////////////////////////////////

#include <boost/spirit/home/x3.hpp>
#include <iostream>
#include <string>

namespace client
{
    namespace x3 = boost::spirit::x3;
    namespace ascii = boost::spirit::x3::ascii;

    using x3::double_;
    using ascii::space;
    using x3::_attr;

    ///////////////////////////////////////////////////////////////////////////
    //  Our adder
    ///////////////////////////////////////////////////////////////////////////

    template <typename Iterator>
    bool adder(Iterator first, Iterator last, double& n)
    {
        auto assign = [&](auto& ctx){ n = _attr(ctx); };
        auto add = [&](auto& ctx){ n += _attr(ctx); };

        bool r = x3::phrase_parse(first, last,

            //  Begin grammar
            (
                double_[assign] >> *(',' >> double_[add])
            )
            ,
            //  End grammar

            space);

        if (first != last) // fail if we did not get a full match
            return false;
        return r;
    }
}

////////////////////////////////////////////////////////////////////////////
//  Main program
////////////////////////////////////////////////////////////////////////////
int
main()
{
    std::cout << "/////////////////////////////////////////////////////////\n\n";
    std::cout << "\t\tA parser for summing a list of numbers...\n\n";
    std::cout << "/////////////////////////////////////////////////////////\n\n";

    std::cout << "Give me a comma separated list of numbers.\n";
    std::cout << "The numbers are added using Phoenix.\n";
    std::cout << "Type [q or Q] to quit\n\n";

    std::string str;
    while (getline(std::cin, str))
    {
        if (str.empty() || str[0] == 'q' || str[0] == 'Q')
            break;

        double n;
        if (client::adder(str.begin(), str.end(), n))
        {
            std::cout << "-------------------------\n";
            std::cout << "Parsing succeeded\n";
            std::cout << str << " Parses OK: " << std::endl;

            std::cout << "sum = " << n;
            std::cout << "\n-------------------------\n";
        }
        else
        {
            std::cout << "-------------------------\n";
            std::cout << "Parsing failed\n";
            std::cout << "-------------------------\n";
        }
    }

    std::cout << "Bye... :-) \n\n";
    return 0;
}
