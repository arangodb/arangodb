////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2019 ArangoDB GmbH, Cologne, Germany
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

#ifndef IRESEARCH_DELTA_COMPRESSION_H
#define IRESEARCH_DELTA_COMPRESSION_H

#include "string.hpp"
#include "compression.hpp"
#include "noncopyable.hpp"

namespace iresearch {
namespace compression {

class IRESEARCH_API delta_compressor : public compressor, private util::noncopyable {
 public:
  virtual bytes_ref compress(byte_type* src, size_t size, bstring& out) override final;
}; // delta_compressor

class IRESEARCH_API delta_decompressor : public decompressor, private util::noncopyable {
 public:
  /// @returns bytes_ref::NIL in case of error
  virtual bytes_ref decompress(const byte_type* src, size_t src_size,
                               byte_type* dst, size_t dst_size) override final;
}; // delta_decompressor

struct IRESEARCH_API delta {
  static constexpr string_ref type_name() noexcept {
    return "iresearch::compression::delta";
  }

  static void init();
  static compression::compressor::ptr compressor(const options& opts);
  static compression::decompressor::ptr decompressor();
}; // delta

} // compression
} // namespace iresearch {

#endif

