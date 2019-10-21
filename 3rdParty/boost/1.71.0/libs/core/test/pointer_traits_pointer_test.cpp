/*
Copyright 2017 Glen Joseph Fernandes
(glenjofe@gmail.com)

Distributed under the Boost Software License, Version 1.0.
(http://www.boost.org/LICENSE_1_0.txt)
*/
#include <boost/core/pointer_traits.hpp>
#include <boost/core/is_same.hpp>
#include <boost/core/lightweight_test_trait.hpp>

template<class T>
struct P { };

int main()
{
    BOOST_TEST_TRAIT_TRUE((boost::core::is_same<int*,
        boost::pointer_traits<int*>::pointer>));
    BOOST_TEST_TRAIT_TRUE((boost::core::is_same<P<int>,
        boost::pointer_traits<P<int> >::pointer>));
    BOOST_TEST_TRAIT_TRUE((boost::core::is_same<void*,
        boost::pointer_traits<void*>::pointer>));
    BOOST_TEST_TRAIT_TRUE((boost::core::is_same<P<void>,
        boost::pointer_traits<P<void> >::pointer>));
    BOOST_TEST_TRAIT_TRUE((boost::core::is_same<const int*,
        boost::pointer_traits<const int*>::pointer>));
    BOOST_TEST_TRAIT_TRUE((boost::core::is_same<P<const int>,
        boost::pointer_traits<P<const int> >::pointer>));
    return boost::report_errors();
}
