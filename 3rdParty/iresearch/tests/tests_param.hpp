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
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#ifndef IRESEARCH_TESTS_PARAM_H
#define IRESEARCH_TESTS_PARAM_H

#include "store/directory.hpp"
#include "store/directory_attributes.hpp"
#include "utils/ctr_encryption.hpp"
#include <memory>

class test_base;

namespace tests {

// -----------------------------------------------------------------------------
// --SECTION--                                                  rot13_encryption
// -----------------------------------------------------------------------------

class rot13_encryption final : public irs::ctr_encryption {
 public:
  static std::shared_ptr<rot13_encryption> make(
       size_t block_size,
       size_t header_length = DEFAULT_HEADER_LENGTH) {
    return std::make_shared<rot13_encryption>(block_size, header_length);
  }

  explicit rot13_encryption(
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
  class rot13_cipher final : public irs::cipher {
   public:
    explicit rot13_cipher(size_t block_size) noexcept
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

  rot13_cipher cipher_;
  size_t header_length_;
}; // rot13_encryption

// -----------------------------------------------------------------------------
// --SECTION--                                               directory factories
// -----------------------------------------------------------------------------

typedef std::pair<std::shared_ptr<irs::directory>, std::string>(*dir_factory_f)(const test_base*);
std::pair<std::shared_ptr<irs::directory>, std::string> memory_directory(const test_base*);
std::pair<std::shared_ptr<irs::directory>, std::string> fs_directory(const test_base* test);
std::pair<std::shared_ptr<irs::directory>, std::string> mmap_directory(const test_base* test);

template<dir_factory_f DirectoryGenerator, size_t BlockSize>
std::pair<std::shared_ptr<irs::directory>, std::string> rot13_cipher_directory(const test_base* ctx) {
  auto info = DirectoryGenerator(ctx);

  if (info.first) {
    info.first->attributes().emplace<rot13_encryption>(BlockSize);
  }

  return std::make_pair(info.first, info.second + "_cipher_rot13_" + std::to_string(BlockSize));
}

// -----------------------------------------------------------------------------
// --SECTION--                                          directory_test_case_base
// -----------------------------------------------------------------------------

class directory_test_case_base : public virtual test_param_base<tests::dir_factory_f> {
 public:
  static std::string to_string(const testing::TestParamInfo<tests::dir_factory_f>& info);

  virtual void SetUp() override;
  virtual void TearDown() override;

  irs::directory& dir() const noexcept { return *dir_; }

 protected:
  std::shared_ptr<irs::directory> dir_;
}; // directory_test_case_base

} // tests

namespace iresearch {

template<>
struct type<tests::rot13_encryption> : type<encryption> {  };

}

#endif

