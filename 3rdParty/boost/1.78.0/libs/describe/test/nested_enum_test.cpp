// Copyright 2020, 2021 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/describe/enumerators.hpp>
#include <boost/describe/enum.hpp>
#include <boost/core/lightweight_test.hpp>

struct X1
{
    enum E
    {
        v1 = 5
    };

    BOOST_DESCRIBE_NESTED_ENUM(E, v1)
};

class X2
{
private:

    enum E
    {
        v1, v2
    };

    BOOST_DESCRIBE_NESTED_ENUM(E, v1, v2)

public:

    typedef E E2;
};

#if !defined(BOOST_DESCRIBE_CXX11)

#include <boost/config/pragma_message.hpp>

BOOST_PRAGMA_MESSAGE("Skipping test because C++11 is not available")
int main() {}

#else

struct X3
{
    enum class E
    {
        v1 = 7
    };

    BOOST_DESCRIBE_NESTED_ENUM(E, v1)
};

class X4
{
private:

    enum class E
    {
        v1, v2
    };

    BOOST_DESCRIBE_NESTED_ENUM(E, v1, v2)

public:

    typedef E E2;
};

#if !defined(BOOST_DESCRIBE_CXX14)

#include <boost/config/pragma_message.hpp>

BOOST_PRAGMA_MESSAGE("Skipping test because C++14 is not available")
int main() {}

#else

#include <boost/mp11.hpp>
using namespace boost::mp11;

int main()
{
    {
        using D = boost::describe::describe_enumerators<X1::E>;

        BOOST_TEST_EQ( mp_size<D>::value, 1 );

        BOOST_TEST_EQ( (mp_at_c<D, 0>::value), X1::v1 );
        BOOST_TEST_CSTR_EQ( (mp_at_c<D, 0>::name), "v1" );
    }

    {
        using D = boost::describe::describe_enumerators<X2::E2>;

        BOOST_TEST_EQ( mp_size<D>::value, 2 );

        BOOST_TEST_EQ( (mp_at_c<D, 0>::value), 0 );
        BOOST_TEST_CSTR_EQ( (mp_at_c<D, 0>::name), "v1" );

        BOOST_TEST_EQ( (mp_at_c<D, 1>::value), 1 );
        BOOST_TEST_CSTR_EQ( (mp_at_c<D, 1>::name), "v2" );
    }

    {
        using D = boost::describe::describe_enumerators<X3::E>;

        BOOST_TEST_EQ( mp_size<D>::value, 1 );

        BOOST_TEST_EQ( (int)(mp_at_c<D, 0>::value), (int)X3::E::v1 );
        BOOST_TEST_CSTR_EQ( (mp_at_c<D, 0>::name), "v1" );
    }

    {
        using D = boost::describe::describe_enumerators<X4::E2>;

        BOOST_TEST_EQ( mp_size<D>::value, 2 );

        BOOST_TEST_EQ( (int)(mp_at_c<D, 0>::value), 0 );
        BOOST_TEST_CSTR_EQ( (mp_at_c<D, 0>::name), "v1" );

        BOOST_TEST_EQ( (int)(mp_at_c<D, 1>::value), 1 );
        BOOST_TEST_CSTR_EQ( (mp_at_c<D, 1>::name), "v2" );
    }

    return boost::report_errors();
}

#endif // !defined(BOOST_DESCRIBE_CXX14)
#endif // !defined(BOOST_DESCRIBE_CXX11)
