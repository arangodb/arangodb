// Copyright 2018 Peter Dimov
//
// Distributed under the Boost Software License, Version 1.0.
//
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt

#include <boost/shared_ptr.hpp>
#include <boost/core/lightweight_test.hpp>

boost::shared_ptr<int> dll_test_41();
boost::shared_ptr<int> dll_test_42();
boost::shared_ptr<int> dll_test_43();
boost::shared_ptr<int[]> dll_test_44();
boost::shared_ptr<int[]> dll_test_45();

int main()
{
    {
        boost::shared_ptr<int> p = dll_test_41();
        BOOST_TEST_EQ( *p, 41 );
    }

    {
        boost::shared_ptr<int> p = dll_test_42();
        BOOST_TEST_EQ( *p, 42 );
    }

    {
        boost::shared_ptr<int> p = dll_test_43();
        BOOST_TEST_EQ( *p, 43 );
    }

    {
        boost::shared_ptr<int[]> p = dll_test_44();
        BOOST_TEST_EQ( p[0], 44 );
    }

    {
        boost::shared_ptr<int[]> p = dll_test_45();
        BOOST_TEST_EQ( p[0], 45 );
    }

    return boost::report_errors();
}
