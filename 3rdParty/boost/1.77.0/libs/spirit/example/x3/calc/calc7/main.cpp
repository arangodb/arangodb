/*=============================================================================
    Copyright (c) 2001-2014 Joel de Guzman

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
=============================================================================*/
///////////////////////////////////////////////////////////////////////////////
//
//  Same as calc6, but this version also shows off grammar modularization.
//  Here you will see how expressions is built as a modular grammars.
//
//  [ JDG Sometime 2000 ]       pre-boost
//  [ JDG September 18, 2002 ]  spirit1
//  [ JDG April 8, 2007 ]       spirit2
//  [ JDG February 18, 2011 ]   Pure attributes. No semantic actions.
//  [ JDG April 9, 2014 ]       Spirit X3 (from qi calc6)
//  [ JDG May 2, 2014 ]         Modular grammar using BOOST_SPIRIT_DEFINE.
//
///////////////////////////////////////////////////////////////////////////////

#include "ast.hpp"
#include "vm.hpp"
#include "compiler.hpp"
#include "expression.hpp"
#include "error_handler.hpp"

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
    typedef client::ast::expression ast_expression;
    typedef client::compiler compiler;

    std::string str;
    while (std::getline(std::cin, str))
    {
        if (str.empty() || str[0] == 'q' || str[0] == 'Q')
            break;

        using boost::spirit::x3::ascii::space;

        client::vmachine mach;                          // Our virtual machine
        std::vector<int> code;                          // Our VM code
        auto calc = client::expression();               // grammar

        ast_expression ast;                             // Our program (AST)
        compiler compile(code);                         // Compiles the program

        iterator_type iter = str.begin();
        iterator_type const end = str.end();
        bool r = phrase_parse(iter, end, calc, space, ast);

        if (r && iter == end)
        {
            std::cout << "-------------------------\n";
            std::cout << "Parsing succeeded\n";
            compile(ast);
            mach.execute(code);
            std::cout << "\nResult: " << mach.top() << std::endl;
            std::cout << "-------------------------\n";
        }
        else
        {
            std::string rest(iter, end);
            std::cout << "-------------------------\n";
            std::cout << "Parsing failed\n";
            std::cout << "-------------------------\n";
        }
    }

    std::cout << "Bye... :-) \n\n";
    return 0;
}
