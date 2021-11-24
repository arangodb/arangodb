// a random number generator supporting different distributions.
//
//  Copyright Steven Ross 2009.
//
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

//  See http://www.boost.org/libs/sort for library home page.

#include <stdio.h>
#include "stdlib.h"
#include <fstream>
#include <iostream>
#include <vector>
#include <string>
#include <boost/random.hpp>

using std::string;
using namespace boost;

int main(int argc, const char ** argv) {
  //Always seed with the same value, to get the same results
  srand(1);
  //defaults
  int mod_shift = 32;
  unsigned count = 1000000;
  //Reading in user arguments
  if (argc > 2)
    count = atoi(argv[2]);
  if (argc > 1)
    mod_shift = atoi(argv[1]) - 1;
  std::ofstream ofile;
  ofile.open("input.txt", std::ios_base::out | std::ios_base::binary |
             std::ios_base::trunc);
  if (ofile.bad()) {
    printf("could not open input.txt for writing!\n");
    return 1;
  }
  int min_int = (std::numeric_limits<int>::min)();
  int max_int = (std::numeric_limits<int>::max)();
  if (mod_shift < 31 && mod_shift >= 0) {
    max_int %= 1 << mod_shift;
    if (-max_int > min_int)
      min_int = -max_int;
  }
  std::vector<int> result;
  result.resize(count);
  mt19937 rng;
  if (argc > 3 && (string(argv[3]) == "-normal")) {
    boost::normal_distribution<> everything(0, max_int/4);      
    variate_generator<mt19937&,normal_distribution<> > gen(rng, everything);
    generate(result.begin(), result.end(), gen);
  }
  else if (argc > 3 && (string(argv[3]) == "-lognormal")) {
    lognormal_distribution<> everything(max_int/2, max_int/4);      
    variate_generator<mt19937&,lognormal_distribution<> > gen(rng, everything);
    generate(result.begin(), result.end(), gen);
  }
  else {
    uniform_int<> everything(min_int, max_int);
    variate_generator<mt19937&,uniform_int<> > gen(rng, everything);
    generate(result.begin(), result.end(), gen);
  }
  ofile.write(reinterpret_cast<char *>(&(result[0])), result.size() * 
              sizeof(int));
  ofile.close();
  return 0;
}
