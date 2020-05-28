//Copyright (c) 2008-2016 Emil Dotchevski and Reverge Studios, Inc.

//Distributed under the Boost Software License, Version 1.0. (See accompanying
//file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/qvm/scalar_traits.hpp>
#include <boost/qvm/vec.hpp>
#include <boost/qvm/mat.hpp>
#include <boost/qvm/quat.hpp>

template <bool>
struct tester;

template <>
struct
tester<true>
    {
    };

using namespace boost::qvm;
tester<is_scalar<char>::value> t1;
tester<is_scalar<signed char>::value> t2;
tester<is_scalar<unsigned char>::value> t3;
tester<is_scalar<signed short>::value> t4;
tester<is_scalar<unsigned short>::value> t5;
tester<is_scalar<signed int>::value> t6;
tester<is_scalar<unsigned int>::value> t7;
tester<is_scalar<signed long>::value> t8;
tester<is_scalar<unsigned long>::value> t9;
tester<is_scalar<float>::value> t10;
tester<is_scalar<double>::value> t11;
tester<is_scalar<long double>::value> t13;
tester<!is_scalar<vec<float,4> >::value> t14;
tester<!is_scalar<mat<float,4,4> >::value> t15;
tester<!is_scalar<quat<float> >::value> t16;
