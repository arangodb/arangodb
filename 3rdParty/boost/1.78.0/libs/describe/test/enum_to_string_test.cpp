// Copyright 2021 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/describe/enum_to_string.hpp>
#include <boost/describe/enum.hpp>
#include <boost/core/lightweight_test.hpp>

#if !defined(BOOST_DESCRIBE_CXX14)

#include <boost/config/pragma_message.hpp>

BOOST_PRAGMA_MESSAGE("Skipping test because C++14 is not available")
int main() {}

#else

enum E1 { v1 };
BOOST_DESCRIBE_ENUM(E1, v1)

enum class E2 { v2 };
BOOST_DESCRIBE_ENUM(E2, v2)

BOOST_DEFINE_ENUM(E3, v3);
BOOST_DEFINE_ENUM_CLASS(E4, v4)

int main()
{
    using boost::describe::enum_to_string;

    BOOST_TEST_CSTR_EQ( enum_to_string( v1, "" ), "v1" );
    BOOST_TEST_CSTR_EQ( enum_to_string( E2::v2, "" ), "v2" );
    BOOST_TEST_CSTR_EQ( enum_to_string( static_cast<E2>( 14 ), "__def__" ), "__def__" );
    BOOST_TEST_CSTR_EQ( enum_to_string( v3, "" ), "v3" );
    BOOST_TEST_CSTR_EQ( enum_to_string( E4::v4, "" ), "v4" );
    BOOST_TEST_EQ( enum_to_string( static_cast<E4>( 14 ), 0 ), static_cast<char const*>( 0 ) );

    return boost::report_errors();
}

#endif // !defined(BOOST_DESCRIBE_CXX14)
