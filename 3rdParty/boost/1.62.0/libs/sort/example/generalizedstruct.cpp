// This example shows how to sort structs using complex multiple part keys using
// string_sort.
//
//  Copyright Steven Ross 2009-2014.
//
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

//  See http://www.boost.org/libs/sort for library home page.

#include <boost/sort/spreadsort/string_sort.hpp>
#include <boost/sort/spreadsort/float_sort.hpp>
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

//[generalized_functors
struct DATA_TYPE {
  time_t birth;
  float net_worth;
  string first_name;
  string last_name;
};

static const int birth_size = sizeof(time_t);
static const int first_name_offset = birth_size + sizeof(float);
static const boost::uint64_t base_mask = 0xff;

struct lessthan {
  inline bool operator()(const DATA_TYPE &x, const DATA_TYPE &y) const {
    if (x.birth != y.birth) {
      return x.birth < y.birth;
    }
    if (x.net_worth != y.net_worth) {
      return x.net_worth < y.net_worth;
    }
    if (x.first_name != y.first_name) {
      return x.first_name < y.first_name;
    }
    return x.last_name < y.last_name;
  }
};

struct bracket {
  inline unsigned char operator()(const DATA_TYPE &x, size_t offset) const {
    // Sort date as a signed int, returning the appropriate byte.
    if (offset < birth_size) {
      const int bit_shift = 8 * (birth_size - offset - 1);
      unsigned char result = (x.birth & (base_mask << bit_shift)) >> bit_shift;
      // Handling the sign bit.  Unnecessary if the data is always positive.
      if (offset == 0) {
        return result ^ 128;
      }

      return result;
    }

    // Sort a signed float.  This requires reversing the order of negatives
    // because of the way floats are represented in bits.
    if (offset < first_name_offset) {
      const int bit_shift = 8 * (first_name_offset - offset - 1);
      unsigned key = float_mem_cast<float, unsigned>(x.net_worth);
      unsigned char result = (key & (base_mask << bit_shift)) >> bit_shift;
      // Handling the sign.
      if (x.net_worth < 0) {
        return 255 - result;
      }
      // Increasing positives so they are higher than negatives.
      if (offset == birth_size) {
        return 128 + result;
      }

      return result;
    }

    // Sort a string that is before the end.  This approach supports embedded
    // nulls.  If embedded nulls are not required, then just delete the "* 2"
    // and the inside of the following if just becomes:
    // return x.first_name[offset - first_name_offset];
    const unsigned first_name_end_offset =
      first_name_offset + x.first_name.size() * 2;
    if (offset < first_name_end_offset) {
      int char_offset = offset - first_name_offset;
      // This signals that the string continues.
      if (!(char_offset & 1)) {
        return 1;
      }
      return x.first_name[char_offset >> 1];
    }

    // This signals that the string has ended, so that shorter strings come
    // before longer ones.
    if (offset == first_name_end_offset) {
      return 0;
    }

    // The final string needs no special consideration.
    return x.last_name[offset - first_name_end_offset - 1];
  }
};

struct getsize {
  inline size_t operator()(const DATA_TYPE &x) const {
    return first_name_offset + x.first_name.size() * 2 + 1 +
      x.last_name.size();
  }
};
//] [/generalized_functors]

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

    // Read in the data.
    DATA_TYPE inval;
    while (!indata.eof() ) {
      indata >> inval.first_name;
      indata >> inval.last_name;
      indata.read(reinterpret_cast<char *>(&(inval.birth)), birth_size);
      indata.read(reinterpret_cast<char *>(&(inval.net_worth)), sizeof(float));
      // Handling nan.
      if (inval.net_worth != inval.net_worth) {
        inval.net_worth = 0;
      }
      if (indata.eof())
        break;
      array.push_back(inval);
    }
    indata.close();

    // Sort the data.
    clock_t start, end;
    double elapsed;
    start = clock();
    if (stdSort) {
      std::sort(array.begin(), array.end(), lessthan());
    } else {
//[generalized_functors_call
      string_sort(array.begin(), array.end(), bracket(), getsize(), lessthan());
//] [/generalized_functors_call]
    }
    end = clock();
    elapsed = static_cast<double>(end - start);
    if (stdSort) {
      outfile.open("standard_sort_out.txt", std::ios_base::out |
                   std::ios_base::binary | std::ios_base::trunc);
    } else {
      outfile.open("boost_sort_out.txt", std::ios_base::out |
                   std::ios_base::binary | std::ios_base::trunc);
    }
    if (outfile.good()) {
      for (unsigned u = 0; u < array.size(); ++u)
        outfile << array[u].birth << " " << array[u].net_worth << " "
                << array[u].first_name << " " << array[u].last_name << "\n";
      outfile.close();
    }
    total += elapsed;
    array.clear();
  }
  if (stdSort) {
    printf("std::sort elapsed time %f\n", total / CLOCKS_PER_SEC);
  } else {
    printf("spreadsort elapsed time %f\n", total / CLOCKS_PER_SEC);
  }
  return 0;
}
