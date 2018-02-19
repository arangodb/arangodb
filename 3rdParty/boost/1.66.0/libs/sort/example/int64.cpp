// spreadsort 64-bit integer sorting example.
//
//  Copyright Steven Ross 2009-2014.
//
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

//  See http://www.boost.org/libs/sort for library home page.

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

//[int64bit_1
#define DATA_TYPE boost::int64_t
//] [/int64bit_1]

//Pass in an argument to test std::sort
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
    while (input.good() && v < uCount)
      input.read(reinterpret_cast<char *>(&(array[v++])), uSize );
    if (v < uCount)
      array.resize(v);
    clock_t start, end;
    double elapsed;
    start = clock();
    if (stdSort)
      //std::sort(&(array[0]), &(array[0]) + uCount);
      std::sort(array.begin(), array.end());
    else
      //boost::sort::spreadsort::spreadsort(&(array[0]), &(array[0]) + uCount);
//[int64bit_2
      boost::sort::spreadsort::spreadsort(array.begin(), array.end());
//] [/int64bit_2]
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
        ofile.write(reinterpret_cast<char *>(&(array[v])), sizeof(array[v]) );
      }
      ofile.close();
    }
    total += elapsed;
    array.clear();
  }
  input.close();
  if (stdSort)
    printf("std::sort elapsed time %f\n", total / CLOCKS_PER_SEC);
  else
    printf("spreadsort elapsed time %f\n", total / CLOCKS_PER_SEC);
  return 0;
}
