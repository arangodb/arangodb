//Copyright (c) 2008-2016 Emil Dotchevski and Reverge Studios, Inc.

//Distributed under the Boost Software License, Version 1.0. (See accompanying
//file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/qvm/vec_traits_array.hpp>
#include <boost/qvm/vec_operations.hpp>
#include <boost/detail/lightweight_test.hpp>

template <class T,class U>
struct same_type;

template <class T>
struct
same_type<T,T>
    {
    };

template <class T,class P>
void
test_ref_cast( T & v, P * ptr )
    {
    using namespace boost::qvm;
    BOOST_QVM_STATIC_ASSERT(is_vec<T>::value);
    BOOST_QVM_STATIC_ASSERT(vec_traits<T>::dim==3);
    BOOST_TEST(vec_traits<T>::template read_element<0>(v)==ptr[0]);
    BOOST_TEST(vec_traits<T>::template read_element<1>(v)==ptr[1]);
    BOOST_TEST(vec_traits<T>::template read_element<2>(v)==ptr[2]);
    BOOST_TEST(&vec_traits<T>::template write_element<0>(v)==&ptr[0]);
    BOOST_TEST(&vec_traits<T>::template write_element<1>(v)==&ptr[1]);
    BOOST_TEST(&vec_traits<T>::template write_element<2>(v)==&ptr[2]);
    BOOST_TEST(&v[0]==&ptr[0]);
    BOOST_TEST(&v[1]==&ptr[1]);
    BOOST_TEST(&v[2]==&ptr[2]);
    }

int
main()
    {
    using namespace boost::qvm;
        {
        BOOST_QVM_STATIC_ASSERT(is_vec<int[3]>::value);
        BOOST_QVM_STATIC_ASSERT(!is_vec<int[3][3]>::value);
        BOOST_QVM_STATIC_ASSERT(!is_vec<int[3][3][3]>::value);
        BOOST_QVM_STATIC_ASSERT((vec_traits<int[3]>::dim==3));
        same_type<vec_traits<int[3]>::scalar_type,int>();
        same_type< vec<int,3>, deduce_vec<int[3]>::type >();
        same_type< vec<int,3>, deduce_vec<int const[3]>::type >();
        int arr[3] = {0,1,2};
        BOOST_TEST((vec_traits<int[3]>::read_element<0>(arr)==0));
        BOOST_TEST((vec_traits<int[3]>::read_element<1>(arr)==1));
        BOOST_TEST((vec_traits<int[3]>::read_element<2>(arr)==2));
        BOOST_TEST((vec_traits<int const[3]>::read_element<0>(arr)==0));
        BOOST_TEST((vec_traits<int const[3]>::read_element<1>(arr)==1));
        BOOST_TEST((vec_traits<int const[3]>::read_element<2>(arr)==2));
        BOOST_TEST((vec_traits<int[3]>::read_element_idx(0,arr)==0));
        BOOST_TEST((vec_traits<int[3]>::read_element_idx(1,arr)==1));
        BOOST_TEST((vec_traits<int[3]>::read_element_idx(2,arr)==2));
        BOOST_TEST((vec_traits<int const[3]>::read_element_idx(0,arr)==0));
        BOOST_TEST((vec_traits<int const[3]>::read_element_idx(1,arr)==1));
        BOOST_TEST((vec_traits<int const[3]>::read_element_idx(2,arr)==2));
        BOOST_TEST((&vec_traits<int[3]>::write_element<0>(arr)==&arr[0]));
        BOOST_TEST((&vec_traits<int[3]>::write_element<1>(arr)==&arr[1]));
        BOOST_TEST((&vec_traits<int[3]>::write_element<2>(arr)==&arr[2]));
        BOOST_TEST((&vec_traits<int[3]>::write_element_idx(0,arr)==&arr[0]));
        BOOST_TEST((&vec_traits<int[3]>::write_element_idx(1,arr)==&arr[1]));
        BOOST_TEST((&vec_traits<int[3]>::write_element_idx(2,arr)==&arr[2]));
        }
        {
        int arr[42] = {0};
        int * ptr=arr+5;
        ptr[0]=42;
        ptr[1]=43;
        ptr[2]=44;
        test_ref_cast(ptr_vref<3>(ptr),ptr);
        int v[3] = {1,1,1};
        ptr_vref<3>(ptr) += vref(v);
        BOOST_TEST(ptr[0]==43);
        BOOST_TEST(ptr[1]==44);
        BOOST_TEST(ptr[2]==45);
        }
    return boost::report_errors();
    }
