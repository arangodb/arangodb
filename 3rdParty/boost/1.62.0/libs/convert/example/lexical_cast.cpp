// Copyright (c) 2009-2014 Vladimir Batov.
// Use, modification and distribution are subject to the Boost Software License,
// Version 1.0. See http://www.boost.org/LICENSE_1_0.txt.

//[lexical_cast_headers1
#include <boost/convert.hpp>
#include <boost/convert/lexical_cast.hpp>
#include <boost/detail/lightweight_test.hpp>

using std::string;
using boost::convert;
using boost::lexical_cast;

struct boost::cnv::by_default : public boost::cnv::lexical_cast {};
//]

int
main(int, char const* [])
{
    //[lexical_cast_example1
    int    i1 = lexical_cast<int>("123");         // Throws if the conversion fails.
    int    i2 = convert<int>("123").value();      // Throws if the conversion fails.
    int    i3 = convert<int>("uhm").value_or(-1); // Returns -1 if the conversion fails.
    string s1 = lexical_cast<string>(123);
    string s2 = convert<string>(123).value();

    BOOST_TEST(i1 == 123);
    BOOST_TEST(i2 == 123);
    BOOST_TEST(i3 == -1);
    BOOST_TEST(s1 == "123");
    BOOST_TEST(s2 == "123");
    //]

    return boost::report_errors();
}
