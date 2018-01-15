//
// atomic_sp_constexpr_test.cpp
//
// Copyright 2017 Peter Dimov
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//

#include <boost/config.hpp>
#include <boost/detail/workaround.hpp>

#define HAVE_CONSTEXPR_INIT

#if defined( BOOST_NO_CXX11_CONSTEXPR )
# undef HAVE_CONSTEXPR_INIT
#endif

#if BOOST_WORKAROUND( BOOST_MSVC, < 1920 )
# undef HAVE_CONSTEXPR_INIT
#endif

#if defined(__clang__) && defined( BOOST_NO_CXX14_CONSTEXPR )
# undef HAVE_CONSTEXPR_INIT
#endif

#if defined( _LIBCPP_VERSION ) && ( _LIBCPP_VERSION < 5000 )
// in libc++, atomic_flag has a non-constexpr constructor from bool
# undef HAVE_CONSTEXPR_INIT
#endif

#if !defined( HAVE_CONSTEXPR_INIT ) || defined( BOOST_NO_CXX11_UNIFIED_INITIALIZATION_SYNTAX )

int main()
{
}

#else

#include <boost/smart_ptr/atomic_shared_ptr.hpp>
#include <boost/core/lightweight_test.hpp>

struct X
{
};

struct Z
{
    Z();
};

static Z z;

static boost::atomic_shared_ptr<X> p1;

Z::Z()
{
    p1 = boost::shared_ptr<X>( new X );
}

int main()
{
    boost::shared_ptr<X> p2 = p1;

    BOOST_TEST( p2.get() != 0 );
    BOOST_TEST_EQ( p2.use_count(), 2 );

    return boost::report_errors();
}

#endif // #if defined( BOOST_NO_CXX11_CONSEXPR )
