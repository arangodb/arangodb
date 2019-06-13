//  Benchmark for integer sorting speed across parallel threads.
//
//  Copyright Steven Ross 2014
//
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

//  See http://www.boost.org/libs/sort for library home page.

#include <boost/random/mersenne_twister.hpp>
#include <boost/random/uniform_int_distribution.hpp>
#include <boost/sort/spreadsort/spreadsort.hpp>
#include <boost/thread.hpp>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <algorithm>
#include <vector>
#include <iostream>
#include <fstream>
#include <string>
using std::string;
using namespace boost::sort::spreadsort;

#define DATA_TYPE string

static bool is_sorted(const std::vector<DATA_TYPE> &array) {
  for (unsigned u = 0; u + 1 < array.size(); ++u) {
    if (array[u] > array[u + 1]) {
      return false;
    }
  }
  return true;
}

static void sort_core(std::vector<DATA_TYPE> &array, bool stdSort,
               unsigned loopCount) {
  if (stdSort)
    std::sort(array.begin(), array.end());
  else
    boost::sort::spreadsort::spreadsort(array.begin(), array.end());
  if (!is_sorted(array)) {
    fprintf(stderr, "sort failed!\n");
    exit(1);
  }
}

static void sort_loop(const std::vector<DATA_TYPE> &base_array, bool stdSort,
               unsigned loopCount) {
  std::vector<DATA_TYPE> array(base_array);
  for (unsigned u = 0; u < loopCount; ++u) {
    for (unsigned v = 0; v < base_array.size(); ++v) {
      array[v] = base_array[v];
    }
    sort_core(array, stdSort, loopCount);
  }
}

//Pass in an argument to test std::sort
int main(int argc, const char ** argv) {
  std::ifstream indata;
  std::ofstream outfile;
  bool stdSort = false;
  int constant_to_random_ratio = -1;
  int threadCount = -1;
  unsigned loopCount = 0;
  for (int u = 1; u < argc; ++u) {
    if (std::string(argv[u]) == "-std")
      stdSort = true;
    else if(threadCount < 0)
      threadCount = atoi(argv[u]);
    else if (!loopCount)
      loopCount = atoi(argv[u]);
    else
      constant_to_random_ratio = atoi(argv[u]);
  }
  if (!loopCount) {
    loopCount = 1;
  }
  printf("threads: %d loops: %d\n", threadCount, loopCount);

  std::vector<DATA_TYPE> base_array;
  if (constant_to_random_ratio >= 0) {
    //Test for random data with gaps of identical data.
    random::mt19937 generator;
    random::uniform_int_distribution<int> distribution(0,255);
    const int constant_to_random_count = 1000000;
    const int string_length = 1000;
    for (int i = 0; i < constant_to_random_count; ++i) {
      DATA_TYPE temp(string_length, 'x');  // fill with default character.
      for (int j = constant_to_random_ratio; j < string_length;) {
        int val = distribution(generator);
        temp[j] = val;
        j += (val * constant_to_random_ratio)/128 + 1;
      }
      base_array.push_back(temp);
    }
  } else {
    indata.open("input.txt", std::ios_base::in | std::ios_base::binary);
    if (indata.bad()) {
      printf("input.txt could not be opened\n");
      return 1;
    }
    DATA_TYPE inval;
    while (!indata.eof() ) {
      indata >> inval;
      base_array.push_back(inval);
    }
  }

  // Sort the input
  clock_t start, end;
  double elapsed;
  std::vector<boost::thread *> workers;
  start = clock();
  if (threadCount == 0) {
    if (loopCount > 1) {
      sort_loop(base_array, stdSort, loopCount);
    } else {
      sort_core(base_array, stdSort, loopCount);
    }
    threadCount = 1;
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

  printf("for %lu strings\n", base_array.size());
  if (stdSort)
    printf("std::sort clock time %lf\n", elapsed/CLOCKS_PER_SEC/threadCount);
  else
    printf("spreadsort clock time %lf\n", elapsed/CLOCKS_PER_SEC/threadCount);
  return 0;
}
