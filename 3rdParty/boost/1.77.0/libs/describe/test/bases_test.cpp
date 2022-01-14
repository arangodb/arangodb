// Copyright 2020 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/describe/bases.hpp>
#include <boost/describe/class.hpp>
#include <boost/core/lightweight_test.hpp>
#include <boost/core/lightweight_test_trait.hpp>

struct X
{
};

BOOST_DESCRIBE_STRUCT(X, (), ())

struct X1
{
};

struct X2
{
};

struct X3
{
};

struct V1
{
    V1(int);
};

struct V2
{
    V2(int);
};

struct V3
{
    V3(int);
};

struct Y: public X1, protected X2, private X3, public virtual V1, protected virtual V2, private virtual V3
{
};

BOOST_DESCRIBE_STRUCT(Y, (X1, X2, X3, V1, V2, V3), ())

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
        using L = describe_bases<X, mod_any_access>;

        BOOST_TEST_EQ( mp_size<L>::value, 0 );
    }

    {
        using L = describe_bases<X, mod_public>;

        BOOST_TEST_EQ( mp_size<L>::value, 0 );
    }

    {
        using L = describe_bases<X, mod_protected>;

        BOOST_TEST_EQ( mp_size<L>::value, 0 );
    }

    {
        using L = describe_bases<X, mod_private>;

        BOOST_TEST_EQ( mp_size<L>::value, 0 );
    }

    {
        using L = describe_bases<Y, mod_any_access>;

        BOOST_TEST_EQ( mp_size<L>::value, 6 );

        BOOST_TEST_TRAIT_SAME( typename mp_at_c<L, 0>::type, X1 );
        BOOST_TEST_EQ( (mp_at_c<L, 0>::modifiers), mod_public );

        BOOST_TEST_TRAIT_SAME( typename mp_at_c<L, 1>::type, X2 );
        BOOST_TEST_EQ( (mp_at_c<L, 1>::modifiers), mod_protected );

        BOOST_TEST_TRAIT_SAME( typename mp_at_c<L, 2>::type, X3 );
        BOOST_TEST_EQ( (mp_at_c<L, 2>::modifiers), mod_private );

        BOOST_TEST_TRAIT_SAME( typename mp_at_c<L, 3>::type, V1 );
        BOOST_TEST_EQ( (mp_at_c<L, 3>::modifiers), mod_public | mod_virtual );

        BOOST_TEST_TRAIT_SAME( typename mp_at_c<L, 4>::type, V2 );
        BOOST_TEST_EQ( (mp_at_c<L, 4>::modifiers), mod_protected | mod_virtual );

        BOOST_TEST_TRAIT_SAME( typename mp_at_c<L, 5>::type, V3 );
        BOOST_TEST_EQ( (mp_at_c<L, 5>::modifiers), mod_private | mod_virtual );
    }

    {
        using L = describe_bases<Y, mod_public>;

        BOOST_TEST_EQ( mp_size<L>::value, 2 );

        BOOST_TEST_TRAIT_SAME( typename mp_at_c<L, 0>::type, X1 );
        BOOST_TEST_EQ( (mp_at_c<L, 0>::modifiers), mod_public );

        BOOST_TEST_TRAIT_SAME( typename mp_at_c<L, 1>::type, V1 );
        BOOST_TEST_EQ( (mp_at_c<L, 1>::modifiers), mod_public | mod_virtual );
    }

    {
        using L = describe_bases<Y, mod_protected>;

        BOOST_TEST_EQ( mp_size<L>::value, 2 );

        BOOST_TEST_TRAIT_SAME( typename mp_at_c<L, 0>::type, X2 );
        BOOST_TEST_EQ( (mp_at_c<L, 0>::modifiers), mod_protected );

        BOOST_TEST_TRAIT_SAME( typename mp_at_c<L, 1>::type, V2 );
        BOOST_TEST_EQ( (mp_at_c<L, 1>::modifiers), mod_protected | mod_virtual );
    }

    {
        using L = describe_bases<Y, mod_private>;

        BOOST_TEST_EQ( mp_size<L>::value, 2 );

        BOOST_TEST_TRAIT_SAME( typename mp_at_c<L, 0>::type, X3 );
        BOOST_TEST_EQ( (mp_at_c<L, 0>::modifiers), mod_private );

        BOOST_TEST_TRAIT_SAME( typename mp_at_c<L, 1>::type, V3 );
        BOOST_TEST_EQ( (mp_at_c<L, 1>::modifiers), mod_private | mod_virtual );
    }

    return boost::report_errors();
}

#endif // !defined(BOOST_DESCRIBE_CXX14)
