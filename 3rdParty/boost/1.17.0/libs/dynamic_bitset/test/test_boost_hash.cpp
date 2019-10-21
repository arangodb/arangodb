//
// Copyright (C) 2019 James E. King III
//
// Permission to copy, use, modify, sell and distribute this software
// is granted provided this copyright notice appears in all copies.
// This software is provided "as is" without express or implied
// warranty, and with no claim as to its suitability for any purpose.
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/config.hpp>
#include <boost/dynamic_bitset.hpp>
#include <boost/core/lightweight_test.hpp>
#include <set>

int main(int, char*[])
{
    typedef boost::dynamic_bitset<unsigned long> bitset_type;
    const std::string long_string =
        "01001110101110110101011010000000000011110101101111111111";
    const std::string long_string_prime_begin =
        "11001110101110110101011010000000000011110101101111111111";
    const std::string long_string_prime_end =
        "01001110101110110101011010000000000011110101101111111110";

    bitset_type zeroes(long_string.size(), 0);
    bitset_type stuff (long_string);
    bitset_type stupb (long_string_prime_begin);
    bitset_type stupe (long_string_prime_end);
    bitset_type ones  (long_string.size(), 1);

    boost::hash<bitset_type> bitset_hasher;
    std::set<std::size_t> results;
    results.insert(bitset_hasher(zeroes));
    results.insert(bitset_hasher(stuff));
    results.insert(bitset_hasher(stupb));
    results.insert(bitset_hasher(stupe));
    results.insert(bitset_hasher(ones));

    // if any hash is identical to another there will be less than 5
    BOOST_TEST_EQ(results.size(), 5);

    return boost::report_errors();
}
