// Copyright 2020 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/describe/members.hpp>
#include <boost/describe/class.hpp>
#include <boost/core/lightweight_test.hpp>

struct A1
{
    int m1;
};

BOOST_DESCRIBE_STRUCT(A1, (), (m1))

struct A2
{
    int m1;
    int m2;
};

BOOST_DESCRIBE_STRUCT(A2, (), (m1, m2))

struct B: A1, A2
{
    int m1;
};

BOOST_DESCRIBE_STRUCT(B, (A1, A2), (m1))

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
        using L = describe_members<B, mod_any_access | mod_inherited | mod_hidden>;

        BOOST_TEST_EQ( mp_size<L>::value, 4 );

        using D1 = mp_at_c<L, 0>;
        using D2 = mp_at_c<L, 1>;
        using D3 = mp_at_c<L, 2>;
        using D4 = mp_at_c<L, 3>;

        BOOST_TEST( D1::pointer == &B::A1::m1 );
        BOOST_TEST_CSTR_EQ( D1::name, "m1" );
        BOOST_TEST_EQ( D1::modifiers, mod_public | mod_inherited | mod_hidden );

        BOOST_TEST( D2::pointer == &B::A2::m1 );
        BOOST_TEST_CSTR_EQ( D2::name, "m1" );
        BOOST_TEST_EQ( D2::modifiers, mod_public | mod_inherited | mod_hidden );

        BOOST_TEST( D3::pointer == &B::A2::m2 );
        BOOST_TEST_CSTR_EQ( D3::name, "m2" );
        BOOST_TEST_EQ( D3::modifiers, mod_public | mod_inherited );

        BOOST_TEST( D4::pointer == &B::m1 );
        BOOST_TEST_CSTR_EQ( D4::name, "m1" );
        BOOST_TEST_EQ( D4::modifiers, mod_public );
    }

    {
        using L = describe_members<B, mod_any_access | mod_inherited>;

        BOOST_TEST_EQ( mp_size<L>::value, 2 );

        using D1 = mp_at_c<L, 0>;
        using D2 = mp_at_c<L, 1>;

        BOOST_TEST( D1::pointer == &B::m2 );
        BOOST_TEST_CSTR_EQ( D1::name, "m2" );
        BOOST_TEST_EQ( D1::modifiers, mod_public | mod_inherited );

        BOOST_TEST( D2::pointer == &B::m1 );
        BOOST_TEST_CSTR_EQ( D2::name, "m1" );
        BOOST_TEST_EQ( D2::modifiers, mod_public );
    }

    return boost::report_errors();
}

#endif // !defined(BOOST_DESCRIBE_CXX14)
