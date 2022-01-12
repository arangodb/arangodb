//  Copyright (c) 2013 Andreas Pokorny
//
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/spirit/include/lex_lexertl.hpp>

#include <boost/core/lightweight_test.hpp>
#include <boost/phoenix/core.hpp>
#include <boost/phoenix/operator.hpp>

#include <fstream>

using namespace std;
using namespace boost::spirit;

template <typename BaseLexer>
struct test_lexer : boost::spirit::lex::lexer<BaseLexer>
{
    test_lexer()
    {
        this->self = lex::string("just something")
            [
                lex::_end = lex::less(boost::phoenix::val(1))
            ]
            ;
    }
};

int main()
{
    typedef lex::lexertl::token<char const*> token_type;
    typedef lex::lexertl::actor_lexer<token_type> lexer_type;

    test_lexer<lexer_type> lexer;

    return boost::report_errors();
}
