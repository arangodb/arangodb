// flexible random number generator providing multiple distributions.
//
//  Copyright Steven Ross 2009-2014.
//
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

//  See http://www.boost.org/libs/sort for library home page.

#include <boost/random/mersenne_twister.hpp>
#include <boost/random/uniform_int_distribution.hpp>
#include <stdio.h>
#include "stdlib.h"
#include <fstream>
#include <iostream>
using namespace boost;

int main(int argc, const char ** argv) {
  random::mt19937 generator;
  random::uniform_int_distribution<unsigned> distribution;
  //defaults
  unsigned high_shift = 16;
  unsigned low_shift = 16;
  unsigned count = 1000000;
  //Reading in user arguments
  if (argc > 1)
    high_shift = atoi(argv[1]);
  if (argc > 2)
    low_shift = atoi(argv[2]);
  if (argc > 3)
    count = atoi(argv[3]);
  if (high_shift > 16)
    high_shift = 16;
  if (low_shift > 16)
    low_shift = 16;
  std::ofstream ofile;
  ofile.open("input.txt", std::ios_base::out | std::ios_base::binary |
             std::ios_base::trunc);
  if (ofile.bad()) {
    printf("could not open input.txt for writing!\n");
    return 1;
  }
  //buffering file output for speed
  unsigned uDivideFactor = 1000;
  //Skipping buffering for small files
  if (count < uDivideFactor * 100)
    uDivideFactor = count;
  unsigned * pNumbers = static_cast<unsigned *>(malloc(uDivideFactor * 
                                                       sizeof(unsigned)));
  //Generating semirandom numbers
  unsigned mask = 0;
  unsigned one = 1;
  for (unsigned u = 0; u < low_shift; ++u) {
    mask += one << u;
  }
  for (unsigned u = 0; u < high_shift; ++u) {
    mask += one << (16 + u);
  }
  for (unsigned u = 0; u < count/uDivideFactor; ++u) {
    unsigned i = 0;
    for (; i< uDivideFactor; ++i) {
      pNumbers[i] = distribution(generator) & mask;
    }
    ofile.write(reinterpret_cast<char *>(pNumbers), uDivideFactor * 4 );
  }
  ofile.close();
  return 0;
}
