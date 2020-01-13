
// Copyright 2018 Peter Dimov.
// Distributed under the Boost Software License, Version 1.0.

#include <boost/system/error_code.hpp>
#include <boost/config/pragma_message.hpp>
#include <boost/static_assert.hpp>

#if !defined(BOOST_SYSTEM_HAS_CONSTEXPR)

BOOST_PRAGMA_MESSAGE("Skipping constexpr test, BOOST_SYSTEM_HAS_CONSTEXPR isn't defined")
int main() {}

#else

using namespace boost::system;

constexpr error_code ec1( 1, system_category() );

BOOST_STATIC_ASSERT( ec1.failed() );
BOOST_STATIC_ASSERT( ec1 );
BOOST_STATIC_ASSERT( !!ec1 );

constexpr error_code ec2( 2, generic_category() );

BOOST_STATIC_ASSERT( ec2.failed() );
BOOST_STATIC_ASSERT( ec2 );
BOOST_STATIC_ASSERT( !!ec2 );

constexpr error_code ec3;

BOOST_STATIC_ASSERT( !ec3.failed() );
BOOST_STATIC_ASSERT( ec3? false: true );
BOOST_STATIC_ASSERT( !ec3 );

constexpr error_condition en1( 1, system_category() );

BOOST_STATIC_ASSERT( en1.failed() );
BOOST_STATIC_ASSERT( en1 );
BOOST_STATIC_ASSERT( !!en1 );

constexpr error_condition en2( 2, generic_category() );

BOOST_STATIC_ASSERT( en2.failed() );
BOOST_STATIC_ASSERT( en2 );
BOOST_STATIC_ASSERT( !!en2 );

constexpr error_condition en3;

BOOST_STATIC_ASSERT( !en3.failed() );
BOOST_STATIC_ASSERT( en3? false: true );
BOOST_STATIC_ASSERT( !en3 );

int main()
{
}

#endif
