// Boost.Convert test and usage example
// Copyright (c) 2009-2016 Vladimir Batov.
// Use, modification and distribution are subject to the Boost Software License,
// Version 1.0. See http://www.boost.org/LICENSE_1_0.txt.

#include <boost/convert.hpp>
#include <boost/convert/stream.hpp>
#include <boost/detail/lightweight_test.hpp>

#ifdef ONLY_FOR_DEMONSTRATION_PURPOSES
//[default_converter_declaration_simple
struct boost::cnv::by_default : boost::cnv::cstream {};
//]
#endif
//[default_converter_declaration_formatted
struct boost::cnv::by_default : boost::cnv::cstream 
{ 
    by_default() { (*this)(std::uppercase)(std::hex); }
};
//]

int
main(int, char const* [])
{
    //[default_converter_example1
    // No explicit converter provided. boost::cnv::by_default is used.
    int         i = boost::convert<int>("F").value_or(-1);
    std::string s = boost::convert<std::string>(255).value_or("bad");

    // 'i' and 's' are converted using boost::cnv::cstream
    // with std::uppercase and std::hex formatting applied.

    BOOST_TEST(i == 15);   // 15(10) = F(16)
    BOOST_TEST(s == "FF"); // 255(10) = FF(16)
    //]

    return boost::report_errors();
}

