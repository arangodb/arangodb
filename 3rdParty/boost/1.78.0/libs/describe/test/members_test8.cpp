// Copyright 2020, 2021 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/describe/members.hpp>
#include <boost/describe/class.hpp>
#include <boost/core/lightweight_test.hpp>

struct A
{
    int m1;
    static int m2;
    int f1() const { return m1; }
    static int f2() { return m2; }
};

BOOST_DESCRIBE_STRUCT(A, (), (m1, m2, f1, f2))

int A::m2;

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
        using L = describe_members<A, mod_any_access | mod_any_member>;

        BOOST_TEST_EQ( mp_size<L>::value, 4 );

        using D1 = mp_at_c<L, 0>;
        using D2 = mp_at_c<L, 1>;
        using D3 = mp_at_c<L, 2>;
        using D4 = mp_at_c<L, 3>;

        BOOST_TEST( D1::pointer == &A::m1 );
        BOOST_TEST_CSTR_EQ( D1::name, "m1" );
        BOOST_TEST_EQ( D1::modifiers, mod_public );

        BOOST_TEST( D2::pointer == &A::m2 );
        BOOST_TEST_CSTR_EQ( D2::name, "m2" );
        BOOST_TEST_EQ( D2::modifiers, mod_public | mod_static );

        BOOST_TEST( D3::pointer == &A::f1 );
        BOOST_TEST_CSTR_EQ( D3::name, "f1" );
        BOOST_TEST_EQ( D3::modifiers, mod_public | mod_function );

        BOOST_TEST( D4::pointer == &A::f2 );
        BOOST_TEST_CSTR_EQ( D4::name, "f2" );
        BOOST_TEST_EQ( D4::modifiers, mod_public | mod_static | mod_function );
    }

    return boost::report_errors();
}

#endif // !defined(BOOST_DESCRIBE_CXX14)
