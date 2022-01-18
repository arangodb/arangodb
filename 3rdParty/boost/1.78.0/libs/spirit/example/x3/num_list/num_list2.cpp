/*=============================================================================
    Copyright (c) 2002-2015 Joel de Guzman

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
=============================================================================*/
///////////////////////////////////////////////////////////////////////////////
//
//  This sample demontrates a parser for a comma separated list of numbers.
//
//  [ JDG May 10, 2002 ]    spirit1
//  [ JDG March 24, 2007 ]  spirit2
//  [ JDG May 12, 2015 ]    spirit X3
//
///////////////////////////////////////////////////////////////////////////////

#include <boost/spirit/home/x3.hpp>

#include <iostream>
#include <string>
#include <vector>

namespace client
{
    namespace x3 = boost::spirit::x3;
    namespace ascii = boost::spirit::x3::ascii;

    ///////////////////////////////////////////////////////////////////////////
    //  Our number list compiler
    ///////////////////////////////////////////////////////////////////////////
    template <typename Iterator>
    bool parse_numbers(Iterator first, Iterator last, std::vector<double>& v)
    {
        using x3::double_;
        using x3::phrase_parse;
        using x3::_attr;
        using ascii::space;

        auto push_back = [&](auto& ctx){ v.push_back(_attr(ctx)); };

        bool r = phrase_parse(first, last,

            //  Begin grammar
            (
                double_[push_back] >> *(',' >> double_[push_back])
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
    std::cout << "\t\tA comma separated list parser for Spirit...\n\n";
    std::cout << "/////////////////////////////////////////////////////////\n\n";

    std::cout << "Give me a comma separated list of numbers.\n";
    std::cout << "The numbers will be inserted in a vector of numbers\n";
    std::cout << "Type [q or Q] to quit\n\n";

    std::string str;
    while (getline(std::cin, str))
    {
        if (str.empty() || str[0] == 'q' || str[0] == 'Q')
            break;

        std::vector<double> v;
        if (client::parse_numbers(str.begin(), str.end(), v))
        {
            std::cout << "-------------------------\n";
            std::cout << "Parsing succeeded\n";
            std::cout << str << " Parses OK: " << std::endl;

            for (std::vector<double>::size_type i = 0; i < v.size(); ++i)
                std::cout << i << ": " << v[i] << std::endl;

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
