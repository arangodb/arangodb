// spreadsort wstring sorting example
//
//  Copyright Steven Ross 2009-2014.
//
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

//  See http://www.boost.org/libs/sort for library home page.

#include <boost/sort/spreadsort/string_sort.hpp>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <algorithm>
#include <vector>
#include <iostream>
#include <fstream>
#include <string>
using std::wstring;
using namespace boost::sort::spreadsort;

#define DATA_TYPE wstring

//Pass in an argument to test std::sort
int main(int argc, const char ** argv) {
  std::ifstream indata;
  std::ofstream outfile;
  bool stdSort = false;
  unsigned loopCount = 1;
  for (int u = 1; u < argc; ++u) {
    if (std::string(argv[u]) == "-std")
      stdSort = true;
    else
      loopCount = atoi(argv[u]);
  }
  double total = 0.0;
  //Run multiple loops, if requested
  std::vector<DATA_TYPE> array;
  for (unsigned u = 0; u < loopCount; ++u) {
    indata.open("input.txt", std::ios_base::in | std::ios_base::binary);  
    if (indata.bad()) {
      printf("input.txt could not be opened\n");
      return 1;
    }
    unsigned short inval;
    DATA_TYPE current;
    while (indata.good()) {
      indata.read(reinterpret_cast<char *>(&inval), sizeof(inval));
      current.push_back(inval);
      //32 characters is a moderately long string
      if (static_cast<int>(current.size()) > inval || current.size() >= 32) {
        array.push_back(current);
        current.clear();
      }
    }
    //adding the last string
    if (!current.empty())
      array.push_back(current);
      
    indata.close();
    clock_t start, end;
    double elapsed;
    start = clock();
    wchar_t cast_type = 0;
    if (stdSort)
      //std::sort(&(array[0]), &(array[0]) + uCount);
      std::sort(array.begin(), array.end());
    else
      //string_sort(&(array[0]), &(array[0]) + uCount, cast_type);
      string_sort(array.begin(), array.end(), cast_type);
    end = clock();
    elapsed = static_cast<double>(end - start);
    if (stdSort)
      outfile.open("standard_sort_out.txt", std::ios_base::out |
                   std::ios_base::binary | std::ios_base::trunc);
    else
      outfile.open("boost_sort_out.txt", std::ios_base::out |
                   std::ios_base::binary | std::ios_base::trunc);
    if (outfile.good()) {
      for (unsigned u = 0; u < array.size(); ++u){
        for (unsigned v = 0; v < array[u].size(); ++v)
          outfile << array[u][v];
        outfile << "\n";
      }
      outfile.close();
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
