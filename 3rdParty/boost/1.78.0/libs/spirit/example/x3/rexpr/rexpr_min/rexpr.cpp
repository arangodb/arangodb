/*=============================================================================
    Copyright (c) 2001-2015 Joel de Guzman

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
=============================================================================*/
///////////////////////////////////////////////////////////////////////////////
//
//  A simple parser for X3 intended as a minimal starting point.
//  'rexpr' is a parser for a language resembling a minimal subset
//  of json, but limited to a dictionary (composed of key=value pairs)
//  where the value can itself be a string or a recursive dictionary.
//
//  Example:
//
//  {
//      "color" = "blue"
//      "size" = "29 cm."
//      "position" = {
//          "x" = "123"
//          "y" = "456"
//      }
//  }
//
///////////////////////////////////////////////////////////////////////////////

#include <boost/spirit/home/x3.hpp>
#include <boost/spirit/home/x3/support/ast/variant.hpp>
#include <boost/fusion/include/adapt_struct.hpp>
#include <boost/fusion/include/std_pair.hpp>
#include <boost/fusion/include/io.hpp>

#include <iostream>
#include <fstream>
#include <string>
#include <map>

///////////////////////////////////////////////////////////////////////////////
//  Our AST
///////////////////////////////////////////////////////////////////////////////
namespace client { namespace ast
{
    namespace fusion = boost::fusion;
    namespace x3 = boost::spirit::x3;

    struct rexpr;

    struct rexpr_value : x3::variant<
            std::string
          , x3::forward_ast<rexpr>
        >
    {
        using base_type::base_type;
        using base_type::operator=;
    };

    typedef std::map<std::string, rexpr_value> rexpr_map;
    typedef std::pair<std::string, rexpr_value> rexpr_key_value;

    struct rexpr
    {
        rexpr_map entries;
    };
}}

// We need to tell fusion about our rexpr struct
// to make it a first-class fusion citizen
BOOST_FUSION_ADAPT_STRUCT(client::ast::rexpr,
    entries
)

///////////////////////////////////////////////////////////////////////////////
//  AST processing
///////////////////////////////////////////////////////////////////////////////
namespace client { namespace ast
{
    ///////////////////////////////////////////////////////////////////////////
    //  Print out the rexpr tree
    ///////////////////////////////////////////////////////////////////////////
    int const tabsize = 4;

    struct rexpr_printer
    {
        typedef void result_type;

        rexpr_printer(int indent = 0)
          : indent(indent) {}

        void operator()(rexpr const& ast) const
        {
            std::cout << '{' << std::endl;
            for (auto const& entry : ast.entries)
            {
                tab(indent+tabsize);
                std::cout << '"' << entry.first << "\" = ";
                boost::apply_visitor(rexpr_printer(indent+tabsize), entry.second);
            }
            tab(indent);
            std::cout << '}' << std::endl;
        }

        void operator()(std::string const& text) const
        {
            std::cout << '"' << text << '"' << std::endl;
        }

        void tab(int spaces) const
        {
            for (int i = 0; i < spaces; ++i)
                std::cout << ' ';
        }

        int indent;
    };
}}

///////////////////////////////////////////////////////////////////////////////
//  Our rexpr grammar
///////////////////////////////////////////////////////////////////////////////
namespace client { namespace parser
{
    namespace x3 = boost::spirit::x3;
    namespace ascii = boost::spirit::x3::ascii;

    using x3::lit;
    using x3::lexeme;

    using ascii::char_;
    using ascii::string;

    x3::rule<class rexpr_value, ast::rexpr_value>
        rexpr_value = "rexpr_value";

    x3::rule<class rexpr, ast::rexpr>
        rexpr = "rexpr";

    x3::rule<class rexpr_key_value, ast::rexpr_key_value>
        rexpr_key_value = "rexpr_key_value";

    auto const quoted_string =
        lexeme['"' >> *(char_ - '"') >> '"'];

    auto const rexpr_value_def =
        quoted_string | rexpr;

    auto const rexpr_key_value_def =
        quoted_string >> '=' >> rexpr_value;

    auto const rexpr_def =
        '{' >> *rexpr_key_value >> '}';

    BOOST_SPIRIT_DEFINE(rexpr_value, rexpr, rexpr_key_value);
}}

///////////////////////////////////////////////////////////////////////////////
//  Main program
///////////////////////////////////////////////////////////////////////////////
int main(int argc, char **argv)
{
    char const* filename;
    if (argc > 1)
    {
        filename = argv[1];
    }
    else
    {
        std::cerr << "Error: No input file provided." << std::endl;
        return 1;
    }

    std::ifstream in(filename, std::ios_base::in);

    if (!in)
    {
        std::cerr << "Error: Could not open input file: "
            << filename << std::endl;
        return 1;
    }

    std::string storage; // We will read the contents here.
    in.unsetf(std::ios::skipws); // No white space skipping!
    std::copy(
        std::istream_iterator<char>(in),
        std::istream_iterator<char>(),
        std::back_inserter(storage));

    using client::parser::rexpr; // Our grammar
    client::ast::rexpr ast; // Our tree

    using boost::spirit::x3::ascii::space;
    std::string::const_iterator iter = storage.begin();
    std::string::const_iterator end = storage.end();
    bool r = phrase_parse(iter, end, rexpr, space, ast);

    if (r && iter == end)
    {
        std::cout << "-------------------------\n";
        std::cout << "Parsing succeeded\n";
        std::cout << "-------------------------\n";
        client::ast::rexpr_printer printer;
        printer(ast);
        return 0;
    }
    else
    {
        std::string::const_iterator some = iter+30;
        std::string context(iter, (some>end)?end:some);
        std::cout << "-------------------------\n";
        std::cout << "Parsing failed\n";
        std::cout << "stopped at: \": " << context << "...\"\n";
        std::cout << "-------------------------\n";
        return 1;
    }
}
