//  Copyright (c) 2011-present, Facebook, Inc.  All rights reserved.
//  This source code is licensed under both the GPLv2 (found in the
//  COPYING file in the root directory) and Apache 2.0 License
//  (found in the LICENSE.Apache file in the root directory).
//
// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#pragma once
#include <stdint.h>

#include <algorithm>
#include <random>

#include "rocksdb/rocksdb_namespace.h"

namespace ROCKSDB_NAMESPACE {

// A very simple random number generator.  Not especially good at
// generating truly random bits, but good enough for our needs in this
// package.
class Random {
 private:
  enum : uint32_t {
    M = 2147483647L  // 2^31-1
  };
  enum : uint64_t {
    A = 16807  // bits 14, 8, 7, 5, 2, 1, 0
  };

  uint32_t seed_;

  static uint32_t GoodSeed(uint32_t s) { return (s & M) != 0 ? (s & M) : 1; }

 public:
  // This is the largest value that can be returned from Next()
  enum : uint32_t { kMaxNext = M };

  explicit Random(uint32_t s) : seed_(GoodSeed(s)) {}

  void Reset(uint32_t s) { seed_ = GoodSeed(s); }

  uint32_t Next() {
    // We are computing
    //       seed_ = (seed_ * A) % M,    where M = 2^31-1
    //
    // seed_ must not be zero or M, or else all subsequent computed values
    // will be zero or M respectively.  For all other values, seed_ will end
    // up cycling through every number in [1,M-1]
    uint64_t product = seed_ * A;

    // Compute (product % M) using the fact that ((x << 31) % M) == x.
    seed_ = static_cast<uint32_t>((product >> 31) + (product & M));
    // The first reduction may overflow by 1 bit, so we may need to
    // repeat.  mod == M is not possible; using > allows the faster
    // sign-bit-based test.
    if (seed_ > M) {
      seed_ -= M;
    }
    return seed_;
  }

  uint64_t Next64() { return (uint64_t{Next()} << 32) | Next(); }

  // Returns a uniformly distributed value in the range [0..n-1]
  // REQUIRES: n > 0
  uint32_t Uniform(int n) { return Next() % n; }

  // Randomly returns true ~"1/n" of the time, and false otherwise.
  // REQUIRES: n > 0
  bool OneIn(int n) { return Uniform(n) == 0; }

  // "Optional" one-in-n, where 0 or negative always returns false
  // (may or may not consume a random value)
  bool OneInOpt(int n) { return n > 0 && OneIn(n); }

  // Returns random bool that is true for the given percentage of
  // calls on average. Zero or less is always false and 100 or more
  // is always true (may or may not consume a random value)
  bool PercentTrue(int percentage) {
    return static_cast<int>(Uniform(100)) < percentage;
  }

  // Skewed: pick "base" uniformly from range [0,max_log] and then
  // return "base" random bits.  The effect is to pick a number in the
  // range [0,2^max_log-1] with exponential bias towards smaller numbers.
  uint32_t Skewed(int max_log) {
    return Uniform(1 << Uniform(max_log + 1));
  }

  // Returns a random string of length "len"
  std::string RandomString(int len);

  // Generates a random string of len bytes using human-readable characters
  std::string HumanReadableString(int len);

  // Generates a random binary data
  std::string RandomBinaryString(int len);

  // Returns a Random instance for use by the current thread without
  // additional locking
  static Random* GetTLSInstance();
};

// A good 32-bit random number generator based on std::mt19937.
// This exists in part to avoid compiler variance in warning about coercing
// uint_fast32_t from mt19937 to uint32_t.
class Random32 {
 private:
  std::mt19937 generator_;

 public:
  explicit Random32(uint32_t s) : generator_(s) {}

  // Generates the next random number
  uint32_t Next() { return static_cast<uint32_t>(generator_()); }

  // Returns a uniformly distributed value in the range [0..n-1]
  // REQUIRES: n > 0
  uint32_t Uniform(uint32_t n) {
    return static_cast<uint32_t>(
        std::uniform_int_distribution<std::mt19937::result_type>(
            0, n - 1)(generator_));
  }

  // Returns an *almost* uniformly distributed value in the range [0..n-1].
  // Much faster than Uniform().
  // REQUIRES: n > 0
  uint32_t Uniformish(uint32_t n) {
    // fastrange (without the header)
    return static_cast<uint32_t>((uint64_t(generator_()) * uint64_t(n)) >> 32);
  }

  // Randomly returns true ~"1/n" of the time, and false otherwise.
  // REQUIRES: n > 0
  bool OneIn(uint32_t n) { return Uniform(n) == 0; }

  // Skewed: pick "base" uniformly from range [0,max_log] and then
  // return "base" random bits.  The effect is to pick a number in the
  // range [0,2^max_log-1] with exponential bias towards smaller numbers.
  uint32_t Skewed(int max_log) {
    return Uniform(uint32_t{1} << Uniform(max_log + 1));
  }

  // Reset the seed of the generator to the given value
  void Seed(uint32_t new_seed) { generator_.seed(new_seed); }
};

// A good 64-bit random number generator based on std::mt19937_64
class Random64 {
 private:
  std::mt19937_64 generator_;

 public:
  explicit Random64(uint64_t s) : generator_(s) { }

  // Generates the next random number
  uint64_t Next() { return generator_(); }

  // Returns a uniformly distributed value in the range [0..n-1]
  // REQUIRES: n > 0
  uint64_t Uniform(uint64_t n) {
    return std::uniform_int_distribution<uint64_t>(0, n - 1)(generator_);
  }

  // Randomly returns true ~"1/n" of the time, and false otherwise.
  // REQUIRES: n > 0
  bool OneIn(uint64_t n) { return Uniform(n) == 0; }

  // Skewed: pick "base" uniformly from range [0,max_log] and then
  // return "base" random bits.  The effect is to pick a number in the
  // range [0,2^max_log-1] with exponential bias towards smaller numbers.
  uint64_t Skewed(int max_log) {
    return Uniform(uint64_t(1) << Uniform(max_log + 1));
  }
};

// A seeded replacement for removed std::random_shuffle
template <class RandomIt>
void RandomShuffle(RandomIt first, RandomIt last, uint32_t seed) {
  std::mt19937 rng(seed);
  std::shuffle(first, last, rng);
}

// A replacement for removed std::random_shuffle
template <class RandomIt>
void RandomShuffle(RandomIt first, RandomIt last) {
  RandomShuffle(first, last, std::random_device{}());
}

}  // namespace ROCKSDB_NAMESPACE
