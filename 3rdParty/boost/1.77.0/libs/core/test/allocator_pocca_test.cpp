/*
Copyright 2020 Glen Joseph Fernandes
(glenjofe@gmail.com)

Distributed under the Boost Software License, Version 1.0.
(http://www.boost.org/LICENSE_1_0.txt)
*/
#include <boost/core/allocator_access.hpp>
#include <boost/core/is_same.hpp>
#include <boost/core/lightweight_test_trait.hpp>

template<class T>
struct A1 {
    typedef T value_type;
};

#if !defined(BOOST_NO_CXX11_ALLOCATOR)
template<class T>
struct A2 {
    typedef T value_type;
    typedef std::true_type propagate_on_container_copy_assignment;
};
#endif

int main()
{
    BOOST_TEST_TRAIT_FALSE((boost::
        allocator_propagate_on_container_copy_assignment<A1<int> >::type));
#if !defined(BOOST_NO_CXX11_ALLOCATOR)
    BOOST_TEST_TRAIT_TRUE((boost::
        allocator_propagate_on_container_copy_assignment<A2<int> >::type));
#endif
    return boost::report_errors();
}
