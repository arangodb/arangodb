// Copyright 2021 Peter Dimov.
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/system.hpp>
#include <boost/core/lightweight_test.hpp>
#include <cerrno>

namespace sys = boost::system;

enum E
{
    none = 0,
    einval = EINVAL
};

namespace boost
{
namespace system
{

template<> struct is_error_code_enum< ::E >
{
    static const bool value = true;
};

} // namespace system
} // namespace boost

sys::error_code make_error_code( E e )
{
    return e == 0? sys::error_code(): sys::error_code( e, sys::generic_category() );
}

#if defined(BOOST_SYSTEM_HAS_SYSTEM_ERROR)

enum E2
{
    e2inval = EINVAL
};

namespace boost
{
namespace system
{

template<> struct is_error_code_enum< ::E2 >
{
    static const bool value = true;
};

} // namespace system
} // namespace boost

std::error_code make_error_code( E2 e )
{
    return std::error_code( e, std::generic_category() );
}

#endif

int main()
{
    {
        sys::error_code ec( einval );

        BOOST_TEST_EQ( ec.value(), EINVAL );
        BOOST_TEST_EQ( &ec.category(), &sys::generic_category() );

        BOOST_TEST( ec.failed() );

        BOOST_TEST( !ec.has_location() );
        BOOST_TEST_EQ( ec.location().line(), 0 );
    }

    {
        BOOST_STATIC_CONSTEXPR boost::source_location loc = BOOST_CURRENT_LOCATION;

        sys::error_code ec( einval, &loc );

        BOOST_TEST_EQ( ec.value(), EINVAL );
        BOOST_TEST_EQ( &ec.category(), &sys::generic_category() );

        BOOST_TEST( ec.failed() );

        BOOST_TEST( ec.has_location() );
        BOOST_TEST_EQ( ec.location().line(), 77 );
    }

    {
        sys::error_code ec( none );

        BOOST_TEST_EQ( ec.value(), 0 );
        BOOST_TEST_EQ( &ec.category(), &sys::system_category() );

        BOOST_TEST( !ec.failed() );

        BOOST_TEST( !ec.has_location() );
        BOOST_TEST_EQ( ec.location().line(), 0 );
    }

    {
        BOOST_STATIC_CONSTEXPR boost::source_location loc = BOOST_CURRENT_LOCATION;

        sys::error_code ec( none, &loc );

        BOOST_TEST_EQ( ec.value(), 0 );
        BOOST_TEST_EQ( &ec.category(), &sys::system_category() );

        BOOST_TEST( !ec.failed() );

        BOOST_TEST( !ec.has_location() );
        BOOST_TEST_EQ( ec.location().line(), 0 );
    }

    {
        sys::error_code ec;

        BOOST_TEST_EQ( ec.value(), 0 );
        BOOST_TEST_EQ( &ec.category(), &sys::system_category() );

        BOOST_TEST( !ec.failed() );

        BOOST_TEST( !ec.has_location() );
        BOOST_TEST_EQ( ec.location().line(), 0 );

        BOOST_STATIC_CONSTEXPR boost::source_location loc = BOOST_CURRENT_LOCATION;

        ec = sys::error_code( einval, &loc );

        BOOST_TEST_EQ( ec.value(), EINVAL );
        BOOST_TEST_EQ( &ec.category(), &sys::generic_category() );

        BOOST_TEST( ec.failed() );

        BOOST_TEST( ec.has_location() );
        BOOST_TEST_EQ( ec.location().line(), 127 );
    }

    {
        sys::error_code ec;

        BOOST_TEST_EQ( ec.value(), 0 );
        BOOST_TEST_EQ( &ec.category(), &sys::system_category() );

        BOOST_TEST( !ec.failed() );

        BOOST_TEST( !ec.has_location() );
        BOOST_TEST_EQ( ec.location().line(), 0 );

        BOOST_STATIC_CONSTEXPR boost::source_location loc = BOOST_CURRENT_LOCATION;

        ec = sys::error_code( none, &loc );

        BOOST_TEST_EQ( ec.value(), 0 );
        BOOST_TEST_EQ( &ec.category(), &sys::system_category() );

        BOOST_TEST( !ec.failed() );

        BOOST_TEST( !ec.has_location() );
        BOOST_TEST_EQ( ec.location().line(), 0 );
    }

    {
        sys::error_code ec;

        BOOST_TEST_EQ( ec.value(), 0 );
        BOOST_TEST_EQ( &ec.category(), &sys::system_category() );

        BOOST_TEST( !ec.failed() );

        BOOST_TEST( !ec.has_location() );
        BOOST_TEST_EQ( ec.location().line(), 0 );

        BOOST_STATIC_CONSTEXPR boost::source_location loc = BOOST_CURRENT_LOCATION;

        ec.assign( einval, &loc );

        BOOST_TEST_EQ( ec.value(), EINVAL );
        BOOST_TEST_EQ( &ec.category(), &sys::generic_category() );

        BOOST_TEST( ec.failed() );

        BOOST_TEST( ec.has_location() );
        BOOST_TEST_EQ( ec.location().line(), 175 );
    }

    {
        sys::error_code ec;

        BOOST_TEST_EQ( ec.value(), 0 );
        BOOST_TEST_EQ( &ec.category(), &sys::system_category() );

        BOOST_TEST( !ec.failed() );

        BOOST_TEST( !ec.has_location() );
        BOOST_TEST_EQ( ec.location().line(), 0 );

        BOOST_STATIC_CONSTEXPR boost::source_location loc = BOOST_CURRENT_LOCATION;

        ec.assign( none, &loc );

        BOOST_TEST_EQ( ec.value(), 0 );
        BOOST_TEST_EQ( &ec.category(), &sys::system_category() );

        BOOST_TEST( !ec.failed() );

        BOOST_TEST( !ec.has_location() );
        BOOST_TEST_EQ( ec.location().line(), 0 );
    }

#if defined(BOOST_SYSTEM_HAS_SYSTEM_ERROR)

    {
        sys::error_code ec( e2inval );

        BOOST_TEST_EQ( ec, std::error_code( EINVAL, std::generic_category() ) );

        BOOST_TEST( ec.failed() );

        BOOST_TEST( !ec.has_location() );
        BOOST_TEST_EQ( ec.location().line(), 0 );
    }

    {
        BOOST_STATIC_CONSTEXPR boost::source_location loc = BOOST_CURRENT_LOCATION;

        sys::error_code ec( e2inval, &loc );

        BOOST_TEST_EQ( ec, std::error_code( EINVAL, std::generic_category() ) );

        BOOST_TEST( ec.failed() );

        BOOST_TEST( !ec.has_location() );
        BOOST_TEST_EQ( ec.location().line(), 0 );
    }

    {
        sys::error_code ec;

        BOOST_TEST_EQ( ec.value(), 0 );
        BOOST_TEST_EQ( &ec.category(), &sys::system_category() );

        BOOST_TEST( !ec.failed() );

        BOOST_TEST( !ec.has_location() );
        BOOST_TEST_EQ( ec.location().line(), 0 );

        BOOST_STATIC_CONSTEXPR boost::source_location loc = BOOST_CURRENT_LOCATION;

        ec = sys::error_code( e2inval, &loc );

        BOOST_TEST_EQ( ec, std::error_code( EINVAL, std::generic_category() ) );

        BOOST_TEST( ec.failed() );

        BOOST_TEST( !ec.has_location() );
        BOOST_TEST_EQ( ec.location().line(), 0 );
    }

    {
        sys::error_code ec;

        BOOST_TEST_EQ( ec.value(), 0 );
        BOOST_TEST_EQ( &ec.category(), &sys::system_category() );

        BOOST_TEST( !ec.failed() );

        BOOST_TEST( !ec.has_location() );
        BOOST_TEST_EQ( ec.location().line(), 0 );

        BOOST_STATIC_CONSTEXPR boost::source_location loc = BOOST_CURRENT_LOCATION;

        ec.assign( e2inval, &loc );

        BOOST_TEST_EQ( ec, std::error_code( EINVAL, std::generic_category() ) );

        BOOST_TEST( ec.failed() );

        BOOST_TEST( !ec.has_location() );
        BOOST_TEST_EQ( ec.location().line(), 0 );
    }

#endif

    return boost::report_errors();
}
