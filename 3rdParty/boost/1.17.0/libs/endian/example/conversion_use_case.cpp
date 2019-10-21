//  endian/example/conversion_use_case.cpp

//  Copyright Beman Dawes 2014

//  Distributed under the Boost Software License, Version 1.0.
//  See http://www.boost.org/LICENSE_1_0.txt

//  This program reads a binary file of fixed length records, with a format defined
//  in a header file supplied by a third-party. The records inserted into a vector,
//  and the vector is sorted. The sorted records are then written to an output file.

//  Full I/O error testing omitted for brevity, So don't try this at home.

#include "third_party_format.hpp"
#include <boost/endian/conversion.hpp>
#include <vector>
#include <fstream>
#include <algorithm>
#include <iostream>

using third_party::record;

int main()
{
  std::ifstream in("data.bin", std::ios::binary);
  if (!in) { std::cout << "Could not open data.bin\n"; return 1; }

  std::ofstream out("sorted-data.bin", std::ios::binary);
  if (!out) { std::cout << "Could not open sorted-data.bin\n"; return 1; }

  record rec;
  std::vector<record> recs;

  while (!in.eof())  // read each record
  {
    in.read((char*)&rec, sizeof(rec));
    rec.balance = boost::endian::big_to_native(rec.balance);  // reverse if needed
    recs.push_back(rec);
  }

  std::sort(recs.begin(), recs.end(),  // decending sort by balance
    [](const record& lhs, const record& rhs) -> bool
      { return lhs.balance > rhs.balance; });

  for (auto &out_rec : recs)  // write each record
  {
    out_rec.balance = boost::endian::native_to_big(out_rec.balance);  // reverse if needed
    out.write((const char*)&out_rec, sizeof(out_rec));
  }

}
