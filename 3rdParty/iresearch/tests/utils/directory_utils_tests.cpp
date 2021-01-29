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

#include "tests_shared.hpp"
#include "index/index_meta.hpp"
#include "store/memory_directory.hpp"
#include "utils/directory_utils.hpp"

#include "index/index_tests.hpp"

namespace {

class directory_utils_tests: public ::testing::Test {
  virtual void SetUp() { }

  virtual void TearDown() { }

 protected:
  class directory_mock: public irs::directory {
   public:
    directory_mock() noexcept { }

    using directory::attributes;

    virtual irs::attribute_store& attributes() noexcept override {
      return attrs_;
    }

    virtual irs::index_output::ptr create(
      const std::string& name
    ) noexcept override {
      return nullptr;
    }

    virtual bool exists(
      bool& result, const std::string& name
    ) const noexcept override {
      return false;
    }

    virtual bool length(
      uint64_t& result, const std::string& name
    ) const noexcept override {
      return false;
    }

    virtual irs::index_lock::ptr make_lock(
      const std::string& name
    ) noexcept override {
      return nullptr;
    }

    virtual bool mtime(
      std::time_t& result, const std::string& name
    ) const noexcept override {
      return false;
    }

    virtual irs::index_input::ptr open(
      const std::string& name,
      irs::IOAdvice advice
    ) const noexcept override {
      return nullptr;
    }

    virtual bool remove(const std::string& name) noexcept override {
      return false;
    }

    virtual bool rename(
      const std::string& src, const std::string& dst
    ) noexcept override {
      return false;
    }

    virtual bool sync(const std::string& name) noexcept override {
      return false;
    }

    virtual bool visit(const irs::directory::visitor_f& visitor) const override {
      return false;
    }

   private:
    irs::attribute_store attrs_;
  }; // directory_mock
};

}

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

TEST_F(directory_utils_tests, ensure_get_allocator) {
  directory_mock dir;
  ASSERT_FALSE(dir.attributes().get<irs::memory_allocator>());
  ASSERT_EQ(&irs::memory_allocator::global(), &irs::directory_utils::get_allocator(dir));

  // size == 0 -> use global allocator
  ASSERT_EQ(&irs::memory_allocator::global(), &irs::directory_utils::ensure_allocator(dir, 0));
  ASSERT_FALSE(dir.attributes().get<irs::memory_allocator>());
  ASSERT_EQ(&irs::memory_allocator::global(), &irs::directory_utils::get_allocator(dir));

  // size != 0 -> create allocator
  auto& alloc = irs::directory_utils::ensure_allocator(dir, 42);
  auto* alloc_attr = dir.attributes().get<irs::memory_allocator>();
  ASSERT_NE(nullptr, alloc_attr);
  ASSERT_NE(nullptr, *alloc_attr);
  ASSERT_EQ(&alloc, alloc_attr->get());
  ASSERT_EQ(&alloc, &irs::directory_utils::get_allocator(dir));
}

TEST_F(directory_utils_tests, test_reference) {
  // test clear refs (all free)
  {
    irs::memory_directory dir;

    ASSERT_TRUE(irs::directory_utils::reference(dir, "abc", true).operator bool());

    auto& attribute = const_cast<const irs::attribute_store&>(dir.attributes()).get<irs::index_file_refs>();

    ASSERT_TRUE(attribute.operator bool());
    ASSERT_FALSE(attribute->refs().empty());
    ASSERT_TRUE(attribute->refs().contains("abc"));

    attribute->clear();
    ASSERT_TRUE(attribute->refs().empty());
  }

  // test clear refs (in-use)
  {
    irs::memory_directory dir;
    auto ref = irs::directory_utils::reference(dir, "abc", true);

    ASSERT_TRUE(ref.operator bool());

    auto& attribute = const_cast<const irs::attribute_store&>(dir.attributes()).get<irs::index_file_refs>();

    ASSERT_TRUE(attribute.operator bool());
    ASSERT_FALSE(attribute->refs().empty());
    ASSERT_TRUE(attribute->refs().contains("abc"));
    ASSERT_THROW(attribute->clear(), irs::illegal_state);
  }

  // test add file (no missing)
  {
    irs::memory_directory dir;

    ASSERT_FALSE(irs::directory_utils::reference(dir, "abc").operator bool());

    auto& attribute = const_cast<const irs::attribute_store&>(dir.attributes()).get<irs::index_file_refs>();

    ASSERT_FALSE(attribute.operator bool());
  }

  // test add file (with missing)
  {
    irs::memory_directory dir;

    ASSERT_TRUE(irs::directory_utils::reference(dir, "abc", true).operator bool());

    auto& attribute = const_cast<const irs::attribute_store&>(dir.attributes()).get<irs::index_file_refs>();

    ASSERT_TRUE(attribute.operator bool());
    ASSERT_FALSE(attribute->refs().empty());
    ASSERT_TRUE(attribute->refs().contains("abc"));
    ASSERT_FALSE(attribute->refs().contains("def"));
  }

  // test add file
  {
    irs::memory_directory dir;
    auto file = dir.create("abc");

    ASSERT_FALSE(!file);
    ASSERT_TRUE(irs::directory_utils::reference(dir, "abc").operator bool());

    auto& attribute = const_cast<const irs::attribute_store&>(dir.attributes()).get<irs::index_file_refs>();

    ASSERT_TRUE(attribute.operator bool());
    ASSERT_FALSE(attribute->refs().empty());
    ASSERT_TRUE(attribute->refs().contains("abc"));
    ASSERT_FALSE(attribute->refs().contains("def"));
  }

  // test add from source
  {
    irs::memory_directory dir;
    std::unordered_set<std::string> files;
    auto file = dir.create("abc");
    ASSERT_FALSE(!file);
    size_t count = 0;
    auto visitor = [&count](irs::index_file_refs::ref_t&& ref)->bool { ++count; return true; };

    files.emplace("abc");
    files.emplace("def");

    auto begin = files.begin();
    auto end = files.end();
    auto source = [&begin, &end]()->const std::string* { 
      if (begin == end) {
        return nullptr;
      }
      return &*(begin++);
    };

    ASSERT_TRUE(irs::directory_utils::reference(dir, source, visitor));
    ASSERT_EQ(1, count);

    auto& attribute = const_cast<const irs::attribute_store&>(dir.attributes()).get<irs::index_file_refs>();

    ASSERT_TRUE(attribute.operator bool());
    ASSERT_FALSE(attribute->refs().empty());
    ASSERT_TRUE(attribute->refs().contains("abc"));
    ASSERT_FALSE(attribute->refs().contains("def"));
  }

  // test add from source (with missing)
  {
    irs::memory_directory dir;
    std::unordered_set<std::string> files;
    auto file = dir.create("abc");
    ASSERT_FALSE(!file);
    size_t count = 0;
    auto visitor = [&count](irs::index_file_refs::ref_t&& ref)->bool { ++count; return true; };

    files.emplace("abc");
    files.emplace("def");

    auto begin = files.begin();
    auto end = files.end();
    auto source = [&begin, &end]() -> const std::string* { 
      if (begin == end) {
        return nullptr;
      }
      
      return &*(begin++);
    };

    ASSERT_TRUE(irs::directory_utils::reference(dir, source, visitor, true));
    ASSERT_EQ(2, count);

    auto& attribute = const_cast<const irs::attribute_store&>(dir.attributes()).get<irs::index_file_refs>();

    ASSERT_TRUE(attribute.operator bool());
    ASSERT_FALSE(attribute->refs().empty());
    ASSERT_TRUE(attribute->refs().contains("abc"));
    ASSERT_TRUE(attribute->refs().contains("def"));
  }

  // test add from source visitor terminate
  {
    irs::memory_directory dir;
    std::unordered_set<std::string> files;
    auto file = dir.create("abc");
    ASSERT_FALSE(!file);
    size_t count = 0;
    auto visitor = [&count](irs::index_file_refs::ref_t&& ref)->bool { ++count; return false; };
    std::unordered_set<std::string> expected_one_of = {
      "abc", "def",
    };

    files.emplace("abc");
    files.emplace("def");

    auto begin = files.begin();
    auto end = files.end();
    auto source = [&begin, &end]()->const std::string*{ 
      if (begin == end) {
        return nullptr;
      }
      return &*(begin++);
    };

    ASSERT_FALSE(irs::directory_utils::reference(dir, source, visitor, true));
    ASSERT_EQ(1, count);

    auto& attribute = const_cast<const irs::attribute_store&>(dir.attributes()).get<irs::index_file_refs>();

    ASSERT_TRUE(attribute.operator bool());
    ASSERT_FALSE(attribute->refs().empty());

    for (auto& file: expected_one_of) {
      if (attribute->refs().contains(file)) {
        expected_one_of.erase(file);
        break;
      }
    }

    ASSERT_EQ(1, expected_one_of.size());

    for (auto& file: expected_one_of) {
      ASSERT_FALSE(attribute->refs().contains(file));
    }
  }

  // test add segment_meta files (empty)
  {
    irs::memory_directory dir;
    irs::segment_meta meta;
    size_t count = 0;
    auto visitor = [&count](irs::index_file_refs::ref_t&& ref)->bool { ++count; return true; };

    ASSERT_TRUE(irs::directory_utils::reference(dir, meta, visitor, true));
    ASSERT_EQ(0, count);

    auto& attribute = const_cast<const irs::attribute_store&>(dir.attributes()).get<irs::index_file_refs>();

    ASSERT_FALSE(attribute.operator bool());
  }

  // test add segment_meta files
  {
    irs::memory_directory dir;
    irs::segment_meta meta;
    auto file = dir.create("abc");
    ASSERT_FALSE(!file);
    size_t count = 0;
    auto visitor = [&count](irs::index_file_refs::ref_t&& ref)->bool { ++count; return true; };

    meta.files.emplace("abc");
    meta.files.emplace("def");

    ASSERT_TRUE(irs::directory_utils::reference(dir, meta, visitor));
    ASSERT_EQ(1, count);

    auto& attribute = const_cast<const irs::attribute_store&>(dir.attributes()).get<irs::index_file_refs>();

    ASSERT_TRUE(attribute.operator bool());
    ASSERT_FALSE(attribute->refs().empty());
    ASSERT_TRUE(attribute->refs().contains("abc"));
    ASSERT_FALSE(attribute->refs().contains("def"));
  }

  // test add segment_meta files (with missing)
  {
    irs::memory_directory dir;
    irs::segment_meta meta;
    auto file = dir.create("abc");
    ASSERT_FALSE(!file);
    size_t count = 0;
    auto visitor = [&count](irs::index_file_refs::ref_t&& ref)->bool { ++count; return true; };

    meta.files.emplace("abc");
    meta.files.emplace("def");

    ASSERT_TRUE(irs::directory_utils::reference(dir, meta, visitor, true));
    ASSERT_EQ(2, count);

    auto& attribute = const_cast<const irs::attribute_store&>(dir.attributes()).get<irs::index_file_refs>();

    ASSERT_TRUE(attribute.operator bool());
    ASSERT_FALSE(attribute->refs().empty());
    ASSERT_TRUE(attribute->refs().contains("abc"));
    ASSERT_TRUE(attribute->refs().contains("def"));
  }

  // test add segment_meta files visitor terminate
  {
    irs::memory_directory dir;
    irs::segment_meta meta;
    auto file = dir.create("abc");
    ASSERT_FALSE(!file);
    size_t count = 0;
    auto visitor = [&count](irs::index_file_refs::ref_t&& ref)->bool { ++count; return false; };
    std::unordered_set<std::string> expected_one_of = {
      "abc",
      "def",
    };

    meta.files.emplace("abc");
    meta.files.emplace("def");

    ASSERT_FALSE(irs::directory_utils::reference(dir, meta, visitor, true));
    ASSERT_EQ(1, count);

    auto& attribute = const_cast<const irs::attribute_store&>(dir.attributes()).get<irs::index_file_refs>();

    ASSERT_TRUE(attribute.operator bool());
    ASSERT_FALSE(attribute->refs().empty());

    for (auto& file: expected_one_of) {
      if (attribute->refs().contains(file)) {
        expected_one_of.erase(file);
        break;
      }
    }

    ASSERT_EQ(1, expected_one_of.size());

    for (auto& file: expected_one_of) {
      ASSERT_FALSE(attribute->refs().contains(file));
    }
  }

  // test add index_meta files (empty)
  {
    irs::memory_directory dir;
    irs::index_meta meta;
    irs::index_meta::index_segments_t segments;
    size_t count = 0;
    auto visitor = [&count](irs::index_file_refs::ref_t&& ref)->bool { ++count; return true; };

    ASSERT_TRUE(irs::directory_utils::reference(dir, meta, visitor));
    ASSERT_EQ(0, count);

    auto& attribute = const_cast<const irs::attribute_store&>(dir.attributes()).get<irs::index_file_refs>();

    ASSERT_FALSE(attribute.operator bool());
  }

  // test add index_meta files
  {
    irs::memory_directory dir;
    irs::index_meta meta;
    irs::index_meta::index_segments_t segments(1);
    auto file = dir.create("abc");
    ASSERT_FALSE(!file);
    size_t count = 0;
    auto visitor = [&count](irs::index_file_refs::ref_t&& ref)->bool { ++count; return true; };

    segments[0].meta.files.emplace("abc");
    segments[0].meta.files.emplace("def");
    meta.add(segments.begin(), segments.end());

    ASSERT_TRUE(irs::directory_utils::reference(dir, meta, visitor));
    ASSERT_EQ(1, count);

    auto& attribute = const_cast<const irs::attribute_store&>(dir.attributes()).get<irs::index_file_refs>();

    ASSERT_TRUE(attribute.operator bool());
    ASSERT_FALSE(attribute->refs().empty());
    ASSERT_TRUE(attribute->refs().contains("abc"));
    ASSERT_FALSE(attribute->refs().contains("def"));
  }

  // test add index_meta files (with missing)
  {
    irs::memory_directory dir;
    irs::index_meta meta;
    irs::index_meta::index_segments_t segments(1);
    auto file = dir.create("abc");
    ASSERT_FALSE(!file);
    size_t count = 0;
    auto visitor = [&count](irs::index_file_refs::ref_t&& ref)->bool { ++count; return true; };

    segments[0].meta.files.emplace("abc");
    segments[0].meta.files.emplace("def");
    meta.add(segments.begin(), segments.end());

    ASSERT_TRUE(irs::directory_utils::reference(dir, meta, visitor, true));
    ASSERT_EQ(3, count); // +1 for segment file

    auto& attribute = const_cast<const irs::attribute_store&>(dir.attributes()).get<irs::index_file_refs>();

    ASSERT_TRUE(attribute.operator bool());
    ASSERT_FALSE(attribute->refs().empty());
    ASSERT_TRUE(attribute->refs().contains("abc"));
    ASSERT_TRUE(attribute->refs().contains("def"));
  }

  // test add index_meta files visitor terminate
  {
    irs::memory_directory dir;
    irs::index_meta meta;
    irs::index_meta::index_segments_t segments(1);
    auto file = dir.create("abc");
    ASSERT_FALSE(!file);
    size_t count = 0;
    auto visitor = [&count](irs::index_file_refs::ref_t&& ref)->bool { ++count; return false; };
    std::unordered_set<std::string> expected_one_of = {
      "abc",
      "def",
      "xyz",
    };

    segments[0].filename = "xyz";
    segments[0].meta.files.emplace("abc");
    segments[0].meta.files.emplace("def");
    meta.add(segments.begin(), segments.end());

    ASSERT_FALSE(irs::directory_utils::reference(dir, meta, visitor, true));
    ASSERT_EQ(1, count);

    auto& attribute = const_cast<const irs::attribute_store&>(dir.attributes()).get<irs::index_file_refs>();

    ASSERT_TRUE(attribute.operator bool());
    ASSERT_FALSE(attribute->refs().empty());

    for (auto& file: expected_one_of) {
      if (attribute->refs().contains(file)) {
        expected_one_of.erase(file);
        break;
      }
    }

    ASSERT_EQ(2, expected_one_of.size());

    for (auto& file: expected_one_of) {
      ASSERT_FALSE(attribute->refs().contains(file));
    }
  }
}

TEST_F(directory_utils_tests, test_ref_tracking_dir) {
  // test move constructor
  {
    irs::memory_directory dir;
    irs::ref_tracking_directory track_dir1(dir);
    irs::ref_tracking_directory track_dir2(std::move(track_dir1));

    ASSERT_EQ(&dir, &(*track_dir2));
  }

  // test dereference and attributes
  {
    irs::memory_directory dir;
    irs::ref_tracking_directory track_dir(dir);

    ASSERT_EQ(&dir, &(*track_dir));
    ASSERT_EQ(&(dir.attributes()), &(track_dir.attributes()));
  }

  // test make_lock
  {
    irs::memory_directory dir;
    irs::ref_tracking_directory track_dir(dir);
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
    irs::ref_tracking_directory track_dir(dir);
    auto file1 = dir.create("abc");

    ASSERT_FALSE(!file1);
    file1->write_byte(42);
    file1->flush();

    auto file2 = track_dir.open("abc", irs::IOAdvice::NORMAL);

    ASSERT_FALSE(!file2);
    ASSERT_TRUE(track_dir.sync("abc")); // does nothing in memory_directory, but adds line coverage
    ASSERT_EQ(1, file2->length());
    ASSERT_TRUE(track_dir.rename("abc", "def"));
    file1.reset(); // release before remove
    file2.reset(); // release before remove
    ASSERT_FALSE(track_dir.remove("abc"));
    bool exists;
    ASSERT_TRUE(dir.exists(exists, "abc") && !exists);
    ASSERT_TRUE(dir.exists(exists, "def") && exists);
    ASSERT_TRUE(track_dir.remove("def"));
    ASSERT_TRUE(dir.exists(exists, "def") && !exists);
  }

  // test visit refs visitor complete
  {
    irs::memory_directory dir;
    irs::ref_tracking_directory track_dir(dir);
    auto file1 = track_dir.create("abc");
    ASSERT_FALSE(!file1);
    auto file2 = track_dir.create("def");
    ASSERT_FALSE(!file2);
    size_t count = 0;
    auto visitor = [&count](const irs::index_file_refs::ref_t& ref)->bool { ++count; return true; };

    ASSERT_TRUE(track_dir.visit_refs(visitor));
    ASSERT_EQ(2, count);
  }

  // test visit refs visitor (no-track-open)
  {
    irs::memory_directory dir;
    irs::ref_tracking_directory track_dir(dir);
    auto file1 = dir.create("abc");
    ASSERT_FALSE(!file1);
    auto file2 = track_dir.open("abc", irs::IOAdvice::NORMAL);
    ASSERT_FALSE(!file2);
    size_t count = 0;
    auto visitor = [&count](const irs::index_file_refs::ref_t& ref)->bool { ++count; return true; };

    ASSERT_TRUE(track_dir.visit_refs(visitor));
    ASSERT_EQ(0, count);
  }

  // test visit refs visitor (track-open)
  {
    irs::memory_directory dir;
    irs::ref_tracking_directory track_dir(dir, true);
    auto file1 = dir.create("abc");
    ASSERT_FALSE(!file1);
    auto file2 = track_dir.open("abc", irs::IOAdvice::NORMAL);
    ASSERT_FALSE(!file2);
    size_t count = 0;
    auto visitor = [&count](const irs::index_file_refs::ref_t& ref)->bool { ++count; return true; };

    ASSERT_TRUE(track_dir.visit_refs(visitor));
    ASSERT_EQ(1, count);
  }

  // test visit refs visitor terminate
  {
    irs::memory_directory dir;
    irs::ref_tracking_directory track_dir(dir);
    auto file1 = track_dir.create("abc");
    ASSERT_FALSE(!file1);
    auto file2 = track_dir.create("def");
    ASSERT_FALSE(!file2);
    size_t count = 0;
    auto visitor = [&count](const irs::index_file_refs::ref_t& ref)->bool { ++count; return false; };

    ASSERT_FALSE(track_dir.visit_refs(visitor));
    ASSERT_EQ(1, count);
  }

  // ...........................................................................
  // errors during file operations
  // ...........................................................................

  struct error_directory: public irs::directory {
    irs::attribute_store attrs;
    virtual irs::attribute_store& attributes() noexcept override { return attrs; }
    virtual irs::index_output::ptr create(const std::string&) noexcept override { return nullptr; }
    virtual bool exists(bool& result, const std::string&) const noexcept override { return false; }
    virtual bool length(uint64_t& result, const std::string&) const noexcept override { return false; }
    virtual bool visit(const visitor_f&) const override { return false; }
    virtual irs::index_lock::ptr make_lock(const std::string&) noexcept override { return nullptr; }
    virtual bool mtime(std::time_t& result, const std::string& name) const noexcept override { return false; }
    virtual irs::index_input::ptr open(const std::string&, irs::IOAdvice) const noexcept override { return nullptr; }
    virtual bool remove(const std::string&) noexcept override { return false; }
    virtual bool rename(const std::string&, const std::string&) noexcept override { return false; }
    virtual bool sync(const std::string& name) noexcept override { return false; }
  } error_dir;

  // test create failure
  {
    error_dir.attributes().clear();

    irs::ref_tracking_directory track_dir(error_dir);

    ASSERT_FALSE(track_dir.create("abc"));

    std::set<std::string> refs;
    auto visitor = [&refs](const irs::index_file_refs::ref_t& ref)->bool { refs.insert(*ref); return true; };

    ASSERT_TRUE(track_dir.visit_refs(visitor));
    ASSERT_TRUE(refs.empty());
  }

  // test open failure
  {
    error_dir.attributes().clear();

    irs::ref_tracking_directory track_dir(error_dir, true);

    ASSERT_FALSE(track_dir.open("abc", irs::IOAdvice::NORMAL));

    std::set<std::string> refs;
    auto visitor = [&refs](const irs::index_file_refs::ref_t& ref)->bool { refs.insert(*ref); return true; };

    ASSERT_TRUE(track_dir.visit_refs(visitor));
    ASSERT_TRUE(refs.empty());
  }
}

TEST_F(directory_utils_tests, test_tracking_dir) {
  // test dereference and attributes
  {
    irs::memory_directory dir;
    irs::tracking_directory track_dir(dir);

    ASSERT_EQ(&dir, &(*track_dir));
    ASSERT_EQ(&(dir.attributes()), &(track_dir.attributes()));
  }

  // test make_lock
  {
    irs::memory_directory dir;
    irs::tracking_directory track_dir(dir);
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
    irs::tracking_directory track_dir(dir);
    auto file1 = dir.create("abc");
    ASSERT_FALSE(!file1);

    file1->write_byte(42);
    file1->flush();

    auto file2 = track_dir.open("abc", irs::IOAdvice::NORMAL);
    ASSERT_FALSE(!file2);

    ASSERT_TRUE(track_dir.sync("abc")); // does nothing in memory_directory, but adds line coverage
    ASSERT_EQ(1, file2->length());
    ASSERT_TRUE(track_dir.rename("abc", "def"));
    file1.reset(); // release before remove
    file2.reset(); // release before remove
    ASSERT_FALSE(track_dir.remove("abc"));
    bool exists;
    ASSERT_TRUE(dir.exists(exists, "abc") && !exists);
    ASSERT_TRUE(dir.exists(exists, "def") && exists);
    ASSERT_TRUE(track_dir.remove("def"));
    ASSERT_TRUE(dir.exists(exists, "def") && !exists);
  }

  // test open (no-track-open)
  {
    irs::memory_directory dir;
    irs::tracking_directory track_dir(dir);
    auto file1 = dir.create("abc");
    ASSERT_FALSE(!file1);
    auto file2 = track_dir.open("abc", irs::IOAdvice::NORMAL);
    ASSERT_FALSE(!file2);
    irs::tracking_directory::file_set files;
    track_dir.flush_tracked(files);
    ASSERT_EQ(0, files.size());
  }

  // test open (track-open)
  {
    irs::memory_directory dir;
    irs::tracking_directory track_dir(dir, true);
    auto file1 = dir.create("abc");
    ASSERT_FALSE(!file1);
    auto file2 = track_dir.open("abc", irs::IOAdvice::NORMAL);
    ASSERT_FALSE(!file2);
    irs::tracking_directory::file_set files;
    track_dir.flush_tracked(files);
    ASSERT_EQ(1, files.size());
    track_dir.flush_tracked(files); // tracked files were cleared
    ASSERT_EQ(0, files.size());
  }
}
