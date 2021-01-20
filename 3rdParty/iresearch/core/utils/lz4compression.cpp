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
#include "store/store_utils.hpp"
#include "utils/string_utils.hpp"
#include "utils/misc.hpp"
#include "utils/type_limits.hpp"

#include <lz4.h>

namespace {

// can reuse stateless instances
irs::compression::lz4::lz4compressor LZ4_BASIC_COMPRESSOR;
irs::compression::lz4::lz4decompressor LZ4_BASIC_DECOMPRESSOR;

inline int acceleration(const irs::compression::options::Hint hint) noexcept {
  static const int FACTORS[] { 0, 2, 0 };
  assert(static_cast<size_t>(hint) < IRESEARCH_COUNTOF(FACTORS));

  return FACTORS[static_cast<size_t>(hint)];
}

}

namespace iresearch {

static_assert(
  sizeof(char) == sizeof(byte_type),
  "sizeof(char) != sizeof(byte_type)"
);

namespace compression {

void LZ4_streamDecode_deleter::operator()(void *p) noexcept {
  if (p) {
    LZ4_freeStreamDecode(reinterpret_cast<LZ4_streamDecode_t*>(p));
  }
}

void LZ4_stream_deleter::operator()(void *p) noexcept {
  if (p) {
    LZ4_freeStream(reinterpret_cast<LZ4_stream_t*>(p));
  }
}

lz4stream lz4_make_stream() {
  return lz4stream(LZ4_createStream());
}

lz4stream_decode lz4_make_stream_decode() {
  return lz4stream_decode(LZ4_createStreamDecode());
}

// -----------------------------------------------------------------------------
// --SECTION--                                                   lz4 compression
// -----------------------------------------------------------------------------

bytes_ref lz4::lz4compressor::compress(byte_type* src, size_t size, bstring& out) {
  assert(size <= integer_traits<int>::const_max); // LZ4 API uses int
  const auto src_size = static_cast<int>(size);

  // ensure we have enough space to store compressed data
  string_utils::oversize(out, size_t(LZ4_COMPRESSBOUND(src_size)));

  const auto* src_data = reinterpret_cast<const char*>(src);
  auto* buf = reinterpret_cast<char*>(&out[0]);
  const auto buf_size = static_cast<int>(out.size());
  const auto lz4_size = LZ4_compress_fast(src_data, buf, src_size, buf_size, acceleration_);

  if (IRS_UNLIKELY(lz4_size < 0)) {
    throw index_error("while compressing, error: LZ4 returned negative size");
  }

  return bytes_ref(reinterpret_cast<const byte_type*>(buf), size_t(lz4_size));
}

bytes_ref lz4::lz4decompressor::decompress(
    const byte_type* src,  size_t src_size,
    byte_type* dst,  size_t dst_size) {
  assert(src_size <= integer_traits<int>::const_max); // LZ4 API uses int

  const auto lz4_size = LZ4_decompress_safe(
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

compressor::ptr lz4::compressor(const options& opts) {
  const auto acceleration = ::acceleration(opts.hint);

  if (0 == acceleration) {
    return compressor::ptr(compressor::ptr(), &LZ4_BASIC_COMPRESSOR);
  }

  return memory::make_shared<lz4compressor>(acceleration);
}

decompressor::ptr lz4::decompressor() {
  return decompressor::ptr(decompressor::ptr(), &LZ4_BASIC_DECOMPRESSOR);
}

void lz4::init() {
  // match registration below
  REGISTER_COMPRESSION(lz4, &lz4::compressor, &lz4::decompressor);
}

REGISTER_COMPRESSION(lz4, &lz4::compressor, &lz4::decompressor);

} // compression
}
