// Copyright 2020 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/describe/members.hpp>
#include <boost/describe/class.hpp>
#include <boost/core/lightweight_test.hpp>

struct X
{
    int f() { return 1; }
    int f() const { return 2; }

    static int f( int x ) { return x; }
    static int f( int x, int y ) { return x + y; }
};

BOOST_DESCRIBE_STRUCT(X, (), (
    (int ()) f,
    (int () const) f,
    (int (int)) f,
    (int (int, int)) f
))

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
        using L = describe_members<X, mod_any_access | mod_function>;

        BOOST_TEST_EQ( mp_size<L>::value, 2 );

        using D1 = mp_at_c<L, 0>;
        using D2 = mp_at_c<L, 1>;

        X x;

        BOOST_TEST_EQ( (x.*D1::pointer)(), 1 );
        BOOST_TEST_CSTR_EQ( D1::name, "f" );
        BOOST_TEST_EQ( D1::modifiers, mod_public | mod_function );

        BOOST_TEST_EQ( (x.*D2::pointer)(), 2 );
        BOOST_TEST_CSTR_EQ( D2::name, "f" );
        BOOST_TEST_EQ( D2::modifiers, mod_public | mod_function );
    }

    {
        using L = describe_members<X, mod_any_access | mod_static | mod_function>;

        BOOST_TEST_EQ( mp_size<L>::value, 2 );

        using D1 = mp_at_c<L, 0>;

        using D1 = mp_at_c<L, 0>;
        using D2 = mp_at_c<L, 1>;

        BOOST_TEST_EQ( (*D1::pointer)( 3 ), 3 );
        BOOST_TEST_CSTR_EQ( D1::name, "f" );
        BOOST_TEST_EQ( D1::modifiers, mod_public | mod_static | mod_function );

        BOOST_TEST_EQ( (*D2::pointer)( 4, 5 ), 9 );
        BOOST_TEST_CSTR_EQ( D2::name, "f" );
        BOOST_TEST_EQ( D2::modifiers, mod_public | mod_static | mod_function );
    }

    return boost::report_errors();
}

#endif // !defined(BOOST_DESCRIBE_CXX14)
