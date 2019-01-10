//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga 2007-2017.
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/move for documentation.
//
//////////////////////////////////////////////////////////////////////////////
#include <boost/move/algo/detail/set_difference.hpp>
#include "order_type.hpp"
#include <boost/core/lightweight_test.hpp>
#include <cstddef>
/*
///////////////////////////////////
//
//      set_difference
//
///////////////////////////////////
void test_set_difference_normal()
{
   order_perf_type range2[10];
   for(std::size_t i = 0; i != sizeof(range2)/sizeof(*range2); ++i){
      range2[i].key = i*2;
      range2[i].val = 0u;
   }

   order_perf_type range1[4];
   range1[0].key = 0u;
   range1[0].val = 1u;
   range1[1].key = 1u;
   range1[1].val = 1u;
   range1[2].key = 3u;
   range1[2].val = 1u;
   range1[3].key = 4u;
   range1[3].val = 1u;

   order_perf_type out[20];
   out[2].key = 998;
   out[2].val = 999;
   boost::movelib::set_difference(range1, range1+4, range2, range2+10, out, order_type_less());
   BOOST_TEST(out[0].key == 1u);
   BOOST_TEST(out[0].val == 1u);
   BOOST_TEST(out[1].key == 3u);
   BOOST_TEST(out[1].val == 1u);
   BOOST_TEST(out[2].key == 998);
   BOOST_TEST(out[2].val == 999);
}

void test_set_difference_range1_repeated()
{
   order_perf_type range2[10];
   for(std::size_t i = 0; i != sizeof(range2)/sizeof(*range2); ++i){
      range2[i].key = i*2;
      range2[i].val = 0u;
   }

   order_perf_type range1[4];
   range1[0].key = 0u;
   range1[0].val = 1u;
   range1[1].key = 2u;
   range1[1].val = 1u;
   range1[2].key = 4u;
   range1[2].val = 1u;
   range1[3].key = 6u;
   range1[3].val = 1u;

   order_perf_type out[20];
   out[0].key = 998;
   out[0].val = 999;
   boost::movelib::set_difference(range1, range1+4, range2, range2+10, out, order_type_less());
   BOOST_TEST(out[0].key == 998);
   BOOST_TEST(out[0].val == 999);
}

void test_set_difference_range1_unique()
{
   order_perf_type range2[10];
   for(std::size_t i = 0; i != sizeof(range2)/sizeof(*range2); ++i){
      range2[i].key = i*2;
      range2[i].val = 0u;
   }

   order_perf_type range1[4];
   range1[0].key = 1u;
   range1[0].val = 1u;
   range1[1].key = 3u;
   range1[1].val = 1u;
   range1[2].key = 5u;
   range1[2].val = 1u;
   range1[3].key = 7u;
   range1[3].val = 1u;

   order_perf_type out[20];
   out[4].key = 998;
   out[4].val = 999;
   boost::movelib::set_difference(range1, range1+4, range2, range2+10, out, order_type_less());
   BOOST_TEST(out[0].key == 1u);
   BOOST_TEST(out[0].val == 1u);
   BOOST_TEST(out[1].key == 3u);
   BOOST_TEST(out[1].val == 1u);
   BOOST_TEST(out[2].key == 5u);
   BOOST_TEST(out[3].val == 1u);
   BOOST_TEST(out[3].key == 7u);
   BOOST_TEST(out[3].val == 1u);
   BOOST_TEST(out[4].key == 998);
   BOOST_TEST(out[4].val == 999);
}
*/

///////////////////////////////////
//
//      set_difference
//
///////////////////////////////////
void test_set_difference_normal()
{
   order_perf_type range2[10];
   for(std::size_t i = 0; i != sizeof(range2)/sizeof(*range2); ++i){
      range2[i].key = i*2;
      range2[i].val = 0u;
   }

   order_perf_type range1[5];
   range1[0].key = 0u;
   range1[0].val = 1u;
   range1[1].key = 1u;
   range1[1].val = 1u;
   range1[2].key = 1u;
   range1[2].val = 2u;
   range1[3].key = 3u;
   range1[3].val = 1u;
   range1[4].key = 4u;
   range1[4].val = 1u;

   order_perf_type out[20];
   out[3].key = 998;
   out[3].val = 999;
   order_perf_type *r =
      boost::movelib::set_difference(range1, range1+5, range2, range2+10, out, order_type_less());
   BOOST_TEST(&out[3] == r);
   BOOST_TEST(out[0].key == 1u);
   BOOST_TEST(out[0].val == 1u);
   BOOST_TEST(out[1].key == 1u);
   BOOST_TEST(out[1].val == 2u);
   BOOST_TEST(out[2].key == 3u);
   BOOST_TEST(out[2].val == 1u);
   BOOST_TEST(out[3].key == 998);
   BOOST_TEST(out[3].val == 999);
}

void test_set_difference_range1_repeated()
{
   order_perf_type range2[10];
   for(std::size_t i = 0; i != sizeof(range2)/sizeof(*range2); ++i){
      range2[i].key = i*2;
      range2[i].val = 0u;
   }

   order_perf_type range1[5];
   range1[0].key = 0u;
   range1[0].val = 1u;
   range1[1].key = 2u;
   range1[1].val = 1u;
   range1[2].key = 2u;
   range1[2].val = 2u;
   range1[3].key = 4u;
   range1[3].val = 1u;
   range1[4].key = 6u;
   range1[4].val = 1u;

   order_perf_type out[20];
   out[0].key = 998;
   out[0].val = 999;
   order_perf_type *r =
      boost::movelib::set_difference(range1, range1+5, range2, range2+10, out, order_type_less());
   BOOST_TEST(&out[1] == r);
   BOOST_TEST(out[0].key == 2);
   BOOST_TEST(out[0].val == 2);
}

void test_set_difference_range1_unique()
{
   order_perf_type range2[10];
   for(std::size_t i = 0; i != sizeof(range2)/sizeof(*range2); ++i){
      range2[i].key = i*2;
      range2[i].val = 0u;
   }

   order_perf_type range1[5];
   range1[0].key = 1u;
   range1[0].val = 1u;
   range1[1].key = 3u;
   range1[1].val = 1u;
   range1[2].key = 5u;
   range1[2].val = 1u;
   range1[3].key = 7u;
   range1[3].val = 1u;
   range1[4].key = 7u;
   range1[4].val = 2u;

   order_perf_type out[20];
   out[5].key = 998;
   out[5].val = 999;
   order_perf_type *r =
      boost::movelib::set_difference(range1, range1+5, range2, range2+10, out, order_type_less());
   BOOST_TEST(&out[5] == r);
   BOOST_TEST(out[0].key == 1u);
   BOOST_TEST(out[0].val == 1u);
   BOOST_TEST(out[1].key == 3u);
   BOOST_TEST(out[1].val == 1u);
   BOOST_TEST(out[2].key == 5u);
   BOOST_TEST(out[2].val == 1u);
   BOOST_TEST(out[3].key == 7u);
   BOOST_TEST(out[3].val == 1u);
   BOOST_TEST(out[4].key == 7u);
   BOOST_TEST(out[4].val == 2u);
   BOOST_TEST(out[5].key == 998);
   BOOST_TEST(out[5].val == 999);
}

/*
///////////////////////////////////
//
//      inplace_set_difference
//
///////////////////////////////////
void test_inplace_set_difference_normal()
{
   order_move_type range2[10];
   for(std::size_t i = 0; i != sizeof(range2)/sizeof(*range2); ++i){
      range2[i].key = i*2;
      range2[i].val = 0u;
   }

   order_move_type range1[4];
   range1[0].key = 0u;
   range1[0].val = 1u;
   range1[1].key = 1u;
   range1[1].val = 1u;
   range1[2].key = 3u;
   range1[2].val = 1u;
   range1[3].key = 4u;
   range1[3].val = 1u;

   order_move_type *ret = boost::movelib::inplace_set_difference(range1, range1+4, range2, range2+10, order_type_less());
   BOOST_TEST(ret == range1+2);
   BOOST_TEST(range1[0].key == 1u);
   BOOST_TEST(range1[0].val == 1u);
   BOOST_TEST(range1[1].key == 3u);
   BOOST_TEST(range1[1].val == 1u);
   BOOST_TEST(range1[2].key == order_move_type::moved_assign_mark);
   BOOST_TEST(range1[2].val == order_move_type::moved_assign_mark);
}

void test_inplace_set_difference_range1_repeated()
{
   order_move_type range2[10];
   for(std::size_t i = 0; i != sizeof(range2)/sizeof(*range2); ++i){
      range2[i].key = i*2;
      range2[i].val = 0u;
   }

   order_move_type range1[5];
   range1[0].key = 0u;
   range1[0].val = 1u;
   range1[1].key = 2u;
   range1[1].val = 1u;
   range1[2].key = 4u;
   range1[2].val = 1u;
   range1[3].key = 6u;
   range1[3].val = 1u;
   range1[4].key = order_move_type::moved_assign_mark;
   range1[4].val = order_move_type::moved_assign_mark;

   order_move_type *ret = boost::movelib::inplace_set_difference(range1, range1+4, range2, range2+10, order_type_less());
   BOOST_TEST(ret == range1+0);
   BOOST_TEST(range1[0].key == 0u);
   BOOST_TEST(range1[0].val == 1u);
   BOOST_TEST(range1[1].key == 2u);
   BOOST_TEST(range1[1].val == 1u);
   BOOST_TEST(range1[2].key == 4u);
   BOOST_TEST(range1[3].val == 1u);
   BOOST_TEST(range1[3].key == 6u);
   BOOST_TEST(range1[3].val == 1u);
   BOOST_TEST(range1[4].key == order_move_type::moved_assign_mark);
   BOOST_TEST(range1[4].val == order_move_type::moved_assign_mark);
}

void test_inplace_set_difference_range1_unique()
{
   order_move_type range2[10];
   for(std::size_t i = 0; i != sizeof(range2)/sizeof(*range2); ++i){
      range2[i].key = i*2;
      range2[i].val = 0u;
   }

   order_move_type range1[5];
   range1[0].key = 1u;
   range1[0].val = 1u;
   range1[1].key = 3u;
   range1[1].val = 1u;
   range1[2].key = 5u;
   range1[2].val = 1u;
   range1[3].key = 7u;
   range1[3].val = 1u;
   range1[4].key = order_move_type::moved_assign_mark;
   range1[4].val = order_move_type::moved_assign_mark;

   order_move_type *ret = boost::movelib::inplace_set_difference(range1, range1+4, range2, range2+10, order_type_less());
   BOOST_TEST(ret == range1+4);
   BOOST_TEST(range1[0].key == 1u);
   BOOST_TEST(range1[0].val == 1u);
   BOOST_TEST(range1[1].key == 3u);
   BOOST_TEST(range1[1].val == 1u);
   BOOST_TEST(range1[2].key == 5u);
   BOOST_TEST(range1[3].val == 1u);
   BOOST_TEST(range1[3].key == 7u);
   BOOST_TEST(range1[3].val == 1u);
   BOOST_TEST(range1[4].key == order_move_type::moved_assign_mark);
   BOOST_TEST(range1[4].val == order_move_type::moved_assign_mark);
}

void test_inplace_set_difference_range1_unique_long()
{
   order_move_type range2[10];
   for(std::size_t i = 0; i != sizeof(range2)/sizeof(*range2); ++i){
      range2[i].key = i*2;
      range2[i].val = 0u;
   }

   order_move_type range1[11];
   for(std::size_t i = 0; i != sizeof(range1)/sizeof(*range1); ++i){
      range1[i].key = i*2+1;
      range1[i].val = 1u;
   }

   order_move_type *ret = boost::movelib::inplace_set_difference(range1, range1+11, range2, range2+10, order_type_less());
   BOOST_TEST(ret == range1+11);
   for(std::size_t i = 0; i != sizeof(range1)/sizeof(*range1); ++i){
      BOOST_TEST(range1[i].key == i*2+1);
      BOOST_TEST(range1[i].val == 1u);
   }
}

void test_inplace_set_difference_range1_same_start()
{
   order_move_type range2[10];
   for(std::size_t i = 0; i != sizeof(range2)/sizeof(*range2); ++i){
      range2[i].key = i*2;
      range2[i].val = 0u;
   }

   order_move_type range1[5];
   range1[0].key = 0u;
   range1[0].val = 1u;
   range1[1].key = 2u;
   range1[1].val = 1u;
   range1[2].key = 4u;
   range1[2].val = 1u;
   range1[3].key = 5u;
   range1[3].val = 1u;
   range1[4].key = 7u;
   range1[4].val = 1u;

   order_move_type *ret = boost::movelib::inplace_set_difference(range1, range1+5, range2, range2+10, order_type_less());
   BOOST_TEST(ret == range1+2);
   BOOST_TEST(range1[0].key == 5u);
   BOOST_TEST(range1[0].val == 1u);
   BOOST_TEST(range1[1].key == 7u);
   BOOST_TEST(range1[1].val == 1u);
}

void test_inplace_set_difference_range1_same_end()
{
   order_move_type range2[10];
   for(std::size_t i = 0; i != sizeof(range2)/sizeof(*range2); ++i){
      range2[i].key = i*2;
      range2[i].val = 0u;
   }

   order_move_type range1[5];
   range1[0].key = 1u;
   range1[0].val = 1u;
   range1[1].key = 3u;
   range1[1].val = 1u;
   range1[2].key = 4u;
   range1[2].val = 1u;
   range1[3].key = 6u;
   range1[3].val = 1u;
   range1[4].key = 8u;
   range1[4].val = 1u;

   order_move_type *ret = boost::movelib::inplace_set_difference(range1, range1+5, range2, range2+10, order_type_less());
   BOOST_TEST(ret == range1+2);
   BOOST_TEST(range1[0].key == 1u);
   BOOST_TEST(range1[0].val == 1u);
   BOOST_TEST(range1[1].key == 3u);
   BOOST_TEST(range1[1].val == 1u);
   BOOST_TEST(range1[2].key == 4u);
   BOOST_TEST(range1[2].val == 1u);
   BOOST_TEST(range1[3].key == 6u);
   BOOST_TEST(range1[3].val == 1u);
}
*/

///////////////////////////////////
//
//      inplace_set_difference
//
///////////////////////////////////
void test_inplace_set_difference_normal()
{
   order_move_type range2[10];
   for(std::size_t i = 0; i != sizeof(range2)/sizeof(*range2); ++i){
      range2[i].key = i*2;
      range2[i].val = 0u;
   }

   order_move_type range1[4];
   range1[0].key = 0u;
   range1[0].val = 1u;
   range1[1].key = 1u;
   range1[1].val = 1u;
   range1[2].key = 3u;
   range1[2].val = 1u;
   range1[3].key = 4u;
   range1[3].val = 1u;

   order_move_type *ret = boost::movelib::inplace_set_difference(range1, range1+4, range2, range2+10, order_type_less());
   BOOST_TEST(ret == range1+2);
   BOOST_TEST(range1[0].key == 1u);
   BOOST_TEST(range1[0].val == 1u);
   BOOST_TEST(range1[1].key == 3u);
   BOOST_TEST(range1[1].val == 1u);
   BOOST_TEST(range1[2].key == order_move_type::moved_assign_mark);
   BOOST_TEST(range1[2].val == order_move_type::moved_assign_mark);
}

void test_inplace_set_difference_range1_repeated()
{
   order_move_type range2[10];
   for(std::size_t i = 0; i != sizeof(range2)/sizeof(*range2); ++i){
      range2[i].key = i*2;
      range2[i].val = 0u;
   }

   order_move_type range1[5];
   range1[0].key = 0u;
   range1[0].val = 1u;
   range1[1].key = 2u;
   range1[1].val = 1u;
   range1[2].key = 4u;
   range1[2].val = 1u;
   range1[3].key = 6u;
   range1[3].val = 1u;
   range1[4].key = order_move_type::moved_assign_mark;
   range1[4].val = order_move_type::moved_assign_mark;

   order_move_type *ret = boost::movelib::inplace_set_difference(range1, range1+4, range2, range2+10, order_type_less());
   BOOST_TEST(ret == range1+0);
   BOOST_TEST(range1[0].key == 0u);
   BOOST_TEST(range1[0].val == 1u);
   BOOST_TEST(range1[1].key == 2u);
   BOOST_TEST(range1[1].val == 1u);
   BOOST_TEST(range1[2].key == 4u);
   BOOST_TEST(range1[3].val == 1u);
   BOOST_TEST(range1[3].key == 6u);
   BOOST_TEST(range1[3].val == 1u);
   BOOST_TEST(range1[4].key == order_move_type::moved_assign_mark);
   BOOST_TEST(range1[4].val == order_move_type::moved_assign_mark);
}

void test_inplace_set_difference_range1_unique()
{
   order_move_type range2[10];
   for(std::size_t i = 0; i != sizeof(range2)/sizeof(*range2); ++i){
      range2[i].key = i*2;
      range2[i].val = 0u;
   }

   order_move_type range1[5];
   range1[0].key = 1u;
   range1[0].val = 1u;
   range1[1].key = 3u;
   range1[1].val = 1u;
   range1[2].key = 5u;
   range1[2].val = 1u;
   range1[3].key = 7u;
   range1[3].val = 1u;
   range1[4].key = order_move_type::moved_assign_mark;
   range1[4].val = order_move_type::moved_assign_mark;

   order_move_type *ret = boost::movelib::inplace_set_difference(range1, range1+4, range2, range2+10, order_type_less());
   BOOST_TEST(ret == range1+4);
   BOOST_TEST(range1[0].key == 1u);
   BOOST_TEST(range1[0].val == 1u);
   BOOST_TEST(range1[1].key == 3u);
   BOOST_TEST(range1[1].val == 1u);
   BOOST_TEST(range1[2].key == 5u);
   BOOST_TEST(range1[3].val == 1u);
   BOOST_TEST(range1[3].key == 7u);
   BOOST_TEST(range1[3].val == 1u);
   BOOST_TEST(range1[4].key == order_move_type::moved_assign_mark);
   BOOST_TEST(range1[4].val == order_move_type::moved_assign_mark);
}

void test_inplace_set_difference_range1_unique_long()
{
   order_move_type range2[10];
   for(std::size_t i = 0; i != sizeof(range2)/sizeof(*range2); ++i){
      range2[i].key = i*2;
      range2[i].val = 0u;
   }

   order_move_type range1[11];
   for(std::size_t i = 0; i != sizeof(range1)/sizeof(*range1); ++i){
      range1[i].key = i*2+1;
      range1[i].val = 1u;
   }

   order_move_type *ret = boost::movelib::inplace_set_difference(range1, range1+11, range2, range2+10, order_type_less());
   BOOST_TEST(ret == range1+11);
   for(std::size_t i = 0; i != sizeof(range1)/sizeof(*range1); ++i){
      BOOST_TEST(range1[i].key == i*2+1);
      BOOST_TEST(range1[i].val == 1u);
   }
}

void test_inplace_set_difference_range1_same_start()
{
   order_move_type range2[10];
   for(std::size_t i = 0; i != sizeof(range2)/sizeof(*range2); ++i){
      range2[i].key = i*2;
      range2[i].val = 0u;
   }

   order_move_type range1[5];
   range1[0].key = 0u;
   range1[0].val = 1u;
   range1[1].key = 2u;
   range1[1].val = 1u;
   range1[2].key = 4u;
   range1[2].val = 1u;
   range1[3].key = 5u;
   range1[3].val = 1u;
   range1[4].key = 7u;
   range1[4].val = 1u;

   order_move_type *ret = boost::movelib::inplace_set_difference(range1, range1+5, range2, range2+10, order_type_less());
   BOOST_TEST(ret == range1+2);
   BOOST_TEST(range1[0].key == 5u);
   BOOST_TEST(range1[0].val == 1u);
   BOOST_TEST(range1[1].key == 7u);
   BOOST_TEST(range1[1].val == 1u);
}

void test_inplace_set_difference_range1_same_end()
{
   order_move_type range2[10];
   for(std::size_t i = 0; i != sizeof(range2)/sizeof(*range2); ++i){
      range2[i].key = i*2;
      range2[i].val = 0u;
   }

   order_move_type range1[5];
   range1[0].key = 1u;
   range1[0].val = 1u;
   range1[1].key = 3u;
   range1[1].val = 1u;
   range1[2].key = 4u;
   range1[2].val = 1u;
   range1[3].key = 6u;
   range1[3].val = 1u;
   range1[4].key = 8u;
   range1[4].val = 1u;

   order_move_type *ret = boost::movelib::inplace_set_difference(range1, range1+5, range2, range2+10, order_type_less());
   BOOST_TEST(ret == range1+2);
   BOOST_TEST(range1[0].key == 1u);
   BOOST_TEST(range1[0].val == 1u);
   BOOST_TEST(range1[1].key == 3u);
   BOOST_TEST(range1[1].val == 1u);
   BOOST_TEST(range1[2].key == 4u);
   BOOST_TEST(range1[2].val == 1u);
   BOOST_TEST(range1[3].key == 6u);
   BOOST_TEST(range1[3].val == 1u);
}


///////////////////////////////////
//
//      set_unique_difference
//
///////////////////////////////////
void test_set_unique_difference_normal()
{
   order_perf_type range2[10];
   for(std::size_t i = 0; i != sizeof(range2)/sizeof(*range2); ++i){
      range2[i].key = i*2;
      range2[i].val = 0u;
   }

   order_perf_type range1[10];
   range1[0].key = 0u;
   range1[0].val = 1u;
   range1[1].key = 1u;
   range1[1].val = 1u;
   range1[2].key = 1u;
   range1[2].val = 2u;
   range1[3].key = 3u;
   range1[3].val = 1u;
   range1[4].key = 4u;
   range1[4].val = 1u;
   range1[5].key = 4u;
   range1[5].val = 2u;
   range1[6].key = 21u;
   range1[6].val = 1u;
   range1[7].key = 21u;
   range1[7].val = 2u;
   range1[8].key = 23u;
   range1[8].val = 1u;
   range1[9].key = 23u;
   range1[9].val = 2u;

   order_perf_type out[20];
   out[4].key = 998;
   out[4].val = 999;
   order_perf_type * r =
      boost::movelib::set_unique_difference(range1, range1+10, range2, range2+10, out, order_type_less());
   BOOST_TEST(&out[4] == r);
   BOOST_TEST(out[0].key == 1u);
   BOOST_TEST(out[0].val == 1u);
   BOOST_TEST(out[1].key == 3u);
   BOOST_TEST(out[1].val == 1u);
   BOOST_TEST(out[2].key == 21u);
   BOOST_TEST(out[2].val == 1u);
   BOOST_TEST(out[3].key == 23u);
   BOOST_TEST(out[3].val == 1u);
   BOOST_TEST(out[4].key == 998);
   BOOST_TEST(out[4].val == 999);
}

void test_set_unique_difference_range1_repeated()
{
   order_perf_type range2[10];
   for(std::size_t i = 0; i != sizeof(range2)/sizeof(*range2); ++i){
      range2[i].key = i*2;
      range2[i].val = 0u;
   }

   order_perf_type range1[11];
   range1[0].key = 0u;
   range1[0].val = 1u;
   range1[1].key = 0u;
   range1[1].val = 2u;
   range1[2].key = 0u;
   range1[2].val = 2u;
   range1[3].key = 2u;
   range1[3].val = 1u;
   range1[4].key = 2u;
   range1[4].val = 2u;
   range1[5].key = 4u;
   range1[5].val = 1u;
   range1[6].key = 6u;
   range1[6].val = 1u;
   range1[7].key = 6u;
   range1[7].val = 2u;
   range1[8].key = 6u;
   range1[8].val = 3u;
   range1[9].key = 6u;
   range1[9].val = 4u;
   range1[10].key = 6u;
   range1[10].val = 5u;

   order_perf_type out[20];
   out[0].key = 998;
   out[0].val = 999;
   order_perf_type * r =
      boost::movelib::set_unique_difference(range1, range1+11, range2, range2+10, out, order_type_less());
   BOOST_TEST(&out[0] == r);
   BOOST_TEST(out[0].key == 998);
   BOOST_TEST(out[0].val == 999);
}

void test_set_unique_difference_range1_unique()
{
   order_perf_type range2[10];
   for(std::size_t i = 0; i != sizeof(range2)/sizeof(*range2); ++i){
      range2[i].key = i*2;
      range2[i].val = 0u;
   }

   order_perf_type range1[7];
   range1[0].key = 1u;
   range1[0].val = 1u;
   range1[1].key = 3u;
   range1[1].val = 1u;
   range1[2].key = 3u;
   range1[2].val = 2u;
   range1[3].key = 5u;
   range1[3].val = 1u;
   range1[4].key = 7u;
   range1[4].val = 1u;
   range1[5].key = 7u;
   range1[5].val = 2u;
   range1[6].key = 7u;
   range1[6].val = 3u;

   order_perf_type out[20];
   out[4].key = 998;
   out[4].val = 999;
   order_perf_type * r =
      boost::movelib::set_unique_difference(range1, range1+7, range2, range2+10, out, order_type_less());
   BOOST_TEST(&out[4] == r);
   BOOST_TEST(out[0].key == 1u);
   BOOST_TEST(out[0].val == 1u);
   BOOST_TEST(out[1].key == 3u);
   BOOST_TEST(out[1].val == 1u);
   BOOST_TEST(out[2].key == 5u);
   BOOST_TEST(out[2].val == 1u);
   BOOST_TEST(out[3].key == 7u);
   BOOST_TEST(out[3].val == 1u);
   BOOST_TEST(out[4].key == 998);
   BOOST_TEST(out[4].val == 999);
}

///////////////////////////////////
//
//      inplace_set_unique_difference
//
///////////////////////////////////
void test_inplace_set_unique_difference_normal()
{
   order_move_type range2[10];
   for(std::size_t i = 0; i != sizeof(range2)/sizeof(*range2); ++i){
      range2[i].key = i*2;
      range2[i].val = 0u;
   }

   order_move_type range1[4];
   range1[0].key = 0u;
   range1[0].val = 1u;
   range1[1].key = 1u;
   range1[1].val = 1u;
   range1[2].key = 3u;
   range1[2].val = 1u;
   range1[3].key = 4u;
   range1[3].val = 1u;

   order_move_type *ret = boost::movelib::inplace_set_unique_difference(range1, range1+4, range2, range2+10, order_type_less());
   BOOST_TEST(ret == range1+2);
   BOOST_TEST(range1[0].key == 1u);
   BOOST_TEST(range1[0].val == 1u);
   BOOST_TEST(range1[1].key == 3u);
   BOOST_TEST(range1[1].val == 1u);
   BOOST_TEST(range1[2].key == order_move_type::moved_assign_mark);
   BOOST_TEST(range1[2].val == order_move_type::moved_assign_mark);
}

void test_inplace_set_unique_difference_range1_repeated()
{
   order_move_type range2[10];
   for(std::size_t i = 0; i != sizeof(range2)/sizeof(*range2); ++i){
      range2[i].key = i*2;
      range2[i].val = 0u;
   }

   order_move_type range1[5];
   range1[0].key = 0u;
   range1[0].val = 1u;
   range1[1].key = 2u;
   range1[1].val = 1u;
   range1[2].key = 4u;
   range1[2].val = 1u;
   range1[3].key = 6u;
   range1[3].val = 1u;
   range1[4].key = order_move_type::moved_assign_mark;
   range1[4].val = order_move_type::moved_assign_mark;

   order_move_type *ret = boost::movelib::inplace_set_unique_difference(range1, range1+4, range2, range2+10, order_type_less());
   BOOST_TEST(ret == range1+0);
   BOOST_TEST(range1[0].key == 0u);
   BOOST_TEST(range1[0].val == 1u);
   BOOST_TEST(range1[1].key == 2u);
   BOOST_TEST(range1[1].val == 1u);
   BOOST_TEST(range1[2].key == 4u);
   BOOST_TEST(range1[3].val == 1u);
   BOOST_TEST(range1[3].key == 6u);
   BOOST_TEST(range1[3].val == 1u);
   BOOST_TEST(range1[4].key == order_move_type::moved_assign_mark);
   BOOST_TEST(range1[4].val == order_move_type::moved_assign_mark);
}

void test_inplace_set_unique_difference_range1_unique()
{
   order_move_type range2[10];
   for(std::size_t i = 0; i != sizeof(range2)/sizeof(*range2); ++i){
      range2[i].key = i*2;
      range2[i].val = 0u;
   }

   order_move_type range1[9];
   range1[0].key = 1u;
   range1[0].val = 1u;
   range1[1].key = 1u;
   range1[1].val = 2u;
   range1[2].key = 3u;
   range1[2].val = 1u;
   range1[3].key = 3u;
   range1[3].val = 2u;
   range1[4].key = 5u;
   range1[4].val = 1u;
   range1[5].key = 7u;
   range1[5].val = 1u;
   range1[6].key = 7u;
   range1[6].val = 2u;
   range1[7].key = 7u;
   range1[7].val = 3u;
   range1[8].val = 3u;
   range1[8].key = order_move_type::moved_assign_mark;
   range1[8].val = order_move_type::moved_assign_mark;

   order_move_type *ret =
      boost::movelib::inplace_set_unique_difference(range1, range1+8, range2, range2+10, order_type_less());
   BOOST_TEST(ret == range1+4);
   BOOST_TEST(range1[0].key == 1u);
   BOOST_TEST(range1[0].val == 1u);
   BOOST_TEST(range1[1].key == 3u);
   BOOST_TEST(range1[1].val == 1u);
   BOOST_TEST(range1[2].key == 5u);
   BOOST_TEST(range1[3].val == 1u);
   BOOST_TEST(range1[3].key == 7u);
   BOOST_TEST(range1[3].val == 1u);
}

void test_inplace_set_unique_difference_range1_unique_long()
{
   order_move_type range2[10];
   for(std::size_t i = 0; i != sizeof(range2)/sizeof(*range2); ++i){
      range2[i].key = i*2;
      range2[i].val = 0u;
   }

   order_move_type range1[22];
   for(std::size_t i = 0; i != sizeof(range1)/sizeof(*range1); ++i){
      range1[i].key = (i/2)*2+1;
      range1[i].val = i%2;
   }

   order_move_type *ret =
      boost::movelib::inplace_set_unique_difference(range1, range1+22, range2, range2+10, order_type_less());
   BOOST_TEST(ret == range1+11);
   for(std::size_t i = 0; i != 11; ++i){
      BOOST_TEST(range1[i].key == i*2+1);
      BOOST_TEST(range1[i].val == 0u);
   }
}

void test_inplace_set_unique_difference_range1_same_start()
{
   order_move_type range2[10];
   for(std::size_t i = 0; i != sizeof(range2)/sizeof(*range2); ++i){
      range2[i].key = i*2;
      range2[i].val = 0u;
   }

   order_move_type range1[6];
   range1[0].key = 0u;
   range1[0].val = 1u;
   range1[1].key = 2u;
   range1[1].val = 1u;
   range1[2].key = 4u;
   range1[2].val = 1u;
   range1[3].key = 4u;
   range1[3].val = 2u;
   range1[4].key = 5u;
   range1[4].val = 1u;
   range1[5].key = 7u;
   range1[5].val = 1u;

   order_move_type *ret =
      boost::movelib::inplace_set_unique_difference(range1, range1+6, range2, range2+10, order_type_less());
   BOOST_TEST(ret == range1+2);
   BOOST_TEST(range1[0].key == 5u);
   BOOST_TEST(range1[0].val == 1u);
   BOOST_TEST(range1[1].key == 7u);
   BOOST_TEST(range1[1].val == 1u);
}

void test_inplace_set_unique_difference_range1_same_end()
{
   order_move_type range2[10];
   for(std::size_t i = 0; i != sizeof(range2)/sizeof(*range2); ++i){
      range2[i].key = i*2;
      range2[i].val = 0u;
   }

   order_move_type range1[8];
   range1[0].key = 1u;
   range1[0].val = 1u;
   range1[1].key = 3u;
   range1[1].val = 1u;
   range1[2].key = 4u;
   range1[2].val = 1u;
   range1[3].key = 4u;
   range1[3].val = 2u;
   range1[4].key = 6u;
   range1[4].val = 1u;
   range1[5].key = 8u;
   range1[5].val = 1u;
   range1[6].key = 8u;
   range1[6].val = 2u;
   range1[7].key = 8u;
   range1[7].val = 3u;

   order_move_type *ret =
      boost::movelib::inplace_set_unique_difference(range1, range1+8, range2, range2+10, order_type_less());
   BOOST_TEST(ret == range1+2);
   BOOST_TEST(range1[0].key == 1u);
   BOOST_TEST(range1[0].val == 1u);
   BOOST_TEST(range1[1].key == 3u);
   BOOST_TEST(range1[1].val == 1u);
}

int main()
{
   //set_difference
   test_set_difference_normal();
   test_set_difference_range1_repeated();
   test_set_difference_range1_unique();
   //inplace_set_difference
   test_inplace_set_difference_normal();
   test_inplace_set_difference_range1_repeated();
   test_inplace_set_difference_range1_unique();
   test_inplace_set_difference_range1_unique_long();
   test_inplace_set_difference_range1_same_start();
   test_inplace_set_difference_range1_same_end();
   //set_unique_difference
   test_set_unique_difference_normal();
   test_set_unique_difference_range1_repeated();
   test_set_unique_difference_range1_unique();
   //inplace_set_unique_difference
   test_inplace_set_unique_difference_normal();
   test_inplace_set_unique_difference_range1_repeated();
   test_inplace_set_unique_difference_range1_unique();
   test_inplace_set_unique_difference_range1_unique_long();
   test_inplace_set_unique_difference_range1_same_start();
   test_inplace_set_unique_difference_range1_same_end();

   return boost::report_errors();
}
