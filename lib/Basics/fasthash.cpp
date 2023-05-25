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

#include "fasthash.h"
#include <cstring>

uint64_t fasthash64(const void* buf, size_t len, uint64_t seed) {
  auto const* pos = static_cast<uint8_t const*>(buf);
  auto const* end = pos + (len / 8) * 8;
  uint64_t h = seed ^ (len * fasthash_m);
  uint64_t v;

  while (pos != end) {
    std::memcpy(&v, pos, 8);
    pos += 8;
    h ^= fasthash_mix(v);
    h *= fasthash_m;
  }

  v = 0;

  switch (len & 7) {
    case 7:
      v ^= (uint64_t)pos[6] << 48;
      [[fallthrough]];
    case 6:
      v ^= (uint64_t)pos[5] << 40;
      [[fallthrough]];
    case 5:
      v ^= (uint64_t)pos[4] << 32;
      [[fallthrough]];
    case 4:
      v ^= (uint64_t)pos[3] << 24;
      [[fallthrough]];
    case 3:
      v ^= (uint64_t)pos[2] << 16;
      [[fallthrough]];
    case 2:
      v ^= (uint64_t)pos[1] << 8;
      [[fallthrough]];
    case 1:
      v ^= (uint64_t)pos[0];
      h ^= fasthash_mix(v);
      h *= fasthash_m;
  }

  return fasthash_mix(h);
#endif
}

uint32_t fasthash32(const void* buf, size_t len, uint32_t seed) {
  // the following trick converts the 64-bit hashcode to Fermat
  // residue, which shall retain information from both the higher
  // and lower parts of hashcode.
  uint64_t h = fasthash64(buf, len, seed);
  return (uint32_t)(h - (h >> 32));
}
