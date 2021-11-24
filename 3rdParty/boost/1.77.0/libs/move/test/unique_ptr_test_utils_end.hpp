//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Howard Hinnant 2009
// (C) Copyright Ion Gaztanaga 2014-2014.
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/move for documentation.
//
//////////////////////////////////////////////////////////////////////////////
#ifndef BOOST_MOVE_UNIQUE_PTR_TEST_UTILS_END_HPP
#define BOOST_MOVE_UNIQUE_PTR_TEST_UTILS_END_HPP

#ifndef BOOST_MOVE_UNIQUE_PTR_TEST_UTILS_BEG_HPP
#error "unique_ptr_test_utils_beg.hpp MUST be included before this header"
#endif

//Define the incomplete I type and out of line functions

struct I
{
   static int count;
   I() {++count;}
   I(const A&) {++count;}
   ~I() {--count;}
};

int I::count = 0;

I* get() {return new I;}
I* get_array(int i) {return new I[i];}

void check(int i)
{
   BOOST_TEST(I::count == i);
}

template <class T, class D>
J<T, D>::~J() {}

void reset_counters()
{  A::count = 0; B::count = 0; I::count = 0; }

#endif   //BOOST_MOVE_UNIQUE_PTR_TEST_UTILS_END_HPP
