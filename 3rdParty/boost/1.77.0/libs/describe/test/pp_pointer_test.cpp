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
#define S3(x) S4(x)
#define S4(x) #x

char const * s1 = S(BOOST_DESCRIBE_PP_POINTER(C1, x));
char const * s2 = S((BOOST_DESCRIBE_PP_POINTER(C2, (R ()) y)));
char const * s3 = S((BOOST_DESCRIBE_PP_POINTER(C3, (R () const) z)));
char const * s4 = S((BOOST_DESCRIBE_PP_POINTER(C4, (R (A1)) v)));
char const * s5 = S((BOOST_DESCRIBE_PP_POINTER(C5, (R (A1, A2) const &&) w)));

int main()
{
    BOOST_TEST_CSTR_EQ( s1, "&C1::x" );
    BOOST_TEST_CSTR_EQ( s2, "(::boost::describe::detail::mfn<C2, R ()>(&C2::y))" );
    BOOST_TEST_CSTR_EQ( s3, "(::boost::describe::detail::mfn<C3, R () const>(&C3::z))" );
    BOOST_TEST_CSTR_EQ( s4, "(::boost::describe::detail::mfn<C4, R (A1)>(&C4::v))" );
    BOOST_TEST_CSTR_EQ( s5, "(::boost::describe::detail::mfn<C5, R (A1, A2) const &&>(&C5::w))" );

    return boost::report_errors();
}

#endif // !defined(BOOST_DESCRIBE_CXX11)
