// Copyright 2020 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/describe/members.hpp>
#include <boost/describe/class.hpp>
#include <boost/core/lightweight_test.hpp>
#include <boost/config.hpp>
#include <boost/config/workaround.hpp>

struct A
{
    int m1;
};

BOOST_DESCRIBE_STRUCT(A, (), (m1))

struct B1: public virtual A
{
    int m2;
};

BOOST_DESCRIBE_STRUCT(B1, (A), (m2))

struct B2: public virtual A
{
    int m2;
};

BOOST_DESCRIBE_STRUCT(B2, (A), (m2))

struct C: public B1, public B2
{
    int m2;
};

BOOST_DESCRIBE_STRUCT(C, (B1, B2), (m2))

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
        using L = describe_members<C, mod_public>;

        BOOST_TEST_EQ( mp_size<L>::value, 1 );

        using D1 = mp_at_c<L, 0>;

#if !BOOST_WORKAROUND(BOOST_MSVC, < 1940)
// https://developercommunity.visualstudio.com/content/problem/1186002/constexpr-pointer-to-member-has-incorrect-value.html
        BOOST_TEST( D1::pointer == &C::m2 );
#endif
        BOOST_TEST_CSTR_EQ( D1::name, "m2" );
        BOOST_TEST_EQ( D1::modifiers, mod_public );
    }

    {
        using L = describe_members<C, mod_public | mod_inherited>;

        BOOST_TEST_EQ( mp_size<L>::value, 2 );

        using D1 = mp_at_c<L, 0>;
        using D2 = mp_at_c<L, 1>;

        BOOST_TEST( D1::pointer == &C::A::m1 );
        BOOST_TEST_CSTR_EQ( D1::name, "m1" );
        BOOST_TEST_EQ( D1::modifiers, mod_public | mod_inherited );

#if !BOOST_WORKAROUND(BOOST_MSVC, < 1940)
        BOOST_TEST( D2::pointer == &C::m2 );
#endif
        BOOST_TEST_CSTR_EQ( D2::name, "m2" );
        BOOST_TEST_EQ( D2::modifiers, mod_public );
    }

    return boost::report_errors();
}

#endif // !defined(BOOST_DESCRIBE_CXX14)
