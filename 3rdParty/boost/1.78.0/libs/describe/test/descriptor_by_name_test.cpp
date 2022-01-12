// Copyright 2021 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/describe/descriptor_by_name.hpp>
#include <boost/describe/class.hpp>
#include <boost/describe/members.hpp>
#include <boost/core/lightweight_test.hpp>

#if !defined(BOOST_DESCRIBE_CXX14)

#include <boost/config/pragma_message.hpp>

BOOST_PRAGMA_MESSAGE("Skipping test because C++14 is not available")
int main() {}

#else

struct X
{
    int a = 1;
    int b = 2;
};

BOOST_DESCRIBE_STRUCT(X, (), (a, b))

struct Y
{
    int a = 1;
    int c = 3;
};

BOOST_DESCRIBE_STRUCT(Y, (), (a, c))

int main()
{
    {
        using L = boost::describe::describe_members<X, boost::describe::mod_any_access>;

        using Na = BOOST_DESCRIBE_MAKE_NAME( a );
        using Da = boost::describe::descriptor_by_name<L, Na>;
        BOOST_TEST_CSTR_EQ( Da::name, "a" );

        using Nb = BOOST_DESCRIBE_MAKE_NAME( b );
        using Db = boost::describe::descriptor_by_name<L, Nb>;
        BOOST_TEST_CSTR_EQ( Db::name, "b" );
    }

    {
        using L = boost::describe::describe_members<Y, boost::describe::mod_any_access>;

        using Na = BOOST_DESCRIBE_MAKE_NAME( a );
        using Da = boost::describe::descriptor_by_name<L, Na>;
        BOOST_TEST_CSTR_EQ( Da::name, "a" );

        using Nc = BOOST_DESCRIBE_MAKE_NAME( c );
        using Dc = boost::describe::descriptor_by_name<L, Nc>;
        BOOST_TEST_CSTR_EQ( Dc::name, "c" );
    }

    return boost::report_errors();
}

#endif // !defined(BOOST_DESCRIBE_CXX14)
