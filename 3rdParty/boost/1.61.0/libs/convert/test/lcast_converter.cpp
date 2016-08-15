// Boost.Convert test and usage example
// Copyright (c) 2009-2014 Vladimir Batov.
// Use, modification and distribution are subject to the Boost Software License,
// Version 1.0. See http://www.boost.org/LICENSE_1_0.txt.

#include "./test.hpp"

#ifdef BOOST_CONVERT_INTEL_SFINAE_BROKEN
int main(int, char const* []) { return 0; }
#else

#include <boost/convert.hpp>
#include <boost/convert/lexical_cast.hpp>
#include <boost/detail/lightweight_test.hpp>

using std::string;
using boost::convert;

struct boost::cnv::by_default : public boost::cnv::lexical_cast {};

static
void
test_invalid()
{
    char const* str[] = { "not", "1 2", " 33", "44 ", "0x11", "7 + 5" };
    int const    size = sizeof(str) / sizeof(str[0]);

    for (int k = 0; k < size; ++k)
    {
        boost::optional<int> const res = convert<int>(str[k]);
        bool const              failed = !res;

        if (!failed)
        {
            printf("test::cnv::invalid() failed for: <%s><%d>\n", str[k], res.value());
        }
        BOOST_TEST(failed);
    }
}

static
void
test_dbl_to_str()
{
//    printf("100.00 %s\n", convert<string>( 99.999).value().c_str());
//    printf( "99.95 %s\n", convert<string>( 99.949).value().c_str());
//    printf("-99.95 %s\n", convert<string>(-99.949).value().c_str());
//    printf(  "99.9 %s\n", convert<string>( 99.949).value().c_str());
//    printf(  "1.00 %s\n", convert<string>(  0.999).value().c_str());
//    printf( "-1.00 %s\n", convert<string>( -0.999).value().c_str());
//    printf(  "0.95 %s\n", convert<string>(  0.949).value().c_str());
//    printf( "-0.95 %s\n", convert<string>( -0.949).value().c_str());
//    printf(   "1.9 %s\n", convert<string>(  1.949).value().c_str());
//    printf(  "-1.9 %s\n", convert<string>( -1.949).value().c_str());
}

int
main(int, char const* [])
{
    string const not_int_str = "not an int";
    string const     std_str = "-11";
    char const* const  c_str = "-12";
    int const            v00 = convert<int>(not_int_str).value_or(-1);
    int const            v01 = convert<int>(    std_str).value_or(-1);
    int const            v02 = convert<int>(      c_str).value_or(-1);

    BOOST_TEST(v00 ==  -1); // Failed conversion. No throw
    BOOST_TEST(v01 == -11);
    BOOST_TEST(v02 == -12);

    test_invalid();
    test_dbl_to_str();

    return boost::report_errors();
}

#endif
