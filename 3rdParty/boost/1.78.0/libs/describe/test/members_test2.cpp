// Copyright 2020 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/describe/members.hpp>
#include <boost/describe/class.hpp>
#include <boost/core/lightweight_test.hpp>

#if !defined(BOOST_DESCRIBE_CXX11)

#include <boost/config/pragma_message.hpp>

BOOST_PRAGMA_MESSAGE("Skipping test because C++11 is not available")
int main() {}

#else

struct X
{
    static int m1;
};

int X::m1 = 1;

BOOST_DESCRIBE_STRUCT(X, (), (m1))

class Y
{
public:

    int m1 = 1;
    static int m2;

private:

    int m3 = 3;
    static int m4;

    BOOST_DESCRIBE_CLASS(Y, (), (m1, m2), (), (m3, m4))

public:

    using Pm = int Y::*;

    static Pm pm3() noexcept
    {
        return &Y::m3;
    }

    static int * pm4() noexcept
    {
        return &m4;
    }
};

int Y::m2 = 2;
int Y::m4 = 4;

#if !defined(BOOST_DESCRIBE_CXX14)

#include <boost/config/pragma_message.hpp>

BOOST_PRAGMA_MESSAGE("Skipping test because C++14 is not available")
int main() {}

#else

#include <boost/mp11.hpp>

int main()
{
    using namespace boost::describe;
    using namespace boost::mp11;

    {
        using L = describe_members<X, mod_any_access>;

        BOOST_TEST_EQ( mp_size<L>::value, 0 );
    }

    {
        using L = describe_members<X, mod_any_access | mod_static>;

        BOOST_TEST_EQ( mp_size<L>::value, 1 );

        using D1 = mp_at_c<L, 0>;

        BOOST_TEST( D1::pointer == &X::m1 );
        BOOST_TEST_CSTR_EQ( D1::name, "m1" );
        BOOST_TEST_EQ( D1::modifiers, mod_public | mod_static );
    }

    {
        using L = describe_members<Y, mod_any_access>;

        BOOST_TEST_EQ( mp_size<L>::value, 2 );

        using D1 = mp_at_c<L, 0>;
        using D2 = mp_at_c<L, 1>;

        BOOST_TEST( D1::pointer == &Y::m1 );
        BOOST_TEST_CSTR_EQ( D1::name, "m1" );
        BOOST_TEST_EQ( D1::modifiers, mod_public );

        BOOST_TEST( D2::pointer == Y::pm3() );
        BOOST_TEST_CSTR_EQ( D2::name, "m3" );
        BOOST_TEST_EQ( D2::modifiers, mod_private );
    }

    {
        using L = describe_members<Y, mod_any_access | mod_static>;

        BOOST_TEST_EQ( mp_size<L>::value, 2 );

        using D1 = mp_at_c<L, 0>;
        using D2 = mp_at_c<L, 1>;

        BOOST_TEST( D1::pointer == &Y::m2 );
        BOOST_TEST_CSTR_EQ( D1::name, "m2" );
        BOOST_TEST_EQ( D1::modifiers, mod_public | mod_static );

        BOOST_TEST( D2::pointer == Y::pm4() );
        BOOST_TEST_CSTR_EQ( D2::name, "m4" );
        BOOST_TEST_EQ( D2::modifiers, mod_private | mod_static );
    }

    {
        using L = describe_members<Y, mod_public>;

        BOOST_TEST_EQ( mp_size<L>::value, 1 );

        using D1 = mp_at_c<L, 0>;

        BOOST_TEST( D1::pointer == &Y::m1 );
        BOOST_TEST_CSTR_EQ( D1::name, "m1" );
        BOOST_TEST_EQ( D1::modifiers, mod_public );
    }

    {
        using L = describe_members<Y, mod_public | mod_static>;

        BOOST_TEST_EQ( mp_size<L>::value, 1 );

        using D1 = mp_at_c<L, 0>;

        BOOST_TEST( D1::pointer == &Y::m2 );
        BOOST_TEST_CSTR_EQ( D1::name, "m2" );
        BOOST_TEST_EQ( D1::modifiers, mod_public | mod_static );
    }

    {
        using L = describe_members<Y, mod_private>;

        BOOST_TEST_EQ( mp_size<L>::value, 1 );

        using D1 = mp_at_c<L, 0>;

        BOOST_TEST( D1::pointer == Y::pm3() );
        BOOST_TEST_CSTR_EQ( D1::name, "m3" );
        BOOST_TEST_EQ( D1::modifiers, mod_private );
    }

    {
        using L = describe_members<Y, mod_private | mod_static>;

        BOOST_TEST_EQ( mp_size<L>::value, 1 );

        using D1 = mp_at_c<L, 0>;

        BOOST_TEST( D1::pointer == Y::pm4() );
        BOOST_TEST_CSTR_EQ( D1::name, "m4" );
        BOOST_TEST_EQ( D1::modifiers, mod_private | mod_static );
    }

    {
        using L = describe_members<Y, mod_protected>;

        BOOST_TEST_EQ( mp_size<L>::value, 0 );
    }

    {
        using L = describe_members<Y, mod_protected | mod_static>;

        BOOST_TEST_EQ( mp_size<L>::value, 0 );
    }

    return boost::report_errors();
}

#endif // !defined(BOOST_DESCRIBE_CXX14)

#endif // !defined(BOOST_DESCRIBE_CXX11)
