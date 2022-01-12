/*=============================================================================
    Copyright (c) 2002-2018 Joel de Guzman

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
=============================================================================*/
///////////////////////////////////////////////////////////////////////////////
//
//  Based on the employee parser (see employee.cpp), this example shows how
//  to annotate the AST with the iterator positions for access to the source
//  code when post processing. This example also shows how to "inject" client
//  data, using the "with" directive, that the handlers can access.
//
//  [ JDG May 9, 2007 ]
//  [ JDG May 13, 2015 ]    spirit X3
//  [ JDG Feb 22, 2018 ]    Parser annotations for spirit X3
//
//    I would like to thank Rainbowverse, llc (https://primeorbial.com/)
//    for sponsoring this work and donating it to the community.
//
///////////////////////////////////////////////////////////////////////////////

#include <boost/spirit/home/x3.hpp>
#include <boost/spirit/home/x3/support/ast/position_tagged.hpp>
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
        //  Our annotation handler
        ///////////////////////////////////////////////////////////////////////

        // tag used to get the position cache from the context
        struct position_cache_tag;

        struct annotate_position
        {
            template <typename T, typename Iterator, typename Context>
            inline void on_success(Iterator const& first, Iterator const& last
            , T& ast, Context const& context)
            {
                auto& position_cache = x3::get<position_cache_tag>(context).get();
                position_cache.annotate(ast, first, last);
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
        auto const person_def = quoted_string >> ',' >> quoted_string;

        auto const employee_def =
                '{'
            >>  int_ >> ','
            >>  person >> ','
            >>  double_
            >>  '}'
            ;

        auto const employees = employee >> *(',' >> employee);

        BOOST_SPIRIT_DEFINE(quoted_string, person, employee);

        struct quoted_string_class {};
        struct person_class : annotate_position {};
        struct employee_class : annotate_position {};
    }
}

///////////////////////////////////////////////////////////////////////////////
//  Main program
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
// Our main parse entry point
///////////////////////////////////////////////////////////////////////////////

using iterator_type = std::string::const_iterator;
using position_cache = boost::spirit::x3::position_cache<std::vector<iterator_type>>;

std::vector<client::ast::employee>
parse(std::string const& input, position_cache& positions)
{
    using boost::spirit::x3::ascii::space;

    std::vector<client::ast::employee> ast;
    iterator_type iter = input.begin();
    iterator_type const end = input.end();

    using boost::spirit::x3::with;

    // Our parser
    using client::parser::employees;
    using client::parser::position_cache_tag;

    auto const parser =
        // we pass our position_cache to the parser so we can access
        // it later in our on_sucess handlers
        with<position_cache_tag>(std::ref(positions))
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
    return ast;
}

// Sample input:

std::string input = R"(
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

int
main()
{
    position_cache positions{input.begin(), input.end()};
    auto ast = parse(input, positions);

    // Get the source of the 2nd employee and print it
    auto pos = positions.position_of(ast[1]); // zero based of course!
    std::cout << "Here's the 2nd employee:" << std::endl;
    std::cout << std::string(pos.begin(), pos.end()) << std::endl;
    std::cout << "-------------------------\n";
    return 0;
}
