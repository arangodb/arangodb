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

#include "formats/formats_10.hpp"
#include "index/doc_generator.hpp"
#include "index/index_tests.hpp"
#include "index/index_writer.hpp"
#include "search/term_filter.hpp"
#include "store/directory_cleaner.hpp"
#include "store/memory_directory.hpp"
#include "tests_shared.hpp"
#include "utils/directory_utils.hpp"

using namespace std::chrono_literals;

namespace {

using namespace irs;

auto MakeByTerm(std::string_view name, std::string_view value) {
  auto filter = std::make_unique<irs::by_term>();
  *filter->mutable_field() = name;
  filter->mutable_options()->term = irs::ViewCast<irs::byte_type>(value);
  return filter;
}

template<typename Visitor>
bool VisitFiles(const IndexMeta& meta, Visitor&& visitor) {
  for (auto& curr_segment : meta.segments) {
    if (!visitor(curr_segment.filename)) {
      return false;
    }

    for (auto& file : curr_segment.meta.files) {
      if (!visitor(file)) {
        return false;
      }
    }
  }
  return true;
}

directory_cleaner::removal_acceptor_t remove_except_current_segments(
  const directory& dir, const format& codec) {
  const auto acceptor = [](std::string_view filename,
                           const absl::flat_hash_set<std::string>& retain) {
    return !retain.contains(filename);
  };

  IndexMeta meta;
  auto reader = codec.get_index_meta_reader();

  std::string segment_file;
  const bool index_exists = reader->last_segments_file(dir, segment_file);

  if (!index_exists) {
    // can't find segments file
    return [](std::string_view) -> bool { return true; };
  }

  reader->read(dir, meta, segment_file);

  absl::flat_hash_set<std::string> retain;

  VisitFiles(meta, [&retain](std::string_view file) {
    retain.emplace(file);
    return true;
  });

  retain.emplace(std::move(segment_file));

  return std::bind(acceptor, std::placeholders::_1, std::move(retain));
}

}  // namespace

TEST(directory_cleaner_tests, test_directory_cleaner) {
  irs::memory_directory dir;

  // add a dummy files
  {
    irs::index_output::ptr tmp;
    tmp = dir.create("dummy.file.1");
    ASSERT_FALSE(!tmp);
    tmp = dir.create("dummy.file.2");
    ASSERT_FALSE(!tmp);
  }

  // test clean before initializing directory
  { ASSERT_EQ(0, irs::directory_cleaner::clean(dir)); }

  // add a more dummy files
  {
    irs::index_output::ptr tmp;
    tmp = dir.create("dummy.file.3");
    ASSERT_FALSE(!tmp);
    tmp = dir.create("dummy.file.4");
    ASSERT_FALSE(!tmp);
  }

  auto& refs = dir.attributes().refs();

  // add tracked files
  auto ref1 = refs.add("tracked.file.1");
  auto ref2 = refs.add("tracked.file.2");
  {
    irs::index_output::ptr tmp;
    tmp = dir.create("tracked.file.1");
    ASSERT_FALSE(!tmp);
    tmp = dir.create("tracked.file.2");
    ASSERT_FALSE(!tmp);
  }

  // test initial directory state
  {
    std::unordered_set<std::string> expected = {
      "dummy.file.1", "dummy.file.2",   "dummy.file.3",
      "dummy.file.4", "tracked.file.1", "tracked.file.2"};
    std::vector<std::string> files;
    auto list_files = [&files](std::string_view name) {
      files.emplace_back(std::move(name));
      return true;
    };
    ASSERT_TRUE(dir.visit(list_files));

    for (auto& file : files) {
      ASSERT_EQ(1, expected.erase(file));
    }

    ASSERT_TRUE(expected.empty());
  }

  // test clean without any changes (refs still active)
  {
    std::unordered_set<std::string> expected = {
      "dummy.file.1", "dummy.file.2",   "dummy.file.3",
      "dummy.file.4", "tracked.file.1", "tracked.file.2"};
    std::vector<std::string> files;
    auto list_files = [&files](std::string_view name) {
      files.emplace_back(std::move(name));
      return true;
    };
    ASSERT_EQ(0, irs::directory_cleaner::clean(dir));
    ASSERT_TRUE(dir.visit(list_files));

    for (auto& file : files) {
      ASSERT_EQ(1, expected.erase(file));
    }

    ASSERT_TRUE(expected.empty());
  }

  // test clean without any changes (due to 'keep')
  {
    std::unordered_set<std::string_view> retain = {"tracked.file.1",
                                                   "tracked.file.2"};
    auto acceptor = [&retain](std::string_view filename) -> bool {
      return retain.find(filename) == retain.end();
    };
    std::unordered_set<std::string_view> expected = {
      "dummy.file.1", "dummy.file.2",   "dummy.file.3",
      "dummy.file.4", "tracked.file.1", "tracked.file.2"};
    std::vector<std::string> files;
    auto list_files = [&files](std::string_view name) {
      files.emplace_back(std::move(name));
      return true;
    };
    ref2.reset();
    ASSERT_EQ(0, irs::directory_cleaner::clean(dir, acceptor));
    ASSERT_TRUE(dir.visit(list_files));

    for (auto& file : files) {
      ASSERT_EQ(1, expected.erase(file));
    }

    ASSERT_TRUE(expected.empty());
  }

  // test clean removing tracked files without references (no 'tracked.file.2')
  {
    std::unordered_set<std::string> expected = {"dummy.file.1", "dummy.file.2",
                                                "dummy.file.3", "dummy.file.4",
                                                "tracked.file.1"};
    std::vector<std::string> files;
    auto list_files = [&files](std::string_view name) {
      files.emplace_back(std::move(name));
      return true;
    };
    ASSERT_EQ(1, irs::directory_cleaner::clean(dir));
    ASSERT_TRUE(dir.visit(list_files));

    for (auto& file : files) {
      ASSERT_EQ(1, expected.erase(file));
    }

    ASSERT_TRUE(expected.empty());
  }

  // test clean removing tracked files without references (no 'tracked.file.1')
  {
    std::unordered_set<std::string_view> retain = {
      "dummy.file.1", "dummy.file.2", "dummy.file.3", "dummy.file.4"};
    auto acceptor = [&retain](std::string_view filename) -> bool {
      return retain.find(filename) == retain.end();
    };
    std::unordered_set<std::string_view> expected = {
      "dummy.file.1", "dummy.file.2", "dummy.file.3", "dummy.file.4"};
    std::vector<std::string> files;
    auto list_files = [&files](std::string_view name) {
      files.emplace_back(std::move(name));
      return true;
    };
    ref1.reset();
    ASSERT_EQ(1, irs::directory_cleaner::clean(dir, acceptor));
    ASSERT_TRUE(dir.visit(list_files));

    for (auto& file : files) {
      ASSERT_EQ(1, expected.erase(file));
    }

    ASSERT_TRUE(expected.empty());
  }

  // test empty references removed too
  ASSERT_TRUE(refs.refs().empty());
}

TEST(directory_cleaner_tests, test_directory_cleaner_current_segment) {
  tests::json_doc_generator gen(test_base::resource("simple_sequential.json"),
                                &tests::generic_json_field_factory);
  tests::document const* doc1 = gen.next();
  tests::document const* doc2 = gen.next();
  filter::ptr query_doc1 = MakeByTerm("name", "A");
  irs::memory_directory dir;
  auto codec_ptr = irs::formats::get("1_0");
  ASSERT_NE(nullptr, codec_ptr);

  // writer commit tracks files that are in active segments
  {
    auto writer = irs::IndexWriter::Make(dir, codec_ptr, irs::OM_CREATE);

    ASSERT_TRUE(insert(*writer, doc1->indexed.begin(), doc1->indexed.end(),
                       doc1->stored.begin(), doc1->stored.end()));
    writer->Commit();
    tests::AssertSnapshotEquality(writer->GetSnapshot(),
                                  irs::DirectoryReader(dir, codec_ptr));

    std::vector<std::string> files;
    auto list_files = [&files](std::string_view name) {
      files.emplace_back(std::move(name));
      return true;
    };
    std::unordered_set<std::string> file_set;
    ASSERT_TRUE(dir.visit(list_files));
    ASSERT_FALSE(files.empty());
    file_set.insert(files.begin(), files.end());

    writer->GetBatch().Remove(std::move(query_doc1));
    ASSERT_TRUE(insert(*writer, doc2->indexed.begin(), doc2->indexed.end(),
                       doc2->stored.begin(), doc2->stored.end()));
    writer->Commit();
    tests::AssertSnapshotEquality(writer->GetSnapshot(),
                                  irs::DirectoryReader(dir, codec_ptr));

    irs::directory_cleaner::clean(
      dir, remove_except_current_segments(dir, *codec_ptr));
    files.clear();
    ASSERT_TRUE(dir.visit(list_files));
    ASSERT_FALSE(files.empty());

    // new list should not overlap due to first segment having been removed
    for (auto& file : files) {
      ASSERT_TRUE(file_set.find(file) == file_set.end());
    }
  }

  std::unordered_set<std::string> file_set;

  // remember files used for first/single segment
  {
    std::string segments_file;

    irs::IndexMeta index_meta;
    auto meta_reader = codec_ptr->get_index_meta_reader();
    const auto index_exists =
      meta_reader->last_segments_file(dir, segments_file);

    ASSERT_TRUE(index_exists);
    meta_reader->read(dir, index_meta, segments_file);

    file_set.insert(segments_file);

    VisitFiles(index_meta, [&file_set](std::string_view file) {
      file_set.emplace(file);
      return true;
    });
  }

  // no active refs keeps files from latest segments
  {
    std::vector<std::string> files;
    auto list_files = [&files](std::string_view name) {
      files.emplace_back(std::move(name));
      return true;
    };
    std::unordered_set<std::string> current_files(file_set);
    irs::directory_cleaner::clean(
      dir, remove_except_current_segments(dir, *codec_ptr));
    ASSERT_TRUE(dir.visit(list_files));
    ASSERT_FALSE(files.empty());

    // new list should be exactly the files listed in index_meta
    for (auto& file : files) {
      ASSERT_EQ(1, current_files.erase(file));
    }

    ASSERT_TRUE(current_files.empty());
  }

  // remember files used for first/single segment
  {
    std::string segments_file;

    irs::IndexMeta index_meta;
    auto meta_reader = codec_ptr->get_index_meta_reader();
    const auto index_exists =
      meta_reader->last_segments_file(dir, segments_file);

    ASSERT_TRUE(index_exists);
    meta_reader->read(dir, index_meta, segments_file);

    file_set.insert(segments_file);

    VisitFiles(index_meta, [&file_set](std::string_view file) {
      file_set.emplace(file);
      return true;
    });
  }

  // active reader refs keeps files referenced by reader
  {
    std::vector<std::string> files;
    auto list_files = [&files](std::string_view name) {
      files.emplace_back(std::move(name));
      return true;
    };
    std::unordered_set<std::string> current_files(file_set);
    auto reader = irs::DirectoryReader{dir, codec_ptr};
    irs::directory_cleaner::clean(dir);
    ASSERT_TRUE(dir.visit(list_files));
    ASSERT_FALSE(files.empty());

    // new list should be exactly the files listed in index_meta
    for (auto& file : files) {
      ASSERT_EQ(1, file_set.erase(file));
    }

    ASSERT_TRUE(file_set.empty());
  }

  // no refs and no current segment removes all files
  {
    std::vector<std::string> files;
    auto list_files = [&files](std::string_view name) {
      files.emplace_back(std::move(name));
      return true;
    };
    irs::directory_cleaner::clean(dir);
    ASSERT_TRUE(dir.visit(list_files));
    ASSERT_TRUE(files.empty());  // current segment should have been removed too
  }
}
