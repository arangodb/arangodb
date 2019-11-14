/*=============================================================================
    Copyright (c) 2002-2018 Joel de Guzman

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
=============================================================================*/
///////////////////////////////////////////////////////////////////////////////
//
//  This is the same employee parser (see employee.cpp) but structured to
//  allow separate compilation of the actual parser in its own definition
//  file (employee_def.hpp) and cpp file (employee.cpp). This main cpp file
//  sees only the header file (employee.hpp). This is a good example on how
//  parsers are structured in a C++ application.
//
//  [ JDG May 9, 2007 ]
//  [ JDG May 13, 2015 ]    spirit X3
//  [ JDG Feb 20, 2018 ]    Minimal "best practice" example
//
//    I would like to thank Rainbowverse, llc (https://primeorbial.com/)
//    for sponsoring this work and donating it to the community.
//
///////////////////////////////////////////////////////////////////////////////

#include "ast.hpp"
#include "ast_adapted.hpp"
#include "employee.hpp"

///////////////////////////////////////////////////////////////////////////////
//  Main program
///////////////////////////////////////////////////////////////////////////////
int
main()
{
    std::cout << "/////////////////////////////////////////////////////////\n\n";
    std::cout << "\t\tAn employee parser for Spirit...\n\n";
    std::cout << "/////////////////////////////////////////////////////////\n\n";

    std::cout
        << "Give me an employee of the form :"
        << "employee{age, \"forename\", \"surname\", salary } \n";
    std::cout << "Type [q or Q] to quit\n\n";

    using boost::spirit::x3::ascii::space;
    using iterator_type = std::string::const_iterator;
    using client::employee;

    std::string str;
    while (getline(std::cin, str))
    {
        if (str.empty() || str[0] == 'q' || str[0] == 'Q')
            break;

        client::ast::employee emp;
        iterator_type iter = str.begin();
        iterator_type const end = str.end();
        bool r = phrase_parse(iter, end, employee(), space, emp);

        if (r && iter == end)
        {
            std::cout << boost::fusion::tuple_open('[');
            std::cout << boost::fusion::tuple_close(']');
            std::cout << boost::fusion::tuple_delimiter(", ");

            std::cout << "-------------------------\n";
            std::cout << "Parsing succeeded\n";
            std::cout << "got: " << emp << std::endl;
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
