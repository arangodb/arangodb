////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2020 ArangoDB GmbH, Cologne, Germany
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
/// @author Andrei Lobov
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_IRESEARCH__TEST_COMPRESSOR_H
#define ARANGODB_IRESEARCH__TEST_COMPRESSOR_H 1

#include "utils/compression.hpp"
#include <functional>

namespace {
// to avoid adding new file to both arangodbtests and arangod 
// we create here a header-only static by using this holder
struct function_holder {
  std::function<irs::bytes_ref(irs::byte_type* src, size_t size, irs::bstring& out)> compress_mock;
  std::function<irs::bytes_ref(irs::byte_type* src, size_t src_size,
    irs::byte_type* dst, size_t dst_size)> decompress_mock;
};
}

namespace iresearch {
namespace compression {
namespace mock {
struct test_compressor {
  class  test_compressor_compressor final : public compressor{
 public:
 
  virtual bytes_ref compress(byte_type* src, size_t size, bstring& out) override {
    return test_compressor::functions().compress_mock ?
             test_compressor::functions().compress_mock(src, size, out) : bytes_ref::EMPTY;
  }
  };

  class test_compressor_decompressor final : public decompressor{
   public:
    virtual bytes_ref decompress(byte_type* src, size_t src_size,
                                  byte_type* dst, size_t dst_size) override {
      return test_compressor::functions().decompress_mock ?
               test_compressor::functions().decompress_mock(src, src_size, dst, dst_size) :
               bytes_ref::EMPTY;
    }
  };

  static void init() {}

  static compression::compressor::ptr compressor(const options& opts) {
    return iresearch::memory::make_shared<test_compressor_compressor>();
  }
  static compression::decompressor::ptr decompressor() {
    return iresearch::memory::make_shared<test_compressor_decompressor>();
  }

  static function_holder& functions() {
    static function_holder holder;
    return holder;
  }

  static iresearch::compression::type_id& type() {
    static iresearch::compression::type_id type("iresearch::compression::mock::test_compressor");
    return type;
  }
};
} // mock
} // compression
} // iresearch
#endif //ARANGODB_IRESEARCH__TEST_COMPRESSOR_H
