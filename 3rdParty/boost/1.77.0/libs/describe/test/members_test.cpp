// Copyright 2020 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/describe/members.hpp>
#include <boost/describe/class.hpp>
#include <boost/core/lightweight_test.hpp>
#include <boost/config.hpp>

struct X
{
};

BOOST_DESCRIBE_STRUCT(X, (), ())

class Y
{
public:

    int m1;
    int m2;

private:

    int m3;
    int m4;

    BOOST_DESCRIBE_CLASS(Y, (), (m1, m2), (), (m3, m4))

public:

    Y( int m1, int m2, int m3, int m4 ): m1( m1 ), m2( m2 ), m3( m3 ), m4( m4 )
    {
    }

    // using Pm = int Y::*;
    typedef int Y::* Pm;

    static Pm m3_pointer() BOOST_NOEXCEPT
    {
        return &Y::m3;
    }

    static Pm m4_pointer() BOOST_NOEXCEPT
    {
        return &Y::m4;
    }
};

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
        using L = describe_members<Y, mod_any_access>;

        BOOST_TEST_EQ( mp_size<L>::value, 4 );

        BOOST_TEST( (mp_at_c<L, 0>::pointer) == &Y::m1 );
        BOOST_TEST_CSTR_EQ( (mp_at_c<L, 0>::name), "m1" );
        BOOST_TEST_EQ( (mp_at_c<L, 0>::modifiers), mod_public );

        BOOST_TEST( (mp_at_c<L, 1>::pointer) == &Y::m2 );
        BOOST_TEST_CSTR_EQ( (mp_at_c<L, 1>::name), "m2" );
        BOOST_TEST_EQ( (mp_at_c<L, 1>::modifiers), mod_public );

        BOOST_TEST( (mp_at_c<L, 2>::pointer) == Y::m3_pointer() );
        BOOST_TEST_CSTR_EQ( (mp_at_c<L, 2>::name), "m3" );
        BOOST_TEST_EQ( (mp_at_c<L, 2>::modifiers), mod_private );

        BOOST_TEST( (mp_at_c<L, 3>::pointer) == Y::m4_pointer() );
        BOOST_TEST_CSTR_EQ( (mp_at_c<L, 3>::name), "m4" );
        BOOST_TEST_EQ( (mp_at_c<L, 3>::modifiers), mod_private );
    }

    {
        using L = describe_members<Y, mod_public>;

        BOOST_TEST_EQ( mp_size<L>::value, 2 );

        BOOST_TEST( (mp_at_c<L, 0>::pointer) == &Y::m1 );
        BOOST_TEST_CSTR_EQ( (mp_at_c<L, 0>::name), "m1" );
        BOOST_TEST_EQ( (mp_at_c<L, 0>::modifiers), mod_public );

        BOOST_TEST( (mp_at_c<L, 1>::pointer) == &Y::m2 );
        BOOST_TEST_CSTR_EQ( (mp_at_c<L, 1>::name), "m2" );
        BOOST_TEST_EQ( (mp_at_c<L, 1>::modifiers), mod_public );
    }

    {
        using L = describe_members<Y, mod_private>;

        BOOST_TEST_EQ( mp_size<L>::value, 2 );

        BOOST_TEST( (mp_at_c<L, 0>::pointer) == Y::m3_pointer() );
        BOOST_TEST_CSTR_EQ( (mp_at_c<L, 0>::name), "m3" );
        BOOST_TEST_EQ( (mp_at_c<L, 0>::modifiers), mod_private );

        BOOST_TEST( (mp_at_c<L, 1>::pointer) == Y::m4_pointer() );
        BOOST_TEST_CSTR_EQ( (mp_at_c<L, 1>::name), "m4" );
        BOOST_TEST_EQ( (mp_at_c<L, 1>::modifiers), mod_private );
    }

    {
        using L = describe_members<Y, mod_protected>;

        BOOST_TEST_EQ( mp_size<L>::value, 0 );
    }

    return boost::report_errors();
}

#endif // !defined(BOOST_DESCRIBE_CXX14)
