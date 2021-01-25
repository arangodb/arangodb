/* The MIT License

   Copyright (C) 2012 Zilong Tan (eric.zltan@gmail.com)

   Permission is hereby granted, free of charge, to any person
   obtaining a copy of this software and associated documentation
   files (the "Software"), to deal in the Software without
   restriction, including without limitation the rights to use, copy,
   modify, merge, publish, distribute, sublicense, and/or sell copies
   of the Software, and to permit persons to whom the Software is
   furnished to do so, subject to the following conditions:

   The above copyright notice and this permission notice shall be
   included in all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
   EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
   MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
   NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
   BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
   ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
   CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
   SOFTWARE.
*/

#ifndef LIB_BASICS_FASTHASH_H
#define LIB_BASICS_FASTHASH_H 1

#include <cstdint>
#include <cstdlib>

static constexpr uint64_t fasthash_m = 0x880355f21e6d1965ULL;

// Compression function for Merkle-Damgard construction.
// This function is generated using the framework provided.
constexpr uint64_t fasthash_mix(uint64_t h) {
  h ^= h >> 23;
  h *= 0x2127599bf4325c37ULL;
  h ^= h >> 47;

  return h;
}

constexpr uint64_t fasthash64_uint64(uint64_t value, uint64_t seed) {
  uint64_t h = seed ^ 4619197404915747624ULL;  // this is h = seed ^ (sizeof(uint64_t) *
                                               // m), but prevents VS warning C4307:
                                               // integral constant overflow
  h ^= fasthash_mix(value);
  h *= fasthash_m;

  return fasthash_mix(h);
}

/**
 * fasthash32 - 32-bit implementation of fasthash
 * @buf:  data buffer
 * @len:  data size
 * @seed: the seed
 */
uint32_t fasthash32(const void* buf, size_t len, uint32_t seed);

/**
 * fasthash64 - 64-bit implementation of fasthash
 * @buf:  data buffer
 * @len:  data size
 * @seed: the seed
 */
uint64_t fasthash64(const void* buf, size_t len, uint64_t seed);

#endif
