// Copyright 2021 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include<boost/bind/apply.hpp>
#include<boost/bind/bind.hpp>
#include <boost/core/lightweight_test.hpp>
#include <boost/config.hpp>
#include <boost/config/pragma_message.hpp>

#if defined(BOOST_NO_CXX11_RVALUE_REFERENCES)

BOOST_PRAGMA_MESSAGE("Skipping test because BOOST_NO_CXX11_RVALUE_REFERENCES is defined")
int main() {}

#elif defined(BOOST_NO_CXX11_VARIADIC_TEMPLATES)

BOOST_PRAGMA_MESSAGE("Skipping test because BOOST_NO_CXX11_VARIADIC_TEMPLATES is defined")
int main() {}

#else

struct F
{
public:

    int operator()( int & x ) const
    {
        return x;
    }

    int operator()( int && x ) const
    {
        return -x;
    }
};

int& get_lvalue_arg()
{
    static int a = 1;
    return a;
}

int get_prvalue_arg()
{
    return 2;
}

F& get_lvalue_f()
{
    static F f;
    return f;
}

F get_prvalue_f()
{
    return F();
}

int main()
{
    using namespace boost::placeholders;

    BOOST_TEST_EQ( boost::bind(boost::apply<int>(), boost::bind(get_lvalue_f), boost::bind(get_lvalue_arg))(), 1 );
    BOOST_TEST_EQ( boost::bind(boost::apply<int>(), boost::bind(get_lvalue_f), boost::bind(get_prvalue_arg))(), -2 );
    BOOST_TEST_EQ( boost::bind(boost::apply<int>(), boost::bind(get_prvalue_f), boost::bind(get_lvalue_arg))(), 1 );
    BOOST_TEST_EQ( boost::bind(boost::apply<int>(), boost::bind(get_prvalue_f), boost::bind(get_prvalue_arg))(), -2 );

    BOOST_TEST_EQ( boost::bind(boost::apply<int>(), boost::bind(boost::apply<F&>(), _1), boost::bind(boost::apply<int&>(), _2))(get_lvalue_f, get_lvalue_arg), 1 );
    BOOST_TEST_EQ( boost::bind(boost::apply<int>(), boost::bind(boost::apply<F&>(), _1), boost::bind(boost::apply<int>(), _2))(get_lvalue_f, get_prvalue_arg), -2 );
    BOOST_TEST_EQ( boost::bind(boost::apply<int>(), boost::bind(boost::apply<F>(), _1), boost::bind(boost::apply<int&>(), _2))(get_prvalue_f, get_lvalue_arg), 1 );
    BOOST_TEST_EQ( boost::bind(boost::apply<int>(), boost::bind(boost::apply<F>(), _1), boost::bind(boost::apply<int>(), _2))(get_prvalue_f, get_prvalue_arg), -2 );

    return boost::report_errors();
}

#endif
