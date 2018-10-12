
// Copyright 2018 Peter Dimov.
// Distributed under the Boost Software License, Version 1.0.

#include <boost/system/error_code.hpp>
#include <boost/core/lightweight_test.hpp>
#include <boost/config/pragma_message.hpp>
#include <boost/static_assert.hpp>
#include <boost/config.hpp>
#include <boost/config/workaround.hpp>

#if !defined(BOOST_SYSTEM_HAS_CONSTEXPR)

BOOST_PRAGMA_MESSAGE("Skipping constexpr test, BOOST_SYSTEM_HAS_CONSTEXPR isn't defined")
int main() {}

#else

using namespace boost::system;

constexpr error_code e1( 1, system_category() );

BOOST_STATIC_ASSERT( e1.value() == 1 );
BOOST_STATIC_ASSERT( e1.category() == system_category() );
BOOST_STATIC_ASSERT( e1 );
BOOST_STATIC_ASSERT( e1 == e1 );

constexpr error_code e2( 2, generic_category() );

BOOST_STATIC_ASSERT( e2.value() == 2 );
BOOST_STATIC_ASSERT( e2.category() == generic_category() );
BOOST_STATIC_ASSERT( e2 );
BOOST_STATIC_ASSERT( e2 == e2 );

#if !BOOST_WORKAROUND(BOOST_GCC, < 80200)

BOOST_STATIC_ASSERT( e1 != e2 );

#endif

constexpr error_code e3;

BOOST_STATIC_ASSERT( e3.value() == 0 );
BOOST_STATIC_ASSERT( e3.category() == system_category() );
BOOST_STATIC_ASSERT( !e3 );
BOOST_STATIC_ASSERT( e3 == e3 );

#if !BOOST_WORKAROUND(BOOST_GCC, < 80200)

BOOST_STATIC_ASSERT( e1 != e3 );

#endif

int main()
{
    error_code e1_( 1, system_category() );
    BOOST_TEST_EQ( e1, e1_ );

    error_code e2_( 2, generic_category() );
    BOOST_TEST_EQ( e2, e2_ );

    error_code e3_;
    BOOST_TEST_EQ( e3, e3_ );

    return boost::report_errors();
}

#endif
