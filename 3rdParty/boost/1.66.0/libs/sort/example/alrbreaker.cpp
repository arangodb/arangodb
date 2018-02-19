// a sorting example that uses the worst-case for conventional MSD radix sorts.
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
using namespace std;

#define DATA_TYPE boost::uint64_t

#define ALR_THRESHOLD 3

const unsigned max_count = ALR_THRESHOLD - 1;
const unsigned bit_shift = detail::rough_log_2_size(max_count) -
  detail::int_log_mean_bin_size;
const unsigned radix_threshold = detail::rough_log_2_size(max_count) + 1;
//Increase this size if too fast to test accurately
const unsigned top_splits = 12;

const DATA_TYPE typed_one = 1;

void
fill_vector(vector<DATA_TYPE> & input, const DATA_TYPE base_value,
            unsigned remaining_bits)
{
  if (remaining_bits >= radix_threshold) {
    input.push_back((base_value << remaining_bits) +
                    ((typed_one << remaining_bits) - 1));
    fill_vector(input, base_value << bit_shift, remaining_bits - bit_shift);
  }
  else {
    for (unsigned u = 0; u < max_count; ++u)
      input.push_back((base_value << remaining_bits) +
                      (rand() % (1 << remaining_bits)));
  }
}

//Tests spreadsort on the worst-case distribution for standard MSD radix sorts.
int main(int, const char **) {
  vector<DATA_TYPE> input;
  for (int ii = (1 << top_splits) - 1; ii >= 0; --ii)
    fill_vector(input, ii, (sizeof(DATA_TYPE) * 8) - top_splits);

  //Run both std::sort and spreadsort
  for (unsigned u = 0; u < 2; ++u) {
    vector<DATA_TYPE> array = input;
    clock_t start, end;
    double elapsed;
    start = clock();
    if (u)
      std::sort(array.begin(), array.end());
    else
      boost::sort::spreadsort::spreadsort(array.begin(), array.end());
    end = clock();
    elapsed = static_cast<double>(end - start);
    if (u)
      printf("std::sort elapsed time %f\n", elapsed / CLOCKS_PER_SEC);
    else
      printf("spreadsort elapsed time %f\n", elapsed / CLOCKS_PER_SEC);
    array.clear();
  }
  return 0;
}
