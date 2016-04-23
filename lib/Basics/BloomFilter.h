////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_BASICS_BLOOM_FILTER_H
#define ARANGODB_BASICS_BLOOM_FILTER_H 1

#include "Basics/Common.h"
#include "Basics/fasthash.h"
#include "Basics/hashes.h"

#include <bitset>

namespace arangodb {
namespace basics {

template <size_t Bits>
class BloomFilter {
  static_assert(Bits >= 10, "invalid number of bits");

 public:
  BloomFilter(BloomFilter const&) = delete;
  BloomFilter& operator=(BloomFilter const&) = delete;

  BloomFilter(size_t numberHashFunctions)
      : _numberHashFunctions(numberHashFunctions),
        _bits(new std::bitset<Bits>()) {
    TRI_ASSERT(numberHashFunctions > 0);
  }

  ~BloomFilter();

  //////////////////////////////////////////////////////////////////////////////
  /// @brief insert an element into the set
  //////////////////////////////////////////////////////////////////////////////

  void insert(char const* data, size_t length) {
    // we're not using k independent hash functions here, but a variant of
    // double hashing (https://en.wikipedia.org/wiki/Double_hashing)
    // see https://www.eecs.harvard.edu/~michaelm/postscripts/rsa2008.pdf
    uint64_t const h0 = hash0(data, length);

    if (_numberHashFunctions == 1) {
      // special case for a single hash function
      _bits->set(h0 % Bits);
      return;
    }

    uint64_t const h1 = hash1(data, length);

    for (size_t i = 0; i < _numberHashFunctions; ++i) {
      size_t position = (h0 + i * h1) % Bits;
      _bits->set(position);
    }
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief check whether an element is contained in the set
  /// will return false if the element is not contained in the set
  /// will return true if the element is contained in the set, or if there
  /// is a hash collision (false positive)
  //////////////////////////////////////////////////////////////////////////////

  bool contains(char const* data, size_t length) {
    // we're not using k independent hash functions here, but a variant of
    // double hashing (https://en.wikipedia.org/wiki/Double_hashing)
    // see https://www.eecs.harvard.edu/~michaelm/postscripts/rsa2008.pdf
    uint64_t const h0 = hash0(data, length);

    if (_numberHashFunctions == 1) {
      // special case for a single hash function
      return _bits->test(h0 % Bits);
    }

    uint64_t const h1 = hash1(data, length);

    for (size_t i = 0; i < _numberHashFunctions; ++i) {
      size_t position = (h0 + i * h1) % Bits;
      if (!_bits->test(position)) {
        // definitely not contained
        return false;
      }
    }
    // either contained in set, or false positive
    return true;
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief return the optimal number of hash functions
  /// (from
  /// https://en.wikipedia.org/wiki/Bloom_filter#Optimal_number_of_hash_functions)
  //////////////////////////////////////////////////////////////////////////////

  static size_t optimalNumberHashes(size_t numberElements) {
    double bitsPerElement = static_cast<double>(Bits) / numberElements;
    return static_cast<size_t>(
        optimalNumberHashes(numberElements, bitsPerElement));
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief return the optimal number of hash functions
  /// (from
  /// https://en.wikipedia.org/wiki/Bloom_filter#Optimal_number_of_hash_functions)
  //////////////////////////////////////////////////////////////////////////////

  static size_t optimalNumberHashes(size_t numberElements,
                                    double bitsPerElement) {
    if (numberElements <= 1) {
      return 1;
    }

    // 0.69xxx = ln 2
    return static_cast<size_t>(
        (std::max)(bitsPerElement * 0.6931471805599453, 1.0));
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief return the expected error rate
  //////////////////////////////////////////////////////////////////////////////

  static double expectedErrorRate(size_t numberElements) {
    return expectedErrorRate(Bits, numberElements);
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief return the expected error rate
  //////////////////////////////////////////////////////////////////////////////

  static double expectedErrorRate(size_t filterSize, size_t numberElements) {
    if (numberElements == 0 || filterSize == 0) {
      // cannot compute for 0 elements
      return 1.0;
    }

    // 0.69xxx = ln 2
    double const k = (filterSize / numberElements) * 0.6931471805599453;
    return std::pow(
        (1.0 - std::pow(1.0 - (1.0 / filterSize), k * numberElements)), k);
  }

 private:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief first hash function
  //////////////////////////////////////////////////////////////////////////////

  uint64_t hash0(char const* data, size_t length) const {
    return fasthash64(static_cast<void const*>(data), length, 0xdeadbeef);
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief second hash function
  //////////////////////////////////////////////////////////////////////////////

  uint64_t hash1(char const* data, size_t length) const {
    return TRI_FnvHashPointer(static_cast<void const*>(data), length);
  }

 private:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief number of hash functions
  //////////////////////////////////////////////////////////////////////////////

  size_t const _numberHashFunctions;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief the bitset representing the bloom filter
  //////////////////////////////////////////////////////////////////////////////

  std::unique_ptr<std::bitset<Bits>> _bits;
};

}  // namespace arangodb::basics
}  // namespace arangodb

#endif
