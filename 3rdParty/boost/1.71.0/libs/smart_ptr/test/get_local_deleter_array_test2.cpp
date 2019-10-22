//
// get_local_deleter_test2.cpp
//
// Copyright 2002, 2017 Peter Dimov
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/config.hpp>

#if defined( BOOST_NO_CXX11_RVALUE_REFERENCES ) || defined( BOOST_NO_CXX11_VARIADIC_TEMPLATES )

int main()
{
}

#else

#include <boost/smart_ptr/local_shared_ptr.hpp>
#include <boost/smart_ptr/make_local_shared.hpp>
#include <boost/core/lightweight_test.hpp>

struct deleter
{
};

struct deleter2;

struct X
{
};

int main()
{
    {
        boost::local_shared_ptr<X[]> p = boost::make_local_shared<X[]>( 1 );

        BOOST_TEST(boost::get_deleter<void>(p) == 0);
        BOOST_TEST(boost::get_deleter<void const>(p) == 0);
        BOOST_TEST(boost::get_deleter<int>(p) == 0);
        BOOST_TEST(boost::get_deleter<int const>(p) == 0);
        BOOST_TEST(boost::get_deleter<X>(p) == 0);
        BOOST_TEST(boost::get_deleter<X const>(p) == 0);
        BOOST_TEST(boost::get_deleter<deleter>(p) == 0);
        BOOST_TEST(boost::get_deleter<deleter const>(p) == 0);
        BOOST_TEST(boost::get_deleter<deleter2>(p) == 0);
        BOOST_TEST(boost::get_deleter<deleter2 const>(p) == 0);
    }

    {
        boost::local_shared_ptr<X[1]> p = boost::make_local_shared<X[1]>();

        BOOST_TEST(boost::get_deleter<void>(p) == 0);
        BOOST_TEST(boost::get_deleter<void const>(p) == 0);
        BOOST_TEST(boost::get_deleter<int>(p) == 0);
        BOOST_TEST(boost::get_deleter<int const>(p) == 0);
        BOOST_TEST(boost::get_deleter<X>(p) == 0);
        BOOST_TEST(boost::get_deleter<X const>(p) == 0);
        BOOST_TEST(boost::get_deleter<deleter>(p) == 0);
        BOOST_TEST(boost::get_deleter<deleter const>(p) == 0);
        BOOST_TEST(boost::get_deleter<deleter2>(p) == 0);
        BOOST_TEST(boost::get_deleter<deleter2 const>(p) == 0);
    }

    return boost::report_errors();
}

struct deleter2
{
};

#endif
