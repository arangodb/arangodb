// Copyright 2021 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/describe/detail/pp_utilities.hpp>
#include <boost/describe/detail/config.hpp>
#include <boost/core/lightweight_test.hpp>

#if !defined(BOOST_DESCRIBE_CXX11)

#include <boost/config/pragma_message.hpp>

BOOST_PRAGMA_MESSAGE("Skipping test because C++11 is not available")
int main() {}

#else

#define S(x) S2(x)
#define S2(x) S3(x)
#define S3(x) #x

char const * s1 = S((BOOST_DESCRIBE_PP_PARSE(x)));
char const * s2 = S((BOOST_DESCRIBE_PP_PARSE(() y)));
char const * s3 = S((BOOST_DESCRIBE_PP_PARSE((a) z)));
char const * s4 = S((BOOST_DESCRIBE_PP_PARSE((a, b) w)));

int main()
{
    BOOST_TEST_CSTR_EQ( s1, "(0, ~, x)" );
    BOOST_TEST_CSTR_EQ( s2, "(1, (), y)" );
    BOOST_TEST_CSTR_EQ( s3, "(1, (a), z)" );
    BOOST_TEST_CSTR_EQ( s4, "(1, (a, b), w)" );

    return boost::report_errors();
}

#endif // !defined(BOOST_DESCRIBE_CXX11)
