////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "utils/compression.hpp"
#include <functional>

namespace {
// to avoid adding new file to both arangodbtests and arangod
// we create here a header-only static by using this holder
struct function_holder {
  std::function<irs::bytes_view(irs::byte_type* src, size_t size,
                                irs::bstring& out)>
      compress_mock;
  std::function<irs::bytes_view(const irs::byte_type* src, size_t src_size,
                                irs::byte_type* dst, size_t dst_size)>
      decompress_mock;
};
}  // namespace

namespace irs::compression::mock {
struct test_compressor {
  class test_compressor_compressor : public ::irs::compression::compressor {
   public:
    bytes_view compress(byte_type* src, size_t size, bstring& out) override {
      return test_compressor::functions().compress_mock
                 ? test_compressor::functions().compress_mock(src, size, out)
                 : kEmptyStringView<byte_type>;
    }
  };

  class test_compressor_decompressor : public ::irs::compression::decompressor {
   public:
    bytes_view decompress(const byte_type* src, size_t src_size, byte_type* dst,
                          size_t dst_size) override {
      return test_compressor::functions().decompress_mock
                 ? test_compressor::functions().decompress_mock(src, src_size,
                                                                dst, dst_size)
                 : kEmptyStringView<byte_type>;
    }
  };

  static compression::compressor::ptr compressor(const options& opts) {
    return irs::memory::make_managed<test_compressor_compressor>();
  }
  static compression::decompressor::ptr decompressor() {
    return irs::memory::make_managed<test_compressor_decompressor>();
  }

  static function_holder& functions() {
    static function_holder holder;
    return holder;
  }

  static constexpr std::string_view type_name() noexcept {
    return "iresearch::compression::mock::test_compressor";
  }
};
}  // namespace irs::compression::mock
