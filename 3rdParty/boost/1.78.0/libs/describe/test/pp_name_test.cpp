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

char const * s1 = BOOST_DESCRIBE_PP_NAME(x);
char const * s2 = BOOST_DESCRIBE_PP_NAME(() y);
char const * s3 = BOOST_DESCRIBE_PP_NAME((a) z);
char const * s4 = BOOST_DESCRIBE_PP_NAME((a, b) w);

int main()
{
    BOOST_TEST_CSTR_EQ( s1, "x" );
    BOOST_TEST_CSTR_EQ( s2, "y" );
    BOOST_TEST_CSTR_EQ( s3, "z" );
    BOOST_TEST_CSTR_EQ( s4, "w" );

    return boost::report_errors();
}

#endif // !defined(BOOST_DESCRIBE_CXX11)
