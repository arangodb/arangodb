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

class IRESEARCH_API lz4compressor_base : public compressor,
                                         private util::noncopyable {
 public:
 // as stated in 'lz4.h', we have to use
 // LZ4_createStream/LZ4_freeStream in context of a DLL
#ifdef IRESEARCH_DLL
  explicit lz4compressor_base() : stream_(LZ4_createStream()) {
    if (!stream_) {
      throw std::bad_alloc();
    }
  }
  ~lz4compressor_base() { LZ4_freeStream(stream_); }
#else
  lz4compressor_base() {
    LZ4_resetStream(&stream_);
  }
#endif

 protected:
#ifdef IRESEARCH_DLL
  LZ4_stream_t* stream() NOEXCEPT { return stream_; }
#else
  LZ4_stream_t* stream() NOEXCEPT { return &stream_; }
#endif

 private:
#ifdef IRESEARCH_DLL
  LZ4_stream_t* stream_;
#else
  LZ4_stream_t stream_;
#endif
}; // lz4compressor_base

class IRESEARCH_API lz4decompressor_base : public decompressor,
                                           private util::noncopyable {
 public:
 // as stated in 'lz4.h', we have to use
 // LZ4_createStreamDecode/LZ4_freeStreamDecode in context of a DLL
#ifdef IRESEARCH_DLL
  lz4decompressor_base() : stream_(LZ4_createStreamDecode()) {
    if (!stream_) {
      throw std::bad_alloc();
    }
  }
  ~lz4decompressor_base() { LZ4_freeStreamDecode(stream_); }
#else
  lz4decompressor_base() NOEXCEPT {
    std::memset(&stream_, 0, sizeof(stream_));
  }
#endif

 protected:
#ifdef IRESEARCH_DLL
  LZ4_streamDecode_t* stream() NOEXCEPT { return stream_; }
#else
  LZ4_streamDecode_t* stream() NOEXCEPT { return &stream_; }
#endif

 private:
#ifdef IRESEARCH_DLL
  LZ4_streamDecode_t* stream_;
#else
  LZ4_streamDecode_t stream_;
#endif
}; // lz4decompressor

struct IRESEARCH_API lz4 {
  DECLARE_COMPRESSION_TYPE();

  class IRESEARCH_API lz4compressor final : public compression::compressor {
   public:
    explicit lz4compressor(int acceleration = 0) NOEXCEPT
      : acceleration_(acceleration) {
    }

    int acceleration() const NOEXCEPT { return acceleration_; }

    virtual bytes_ref compress(byte_type* src, size_t size, bstring& out) override;

   private:
    const int acceleration_{0}; // 0 - default acceleration
  };

  class IRESEARCH_API lz4decompressor final : public compression::decompressor {
   public:
    virtual bytes_ref decompress(byte_type* src, size_t src_size,
                                 byte_type* dst, size_t dst_size) override;
  };

  static void init();
  static compression::compressor::ptr compressor(const options& opts);
  static compression::decompressor::ptr decompressor();
}; // lz4basic

NS_END // compression
NS_END // NS_ROOT

#endif
