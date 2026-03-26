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

#pragma once

#include <array>
#include <bit>
#include <memory>

#include "store/caching_directory.hpp"
#include "store/directory.hpp"
#include "store/directory_attributes.hpp"
#include "tests_shared.hpp"
#include "utils/ctr_encryption.hpp"
#include "utils/type_id.hpp"

class test_base;

namespace tests {

class rot13_encryption final : public irs::ctr_encryption {
 public:
  static std::shared_ptr<rot13_encryption> make(
    size_t block_size, size_t header_length = DEFAULT_HEADER_LENGTH) {
    return std::make_shared<rot13_encryption>(block_size, header_length);
  }

  explicit rot13_encryption(
    size_t block_size, size_t header_length = DEFAULT_HEADER_LENGTH) noexcept
    : irs::ctr_encryption(cipher_),
      cipher_(block_size),
      header_length_(header_length) {}

  size_t header_length() noexcept final { return header_length_; }

 private:
  class rot13_cipher final : public irs::cipher {
   public:
    explicit rot13_cipher(size_t block_size) noexcept
      : block_size_(block_size) {}

    size_t block_size() const noexcept final { return block_size_; }

    bool decrypt(irs::byte_type* data) const final {
      for (size_t i = 0; i < block_size_; ++i) {
        data[i] -= 13;
      }
      return true;
    }

    bool encrypt(irs::byte_type* data) const final {
      for (size_t i = 0; i < block_size_; ++i) {
        data[i] += 13;
      }
      return true;
    }

   private:
    size_t block_size_;
  };

  rot13_cipher cipher_;
  size_t header_length_;
};

template<typename Impl, typename... Args>
std::shared_ptr<irs::directory> MakePhysicalDirectory(
  const test_base* test, irs::directory_attributes attrs, Args&&... args) {
  if (test) {
    const auto dir_path = test->test_dir() / "index";
    std::filesystem::create_directories(dir_path);

    auto dir = std::make_unique<Impl>(std::forward<Args>(args)..., dir_path,
                                      std::move(attrs),
                                      test->GetResourceManager().options);

    return {dir.release(), [dir_path = std::move(dir_path)](irs::directory* p) {
              std::filesystem::remove_all(dir_path);
              delete p;
            }};
  }

  return nullptr;
}

std::shared_ptr<irs::directory> memory_directory(
  const test_base*, irs::directory_attributes attrs);
std::shared_ptr<irs::directory> fs_directory(const test_base*,
                                             irs::directory_attributes attrs);
std::shared_ptr<irs::directory> mmap_directory(const test_base*,
                                               irs::directory_attributes attrs);
#ifdef IRESEARCH_URING
std::shared_ptr<irs::directory> async_directory(
  const test_base*, irs::directory_attributes attrs);
#endif

using dir_generator_f = std::shared_ptr<irs::directory> (*)(
  const test_base*, irs::directory_attributes);

std::string to_string(dir_generator_f generator);

template<dir_generator_f DirectoryGenerator>
std::pair<std::shared_ptr<irs::directory>, std::string> directory(
  const test_base* ctx) {
  auto dir = DirectoryGenerator(ctx, irs::directory_attributes{});

  return std::make_pair(dir, to_string(DirectoryGenerator));
}

template<dir_generator_f DirectoryGenerator, size_t BlockSize>
std::pair<std::shared_ptr<irs::directory>, std::string> rot13_directory(
  const test_base* ctx) {
  auto dir = DirectoryGenerator(
    ctx,
    irs::directory_attributes{std::make_unique<rot13_encryption>(BlockSize)});

  return std::make_pair(dir, to_string(DirectoryGenerator) + "_cipher_rot13_" +
                               std::to_string(BlockSize));
}

using dir_param_f =
  std::pair<std::shared_ptr<irs::directory>, std::string> (*)(const test_base*);

enum Types : uint64_t {
  kTypesDefault = 1 << 0,
  kTypesRot13_16 = 1 << 2,
  kTypesRot13_7 = 1 << 3,
};

// #define IRESEARCH_ONLY_MEMORY

template<uint64_t Type>
constexpr auto getDirectories() {
  constexpr auto kCount = std::popcount(Type);
#ifdef IRESEARCH_ONLY_MEMORY
  std::array<dir_param_f, kCount * 1> data;
#elif defined(IRESEARCH_URING)
  std::array<dir_param_f, kCount * 4> data;
#else
  std::array<dir_param_f, kCount * 3> data;
#endif
  auto* p = data.data();
  if constexpr (Type & kTypesDefault) {
    *p++ = &tests::directory<&tests::memory_directory>;
#ifndef IRESEARCH_ONLY_MEMORY
#ifdef IRESEARCH_URING
    *p++ = &tests::directory<&tests::async_directory>;
#endif
    *p++ = &tests::directory<&tests::mmap_directory>;
    *p++ = &tests::directory<&tests::fs_directory>;
#endif
  }
  if constexpr (Type & kTypesRot13_16) {
    *p++ = &tests::rot13_directory<&tests::memory_directory, 16>;
#ifndef IRESEARCH_ONLY_MEMORY
#ifdef IRESEARCH_URING
    *p++ = &tests::rot13_directory<&tests::async_directory, 16>;
#endif
    *p++ = &tests::rot13_directory<&tests::mmap_directory, 16>;
    *p++ = &tests::rot13_directory<&tests::fs_directory, 16>;
#endif
  }
  if constexpr (Type & kTypesRot13_7) {
    *p++ = &tests::rot13_directory<&tests::memory_directory, 7>;
#ifndef IRESEARCH_ONLY_MEMORY
#ifdef IRESEARCH_URING
    *p++ = &tests::rot13_directory<&tests::async_directory, 7>;
#endif
    *p++ = &tests::rot13_directory<&tests::mmap_directory, 7>;
    *p++ = &tests::rot13_directory<&tests::fs_directory, 7>;
#endif
  }
  return data;
}

template<typename... Args>
class directory_test_case_base
  : public virtual test_param_base<std::tuple<tests::dir_param_f, Args...>> {
 public:
  using ParamType = std::tuple<tests::dir_param_f, Args...>;

  static std::string to_string(const testing::TestParamInfo<ParamType>& info) {
    auto& p = info.param;
    return (*std::get<0>(p))(nullptr).second;
  }

  void SetUp() final {
    test_base::SetUp();

    auto& p =
      test_param_base<std::tuple<tests::dir_param_f, Args...>>::GetParam();

    auto* factory = std::get<0>(p);
    ASSERT_NE(nullptr, factory);

    dir_ = (*factory)(this).first;
    ASSERT_NE(nullptr, dir_);
  }

  void TearDown() final {
    dir_ = nullptr;
    test_base::TearDown();
  }

  irs::directory& dir() const noexcept { return *dir_; }

 protected:
  std::shared_ptr<irs::directory> dir_;
};

}  // namespace tests

namespace irs {

template<>
struct type<tests::rot13_encryption> : type<encryption> {};

}  // namespace irs
