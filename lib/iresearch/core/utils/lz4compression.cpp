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

#include "lz4compression.hpp"

#include <lz4.h>

#include "error/error.hpp"
#include "shared.hpp"
#include "utils/misc.hpp"
#include "utils/type_limits.hpp"

namespace irs {
namespace {

// can reuse stateless instances
compression::lz4::lz4compressor kLZ4BasicCompressor;
compression::lz4::lz4decompressor kLZ4BasicDecompressor;

inline int acceleration(const compression::options::Hint hint) noexcept {
  static constexpr int FACTORS[]{0, 2, 0};
  IRS_ASSERT(static_cast<size_t>(hint) < std::size(FACTORS));

  return FACTORS[static_cast<size_t>(hint)];
}

}  // namespace

static_assert(sizeof(char) == sizeof(byte_type));

namespace compression {

void LZ4_streamDecode_deleter::operator()(void* p) noexcept {
  if (p != nullptr) {
    LZ4_freeStreamDecode(reinterpret_cast<LZ4_streamDecode_t*>(p));
  }
}

void LZ4_stream_deleter::operator()(void* p) noexcept {
  if (p != nullptr) {
    LZ4_freeStream(reinterpret_cast<LZ4_stream_t*>(p));
  }
}

lz4stream lz4_make_stream() { return lz4stream(LZ4_createStream()); }

lz4stream_decode lz4_make_stream_decode() {
  return lz4stream_decode(LZ4_createStreamDecode());
}

bytes_view lz4::lz4compressor::compress(byte_type* src, size_t size,
                                        bstring& out) {
  IRS_ASSERT(size <= static_cast<unsigned>(
                       std::numeric_limits<int>::max()));  // LZ4 API uses int
  const auto src_size = static_cast<int>(size);

  // Ensure we have enough space to store compressed data,
  // but preserve original size
  const uint32_t bound = LZ4_COMPRESSBOUND(src_size);
  out.resize(std::max(out.size(), size_t{bound}));

  const auto* src_data = reinterpret_cast<const char*>(src);
  auto* buf = reinterpret_cast<char*>(out.data());
  const auto buf_size = static_cast<int>(out.size());
  const auto lz4_size =
    LZ4_compress_fast(src_data, buf, src_size, buf_size, acceleration_);

  if (IRS_UNLIKELY(lz4_size < 0)) {
    throw index_error{"While compressing, error: LZ4 returned negative size"};
  }

  return {reinterpret_cast<const byte_type*>(buf),
          static_cast<size_t>(lz4_size)};
}

bytes_view lz4::lz4decompressor::decompress(const byte_type* src,
                                            size_t src_size, byte_type* dst,
                                            size_t dst_size) {
  IRS_ASSERT(src_size <=
             static_cast<unsigned>(
               std::numeric_limits<int>::max()));  // LZ4 API uses int

  const auto lz4_size = LZ4_decompress_safe(
    reinterpret_cast<const char*>(src), reinterpret_cast<char*>(dst),
    static_cast<int>(src_size),  // LZ4 API uses int
    static_cast<int>(std::min(
      dst_size, static_cast<size_t>(
                  std::numeric_limits<int>::max())))  // LZ4 API uses int
  );

  if (IRS_UNLIKELY(lz4_size < 0)) {
    return {};  // corrupted index
  }

  return bytes_view{dst, static_cast<size_t>(lz4_size)};
}

compressor::ptr lz4::compressor(const options& opts) {
  const auto acceleration = irs::acceleration(opts.hint);

  if (0 == acceleration) {
    return memory::to_managed<lz4compressor>(kLZ4BasicCompressor);
  }

  return memory::make_managed<lz4compressor>(acceleration);
}

decompressor::ptr lz4::decompressor() {
  return memory::to_managed<lz4decompressor>(kLZ4BasicDecompressor);
}

void lz4::init() {
  // match registration below
  REGISTER_COMPRESSION(lz4, &lz4::compressor, &lz4::decompressor);
}

REGISTER_COMPRESSION(lz4, &lz4::compressor, &lz4::decompressor);

}  // namespace compression
}  // namespace irs
