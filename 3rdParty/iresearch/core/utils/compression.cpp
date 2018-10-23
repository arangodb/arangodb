////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2016 by EMC Corporation, All Rights Reserved
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
/// Copyright holder is EMC Corporation
///
/// @author Andrey Abramov
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#include "shared.hpp"
#include "error/error.hpp"
#include "compression.hpp"
#include "utils/string_utils.hpp"
#include "utils/type_limits.hpp"

#include <lz4.h>

NS_ROOT

compressor::compressor(unsigned int chunk_size):
  dict_size_(0),
  stream_(LZ4_createStream(), [](void* ptr)->void { LZ4_freeStream(reinterpret_cast<LZ4_stream_t*>(ptr)); }) {
  string_utils::oversize(buf_, LZ4_COMPRESSBOUND(chunk_size));
}

void compressor::compress(const char* src, size_t size) {
  assert(size <= std::numeric_limits<int>::max()); // LZ4 API uses int
  auto src_size = static_cast<int>(size);
  auto* stream = reinterpret_cast<LZ4_stream_t*>(stream_.get());

  // ensure LZ4 dictionary from the previous run is at the start of buf_
  {
    auto* dict_store = dict_size_ ? &(buf_[0]) : nullptr;

    // move the LZ4 dictionary from the previous run to the start of buf_
    if (dict_store) {
      dict_size_ = LZ4_saveDict(stream, dict_store, dict_size_);
      assert(dict_size_ >= 0);
    }

    string_utils::oversize(buf_, LZ4_compressBound(src_size) + dict_size_);

    // reload the LZ4 dictionary if buf_ has changed
    if (&(buf_[0]) != dict_store) {
      dict_size_ = LZ4_loadDict(stream, &(buf_[0]), dict_size_);
      assert(dict_size_ >= 0);
    }
  }

  auto* buf = &(buf_[dict_size_]);
  auto buf_size = static_cast<int>(std::min(
    buf_.size() - dict_size_,
    static_cast<size_t>(std::numeric_limits<int>::max())) // LZ4 API uses int
  );

  #if defined(LZ4_VERSION_NUMBER) && (LZ4_VERSION_NUMBER >= 10700)
    auto lz4_size = LZ4_compress_fast_continue(stream, src, buf, src_size, buf_size, 0); // 0 == use default acceleration
  #else
    auto lz4_size = LZ4_compress_limitedOutput_continue(stream, src, buf, src_size, buf_size); // use for LZ4 <= v1.6.0
  #endif

  if (lz4_size < 0) {
    this->size_ = 0;

    throw index_error("while compressing, error: LZ4 returned negative size");
  }

  this->data_ = reinterpret_cast<const byte_type*>(buf);
  this->size_ = lz4_size;
}

decompressor::decompressor()
  : stream_(LZ4_createStreamDecode(), [](void* ptr)->void { LZ4_freeStreamDecode(reinterpret_cast<LZ4_streamDecode_t*>(ptr)); }) {
}
  
size_t decompressor::deflate(
    const char* src, size_t src_size,
    char* dst, size_t dst_size) const {
  assert(src_size <= integer_traits<int>::const_max); // LZ4 API uses int
  
  auto& stream = *reinterpret_cast<LZ4_streamDecode_t*>(stream_.get());

  const auto lz4_size = LZ4_decompress_safe_continue(
    &stream, 
    src, 
    dst, 
    static_cast<int>(src_size),  // LZ4 API uses int
    static_cast<int>(std::min(dst_size, static_cast<size_t>(integer_traits<int>::const_max))) // LZ4 API uses int
  );

  return lz4_size < 0 
    ? type_limits<type_t::address_t>::invalid() // corrupted index
    : lz4_size;
}

NS_END
