//  Boost Sort library int_test.cpp file  ------------------------------------//

//  Copyright Steven Ross 2009-2014. Use, modification and
//  distribution is subject to the Boost Software License, Version
//  1.0. (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

//  See http://www.boost.org/libs/sort for library home page.

#include <boost/cstdint.hpp>
#include <boost/sort/spreadsort/spreadsort.hpp>
// Include unit test framework
#include <boost/test/included/test_exec_monitor.hpp>
#include <boost/test/test_tools.hpp>
#include <vector>

#include <iostream>


using namespace std;
using namespace boost::sort::spreadsort;

struct rightshift {
  int operator()(int x, unsigned offset) { return x >> offset; }
};

struct rightshift_max {
  boost::intmax_t operator()(const boost::intmax_t &x, unsigned offset) {
    return x >> offset;
  }
};

struct negrightshift {
  int operator()(const int &x, const unsigned offset) { return -(x >> offset); }
};

struct negrightshift_max {
  boost::intmax_t operator()(const boost::intmax_t &x, const unsigned offset) {
    return -(x >> offset);
  }
};

boost::int32_t 
rand_32(bool sign = true) {
   boost::int32_t result = rand() | (rand()<< 16);
   if (rand() % 2)
     result |= 1 << 15;
   //Adding the sign bit
   if (sign && (rand() % 2))
     result *= -1;
   return result;
}

void int_test()
{
  // Prepare inputs
  vector<int> base_vec;
  unsigned count = 100000;
  srand(1);
  //Generating semirandom numbers
  for (unsigned u = 0; u < count; ++u)
    base_vec.push_back(rand_32());
  vector<int> sorted_vec = base_vec;
  vector<int> test_vec = base_vec;
  std::sort(sorted_vec.begin(), sorted_vec.end());
  //Testing basic call
  integer_sort(test_vec.begin(), test_vec.end());
  BOOST_CHECK(test_vec == sorted_vec);
  //boost::sort::spreadsort variant
  test_vec = base_vec;
  boost::sort::spreadsort::spreadsort(test_vec.begin(), test_vec.end());
  BOOST_CHECK(test_vec == sorted_vec);
  //One functor
  test_vec = base_vec;
  integer_sort(test_vec.begin(), test_vec.end(), rightshift());
  BOOST_CHECK(test_vec == sorted_vec);
  //Both functors
  test_vec = base_vec;
  integer_sort(test_vec.begin(), test_vec.end(), rightshift(), less<int>());
  BOOST_CHECK(test_vec == sorted_vec);
  //reverse order
  std::sort(sorted_vec.begin(), sorted_vec.end(), greater<int>());
  integer_sort(test_vec.begin(), test_vec.end(), negrightshift(),
               greater<int>());
  BOOST_CHECK(test_vec == sorted_vec);

  //Making sure we're correctly sorting boost::intmax_ts; should use std::sort
  vector<boost::intmax_t> long_base_vec;
  for (unsigned u = 0; u < base_vec.size(); ++u)
    long_base_vec.push_back((((boost::intmax_t)rand_32()) <<
                             ((8 * sizeof(int)) -1)) + rand_32(false));
  vector<boost::intmax_t> long_sorted_vec = long_base_vec;
  vector<boost::intmax_t> long_test_vec = long_base_vec;
  integer_sort(long_test_vec.begin(), long_test_vec.end());
  std::sort(long_sorted_vec.begin(), long_sorted_vec.end());
  BOOST_CHECK(long_test_vec == long_sorted_vec);
  //One functor
  long_test_vec = long_base_vec;
  integer_sort(long_test_vec.begin(), long_test_vec.end(), rightshift_max());
  BOOST_CHECK(long_test_vec == long_sorted_vec);
  //Both functors
  long_test_vec = long_base_vec;
  integer_sort(long_test_vec.begin(), long_test_vec.end(), rightshift_max(),
               less<boost::intmax_t>());
  BOOST_CHECK(long_test_vec == long_sorted_vec);
  //reverse order
  std::sort(long_sorted_vec.begin(), long_sorted_vec.end(),
            greater<boost::intmax_t>());
  integer_sort(long_test_vec.begin(), long_test_vec.end(), negrightshift_max(),
               greater<boost::intmax_t>());
  BOOST_CHECK(long_test_vec == long_sorted_vec);
}

// Verify that 0 and 1 elements work correctly.
void corner_test() {
  vector<int> test_vec;
  boost::sort::spreadsort::spreadsort(test_vec.begin(), test_vec.end());
  const int test_value = 42;
  test_vec.push_back(test_value);
  boost::sort::spreadsort::spreadsort(test_vec.begin(), test_vec.end());
  BOOST_CHECK(test_vec.size() == 1);
  BOOST_CHECK(test_vec[0] == test_value);
}

// test main 
int test_main( int, char*[] )
{
  int_test();
  corner_test();    
  return 0;
}
