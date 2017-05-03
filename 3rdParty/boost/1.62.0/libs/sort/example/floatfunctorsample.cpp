// spreadsort float functor sorting example.
//
//  Copyright Steven Ross 2009.
//
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

//  See http://www.boost.org/libs/sort for library home page.

// Caution: this file contains Quickbook markup as well as code
// and comments, don't change any of the special comment markups!

#include <boost/sort/spreadsort/spreadsort.hpp>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <algorithm>
#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include <iostream>

using namespace boost::sort::spreadsort;

//[float_functor_types
#define CAST_TYPE int
#define KEY_TYPE float
//] [/float_functor_types]


//[float_functor_datatypes
struct DATA_TYPE {
    KEY_TYPE key;
    std::string data;
};
//] [/float_functor_datatypes]


//[float_functor_rightshift
// Casting to an integer before bitshifting
struct rightshift {
  int operator()(const DATA_TYPE &x, const unsigned offset) const {
    return float_mem_cast<KEY_TYPE, CAST_TYPE>(x.key) >> offset;
  }
};
//] [/float_functor_rightshift]

//[float_functor_lessthan
struct lessthan {
  bool operator()(const DATA_TYPE &x, const DATA_TYPE &y) const {
    return x.key < y.key;
  }
};
//] [/float_functor_lessthan]

// Pass in an argument to test std::sort
// Note that this converts NaNs and -0.0 to 0.0, so that sorting results are
// identical every time
int main(int argc, const char ** argv) {
  size_t uCount,uSize=sizeof(DATA_TYPE);
  bool stdSort = false;
  unsigned loopCount = 1;
  for (int u = 1; u < argc; ++u) {
    if (std::string(argv[u]) == "-std")
      stdSort = true;
    else
      loopCount = atoi(argv[u]);
  }
  std::ifstream input("input.txt", std::ios_base::in | std::ios_base::binary);
  if (input.fail()) {
    printf("input.txt could not be opened\n");
    return 1;
  }
  double total = 0.0;
  std::vector<DATA_TYPE> array;
  input.seekg (0, std::ios_base::end);
    size_t length = input.tellg();
  uCount = length/uSize;
  //Run multiple loops, if requested
  for (unsigned u = 0; u < loopCount; ++u) {
    input.seekg (0, std::ios_base::beg);
    //Conversion to a vector
    array.resize(uCount);
    unsigned v = 0;
    while (input.good() && v < uCount) {
      input.read(reinterpret_cast<char *>(&(array[v].key)),
                 sizeof(array[v].key));
     //Checking for denormalized numbers; float_sort looks too fast on them.
     if (!(float_mem_cast<KEY_TYPE, CAST_TYPE>(array[v].key) & 0x7f800000)) {
       //Make the top exponent bit high
       CAST_TYPE temp = 0x40000000 |
         float_mem_cast<KEY_TYPE, CAST_TYPE>(array[v].key);
       memcpy(&(array[v].key), &temp, sizeof(KEY_TYPE));
     }
     //Testcase doesn't sort NaNs; they just cause confusion
     if (!(array[v].key < 0.0) && !(0.0 < array[v].key))
       array[v].key = 0.0;
     //Adding the data, in this case a string
     std::stringstream intstr;
     intstr << array[v].key;
     array[v].data = intstr.str();
     ++v;
    }
    clock_t start, end;
    double elapsed;
    start = clock();
    if (stdSort)
      std::sort(array.begin(), array.end(), lessthan());
    else
      float_sort(array.begin(), array.end(), rightshift(), lessthan());
    end = clock();
    elapsed = static_cast<double>(end - start) ;
    std::ofstream ofile;
    if (stdSort)
      ofile.open("standard_sort_out.txt", std::ios_base::out |
                 std::ios_base::binary | std::ios_base::trunc);
    else
      ofile.open("boost_sort_out.txt", std::ios_base::out |
                 std::ios_base::binary | std::ios_base::trunc);
    if (ofile.good()) {
      for (unsigned v = 0; v < array.size(); ++v) {
        ofile.write(reinterpret_cast<char *>(&(array[v].key)),
                    sizeof(array[v].key));
        ofile << array[v].data;
      }
      ofile.close();
    }
    total += elapsed;
    array.clear();
  }
  if (stdSort)
    printf("std::sort elapsed time %f\n", total / CLOCKS_PER_SEC);
  else
    printf("spreadsort elapsed time %f\n", total / CLOCKS_PER_SEC);
  return 0;
}
