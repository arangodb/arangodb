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

#include <memory>

#include "tests_shared.hpp"
#include "store/directory.hpp"
#include "store/directory_attributes.hpp"
#include "utils/ctr_encryption.hpp"
#include "utils/type_id.hpp"

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

std::shared_ptr<irs::directory> memory_directory(
  const test_base*, irs::directory_attributes attrs);
std::shared_ptr<irs::directory> fs_directory(
  const test_base*, irs::directory_attributes attrs);
std::shared_ptr<irs::directory> mmap_directory(
  const test_base*, irs::directory_attributes attrs);

using dir_generator_f = std::shared_ptr<irs::directory>(*)(
  const test_base*, irs::directory_attributes);

template<dir_generator_f DirectoryGenerator>
struct stringify;

template<>
struct stringify<&memory_directory> {
  static std::string type() {
    return "memory";
  }
};

template<>
struct stringify<&fs_directory> {
  static std::string type() {
    return "fs";
  }
};

template<>
struct stringify<&mmap_directory> {
  static std::string type() {
    return "mmap";
  }
};

template<dir_generator_f DirectoryGenerator>
std::pair<std::shared_ptr<irs::directory>, std::string> directory(
    const test_base* ctx) {
  auto dir = DirectoryGenerator(ctx, irs::directory_attributes{});

  return std::make_pair(dir, stringify<DirectoryGenerator>::type());
}

template<dir_generator_f DirectoryGenerator, size_t BlockSize>
std::pair<std::shared_ptr<irs::directory>, std::string> rot13_directory(
    const test_base* ctx) {
  auto dir = DirectoryGenerator(
    ctx,
    irs::directory_attributes{0, std::make_unique<rot13_encryption>(BlockSize)});

  return std::make_pair(
    dir,
    stringify<DirectoryGenerator>::type() + "_cipher_rot13_" + std::to_string(BlockSize));
}

// -----------------------------------------------------------------------------
// --SECTION--                                          directory_test_case_base
// -----------------------------------------------------------------------------

using dir_param_f = std::pair<std::shared_ptr<irs::directory>, std::string>(*)(
  const test_base*);

template<typename... Args>
class directory_test_case_base
   : public virtual test_param_base<std::tuple<tests::dir_param_f, Args...>> {
 public:
  static std::string to_string(
      const testing::TestParamInfo<std::tuple<tests::dir_param_f, Args...>>& info) {
    auto& p = info.param;
    return (*std::get<0>(p))(nullptr).second;
  }

  virtual void SetUp() override {
    test_base::SetUp();

    auto& p = test_param_base<std::tuple<tests::dir_param_f, Args...>>::GetParam();

    auto* factory = std::get<0>(p);
    ASSERT_NE(nullptr, factory);

    dir_ = (*factory)(this).first;
    ASSERT_NE(nullptr, dir_);
  }

  virtual void TearDown() override {
    dir_ = nullptr;
    test_base::TearDown();
  }

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

