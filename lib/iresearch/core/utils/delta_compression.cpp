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

#include "delta_compression.hpp"

#include "shared.hpp"
#include "store/store_utils.hpp"

namespace irs::compression {
namespace {

delta_compressor kCompressor;
delta_decompressor kDecompressor;

}  // namespace

bytes_view delta_compressor::compress(byte_type* src, size_t size,
                                      bstring& buf) {
  auto* begin = reinterpret_cast<uint64_t*>(src);
  auto* end = reinterpret_cast<uint64_t*>(src + size);
  encode::delta::encode(begin, end);

  // ensure we have enough space in the worst case
  IRS_ASSERT(end >= begin);
  buf.resize(static_cast<size_t>(std::distance(begin, end)) *
             bytes_io<uint64_t>::const_max_vsize);

  auto* out = buf.data();
  for (; begin != end; ++begin) {
    vwrite(out, zig_zag_encode64(static_cast<int64_t>(*begin)));
  }

  IRS_ASSERT(out >= buf.data());
  return {buf.c_str(), static_cast<size_t>(out - buf.data())};
}

bytes_view delta_decompressor::decompress(const byte_type* src, size_t src_size,
                                          byte_type* dst, size_t dst_size) {
  auto* dst_end = reinterpret_cast<uint64_t*>(dst);

  for (const auto* src_end = src + src_size; src != src_end; ++dst_end) {
    *dst_end = static_cast<uint64_t>(zig_zag_decode64(vread<uint64_t>(src)));
  }

  encode::delta::decode(reinterpret_cast<uint64_t*>(dst), dst_end);

  return {dst, dst_size};
}

compressor::ptr delta::compressor(const options& /*opts*/) {
  return memory::to_managed<delta_compressor>(kCompressor);
}

decompressor::ptr delta::decompressor() {
  return memory::to_managed<delta_decompressor>(kDecompressor);
}

void delta::init() {
  // match registration below
  REGISTER_COMPRESSION(delta, &delta::compressor, &delta::decompressor);
}

REGISTER_COMPRESSION(delta, &delta::compressor, &delta::decompressor);

}  // namespace irs::compression
