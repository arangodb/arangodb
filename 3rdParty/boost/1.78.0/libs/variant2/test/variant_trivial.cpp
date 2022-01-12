// Copyright 2020 Peter Dimov.
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#if defined(_MSC_VER) && _MSC_VER < 1910
# pragma warning(disable: 4503) // decorated name length exceeded
#endif

#include <boost/variant2/variant.hpp>
#include <boost/core/lightweight_test_trait.hpp>
#include <boost/config.hpp>
#include <boost/config/workaround.hpp>

#include <boost/mp11.hpp>
using namespace boost::mp11;

//

struct D
{
    ~D() noexcept {}
};

struct CC1
{
    CC1( CC1 const& ) noexcept {}
};

struct CC2
{
    CC2( CC2 const& ) = delete;
};

struct MC1
{
    MC1( MC1 && ) noexcept {}
};

struct MC2
{
    MC2( MC2 && ) = delete;
};

struct CA1
{
    CA1& operator=( CA1 const& ) noexcept { return *this; }
};

struct CA2
{
    CA2& operator=( CA2 const& ) = delete;
};

struct MA1
{
    MA1& operator=( MA1 && ) noexcept { return *this; }
};

struct MA2
{
    MA2& operator=( MA2 && ) = delete;
};

using namespace boost::variant2;
namespace v2d = boost::variant2::detail;

struct test
{
    template<class... T> void operator()( mp_list<T...> ) const noexcept
    {
        using U = mp_inherit<T...>;

#if !BOOST_WORKAROUND( __GNUC__, < 5 )

        BOOST_TEST_EQ( v2d::is_trivially_copy_constructible<variant<U>>::value, v2d::is_trivially_copy_constructible<U>::value );
        BOOST_TEST_EQ( v2d::is_trivially_copy_assignable<variant<U>>::value, std::is_trivially_destructible<U>::value && v2d::is_trivially_copy_constructible<U>::value && v2d::is_trivially_copy_assignable<U>::value );

#endif

        BOOST_TEST_EQ( std::is_trivially_destructible<variant<U>>::value, std::is_trivially_destructible<U>::value );

#if !BOOST_WORKAROUND(BOOST_LIBSTDCXX_VERSION, < 50000)

        BOOST_TEST_EQ( v2d::is_trivially_move_constructible<variant<U>>::value, v2d::is_trivially_move_constructible<U>::value );
        BOOST_TEST_EQ( v2d::is_trivially_move_assignable<variant<U>>::value, std::is_trivially_destructible<U>::value && v2d::is_trivially_move_constructible<U>::value && v2d::is_trivially_move_assignable<U>::value );

#endif
    }
};

int main()
{
    mp_for_each< mp_power_set< mp_list<D, CC1, CC2, MC1, MC2, CA1, CA2, MA1, MA2> > >( test() );
    return boost::report_errors();
}
