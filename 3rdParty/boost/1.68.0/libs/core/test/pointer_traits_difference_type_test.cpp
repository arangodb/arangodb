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

template<class T>
struct E {
    typedef long difference_type;
};

int main()
{
    BOOST_TEST_TRAIT_TRUE((boost::core::is_same<std::ptrdiff_t,
        boost::pointer_traits<int*>::difference_type>));
    BOOST_TEST_TRAIT_TRUE((boost::core::is_same<std::ptrdiff_t,
        boost::pointer_traits<P<int> >::difference_type>));
    BOOST_TEST_TRAIT_TRUE((boost::core::is_same<long,
        boost::pointer_traits<E<int> >::difference_type>));
    BOOST_TEST_TRAIT_TRUE((boost::core::is_same<std::ptrdiff_t,
        boost::pointer_traits<void*>::difference_type>));
    BOOST_TEST_TRAIT_TRUE((boost::core::is_same<std::ptrdiff_t,
        boost::pointer_traits<P<void> >::difference_type>));
    BOOST_TEST_TRAIT_TRUE((boost::core::is_same<long,
        boost::pointer_traits<E<void> >::difference_type>));
    BOOST_TEST_TRAIT_TRUE((boost::core::is_same<std::ptrdiff_t,
        boost::pointer_traits<const int*>::difference_type>));
    BOOST_TEST_TRAIT_TRUE((boost::core::is_same<std::ptrdiff_t,
        boost::pointer_traits<P<const int> >::difference_type>));
    BOOST_TEST_TRAIT_TRUE((boost::core::is_same<long,
        boost::pointer_traits<E<const int> >::difference_type>));
    return boost::report_errors();
}
