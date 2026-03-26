////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2022 ArangoDB GmbH, Cologne, Germany
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

// clang-format off
#include "store/caching_directory.hpp"
#include "store/fs_directory.hpp"
#include "store/mmap_directory.hpp"

#include "tests_param.hpp"
// clang-format on

namespace tests {

template<typename Impl>
class DirectoryProxy : public Impl {
 public:
  template<typename... Args>
  explicit DirectoryProxy(Args&&... args) noexcept
    : Impl{std::forward<Args>(args)...} {}

  bool exists(bool& result, std::string_view name) const noexcept override {
    EXPECT_TRUE(expect_call_);
    return Impl::exists(result, name);
  }

  bool length(uint64_t& result, std::string_view name) const noexcept override {
    EXPECT_TRUE(expect_call_);
    return Impl::length(result, name);
  }

  irs::index_input::ptr open(std::string_view name,
                             irs::IOAdvice advice) const noexcept override {
    EXPECT_TRUE(expect_call_);
    return Impl::open(name, advice);
  }

  void ExpectCall(bool v) noexcept { expect_call_ = v; }

 private:
  bool expect_call_{false};
};

template<typename Directory>
struct IsProxy : std::false_type {};

template<typename Impl>
struct IsProxy<DirectoryProxy<Impl>> : std::true_type {};

template<typename Impl>
class CachingDirectory
  : public irs::CachingDirectoryBase<Impl, irs::index_input::ptr> {
 public:
  template<typename... Args>
  explicit CachingDirectory(Args&&... args)
    : irs::CachingDirectoryBase<Impl, irs::index_input::ptr>{
        std::forward<Args>(args)...} {}

  bool length(uint64_t& result, std::string_view name) const noexcept final {
    if (this->cache_.Visit(name, [&](auto& stream) noexcept {
          result = stream->length();
          return true;
        })) {
      return true;
    }

    return Impl::length(result, name);
  }

  irs::index_input::ptr open(std::string_view name,
                             irs::IOAdvice advice) const noexcept final {
    if (bool(advice & (irs::IOAdvice::READONCE | irs::IOAdvice::DIRECT_READ))) {
      return Impl::open(name, advice);
    }

    irs::index_input::ptr stream;

    if (this->cache_.Visit(name, [&](auto& cached) noexcept {
          stream = cached->reopen();
          return stream != nullptr;
        })) {
      return stream;
    }

    stream = Impl::open(name, advice);

    if (!stream) {
      return nullptr;
    }

    this->cache_.Put(name, [&]() { return stream->reopen(); });

    return stream;
  }
};

template<typename Directory>
class CachingDirectoryTestCase : public test_base {
 protected:
  void SetUp() final {
    test_base::SetUp();
    dir_ = std::static_pointer_cast<Directory>(MakePhysicalDirectory<Directory>(
      this, irs::directory_attributes{}, size_t{1}));
  }

  void TearDown() final {
    ASSERT_NE(nullptr, dir_);
    dir_->Cache().Clear();
    dir_ = nullptr;
    test_base::TearDown();
  }

  Directory& GetDirectory() noexcept {
    EXPECT_NE(nullptr, dir_);
    return *dir_;
  }

  template<typename IsCached>
  void TestCachingImpl(IsCached&& is_cached);

 private:
  std::shared_ptr<Directory> dir_;
};

template<typename Directory>
template<typename IsCached>
void CachingDirectoryTestCase<Directory>::TestCachingImpl(
  IsCached&& is_cached) {
  auto& dir = GetDirectory();

  auto create_file = [&](std::string_view name, irs::byte_type b) {
    auto stream = dir.create(name);
    ASSERT_NE(nullptr, stream);
    stream->write_byte(b);
  };

  auto check_file = [&](std::string_view name, irs::byte_type b,
                        irs::IOAdvice advice = irs::IOAdvice::NORMAL) {
    auto stream = dir.open(name, advice);
    ASSERT_NE(nullptr, stream);
    ASSERT_EQ(1, stream->length());
    ASSERT_EQ(0, stream->file_pointer());
    ASSERT_EQ(b, stream->read_byte());
    ASSERT_EQ(1, stream->file_pointer());
  };

  ASSERT_EQ(0, dir.Cache().Count());
  ASSERT_EQ(1, dir.Cache().MaxCount());

  if constexpr (IsProxy<typename Directory::ImplType>::value) {
    dir.ExpectCall(true);
  }

  bool exists = true;
  uint64_t length{};

  // File doesn't exist yet
  ASSERT_TRUE(dir.exists(exists, "0"));
  ASSERT_FALSE(exists);
  ASSERT_FALSE(dir.length(length, "0"));
  ASSERT_EQ(nullptr, dir.open("0", irs::IOAdvice::NORMAL));

  ASSERT_FALSE(is_cached("0"));
  create_file("0", 42);
  ASSERT_FALSE(is_cached("0"));
  ASSERT_EQ(0, dir.Cache().Count());
  check_file("0", 42);
  ASSERT_EQ(1, dir.Cache().Count());

  if constexpr (IsProxy<Directory>::value) {
    dir.ExpectCall(false);
  }

  ASSERT_TRUE(is_cached("0"));
  check_file("0", 42);
  ASSERT_EQ(1, dir.Cache().Count());
  ASSERT_TRUE(is_cached("0"));

// Rename
#ifdef _MSC_VER
  constexpr bool kIsCachedAfterRename = false;
#else
  constexpr bool kIsCachedAfterRename = true;
#endif
  ASSERT_TRUE(dir.rename("0", "2"));
  ASSERT_EQ(kIsCachedAfterRename, is_cached("2"));
  check_file("2", 42);  // Entry is cached after first check
  ASSERT_EQ(1, dir.Cache().Count());
  ASSERT_TRUE(is_cached("2"));

  // Following entry must not be cached because of cache size
  if constexpr (IsProxy<typename Directory::ImplType>::value) {
    dir.ExpectCall(true);
  }
  create_file("1", 24);
  ASSERT_FALSE(is_cached("1"));
  ASSERT_EQ(1, dir.Cache().Count());
  check_file("1", 24);
  ASSERT_FALSE(is_cached("1"));
  ASSERT_EQ(1, dir.Cache().Count());
  check_file("1", 24);
  ASSERT_FALSE(is_cached("1"));
  ASSERT_EQ(1, dir.Cache().Count());

  // Remove
  ASSERT_TRUE(dir.remove("2"));
  ASSERT_EQ(0, dir.Cache().Count());
  ASSERT_FALSE(is_cached("2"));

  // We don't use cache for readonce files
  check_file("1", 24, irs::IOAdvice::READONCE);
  ASSERT_EQ(0, dir.Cache().Count());
  ASSERT_FALSE(is_cached("1"));

  check_file("1", 24);
  ASSERT_EQ(1, dir.Cache().Count());
  ASSERT_TRUE(is_cached("1"));

  // We now can use cache
  if constexpr (IsProxy<typename Directory::ImplType>::value) {
    dir.ExpectCall(false);
  }
  check_file("1", 24);
  ASSERT_EQ(1, dir.Cache().Count());
  ASSERT_TRUE(is_cached("1"));

  dir.Cache().Clear();
  ASSERT_EQ(0, dir.Cache().Count());
  ASSERT_FALSE(is_cached("1"));
}

using CachingDirectoryTest = CachingDirectoryTestCase<
  CachingDirectory<DirectoryProxy<irs::MMapDirectory>>>;

TEST_F(CachingDirectoryTest, TestCaching) {
  TestCachingImpl([&](std::string_view name) -> bool {
    irs::index_input* handle{};
    const bool found = GetDirectory().Cache().Visit(name, [&](auto& cached) {
      handle = cached.get();
      return true;
    });
    return found && handle;
  });
}

using CachingMMapDirectoryTest =
  CachingDirectoryTestCase<irs::CachingMMapDirectory>;

TEST_F(CachingMMapDirectoryTest, TestCaching) {
  TestCachingImpl([&](std::string_view name) -> bool {
    std::shared_ptr<irs::mmap_utils::mmap_handle> handle;
    const bool found = GetDirectory().Cache().Visit(name, [&](auto& cached) {
      handle = cached;
      return true;
    });
    return found && 2 == handle.use_count();
  });
}

using CachingFSDirectoryTest =
  CachingDirectoryTestCase<irs::CachingFSDirectory>;

TEST_F(CachingFSDirectoryTest, TestCaching) {
  TestCachingImpl([&](std::string_view name) -> bool {
    uint64_t size = std::numeric_limits<uint64_t>::max();
    const bool found = GetDirectory().Cache().Visit(name, [&](uint64_t cached) {
      size = cached;
      return true;
    });
    return found && 1 == size;
  });
}

}  // namespace tests
