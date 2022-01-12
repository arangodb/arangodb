// Copyright 2020, 2021 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/describe/bases.hpp>
#include <boost/describe/members.hpp>
#include <boost/describe/class.hpp>
#include <boost/core/lightweight_test.hpp>
#include <boost/config.hpp>

template<class A1, class A2> struct X
{
    A1 a1;
    A2 a2;

    BOOST_DESCRIBE_CLASS(X, (), (a1, a2), (), ())
};

template<class B1, class B2> class Y
{
private:

    B1 b1;
    B2 b2;

    BOOST_DESCRIBE_CLASS(Y, (), (), (), (b1, b2))
};

template<class T1, class T2> class Z: public X<T1, T2>
{
private:

    // error: declaration of 'typedef struct X<T1, T2> Z<T1, T2>::X' changes meaning of 'X' (g++)
    // typedef X<T1, T2> X;

    typedef X<T1, T2> XB;

protected:

    Y<T1, T2> y;

    BOOST_DESCRIBE_CLASS(Z, (XB), (), (y), ())
};

#if !defined(BOOST_DESCRIBE_CXX14)

#include <boost/config/pragma_message.hpp>

BOOST_PRAGMA_MESSAGE("Skipping test because C++14 is not available")
int main() {}

#elif defined(BOOST_MSVC) && BOOST_MSVC < 1920

BOOST_PRAGMA_MESSAGE("Skipping test because BOOST_MSVC is below 1920")
int main() {}

#elif defined(BOOST_MSVC) && BOOST_MSVC >= 1920 && BOOST_MSVC < 1940 && _MSVC_LANG <= 201703L

BOOST_PRAGMA_MESSAGE("Skipping test because BOOST_MSVC is 192x or 193x and _MSVC_LANG is 201703L or below")
int main() {}

#else

#include <boost/mp11.hpp>
using namespace boost::mp11;

int main()
{
    using namespace boost::describe;
    using namespace boost::mp11;

    {
        using L = describe_members<X<int, float>, mod_any_access>;

        BOOST_TEST_EQ( mp_size<L>::value, 2 );

        using D1 = mp_at_c<L, 0>;
        using D2 = mp_at_c<L, 1>;

        BOOST_TEST( D1::pointer == (&X<int, float>::a1) );
        BOOST_TEST_CSTR_EQ( D1::name, "a1" );
        BOOST_TEST_EQ( D1::modifiers, mod_public );

        BOOST_TEST( D2::pointer == (&X<int, float>::a2) );
        BOOST_TEST_CSTR_EQ( D2::name, "a2" );
        BOOST_TEST_EQ( D2::modifiers, mod_public );
    }

    {
        using L = describe_members<Y<int, float>, mod_any_access>;

        BOOST_TEST_EQ( mp_size<L>::value, 2 );

        using D1 = mp_at_c<L, 0>;
        using D2 = mp_at_c<L, 1>;

        // BOOST_TEST( D1::pointer == (&Y<int, float>::b1) );
        BOOST_TEST_CSTR_EQ( D1::name, "b1" );
        BOOST_TEST_EQ( D1::modifiers, mod_private );

        // BOOST_TEST( D2::pointer == (&Y<int, float>::b2) );
        BOOST_TEST_CSTR_EQ( D2::name, "b2" );
        BOOST_TEST_EQ( D2::modifiers, mod_private );
    }

    {
        using L = describe_members<Z<int, float>, mod_any_access>;

        BOOST_TEST_EQ( mp_size<L>::value, 1 );

        using D1 = mp_at_c<L, 0>;

        // BOOST_TEST( D1::pointer == (&Z<int, float>::y) );
        BOOST_TEST_CSTR_EQ( D1::name, "y" );
        BOOST_TEST_EQ( D1::modifiers, mod_protected );
    }

    {
        using L = describe_members<Z<int, float>, mod_any_access | mod_inherited>;

        BOOST_TEST_EQ( mp_size<L>::value, 3 );

        using D1 = mp_at_c<L, 0>;
        using D2 = mp_at_c<L, 1>;
        using D3 = mp_at_c<L, 2>;

        BOOST_TEST( D1::pointer == (&X<int, float>::a1) );
        BOOST_TEST_CSTR_EQ( D1::name, "a1" );
        BOOST_TEST_EQ( D1::modifiers, mod_public | mod_inherited );

        BOOST_TEST( D2::pointer == (&X<int, float>::a2) );
        BOOST_TEST_CSTR_EQ( D2::name, "a2" );
        BOOST_TEST_EQ( D2::modifiers, mod_public | mod_inherited );

        // BOOST_TEST( D3::pointer == (&Z<int, float>::y) );
        BOOST_TEST_CSTR_EQ( D3::name, "y" );
        BOOST_TEST_EQ( D3::modifiers, mod_protected );
    }

    return boost::report_errors();
}

#endif // !defined(BOOST_DESCRIBE_CXX14)
