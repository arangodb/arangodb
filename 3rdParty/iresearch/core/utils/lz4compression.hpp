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

#ifndef IRESEARCH_LZ4COMPRESSION_H
#define IRESEARCH_LZ4COMPRESSION_H

#include "string.hpp"
#include "compression.hpp"
#include "noncopyable.hpp"

#include <memory>
#include <lz4.h>

NS_ROOT
NS_BEGIN(compression)

class IRESEARCH_API lz4compressor : public compressor, private util::noncopyable {
 public:
 // as stated in 'lz4.h', we have to use
 // LZ4_createStream/LZ4_freeStream in context of a DLL
#ifdef IRESEARCH_DLL
  explicit lz4compressor(int acceleration = 0)
    : stream_(LZ4_createStream()),
      acceleration_(acceleration) {
    if (!stream_) {
      throw std::bad_alloc();
    }
  }
  ~lz4compressor() { LZ4_freeStream(stream_); }
#else
  explicit lz4compressor(int acceleration = 0)
    : acceleration_(acceleration) {
    LZ4_resetStream(&stream_);
  }
#endif

  int acceleration() const NOEXCEPT { return acceleration_; }

  virtual bytes_ref compress(byte_type* src, size_t size, bstring& out) override final;

 private:
#ifdef IRESEARCH_DLL
  LZ4_stream_t* stream() NOEXCEPT { return stream_; }
  LZ4_stream_t* stream_;
#else
  LZ4_stream_t* stream() NOEXCEPT { return &stream_; }
  LZ4_stream_t stream_;
#endif
  const int acceleration_{0}; // 0 - default acceleration
  int dict_size_{}; // the size of the LZ4 dictionary from the previous call
}; // lz4compressor

class IRESEARCH_API lz4decompressor : public decompressor, private util::noncopyable {
 public:
 // as stated in 'lz4.h', we have to use
 // LZ4_createStreamDecode/LZ4_freeStreamDecode in context of a DLL
#ifdef IRESEARCH_DLL
  lz4decompressor() : stream_(LZ4_createStreamDecode()) {
    if (!stream_) {
      throw std::bad_alloc();
    }
  }
  ~lz4decompressor() { LZ4_freeStreamDecode(stream_); }
#else
  lz4decompressor() NOEXCEPT {
    std::memset(&stream_, 0, sizeof(stream_));
  }
#endif

  /// @returns bytes_ref::NIL in case of error
  virtual bytes_ref decompress(byte_type* src, size_t src_size,
                               byte_type* dst, size_t dst_size) override final;

 private:
#ifdef IRESEARCH_DLL
  LZ4_streamDecode_t* stream() NOEXCEPT { return stream_; }
  LZ4_streamDecode_t* stream_;
#else
  LZ4_streamDecode_t* stream() NOEXCEPT { return &stream_; }
  LZ4_streamDecode_t stream_;
#endif
}; // lz4decompressor

struct lz4 {
  DECLARE_COMPRESSION_TYPE();

  static void init();
  static compression::compressor::ptr compressor();
  static compression::decompressor::ptr decompressor();
}; // lz4

NS_END // compression
NS_END // NS_ROOT

#endif
