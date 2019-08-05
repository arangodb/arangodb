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

#include "shared.hpp"
#include "lz4compression.hpp"
#include "error/error.hpp"
#include "utils/string_utils.hpp"
#include "utils/type_limits.hpp"

NS_ROOT
NS_BEGIN(compression)

static_assert(
  sizeof(char) == sizeof(byte_type),
  "sizeof(char) != sizeof(byte_type)"
);

bytes_ref lz4compressor::compress(byte_type* src, size_t size, bstring& out) {
  assert(size <= integer_traits<int>::const_max); // LZ4 API uses int
  auto src_size = static_cast<int>(size);
  auto src_data = reinterpret_cast<const char*>(src);

  // ensure LZ4 dictionary from the previous run is at the start of buf
  {
    auto* dict_store = dict_size_ ? reinterpret_cast<char*>(&out[0]) : nullptr;
    const size_t compression_bound = size_t(LZ4_compressBound(src_size));

    // ensure we have enough space to store dictionary from previous run
    string_utils::oversize(out, compression_bound + size_t(dict_size_));

    // move the LZ4 dictionary from the previous run to the start of buf
    if (dict_store) {
      const auto dict_size = LZ4_saveDict(stream(), dict_store, dict_size_);
      assert(dict_size >= 0);

      const auto required_size = compression_bound + size_t(dict_size);

      if (IRS_UNLIKELY(out.size() < required_size)) {
        // very very unlikely...
        // basically guard against unexpected changes in lz4
        string_utils::oversize(out, required_size);
      }

      dict_size_ = dict_size;
    }

    // reload the LZ4 dictionary if buf has changed
    if (reinterpret_cast<char*>(&out[0]) != dict_store) {
      dict_size_ = LZ4_loadDict(stream(), reinterpret_cast<char*>(&out[0]), dict_size_);
      assert(dict_size_ >= 0);
    }
  }

  auto* buf = reinterpret_cast<char*>(&out[size_t(dict_size_)]);
  auto buf_size = static_cast<int>(std::min(
    out.size() - size_t(dict_size_),
    size_t(integer_traits<int>::const_max)) // LZ4 API uses int
  );

  #if defined(LZ4_VERSION_NUMBER) && (LZ4_VERSION_NUMBER >= 10700)
    const auto lz4_size = LZ4_compress_fast_continue(stream(), src_data, buf, src_size, buf_size, acceleration_);
  #else
    const auto lz4_size = LZ4_compress_limitedOutput_continue(stream(), src_data, buf, src_size, buf_size); // use for LZ4 <= v1.6.0
  #endif

  if (IRS_UNLIKELY(lz4_size < 0)) {
    throw index_error("while compressing, error: LZ4 returned negative size");
  }

  return bytes_ref(reinterpret_cast<const byte_type*>(buf), size_t(lz4_size));
}

bytes_ref lz4decompressor::decompress(
    byte_type* src,  size_t src_size,
    byte_type* dst,  size_t dst_size) {
  assert(src_size <= integer_traits<int>::const_max); // LZ4 API uses int
  
  const auto lz4_size = LZ4_decompress_safe_continue(
    stream(),
    reinterpret_cast<const char*>(src),
    reinterpret_cast<char*>(dst),
    static_cast<int>(src_size),  // LZ4 API uses int
    static_cast<int>(std::min(dst_size, static_cast<size_t>(integer_traits<int>::const_max))) // LZ4 API uses int
  );

  if (IRS_UNLIKELY(lz4_size < 0)) {
    return bytes_ref::NIL; // corrupted index
  }

  return bytes_ref(dst, size_t(lz4_size));
}

compressor::ptr lz4::compressor() { return std::make_shared<lz4compressor>(); }
decompressor::ptr lz4::decompressor() { return std::make_shared<lz4decompressor>(); }

void lz4::init() {
  // match registration below
  REGISTER_COMPRESSION(lz4, &lz4::compressor, &lz4::decompressor);
}

DEFINE_COMPRESSION_TYPE(iresearch::compression::lz4,
                        iresearch::compression::type_id::Scope::GLOBAL);

REGISTER_COMPRESSION(lz4, &lz4::compressor, &lz4::decompressor);

NS_END // compression
NS_END
