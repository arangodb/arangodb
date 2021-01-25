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
#include "formats/formats_10.hpp"
#include "index/doc_generator.hpp"
#include "index/index_tests.hpp"
#include "index/index_writer.hpp"
#include "iql/query_builder.hpp"
#include "store/directory_cleaner.hpp"
#include "store/memory_directory.hpp"
#include "utils/directory_utils.hpp"

using namespace std::chrono_literals;

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

TEST(directory_cleaner_tests, test_directory_cleaner) {
  irs::memory_directory dir;

  // add a dummy files
  {
    iresearch::index_output::ptr tmp;
    tmp = dir.create("dummy.file.1");
    ASSERT_FALSE(!tmp);
    tmp = dir.create("dummy.file.2");
    ASSERT_FALSE(!tmp);
  }

  // test clean before initializing directory
  {
    ASSERT_EQ(0, iresearch::directory_cleaner::clean(dir));
  }

  // start tracking refs
  auto& refs = iresearch::directory_cleaner::init(dir);

  // add a more dummy files
  {
    iresearch::index_output::ptr tmp;
    tmp = dir.create("dummy.file.3");
    ASSERT_FALSE(!tmp);
    tmp = dir.create("dummy.file.4");
    ASSERT_FALSE(!tmp);
  }

  // add tracked files
  auto ref1 = refs.add("tracked.file.1");
  auto ref2 = refs.add("tracked.file.2");
  {
    iresearch::index_output::ptr tmp;
    tmp = dir.create("tracked.file.1");
    ASSERT_FALSE(!tmp);
    tmp = dir.create("tracked.file.2");
    ASSERT_FALSE(!tmp);
  }

  // test initial directory state
  {
    std::unordered_set<std::string> expected = {
      "dummy.file.1",
      "dummy.file.2",
      "dummy.file.3",
      "dummy.file.4",
      "tracked.file.1",
      "tracked.file.2"
    };
    std::vector<std::string> files;
    auto list_files = [&files] (std::string& name) {
      files.emplace_back(std::move(name));
      return true;
    };
    ASSERT_TRUE(dir.visit(list_files));

    for (auto& file: files) {
      ASSERT_EQ(1, expected.erase(file));
    }

    ASSERT_TRUE(expected.empty());
  }

  // test clean without any changes (refs still active)
  {
    std::unordered_set<std::string> expected = {
      "dummy.file.1",
      "dummy.file.2",
      "dummy.file.3",
      "dummy.file.4",
      "tracked.file.1",
      "tracked.file.2"
    };
    std::vector<std::string> files;
    auto list_files = [&files] (std::string& name) {
      files.emplace_back(std::move(name));
      return true;
    };
    ASSERT_EQ(0, iresearch::directory_cleaner::clean(dir));
    ASSERT_TRUE(dir.visit(list_files));

    for (auto& file: files) {
      ASSERT_EQ(1, expected.erase(file));
    }

    ASSERT_TRUE(expected.empty());
  }

  // test clean without any changes (due to 'keep')
  {
    std::unordered_set<std::string> retain = {
      "tracked.file.1",
      "tracked.file.2"
    };
    auto acceptor = [&retain] (const std::string &filename)->bool {
      return retain.find(filename) == retain.end();
    };
    std::unordered_set<std::string> expected = {
      "dummy.file.1",
      "dummy.file.2",
      "dummy.file.3",
      "dummy.file.4",
      "tracked.file.1",
      "tracked.file.2"
    };
    std::vector<std::string> files;
    auto list_files = [&files] (std::string& name) {
      files.emplace_back(std::move(name));
      return true;
    };
    ref2.reset();
    ASSERT_EQ(0, iresearch::directory_cleaner::clean(dir, acceptor));
    ASSERT_TRUE(dir.visit(list_files));

    for (auto& file: files) {
      ASSERT_EQ(1, expected.erase(file));
    }

    ASSERT_TRUE(expected.empty());
  }

  // test clean removing tracked files without references (no 'tracked.file.2')
  {
    std::unordered_set<std::string> expected = {
      "dummy.file.1",
      "dummy.file.2",
      "dummy.file.3",
      "dummy.file.4",
      "tracked.file.1"
    };
    std::vector<std::string> files;
    auto list_files = [&files] (std::string& name) {
      files.emplace_back(std::move(name));
      return true;
    };
    ASSERT_EQ(1, iresearch::directory_cleaner::clean(dir));
    ASSERT_TRUE(dir.visit(list_files));

    for (auto& file: files) {
      ASSERT_EQ(1, expected.erase(file));
    }

    ASSERT_TRUE(expected.empty());
  }

  // test clean removing tracked files without references (no 'tracked.file.1')
  {
    std::unordered_set<std::string> retain = {
      "dummy.file.1",
      "dummy.file.2",
      "dummy.file.3",
      "dummy.file.4"
    };
    auto acceptor = [&retain](const std::string& filename)->bool {
      return retain.find(filename) == retain.end();
    };
    std::unordered_set<std::string> expected = {
      "dummy.file.1",
      "dummy.file.2",
      "dummy.file.3",
      "dummy.file.4"
    };
    std::vector<std::string> files;
    auto list_files = [&files] (std::string& name) {
      files.emplace_back(std::move(name));
      return true;
    };
    ref1.reset();
    ASSERT_EQ(1, iresearch::directory_cleaner::clean(dir, acceptor));
    ASSERT_TRUE(dir.visit(list_files));

    for (auto& file: files) {
      ASSERT_EQ(1, expected.erase(file));
    }

    ASSERT_TRUE(expected.empty());
  }

  // test empty references removed too
  ASSERT_TRUE(refs.empty());
}

TEST(directory_cleaner_tests, test_directory_cleaner_current_segment) {
  tests::json_doc_generator gen(
    test_base::resource("simple_sequential.json"),
    &tests::generic_json_field_factory 
  );
  tests::document const* doc1 = gen.next();
  tests::document const* doc2 = gen.next();
  auto query_doc1 = iresearch::iql::query_builder().build("name==A", std::locale::classic());
  irs::memory_directory dir;
  auto codec_ptr = irs::formats::get("1_0");
  ASSERT_NE(nullptr, codec_ptr);

  iresearch::directory_cleaner::init(dir);

  // writer commit tracks files that are in active segments
  {
    auto writer = iresearch::index_writer::make(dir, codec_ptr, iresearch::OM_CREATE);

    ASSERT_TRUE(insert(*writer,
      doc1->indexed.begin(), doc1->indexed.end(),
      doc1->stored.begin(), doc1->stored.end()
    ));
    writer->commit();

    std::vector<std::string> files;
    auto list_files = [&files] (std::string& name) {
      files.emplace_back(std::move(name));
      return true;
    };
    std::unordered_set<std::string> file_set;
    ASSERT_TRUE(dir.visit(list_files));
    ASSERT_FALSE(files.empty());
    file_set.insert(files.begin(), files.end());

    writer->documents().remove(std::move(query_doc1.filter));
    ASSERT_TRUE(insert(*writer,
      doc2->indexed.begin(), doc2->indexed.end(),
      doc2->stored.begin(), doc2->stored.end()
    ));
    writer->commit();

    iresearch::directory_cleaner::clean(dir, iresearch::directory_utils::remove_except_current_segments(dir, *codec_ptr));
    files.clear();
    ASSERT_TRUE(dir.visit(list_files));
    ASSERT_FALSE(files.empty());

    // new list should not overlap due to first segment having been removed
    for (auto& file: files) {
      ASSERT_TRUE(file_set.find(file) == file_set.end());
    }
  }

  std::unordered_set<std::string> file_set;

  // remember files used for first/single segment
  {
    std::string segments_file;

    iresearch::index_meta index_meta;
    auto meta_reader = codec_ptr->get_index_meta_reader();
    const auto index_exists = meta_reader->last_segments_file(dir, segments_file);

    ASSERT_TRUE(index_exists);
    meta_reader->read(dir, index_meta, segments_file);

    file_set.insert(segments_file);

    index_meta.visit_files([&file_set] (std::string& file) {
      file_set.emplace(std::move(file));
      return true;
    });
  }

  // no active refs keeps files from latest segments
  {
    std::vector<std::string> files;
    auto list_files = [&files] (std::string& name) {
      files.emplace_back(std::move(name));
      return true;
    };
    std::unordered_set<std::string> current_files(file_set);
    iresearch::directory_cleaner::clean(dir, iresearch::directory_utils::remove_except_current_segments(dir, *codec_ptr));
    ASSERT_TRUE(dir.visit(list_files));
    ASSERT_FALSE(files.empty());

    // new list should be exactly the files listed in index_meta
    for (auto& file: files) {
      ASSERT_EQ(1, current_files.erase(file));
    }

    ASSERT_TRUE(current_files.empty());
  }

  // active reader refs keeps files referenced by reader
  {
    std::vector<std::string> files;
    auto list_files = [&files] (std::string& name) {
      files.emplace_back(std::move(name));
      return true;
    };
    std::unordered_set<std::string> current_files(file_set);
    auto reader = iresearch::directory_reader::open(dir, codec_ptr);
    iresearch::directory_cleaner::clean(dir);
    ASSERT_TRUE(dir.visit(list_files));
    ASSERT_FALSE(files.empty());

    // new list should be exactly the files listed in index_meta
    for (auto& file: files) {
      ASSERT_EQ(1, file_set.erase(file));
    }

    ASSERT_TRUE(file_set.empty());
  }

  // no refs and no current segment removes all files
  {
    std::vector<std::string> files;
    auto list_files = [&files] (std::string& name) {
      files.emplace_back(std::move(name));
      return true;
    };
    iresearch::directory_cleaner::clean(dir);
    ASSERT_TRUE(dir.visit(list_files));
    ASSERT_TRUE(files.empty()); // current segment should have been removed too
  }
}
