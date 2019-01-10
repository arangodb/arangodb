// spreadsort key and data sorting example.
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

struct DATA_TYPE {
    int key;
    std::string data;
    };
//functor example
struct lessthan {
  inline bool operator()(const DATA_TYPE &x, const DATA_TYPE &y) const {
    return x.key < y.key;
  }
};

struct rightshift {
  inline int operator()(const DATA_TYPE &x, const unsigned offset) {
    return x.key >> offset;
  }
};

//Pass in an argument to test std::sort
int main(int argc, const char ** argv) {
  size_t uSize = sizeof(int);
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
  input.seekg (0, std::ios_base::end);
    size_t length = input.tellg();
  double total = 0.0;
  std::vector<DATA_TYPE> array;
  array.reserve(length/uSize);
  unsigned uCount = length/uSize;
  //Run multiple loops, if requested
  for (unsigned u = 0; u < loopCount; ++u) {
    input.seekg (0, std::ios_base::beg);
    unsigned v = 0;
    while (input.good() && v++ < uCount) { // EOF or failure stops the reading
     DATA_TYPE element;
     input.read(reinterpret_cast<char *>(&(element.key)), sizeof(element.key));
     std::stringstream intstr;
         intstr << element.key;
         element.data = intstr.str();
         array.push_back(element);
    }  
    clock_t start, end;
    double elapsed;
    start = clock();
    if (stdSort)
      std::sort(array.begin(), array.end(), lessthan());
    else
      integer_sort(array.begin(), array.end(), rightshift(), lessthan());
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
  input.close();
  if (stdSort)
    printf("std::sort elapsed time %f\n", total / CLOCKS_PER_SEC);
  else
    printf("spreadsort elapsed time %f\n", total / CLOCKS_PER_SEC);
  return 0;
}
