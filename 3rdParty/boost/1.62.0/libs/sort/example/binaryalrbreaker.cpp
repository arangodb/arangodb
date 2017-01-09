// a sorting example that uses the worst-case distribution for spreadsort.
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

const DATA_TYPE typed_one = 1;

void
fill_vector(vector<DATA_TYPE> & input, const DATA_TYPE base_value,
            unsigned remaining_bits, const vector<unsigned> & indices,
            int index)
{
  if (index < 0) {
    for (unsigned u = 0; u < max_count; ++u)
      input.push_back((base_value << remaining_bits) +
                      (rand() % (1 << remaining_bits)));
  }
  else {
    unsigned shift = indices[index];
    fill_vector(input, (base_value << shift) + ((1 << shift) - 1),
                remaining_bits - shift, indices, index - 1);
    fill_vector(input, base_value << shift, remaining_bits - shift, indices,
                index - 1);
  }
}

//Generates a random index from 0 up to but not including count.
unsigned
get_index(unsigned count)
{
  unsigned result = unsigned((rand() % (1 << 16))*uint64_t(count)/(1 << 16));
  if (result >= count)
    return count - 1;
  return result;
}

//Tests std::sort vs boost::sort::spreadsort on boost::sort's worst distribution.
int main(int, const char **) {
  unsigned total_length = sizeof(DATA_TYPE) * 8;
  double std_sort_time = 0;
  double spreadsort_time = 0;
  for (int repetition = 0; repetition < 10; ++repetition) {
    vector<DATA_TYPE> input;
    vector<unsigned> offsets;
    unsigned bit_length = total_length - radix_threshold;
    unsigned bit_offset = bit_shift;
    for (; bit_length >= ++bit_offset; bit_length -= bit_offset)
      offsets.push_back(bit_offset);
    for (int ii = (1 << bit_length) - 1; ii >= 0; --ii)
      fill_vector(input, ii, total_length - bit_length,
                  offsets, offsets.size() - 1);

    //Randomize the inputs slightly so they aren't in reverse-sorted order, for
    //which std::sort is very fast.
    for (unsigned u = 0; u < input.size() / 10; ++u)
      std::swap(input[get_index(input.size())], input[get_index(input.size())]);

    //Run both std::sort and boost::sort::spreadsort.
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
        std_sort_time += elapsed / CLOCKS_PER_SEC;
      else
        spreadsort_time += elapsed / CLOCKS_PER_SEC;
      array.clear();
    }
  }

  printf("std::sort elapsed time %f\n", std_sort_time);
  printf("spreadsort elapsed time %f\n", spreadsort_time);
  return 0;
}
