//  bll_and_function.cpp  - The Boost Lambda Library -----------------------
//
// Copyright (C) 2000-2003 Jaakko Jarvi (jaakko.jarvi@cs.utu.fi)
// Copyright (C) 2000-2003 Gary Powell (powellg@amazon.com)
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
//
// For more information, see www.boost.org

// test using BLL and boost::function

#include <boost/core/lightweight_test.hpp>

#include "boost/lambda/lambda.hpp"

#include "boost/function.hpp"

#include <vector>
#include <map>
#include <set>
#include <string>


using namespace boost::lambda;

using namespace std;

void test_function() {

  boost::function<int (int, int)> f;
  f = _1 + _2;

 BOOST_TEST_EQ(f(1, 2), 3);

 int i=1; int j=2;
 boost::function<int& (int&, int)> g = _1 += _2;
 g(i, j);
 BOOST_TEST_EQ(i, 3);



  int* sum = new int();
  *sum = 0;
  boost::function<int& (int)> counter = *sum += _1;
  counter(5); // ok, sum* = 5;
  BOOST_TEST_EQ(*sum, 5);
  delete sum; 
  
  // The next statement would lead to a dangling reference
  // counter(3); // error, *sum does not exist anymore

}


int main()
{
  test_function();

  return boost::report_errors();
}
