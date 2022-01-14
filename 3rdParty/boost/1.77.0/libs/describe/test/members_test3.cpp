// Copyright 2020 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/describe/members.hpp>
#include <boost/describe/class.hpp>
#include <boost/core/lightweight_test.hpp>

struct X
{
    void f() {}
    int g() const { return 1; }
    static void h() {}
};

BOOST_DESCRIBE_STRUCT(X, (), (f, g, h))

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
        using L = describe_members<X, mod_protected | mod_function>;
        BOOST_TEST_EQ( mp_size<L>::value, 0 );
    }

    {
        using L = describe_members<X, mod_private | mod_function>;
        BOOST_TEST_EQ( mp_size<L>::value, 0 );
    }

    {
        using L = describe_members<X, mod_public | mod_function>;

        BOOST_TEST_EQ( mp_size<L>::value, 2 );

        using D1 = mp_at_c<L, 0>;
        using D2 = mp_at_c<L, 1>;

        BOOST_TEST( D1::pointer == &X::f );
        BOOST_TEST_CSTR_EQ( D1::name, "f" );
        BOOST_TEST_EQ( D1::modifiers, mod_public | mod_function );

        BOOST_TEST( D2::pointer == &X::g );
        BOOST_TEST_CSTR_EQ( D2::name, "g" );
        BOOST_TEST_EQ( D2::modifiers, mod_public | mod_function );
    }

    {
        using L = describe_members<X, mod_public | mod_static | mod_function>;

        BOOST_TEST_EQ( mp_size<L>::value, 1 );

        using D1 = mp_at_c<L, 0>;

        BOOST_TEST( D1::pointer == &X::h );
        BOOST_TEST_CSTR_EQ( D1::name, "h" );
        BOOST_TEST_EQ( D1::modifiers, mod_public | mod_static | mod_function );
    }

    return boost::report_errors();
}

#endif // !defined(BOOST_DESCRIBE_CXX14)
