/*=============================================================================
    Copyright (c) 2002-2018 Joel de Guzman

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
=============================================================================*/
///////////////////////////////////////////////////////////////////////////////
//
//  Based on the employee parser (see employee.cpp), this example shows how
//  to implement error handling. This example also shows how to "inject" client
//  data, using the "with" directive, that the handlers can access.
//
//  [ JDG May 9, 2007 ]
//  [ JDG May 13, 2015 ]    spirit X3
//  [ JDG Feb 19, 2018 ]    Error handling for spirit X3
//
//    I would like to thank Rainbowverse, llc (https://primeorbial.com/)
//    for sponsoring this work and donating it to the community.
//
///////////////////////////////////////////////////////////////////////////////

#include <boost/config/warning_disable.hpp>
#include <boost/spirit/home/x3.hpp>
#include <boost/spirit/home/x3/support/ast/position_tagged.hpp>
#include <boost/spirit/home/x3/support/utility/error_reporting.hpp>
#include <boost/spirit/home/x3/support/utility/annotate_on_success.hpp>
#include <boost/fusion/include/adapt_struct.hpp>
#include <boost/fusion/include/io.hpp>

#include <iostream>
#include <string>

namespace client { namespace ast
{
    ///////////////////////////////////////////////////////////////////////////
    //  Our AST (employee and person structs)
    ///////////////////////////////////////////////////////////////////////////
    namespace x3 = boost::spirit::x3;

    struct person : x3::position_tagged
    {
        person(
            std::string const& first_name = ""
          , std::string const& last_name = ""
        )
          : first_name(first_name)
          , last_name(last_name)
        {}

        std::string first_name, last_name;
    };

    struct employee : x3::position_tagged
    {
        int age;
        person who;
        double salary;
    };

    using boost::fusion::operator<<;
}}

// We need to tell fusion about our employee struct
// to make it a first-class fusion citizen. This has to
// be in global scope.

BOOST_FUSION_ADAPT_STRUCT(client::ast::person,
    first_name, last_name
)

BOOST_FUSION_ADAPT_STRUCT(client::ast::employee,
    age, who, salary
)

namespace client
{
    namespace parser
    {
        namespace x3 = boost::spirit::x3;
        namespace ascii = boost::spirit::x3::ascii;

        ///////////////////////////////////////////////////////////////////////
        //  Our error handler
        ///////////////////////////////////////////////////////////////////////
        struct error_handler
        {
            template <typename Iterator, typename Exception, typename Context>
            x3::error_handler_result on_error(
                Iterator& first, Iterator const& last
              , Exception const& x, Context const& context)
            {
                auto& error_handler = x3::get<x3::error_handler_tag>(context).get();
                std::string message = "Error! Expecting: " + x.which() + " here:";
                error_handler(x.where(), message);
                return x3::error_handler_result::fail;
            }
        };

        ///////////////////////////////////////////////////////////////////////
        //  Our employee parser
        ///////////////////////////////////////////////////////////////////////

        using x3::int_;
        using x3::double_;
        using x3::lexeme;
        using ascii::char_;

        struct quoted_string_class;
        struct person_class;
        struct employee_class;

        x3::rule<quoted_string_class, std::string> const quoted_string = "quoted_string";
        x3::rule<person_class, ast::person> const person = "person";
        x3::rule<employee_class, ast::employee> const employee = "employee";

        auto const quoted_string_def = lexeme['"' >> +(char_ - '"') >> '"'];
        auto const person_def = quoted_string > ',' > quoted_string;

        auto const employee_def =
                '{'
            >   int_ > ','
            >   person > ','
            >   double_
            >   '}'
            ;

        auto const employees = employee >> *(',' >> employee);

        BOOST_SPIRIT_DEFINE(quoted_string, person, employee);

        struct quoted_string_class {};
        struct person_class : x3::annotate_on_success {};
        struct employee_class : error_handler, x3::annotate_on_success {};
    }
}

///////////////////////////////////////////////////////////////////////////////
//  Main program
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
// Our main parse entry point
///////////////////////////////////////////////////////////////////////////////

void parse(std::string const& input)
{
    using boost::spirit::x3::ascii::space;
    typedef std::string::const_iterator iterator_type;

    std::vector<client::ast::employee> ast;
    iterator_type iter = input.begin();
    iterator_type const end = input.end();

    using boost::spirit::x3::with;
    using boost::spirit::x3::error_handler_tag;
    using error_handler_type = boost::spirit::x3::error_handler<iterator_type>;

    // Our error handler
    error_handler_type error_handler(iter, end, std::cerr);

    // Our parser
    using client::parser::employees;
    auto const parser =
        // we pass our error handler to the parser so we can access
        // it later in our on_error and on_sucess handlers
        with<error_handler_tag>(std::ref(error_handler))
        [
            employees
        ];

    bool r = phrase_parse(iter, end, parser, space, ast);

    if (r && iter == end)
    {
        std::cout << boost::fusion::tuple_open('[');
        std::cout << boost::fusion::tuple_close(']');
        std::cout << boost::fusion::tuple_delimiter(", ");

        std::cout << "-------------------------\n";
        std::cout << "Parsing succeeded\n";

        for (auto const& emp : ast)
        {
            std::cout << "got: " << emp << std::endl;
        }
        std::cout << "\n-------------------------\n";

    }
    else
    {
        std::cout << "-------------------------\n";
        std::cout << "Parsing failed\n";
        std::cout << "-------------------------\n";
        ast.clear();
    }
}

// Good sample:

std::string good_input = R"(
{
    23,
    "Amanda",
    "Stefanski",
    1000.99
},
{
    35,
    "Angie",
    "Chilcote",
    2000.99
},
{
    43,
    "Dannie",
    "Dillinger",
    3000.99
},
{
    22,
    "Dorene",
    "Dole",
    2500.99
},
{
    38,
    "Rossana",
    "Rafferty",
    5000.99
}
)";

// Input sample with error:

std::string bad_input = R"(
{
    23,
    "Amanda",
    "Stefanski",
    1000.99
},
{
    35,
    "Angie",
    "Chilcote",
    2000.99
},
{
    43,
    'I am not a person!'    <--- this should be a person
    3000.99
},
{
    22,
    "Dorene",
    "Dole",
    2500.99
},
{
    38,
    "Rossana",
    "Rafferty",
    5000.99
}
)";

int
main()
{
    // Good input
    parse(good_input);

   // Bad input
    std::cout << "Now we have some errors" << std::endl;
    parse(bad_input);
    return 0;
}
