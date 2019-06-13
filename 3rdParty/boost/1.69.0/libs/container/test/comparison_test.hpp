//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga 2017-2017. Distributed under the Boost
// Software License, Version 1.0. (See accompanying file
// LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/container for documentation.
//
//////////////////////////////////////////////////////////////////////////////

#ifndef BOOST_CONTAINER_TEST_COMPARISON_TEST_HPP
#define BOOST_CONTAINER_TEST_COMPARISON_TEST_HPP

#include <deque>
#include <boost/core/lightweight_test.hpp>

namespace boost {
namespace container {
namespace test {


template<class Cont>
bool test_container_comparisons()
{
   typedef typename Cont::value_type value_type;

   Cont cont;
   cont.push_back(value_type(1));
   cont.push_back(value_type(2));
   cont.push_back(value_type(3));

   Cont cont_equal(cont);

   Cont cont_less;
   cont_less.push_back(value_type(1));
   cont_less.push_back(value_type(2));
   cont_less.push_back(value_type(2));

   BOOST_TEST(cont == cont_equal);
   BOOST_TEST(!(cont != cont_equal));
   BOOST_TEST(cont != cont_less);
   BOOST_TEST(cont_less < cont);
   BOOST_TEST(cont_less <= cont);
   BOOST_TEST(!(cont_less > cont));
   BOOST_TEST(!(cont_less >= cont));
   BOOST_TEST(!(cont < cont_less));
   BOOST_TEST(!(cont <= cont_less));
   BOOST_TEST(cont > cont_less);
   BOOST_TEST(cont >= cont_less);

   return ::boost::report_errors() == 0;
}

}  //namespace test {
}  //namespace container {
}  //namespace boost {

#endif   //#ifndef BOOST_CONTAINER_TEST_COMPARISON_TEST_HPP
