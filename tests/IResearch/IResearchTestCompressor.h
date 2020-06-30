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
#include "utils/ctr_encryption.hpp"
#include <functional>

namespace {
// to avoid adding new file to both arangodbtests and arangod 
// we create here a header-only static by using this holder
struct function_holder {
  std::function<irs::bytes_ref(irs::byte_type* src, size_t size, irs::bstring& out)> compress_mock;
  std::function<irs::bytes_ref(const irs::byte_type* src, size_t src_size,
    irs::byte_type* dst, size_t dst_size)> decompress_mock;
};
}

namespace iresearch {
namespace compression {
namespace mock {
struct test_compressor {
  class test_compressor_compressor final : public compressor {
   public:
    virtual bytes_ref compress(byte_type* src, size_t size, bstring& out) override {
      return test_compressor::functions().compress_mock ?
                              test_compressor::functions().compress_mock(src, size, out) : bytes_ref::EMPTY;
    }
  };

  class test_compressor_decompressor final : public decompressor {
   public:
    virtual bytes_ref decompress(const byte_type* src, size_t src_size,
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

  static constexpr irs::string_ref type_name() noexcept {
    return "iresearch::compression::mock::test_compressor";
  }
};
} // mock
} // compression

namespace mock {

class test_encryption final : public ctr_encryption {
public:
  static std::shared_ptr<test_encryption> make(
    size_t block_size,
    size_t header_length = DEFAULT_HEADER_LENGTH) {
    return std::make_shared<test_encryption>(block_size, header_length);
  }

  explicit test_encryption(
    size_t block_size,
    size_t header_length = DEFAULT_HEADER_LENGTH
    ) noexcept
    : irs::ctr_encryption(cipher_),
    cipher_(block_size),
    header_length_(header_length) {
  }

  virtual size_t header_length() noexcept override {
    return header_length_;
  }

private:
  class test_cipher final : public irs::cipher {
  public:
    explicit test_cipher(size_t block_size) noexcept
      : block_size_(block_size) {
    }

    virtual size_t block_size() const noexcept override {
      return block_size_;
    }

    virtual bool decrypt(irs::byte_type* data) const override {
      for (size_t i = 0; i < block_size_; ++i) {
        data[i] -= 13;
      }
      return true;
    }

    virtual bool encrypt(irs::byte_type* data) const override {
      for (size_t i = 0; i < block_size_; ++i) {
        data[i] += 13;
      }
      return true;
    }

  private:
    size_t block_size_;
  }; // rot13_cipher

  test_cipher cipher_;
  size_t header_length_;
}; // rot13_encryption
} // mock

} // iresearch
#endif //ARANGODB_IRESEARCH__TEST_COMPRESSOR_H
