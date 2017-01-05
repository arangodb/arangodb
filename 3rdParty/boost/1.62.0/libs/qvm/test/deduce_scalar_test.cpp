//Copyright (c) 2008-2016 Emil Dotchevski and Reverge Studios, Inc.

//Distributed under the Boost Software License, Version 1.0. (See accompanying
//file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/qvm/deduce_scalar.hpp>

template <class T,class U>
struct same_type;

template <class T>
struct
same_type<T,T>
    {
    };

template <class A,class B,class R>
struct
check
    {
    same_type<typename boost::qvm::deduce_scalar<A,B>::type,R> a;
    same_type<typename boost::qvm::deduce_scalar<B,A>::type,R> b;
    };

int
main()
    {
    check<signed char,signed char,signed char>();
    check<signed char,signed short,signed short>();
    check<signed char,signed int,signed int>();
    check<signed char,signed long,signed long>();
    check<signed char,unsigned char,unsigned char>();
    check<signed char,unsigned short,unsigned short>();
    check<signed char,unsigned int,unsigned int>();
    check<signed char,unsigned long,unsigned long>();
    check<signed char,float,float>();
    check<signed char,double,double>();
    check<signed short,signed short,signed short>();
    check<signed short,signed int,signed int>();
    check<signed short,signed long,signed long>();
    check<signed short,unsigned short,unsigned short>();
    check<signed short,unsigned int,unsigned int>();
    check<signed short,unsigned long,unsigned long>();
    check<signed short,float,float>();
    check<signed short,double,double>();
    check<signed int,signed int,signed int>();
    check<signed int,signed long,signed long>();
    check<signed int,unsigned int,unsigned int>();
    check<signed int,unsigned long,unsigned long>();
    check<signed int,float,float>();
    check<signed int,double,double>();
    check<signed long,signed long,signed long>();
    check<signed long,unsigned long,unsigned long>();
    check<signed long,float,float>();
    check<signed long,double,double>();
    check<unsigned char,unsigned char,unsigned char>();
    check<unsigned char,unsigned short,unsigned short>();
    check<unsigned char,unsigned int,unsigned int>();
    check<unsigned char,unsigned long,unsigned long>();
    check<unsigned char,float,float>();
    check<unsigned char,double,double>();
    check<unsigned short,unsigned short,unsigned short>();
    check<unsigned short,unsigned int,unsigned int>();
    check<unsigned short,unsigned long,unsigned long>();
    check<unsigned short,float,float>();
    check<unsigned short,double,double>();
    check<unsigned int,unsigned int,unsigned int>();
    check<unsigned int,unsigned long,unsigned long>();
    check<unsigned int,float,float>();
    check<unsigned int,double,double>();
    check<unsigned long,unsigned long,unsigned long>();
    check<unsigned long,float,float>();
    check<unsigned long,double,double>();
    check<float,float,float>();
    check<float,double,double>();
    check<double,double,double>();
    }
