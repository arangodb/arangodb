//  Benchmark for integer sorting speed across parallel threads.
//
//  Copyright Steven Ross 2014
//
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

//  See http://www.boost.org/libs/sort for library home page.

#include <boost/sort/spreadsort/spreadsort.hpp>
#include <boost/thread.hpp>
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

#define DATA_TYPE int

bool is_sorted(const std::vector<DATA_TYPE> &array) {
  for (unsigned u = 0; u + 1 < array.size(); ++u) {
    if (array[u] > array[u + 1]) {
      return false;
    }
  }
  return true;
}

void sort_loop(const std::vector<DATA_TYPE> &base_array, bool stdSort, 
               unsigned loopCount) {
  std::vector<DATA_TYPE> array(base_array);
  for (unsigned u = 0; u < loopCount; ++u) {
    for (unsigned v = 0; v < base_array.size(); ++v) {
      array[v] = base_array[v];
    }
    if (stdSort)
      std::sort(array.begin(), array.end());
    else
      boost::sort::spreadsort::spreadsort(array.begin(), array.end());
    if (!is_sorted(array)) {
      fprintf(stderr, "sort failed!\n");
      exit(1);
    }
  }
}

//Pass in an argument to test std::sort
int main(int argc, const char ** argv) {
  size_t uCount,uSize=sizeof(DATA_TYPE);
  bool stdSort = false;
  int threadCount = -1;
  unsigned loopCount = 0;
  for (int u = 1; u < argc; ++u) {
    if (std::string(argv[u]) == "-std")
      stdSort = true;
    else if(threadCount < 0)
      threadCount = atoi(argv[u]);
    else
      loopCount = atoi(argv[u]);
  }
  if (!loopCount) {
    loopCount = 1;
  }
  printf("threads: %d loops: %d\n", threadCount, loopCount);

  std::ifstream input("input.txt", std::ios_base::in | std::ios_base::binary);
  if (input.fail()) {
    printf("input.txt could not be opened\n");
    return 1;
  }
  std::vector<DATA_TYPE> base_array;
  input.seekg (0, std::ios_base::end);
  size_t length = input.tellg();
  uCount = length/uSize;
  input.seekg (0, std::ios_base::beg);
  //Conversion to a vector
  base_array.resize(uCount);
  unsigned v = 0;
  while (input.good() && v < uCount)
    input.read(reinterpret_cast<char *>(&(base_array[v++])), uSize );
  input.close();
  if (v < uCount)
    base_array.resize(v);
  //Run multiple loops, if requested
  clock_t start, end;
  double elapsed;
  std::vector<boost::thread *> workers;
  start = clock();
  if (threadCount == 0) {
    sort_loop(base_array, stdSort, loopCount);
  } else {
    for (int i = 0; i < threadCount; ++i) {
      workers.push_back(new boost::thread(sort_loop, base_array, stdSort, 
                                          loopCount));
    }
    for (int i = 0; i < threadCount; ++i) {
      workers[i]->join();
      delete workers[i];
    }
  }
  end = clock();
  elapsed = static_cast<double>(end - start) ;
  
  if (stdSort)
    printf("std::sort clock time %lf\n", elapsed/CLOCKS_PER_SEC/threadCount);
  else
    printf("spreadsort clock time %lf\n", elapsed/CLOCKS_PER_SEC/threadCount);
  return 0;
}
