//Copyright (c) 2008-2016 Emil Dotchevski and Reverge Studios, Inc.

//Distributed under the Boost Software License, Version 1.0. (See accompanying
//file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/qvm/mat_traits_array.hpp>
#include <boost/qvm/mat_operations.hpp>
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
    BOOST_QVM_STATIC_ASSERT(is_mat<T>::value);
    BOOST_QVM_STATIC_ASSERT(mat_traits<T>::rows==3);
    BOOST_QVM_STATIC_ASSERT(mat_traits<T>::cols==2);
    BOOST_TEST((mat_traits<T>::template read_element<0,0>(v)==ptr[0*2+0]));
    BOOST_TEST((mat_traits<T>::template read_element<0,1>(v)==ptr[0*2+1]));
    BOOST_TEST((mat_traits<T>::template read_element<1,0>(v)==ptr[1*2+0]));
    BOOST_TEST((mat_traits<T>::template read_element<1,1>(v)==ptr[1*2+1]));
    BOOST_TEST((mat_traits<T>::template read_element<2,0>(v)==ptr[2*2+0]));
    BOOST_TEST((mat_traits<T>::template read_element<2,1>(v)==ptr[2*2+1]));
    BOOST_TEST((&mat_traits<T>::template write_element<0,0>(v)==&ptr[0*2+0]));
    BOOST_TEST((&mat_traits<T>::template write_element<0,1>(v)==&ptr[0*2+1]));
    BOOST_TEST((&mat_traits<T>::template write_element<1,0>(v)==&ptr[1*2+0]));
    BOOST_TEST((&mat_traits<T>::template write_element<1,1>(v)==&ptr[1*2+1]));
    BOOST_TEST((&mat_traits<T>::template write_element<2,0>(v)==&ptr[2*2+0]));
    BOOST_TEST((&mat_traits<T>::template write_element<2,1>(v)==&ptr[2*2+1]));
    BOOST_TEST(&v[0][0]==&ptr[0*2+0]);
    BOOST_TEST(&v[0][1]==&ptr[0*2+1]);
    BOOST_TEST(&v[1][0]==&ptr[1*2+0]);
    BOOST_TEST(&v[1][1]==&ptr[1*2+1]);
    BOOST_TEST(&v[2][0]==&ptr[2*2+0]);
    BOOST_TEST(&v[2][1]==&ptr[2*2+1]);
    }

int
main()
    {
    using namespace boost::qvm;
        {
        BOOST_QVM_STATIC_ASSERT(!is_mat<int[3]>::value);
        BOOST_QVM_STATIC_ASSERT(!is_mat<int[3][3][3]>::value);
        BOOST_QVM_STATIC_ASSERT(is_mat<int[3][4]>::value);
        BOOST_QVM_STATIC_ASSERT(mat_traits<int[3][4]>::rows==3);
        BOOST_QVM_STATIC_ASSERT(mat_traits<int[3][4]>::cols==4);
        same_type<mat_traits<int[3][4]>::scalar_type,int>();
        same_type< mat<int,3,3>, deduce_mat<int[3][3]>::type >();
        same_type< mat<int,3,3>, deduce_mat<int const[3][3]>::type >();
        int arr[3][3] = {{00,01,02},{10,11,12},{20,21,22}};
        BOOST_TEST((mat_traits<int[3][3]>::read_element<0,0>(arr)==00));
        BOOST_TEST((mat_traits<int[3][3]>::read_element<0,1>(arr)==01));
        BOOST_TEST((mat_traits<int[3][3]>::read_element<0,2>(arr)==02));
        BOOST_TEST((mat_traits<int[3][3]>::read_element<1,0>(arr)==10));
        BOOST_TEST((mat_traits<int[3][3]>::read_element<1,1>(arr)==11));
        BOOST_TEST((mat_traits<int[3][3]>::read_element<1,2>(arr)==12));
        BOOST_TEST((mat_traits<int[3][3]>::read_element<2,0>(arr)==20));
        BOOST_TEST((mat_traits<int[3][3]>::read_element<2,1>(arr)==21));
        BOOST_TEST((mat_traits<int[3][3]>::read_element<2,2>(arr)==22));
        BOOST_TEST((mat_traits<int const[3][3]>::read_element<0,0>(arr)==00));
        BOOST_TEST((mat_traits<int const[3][3]>::read_element<0,1>(arr)==01));
        BOOST_TEST((mat_traits<int const[3][3]>::read_element<0,2>(arr)==02));
        BOOST_TEST((mat_traits<int const[3][3]>::read_element<1,0>(arr)==10));
        BOOST_TEST((mat_traits<int const[3][3]>::read_element<1,1>(arr)==11));
        BOOST_TEST((mat_traits<int const[3][3]>::read_element<1,2>(arr)==12));
        BOOST_TEST((mat_traits<int const[3][3]>::read_element<2,0>(arr)==20));
        BOOST_TEST((mat_traits<int const[3][3]>::read_element<2,1>(arr)==21));
        BOOST_TEST((mat_traits<int const[3][3]>::read_element<2,2>(arr)==22));
        BOOST_TEST((mat_traits<int[3][3]>::read_element_idx(0,0,arr)==00));
        BOOST_TEST((mat_traits<int[3][3]>::read_element_idx(0,1,arr)==01));
        BOOST_TEST((mat_traits<int[3][3]>::read_element_idx(0,2,arr)==02));
        BOOST_TEST((mat_traits<int[3][3]>::read_element_idx(1,0,arr)==10));
        BOOST_TEST((mat_traits<int[3][3]>::read_element_idx(1,1,arr)==11));
        BOOST_TEST((mat_traits<int[3][3]>::read_element_idx(1,2,arr)==12));
        BOOST_TEST((mat_traits<int[3][3]>::read_element_idx(2,0,arr)==20));
        BOOST_TEST((mat_traits<int[3][3]>::read_element_idx(2,1,arr)==21));
        BOOST_TEST((mat_traits<int[3][3]>::read_element_idx(2,2,arr)==22));
        BOOST_TEST((mat_traits<int const[3][3]>::read_element_idx(0,0,arr)==00));
        BOOST_TEST((mat_traits<int const[3][3]>::read_element_idx(0,1,arr)==01));
        BOOST_TEST((mat_traits<int const[3][3]>::read_element_idx(0,2,arr)==02));
        BOOST_TEST((mat_traits<int const[3][3]>::read_element_idx(1,0,arr)==10));
        BOOST_TEST((mat_traits<int const[3][3]>::read_element_idx(1,1,arr)==11));
        BOOST_TEST((mat_traits<int const[3][3]>::read_element_idx(1,2,arr)==12));
        BOOST_TEST((mat_traits<int const[3][3]>::read_element_idx(2,0,arr)==20));
        BOOST_TEST((mat_traits<int const[3][3]>::read_element_idx(2,1,arr)==21));
        BOOST_TEST((mat_traits<int const[3][3]>::read_element_idx(2,2,arr)==22));
        BOOST_TEST((&mat_traits<int[3][3]>::write_element<0,0>(arr)==&arr[0][0]));
        BOOST_TEST((&mat_traits<int[3][3]>::write_element<0,1>(arr)==&arr[0][1]));
        BOOST_TEST((&mat_traits<int[3][3]>::write_element<0,2>(arr)==&arr[0][2]));
        BOOST_TEST((&mat_traits<int[3][3]>::write_element<1,0>(arr)==&arr[1][0]));
        BOOST_TEST((&mat_traits<int[3][3]>::write_element<1,1>(arr)==&arr[1][1]));
        BOOST_TEST((&mat_traits<int[3][3]>::write_element<1,2>(arr)==&arr[1][2]));
        BOOST_TEST((&mat_traits<int[3][3]>::write_element<2,0>(arr)==&arr[2][0]));
        BOOST_TEST((&mat_traits<int[3][3]>::write_element<2,1>(arr)==&arr[2][1]));
        BOOST_TEST((&mat_traits<int[3][3]>::write_element<2,2>(arr)==&arr[2][2]));
        BOOST_TEST((&mat_traits<int[3][3]>::write_element_idx(0,0,arr)==&arr[0][0]));
        BOOST_TEST((&mat_traits<int[3][3]>::write_element_idx(0,1,arr)==&arr[0][1]));
        BOOST_TEST((&mat_traits<int[3][3]>::write_element_idx(0,2,arr)==&arr[0][2]));
        BOOST_TEST((&mat_traits<int[3][3]>::write_element_idx(1,0,arr)==&arr[1][0]));
        BOOST_TEST((&mat_traits<int[3][3]>::write_element_idx(1,1,arr)==&arr[1][1]));
        BOOST_TEST((&mat_traits<int[3][3]>::write_element_idx(1,2,arr)==&arr[1][2]));
        BOOST_TEST((&mat_traits<int[3][3]>::write_element_idx(2,0,arr)==&arr[2][0]));
        BOOST_TEST((&mat_traits<int[3][3]>::write_element_idx(2,1,arr)==&arr[2][1]));
        BOOST_TEST((&mat_traits<int[3][3]>::write_element_idx(2,2,arr)==&arr[2][2]));
        }
        {
        int arr[42] = {0};
        int * ptr=arr+5;
        ptr[0*2+0]=42;
        ptr[0*2+1]=43;
        ptr[1*2+0]=44;
        ptr[1*2+1]=45;
        ptr[2*2+0]=46;
        ptr[2*2+1]=47;
        test_ref_cast(ptr_mref<3,2>(ptr),ptr);
        int m[3][2] = {{1,1},{1,1},{1,1}};
        ptr_mref<3,2>(ptr) += mref(m);
        BOOST_TEST(ptr[0*2+0]=43);
        BOOST_TEST(ptr[0*2+1]=44);
        BOOST_TEST(ptr[1*2+0]=45);
        BOOST_TEST(ptr[1*2+1]=46);
        BOOST_TEST(ptr[2*2+0]=47);
        BOOST_TEST(ptr[2*2+1]=48);
        }
    return boost::report_errors();
    }
