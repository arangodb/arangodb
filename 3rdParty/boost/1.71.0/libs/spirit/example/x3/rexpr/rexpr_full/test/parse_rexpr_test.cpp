/*=============================================================================
    Copyright (c) 2001-2015 Joel de Guzman

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
=============================================================================*/
#include <iostream>
#include <iterator>
#include <algorithm>
#include <sstream>

#include "../rexpr/ast.hpp"
#include "../rexpr/rexpr.hpp"
#include "../rexpr/error_handler.hpp"
#include "../rexpr/config.hpp"
#include "../rexpr/printer.hpp"

#include "testing.hpp"

namespace fs = boost::filesystem;
namespace testing = boost::spirit::x3::testing;

auto parse = [](std::string const& source, fs::path input_path)-> std::string
{
    std::stringstream out;

    using rexpr::parser::iterator_type;
    iterator_type iter(source.begin());
    iterator_type const end(source.end());

    // Our AST
    rexpr::ast::rexpr ast;

    // Our error handler
    using boost::spirit::x3::with;
    using rexpr::parser::error_handler_type;
    using rexpr::parser::error_handler_tag;
    error_handler_type error_handler(iter, end, out, input_path.string()); // Our error handler

    // Our parser
    auto const parser =
        // we pass our error handler to the parser so we can access
        // it later on in our on_error and on_sucess handlers
        with<error_handler_tag>(std::ref(error_handler))
        [
            rexpr::rexpr()
        ];

    // Go forth and parse!
    using boost::spirit::x3::ascii::space;
    bool success = phrase_parse(iter, end, parser, space, ast);

    if (success)
    {
        if (iter != end)
            error_handler(iter, "Error! Expecting end of input here: ");
        else
            rexpr::ast::rexpr_printer{out}(ast);
    }

    return out.str();
};

int num_files_tested = 0;
auto compare = [](fs::path input_path, fs::path expect_path)
{
   testing::compare(input_path, expect_path, parse);
   ++num_files_tested;
};

int main(int argc, char* argv[])
{
    if (argc < 2)
    {
       std::cout << "usage: " << fs::path(argv[0]).filename() << " path/to/test/files" << std::endl;
       return -1;
    }

    std::cout << "===================================================================================================" << std::endl;
    std::cout << "Testing: " << fs::absolute(fs::path(argv[1])) << std::endl;
    int r = testing::for_each_file(fs::path(argv[1]), compare);
    if (r == 0)
        std::cout << num_files_tested << " files tested." << std::endl;
    std::cout << "===================================================================================================" << std::endl;
    return r;
}
