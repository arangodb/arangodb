/*=============================================================================
    Copyright (c) 2001-2011 Hartmut Kaiser
    Copyright (c) 2011 Robert Nelson

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
=============================================================================*/

// These (compilation) tests verify that Proto's operator overloads do not 
// trigger the corresponding operator overloads exposed by Fusion.

#include <boost/fusion/tuple.hpp> 
#include <boost/spirit/include/qi_operator.hpp>
#include <boost/spirit/include/qi_eps.hpp>
#include <boost/spirit/include/qi_nonterminal.hpp>
#include <boost/phoenix/core/reference.hpp>
#include <string>

int main()
{
    namespace qi = boost::spirit::qi;

    static qi::rule<std::string::const_iterator> const a;
    static qi::rule<std::string::const_iterator> const b;
    qi::rule<std::string::const_iterator> rule = a > b; 

    int vars;
    qi::rule<std::string::const_iterator, int(const int&)> const r;
    qi::rule<std::string::const_iterator, int()> r2 = 
        r(boost::phoenix::ref(vars)) > qi::eps;
}
