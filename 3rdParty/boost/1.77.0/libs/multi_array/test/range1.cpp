// Copyright 2002 The Trustees of Indiana University.

// Use, modification and distribution is subject to the Boost Software 
// License, Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

//  Boost.MultiArray Library
//  Authors: Ronald Garcia
//           Jeremy Siek
//           Andrew Lumsdaine
//  See http://www.boost.org/libs/multi_array for documentation.

//
// range1.cpp - test of index_range
//


#include <boost/multi_array/index_range.hpp>

#include <boost/core/lightweight_test.hpp>

#include <boost/array.hpp>
#include <cstddef>

int
main()
{
  typedef boost::detail::multi_array::index_range<int,std::size_t> range;

  {
    // typical range creation and extraction
    range r1(-3,5);
    BOOST_TEST(r1.start() == -3);
    BOOST_TEST(r1.finish() == 5);
    BOOST_TEST(r1.stride() == 1);
    BOOST_TEST(r1.size(0) == 8);
    BOOST_TEST(!r1.is_degenerate());
    BOOST_TEST(r1.get_start(0) == -3);
    BOOST_TEST(r1.get_finish(100) == 5);
  }

  {
    range r2(-3,5,2);
    BOOST_TEST(r2.start() == -3);
    BOOST_TEST(r2.finish() == 5);
    BOOST_TEST(r2.stride() == 2);
    BOOST_TEST(r2.size(0) == 4);
    BOOST_TEST(!r2.is_degenerate());
  }

  {
    // degenerate creation
    range r3(5);
    BOOST_TEST(r3.start() == 5);
    BOOST_TEST(r3.finish() == 6);
    BOOST_TEST(r3.stride() == 1);
    BOOST_TEST(r3.size(0) == 1);
    BOOST_TEST(r3.is_degenerate());
  }

  {
    // default range creation
    range r4;
    BOOST_TEST(r4.get_start(0) == 0);
    BOOST_TEST(r4.get_finish(100) == 100);
    BOOST_TEST(r4.stride() == 1);
    BOOST_TEST(r4.size(0) == 0);
  }

  {
    // create a range using the setter methods
    range r5 = range().stride(2).start(-3).finish(7);
    BOOST_TEST(r5.start() == -3);
    BOOST_TEST(r5.stride() == 2);
    BOOST_TEST(r5.finish() == 7);
    BOOST_TEST(r5.size(0) == 5);
  }

  // try out all the comparison operators
  {
    range r6 = -3 <= range().stride(2) < 7;
    BOOST_TEST(r6.start() == -3);
    BOOST_TEST(r6.stride() == 2);
    BOOST_TEST(r6.finish() == 7);
    BOOST_TEST(r6.size(0) == 5);
  }

  {
    range r7 = -3 < range() <= 7;
    BOOST_TEST(r7.start() == -2);
    BOOST_TEST(r7.stride() == 1);
    BOOST_TEST(r7.finish() == 8);
    BOOST_TEST(r7.size(0) == 10);
  }

  // arithmetic operators
  {
    range r8 = range(0,5) + 2;
    BOOST_TEST(r8.start() == 2);
    BOOST_TEST(r8.stride() == 1);
    BOOST_TEST(r8.finish() == 7);
    BOOST_TEST(r8.size(0) == 5);
  }

  {
    range r9 = range(0,5) - 2;
    BOOST_TEST(r9.start() == -2);
    BOOST_TEST(r9.stride() == 1);
    BOOST_TEST(r9.finish() == 3);
    BOOST_TEST(r9.size(0) == 5);
  }

  return boost::report_errors();
}
