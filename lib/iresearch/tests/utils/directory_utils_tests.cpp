////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2016 by EMC Corporation, All Rights Reserved
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
/// Copyright holder is EMC Corporation
///
/// @author Andrey Abramov
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#include <unordered_set>

#include "index/index_meta.hpp"
#include "index/index_tests.hpp"
#include "store/memory_directory.hpp"
#include "tests_shared.hpp"
#include "utils/directory_utils.hpp"

namespace {

class directory_utils_tests : public ::testing::Test {
 protected:
  class directory_mock : public irs::directory {
   public:
    using directory::attributes;

    irs::directory_attributes& attributes() noexcept final { return attrs_; }

    irs::index_output::ptr create(std::string_view) noexcept final {
      return nullptr;
    }

    bool exists(bool&, std::string_view) const noexcept final { return false; }

    bool length(uint64_t&, std::string_view) const noexcept final {
      return false;
    }

    irs::index_lock::ptr make_lock(std::string_view) noexcept final {
      return nullptr;
    }

    bool mtime(std::time_t&, std::string_view) const noexcept final {
      return false;
    }

    irs::index_input::ptr open(std::string_view,
                               irs::IOAdvice) const noexcept final {
      return nullptr;
    }

    bool remove(std::string_view) noexcept final { return false; }

    bool rename(std::string_view, std::string_view) noexcept final {
      return false;
    }

    bool sync(std::span<const std::string_view>) noexcept final {
      return false;
    }

    bool visit(const irs::directory::visitor_f&) const final { return false; }

   private:
    irs::directory_attributes attrs_{};
  };

  struct callback_directory : tests::directory_mock {
    typedef std::function<void()> AfterCallback;

    explicit callback_directory(irs::directory& impl, AfterCallback&& p)
      : tests::directory_mock(impl), after(p) {}

    irs::index_input::ptr open(std::string_view name,
                               irs::IOAdvice advice) const noexcept final {
      auto stream = tests::directory_mock::open(name, advice);
      after();
      return stream;
    }

    AfterCallback after;
  };
};

}  // namespace

TEST_F(directory_utils_tests, test_reference) {
  // test add file
  {
    irs::memory_directory dir;
    auto file = dir.create("abc");

    ASSERT_FALSE(!file);
    ASSERT_TRUE(static_cast<bool>(irs::directory_utils::Reference(dir, "abc")));

    auto& attribute = dir.attributes().refs();

    ASSERT_FALSE(attribute.refs().empty());
    ASSERT_TRUE(attribute.refs().contains("abc"));
    ASSERT_FALSE(attribute.refs().contains("def"));
  }
}

TEST_F(directory_utils_tests, test_ref_tracking_dir) {
  // test move constructor
  {
    irs::memory_directory dir;
    irs::RefTrackingDirectory track_dir1(dir);
    irs::RefTrackingDirectory track_dir2(std::move(track_dir1));

    ASSERT_EQ(&dir, &(*track_dir2));
  }

  // test dereference and attributes
  {
    irs::memory_directory dir;
    irs::RefTrackingDirectory track_dir(dir);

    ASSERT_EQ(&dir, &(*track_dir));
    ASSERT_EQ(&(dir.attributes()), &(track_dir.attributes()));
  }

  // test make_lock
  {
    irs::memory_directory dir;
    irs::RefTrackingDirectory track_dir(dir);
    auto lock1 = dir.make_lock("abc");
    ASSERT_FALSE(!lock1);
    auto lock2 = track_dir.make_lock("abc");
    ASSERT_FALSE(!lock2);

    ASSERT_TRUE(lock1->lock());
    ASSERT_FALSE(lock2->lock());
    ASSERT_TRUE(lock1->unlock());
    ASSERT_TRUE(lock2->lock());
  }

  // test open
  {
    irs::memory_directory dir;
    irs::RefTrackingDirectory track_dir(dir);
    const std::string_view file{"abc"};
    auto file1 = dir.create(file);

    ASSERT_FALSE(!file1);
    file1->write_byte(42);
    file1->flush();

    auto file2 = track_dir.open(file, irs::IOAdvice::NORMAL);

    ASSERT_FALSE(!file2);
    // does nothing in memory_directory, but adds line coverage
    ASSERT_TRUE(track_dir.sync({&file, 1}));
    ASSERT_EQ(1, file2->length());
    ASSERT_TRUE(track_dir.rename(file, "def"));
    file1.reset();  // release before remove
    file2.reset();  // release before remove
    ASSERT_FALSE(track_dir.remove(file));
    bool exists;
    ASSERT_TRUE(dir.exists(exists, file) && !exists);
    ASSERT_TRUE(dir.exists(exists, "def") && exists);
    ASSERT_TRUE(track_dir.remove("def"));
    ASSERT_TRUE(dir.exists(exists, "def") && !exists);
  }

  // test visit refs visitor complete
  {
    irs::memory_directory dir;
    irs::RefTrackingDirectory track_dir(dir);
    auto file1 = track_dir.create("abc");
    ASSERT_FALSE(!file1);
    auto file2 = track_dir.create("def");
    ASSERT_FALSE(!file2);

    ASSERT_EQ(2, track_dir.GetRefs().size());
  }

  // test visit refs visitor (no-track-open)
  {
    irs::memory_directory dir;
    irs::RefTrackingDirectory track_dir(dir);
    auto file1 = dir.create("abc");
    ASSERT_FALSE(!file1);
    auto file2 = track_dir.open("abc", irs::IOAdvice::NORMAL);
    ASSERT_FALSE(!file2);

    ASSERT_EQ(0, track_dir.GetRefs().size());
  }

  // test visit refs visitor (track-open)
  {
    irs::memory_directory dir;
    irs::RefTrackingDirectory track_dir(dir, true);
    auto file1 = dir.create("abc");
    ASSERT_FALSE(!file1);
    auto file2 = track_dir.open("abc", irs::IOAdvice::NORMAL);
    ASSERT_FALSE(!file2);

    ASSERT_EQ(1, track_dir.GetRefs().size());
  }

  // test open (track-open)
  {
    irs::memory_directory dir;

    auto clean = [&dir]() { irs::directory_utils::RemoveAllUnreferenced(dir); };

    callback_directory callback_dir{dir, clean};
    irs::RefTrackingDirectory track_dir(callback_dir, true);
    auto file1 = dir.create("abc");
    ASSERT_NE(nullptr, file1);
    auto file2 = track_dir.open("abc", irs::IOAdvice::NORMAL);
    ASSERT_NE(nullptr, file2);

    ASSERT_EQ(1, track_dir.GetRefs().size());

    auto file3 = dir.open("abc", irs::IOAdvice::NORMAL);
    ASSERT_NE(nullptr, file3);
  }

  {
    irs::memory_directory dir;
    irs::RefTrackingDirectory track_dir(dir);
    auto file1 = track_dir.create("abc");
    ASSERT_FALSE(!file1);
    auto file2 = track_dir.create("def");
    ASSERT_FALSE(!file2);

    ASSERT_EQ(2, track_dir.GetRefs().size());
  }

  // ...........................................................................
  // errors during file operations
  // ...........................................................................

  struct error_directory : public irs::directory {
    irs::directory_attributes attrs{};
    irs::directory_attributes& attributes() noexcept final { return attrs; }
    irs::index_output::ptr create(std::string_view) noexcept final {
      return nullptr;
    }
    bool exists(bool&, std::string_view) const noexcept final { return false; }
    bool length(uint64_t&, std::string_view) const noexcept final {
      return false;
    }
    bool visit(const visitor_f&) const final { return false; }
    irs::index_lock::ptr make_lock(std::string_view) noexcept final {
      return nullptr;
    }
    bool mtime(std::time_t&, std::string_view) const noexcept final {
      return false;
    }
    irs::index_input::ptr open(std::string_view,
                               irs::IOAdvice) const noexcept final {
      return nullptr;
    }
    bool remove(std::string_view) noexcept final { return false; }
    bool rename(std::string_view, std::string_view) noexcept final {
      return false;
    }
    bool sync(std::span<const std::string_view>) noexcept final {
      return false;
    }
  } error_dir;

  // test create failure
  {
    irs::RefTrackingDirectory track_dir(error_dir);

    ASSERT_FALSE(track_dir.create("abc"));

    ASSERT_TRUE(track_dir.GetRefs().empty());
  }

  // test open failure
  {
    irs::RefTrackingDirectory track_dir(error_dir, true);

    ASSERT_FALSE(track_dir.open("abc", irs::IOAdvice::NORMAL));

    ASSERT_TRUE(track_dir.GetRefs().empty());
  }
}

TEST_F(directory_utils_tests, test_tracking_dir) {
  // test dereference and attributes
  {
    irs::memory_directory dir;
    irs::TrackingDirectory track_dir(dir);

    ASSERT_EQ(&dir, &track_dir.GetImpl());
    ASSERT_EQ(&(dir.attributes()), &(track_dir.attributes()));
  }

  // test make_lock
  {
    irs::memory_directory dir;
    irs::TrackingDirectory track_dir(dir);
    auto lock1 = dir.make_lock("abc");
    ASSERT_FALSE(!lock1);
    auto lock2 = track_dir.make_lock("abc");
    ASSERT_FALSE(!lock2);

    ASSERT_TRUE(lock1->lock());
    ASSERT_FALSE(lock2->lock());
    ASSERT_TRUE(lock1->unlock());
    ASSERT_TRUE(lock2->lock());
  }

  // test create
  {
    irs::memory_directory dir;
    irs::TrackingDirectory track_dir{dir};
    const std::string_view file{"abc"};
    {
      auto file1 = track_dir.create(file);
      ASSERT_FALSE(!file1);
      file1->write_byte(42);
      file1->flush();
    }
    ASSERT_TRUE(track_dir.sync({&file, 1}));
    uint64_t byte_size{0};
    ASSERT_TRUE(track_dir.length(byte_size, file));
    ASSERT_EQ(1, byte_size);
    bool exists{false};
    ASSERT_TRUE(dir.exists(exists, file) && exists);
    ASSERT_FALSE(track_dir.remove(file));
    ASSERT_TRUE(track_dir.rename(file, "def"));
    ASSERT_FALSE(track_dir.remove(file));
    ASSERT_TRUE(dir.exists(exists, file) && !exists);
    ASSERT_TRUE(dir.exists(exists, "def") && exists);
    ASSERT_FALSE(track_dir.remove("def"));
    ASSERT_TRUE(dir.exists(exists, "def") && exists);

    byte_size = 0;
    auto files = track_dir.FlushTracked(byte_size);
    ASSERT_EQ(1, files.size());
    ASSERT_EQ("def", files.front());
    ASSERT_EQ(1, byte_size);

    // Tracked files are cleared
    files = track_dir.FlushTracked(byte_size);
    ASSERT_EQ(0, files.size());
    ASSERT_EQ(0, byte_size);
  }
}
