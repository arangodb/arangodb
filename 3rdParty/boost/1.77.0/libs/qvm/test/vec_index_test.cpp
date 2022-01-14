//Copyright (c) 2008-2016 Emil Dotchevski and Reverge Studios, Inc.

//Distributed under the Boost Software License, Version 1.0. (See accompanying
//file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include "test_qvm_vector.hpp"

int
main()
    {       
    using namespace boost::qvm;

    typedef test_qvm::vector<V1,4> V;
    V v;
    v.a[0]=42.0f;
    v.a[1]=43.0f;
    v.a[2]=44.0f;
    v.a[3]=45.0f;
    BOOST_TEST(vec_traits<V>::read_element_idx(0,v)==v.a[0]);
    BOOST_TEST(vec_traits<V>::read_element_idx(1,v)==v.a[1]);
    BOOST_TEST(vec_traits<V>::read_element_idx(2,v)==v.a[2]);
    BOOST_TEST(vec_traits<V>::read_element_idx(3,v)==v.a[3]);
    BOOST_TEST(&vec_traits<V>::write_element_idx(0,v)==&v.a[0]);
    BOOST_TEST(&vec_traits<V>::write_element_idx(1,v)==&v.a[1]);
    BOOST_TEST(&vec_traits<V>::write_element_idx(2,v)==&v.a[2]);
    BOOST_TEST(&vec_traits<V>::write_element_idx(3,v)==&v.a[3]);
    return boost::report_errors();
    }
