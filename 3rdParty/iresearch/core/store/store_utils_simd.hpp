////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2018 ArangoDB GmbH, Cologne, Germany
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
/// @author Andrey Abramov
////////////////////////////////////////////////////////////////////////////////

#ifndef IRESEARCH_STORE_UTILS_SIMD_H
#define IRESEARCH_STORE_UTILS_SIMD_H

namespace iresearch {

struct data_input;
struct data_output;

namespace encode {
namespace bitpack {

// reads block of 128 integers from the stream
// that was previously encoded with the corresponding
// 'write_block' function using low-level optimizations
IRESEARCH_API void read_block_simd(
  data_input& in,
  uint32_t* RESTRICT encoded,
  uint32_t* RESTRICT decoded);

// reads block of the specified size from the stream
// that was previously encoded with the corresponding
// 'write_block' function using low-level optimizations
IRESEARCH_API void read_block_simd(
  data_input& in,
  uint32_t size,
  uint32_t* RESTRICT encoded,
  uint32_t* RESTRICT decoded);

// writes block of integers to stream
//   all values are equal -> RL encoding,
//   otherwise            -> bit packing
// returns number of bits used to encoded the block (0 == RL)
IRESEARCH_API uint32_t write_block_simd(
  data_output& out,
  const uint32_t* RESTRICT decoded,
  uint32_t* RESTRICT encoded);

// writes block of the specified size to stream
//   all values are equal -> RL encoding,
//   otherwise            -> bit packing
// returns number of bits used to encoded the block (0 == RL)
IRESEARCH_API uint32_t write_block_simd(
  data_output& out,
  const uint32_t* RESTRICT decoded,
  uint32_t size,
  uint32_t* RESTRICT encoded);

} // encode
} // bitpack
} // ROOT

#endif
