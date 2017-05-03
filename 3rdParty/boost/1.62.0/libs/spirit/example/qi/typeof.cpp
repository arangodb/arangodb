/*=============================================================================
    Copyright (c) 2001-2010 Joel de Guzman

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
=============================================================================*/
#include <boost/config/warning_disable.hpp>
#include <boost/spirit/include/qi.hpp>
#include <boost/spirit/include/qi_copy.hpp>
#include <boost/spirit/include/support_auto.hpp>
#include <iostream>
#include <string>

///////////////////////////////////////////////////////////////////////////////
//  Main program
///////////////////////////////////////////////////////////////////////////////

int
main()
{
    using boost::spirit::ascii::space;
    using boost::spirit::ascii::char_;
    using boost::spirit::qi::parse;
    typedef std::string::const_iterator iterator_type;

///////////////////////////////////////////////////////////////////////////////
// this works for non-c++11 compilers
#ifdef BOOST_NO_CXX11_AUTO_DECLARATIONS

    BOOST_SPIRIT_AUTO(qi, comment, "/*" >> *(char_ - "*/") >> "*/");

///////////////////////////////////////////////////////////////////////////////
// but this is better for c++11 compilers with auto
#else

    using boost::spirit::qi::copy;

    auto comment = copy("/*" >> *(char_ - "*/") >> "*/");

#endif

    std::string str = "/*This is a comment*/";
    std::string::const_iterator iter = str.begin();
    std::string::const_iterator end = str.end();
    bool r = parse(iter, end, comment);

    if (r && iter == end)
    {
        std::cout << "-------------------------\n";
        std::cout << "Parsing succeeded\n";
        std::cout << "-------------------------\n";
    }
    else
    {
        std::string rest(iter, end);
        std::cout << "-------------------------\n";
        std::cout << "Parsing failed\n";
        std::cout << "stopped at: \": " << rest << "\"\n";
        std::cout << "-------------------------\n";
    }

    return 0;
}


