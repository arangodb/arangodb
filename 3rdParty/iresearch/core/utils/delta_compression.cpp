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
#include "delta_compression.hpp"
#include "store/store_utils.hpp"

namespace {

irs::compression::delta_compressor COMPRESSOR;
irs::compression::delta_decompressor DECOMPRESSOR;

}

namespace iresearch {
namespace compression {

bytes_ref delta_compressor::compress(byte_type* src, size_t size, bstring& buf) {
  auto* begin = reinterpret_cast<uint64_t*>(src);
  auto* end = reinterpret_cast<uint64_t*>(src + size);
  encode::delta::encode(begin, end);

  // ensure we have enough space in the worst case
  assert(end >= begin);
  buf.resize(size_t(std::distance(begin, end))*bytes_io<uint64_t>::const_max_vsize);

  auto* out = const_cast<byte_type*>(buf.data());
  for (;begin != end; ++begin) {
    vwrite(out, zig_zag_encode64(int64_t(*begin)));
  }

  assert(out >= buf.data());
  return { buf.c_str(), size_t(out - buf.data()) };
}

bytes_ref delta_decompressor::decompress(
    const byte_type* src, size_t src_size,
    byte_type* dst, size_t dst_size) {

  auto* dst_end = reinterpret_cast<uint64_t*>(dst);

  for (const auto* src_end = src + src_size; src != src_end; ++dst_end) {
    *dst_end = uint64_t(zig_zag_decode64(vread<uint64_t>(src)));
  }

  encode::delta::decode(reinterpret_cast<uint64_t*>(dst), dst_end);

  return bytes_ref(dst, dst_size);
}

compressor::ptr delta::compressor(const options& /*opts*/) {
  return compressor::ptr(compressor::ptr(), &COMPRESSOR);
}

decompressor::ptr delta::decompressor() {
  return decompressor::ptr(decompressor::ptr(), &DECOMPRESSOR);
}

void delta::init() {
  // match registration below
  REGISTER_COMPRESSION(delta, &delta::compressor, &delta::decompressor);
}

REGISTER_COMPRESSION(delta, &delta::compressor, &delta::decompressor);

} // compression
}
