// Copyright 2020 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/describe/members.hpp>
#include <boost/describe/class.hpp>
#include <boost/core/lightweight_test.hpp>

struct A
{
    int m1;
    int m2;
    int m3;
};

BOOST_DESCRIBE_STRUCT(A, (), (m1, m2, m3))

class B: public A
{
public:

    static int m1;

    void m2() const {}

private:

    int m3;

    BOOST_DESCRIBE_CLASS(B, (A), (m1, m2), (), (m3))
};

int B::m1;

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
        using L = describe_members<B, mod_public | mod_inherited>;

        BOOST_TEST_EQ( mp_size<L>::value, 0 );
    }

    {
        using L = describe_members<B, mod_public | mod_inherited | mod_hidden>;

        BOOST_TEST_EQ( mp_size<L>::value, 3 );

        using D1 = mp_at_c<L, 0>;
        using D2 = mp_at_c<L, 1>;
        using D3 = mp_at_c<L, 2>;

        BOOST_TEST( D1::pointer == &B::A::m1 );
        BOOST_TEST_CSTR_EQ( D1::name, "m1" );
        BOOST_TEST_EQ( D1::modifiers, mod_public | mod_inherited | mod_hidden );

        BOOST_TEST( D2::pointer == &B::A::m2 );
        BOOST_TEST_CSTR_EQ( D2::name, "m2" );
        BOOST_TEST_EQ( D2::modifiers, mod_public | mod_inherited | mod_hidden );

        BOOST_TEST( D3::pointer == &B::A::m3 );
        BOOST_TEST_CSTR_EQ( D3::name, "m3" );
        BOOST_TEST_EQ( D3::modifiers, mod_public | mod_inherited | mod_hidden );
    }

    return boost::report_errors();
}

#endif // !defined(BOOST_DESCRIBE_CXX14)
