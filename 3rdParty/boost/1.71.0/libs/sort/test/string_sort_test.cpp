//  Boost Sort library string_sort_test.cpp file  ----------------------------//

//  Copyright Steven Ross 2009. Use, modification and
//  distribution is subject to the Boost Software License, Version
//  1.0. (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

//  See http://www.boost.org/libs/sort for library home page.

#include <boost/sort/spreadsort/detail/string_sort.hpp>
#include <boost/sort/spreadsort/string_sort.hpp>
#include <boost/sort/spreadsort/spreadsort.hpp>
// Include unit test framework
#include <boost/test/included/test_exec_monitor.hpp>
#include <boost/test/test_tools.hpp>
#include <vector>
#include <string>


using namespace std;
using namespace boost::sort::spreadsort;
using boost::sort::spreadsort::detail::offset_less_than;
using boost::sort::spreadsort::detail::offset_greater_than;
using boost::sort::spreadsort::detail::update_offset;

struct bracket {
  unsigned char operator()(const string &x, size_t offset) const {
    return x[offset];
  }
};

struct wbracket {
  wchar_t operator()(const wstring &x, size_t offset) const {
    return x[offset];
  }
};

struct get_size {
  size_t operator()(const string &x) const{ return x.size(); }
};

struct wget_size {
  size_t operator()(const wstring &x) const{ return x.size(); }
};

static const unsigned input_count = 100000;

// Test that update_offset finds the first character with a difference.
void update_offset_test() {
  vector<string> input;
  input.push_back("test1");
  input.push_back("test2");
  size_t char_offset = 1;
  update_offset<vector<string>::iterator, unsigned char>(input.begin(),
                                                         input.end(),
                                                         char_offset);
  BOOST_CHECK(char_offset == 4);

  // Functor version
  char_offset = 1;
  update_offset(input.begin(), input.end(), char_offset, bracket(), get_size());
  BOOST_CHECK(char_offset == 4);
}

// Test that offset comparison operators only look after the offset.
void offset_comparison_test() {
  string input1 = "ab";
  string input2 = "ba";
  string input3 = "aba";
  offset_less_than<string, unsigned char> less_than(0);
  offset_greater_than<string, unsigned char> greater_than(0);
  BOOST_CHECK(less_than(input1, input2));
  BOOST_CHECK(less_than(input1, input3));
  BOOST_CHECK(!less_than(input2, input1));
  BOOST_CHECK(!less_than(input1, input1));
  BOOST_CHECK(!greater_than(input1, input2));
  BOOST_CHECK(!greater_than(input1, input3));
  BOOST_CHECK(greater_than(input2, input1));
  BOOST_CHECK(!greater_than(input1, input1));

  // Offset comparisons only check after the specified offset.
  offset_less_than<string, unsigned char> offset_less(1);
  offset_greater_than<string, unsigned char> offset_greater(1);
  BOOST_CHECK(!offset_less(input1, input2));
  BOOST_CHECK(offset_less(input1, input3));
  BOOST_CHECK(offset_less(input2, input1));
  BOOST_CHECK(!offset_less(input1, input1));
  BOOST_CHECK(offset_greater(input1, input2));
  BOOST_CHECK(!offset_greater(input1, input3));
  BOOST_CHECK(!offset_greater(input2, input1));
  BOOST_CHECK(!offset_greater(input1, input1));
}

void string_test()
{
  // Prepare inputs
  vector<string> base_vec;
  const unsigned max_length = 32;
  srand(1);
  //Generating semirandom numbers
  for (unsigned u = 0; u < input_count; ++u) {
    unsigned length = rand() % max_length;
    string result;
    for (unsigned v = 0; v < length; ++v) {
      result.push_back(rand() % 256);
    }
    base_vec.push_back(result);
  }
  vector<string> sorted_vec = base_vec;
  vector<string> test_vec = base_vec;
  std::sort(sorted_vec.begin(), sorted_vec.end());
  //Testing basic call
  string_sort(test_vec.begin(), test_vec.end());
  BOOST_CHECK(test_vec == sorted_vec);
  //Testing boost::sort::spreadsort wrapper
  test_vec = base_vec;
  boost::sort::spreadsort::spreadsort(test_vec.begin(), test_vec.end());
  BOOST_CHECK(test_vec == sorted_vec);
  //Character functors
  test_vec = base_vec;
  string_sort(test_vec.begin(), test_vec.end(), bracket(), get_size());
  BOOST_CHECK(test_vec == sorted_vec);
  //All functors
  test_vec = base_vec;
  string_sort(test_vec.begin(), test_vec.end(), bracket(), get_size(),
              less<string>());
  BOOST_CHECK(test_vec == sorted_vec);
  //reverse order
  std::sort(sorted_vec.begin(), sorted_vec.end(), greater<string>());
  reverse_string_sort(test_vec.begin(), test_vec.end(), greater<string>());
  BOOST_CHECK(test_vec == sorted_vec);
  //reverse order with functors
  test_vec = base_vec;
  reverse_string_sort(test_vec.begin(), test_vec.end(), bracket(), get_size(),
                      greater<string>());
  BOOST_CHECK(test_vec == sorted_vec);  
}

// Verify that 0, 1, and input_count empty strings all sort correctly.
void corner_test() {
  vector<string> test_vec;
  boost::sort::spreadsort::spreadsort(test_vec.begin(), test_vec.end());
  test_vec.resize(1);
  boost::sort::spreadsort::spreadsort(test_vec.begin(), test_vec.end());
  BOOST_CHECK(test_vec[0].empty());
  test_vec.resize(input_count);
  boost::sort::spreadsort::spreadsort(test_vec.begin(), test_vec.end());
  BOOST_CHECK(test_vec.size() == input_count);
  for (unsigned i = 0; i < test_vec.size(); ++i) {
    BOOST_CHECK(test_vec[i].empty());
  }
}

// test main 
int test_main( int, char*[] )
{
  update_offset_test();
  offset_comparison_test();
  string_test();
  corner_test();
  return 0;
}
