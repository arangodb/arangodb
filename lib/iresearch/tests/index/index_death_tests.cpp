////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2018 ArangoDB GmbH, Cologne, Germany
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

#include "formats/formats.hpp"
#include "index_tests.hpp"
#include "search/term_filter.hpp"
#include "store/memory_directory.hpp"
#include "tests_shared.hpp"
#include "utils/index_utils.hpp"

namespace {

auto MakeByTerm(std::string_view name, std::string_view value) {
  auto filter = std::make_unique<irs::by_term>();
  *filter->mutable_field() = name;
  filter->mutable_options()->term = irs::ViewCast<irs::byte_type>(value);
  return filter;
}

class failing_directory : public tests::directory_mock {
 public:
  enum class Failure : size_t {
    CREATE = 0,
    EXISTS,
    LENGTH,
    MAKE_LOCK,
    MTIME,
    OPEN,
    RENAME,
    REMOVE,
    SYNC,
    REOPEN,
    REOPEN_NULL,  // return nullptr from index_input::reopen
    DUP,
    DUP_NULL  // return nullptr from index_input::dup
  };

 private:
  class failing_index_input : public irs::index_input {
   public:
    explicit failing_index_input(index_input::ptr&& impl, std::string_view name,
                                 const failing_directory& dir)
      : impl_(std::move(impl)), dir_(&dir), name_(name) {}
    const irs::byte_type* read_buffer(size_t offset, size_t size,
                                      irs::BufferHint hint) final {
      return impl_->read_buffer(offset, size, hint);
    }
    const irs::byte_type* read_buffer(size_t size, irs::BufferHint hint) final {
      return impl_->read_buffer(size, hint);
    }
    irs::byte_type read_byte() final { return impl_->read_byte(); }
    size_t read_bytes(irs::byte_type* b, size_t count) final {
      return impl_->read_bytes(b, count);
    }
    size_t read_bytes(size_t offset, irs::byte_type* b, size_t count) final {
      return impl_->read_bytes(offset, b, count);
    }
    size_t file_pointer() const final { return impl_->file_pointer(); }
    size_t length() const final { return impl_->length(); }
    bool eof() const final { return impl_->eof(); }
    ptr dup() const final {
      if (dir_->should_fail(Failure::DUP, name_)) {
        throw irs::io_error();
      }

      if (dir_->should_fail(Failure::DUP_NULL, name_)) {
        return nullptr;
      }

      return std::make_unique<failing_index_input>(impl_->dup(), this->name_,
                                                   *this->dir_);
    }
    ptr reopen() const final {
      if (dir_->should_fail(Failure::REOPEN, name_)) {
        throw irs::io_error();
      }

      if (dir_->should_fail(Failure::REOPEN_NULL, name_)) {
        return nullptr;
      }

      return std::make_unique<failing_index_input>(impl_->reopen(), this->name_,
                                                   *this->dir_);
    }
    void seek(size_t pos) final { impl_->seek(pos); }
    int64_t checksum(size_t offset) const final {
      return impl_->checksum(offset);
    }

   private:
    index_input::ptr impl_;
    const failing_directory* dir_;
    std::string name_;
  };

 public:
  explicit failing_directory(irs::directory& impl) noexcept
    : tests::directory_mock(impl) {}

  bool register_failure(Failure type, const std::string& name) {
    return failures_.emplace(name, type).second;
  }

  void clear_failures() noexcept { failures_.clear(); }

  size_t num_failures() const noexcept { return failures_.size(); }

  bool no_failures() const noexcept { return failures_.empty(); }

  irs::index_output::ptr create(std::string_view name) noexcept final {
    if (should_fail(Failure::CREATE, name)) {
      return nullptr;
    }

    return tests::directory_mock::create(name);
  }
  bool exists(bool& result, std::string_view name) const noexcept final {
    if (should_fail(Failure::EXISTS, name)) {
      return false;
    }

    return tests::directory_mock::exists(result, name);
  }
  bool length(uint64_t& result, std::string_view name) const noexcept final {
    if (should_fail(Failure::LENGTH, name)) {
      return false;
    }

    return tests::directory_mock::length(result, name);
  }
  irs::index_lock::ptr make_lock(std::string_view name) noexcept final {
    if (should_fail(Failure::MAKE_LOCK, name)) {
      return nullptr;
    }

    return tests::directory_mock::make_lock(name);
  }
  bool mtime(std::time_t& result, std::string_view name) const noexcept final {
    if (should_fail(Failure::MTIME, name)) {
      return false;
    }

    return tests::directory_mock::mtime(result, name);
  }
  irs::index_input::ptr open(std::string_view name,
                             irs::IOAdvice advice) const noexcept final {
    if (should_fail(Failure::OPEN, name)) {
      return nullptr;
    }

    return std::make_unique<failing_index_input>(
      tests::directory_mock::open(name, advice), name, *this);
  }
  bool remove(std::string_view name) noexcept final {
    if (should_fail(Failure::REMOVE, name)) {
      return false;
    }

    return tests::directory_mock::remove(name);
  }
  bool rename(std::string_view src, std::string_view dst) noexcept final {
    if (should_fail(Failure::RENAME, src)) {
      return false;
    }

    return tests::directory_mock::rename(src, dst);
  }
  bool sync(std::span<const std::string_view> files) noexcept final {
    return std::all_of(std::begin(files), std::end(files),
                       [this](std::string_view name) mutable noexcept {
                         if (should_fail(Failure::SYNC, name)) {
                           return false;
                         }

                         return tests::directory_mock::sync({&name, 1});
                       });
  }

 private:
  bool should_fail(Failure type, std::string_view name) const {
    auto it = failures_.find(std::make_pair(std::string{name}, type));

    if (failures_.end() != it) {
      failures_.erase(it);
      return true;
    }

    return false;
  }

  typedef std::pair<std::string, Failure> fail_t;

  struct fail_less {
    bool operator()(const fail_t& lhs, const fail_t& rhs) const noexcept {
      if (lhs.second == rhs.second) {
        return lhs.first < rhs.first;
      }

      return lhs.second < rhs.second;
    }
  };

  mutable std::set<fail_t, fail_less> failures_;
};

irs::FeatureInfoProvider default_feature_info() {
  return [](irs::type_info::type_id) {
    return std::make_pair(
      irs::ColumnInfo{.compression = irs::type<irs::compression::none>::get(),
                      .options = {},
                      .encryption = true,
                      .track_prev_doc = false},
      irs::FeatureWriterFactory{});
  };
}

void open_reader(
  std::string_view format,
  std::function<void(failing_directory& dir)> failure_registerer) {
  constexpr irs::IndexFeatures all_features =
    irs::IndexFeatures::FREQ | irs::IndexFeatures::POS |
    irs::IndexFeatures::OFFS | irs::IndexFeatures::PAY;

  tests::json_doc_generator gen(test_base::resource("simple_sequential.json"),
                                &tests::payloaded_json_field_factory);
  const auto* doc1 = gen.next();
  const auto* doc2 = gen.next();
  auto query_doc2 = MakeByTerm("name", "B");

  auto codec = irs::formats::get(format);
  ASSERT_NE(nullptr, codec);

  // create source segment
  irs::memory_directory impl;
  failing_directory dir(impl);

  // write index
  {
    auto writer = irs::IndexWriter::Make(dir, codec, irs::OM_CREATE);
    ASSERT_NE(nullptr, writer);

    ASSERT_TRUE(insert(*writer, doc1->indexed.begin(), doc1->indexed.end(),
                       doc1->stored.begin(), doc1->stored.end()));

    ASSERT_TRUE(insert(*writer, doc2->indexed.begin(), doc2->indexed.end(),
                       doc2->stored.begin(), doc2->stored.end()));

    writer->GetBatch().Remove(*query_doc2);

    ASSERT_TRUE(writer->Commit());
    tests::AssertSnapshotEquality(writer->GetSnapshot(),
                                  irs::DirectoryReader(dir));
  }

  failure_registerer(dir);

  while (!dir.no_failures()) {
    ASSERT_THROW(irs::DirectoryReader{dir}, irs::io_error);
  }

  // check data
  auto reader = irs::DirectoryReader(dir);
  ASSERT_TRUE(reader);
  ASSERT_EQ(1, reader->size());
  ASSERT_EQ(2, reader->docs_count());
  ASSERT_EQ(1, reader->live_docs_count());

  // validate index
  tests::index_t expected_index;
  expected_index.emplace_back(::default_feature_info());
  expected_index.back().insert(*doc1);
  expected_index.back().insert(*doc2);
  tests::assert_index(reader.GetImpl(), expected_index, all_features);

  // validate columnstore
  auto& segment = reader[0];  // assume 0 is id of first/only segment
  const auto* column = segment.column("name");
  ASSERT_NE(nullptr, column);
  auto values = column->iterator(irs::ColumnHint::kNormal);
  ASSERT_NE(nullptr, values);
  auto* actual_value = irs::get<irs::payload>(*values);
  ASSERT_NE(nullptr, actual_value);
  ASSERT_EQ(2, segment.docs_count());       // total count of documents
  ASSERT_EQ(1, segment.live_docs_count());  // total count of documents
  auto terms = segment.field("same");
  ASSERT_NE(nullptr, terms);
  auto termItr = terms->iterator(irs::SeekMode::NORMAL);
  ASSERT_TRUE(termItr->next());
  auto docsItr = termItr->postings(irs::IndexFeatures::NONE);
  ASSERT_TRUE(docsItr->next());
  ASSERT_EQ(docsItr->value(), values->seek(docsItr->value()));
  ASSERT_EQ("A", irs::to_string<std::string_view>(
                   actual_value->value.data()));  // 'name' value in doc3
  ASSERT_TRUE(docsItr->next());
  ASSERT_EQ(docsItr->value(), values->seek(docsItr->value()));
  ASSERT_EQ("B", irs::to_string<std::string_view>(
                   actual_value->value.data()));  // 'name' value in doc3
  ASSERT_FALSE(docsItr->next());

  // validate live docs
  auto live_docs = segment.docs_iterator();
  ASSERT_TRUE(live_docs->next());
  ASSERT_EQ(1, live_docs->value());
  ASSERT_FALSE(live_docs->next());
  ASSERT_EQ(irs::doc_limits::eof(), live_docs->value());
}

}  // namespace

TEST(index_death_test_formats_10, index_meta_write_fail_1st_phase) {
  tests::json_doc_generator gen(
    test_base::resource("simple_sequential.json"),
    [](tests::document& doc, const std::string& name,
       const tests::json_doc_generator::json_value& data) {
      if (data.is_string()) {
        doc.insert(std::make_shared<tests::string_field>(name, data.str));
      }
    });
  const auto* doc1 = gen.next();

  auto codec = irs::formats::get("1_0");
  ASSERT_NE(nullptr, codec);

  {
    irs::memory_directory impl;
    failing_directory dir(impl);
    dir.register_failure(
      failing_directory::Failure::CREATE,
      "pending_segments_1");  // fail first phase of transaction
    dir.register_failure(
      failing_directory::Failure::SYNC,
      "pending_segments_1");  // fail first phase of transaction

    // write index
    auto writer = irs::IndexWriter::Make(dir, codec, irs::OM_CREATE);
    ASSERT_NE(nullptr, writer);

    ASSERT_TRUE(insert(*writer, doc1->indexed.begin(), doc1->indexed.end(),
                       doc1->stored.begin(), doc1->stored.end()));

    ASSERT_THROW(writer->Begin(), irs::io_error);  // creation failure
    ASSERT_THROW(writer->Begin(), irs::io_error);  // synchronization failure

    // successful attempt
    ASSERT_TRUE(writer->Begin());
    ASSERT_FALSE(writer->Commit());
    tests::AssertSnapshotEquality(writer->GetSnapshot(),
                                  irs::DirectoryReader(dir));

    // ensure no data
    auto reader = irs::DirectoryReader(dir);
    ASSERT_TRUE(reader);
    ASSERT_EQ(0, reader->size());
    ASSERT_EQ(0, reader->docs_count());
    ASSERT_EQ(0, reader->live_docs_count());
  }

  {
    constexpr irs::IndexFeatures all_features =
      irs::IndexFeatures::FREQ | irs::IndexFeatures::POS |
      irs::IndexFeatures::OFFS | irs::IndexFeatures::PAY;

    irs::memory_directory impl;
    failing_directory dir(impl);
    dir.register_failure(
      failing_directory::Failure::CREATE,
      "pending_segments_1");  // fail first phase of transaction
    dir.register_failure(
      failing_directory::Failure::SYNC,
      "pending_segments_1");  // fail first phase of transaction

    // write index
    auto writer = irs::IndexWriter::Make(dir, codec, irs::OM_CREATE);
    ASSERT_NE(nullptr, writer);

    ASSERT_TRUE(insert(*writer, doc1->indexed.begin(), doc1->indexed.end(),
                       doc1->stored.begin(), doc1->stored.end()));

    ASSERT_THROW(writer->Begin(), irs::io_error);  // creation failure
    ASSERT_THROW(writer->Begin(), irs::io_error);  // synchronization failure

    ASSERT_TRUE(insert(*writer, doc1->indexed.begin(), doc1->indexed.end(),
                       doc1->stored.begin(), doc1->stored.end()));

    // successful attempt
    ASSERT_TRUE(writer->Begin());
    ASSERT_FALSE(writer->Commit());

    // check data
    auto reader = irs::DirectoryReader(dir);
    tests::AssertSnapshotEquality(writer->GetSnapshot(), reader);
    ASSERT_TRUE(reader);
    ASSERT_EQ(1, reader->size());
    ASSERT_EQ(1, reader->docs_count());
    ASSERT_EQ(1, reader->live_docs_count());

    // validate index
    tests::index_t expected_index;
    expected_index.emplace_back(writer->FeatureInfo());
    expected_index.back().insert(*doc1);
    tests::assert_index(reader.GetImpl(), expected_index, all_features);

    // validate columnstore
    auto& segment = reader[0];  // assume 0 is id of first/only segment
    const auto* column = segment.column("name");
    ASSERT_NE(nullptr, column);
    auto values = column->iterator(irs::ColumnHint::kNormal);
    ASSERT_NE(nullptr, values);
    auto* actual_value = irs::get<irs::payload>(*values);
    ASSERT_NE(nullptr, actual_value);
    ASSERT_EQ(1, segment.docs_count());       // total count of documents
    ASSERT_EQ(1, segment.live_docs_count());  // total count of documents
    auto terms = segment.field("same");
    ASSERT_NE(nullptr, terms);
    auto termItr = terms->iterator(irs::SeekMode::NORMAL);
    ASSERT_TRUE(termItr->next());
    auto docsItr = termItr->postings(irs::IndexFeatures::NONE);
    ASSERT_TRUE(docsItr->next());
    ASSERT_EQ(docsItr->value(), values->seek(docsItr->value()));
    ASSERT_EQ("A", irs::to_string<std::string_view>(
                     actual_value->value.data()));  // 'name' value in doc3
    ASSERT_FALSE(docsItr->next());
  }
}

TEST(index_death_test_formats_10, index_commit_fail_sync_1st_phase) {
  tests::json_doc_generator gen(
    test_base::resource("simple_sequential.json"),
    [](tests::document& doc, const std::string& name,
       const tests::json_doc_generator::json_value& data) {
      if (data.is_string()) {
        doc.insert(std::make_shared<tests::string_field>(name, data.str));
      }
    });
  const auto* doc1 = gen.next();

  auto codec = irs::formats::get("1_0");
  ASSERT_NE(nullptr, codec);

  {
    irs::memory_directory impl;
    failing_directory dir(impl);
    dir.register_failure(failing_directory::Failure::SYNC,
                         "_1.0.sm");  // unable to sync segment meta
    dir.register_failure(failing_directory::Failure::SYNC,
                         "_2.doc");  // unable to sync postings
    dir.register_failure(failing_directory::Failure::SYNC,
                         "_3.ti");  // unable to sync term index

    // write index
    auto writer = irs::IndexWriter::Make(dir, codec, irs::OM_CREATE);
    ASSERT_NE(nullptr, writer);

    ASSERT_TRUE(insert(*writer, doc1->indexed.begin(), doc1->indexed.end(),
                       doc1->stored.begin(), doc1->stored.end()));

    ASSERT_THROW(writer->Begin(), irs::io_error);  // synchronization failure

    ASSERT_TRUE(insert(*writer, doc1->indexed.begin(), doc1->indexed.end(),
                       doc1->stored.begin(), doc1->stored.end()));

    ASSERT_THROW(writer->Begin(), irs::io_error);  // synchronization failure

    ASSERT_TRUE(insert(*writer, doc1->indexed.begin(), doc1->indexed.end(),
                       doc1->stored.begin(), doc1->stored.end()));

    ASSERT_THROW(writer->Begin(), irs::io_error);  // synchronization failure

    // successful attempt
    ASSERT_TRUE(writer->Begin());
    ASSERT_FALSE(writer->Commit());

    // ensure no data
    auto reader = irs::DirectoryReader(dir);
    tests::AssertSnapshotEquality(writer->GetSnapshot(), reader);
    ASSERT_TRUE(reader);
    ASSERT_EQ(0, reader->size());
    ASSERT_EQ(0, reader->docs_count());
    ASSERT_EQ(0, reader->live_docs_count());
  }

  {
    constexpr irs::IndexFeatures all_features =
      irs::IndexFeatures::FREQ | irs::IndexFeatures::POS |
      irs::IndexFeatures::OFFS | irs::IndexFeatures::PAY;

    irs::memory_directory impl;
    failing_directory dir(impl);
    dir.register_failure(failing_directory::Failure::SYNC,
                         "_1.0.sm");  // unable to sync segment meta
    dir.register_failure(failing_directory::Failure::SYNC,
                         "_2.doc");  // unable to sync postings
    dir.register_failure(failing_directory::Failure::SYNC,
                         "_3.tm");  // unable to sync term index

    // write index
    auto writer = irs::IndexWriter::Make(dir, codec, irs::OM_CREATE);
    ASSERT_NE(nullptr, writer);

    // initial commit
    ASSERT_TRUE(writer->Begin());
    ASSERT_FALSE(writer->Commit());
    tests::AssertSnapshotEquality(writer->GetSnapshot(),
                                  irs::DirectoryReader(dir));

    ASSERT_TRUE(insert(*writer, doc1->indexed.begin(), doc1->indexed.end(),
                       doc1->stored.begin(), doc1->stored.end()));

    ASSERT_THROW(writer->Begin(), irs::io_error);  // synchronization failure
    ASSERT_FALSE(writer->Begin());                 // nothing to flush

    ASSERT_TRUE(insert(*writer, doc1->indexed.begin(), doc1->indexed.end(),
                       doc1->stored.begin(), doc1->stored.end()));

    ASSERT_THROW(writer->Begin(), irs::io_error);  // synchronization failure
    ASSERT_FALSE(writer->Begin());                 // nothing to flush

    ASSERT_TRUE(insert(*writer, doc1->indexed.begin(), doc1->indexed.end(),
                       doc1->stored.begin(), doc1->stored.end()));

    ASSERT_THROW(writer->Begin(), irs::io_error);  // synchronization failure
    ASSERT_FALSE(writer->Begin());                 // nothing to flush

    ASSERT_TRUE(insert(*writer, doc1->indexed.begin(), doc1->indexed.end(),
                       doc1->stored.begin(), doc1->stored.end()));

    // successful attempt
    ASSERT_TRUE(writer->Begin());
    ASSERT_FALSE(writer->Commit());

    // check data
    auto reader = irs::DirectoryReader(dir);
    tests::AssertSnapshotEquality(writer->GetSnapshot(), reader);
    ASSERT_TRUE(reader);
    ASSERT_EQ(1, reader->size());
    ASSERT_EQ(1, reader->docs_count());
    ASSERT_EQ(1, reader->live_docs_count());

    // validate index
    tests::index_t expected_index;
    expected_index.emplace_back(writer->FeatureInfo());
    expected_index.back().insert(*doc1);
    tests::assert_index(reader.GetImpl(), expected_index, all_features);

    // validate columnstore
    auto& segment = reader[0];  // assume 0 is id of first/only segment
    const auto* column = segment.column("name");
    ASSERT_NE(nullptr, column);
    auto values = column->iterator(irs::ColumnHint::kNormal);
    ASSERT_NE(nullptr, values);
    auto* actual_value = irs::get<irs::payload>(*values);
    ASSERT_NE(nullptr, actual_value);
    ASSERT_EQ(1, segment.docs_count());       // total count of documents
    ASSERT_EQ(1, segment.live_docs_count());  // total count of documents
    auto terms = segment.field("same");
    ASSERT_NE(nullptr, terms);
    auto termItr = terms->iterator(irs::SeekMode::NORMAL);
    ASSERT_TRUE(termItr->next());
    auto docsItr = termItr->postings(irs::IndexFeatures::NONE);
    ASSERT_TRUE(docsItr->next());
    ASSERT_EQ(docsItr->value(), values->seek(docsItr->value()));
    ASSERT_EQ("A", irs::to_string<std::string_view>(
                     actual_value->value.data()));  // 'name' value in doc3
    ASSERT_FALSE(docsItr->next());
  }
}

TEST(index_death_test_formats_10, index_meta_write_failure_2nd_phase) {
  tests::json_doc_generator gen(
    test_base::resource("simple_sequential.json"),
    [](tests::document& doc, const std::string& name,
       const tests::json_doc_generator::json_value& data) {
      if (data.is_string()) {
        doc.insert(std::make_shared<tests::string_field>(name, data.str));
      }
    });
  const auto* doc1 = gen.next();

  auto codec = irs::formats::get("1_0");
  ASSERT_NE(nullptr, codec);

  {
    irs::memory_directory impl;
    failing_directory dir(impl);
    // fail second phase of transaction
    dir.register_failure(failing_directory::Failure::RENAME,
                         "pending_segments_1");

    // write index
    auto writer = irs::IndexWriter::Make(dir, codec, irs::OM_CREATE);
    ASSERT_NE(nullptr, writer);

    ASSERT_TRUE(insert(*writer, doc1->indexed.begin(), doc1->indexed.end(),
                       doc1->stored.begin(), doc1->stored.end()));

    ASSERT_TRUE(writer->Begin());
    ASSERT_THROW(writer->Commit(), irs::io_error);
    ASSERT_THROW((irs::DirectoryReader{dir}), irs::index_not_found);

    // second attempt
    ASSERT_TRUE(writer->Begin());
    ASSERT_FALSE(writer->Commit());
    tests::AssertSnapshotEquality(writer->GetSnapshot(),
                                  irs::DirectoryReader(dir));

    // ensure no data
    auto reader = irs::DirectoryReader(dir);
    ASSERT_TRUE(reader);
    ASSERT_EQ(0, reader->size());
    ASSERT_EQ(0, reader->docs_count());
    ASSERT_EQ(0, reader->live_docs_count());
  }

  {
    constexpr irs::IndexFeatures all_features =
      irs::IndexFeatures::FREQ | irs::IndexFeatures::POS |
      irs::IndexFeatures::OFFS | irs::IndexFeatures::PAY;

    irs::memory_directory impl;
    failing_directory dir(impl);
    dir.register_failure(
      failing_directory::Failure::RENAME,
      "pending_segments_1");  // fail second phase of transaction

    // write index
    auto writer = irs::IndexWriter::Make(dir, codec, irs::OM_CREATE);
    ASSERT_NE(nullptr, writer);

    ASSERT_TRUE(insert(*writer, doc1->indexed.begin(), doc1->indexed.end(),
                       doc1->stored.begin(), doc1->stored.end()));

    ASSERT_TRUE(writer->Begin());
    ASSERT_THROW(writer->Commit(), irs::io_error);
    ASSERT_THROW((irs::DirectoryReader{dir}), irs::index_not_found);

    ASSERT_TRUE(insert(*writer, doc1->indexed.begin(), doc1->indexed.end(),
                       doc1->stored.begin(), doc1->stored.end()));

    // second attempt
    ASSERT_TRUE(writer->Begin());
    ASSERT_FALSE(writer->Commit());
    tests::AssertSnapshotEquality(writer->GetSnapshot(),
                                  irs::DirectoryReader(dir));

    // check data
    auto reader = irs::DirectoryReader(dir);
    ASSERT_TRUE(reader);
    ASSERT_EQ(1, reader->size());
    ASSERT_EQ(1, reader->docs_count());
    ASSERT_EQ(1, reader->live_docs_count());

    // validate index
    tests::index_t expected_index;
    expected_index.emplace_back(writer->FeatureInfo());
    expected_index.back().insert(*doc1);
    tests::assert_index(reader.GetImpl(), expected_index, all_features);

    // validate columnstore
    auto& segment = reader[0];  // assume 0 is id of first/only segment
    const auto* column = segment.column("name");
    ASSERT_NE(nullptr, column);
    auto values = column->iterator(irs::ColumnHint::kNormal);
    ASSERT_NE(nullptr, values);
    auto* actual_value = irs::get<irs::payload>(*values);
    ASSERT_NE(nullptr, actual_value);
    ASSERT_EQ(1, segment.docs_count());       // total count of documents
    ASSERT_EQ(1, segment.live_docs_count());  // total count of documents
    auto terms = segment.field("same");
    ASSERT_NE(nullptr, terms);
    auto termItr = terms->iterator(irs::SeekMode::NORMAL);
    ASSERT_TRUE(termItr->next());
    auto docsItr = termItr->postings(irs::IndexFeatures::NONE);
    ASSERT_TRUE(docsItr->next());
    ASSERT_EQ(docsItr->value(), values->seek(docsItr->value()));
    ASSERT_EQ("A", irs::to_string<std::string_view>(
                     actual_value->value.data()));  // 'name' value in doc3
    ASSERT_FALSE(docsItr->next());
  }
}

TEST(index_death_test_formats_10,
     segment_columnstore_creation_failure_1st_phase_flush) {
  tests::json_doc_generator gen(test_base::resource("simple_sequential.json"),
                                &tests::payloaded_json_field_factory);
  const auto* doc1 = gen.next();
  const auto* doc2 = gen.next();

  auto codec = irs::formats::get("1_0");
  ASSERT_NE(nullptr, codec);

  {
    irs::memory_directory impl;
    failing_directory dir(impl);
    dir.register_failure(failing_directory::Failure::CREATE,
                         "_1.cs");  // columnstore

    // write index
    auto writer = irs::IndexWriter::Make(dir, codec, irs::OM_CREATE);
    ASSERT_NE(nullptr, writer);

    // segment meta
    ASSERT_THROW(insert(*writer, doc1->indexed.begin(), doc1->indexed.end(),
                        doc1->stored.begin(), doc1->stored.end()),
                 irs::io_error);

    // successul attempt
    ASSERT_TRUE(writer->Begin());
    ASSERT_FALSE(writer->Commit());
    tests::AssertSnapshotEquality(writer->GetSnapshot(),
                                  irs::DirectoryReader(dir));

    // ensure no data
    auto reader = irs::DirectoryReader(dir);
    ASSERT_TRUE(reader);
    ASSERT_EQ(0, reader->size());
    ASSERT_EQ(0, reader->docs_count());
    ASSERT_EQ(0, reader->live_docs_count());
  }

  {
    constexpr irs::IndexFeatures all_features =
      irs::IndexFeatures::FREQ | irs::IndexFeatures::POS |
      irs::IndexFeatures::OFFS | irs::IndexFeatures::PAY;

    irs::memory_directory impl;
    failing_directory dir(impl);
    dir.register_failure(failing_directory::Failure::CREATE,
                         "_1.cs");  // columnstore

    // write index
    auto writer = irs::IndexWriter::Make(dir, codec, irs::OM_CREATE);
    ASSERT_NE(nullptr, writer);

    ASSERT_THROW(insert(*writer, doc2->indexed.begin(), doc2->indexed.end(),
                        doc2->stored.begin(), doc2->stored.end()),
                 irs::io_error);

    ASSERT_TRUE(insert(*writer, doc1->indexed.begin(), doc1->indexed.end(),
                       doc1->stored.begin(), doc1->stored.end()));

    // successul attempt
    ASSERT_TRUE(writer->Begin());
    ASSERT_FALSE(writer->Commit());
    tests::AssertSnapshotEquality(writer->GetSnapshot(),
                                  irs::DirectoryReader(dir));

    // check data
    auto reader = irs::DirectoryReader(dir);
    ASSERT_TRUE(reader);
    ASSERT_EQ(1, reader->size());
    ASSERT_EQ(1, reader->docs_count());
    ASSERT_EQ(1, reader->live_docs_count());

    // validate index
    tests::index_t expected_index;
    expected_index.emplace_back(writer->FeatureInfo());
    expected_index.back().insert(*doc1);
    tests::assert_index(reader.GetImpl(), expected_index, all_features);

    // validate columnstore
    auto& segment = reader[0];  // assume 0 is id of first/only segment
    const auto* column = segment.column("name");
    ASSERT_NE(nullptr, column);
    auto values = column->iterator(irs::ColumnHint::kNormal);
    ASSERT_NE(nullptr, values);
    auto* actual_value = irs::get<irs::payload>(*values);
    ASSERT_NE(nullptr, actual_value);
    ASSERT_EQ(1, segment.docs_count());       // total count of documents
    ASSERT_EQ(1, segment.live_docs_count());  // total count of documents
    auto terms = segment.field("same");
    ASSERT_NE(nullptr, terms);
    auto termItr = terms->iterator(irs::SeekMode::NORMAL);
    ASSERT_TRUE(termItr->next());
    auto docsItr = termItr->postings(irs::IndexFeatures::NONE);
    ASSERT_TRUE(docsItr->next());
    ASSERT_EQ(docsItr->value(), values->seek(docsItr->value()));
    ASSERT_EQ("A", irs::to_string<std::string_view>(
                     actual_value->value.data()));  // 'name' value in doc3
    ASSERT_FALSE(docsItr->next());
  }

  {
    constexpr irs::IndexFeatures all_features =
      irs::IndexFeatures::FREQ | irs::IndexFeatures::POS |
      irs::IndexFeatures::OFFS | irs::IndexFeatures::PAY;

    irs::memory_directory impl;
    failing_directory dir(impl);
    dir.register_failure(failing_directory::Failure::CREATE,
                         "_2.cs");  // columnstore

    // write index
    auto writer = irs::IndexWriter::Make(dir, codec, irs::OM_CREATE);
    ASSERT_NE(nullptr, writer);

    ASSERT_TRUE(insert(*writer, doc1->indexed.begin(), doc1->indexed.end(),
                       doc1->stored.begin(), doc1->stored.end()));

    // successul attempt
    ASSERT_TRUE(writer->Begin());
    ASSERT_FALSE(writer->Commit());
    tests::AssertSnapshotEquality(writer->GetSnapshot(),
                                  irs::DirectoryReader(dir));

    ASSERT_THROW(insert(*writer, doc2->indexed.begin(), doc2->indexed.end(),
                        doc2->stored.begin(), doc2->stored.end()),
                 irs::io_error);

    // nothing to flush
    ASSERT_FALSE(writer->Begin());

    // check data
    auto reader = irs::DirectoryReader(dir);
    ASSERT_TRUE(reader);
    ASSERT_EQ(1, reader->size());
    ASSERT_EQ(1, reader->docs_count());
    ASSERT_EQ(1, reader->live_docs_count());

    // validate index
    tests::index_t expected_index;
    expected_index.emplace_back(writer->FeatureInfo());
    expected_index.back().insert(*doc1);
    tests::assert_index(reader.GetImpl(), expected_index, all_features);

    // validate columnstore
    auto& segment = reader[0];  // assume 0 is id of first/only segment
    const auto* column = segment.column("name");
    ASSERT_NE(nullptr, column);
    auto values = column->iterator(irs::ColumnHint::kNormal);
    ASSERT_NE(nullptr, values);
    auto* actual_value = irs::get<irs::payload>(*values);
    ASSERT_NE(nullptr, actual_value);
    ASSERT_EQ(1, segment.docs_count());       // total count of documents
    ASSERT_EQ(1, segment.live_docs_count());  // total count of documents
    auto terms = segment.field("same");
    ASSERT_NE(nullptr, terms);
    auto termItr = terms->iterator(irs::SeekMode::NORMAL);
    ASSERT_TRUE(termItr->next());
    auto docsItr = termItr->postings(irs::IndexFeatures::NONE);
    ASSERT_TRUE(docsItr->next());
    ASSERT_EQ(docsItr->value(), values->seek(docsItr->value()));
    ASSERT_EQ("A", irs::to_string<std::string_view>(
                     actual_value->value.data()));  // 'name' value in doc3
    ASSERT_FALSE(docsItr->next());
  }

  {
    constexpr irs::IndexFeatures all_features =
      irs::IndexFeatures::FREQ | irs::IndexFeatures::POS |
      irs::IndexFeatures::OFFS | irs::IndexFeatures::PAY;

    irs::memory_directory impl;
    failing_directory dir(impl);
    dir.register_failure(failing_directory::Failure::CREATE,
                         "_2.cs");  // columnstore

    // write index
    auto writer = irs::IndexWriter::Make(dir, codec, irs::OM_CREATE);
    ASSERT_NE(nullptr, writer);

    ASSERT_TRUE(insert(*writer, doc1->indexed.begin(), doc1->indexed.end(),
                       doc1->stored.begin(), doc1->stored.end()));

    // successul attempt
    ASSERT_TRUE(writer->Begin());
    ASSERT_FALSE(writer->Commit());
    tests::AssertSnapshotEquality(writer->GetSnapshot(),
                                  irs::DirectoryReader(dir));

    ASSERT_THROW(insert(*writer, doc2->indexed.begin(), doc2->indexed.end(),
                        doc2->stored.begin(), doc2->stored.end()),
                 irs::io_error);

    ASSERT_TRUE(insert(*writer, doc2->indexed.begin(), doc2->indexed.end(),
                       doc2->stored.begin(), doc2->stored.end()));

    // nothing to flush
    ASSERT_TRUE(writer->Begin());
    ASSERT_FALSE(writer->Commit());
    tests::AssertSnapshotEquality(writer->GetSnapshot(),
                                  irs::DirectoryReader(dir));

    // check data
    auto reader = irs::DirectoryReader(dir);
    ASSERT_TRUE(reader);
    ASSERT_EQ(2, reader->size());
    ASSERT_EQ(2, reader->docs_count());
    ASSERT_EQ(2, reader->live_docs_count());

    // validate index
    tests::index_t expected_index;
    expected_index.emplace_back(writer->FeatureInfo());
    expected_index.back().insert(*doc1);
    expected_index.emplace_back(writer->FeatureInfo());
    expected_index.back().insert(*doc2);
    tests::assert_index(reader.GetImpl(), expected_index, all_features);

    // validate columnstore (segment 0)
    {
      auto& segment = reader[0];  // assume 0 is id of first/only segment
      const auto* column = segment.column("name");
      ASSERT_NE(nullptr, column);
      auto values = column->iterator(irs::ColumnHint::kNormal);
      ASSERT_NE(nullptr, values);
      auto* actual_value = irs::get<irs::payload>(*values);
      ASSERT_NE(nullptr, actual_value);
      ASSERT_EQ(1, segment.docs_count());       // total count of documents
      ASSERT_EQ(1, segment.live_docs_count());  // total count of documents
      auto terms = segment.field("same");
      ASSERT_NE(nullptr, terms);
      auto termItr = terms->iterator(irs::SeekMode::NORMAL);
      ASSERT_TRUE(termItr->next());
      auto docsItr = termItr->postings(irs::IndexFeatures::NONE);
      ASSERT_TRUE(docsItr->next());
      ASSERT_EQ(docsItr->value(), values->seek(docsItr->value()));
      ASSERT_EQ("A", irs::to_string<std::string_view>(
                       actual_value->value.data()));  // 'name' value in doc3
      ASSERT_FALSE(docsItr->next());
    }

    // validate columnstore (segment 1)
    {
      auto& segment = reader[1];  // assume 0 is id of first/only segment
      const auto* column = segment.column("name");
      ASSERT_NE(nullptr, column);
      auto values = column->iterator(irs::ColumnHint::kNormal);
      ASSERT_NE(nullptr, values);
      auto* actual_value = irs::get<irs::payload>(*values);
      ASSERT_NE(nullptr, actual_value);
      ASSERT_EQ(1, segment.docs_count());       // total count of documents
      ASSERT_EQ(1, segment.live_docs_count());  // total count of documents
      auto terms = segment.field("same");
      ASSERT_NE(nullptr, terms);
      auto termItr = terms->iterator(irs::SeekMode::NORMAL);
      ASSERT_TRUE(termItr->next());
      auto docsItr = termItr->postings(irs::IndexFeatures::NONE);
      ASSERT_TRUE(docsItr->next());
      ASSERT_EQ(docsItr->value(), values->seek(docsItr->value()));
      ASSERT_EQ("B", irs::to_string<std::string_view>(
                       actual_value->value.data()));  // 'name' value in doc3
      ASSERT_FALSE(docsItr->next());
    }
  }
}

TEST(index_death_test_formats_10,
     segment_components_creation_failure_1st_phase_flush) {
  tests::json_doc_generator gen(test_base::resource("simple_sequential.json"),
                                &tests::payloaded_json_field_factory);
  const auto* doc1 = gen.next();
  const auto* doc2 = gen.next();
  auto query_doc2 = MakeByTerm("name", "B");

  auto codec = irs::formats::get("1_0");
  ASSERT_NE(nullptr, codec);

  {
    irs::memory_directory impl;
    failing_directory dir(impl);
    dir.register_failure(failing_directory::Failure::CREATE,
                         "_1.doc");  // postings list (documents)
    dir.register_failure(failing_directory::Failure::CREATE,
                         "_2.2.doc_mask");  // deleted docs
    dir.register_failure(failing_directory::Failure::CREATE,
                         "_3.cm");  // column meta
    dir.register_failure(failing_directory::Failure::CREATE,
                         "_4.ti");  // term index
    dir.register_failure(failing_directory::Failure::CREATE,
                         "_5.tm");  // term data
    dir.register_failure(failing_directory::Failure::CREATE,
                         "_6.pos");  // postings list (positions)
    dir.register_failure(failing_directory::Failure::CREATE,
                         "_7.pay");  // postings list (offset + payload)

    // write index
    auto writer = irs::IndexWriter::Make(dir, codec, irs::OM_CREATE);
    ASSERT_NE(nullptr, writer);

    // initial commit
    ASSERT_TRUE(writer->Begin());
    ASSERT_FALSE(writer->Commit());
    tests::AssertSnapshotEquality(writer->GetSnapshot(),
                                  irs::DirectoryReader(dir));

    // segment meta
    while (!dir.no_failures()) {
      ASSERT_TRUE(insert(*writer, doc1->indexed.begin(), doc1->indexed.end(),
                         doc1->stored.begin(), doc1->stored.end()));
      ASSERT_TRUE(insert(*writer, doc2->indexed.begin(), doc2->indexed.end(),
                         doc2->stored.begin(), doc2->stored.end()));

      writer->GetBatch().Remove(*query_doc2);

      ASSERT_THROW(writer->Begin(), irs::io_error);
      ASSERT_FALSE(writer->Begin());  // nothing to flush
    }

    // ensure no data
    auto reader = irs::DirectoryReader(dir);
    ASSERT_TRUE(reader);
    ASSERT_EQ(0, reader->size());
    ASSERT_EQ(0, reader->docs_count());
    ASSERT_EQ(0, reader->live_docs_count());
  }

  {
    constexpr irs::IndexFeatures all_features =
      irs::IndexFeatures::FREQ | irs::IndexFeatures::POS |
      irs::IndexFeatures::OFFS | irs::IndexFeatures::PAY;

    irs::memory_directory impl;
    failing_directory dir(impl);
    dir.register_failure(failing_directory::Failure::CREATE,
                         "_1.doc");  // postings list (documents)
    dir.register_failure(failing_directory::Failure::CREATE,
                         "_2.2.doc_mask");  // deleted docs
    dir.register_failure(failing_directory::Failure::CREATE,
                         "_3.cm");  // column meta
    dir.register_failure(failing_directory::Failure::CREATE,
                         "_4.ti");  // term index
    dir.register_failure(failing_directory::Failure::CREATE,
                         "_5.tm");  // term data
    dir.register_failure(failing_directory::Failure::CREATE,
                         "_6.pos");  // postings list (positions)
    dir.register_failure(failing_directory::Failure::CREATE,
                         "_7.pay");  // postings list (offset + payload)

    // write index
    auto writer = irs::IndexWriter::Make(dir, codec, irs::OM_CREATE);
    ASSERT_NE(nullptr, writer);

    // segment meta
    while (!dir.no_failures()) {
      ASSERT_TRUE(insert(*writer, doc1->indexed.begin(), doc1->indexed.end(),
                         doc1->stored.begin(), doc1->stored.end()));
      ASSERT_TRUE(insert(*writer, doc2->indexed.begin(), doc2->indexed.end(),
                         doc2->stored.begin(), doc2->stored.end()));

      writer->GetBatch().Remove(*query_doc2);

      ASSERT_THROW(writer->Begin(), irs::io_error);
    }

    ASSERT_TRUE(insert(*writer, doc1->indexed.begin(), doc1->indexed.end(),
                       doc1->stored.begin(), doc1->stored.end()));

    // successul attempt
    ASSERT_TRUE(writer->Begin());
    ASSERT_FALSE(writer->Commit());
    tests::AssertSnapshotEquality(writer->GetSnapshot(),
                                  irs::DirectoryReader(dir));

    // check data
    auto reader = irs::DirectoryReader(dir);
    ASSERT_TRUE(reader);
    ASSERT_EQ(1, reader->size());
    ASSERT_EQ(1, reader->docs_count());
    ASSERT_EQ(1, reader->live_docs_count());

    // validate index
    tests::index_t expected_index;
    expected_index.emplace_back(writer->FeatureInfo());
    expected_index.back().insert(*doc1);
    tests::assert_index(reader.GetImpl(), expected_index, all_features);

    // validate columnstore
    auto& segment = reader[0];  // assume 0 is id of first/only segment
    const auto* column = segment.column("name");
    ASSERT_NE(nullptr, column);
    auto values = column->iterator(irs::ColumnHint::kNormal);
    ASSERT_NE(nullptr, values);
    auto* actual_value = irs::get<irs::payload>(*values);
    ASSERT_NE(nullptr, actual_value);
    ASSERT_EQ(1, segment.docs_count());       // total count of documents
    ASSERT_EQ(1, segment.live_docs_count());  // total count of documents
    auto terms = segment.field("same");
    ASSERT_NE(nullptr, terms);
    auto termItr = terms->iterator(irs::SeekMode::NORMAL);
    ASSERT_TRUE(termItr->next());
    auto docsItr = termItr->postings(irs::IndexFeatures::NONE);
    ASSERT_TRUE(docsItr->next());
    ASSERT_EQ(docsItr->value(), values->seek(docsItr->value()));
    ASSERT_EQ("A", irs::to_string<std::string_view>(
                     actual_value->value.data()));  // 'name' value in doc3
    ASSERT_FALSE(docsItr->next());
  }
}

TEST(index_death_test_formats_10,
     segment_components_sync_failure_1st_phase_flush) {
  tests::json_doc_generator gen(test_base::resource("simple_sequential.json"),
                                &tests::payloaded_json_field_factory);
  const auto* doc1 = gen.next();
  const auto* doc2 = gen.next();
  auto query_doc2 = MakeByTerm("name", "B");

  auto codec = irs::formats::get("1_0");
  ASSERT_NE(nullptr, codec);

  {
    irs::memory_directory impl;
    failing_directory dir(impl);
    dir.register_failure(failing_directory::Failure::SYNC,
                         "_1.2.sm");  // segment meta
    dir.register_failure(failing_directory::Failure::SYNC,
                         "_2.doc");  // postings list (documents)
    dir.register_failure(failing_directory::Failure::SYNC,
                         "_3.2.doc_mask");  // deleted docs
    dir.register_failure(failing_directory::Failure::SYNC,
                         "_4.cm");  // column meta
    dir.register_failure(failing_directory::Failure::SYNC,
                         "_5.cs");  // columnstore
    dir.register_failure(failing_directory::Failure::SYNC,
                         "_6.ti");  // term index
    dir.register_failure(failing_directory::Failure::SYNC,
                         "_7.tm");  // term data
    dir.register_failure(failing_directory::Failure::SYNC,
                         "_8.pos");  // postings list (positions)
    dir.register_failure(failing_directory::Failure::SYNC,
                         "_9.pay");  // postings list (offset + payload)

    // write index
    auto writer = irs::IndexWriter::Make(dir, codec, irs::OM_CREATE);
    ASSERT_NE(nullptr, writer);

    // segment meta
    while (!dir.no_failures()) {
      ASSERT_TRUE(insert(*writer, doc1->indexed.begin(), doc1->indexed.end(),
                         doc1->stored.begin(), doc1->stored.end()));
      ASSERT_TRUE(insert(*writer, doc2->indexed.begin(), doc2->indexed.end(),
                         doc2->stored.begin(), doc2->stored.end()));

      writer->GetBatch().Remove(*query_doc2);

      ASSERT_THROW(writer->Begin(), irs::io_error);
    }

    // successul attempt
    ASSERT_TRUE(writer->Begin());
    ASSERT_FALSE(writer->Commit());
    tests::AssertSnapshotEquality(writer->GetSnapshot(),
                                  irs::DirectoryReader(dir));

    // ensure no data
    auto reader = irs::DirectoryReader(dir);
    ASSERT_TRUE(reader);
    ASSERT_EQ(0, reader->size());
    ASSERT_EQ(0, reader->docs_count());
    ASSERT_EQ(0, reader->live_docs_count());
  }

  {
    constexpr irs::IndexFeatures all_features =
      irs::IndexFeatures::FREQ | irs::IndexFeatures::POS |
      irs::IndexFeatures::OFFS | irs::IndexFeatures::PAY;

    irs::memory_directory impl;
    failing_directory dir(impl);
    dir.register_failure(failing_directory::Failure::SYNC,
                         "_1.2.sm");  // segment meta
    dir.register_failure(failing_directory::Failure::SYNC,
                         "_2.doc");  // postings list (documents)
    dir.register_failure(failing_directory::Failure::SYNC,
                         "_3.2.doc_mask");  // deleted docs
    dir.register_failure(failing_directory::Failure::SYNC,
                         "_4.cm");  // column meta
    dir.register_failure(failing_directory::Failure::SYNC,
                         "_5.cs");  // columnstore
    dir.register_failure(failing_directory::Failure::SYNC,
                         "_6.ti");  // term index
    dir.register_failure(failing_directory::Failure::SYNC,
                         "_7.tm");  // term data
    dir.register_failure(failing_directory::Failure::SYNC,
                         "_8.pos");  // postings list (positions)
    dir.register_failure(failing_directory::Failure::SYNC,
                         "_9.pay");  // postings list (offset + payload)

    // write index
    auto writer = irs::IndexWriter::Make(dir, codec, irs::OM_CREATE);
    ASSERT_NE(nullptr, writer);

    // segment meta
    while (!dir.no_failures()) {
      ASSERT_TRUE(insert(*writer, doc1->indexed.begin(), doc1->indexed.end(),
                         doc1->stored.begin(), doc1->stored.end()));
      ASSERT_TRUE(insert(*writer, doc2->indexed.begin(), doc2->indexed.end(),
                         doc2->stored.begin(), doc2->stored.end()));

      writer->GetBatch().Remove(*query_doc2);

      ASSERT_THROW(writer->Begin(), irs::io_error);
    }

    ASSERT_TRUE(insert(*writer, doc1->indexed.begin(), doc1->indexed.end(),
                       doc1->stored.begin(), doc1->stored.end()));

    // successul attempt
    ASSERT_TRUE(writer->Begin());
    ASSERT_FALSE(writer->Commit());
    tests::AssertSnapshotEquality(writer->GetSnapshot(),
                                  irs::DirectoryReader(dir));

    // check data
    auto reader = irs::DirectoryReader(dir);
    ASSERT_TRUE(reader);
    ASSERT_EQ(1, reader->size());
    ASSERT_EQ(1, reader->docs_count());
    ASSERT_EQ(1, reader->live_docs_count());

    // validate index
    tests::index_t expected_index;
    expected_index.emplace_back(writer->FeatureInfo());
    expected_index.back().insert(*doc1);

    tests::assert_index(reader.GetImpl(), expected_index, all_features);

    // validate columnstore
    auto& segment = reader[0];  // assume 0 is id of first/only segment
    const auto* column = segment.column("name");
    ASSERT_NE(nullptr, column);
    auto values = column->iterator(irs::ColumnHint::kNormal);
    ASSERT_NE(nullptr, values);
    auto* actual_value = irs::get<irs::payload>(*values);
    ASSERT_NE(nullptr, actual_value);
    ASSERT_EQ(1, segment.docs_count());       // total count of documents
    ASSERT_EQ(1, segment.live_docs_count());  // total count of documents
    auto terms = segment.field("same");
    ASSERT_NE(nullptr, terms);
    auto termItr = terms->iterator(irs::SeekMode::NORMAL);
    ASSERT_TRUE(termItr->next());
    auto docsItr = termItr->postings(irs::IndexFeatures::NONE);
    ASSERT_TRUE(docsItr->next());
    ASSERT_EQ(docsItr->value(), values->seek(docsItr->value()));
    ASSERT_EQ("A", irs::to_string<std::string_view>(
                     actual_value->value.data()));  // 'name' value in doc3
    ASSERT_FALSE(docsItr->next());
  }
}

TEST(index_death_test_formats_10,
     segment_meta_creation_failure_1st_phase_flush) {
  tests::json_doc_generator gen(
    test_base::resource("simple_sequential.json"),
    [](tests::document& doc, const std::string& name,
       const tests::json_doc_generator::json_value& data) {
      if (data.is_string()) {
        doc.insert(std::make_shared<tests::string_field>(name, data.str));
      }
    });
  const auto* doc1 = gen.next();

  auto codec = irs::formats::get("1_0");
  ASSERT_NE(nullptr, codec);

  {
    irs::memory_directory impl;
    failing_directory dir(impl);
    dir.register_failure(failing_directory::Failure::CREATE,
                         "_1.0.sm");  // fail at segment meta creation
    dir.register_failure(failing_directory::Failure::SYNC,
                         "_2.0.sm");  // fail at segment meta synchronization

    // write index
    auto writer = irs::IndexWriter::Make(dir, codec, irs::OM_CREATE);
    ASSERT_NE(nullptr, writer);

    // creation issue
    ASSERT_TRUE(insert(*writer, doc1->indexed.begin(), doc1->indexed.end(),
                       doc1->stored.begin(), doc1->stored.end()));

    ASSERT_THROW(writer->Begin(), irs::io_error);

    // synchornization issue
    ASSERT_TRUE(insert(*writer, doc1->indexed.begin(), doc1->indexed.end(),
                       doc1->stored.begin(), doc1->stored.end()));

    ASSERT_THROW(writer->Begin(), irs::io_error);

    // second attempt
    ASSERT_TRUE(writer->Begin());
    ASSERT_FALSE(writer->Commit());
    tests::AssertSnapshotEquality(writer->GetSnapshot(),
                                  irs::DirectoryReader(dir));

    // ensure no data
    auto reader = irs::DirectoryReader(dir);
    ASSERT_TRUE(reader);
    ASSERT_EQ(0, reader->size());
    ASSERT_EQ(0, reader->docs_count());
    ASSERT_EQ(0, reader->live_docs_count());
  }

  {
    constexpr irs::IndexFeatures all_features =
      irs::IndexFeatures::FREQ | irs::IndexFeatures::POS |
      irs::IndexFeatures::OFFS | irs::IndexFeatures::PAY;

    irs::memory_directory impl;
    failing_directory dir(impl);
    dir.register_failure(failing_directory::Failure::CREATE,
                         "_1.0.sm");  // fail at segment meta creation
    dir.register_failure(failing_directory::Failure::SYNC,
                         "_2.0.sm");  // fail at segment meta synchronization

    // write index
    auto writer = irs::IndexWriter::Make(dir, codec, irs::OM_CREATE);
    ASSERT_NE(nullptr, writer);

    // creation issue
    ASSERT_TRUE(insert(*writer, doc1->indexed.begin(), doc1->indexed.end(),
                       doc1->stored.begin(), doc1->stored.end()));

    ASSERT_THROW(writer->Begin(), irs::io_error);

    // synchornization issue
    ASSERT_TRUE(insert(*writer, doc1->indexed.begin(), doc1->indexed.end(),
                       doc1->stored.begin(), doc1->stored.end()));

    ASSERT_THROW(writer->Begin(), irs::io_error);

    ASSERT_TRUE(insert(*writer, doc1->indexed.begin(), doc1->indexed.end(),
                       doc1->stored.begin(), doc1->stored.end()));

    // second attempt
    ASSERT_TRUE(writer->Begin());
    ASSERT_FALSE(writer->Commit());
    tests::AssertSnapshotEquality(writer->GetSnapshot(),
                                  irs::DirectoryReader(dir));

    // check data
    auto reader = irs::DirectoryReader(dir);
    ASSERT_TRUE(reader);
    ASSERT_EQ(1, reader->size());
    ASSERT_EQ(1, reader->docs_count());
    ASSERT_EQ(1, reader->live_docs_count());

    // validate index
    tests::index_t expected_index;
    expected_index.emplace_back(writer->FeatureInfo());
    expected_index.back().insert(*doc1);
    tests::assert_index(reader.GetImpl(), expected_index, all_features);

    // validate columnstore
    auto& segment = reader[0];  // assume 0 is id of first/only segment
    const auto* column = segment.column("name");
    ASSERT_NE(nullptr, column);
    auto values = column->iterator(irs::ColumnHint::kNormal);
    ASSERT_NE(nullptr, values);
    auto* actual_value = irs::get<irs::payload>(*values);
    ASSERT_NE(nullptr, actual_value);
    ASSERT_EQ(1, segment.docs_count());       // total count of documents
    ASSERT_EQ(1, segment.live_docs_count());  // total count of documents
    auto terms = segment.field("same");
    ASSERT_NE(nullptr, terms);
    auto termItr = terms->iterator(irs::SeekMode::NORMAL);
    ASSERT_TRUE(termItr->next());
    auto docsItr = termItr->postings(irs::IndexFeatures::NONE);
    ASSERT_TRUE(docsItr->next());
    ASSERT_EQ(docsItr->value(), values->seek(docsItr->value()));
    ASSERT_EQ("A", irs::to_string<std::string_view>(
                     actual_value->value.data()));  // 'name' value in doc3
    ASSERT_FALSE(docsItr->next());
  }
}

TEST(index_death_test_formats_10,
     segment_meta_write_fail_immediate_consolidation) {
  tests::json_doc_generator gen(
    test_base::resource("simple_sequential.json"),
    [](tests::document& doc, const std::string& name,
       const tests::json_doc_generator::json_value& data) {
      if (data.is_string()) {
        doc.insert(std::make_shared<tests::string_field>(name, data.str));
      }
    });
  const auto* doc1 = gen.next();
  const auto* doc2 = gen.next();

  auto codec = irs::formats::get("1_0");
  ASSERT_NE(nullptr, codec);

  {
    constexpr irs::IndexFeatures all_features =
      irs::IndexFeatures::FREQ | irs::IndexFeatures::POS |
      irs::IndexFeatures::OFFS | irs::IndexFeatures::PAY;

    irs::memory_directory impl;
    failing_directory dir(impl);

    // write index
    auto writer = irs::IndexWriter::Make(dir, codec, irs::OM_CREATE);
    ASSERT_NE(nullptr, writer);

    // segment 0
    ASSERT_TRUE(insert(*writer, doc1->indexed.begin(), doc1->indexed.end(),
                       doc1->stored.begin(), doc1->stored.end()));
    ASSERT_TRUE(writer->Commit());
    tests::AssertSnapshotEquality(writer->GetSnapshot(),
                                  irs::DirectoryReader(dir));

    // segment 1
    ASSERT_TRUE(insert(*writer, doc2->indexed.begin(), doc2->indexed.end(),
                       doc2->stored.begin(), doc2->stored.end()));
    ASSERT_TRUE(writer->Commit());
    tests::AssertSnapshotEquality(writer->GetSnapshot(),
                                  irs::DirectoryReader(dir));

    // register failures
    dir.register_failure(
      failing_directory::Failure::CREATE,
      "_3.0.sm");  // fail at segment meta creation on consolidation
    dir.register_failure(
      failing_directory::Failure::SYNC,
      "_4.0.sm");  // fail at segment meta synchronization on consolidation

    const irs::index_utils::ConsolidateCount consolidate_all;

    // segment meta creation failure
    ASSERT_THROW(
      writer->Consolidate(irs::index_utils::MakePolicy(consolidate_all)),
      irs::io_error);
    ASSERT_FALSE(writer->Begin());  // nothing to flush

    // segment meta synchronization failure
    ASSERT_TRUE(
      writer->Consolidate(irs::index_utils::MakePolicy(consolidate_all)));
    ASSERT_THROW(writer->Begin(), irs::io_error);
    ASSERT_FALSE(writer->Begin());  // nothing to flush

    // check data
    auto reader = irs::DirectoryReader(dir);
    ASSERT_TRUE(reader);
    ASSERT_EQ(2, reader->size());
    ASSERT_EQ(2, reader->docs_count());
    ASSERT_EQ(2, reader->live_docs_count());

    // validate index
    tests::index_t expected_index;
    expected_index.emplace_back(writer->FeatureInfo());
    expected_index.back().insert(*doc1);
    expected_index.emplace_back(writer->FeatureInfo());
    expected_index.back().insert(*doc2);
    tests::assert_index(reader.GetImpl(), expected_index, all_features);

    // validate columnstore (segment 0)
    {
      auto& segment = reader[0];  // assume 0 is id of first/only segment
      const auto* column = segment.column("name");
      ASSERT_NE(nullptr, column);
      auto values = column->iterator(irs::ColumnHint::kNormal);
      ASSERT_NE(nullptr, values);
      auto* actual_value = irs::get<irs::payload>(*values);
      ASSERT_NE(nullptr, actual_value);
      ASSERT_EQ(1, segment.docs_count());       // total count of documents
      ASSERT_EQ(1, segment.live_docs_count());  // total count of documents
      auto terms = segment.field("same");
      ASSERT_NE(nullptr, terms);
      auto termItr = terms->iterator(irs::SeekMode::NORMAL);
      ASSERT_TRUE(termItr->next());
      auto docsItr = termItr->postings(irs::IndexFeatures::NONE);
      ASSERT_TRUE(docsItr->next());
      ASSERT_EQ(docsItr->value(), values->seek(docsItr->value()));
      ASSERT_EQ("A", irs::to_string<std::string_view>(
                       actual_value->value.data()));  // 'name' value in doc3
      ASSERT_FALSE(docsItr->next());
    }

    // validate columnstore (segment 1)
    {
      auto& segment = reader[1];  // assume 0 is id of first/only segment
      const auto* column = segment.column("name");
      ASSERT_NE(nullptr, column);
      auto values = column->iterator(irs::ColumnHint::kNormal);
      ASSERT_NE(nullptr, values);
      auto* actual_value = irs::get<irs::payload>(*values);
      ASSERT_NE(nullptr, actual_value);
      ASSERT_EQ(1, segment.docs_count());       // total count of documents
      ASSERT_EQ(1, segment.live_docs_count());  // total count of documents
      auto terms = segment.field("same");
      ASSERT_NE(nullptr, terms);
      auto termItr = terms->iterator(irs::SeekMode::NORMAL);
      ASSERT_TRUE(termItr->next());
      auto docsItr = termItr->postings(irs::IndexFeatures::NONE);
      ASSERT_TRUE(docsItr->next());
      ASSERT_EQ(docsItr->value(), values->seek(docsItr->value()));
      ASSERT_EQ("B", irs::to_string<std::string_view>(
                       actual_value->value.data()));  // 'name' value in doc3
      ASSERT_FALSE(docsItr->next());
    }
  }
}

TEST(index_death_test_formats_10,
     segment_meta_write_fail_deffered_consolidation) {
  tests::json_doc_generator gen(
    test_base::resource("simple_sequential.json"),
    [](tests::document& doc, const std::string& name,
       const tests::json_doc_generator::json_value& data) {
      if (data.is_string()) {
        doc.insert(std::make_shared<tests::string_field>(name, data.str));
      }
    });
  const auto* doc1 = gen.next();
  const auto* doc2 = gen.next();
  const auto* doc3 = gen.next();
  const auto* doc4 = gen.next();

  auto codec = irs::formats::get("1_0");
  ASSERT_NE(nullptr, codec);

  {
    constexpr irs::IndexFeatures all_features =
      irs::IndexFeatures::FREQ | irs::IndexFeatures::POS |
      irs::IndexFeatures::OFFS | irs::IndexFeatures::PAY;

    irs::memory_directory impl;
    failing_directory dir(impl);

    // write index
    auto writer = irs::IndexWriter::Make(dir, codec, irs::OM_CREATE);
    ASSERT_NE(nullptr, writer);

    // segment 0
    ASSERT_TRUE(insert(*writer, doc1->indexed.begin(), doc1->indexed.end(),
                       doc1->stored.begin(), doc1->stored.end()));
    ASSERT_TRUE(writer->Commit());
    tests::AssertSnapshotEquality(writer->GetSnapshot(),
                                  irs::DirectoryReader(dir));

    // segment 1
    ASSERT_TRUE(insert(*writer, doc2->indexed.begin(), doc2->indexed.end(),
                       doc2->stored.begin(), doc2->stored.end()));
    ASSERT_TRUE(writer->Commit());
    tests::AssertSnapshotEquality(writer->GetSnapshot(),
                                  irs::DirectoryReader(dir));

    // register failures
    dir.register_failure(
      failing_directory::Failure::CREATE,
      "_4.0.sm");  // fail at segment meta creation on consolidation
    dir.register_failure(
      failing_directory::Failure::SYNC,
      "_6.0.sm");  // fail at segment meta synchronization on consolidation

    const irs::index_utils::ConsolidateCount consolidate_all;

    // segment meta creation failure
    ASSERT_TRUE(insert(*writer, doc3->indexed.begin(), doc3->indexed.end(),
                       doc3->stored.begin(), doc3->stored.end()));
    ASSERT_TRUE(writer->Begin());  // start transaction
    ASSERT_TRUE(writer->Consolidate(irs::index_utils::MakePolicy(
      consolidate_all)));            // register pending consolidation
    ASSERT_FALSE(writer->Commit());  // commit started transaction
    tests::AssertSnapshotEquality(writer->GetSnapshot(),
                                  irs::DirectoryReader(dir));
    ASSERT_THROW(
      writer->Begin(),
      irs::io_error);  // start transaction to commit pending consolidation

    // segment meta synchronization failure
    ASSERT_TRUE(insert(*writer, doc4->indexed.begin(), doc4->indexed.end(),
                       doc4->stored.begin(), doc4->stored.end()));
    ASSERT_TRUE(writer->Begin());  // start transaction
    ASSERT_TRUE(writer->Consolidate(irs::index_utils::MakePolicy(
      consolidate_all)));            // register pending consolidation
    ASSERT_FALSE(writer->Commit());  // commit started transaction
    tests::AssertSnapshotEquality(writer->GetSnapshot(),
                                  irs::DirectoryReader(dir));
    ASSERT_THROW(
      writer->Begin(),
      irs::io_error);  // start transaction to commit pending consolidation

    // check data
    auto reader = irs::DirectoryReader(dir);
    ASSERT_TRUE(reader);
    ASSERT_EQ(4, reader->size());
    ASSERT_EQ(4, reader->docs_count());
    ASSERT_EQ(4, reader->live_docs_count());

    // validate index
    tests::index_t expected_index;
    expected_index.emplace_back(writer->FeatureInfo());
    expected_index.back().insert(*doc1);
    expected_index.emplace_back(writer->FeatureInfo());
    expected_index.back().insert(*doc2);
    expected_index.emplace_back(writer->FeatureInfo());
    expected_index.back().insert(*doc3);
    expected_index.emplace_back(writer->FeatureInfo());
    expected_index.back().insert(*doc4);
    tests::assert_index(reader.GetImpl(), expected_index, all_features);

    // validate columnstore (segment 0)
    {
      auto& segment = reader[0];  // assume 0 is id of first/only segment
      const auto* column = segment.column("name");
      ASSERT_NE(nullptr, column);
      auto values = column->iterator(irs::ColumnHint::kNormal);
      ASSERT_NE(nullptr, values);
      auto* actual_value = irs::get<irs::payload>(*values);
      ASSERT_NE(nullptr, actual_value);
      ASSERT_EQ(1, segment.docs_count());       // total count of documents
      ASSERT_EQ(1, segment.live_docs_count());  // total count of documents
      auto terms = segment.field("same");
      ASSERT_NE(nullptr, terms);
      auto termItr = terms->iterator(irs::SeekMode::NORMAL);
      ASSERT_TRUE(termItr->next());
      auto docsItr = termItr->postings(irs::IndexFeatures::NONE);
      ASSERT_TRUE(docsItr->next());
      ASSERT_EQ(docsItr->value(), values->seek(docsItr->value()));
      ASSERT_EQ("A", irs::to_string<std::string_view>(
                       actual_value->value.data()));  // 'name' value in doc3
      ASSERT_FALSE(docsItr->next());
    }

    // validate columnstore (segment 1)
    {
      auto& segment = reader[1];  // assume 0 is id of first/only segment
      const auto* column = segment.column("name");
      ASSERT_NE(nullptr, column);
      auto values = column->iterator(irs::ColumnHint::kNormal);
      ASSERT_NE(nullptr, values);
      auto* actual_value = irs::get<irs::payload>(*values);
      ASSERT_NE(nullptr, actual_value);
      ASSERT_EQ(1, segment.docs_count());       // total count of documents
      ASSERT_EQ(1, segment.live_docs_count());  // total count of documents
      auto terms = segment.field("same");
      ASSERT_NE(nullptr, terms);
      auto termItr = terms->iterator(irs::SeekMode::NORMAL);
      ASSERT_TRUE(termItr->next());
      auto docsItr = termItr->postings(irs::IndexFeatures::NONE);
      ASSERT_TRUE(docsItr->next());
      ASSERT_EQ(docsItr->value(), values->seek(docsItr->value()));
      ASSERT_EQ("B", irs::to_string<std::string_view>(
                       actual_value->value.data()));  // 'name' value in doc3
      ASSERT_FALSE(docsItr->next());
    }

    // validate columnstore (segment 2)
    {
      auto& segment = reader[2];  // assume 0 is id of first/only segment
      const auto* column = segment.column("name");
      ASSERT_NE(nullptr, column);
      auto values = column->iterator(irs::ColumnHint::kNormal);
      ASSERT_NE(nullptr, values);
      auto* actual_value = irs::get<irs::payload>(*values);
      ASSERT_NE(nullptr, actual_value);
      ASSERT_EQ(1, segment.docs_count());       // total count of documents
      ASSERT_EQ(1, segment.live_docs_count());  // total count of documents
      auto terms = segment.field("same");
      ASSERT_NE(nullptr, terms);
      auto termItr = terms->iterator(irs::SeekMode::NORMAL);
      ASSERT_TRUE(termItr->next());
      auto docsItr = termItr->postings(irs::IndexFeatures::NONE);
      ASSERT_TRUE(docsItr->next());
      ASSERT_EQ(docsItr->value(), values->seek(docsItr->value()));
      ASSERT_EQ("C", irs::to_string<std::string_view>(
                       actual_value->value.data()));  // 'name' value in doc3
      ASSERT_FALSE(docsItr->next());
    }

    // validate columnstore (segment 3)
    {
      auto& segment = reader[3];  // assume 0 is id of first/only segment
      const auto* column = segment.column("name");
      ASSERT_NE(nullptr, column);
      auto values = column->iterator(irs::ColumnHint::kNormal);
      ASSERT_NE(nullptr, values);
      auto* actual_value = irs::get<irs::payload>(*values);
      ASSERT_NE(nullptr, actual_value);
      ASSERT_EQ(1, segment.docs_count());       // total count of documents
      ASSERT_EQ(1, segment.live_docs_count());  // total count of documents
      auto terms = segment.field("same");
      ASSERT_NE(nullptr, terms);
      auto termItr = terms->iterator(irs::SeekMode::NORMAL);
      ASSERT_TRUE(termItr->next());
      auto docsItr = termItr->postings(irs::IndexFeatures::NONE);
      ASSERT_TRUE(docsItr->next());
      ASSERT_EQ(docsItr->value(), values->seek(docsItr->value()));
      ASSERT_EQ("D", irs::to_string<std::string_view>(
                       actual_value->value.data()));  // 'name' value in doc3
      ASSERT_FALSE(docsItr->next());
    }
  }
}

TEST(index_death_test_formats_10,
     segment_meta_write_fail_long_running_consolidation) {
  tests::json_doc_generator gen(
    test_base::resource("simple_sequential.json"),
    [](tests::document& doc, const std::string& name,
       const tests::json_doc_generator::json_value& data) {
      if (data.is_string()) {
        doc.insert(std::make_shared<tests::string_field>(name, data.str));
      }
    });
  const auto* doc1 = gen.next();
  const auto* doc2 = gen.next();
  const auto* doc3 = gen.next();

  auto codec = irs::formats::get("1_0");
  ASSERT_NE(nullptr, codec);

  // segment meta creation failure
  {
    constexpr irs::IndexFeatures all_features =
      irs::IndexFeatures::FREQ | irs::IndexFeatures::POS |
      irs::IndexFeatures::OFFS | irs::IndexFeatures::PAY;

    irs::memory_directory impl;
    failing_directory failing_dir(impl);
    tests::blocking_directory dir(failing_dir, "_3.cs");

    // write index
    auto writer = irs::IndexWriter::Make(dir, codec, irs::OM_CREATE);
    ASSERT_NE(nullptr, writer);

    // segment 0
    ASSERT_TRUE(insert(*writer, doc1->indexed.begin(), doc1->indexed.end(),
                       doc1->stored.begin(), doc1->stored.end()));
    ASSERT_TRUE(writer->Commit());
    tests::AssertSnapshotEquality(writer->GetSnapshot(),
                                  irs::DirectoryReader(dir));

    // segment 1
    ASSERT_TRUE(insert(*writer, doc2->indexed.begin(), doc2->indexed.end(),
                       doc2->stored.begin(), doc2->stored.end()));
    ASSERT_TRUE(writer->Commit());
    tests::AssertSnapshotEquality(writer->GetSnapshot(),
                                  irs::DirectoryReader(dir));

    // register failures
    failing_dir.register_failure(
      failing_directory::Failure::CREATE,
      "_3.0.sm");  // fail at segment meta creation on consolidation

    dir.intermediate_commits_lock
      .lock();  // acquire directory lock, and block consolidation

    std::thread consolidation_thread([&writer]() {
      const irs::index_utils::ConsolidateCount consolidate_all;
      ASSERT_THROW(
        writer->Consolidate(irs::index_utils::MakePolicy(consolidate_all)),
        irs::io_error);  // consolidate
    });

    dir.wait_for_blocker();

    // add another segment
    ASSERT_TRUE(insert(*writer, doc3->indexed.begin(), doc3->indexed.end(),
                       doc3->stored.begin(), doc3->stored.end()));
    ASSERT_TRUE(writer->Commit());
    tests::AssertSnapshotEquality(writer->GetSnapshot(),
                                  irs::DirectoryReader(dir));

    dir.intermediate_commits_lock.unlock();  // finish consolidation
    consolidation_thread.join();  // wait for the consolidation to complete

    // check data
    auto reader = irs::DirectoryReader(dir);
    ASSERT_TRUE(reader);
    ASSERT_EQ(3, reader->size());
    ASSERT_EQ(3, reader->docs_count());
    ASSERT_EQ(3, reader->live_docs_count());

    // validate index
    tests::index_t expected_index;
    expected_index.emplace_back(writer->FeatureInfo());
    expected_index.back().insert(*doc1);
    expected_index.emplace_back(writer->FeatureInfo());
    expected_index.back().insert(*doc2);
    expected_index.emplace_back(writer->FeatureInfo());
    expected_index.back().insert(*doc3);
    tests::assert_index(reader.GetImpl(), expected_index, all_features);

    // validate columnstore (segment 0)
    {
      auto& segment = reader[0];  // assume 0 is id of first/only segment
      const auto* column = segment.column("name");
      ASSERT_NE(nullptr, column);
      auto values = column->iterator(irs::ColumnHint::kNormal);
      ASSERT_NE(nullptr, values);
      auto* actual_value = irs::get<irs::payload>(*values);
      ASSERT_NE(nullptr, actual_value);
      ASSERT_EQ(1, segment.docs_count());       // total count of documents
      ASSERT_EQ(1, segment.live_docs_count());  // total count of documents
      auto terms = segment.field("same");
      ASSERT_NE(nullptr, terms);
      auto termItr = terms->iterator(irs::SeekMode::NORMAL);
      ASSERT_TRUE(termItr->next());
      auto docsItr = termItr->postings(irs::IndexFeatures::NONE);
      ASSERT_TRUE(docsItr->next());
      ASSERT_EQ(docsItr->value(), values->seek(docsItr->value()));
      ASSERT_EQ("A", irs::to_string<std::string_view>(
                       actual_value->value.data()));  // 'name' value in doc3
      ASSERT_FALSE(docsItr->next());
    }

    // validate columnstore (segment 1)
    {
      auto& segment = reader[1];  // assume 0 is id of first/only segment
      const auto* column = segment.column("name");
      ASSERT_NE(nullptr, column);
      auto values = column->iterator(irs::ColumnHint::kNormal);
      ASSERT_NE(nullptr, values);
      auto* actual_value = irs::get<irs::payload>(*values);
      ASSERT_NE(nullptr, actual_value);
      ASSERT_EQ(1, segment.docs_count());       // total count of documents
      ASSERT_EQ(1, segment.live_docs_count());  // total count of documents
      auto terms = segment.field("same");
      ASSERT_NE(nullptr, terms);
      auto termItr = terms->iterator(irs::SeekMode::NORMAL);
      ASSERT_TRUE(termItr->next());
      auto docsItr = termItr->postings(irs::IndexFeatures::NONE);
      ASSERT_TRUE(docsItr->next());
      ASSERT_EQ(docsItr->value(), values->seek(docsItr->value()));
      ASSERT_EQ("B", irs::to_string<std::string_view>(
                       actual_value->value.data()));  // 'name' value in doc3
      ASSERT_FALSE(docsItr->next());
    }

    // validate columnstore (segment 2)
    {
      auto& segment = reader[2];  // assume 0 is id of first/only segment
      const auto* column = segment.column("name");
      ASSERT_NE(nullptr, column);
      auto values = column->iterator(irs::ColumnHint::kNormal);
      ASSERT_NE(nullptr, values);
      auto* actual_value = irs::get<irs::payload>(*values);
      ASSERT_NE(nullptr, actual_value);
      ASSERT_EQ(1, segment.docs_count());       // total count of documents
      ASSERT_EQ(1, segment.live_docs_count());  // total count of documents
      auto terms = segment.field("same");
      ASSERT_NE(nullptr, terms);
      auto termItr = terms->iterator(irs::SeekMode::NORMAL);
      ASSERT_TRUE(termItr->next());
      auto docsItr = termItr->postings(irs::IndexFeatures::NONE);
      ASSERT_TRUE(docsItr->next());
      ASSERT_EQ(docsItr->value(), values->seek(docsItr->value()));
      ASSERT_EQ("C", irs::to_string<std::string_view>(
                       actual_value->value.data()));  // 'name' value in doc3
      ASSERT_FALSE(docsItr->next());
    }
  }

  // segment meta synchonization failure
  {
    constexpr irs::IndexFeatures all_features =
      irs::IndexFeatures::FREQ | irs::IndexFeatures::POS |
      irs::IndexFeatures::OFFS | irs::IndexFeatures::PAY;

    irs::memory_directory impl;
    failing_directory failing_dir(impl);
    tests::blocking_directory dir(failing_dir, "_3.cs");

    // write index
    auto writer = irs::IndexWriter::Make(dir, codec, irs::OM_CREATE);
    ASSERT_NE(nullptr, writer);

    // segment 0
    ASSERT_TRUE(insert(*writer, doc1->indexed.begin(), doc1->indexed.end(),
                       doc1->stored.begin(), doc1->stored.end()));
    ASSERT_TRUE(writer->Commit());
    tests::AssertSnapshotEquality(writer->GetSnapshot(),
                                  irs::DirectoryReader(dir));

    // segment 1
    ASSERT_TRUE(insert(*writer, doc2->indexed.begin(), doc2->indexed.end(),
                       doc2->stored.begin(), doc2->stored.end()));
    ASSERT_TRUE(writer->Commit());
    tests::AssertSnapshotEquality(writer->GetSnapshot(),
                                  irs::DirectoryReader(dir));

    // register failures
    failing_dir.register_failure(
      failing_directory::Failure::SYNC,
      "_3.0.sm");  // fail at segment meta synchronization on consolidation

    dir.intermediate_commits_lock
      .lock();  // acquire directory lock, and block consolidation

    std::thread consolidation_thread([&writer]() {
      const irs::index_utils::ConsolidateCount consolidate_all;
      ASSERT_TRUE(writer->Consolidate(
        irs::index_utils::MakePolicy(consolidate_all)));  // consolidate
    });

    dir.wait_for_blocker();

    // add another segment
    ASSERT_TRUE(insert(*writer, doc3->indexed.begin(), doc3->indexed.end(),
                       doc3->stored.begin(), doc3->stored.end()));
    ASSERT_TRUE(writer->Commit());
    tests::AssertSnapshotEquality(writer->GetSnapshot(),
                                  irs::DirectoryReader(dir));

    dir.intermediate_commits_lock.unlock();  // finish consolidation
    consolidation_thread.join();  // wait for the consolidation to complete

    // commit consolidation
    ASSERT_THROW(writer->Begin(), irs::io_error);

    // check data
    auto reader = irs::DirectoryReader(dir);
    ASSERT_TRUE(reader);
    ASSERT_EQ(3, reader->size());
    ASSERT_EQ(3, reader->docs_count());
    ASSERT_EQ(3, reader->live_docs_count());

    // validate index
    tests::index_t expected_index;
    expected_index.emplace_back(writer->FeatureInfo());
    expected_index.back().insert(*doc1);
    expected_index.emplace_back(writer->FeatureInfo());
    expected_index.back().insert(*doc2);
    expected_index.emplace_back(writer->FeatureInfo());
    expected_index.back().insert(*doc3);
    tests::assert_index(reader.GetImpl(), expected_index, all_features);

    // validate columnstore (segment 0)
    {
      auto& segment = reader[0];  // assume 0 is id of first/only segment
      const auto* column = segment.column("name");
      ASSERT_NE(nullptr, column);
      auto values = column->iterator(irs::ColumnHint::kNormal);
      ASSERT_NE(nullptr, values);
      auto* actual_value = irs::get<irs::payload>(*values);
      ASSERT_NE(nullptr, actual_value);
      ASSERT_EQ(1, segment.docs_count());       // total count of documents
      ASSERT_EQ(1, segment.live_docs_count());  // total count of documents
      auto terms = segment.field("same");
      ASSERT_NE(nullptr, terms);
      auto termItr = terms->iterator(irs::SeekMode::NORMAL);
      ASSERT_TRUE(termItr->next());
      auto docsItr = termItr->postings(irs::IndexFeatures::NONE);
      ASSERT_TRUE(docsItr->next());
      ASSERT_EQ(docsItr->value(), values->seek(docsItr->value()));
      ASSERT_EQ("A", irs::to_string<std::string_view>(
                       actual_value->value.data()));  // 'name' value in doc3
      ASSERT_FALSE(docsItr->next());
    }

    // validate columnstore (segment 1)
    {
      auto& segment = reader[1];  // assume 0 is id of first/only segment
      const auto* column = segment.column("name");
      ASSERT_NE(nullptr, column);
      auto values = column->iterator(irs::ColumnHint::kNormal);
      ASSERT_NE(nullptr, values);
      auto* actual_value = irs::get<irs::payload>(*values);
      ASSERT_NE(nullptr, actual_value);
      ASSERT_EQ(1, segment.docs_count());       // total count of documents
      ASSERT_EQ(1, segment.live_docs_count());  // total count of documents
      auto terms = segment.field("same");
      ASSERT_NE(nullptr, terms);
      auto termItr = terms->iterator(irs::SeekMode::NORMAL);
      ASSERT_TRUE(termItr->next());
      auto docsItr = termItr->postings(irs::IndexFeatures::NONE);
      ASSERT_TRUE(docsItr->next());
      ASSERT_EQ(docsItr->value(), values->seek(docsItr->value()));
      ASSERT_EQ("B", irs::to_string<std::string_view>(
                       actual_value->value.data()));  // 'name' value in doc3
      ASSERT_FALSE(docsItr->next());
    }

    // validate columnstore (segment 2)
    {
      auto& segment = reader[2];  // assume 0 is id of first/only segment
      const auto* column = segment.column("name");
      ASSERT_NE(nullptr, column);
      auto values = column->iterator(irs::ColumnHint::kNormal);
      ASSERT_NE(nullptr, values);
      auto* actual_value = irs::get<irs::payload>(*values);
      ASSERT_NE(nullptr, actual_value);
      ASSERT_EQ(1, segment.docs_count());       // total count of documents
      ASSERT_EQ(1, segment.live_docs_count());  // total count of documents
      auto terms = segment.field("same");
      ASSERT_NE(nullptr, terms);
      auto termItr = terms->iterator(irs::SeekMode::NORMAL);
      ASSERT_TRUE(termItr->next());
      auto docsItr = termItr->postings(irs::IndexFeatures::NONE);
      ASSERT_TRUE(docsItr->next());
      ASSERT_EQ(docsItr->value(), values->seek(docsItr->value()));
      ASSERT_EQ("C", irs::to_string<std::string_view>(
                       actual_value->value.data()));  // 'name' value in doc3
      ASSERT_FALSE(docsItr->next());
    }
  }
}

TEST(index_death_test_formats_10, segment_components_write_fail_consolidation) {
  tests::json_doc_generator gen(test_base::resource("simple_sequential.json"),
                                &tests::payloaded_json_field_factory);
  const auto* doc1 = gen.next();
  const auto* doc2 = gen.next();

  auto codec = irs::formats::get("1_0");
  ASSERT_NE(nullptr, codec);

  {
    constexpr irs::IndexFeatures all_features =
      irs::IndexFeatures::FREQ | irs::IndexFeatures::POS |
      irs::IndexFeatures::OFFS | irs::IndexFeatures::PAY;

    irs::memory_directory impl;
    failing_directory dir(impl);

    // write index
    auto writer = irs::IndexWriter::Make(dir, codec, irs::OM_CREATE);
    ASSERT_NE(nullptr, writer);

    // segment 0
    ASSERT_TRUE(insert(*writer, doc1->indexed.begin(), doc1->indexed.end(),
                       doc1->stored.begin(), doc1->stored.end()));
    ASSERT_TRUE(writer->Commit());
    tests::AssertSnapshotEquality(writer->GetSnapshot(),
                                  irs::DirectoryReader(dir));

    // segment 1
    ASSERT_TRUE(insert(*writer, doc2->indexed.begin(), doc2->indexed.end(),
                       doc2->stored.begin(), doc2->stored.end()));
    ASSERT_TRUE(writer->Commit());
    tests::AssertSnapshotEquality(writer->GetSnapshot(),
                                  irs::DirectoryReader(dir));

    // register failures
    dir.register_failure(failing_directory::Failure::CREATE,
                         "_3.doc");  // postings list (documents)
    dir.register_failure(failing_directory::Failure::CREATE,
                         "_4.cm");  // column meta
    dir.register_failure(failing_directory::Failure::CREATE,
                         "_5.ti");  // term index
    dir.register_failure(failing_directory::Failure::CREATE,
                         "_6.tm");  // term data
    dir.register_failure(failing_directory::Failure::CREATE,
                         "_7.pos");  // postings list (positions)
    dir.register_failure(failing_directory::Failure::CREATE,
                         "_8.pay");  // postings list (offset + payload)

    const irs::index_utils::ConsolidateCount consolidate_all;

    while (!dir.no_failures()) {
      ASSERT_THROW(
        writer->Consolidate(irs::index_utils::MakePolicy(consolidate_all)),
        irs::io_error);
      ASSERT_FALSE(writer->Begin());  // nothing to flush
    }

    // check data
    auto reader = irs::DirectoryReader(dir);
    ASSERT_TRUE(reader);
    ASSERT_EQ(2, reader->size());
    ASSERT_EQ(2, reader->docs_count());
    ASSERT_EQ(2, reader->live_docs_count());

    // validate index
    tests::index_t expected_index;
    expected_index.emplace_back(writer->FeatureInfo());
    expected_index.back().insert(*doc1);
    expected_index.emplace_back(writer->FeatureInfo());
    expected_index.back().insert(*doc2);
    tests::assert_index(reader.GetImpl(), expected_index, all_features);

    // validate columnstore (segment 0)
    {
      auto& segment = reader[0];  // assume 0 is id of first/only segment
      const auto* column = segment.column("name");
      ASSERT_NE(nullptr, column);
      auto values = column->iterator(irs::ColumnHint::kNormal);
      ASSERT_NE(nullptr, values);
      auto* actual_value = irs::get<irs::payload>(*values);
      ASSERT_NE(nullptr, actual_value);
      ASSERT_EQ(1, segment.docs_count());       // total count of documents
      ASSERT_EQ(1, segment.live_docs_count());  // total count of documents
      auto terms = segment.field("same");
      ASSERT_NE(nullptr, terms);
      auto termItr = terms->iterator(irs::SeekMode::NORMAL);
      ASSERT_TRUE(termItr->next());
      auto docsItr = termItr->postings(irs::IndexFeatures::NONE);
      ASSERT_TRUE(docsItr->next());
      ASSERT_EQ(docsItr->value(), values->seek(docsItr->value()));
      ASSERT_EQ("A", irs::to_string<std::string_view>(
                       actual_value->value.data()));  // 'name' value in doc3
      ASSERT_FALSE(docsItr->next());
    }

    // validate columnstore (segment 1)
    {
      auto& segment = reader[1];  // assume 0 is id of first/only segment
      const auto* column = segment.column("name");
      ASSERT_NE(nullptr, column);
      auto values = column->iterator(irs::ColumnHint::kNormal);
      ASSERT_NE(nullptr, values);
      auto* actual_value = irs::get<irs::payload>(*values);
      ASSERT_NE(nullptr, actual_value);
      ASSERT_EQ(1, segment.docs_count());       // total count of documents
      ASSERT_EQ(1, segment.live_docs_count());  // total count of documents
      auto terms = segment.field("same");
      ASSERT_NE(nullptr, terms);
      auto termItr = terms->iterator(irs::SeekMode::NORMAL);
      ASSERT_TRUE(termItr->next());
      auto docsItr = termItr->postings(irs::IndexFeatures::NONE);
      ASSERT_TRUE(docsItr->next());
      ASSERT_EQ(docsItr->value(), values->seek(docsItr->value()));
      ASSERT_EQ("B", irs::to_string<std::string_view>(
                       actual_value->value.data()));  // 'name' value in doc3
      ASSERT_FALSE(docsItr->next());
    }
  }
}

TEST(index_death_test_formats_10, segment_components_sync_fail_consolidation) {
  tests::json_doc_generator gen(test_base::resource("simple_sequential.json"),
                                &tests::payloaded_json_field_factory);
  const auto* doc1 = gen.next();
  const auto* doc2 = gen.next();

  auto codec = irs::formats::get("1_0");
  ASSERT_NE(nullptr, codec);

  {
    constexpr irs::IndexFeatures all_features =
      irs::IndexFeatures::FREQ | irs::IndexFeatures::POS |
      irs::IndexFeatures::OFFS | irs::IndexFeatures::PAY;

    irs::memory_directory impl;
    failing_directory dir(impl);

    // write index
    auto writer = irs::IndexWriter::Make(dir, codec, irs::OM_CREATE);
    ASSERT_NE(nullptr, writer);

    // segment 0
    ASSERT_TRUE(insert(*writer, doc1->indexed.begin(), doc1->indexed.end(),
                       doc1->stored.begin(), doc1->stored.end()));
    ASSERT_TRUE(writer->Commit());
    tests::AssertSnapshotEquality(writer->GetSnapshot(),
                                  irs::DirectoryReader(dir));

    // segment 1
    ASSERT_TRUE(insert(*writer, doc2->indexed.begin(), doc2->indexed.end(),
                       doc2->stored.begin(), doc2->stored.end()));
    ASSERT_TRUE(writer->Commit());
    tests::AssertSnapshotEquality(writer->GetSnapshot(),
                                  irs::DirectoryReader(dir));

    // register failures
    dir.register_failure(failing_directory::Failure::SYNC,
                         "_3.doc");  // postings list (documents)
    dir.register_failure(failing_directory::Failure::SYNC,
                         "_4.cm");  // column meta
    dir.register_failure(failing_directory::Failure::SYNC,
                         "_5.ti");  // term index
    dir.register_failure(failing_directory::Failure::SYNC,
                         "_6.tm");  // term data
    dir.register_failure(failing_directory::Failure::SYNC,
                         "_7.pos");  // postings list (positions)
    dir.register_failure(failing_directory::Failure::SYNC,
                         "_8.pay");  // postings list (offset + payload)

    const irs::index_utils::ConsolidateCount consolidate_all;

    while (!dir.no_failures()) {
      ASSERT_TRUE(
        writer->Consolidate(irs::index_utils::MakePolicy(consolidate_all)));
      ASSERT_THROW(writer->Begin(), irs::io_error);  // nothing to flush
      ASSERT_FALSE(writer->Begin());
    }

    // check data
    auto reader = irs::DirectoryReader(dir);
    ASSERT_TRUE(reader);
    ASSERT_EQ(2, reader->size());
    ASSERT_EQ(2, reader->docs_count());
    ASSERT_EQ(2, reader->live_docs_count());

    // validate index
    tests::index_t expected_index;
    expected_index.emplace_back(writer->FeatureInfo());
    expected_index.back().insert(*doc1);
    expected_index.emplace_back(writer->FeatureInfo());
    expected_index.back().insert(*doc2);
    tests::assert_index(reader.GetImpl(), expected_index, all_features);

    // validate columnstore (segment 0)
    {
      auto& segment = reader[0];  // assume 0 is id of first/only segment
      const auto* column = segment.column("name");
      ASSERT_NE(nullptr, column);
      auto values = column->iterator(irs::ColumnHint::kNormal);
      ASSERT_NE(nullptr, values);
      auto* actual_value = irs::get<irs::payload>(*values);
      ASSERT_NE(nullptr, actual_value);
      ASSERT_EQ(1, segment.docs_count());       // total count of documents
      ASSERT_EQ(1, segment.live_docs_count());  // total count of documents
      auto terms = segment.field("same");
      ASSERT_NE(nullptr, terms);
      auto termItr = terms->iterator(irs::SeekMode::NORMAL);
      ASSERT_TRUE(termItr->next());
      auto docsItr = termItr->postings(irs::IndexFeatures::NONE);
      ASSERT_TRUE(docsItr->next());
      ASSERT_EQ(docsItr->value(), values->seek(docsItr->value()));
      ASSERT_EQ("A", irs::to_string<std::string_view>(
                       actual_value->value.data()));  // 'name' value in doc3
      ASSERT_FALSE(docsItr->next());
    }

    // validate columnstore (segment 1)
    {
      auto& segment = reader[1];  // assume 0 is id of first/only segment
      const auto* column = segment.column("name");
      ASSERT_NE(nullptr, column);
      auto values = column->iterator(irs::ColumnHint::kNormal);
      ASSERT_NE(nullptr, values);
      auto* actual_value = irs::get<irs::payload>(*values);
      ASSERT_NE(nullptr, actual_value);
      ASSERT_EQ(1, segment.docs_count());       // total count of documents
      ASSERT_EQ(1, segment.live_docs_count());  // total count of documents
      auto terms = segment.field("same");
      ASSERT_NE(nullptr, terms);
      auto termItr = terms->iterator(irs::SeekMode::NORMAL);
      ASSERT_TRUE(termItr->next());
      auto docsItr = termItr->postings(irs::IndexFeatures::NONE);
      ASSERT_TRUE(docsItr->next());
      ASSERT_EQ(docsItr->value(), values->seek(docsItr->value()));
      ASSERT_EQ("B", irs::to_string<std::string_view>(
                       actual_value->value.data()));  // 'name' value in doc3
      ASSERT_FALSE(docsItr->next());
    }
  }
}

TEST(index_death_test_formats_10, segment_components_fail_import) {
  constexpr irs::IndexFeatures all_features =
    irs::IndexFeatures::FREQ | irs::IndexFeatures::POS |
    irs::IndexFeatures::OFFS | irs::IndexFeatures::PAY;

  tests::json_doc_generator gen(test_base::resource("simple_sequential.json"),
                                &tests::payloaded_json_field_factory);
  const auto* doc1 = gen.next();

  auto codec = irs::formats::get("1_0");
  ASSERT_NE(nullptr, codec);

  // create source segment
  irs::memory_directory src_dir;

  {
    // write index
    auto writer = irs::IndexWriter::Make(src_dir, codec, irs::OM_CREATE);
    ASSERT_NE(nullptr, writer);

    ASSERT_TRUE(insert(*writer, doc1->indexed.begin(), doc1->indexed.end(),
                       doc1->stored.begin(), doc1->stored.end()));

    ASSERT_TRUE(writer->Commit());
    tests::AssertSnapshotEquality(writer->GetSnapshot(),
                                  irs::DirectoryReader(src_dir));
  }

  auto src_index = irs::DirectoryReader(src_dir);
  ASSERT_TRUE(src_index);

  // file creation failures
  {
    irs::memory_directory impl;
    failing_directory dir(impl);

    // register failures
    dir.register_failure(failing_directory::Failure::CREATE,
                         "_1.doc");  // postings list (documents)
    dir.register_failure(failing_directory::Failure::CREATE,
                         "_2.doc");  // postings list (documents)
    dir.register_failure(failing_directory::Failure::CREATE,
                         "_3.cm");  // column meta
    dir.register_failure(failing_directory::Failure::CREATE,
                         "_4.ti");  // term index
    dir.register_failure(failing_directory::Failure::CREATE,
                         "_5.tm");  // term data
    dir.register_failure(failing_directory::Failure::CREATE,
                         "_6.pos");  // postings list (positions)
    dir.register_failure(failing_directory::Failure::CREATE,
                         "_7.pay");  // postings list (offset + payload)
    dir.register_failure(failing_directory::Failure::CREATE,
                         "_8.cs");  // columnstore
    dir.register_failure(failing_directory::Failure::CREATE,
                         "_9.0.sm");  // segment meta

    // write index
    auto writer = irs::IndexWriter::Make(dir, codec, irs::OM_CREATE);
    ASSERT_NE(nullptr, writer);

    // initial commit
    ASSERT_TRUE(writer->Begin());
    ASSERT_FALSE(writer->Commit());
    tests::AssertSnapshotEquality(writer->GetSnapshot(),
                                  irs::DirectoryReader(dir));

    while (!dir.no_failures()) {
      ASSERT_THROW(writer->Import(*src_index), irs::io_error);
      ASSERT_FALSE(writer->Begin());  // nothing to commit
    }

    // check data
    auto reader = irs::DirectoryReader(dir);
    ASSERT_TRUE(reader);
    ASSERT_EQ(0, reader->size());
    ASSERT_EQ(0, reader->docs_count());
    ASSERT_EQ(0, reader->live_docs_count());
  }

  // file creation failures
  {
    irs::memory_directory impl;
    failing_directory dir(impl);

    // register failures
    dir.register_failure(failing_directory::Failure::CREATE,
                         "_1.doc");  // postings list (documents)
    dir.register_failure(failing_directory::Failure::CREATE,
                         "_2.doc");  // postings list (documents)
    dir.register_failure(failing_directory::Failure::CREATE,
                         "_3.cm");  // column meta
    dir.register_failure(failing_directory::Failure::CREATE,
                         "_4.ti");  // term index
    dir.register_failure(failing_directory::Failure::CREATE,
                         "_5.tm");  // term data
    dir.register_failure(failing_directory::Failure::CREATE,
                         "_6.pos");  // postings list (positions)
    dir.register_failure(failing_directory::Failure::CREATE,
                         "_7.pay");  // postings list (offset + payload)
    dir.register_failure(failing_directory::Failure::CREATE,
                         "_8.cs");  // columnstore
    dir.register_failure(failing_directory::Failure::CREATE,
                         "_9.0.sm");  // segment meta

    // write index
    auto writer = irs::IndexWriter::Make(dir, codec, irs::OM_CREATE);
    ASSERT_NE(nullptr, writer);

    // initial commit
    ASSERT_TRUE(writer->Begin());
    ASSERT_FALSE(writer->Commit());
    tests::AssertSnapshotEquality(writer->GetSnapshot(),
                                  irs::DirectoryReader(dir));

    while (!dir.no_failures()) {
      ASSERT_THROW(writer->Import(*src_index), irs::io_error);
      ASSERT_FALSE(writer->Begin());  // nothing to commit
    }

    // successful commit
    ASSERT_TRUE(writer->Import(*src_index));
    ASSERT_TRUE(writer->Begin());
    ASSERT_FALSE(writer->Commit());
    tests::AssertSnapshotEquality(writer->GetSnapshot(),
                                  irs::DirectoryReader(dir));

    // check data
    auto reader = irs::DirectoryReader(dir);
    ASSERT_TRUE(reader);
    ASSERT_EQ(1, reader->size());
    ASSERT_EQ(1, reader->docs_count());
    ASSERT_EQ(1, reader->live_docs_count());

    // validate index
    tests::index_t expected_index;
    expected_index.emplace_back(writer->FeatureInfo());
    expected_index.back().insert(*doc1);
    tests::assert_index(reader.GetImpl(), expected_index, all_features);

    // validate columnstore (segment 0)
    {
      auto& segment = reader[0];  // assume 0 is id of first/only segment
      const auto* column = segment.column("name");
      ASSERT_NE(nullptr, column);
      auto values = column->iterator(irs::ColumnHint::kNormal);
      ASSERT_NE(nullptr, values);
      auto* actual_value = irs::get<irs::payload>(*values);
      ASSERT_NE(nullptr, actual_value);
      ASSERT_EQ(1, segment.docs_count());       // total count of documents
      ASSERT_EQ(1, segment.live_docs_count());  // total count of documents
      auto terms = segment.field("same");
      ASSERT_NE(nullptr, terms);
      auto termItr = terms->iterator(irs::SeekMode::NORMAL);
      ASSERT_TRUE(termItr->next());
      auto docsItr = termItr->postings(irs::IndexFeatures::NONE);
      ASSERT_TRUE(docsItr->next());
      ASSERT_EQ(docsItr->value(), values->seek(docsItr->value()));
      ASSERT_EQ("A", irs::to_string<std::string_view>(
                       actual_value->value.data()));  // 'name' value in doc3
      ASSERT_FALSE(docsItr->next());
    }
  }

  // file synchronization failures
  {
    irs::memory_directory impl;
    failing_directory dir(impl);

    // register failures
    dir.register_failure(failing_directory::Failure::SYNC,
                         "_1.doc");  // postings list (documents)
    dir.register_failure(failing_directory::Failure::SYNC,
                         "_2.doc");  // postings list (documents)
    dir.register_failure(failing_directory::Failure::SYNC,
                         "_3.cm");  // column meta
    dir.register_failure(failing_directory::Failure::SYNC,
                         "_4.ti");  // term index
    dir.register_failure(failing_directory::Failure::SYNC,
                         "_5.tm");  // term data
    dir.register_failure(failing_directory::Failure::SYNC,
                         "_6.pos");  // postings list (positions)
    dir.register_failure(failing_directory::Failure::SYNC,
                         "_7.pay");  // postings list (offset + payload)
    dir.register_failure(failing_directory::Failure::SYNC,
                         "_8.cs");  // columnstore
    dir.register_failure(failing_directory::Failure::SYNC,
                         "_9.0.sm");  // segment meta

    // write index
    auto writer = irs::IndexWriter::Make(dir, codec, irs::OM_CREATE);
    ASSERT_NE(nullptr, writer);

    // initial commit
    ASSERT_TRUE(writer->Begin());
    ASSERT_FALSE(writer->Commit());
    tests::AssertSnapshotEquality(writer->GetSnapshot(),
                                  irs::DirectoryReader(dir));

    while (!dir.no_failures()) {
      ASSERT_TRUE(writer->Import(*src_index));
      ASSERT_THROW(writer->Begin(), irs::io_error);  // nothing to commit
    }

    // check data
    auto reader = irs::DirectoryReader(dir);
    ASSERT_TRUE(reader);
    ASSERT_EQ(0, reader->size());
    ASSERT_EQ(0, reader->docs_count());
    ASSERT_EQ(0, reader->live_docs_count());
  }

  // file synchronization failures
  {
    irs::memory_directory impl;
    failing_directory dir(impl);

    // register failures
    dir.register_failure(failing_directory::Failure::SYNC,
                         "_1.doc");  // postings list (documents)
    dir.register_failure(failing_directory::Failure::SYNC,
                         "_2.doc");  // postings list (documents)
    dir.register_failure(failing_directory::Failure::SYNC,
                         "_3.cm");  // column meta
    dir.register_failure(failing_directory::Failure::SYNC,
                         "_4.ti");  // term index
    dir.register_failure(failing_directory::Failure::SYNC,
                         "_5.tm");  // term data
    dir.register_failure(failing_directory::Failure::SYNC,
                         "_6.pos");  // postings list (positions)
    dir.register_failure(failing_directory::Failure::SYNC,
                         "_7.pay");  // postings list (offset + payload)
    dir.register_failure(failing_directory::Failure::SYNC,
                         "_8.cs");  // columnstore
    dir.register_failure(failing_directory::Failure::SYNC,
                         "_9.0.sm");  // segment meta

    // write index
    auto writer = irs::IndexWriter::Make(dir, codec, irs::OM_CREATE);
    ASSERT_NE(nullptr, writer);

    // initial commit
    ASSERT_TRUE(writer->Begin());
    ASSERT_FALSE(writer->Commit());
    tests::AssertSnapshotEquality(writer->GetSnapshot(),
                                  irs::DirectoryReader(dir));

    while (!dir.no_failures()) {
      ASSERT_TRUE(writer->Import(*src_index));
      ASSERT_THROW(writer->Begin(), irs::io_error);  // nothing to commit
    }

    // successful commit
    ASSERT_TRUE(writer->Import(*src_index));
    ASSERT_TRUE(writer->Begin());
    ASSERT_FALSE(writer->Commit());
    tests::AssertSnapshotEquality(writer->GetSnapshot(),
                                  irs::DirectoryReader(dir));

    // check data
    auto reader = irs::DirectoryReader(dir);
    ASSERT_TRUE(reader);
    ASSERT_EQ(1, reader->size());
    ASSERT_EQ(1, reader->docs_count());
    ASSERT_EQ(1, reader->live_docs_count());

    // validate index
    tests::index_t expected_index;
    expected_index.emplace_back(writer->FeatureInfo());
    expected_index.back().insert(*doc1);
    tests::assert_index(reader.GetImpl(), expected_index, all_features);

    // validate columnstore (segment 0)
    {
      auto& segment = reader[0];  // assume 0 is id of first/only segment
      const auto* column = segment.column("name");
      ASSERT_NE(nullptr, column);
      auto values = column->iterator(irs::ColumnHint::kNormal);
      ASSERT_NE(nullptr, values);
      auto* actual_value = irs::get<irs::payload>(*values);
      ASSERT_NE(nullptr, actual_value);
      ASSERT_EQ(1, segment.docs_count());       // total count of documents
      ASSERT_EQ(1, segment.live_docs_count());  // total count of documents
      auto terms = segment.field("same");
      ASSERT_NE(nullptr, terms);
      auto termItr = terms->iterator(irs::SeekMode::NORMAL);
      ASSERT_TRUE(termItr->next());
      auto docsItr = termItr->postings(irs::IndexFeatures::NONE);
      ASSERT_TRUE(docsItr->next());
      ASSERT_EQ(docsItr->value(), values->seek(docsItr->value()));
      ASSERT_EQ("A", irs::to_string<std::string_view>(
                       actual_value->value.data()));  // 'name' value in doc3
      ASSERT_FALSE(docsItr->next());
    }
  }
}

TEST(index_death_test_formats_10,
     segment_components_creation_fail_implicit_segment_flush) {
  constexpr irs::IndexFeatures all_features =
    irs::IndexFeatures::FREQ | irs::IndexFeatures::POS |
    irs::IndexFeatures::OFFS | irs::IndexFeatures::PAY;

  tests::json_doc_generator gen(test_base::resource("simple_sequential.json"),
                                &tests::payloaded_json_field_factory);
  const auto* doc1 = gen.next();
  const auto* doc2 = gen.next();
  const auto* doc3 = gen.next();

  auto codec = irs::formats::get("1_0");
  ASSERT_NE(nullptr, codec);

  size_t i{};
  // file creation failures
  {
    irs::memory_directory impl;
    failing_directory dir(impl);

    // register failures
    dir.register_failure(failing_directory::Failure::CREATE,
                         "_1.doc");  // postings list (documents)
    dir.register_failure(failing_directory::Failure::CREATE,
                         "_2.doc");  // postings list (documents)
    dir.register_failure(failing_directory::Failure::CREATE,
                         "_3.cm");  // column meta
    dir.register_failure(failing_directory::Failure::CREATE,
                         "_4.ti");  // term index
    dir.register_failure(failing_directory::Failure::CREATE,
                         "_5.tm");  // term data
    dir.register_failure(failing_directory::Failure::CREATE,
                         "_6.pos");  // postings list (positions)
    dir.register_failure(failing_directory::Failure::CREATE,
                         "_7.pay");  // postings list (offset + payload)
    dir.register_failure(failing_directory::Failure::CREATE,
                         "_8.0.sm");  // segment meta

    // write index
    irs::IndexWriterOptions opts;
    opts.segment_docs_max = 1;  // flush every 2nd document

    auto writer = irs::IndexWriter::Make(dir, codec, irs::OM_CREATE, opts);
    ASSERT_NE(nullptr, writer);

    // initial commit
    ASSERT_TRUE(writer->Begin());
    ASSERT_FALSE(writer->Commit());
    tests::AssertSnapshotEquality(writer->GetSnapshot(),
                                  irs::DirectoryReader(dir));
    i = 1;
    while (!dir.no_failures()) {
      ASSERT_TRUE(insert(*writer, doc1->indexed.begin(), doc1->indexed.end(),
                         doc1->stored.begin(), doc1->stored.end()));
      if (i++ == 8) {
        ASSERT_TRUE(insert(*writer, doc2->indexed.begin(), doc2->indexed.end(),
                           doc2->stored.begin(), doc2->stored.end()));
        ASSERT_THROW(writer->Begin(), irs::io_error);
      } else {
        ASSERT_THROW(insert(*writer, doc2->indexed.begin(), doc2->indexed.end(),
                            doc2->stored.begin(), doc2->stored.end()),
                     irs::io_error);
      }
      ASSERT_FALSE(writer->Begin());  // nothing to commit
    }

    // check data
    auto reader = irs::DirectoryReader(dir);
    ASSERT_TRUE(reader);
    ASSERT_EQ(0, reader->size());
    ASSERT_EQ(0, reader->docs_count());
    ASSERT_EQ(0, reader->live_docs_count());
  }

  // file creation failures
  {
    irs::memory_directory impl;
    failing_directory dir(impl);

    // register failures
    dir.register_failure(failing_directory::Failure::CREATE,
                         "_1.doc");  // postings list (documents)
    dir.register_failure(failing_directory::Failure::CREATE,
                         "_2.doc");  // postings list (documents)
    dir.register_failure(failing_directory::Failure::CREATE,
                         "_3.cm");  // column meta
    dir.register_failure(failing_directory::Failure::CREATE,
                         "_4.ti");  // term index
    dir.register_failure(failing_directory::Failure::CREATE,
                         "_5.tm");  // term data
    dir.register_failure(failing_directory::Failure::CREATE,
                         "_6.pos");  // postings list (positions)
    dir.register_failure(failing_directory::Failure::CREATE,
                         "_7.pay");  // postings list (offset + payload)
    dir.register_failure(failing_directory::Failure::CREATE,
                         "_8.0.sm");  // segment meta

    // write index
    irs::IndexWriterOptions opts;
    opts.segment_docs_max = 1;  // flush every 2nd document

    auto writer = irs::IndexWriter::Make(dir, codec, irs::OM_CREATE, opts);
    ASSERT_NE(nullptr, writer);

    // initial commit
    ASSERT_TRUE(writer->Begin());
    ASSERT_FALSE(writer->Commit());
    tests::AssertSnapshotEquality(writer->GetSnapshot(),
                                  irs::DirectoryReader(dir));

    i = 1;
    while (!dir.no_failures()) {
      ASSERT_TRUE(insert(*writer, doc1->indexed.begin(), doc1->indexed.end(),
                         doc1->stored.begin(), doc1->stored.end()));
      if (i++ == 8) {
        ASSERT_TRUE(insert(*writer, doc2->indexed.begin(), doc2->indexed.end(),
                           doc2->stored.begin(), doc2->stored.end()));
        ASSERT_THROW(writer->Begin(), irs::io_error);
      } else {
        ASSERT_THROW(insert(*writer, doc2->indexed.begin(), doc2->indexed.end(),
                            doc2->stored.begin(), doc2->stored.end()),
                     irs::io_error);
      }

      ASSERT_FALSE(writer->Begin());  // nothing to commit
    }

    ASSERT_TRUE(insert(*writer, doc3->indexed.begin(), doc3->indexed.end(),
                       doc3->stored.begin(), doc3->stored.end()));

    ASSERT_TRUE(writer->Begin());
    ASSERT_FALSE(writer->Commit());
    tests::AssertSnapshotEquality(writer->GetSnapshot(),
                                  irs::DirectoryReader(dir));

    // check data
    auto reader = irs::DirectoryReader(dir);
    ASSERT_TRUE(reader);
    ASSERT_EQ(1, reader->size());
    ASSERT_EQ(1, reader->docs_count());
    ASSERT_EQ(1, reader->live_docs_count());

    // validate index
    tests::index_t expected_index;
    expected_index.emplace_back(writer->FeatureInfo());
    expected_index.back().insert(*doc3);
    tests::assert_index(reader.GetImpl(), expected_index, all_features);

    // validate columnstore
    auto& segment = reader[0];  // assume 0 is id of first/only segment
    const auto* column = segment.column("name");
    ASSERT_NE(nullptr, column);
    auto values = column->iterator(irs::ColumnHint::kNormal);
    ASSERT_NE(nullptr, values);
    auto* actual_value = irs::get<irs::payload>(*values);
    ASSERT_NE(nullptr, actual_value);
    ASSERT_EQ(1, segment.docs_count());       // total count of documents
    ASSERT_EQ(1, segment.live_docs_count());  // total count of documents
    auto terms = segment.field("same");
    ASSERT_NE(nullptr, terms);
    auto termItr = terms->iterator(irs::SeekMode::NORMAL);
    ASSERT_TRUE(termItr->next());
    auto docsItr = termItr->postings(irs::IndexFeatures::NONE);
    ASSERT_TRUE(docsItr->next());
    ASSERT_EQ(docsItr->value(), values->seek(docsItr->value()));
    ASSERT_EQ("C", irs::to_string<std::string_view>(
                     actual_value->value.data()));  // 'name' value in doc3
    ASSERT_FALSE(docsItr->next());
  }
}

TEST(index_death_test_formats_10,
     columnstore_creation_fail_implicit_segment_flush) {
  constexpr irs::IndexFeatures all_features =
    irs::IndexFeatures::FREQ | irs::IndexFeatures::POS |
    irs::IndexFeatures::OFFS | irs::IndexFeatures::PAY;

  tests::json_doc_generator gen(test_base::resource("simple_sequential.json"),
                                &tests::payloaded_json_field_factory);
  const auto* doc1 = gen.next();
  const auto* doc2 = gen.next();

  auto codec = irs::formats::get("1_0");
  ASSERT_NE(nullptr, codec);

  // columnstore creation failure
  {
    irs::memory_directory impl;
    failing_directory dir(impl);

    // write index
    irs::IndexWriterOptions opts;
    opts.segment_docs_max = 1;  // flush every 2nd document

    auto writer = irs::IndexWriter::Make(dir, codec, irs::OM_CREATE, opts);
    ASSERT_NE(nullptr, writer);

    // initial commit
    ASSERT_TRUE(writer->Begin());
    ASSERT_FALSE(writer->Commit());
    tests::AssertSnapshotEquality(writer->GetSnapshot(),
                                  irs::DirectoryReader(dir));

    ASSERT_TRUE(insert(*writer, doc1->indexed.begin(), doc1->indexed.end(),
                       doc1->stored.begin(), doc1->stored.end()));

    dir.register_failure(failing_directory::Failure::CREATE,
                         "_2.cs");  // columnstore creation failure

    ASSERT_THROW(insert(*writer, doc2->indexed.begin(), doc2->indexed.end(),
                        doc2->stored.begin(), doc2->stored.end()),
                 irs::io_error);

    ASSERT_TRUE(writer->Begin());  // nothing to commit
    ASSERT_FALSE(writer->Commit());
    tests::AssertSnapshotEquality(writer->GetSnapshot(),
                                  irs::DirectoryReader(dir));

    // check data
    auto reader = irs::DirectoryReader(dir);
    ASSERT_TRUE(reader);
    ASSERT_EQ(1, reader->size());
    ASSERT_EQ(1, reader->docs_count());
    ASSERT_EQ(1, reader->live_docs_count());

    // validate index
    tests::index_t expected_index;
    expected_index.emplace_back(writer->FeatureInfo());
    expected_index.back().insert(*doc1);
    tests::assert_index(reader.GetImpl(), expected_index, all_features);

    // validate columnstore
    auto& segment = reader[0];  // assume 0 is id of first/only segment
    const auto* column = segment.column("name");
    ASSERT_NE(nullptr, column);
    auto values = column->iterator(irs::ColumnHint::kNormal);
    ASSERT_NE(nullptr, values);
    auto* actual_value = irs::get<irs::payload>(*values);
    ASSERT_NE(nullptr, actual_value);
    ASSERT_EQ(1, segment.docs_count());       // total count of documents
    ASSERT_EQ(1, segment.live_docs_count());  // total count of documents
    auto terms = segment.field("same");
    ASSERT_NE(nullptr, terms);
    auto termItr = terms->iterator(irs::SeekMode::NORMAL);
    ASSERT_TRUE(termItr->next());
    auto docsItr = termItr->postings(irs::IndexFeatures::NONE);
    ASSERT_TRUE(docsItr->next());
    ASSERT_EQ(docsItr->value(), values->seek(docsItr->value()));
    ASSERT_EQ("A", irs::to_string<std::string_view>(
                     actual_value->value.data()));  // 'name' value in doc3
    ASSERT_FALSE(docsItr->next());
  }
}

TEST(index_death_test_formats_14,
     columnstore_creation_fail_implicit_segment_flush) {
  constexpr irs::IndexFeatures all_features =
    irs::IndexFeatures::FREQ | irs::IndexFeatures::POS |
    irs::IndexFeatures::OFFS | irs::IndexFeatures::PAY;

  tests::json_doc_generator gen(test_base::resource("simple_sequential.json"),
                                &tests::payloaded_json_field_factory);
  const auto* doc1 = gen.next();
  const auto* doc2 = gen.next();

  auto codec = irs::formats::get("1_4");
  ASSERT_NE(nullptr, codec);

  // columnstore creation failure
  {
    irs::memory_directory impl;
    failing_directory dir(impl);

    // write index
    irs::IndexWriterOptions opts;
    opts.segment_docs_max = 1;  // flush every 2nd document

    auto writer = irs::IndexWriter::Make(dir, codec, irs::OM_CREATE, opts);
    ASSERT_NE(nullptr, writer);

    uint64_t length;

    // initial commit
    ASSERT_TRUE(writer->Begin());
    ASSERT_FALSE(writer->Commit());
    tests::AssertSnapshotEquality(writer->GetSnapshot(),
                                  irs::DirectoryReader(dir));

    ASSERT_TRUE(insert(*writer, doc1->indexed.begin(), doc1->indexed.end(),
                       doc1->stored.begin(), doc1->stored.end()));

    // basic length check
    dir.length(length, "_1.csd");
    ASSERT_EQ(length, 0);

    dir.register_failure(failing_directory::Failure::CREATE,
                         "_2.csd");  // columnstore data creation failure

    ASSERT_THROW(insert(*writer, doc2->indexed.begin(), doc2->indexed.end(),
                        doc2->stored.begin(), doc2->stored.end()),
                 irs::io_error);

    dir.register_failure(failing_directory::Failure::CREATE,
                         "_3.csi");  // columnstore index creation failure

    ASSERT_TRUE(insert(*writer, doc1->indexed.begin(), doc1->indexed.end(),
                       doc1->stored.begin(), doc1->stored.end()));

    dir.length(length, "_3.csd");
    ASSERT_EQ(length, 0);

    ASSERT_THROW(insert(*writer, doc2->indexed.begin(), doc2->indexed.end(),
                        doc2->stored.begin(), doc2->stored.end()),
                 irs::io_error);

    dir.length(length, "_3.csd");
    ASSERT_GT(length, 0);

    ASSERT_TRUE(writer->Begin());
    ASSERT_FALSE(writer->Commit());
    tests::AssertSnapshotEquality(writer->GetSnapshot(),
                                  irs::DirectoryReader(dir));

    // check data
    auto reader = irs::DirectoryReader(dir);
    ASSERT_TRUE(reader);

    ASSERT_EQ(1, reader->size());
    ASSERT_EQ(1, reader->docs_count());
    ASSERT_EQ(1, reader->live_docs_count());

    // validate index
    tests::index_t expected_index;
    expected_index.emplace_back(writer->FeatureInfo());
    expected_index.back().insert(*doc1);
    tests::assert_index(reader.GetImpl(), expected_index, all_features);

    // validate columnstore
    auto& segment = reader[0];  // assume 0 is id of first/only segment
    const auto* column = segment.column("name");
    ASSERT_NE(nullptr, column);
    auto values = column->iterator(irs::ColumnHint::kNormal);
    ASSERT_NE(nullptr, values);
    auto* actual_value = irs::get<irs::payload>(*values);
    ASSERT_NE(nullptr, actual_value);
    ASSERT_EQ(1, segment.docs_count());       // total count of documents
    ASSERT_EQ(1, segment.live_docs_count());  // total count of documents
    auto terms = segment.field("same");
    ASSERT_NE(nullptr, terms);
    auto termItr = terms->iterator(irs::SeekMode::NORMAL);
    ASSERT_TRUE(termItr->next());
    auto docsItr = termItr->postings(irs::IndexFeatures::NONE);
    ASSERT_TRUE(docsItr->next());
    ASSERT_EQ(docsItr->value(), values->seek(docsItr->value()));
    ASSERT_EQ("A", irs::to_string<std::string_view>(
                     actual_value->value.data()));  // 'name' value in doc3
    ASSERT_FALSE(docsItr->next());
  }
}

TEST(index_death_test_formats_10,
     columnstore_creation_sync_fail_implicit_segment_flush) {
  tests::json_doc_generator gen(test_base::resource("simple_sequential.json"),
                                &tests::payloaded_json_field_factory);
  const auto* doc1 = gen.next();
  const auto* doc2 = gen.next();

  auto codec = irs::formats::get("1_0");
  ASSERT_NE(nullptr, codec);

  // columnstore creation + sync failures
  {
    irs::memory_directory impl;
    failing_directory dir(impl);

    // write index
    irs::IndexWriterOptions opts;
    opts.segment_docs_max = 1;  // flush every 2nd document

    auto writer = irs::IndexWriter::Make(dir, codec, irs::OM_CREATE, opts);
    ASSERT_NE(nullptr, writer);

    // initial commit
    ASSERT_TRUE(writer->Begin());
    ASSERT_FALSE(writer->Commit());
    tests::AssertSnapshotEquality(writer->GetSnapshot(),
                                  irs::DirectoryReader(dir));

    ASSERT_TRUE(insert(*writer, doc1->indexed.begin(), doc1->indexed.end(),
                       doc1->stored.begin(), doc1->stored.end()));

    dir.register_failure(failing_directory::Failure::CREATE,
                         "_2.cs");  // columnstore creation failure
    dir.register_failure(failing_directory::Failure::SYNC,
                         "_1.cs");  // columnstore sync failure

    ASSERT_THROW(insert(*writer, doc2->indexed.begin(), doc2->indexed.end(),
                        doc2->stored.begin(), doc2->stored.end()),
                 irs::io_error);

    ASSERT_THROW(writer->Begin(), irs::io_error);

    // check data
    auto reader = irs::DirectoryReader(dir);
    ASSERT_TRUE(reader);
    ASSERT_EQ(0, reader->size());
    ASSERT_EQ(0, reader->docs_count());
    ASSERT_EQ(0, reader->live_docs_count());
  }
}

TEST(index_death_test_formats_14,
     columnstore_creation_sync_fail_implicit_segment_flush) {
  tests::json_doc_generator gen(test_base::resource("simple_sequential.json"),
                                &tests::payloaded_json_field_factory);
  const auto* doc1 = gen.next();
  const auto* doc2 = gen.next();

  auto codec = irs::formats::get("1_4");
  ASSERT_NE(nullptr, codec);

  // columnstore creation + sync failures
  {
    irs::memory_directory impl;
    failing_directory dir(impl);

    // write index
    irs::IndexWriterOptions opts;
    opts.segment_docs_max = 1;  // flush every 2nd document

    auto writer = irs::IndexWriter::Make(dir, codec, irs::OM_CREATE, opts);
    ASSERT_NE(nullptr, writer);

    // initial commit
    ASSERT_TRUE(writer->Begin());
    ASSERT_FALSE(writer->Commit());
    tests::AssertSnapshotEquality(writer->GetSnapshot(),
                                  irs::DirectoryReader(dir));

    ASSERT_TRUE(insert(*writer, doc1->indexed.begin(), doc1->indexed.end(),
                       doc1->stored.begin(), doc1->stored.end()));

    dir.register_failure(failing_directory::Failure::CREATE,
                         "_2.csd");  // columnstore data creation failure
    ASSERT_THROW(insert(*writer, doc2->indexed.begin(), doc2->indexed.end(),
                        doc2->stored.begin(), doc2->stored.end()),
                 irs::io_error);

    dir.register_failure(failing_directory::Failure::SYNC,
                         "_1.csd");  // columnstore data sync failure
    ASSERT_THROW(writer->Begin(), irs::io_error);

    ASSERT_TRUE(insert(*writer, doc2->indexed.begin(), doc2->indexed.end(),
                       doc2->stored.begin(), doc2->stored.end()));

    dir.register_failure(failing_directory::Failure::CREATE,
                         "_3.csi");  // columnstore index creation failure
    ASSERT_THROW(writer->Begin(), irs::io_error);

    ASSERT_TRUE(insert(*writer, doc2->indexed.begin(), doc2->indexed.end(),
                       doc2->stored.begin(), doc2->stored.end()));

    dir.register_failure(failing_directory::Failure::SYNC,
                         "_4.csi");  // columnstore index sync failure
    ASSERT_THROW(writer->Begin(), irs::io_error);

    // check data
    auto reader = irs::DirectoryReader(dir);
    ASSERT_TRUE(reader);
    ASSERT_EQ(0, reader->size());
    ASSERT_EQ(0, reader->docs_count());
    ASSERT_EQ(0, reader->live_docs_count());
  }
}

TEST(index_death_test_formats_14, fails_in_consolidate_with_removals) {
  tests::json_doc_generator gen(test_base::resource("simple_sequential.json"),
                                &tests::payloaded_json_field_factory);
  const auto* doc1 = gen.next();
  const auto* doc2 = gen.next();

  auto codec = irs::formats::get("1_4");
  ASSERT_NE(nullptr, codec);

  constexpr irs::IndexFeatures all_features =
    irs::IndexFeatures::FREQ | irs::IndexFeatures::POS |
    irs::IndexFeatures::OFFS | irs::IndexFeatures::PAY;

  {
    irs::memory_directory impl;
    failing_directory dir(impl);

    // write index
    irs::IndexWriterOptions opts;

    auto writer = irs::IndexWriter::Make(dir, codec, irs::OM_CREATE, opts);
    ASSERT_NE(nullptr, writer);

    dir.register_failure(failing_directory::Failure::CREATE,
                         "pending_segments_1");  //
    // initial commit fails
    ASSERT_THROW(writer->Begin(), irs::io_error);

    dir.register_failure(failing_directory::Failure::RENAME,
                         "pending_segments_1");  //
    ASSERT_THROW(writer->Commit(), irs::io_error);
    ASSERT_THROW((irs::DirectoryReader{dir}), irs::index_not_found);

    // now it is OK
    ASSERT_TRUE(writer->Commit());
    tests::AssertSnapshotEquality(writer->GetSnapshot(),
                                  irs::DirectoryReader(dir));

    dir.register_failure(failing_directory::Failure::CREATE,
                         "_1.csd");  // columnstore data creation failure
    ASSERT_THROW(insert(*writer, doc1->indexed.begin(), doc1->indexed.end(),
                        doc1->stored.begin(), doc1->stored.end()),
                 irs::io_error);

    // Nothing to commit
    ASSERT_FALSE(writer->Commit());
    tests::AssertSnapshotEquality(writer->GetSnapshot(),
                                  irs::DirectoryReader(dir));

    ASSERT_TRUE(insert(*writer, doc1->indexed.begin(), doc1->indexed.end(),
                       doc1->stored.begin(), doc1->stored.end()));

    ASSERT_TRUE(insert(*writer, doc2->indexed.begin(), doc2->indexed.end(),
                       doc2->stored.begin(), doc2->stored.end()));

    dir.register_failure(failing_directory::Failure::CREATE,
                         "_2.csi");  // columnstore index creation failure
    ASSERT_THROW(writer->Commit(), irs::io_error);
    tests::AssertSnapshotEquality(writer->GetSnapshot(),
                                  irs::DirectoryReader(dir));

    // Nothing to commit
    ASSERT_FALSE(writer->Commit());
    tests::AssertSnapshotEquality(writer->GetSnapshot(),
                                  irs::DirectoryReader(dir));

    ASSERT_TRUE(insert(*writer, doc1->indexed.begin(), doc1->indexed.end(),
                       doc1->stored.begin(), doc1->stored.end()));

    dir.register_failure(failing_directory::Failure::SYNC,
                         "_3.csd");  // columnstore index creation failure
    ASSERT_THROW(writer->Commit(), irs::io_error);
    tests::AssertSnapshotEquality(writer->GetSnapshot(),
                                  irs::DirectoryReader(dir));

    ASSERT_TRUE(insert(*writer, doc2->indexed.begin(), doc2->indexed.end(),
                       doc2->stored.begin(), doc2->stored.end()));

    dir.register_failure(failing_directory::Failure::SYNC,
                         "_4.csi");  // columnstore index creation failure
    ASSERT_THROW(writer->Commit(), irs::io_error);
    tests::AssertSnapshotEquality(writer->GetSnapshot(),
                                  irs::DirectoryReader(dir));

    // NOW IT IS OK

    ASSERT_TRUE(insert(*writer, doc1->indexed.begin(), doc1->indexed.end(),
                       doc1->stored.begin(), doc1->stored.end()));
    ASSERT_TRUE(writer->Commit());
    tests::AssertSnapshotEquality(writer->GetSnapshot(),
                                  irs::DirectoryReader(dir));

    ASSERT_TRUE(insert(*writer, doc2->indexed.begin(), doc2->indexed.end(),
                       doc2->stored.begin(), doc2->stored.end()));
    ASSERT_TRUE(writer->Commit());
    tests::AssertSnapshotEquality(writer->GetSnapshot(),
                                  irs::DirectoryReader(dir));

    const irs::index_utils::ConsolidateCount consolidate_all;

    ASSERT_TRUE(
      writer->Consolidate(irs::index_utils::MakePolicy(consolidate_all)));
    ASSERT_TRUE(writer->Commit());
    tests::AssertSnapshotEquality(writer->GetSnapshot(),
                                  irs::DirectoryReader(dir));

    dir.register_failure(failing_directory::Failure::REMOVE,
                         "_2.csd");  // columnstore data deletion failure
    dir.register_failure(failing_directory::Failure::REMOVE,
                         "_3.csi");  // columnstore index deletion failure
    dir.register_failure(failing_directory::Failure::REMOVE,
                         "_4.csd");  // columnstore data deletion failure
    dir.register_failure(failing_directory::Failure::REMOVE,
                         "_4.csi");  // columnstore index deletion failure
    dir.register_failure(failing_directory::Failure::REMOVE,
                         "_5.csi");  // columnstore index deletion failure
    dir.register_failure(failing_directory::Failure::REMOVE,
                         "_6.csd");  // columnstore data deletion failure
    irs::directory_cleaner::clean(dir);
    ASSERT_FALSE(writer->Commit());
    tests::AssertSnapshotEquality(writer->GetSnapshot(),
                                  irs::DirectoryReader(dir));

    ASSERT_TRUE(dir.no_failures());

    // check data
    auto reader = irs::DirectoryReader{dir};
    ASSERT_TRUE(reader);
    ASSERT_EQ(1, reader->size());
    ASSERT_EQ(2, reader->docs_count());
    ASSERT_EQ(2, reader->live_docs_count());

    // validate index
    tests::index_t expected_index;
    expected_index.emplace_back(writer->FeatureInfo());
    expected_index.back().insert(*doc1);
    expected_index.back().insert(*doc2);
    tests::assert_index(reader.GetImpl(), expected_index, all_features);

    // validate columnstore
    auto& segment = reader[0];  // assume 0 is id of first/only segment
    const auto* column = segment.column("name");
    ASSERT_NE(nullptr, column);
    auto values = column->iterator(irs::ColumnHint::kNormal);
    ASSERT_NE(nullptr, values);
    auto* actual_value = irs::get<irs::payload>(*values);
    ASSERT_NE(nullptr, actual_value);
    ASSERT_EQ(2, segment.docs_count());       // total count of documents
    ASSERT_EQ(2, segment.live_docs_count());  // total count of documents
    auto terms = segment.field("same");
    ASSERT_NE(nullptr, terms);
    auto termItr = terms->iterator(irs::SeekMode::NORMAL);
    ASSERT_TRUE(termItr->next());
    auto docsItr = termItr->postings(irs::IndexFeatures::NONE);
    ASSERT_TRUE(docsItr->next());
    ASSERT_EQ(docsItr->value(), values->seek(docsItr->value()));
    ASSERT_EQ("A", irs::to_string<std::string_view>(
                     actual_value->value.data()));  // 'name' value in doc1
    ASSERT_TRUE(docsItr->next());
    ASSERT_TRUE(values->next());
    actual_value = irs::get<irs::payload>(*values);
    ASSERT_EQ("B", irs::to_string<std::string_view>(
                     actual_value->value.data()));  // 'name' value in doc2
    ASSERT_FALSE(docsItr->next());
  }
}

TEST(index_death_test_formats_14, fails_in_exists) {
  tests::json_doc_generator gen(test_base::resource("simple_sequential.json"),
                                &tests::payloaded_json_field_factory);

  constexpr irs::IndexFeatures all_features =
    irs::IndexFeatures::FREQ | irs::IndexFeatures::POS |
    irs::IndexFeatures::OFFS | irs::IndexFeatures::PAY;

  const auto* doc1 = gen.next();
  const auto* doc2 = gen.next();
  const auto* doc3 = gen.next();
  const auto* doc4 = gen.next();

  auto codec = irs::formats::get("1_4");
  ASSERT_NE(nullptr, codec);

  {
    irs::memory_directory impl;
    failing_directory dir(impl);

    // write index
    irs::IndexWriterOptions opts;

    auto writer = irs::IndexWriter::Make(dir, codec, irs::OM_CREATE, opts);
    ASSERT_NE(nullptr, writer);

    // Will force error during commit becuase of segment reader reopen.
    dir.register_failure(failing_directory::Failure::EXISTS, "_1.csi");
    dir.register_failure(failing_directory::Failure::EXISTS, "_2.csd");
    dir.register_failure(failing_directory::Failure::EXISTS, "_3.csi");
    dir.register_failure(failing_directory::Failure::EXISTS, "_4.csd");

    while (!dir.no_failures()) {
      ASSERT_TRUE(insert(*writer, doc1->indexed.begin(), doc1->indexed.end(),
                         doc1->stored.begin(), doc1->stored.end()));
      ASSERT_THROW(writer->Commit(), irs::io_error);
      ASSERT_THROW((irs::DirectoryReader{dir}), irs::index_not_found);
    }

    ASSERT_TRUE(insert(*writer, doc1->indexed.begin(), doc1->indexed.end(),
                       doc1->stored.begin(), doc1->stored.end()));
    ASSERT_TRUE(insert(*writer, doc2->indexed.begin(), doc2->indexed.end(),
                       doc2->stored.begin(), doc2->stored.end()));
    ASSERT_TRUE(writer->Commit());
    tests::AssertSnapshotEquality(writer->GetSnapshot(),
                                  irs::DirectoryReader(dir));

    ASSERT_TRUE(insert(*writer, doc3->indexed.begin(), doc3->indexed.end(),
                       doc3->stored.begin(), doc3->stored.end()));
    ASSERT_TRUE(insert(*writer, doc4->indexed.begin(), doc4->indexed.end(),
                       doc4->stored.begin(), doc4->stored.end()));
    ASSERT_TRUE(writer->Commit());
    tests::AssertSnapshotEquality(writer->GetSnapshot(),
                                  irs::DirectoryReader(dir));

    const irs::index_utils::ConsolidateCount consolidate_all;

    ASSERT_TRUE(
      writer->Consolidate(irs::index_utils::MakePolicy(consolidate_all)));
    ASSERT_TRUE(writer->Commit());
    tests::AssertSnapshotEquality(writer->GetSnapshot(),
                                  irs::DirectoryReader(dir));

    ASSERT_TRUE(dir.no_failures());

    // check data
    auto reader = irs::DirectoryReader(dir);
    ASSERT_TRUE(reader);
    ASSERT_EQ(1, reader->size());
    ASSERT_EQ(4, reader->docs_count());
    ASSERT_EQ(4, reader->live_docs_count());

    // validate index
    tests::index_t expected_index;
    expected_index.emplace_back(writer->FeatureInfo());
    expected_index.back().insert(*doc1);
    expected_index.back().insert(*doc2);
    expected_index.back().insert(*doc3);
    expected_index.back().insert(*doc4);
    tests::assert_index(reader.GetImpl(), expected_index, all_features);

    // validate columnstore
    auto& segment = reader[0];  // assume 0 is id of first/only segment
    const auto* column = segment.column("name");
    ASSERT_NE(nullptr, column);
    auto values = column->iterator(irs::ColumnHint::kNormal);
    ASSERT_NE(nullptr, values);
    auto* actual_value = irs::get<irs::payload>(*values);
    ASSERT_NE(nullptr, actual_value);
    ASSERT_EQ(4, segment.docs_count());       // total count of documents
    ASSERT_EQ(4, segment.live_docs_count());  // total count of documents
    auto terms = segment.field("same");
    ASSERT_NE(nullptr, terms);
    auto termItr = terms->iterator(irs::SeekMode::NORMAL);
    ASSERT_TRUE(termItr->next());
    auto docsItr = termItr->postings(irs::IndexFeatures::NONE);
    ASSERT_TRUE(docsItr->next());
    ASSERT_EQ(docsItr->value(), values->seek(docsItr->value()));
    ASSERT_EQ("A", irs::to_string<std::string_view>(
                     actual_value->value.data()));  // 'name' value in doc1
    ASSERT_TRUE(docsItr->next());
    ASSERT_TRUE(values->next());
    actual_value = irs::get<irs::payload>(*values);
    ASSERT_EQ("B", irs::to_string<std::string_view>(
                     actual_value->value.data()));  // 'name' value in doc2
    ASSERT_TRUE(docsItr->next());
    ASSERT_TRUE(values->next());
    actual_value = irs::get<irs::payload>(*values);
    ASSERT_EQ("C", irs::to_string<std::string_view>(
                     actual_value->value.data()));  // 'name' value in doc3
    ASSERT_TRUE(docsItr->next());
    ASSERT_TRUE(values->next());
    actual_value = irs::get<irs::payload>(*values);
    ASSERT_EQ("D", irs::to_string<std::string_view>(
                     actual_value->value.data()));  // 'name' value in doc4
    ASSERT_FALSE(docsItr->next());
  }
}

TEST(index_death_test_formats_14, fails_in_length) {
  tests::json_doc_generator gen(
    test_base::resource("simple_sequential.json"),
    [](tests::document& doc, const std::string& name,
       const tests::json_doc_generator::json_value& data) {
      if (data.is_string()) {
        doc.insert(std::make_shared<tests::string_field>(name, data.str));
      }
    });
  const auto* doc1 = gen.next();
  const auto* doc2 = gen.next();
  const auto* doc3 = gen.next();
  const auto* doc4 = gen.next();

  auto codec = irs::formats::get("1_4");
  ASSERT_NE(nullptr, codec);

  {
    constexpr irs::IndexFeatures all_features =
      irs::IndexFeatures::FREQ | irs::IndexFeatures::POS |
      irs::IndexFeatures::OFFS | irs::IndexFeatures::PAY;

    irs::memory_directory impl;
    failing_directory dir(impl);

    dir.register_failure(failing_directory::Failure::LENGTH, "_1.csd");
    dir.register_failure(failing_directory::Failure::LENGTH, "_1.csi");
    dir.register_failure(failing_directory::Failure::LENGTH,
                         "_1.ti");  // term index
    dir.register_failure(failing_directory::Failure::LENGTH,
                         "_1.tm");  // term data
    dir.register_failure(failing_directory::Failure::LENGTH,
                         "_1.pos");  // postings list (positions)
    dir.register_failure(failing_directory::Failure::LENGTH, "_1.doc");

    dir.register_failure(failing_directory::Failure::LENGTH, "_2.csd");
    dir.register_failure(failing_directory::Failure::LENGTH, "_2.csi");
    dir.register_failure(failing_directory::Failure::LENGTH,
                         "_2.ti");  // term index
    dir.register_failure(failing_directory::Failure::LENGTH,
                         "_2.tm");  // term data
    dir.register_failure(failing_directory::Failure::LENGTH,
                         "_2.pos");  // postings list (positions)
    dir.register_failure(failing_directory::Failure::LENGTH, "_2.doc");

    dir.register_failure(failing_directory::Failure::LENGTH, "_3.csd");
    dir.register_failure(failing_directory::Failure::LENGTH, "_3.csi");
    dir.register_failure(failing_directory::Failure::LENGTH,
                         "_3.ti");  // term index
    dir.register_failure(failing_directory::Failure::LENGTH,
                         "_3.tm");  // term data
    dir.register_failure(failing_directory::Failure::LENGTH,
                         "_3.pos");  // postings list (positions)
    dir.register_failure(failing_directory::Failure::LENGTH, "_3.doc");

    dir.register_failure(failing_directory::Failure::LENGTH, "_4.csd");
    dir.register_failure(failing_directory::Failure::LENGTH, "_4.csi");
    dir.register_failure(failing_directory::Failure::LENGTH,
                         "_4.ti");  // term index
    dir.register_failure(failing_directory::Failure::LENGTH,
                         "_4.tm");  // term data
    dir.register_failure(failing_directory::Failure::LENGTH,
                         "_4.pos");  // postings list (positions)
    dir.register_failure(failing_directory::Failure::LENGTH, "_4.doc");

    const size_t num_failures = dir.num_failures();

    // write index
    auto writer = irs::IndexWriter::Make(dir, codec, irs::OM_CREATE);
    ASSERT_NE(nullptr, writer);

    // segment 0
    ASSERT_TRUE(insert(*writer, doc1->indexed.begin(), doc1->indexed.end(),
                       doc1->stored.begin(), doc1->stored.end()));
    ASSERT_TRUE(writer->Commit());
    tests::AssertSnapshotEquality(writer->GetSnapshot(),
                                  irs::DirectoryReader(dir));

    // segment 1
    ASSERT_TRUE(insert(*writer, doc2->indexed.begin(), doc2->indexed.end(),
                       doc2->stored.begin(), doc2->stored.end()));
    ASSERT_TRUE(writer->Commit());
    tests::AssertSnapshotEquality(writer->GetSnapshot(),
                                  irs::DirectoryReader(dir));

    // segment 2
    ASSERT_TRUE(insert(*writer, doc3->indexed.begin(), doc3->indexed.end(),
                       doc3->stored.begin(), doc3->stored.end()));
    ASSERT_TRUE(writer->Commit());
    tests::AssertSnapshotEquality(writer->GetSnapshot(),
                                  irs::DirectoryReader(dir));

    // segment 3
    ASSERT_TRUE(insert(*writer, doc4->indexed.begin(), doc4->indexed.end(),
                       doc4->stored.begin(), doc4->stored.end()));
    ASSERT_TRUE(writer->Commit());
    tests::AssertSnapshotEquality(writer->GetSnapshot(),
                                  irs::DirectoryReader(dir));

    {
      // check data
      auto reader = irs::DirectoryReader(dir);
      ASSERT_TRUE(reader);
      ASSERT_EQ(4, reader->size());
      ASSERT_EQ(4, reader->docs_count());
      ASSERT_EQ(4, reader->live_docs_count());
    }

    // Commit without removals doesn't call `length`
    ASSERT_EQ(num_failures, dir.num_failures());
    dir.clear_failures();

    dir.register_failure(failing_directory::Failure::EXISTS, "_1.csd");
    dir.register_failure(failing_directory::Failure::EXISTS, "_1.csi");

    dir.register_failure(failing_directory::Failure::EXISTS, "_2.csd");
    dir.register_failure(failing_directory::Failure::EXISTS, "_2.csi");

    dir.register_failure(failing_directory::Failure::EXISTS, "_3.csd");
    dir.register_failure(failing_directory::Failure::EXISTS, "_3.csi");

    dir.register_failure(failing_directory::Failure::EXISTS, "_4.csd");
    dir.register_failure(failing_directory::Failure::EXISTS, "_4.csi");

    const irs::index_utils::ConsolidateCount consolidate_all;

    const auto num_failures_before = dir.num_failures();
    ASSERT_TRUE(
      writer->Consolidate(irs::index_utils::MakePolicy(consolidate_all)));
    ASSERT_EQ(num_failures_before, dir.num_failures());

    irs::directory_cleaner::clean(dir);
    ASSERT_TRUE(writer->Commit());
    tests::AssertSnapshotEquality(writer->GetSnapshot(),
                                  irs::DirectoryReader(dir));

    ASSERT_EQ(num_failures_before, dir.num_failures());

    // check data
    auto reader = irs::DirectoryReader(dir);
    ASSERT_TRUE(reader);
    ASSERT_EQ(1, reader->size());
    ASSERT_EQ(4, reader->docs_count());
    ASSERT_EQ(4, reader->live_docs_count());

    // validate index
    tests::index_t expected_index;
    expected_index.emplace_back(writer->FeatureInfo());
    expected_index.back().insert(*doc1);
    expected_index.back().insert(*doc2);
    expected_index.back().insert(*doc3);
    expected_index.back().insert(*doc4);
    tests::assert_index(reader.GetImpl(), expected_index, all_features);

    // validate columnstore (segment 0)
    {
      auto& segment = reader[0];  // assume 0 is id of first/only segment
      const auto* column = segment.column("name");
      ASSERT_NE(nullptr, column);
      auto values = column->iterator(irs::ColumnHint::kNormal);
      ASSERT_NE(nullptr, values);
      auto* actual_value = irs::get<irs::payload>(*values);
      ASSERT_NE(nullptr, actual_value);
      ASSERT_EQ(4, segment.docs_count());       // total count of documents
      ASSERT_EQ(4, segment.live_docs_count());  // total count of documents
      auto terms = segment.field("same");
      ASSERT_NE(nullptr, terms);
      auto termItr = terms->iterator(irs::SeekMode::NORMAL);
      ASSERT_TRUE(termItr->next());
      auto docsItr = termItr->postings(irs::IndexFeatures::NONE);
      ASSERT_TRUE(docsItr->next());
      ASSERT_EQ(docsItr->value(), values->seek(docsItr->value()));
      ASSERT_EQ("A", irs::to_string<std::string_view>(
                       actual_value->value.data()));  // 'name' value in doc1
      ASSERT_TRUE(docsItr->next());
      ASSERT_TRUE(values->next());
      actual_value = irs::get<irs::payload>(*values);
      ASSERT_EQ("B", irs::to_string<std::string_view>(
                       actual_value->value.data()));  // 'name' value in doc2
      ASSERT_TRUE(docsItr->next());
      ASSERT_TRUE(values->next());
      actual_value = irs::get<irs::payload>(*values);
      ASSERT_EQ("C", irs::to_string<std::string_view>(
                       actual_value->value.data()));  // 'name' value in doc3
      ASSERT_TRUE(docsItr->next());
      ASSERT_TRUE(values->next());
      actual_value = irs::get<irs::payload>(*values);
      ASSERT_EQ("D", irs::to_string<std::string_view>(
                       actual_value->value.data()));  // 'name' value in doc4
      ASSERT_FALSE(docsItr->next());
    }
  }
}

TEST(index_death_test_formats_10, open_reader) {
  ::open_reader("1_0", [](failing_directory& dir) {
    // postings list (documents)
    dir.register_failure(failing_directory::Failure::OPEN, "_1.doc");
    // deleted docs
    dir.register_failure(failing_directory::Failure::EXISTS, "_1.2.doc_mask");
    // deleted docs
    dir.register_failure(failing_directory::Failure::OPEN, "_1.2.doc_mask");
    // column meta
    dir.register_failure(failing_directory::Failure::EXISTS, "_1.cm");
    // column meta
    dir.register_failure(failing_directory::Failure::OPEN, "_1.cm");
    // columnstore
    dir.register_failure(failing_directory::Failure::EXISTS, "_1.cs");
    // columnstore
    dir.register_failure(failing_directory::Failure::OPEN, "_1.cs");
    // term index
    dir.register_failure(failing_directory::Failure::OPEN, "_1.ti");
    // term data
    dir.register_failure(failing_directory::Failure::OPEN, "_1.tm");
    // postings list (positions)
    dir.register_failure(failing_directory::Failure::OPEN, "_1.pos");
    // postings list (offset + payload)
    dir.register_failure(failing_directory::Failure::OPEN, "_1.pay");
  });
}

TEST(index_death_test_formats_14, open_reader) {
  ::open_reader("1_4", [](failing_directory& dir) {
    // postings list (documents)
    dir.register_failure(failing_directory::Failure::OPEN, "_1.doc");
    // deleted docs
    dir.register_failure(failing_directory::Failure::EXISTS, "_1.2.doc_mask");
    // deleted docs
    dir.register_failure(failing_directory::Failure::OPEN, "_1.2.doc_mask");
    // columnstore index
    dir.register_failure(failing_directory::Failure::EXISTS, "_1.csi");
    // columnstore index
    dir.register_failure(failing_directory::Failure::OPEN, "_1.csi");
    // columnstore data
    dir.register_failure(failing_directory::Failure::EXISTS, "_1.csd");
    // columnstore data
    dir.register_failure(failing_directory::Failure::OPEN, "_1.csd");
    // term index
    dir.register_failure(failing_directory::Failure::OPEN, "_1.ti");
    // term data
    dir.register_failure(failing_directory::Failure::OPEN, "_1.tm");
    // postings list (positions)
    dir.register_failure(failing_directory::Failure::OPEN, "_1.pos");
    // postings list (offset + payload)
    dir.register_failure(failing_directory::Failure::OPEN, "_1.pay");
  });
}

TEST(index_death_test_formats_10, columnstore_reopen_fail) {
  constexpr irs::IndexFeatures all_features =
    irs::IndexFeatures::FREQ | irs::IndexFeatures::POS |
    irs::IndexFeatures::OFFS | irs::IndexFeatures::PAY;

  tests::json_doc_generator gen(test_base::resource("simple_sequential.json"),
                                &tests::payloaded_json_field_factory);
  const auto* doc1 = gen.next();
  const auto* doc2 = gen.next();
  auto query_doc2 = MakeByTerm("name", "B");

  auto codec = irs::formats::get("1_0");
  ASSERT_NE(nullptr, codec);

  // create source segment
  irs::memory_directory impl;
  failing_directory dir(impl);

  // write index
  {
    auto writer = irs::IndexWriter::Make(dir, codec, irs::OM_CREATE);
    ASSERT_NE(nullptr, writer);

    ASSERT_TRUE(insert(*writer, doc1->indexed.begin(), doc1->indexed.end(),
                       doc1->stored.begin(), doc1->stored.end()));

    ASSERT_TRUE(insert(*writer, doc2->indexed.begin(), doc2->indexed.end(),
                       doc2->stored.begin(), doc2->stored.end()));

    writer->GetBatch().Remove(*query_doc2);

    ASSERT_TRUE(writer->Commit());
    tests::AssertSnapshotEquality(writer->GetSnapshot(),
                                  irs::DirectoryReader(dir));
  }

  // check data
  auto reader = irs::DirectoryReader(dir);
  ASSERT_TRUE(reader);
  ASSERT_EQ(1, reader->size());
  ASSERT_EQ(2, reader->docs_count());
  ASSERT_EQ(1, reader->live_docs_count());

  // validate index
  tests::index_t expected_index;
  expected_index.emplace_back(::default_feature_info());
  expected_index.back().insert(*doc1);
  expected_index.back().insert(*doc2);
  tests::assert_index(reader.GetImpl(), expected_index, all_features);

  // regiseter reopen failure in columnstore
  dir.register_failure(failing_directory::Failure::REOPEN, "_1.cs");
  // regiseter reopen failure in columnstore
  dir.register_failure(failing_directory::Failure::REOPEN_NULL, "_1.cs");

  // validate columnstore
  auto& segment = reader[0];  // assume 0 is id of first/only segment
  const auto* column = segment.column("name");
  ASSERT_NE(nullptr, column);
  auto values = column->iterator(irs::ColumnHint::kNormal);
  ASSERT_NE(nullptr, values);
  auto* actual_value = irs::get<irs::payload>(*values);
  ASSERT_NE(nullptr, actual_value);
  ASSERT_EQ(2, segment.docs_count());       // total count of documents
  ASSERT_EQ(1, segment.live_docs_count());  // total count of documents
  auto terms = segment.field("same");
  ASSERT_NE(nullptr, terms);
  auto termItr = terms->iterator(irs::SeekMode::NORMAL);
  ASSERT_TRUE(termItr->next());
  auto docsItr = termItr->postings(irs::IndexFeatures::NONE);
  ASSERT_TRUE(docsItr->next());
  // failed to reopen
  ASSERT_THROW(values->seek(docsItr->value()), irs::io_error);
  // failed to reopen (nullptr)
  ASSERT_THROW(values->seek(docsItr->value()), irs::io_error);
  // successful attempt
  ASSERT_EQ(docsItr->value(), values->seek(docsItr->value()));
  // don't need to reopen anymore
  dir.register_failure(failing_directory::Failure::REOPEN, "_1.cs");
  // 'name' value in doc3
  ASSERT_EQ("A", irs::to_string<std::string_view>(actual_value->value.data()));
  ASSERT_TRUE(docsItr->next());
  ASSERT_EQ(docsItr->value(), values->seek(docsItr->value()));
  // 'name' value in doc3
  ASSERT_EQ("B", irs::to_string<std::string_view>(actual_value->value.data()));
  ASSERT_FALSE(docsItr->next());

  // validate live docs
  auto live_docs = segment.docs_iterator();
  ASSERT_TRUE(live_docs->next());
  ASSERT_EQ(1, live_docs->value());
  ASSERT_FALSE(live_docs->next());
  ASSERT_EQ(irs::doc_limits::eof(), live_docs->value());
}

TEST(index_death_test_formats_14, columnstore_reopen_fail) {
  constexpr irs::IndexFeatures all_features =
    irs::IndexFeatures::FREQ | irs::IndexFeatures::POS |
    irs::IndexFeatures::OFFS | irs::IndexFeatures::PAY;

  tests::json_doc_generator gen(test_base::resource("simple_sequential.json"),
                                &tests::payloaded_json_field_factory);
  const auto* doc1 = gen.next();
  const auto* doc2 = gen.next();
  auto query_doc2 = MakeByTerm("name", "B");

  auto codec = irs::formats::get("1_4");
  ASSERT_NE(nullptr, codec);

  // create source segment
  irs::memory_directory impl;
  failing_directory dir(impl);

  // write index
  {
    auto writer = irs::IndexWriter::Make(dir, codec, irs::OM_CREATE);
    ASSERT_NE(nullptr, writer);

    ASSERT_TRUE(insert(*writer, doc1->indexed.begin(), doc1->indexed.end(),
                       doc1->stored.begin(), doc1->stored.end()));

    ASSERT_TRUE(insert(*writer, doc2->indexed.begin(), doc2->indexed.end(),
                       doc2->stored.begin(), doc2->stored.end()));

    writer->GetBatch().Remove(*query_doc2);

    ASSERT_TRUE(writer->Commit());
    tests::AssertSnapshotEquality(writer->GetSnapshot(),
                                  irs::DirectoryReader(dir));
  }

  dir.register_failure(failing_directory::Failure::OPEN,
                       "_1.csi");  // regiseter open failure in columnstore
  dir.register_failure(failing_directory::Failure::OPEN,
                       "_1.csd");  // regiseter open failure in columnstore
  ASSERT_THROW(irs::DirectoryReader{dir}, irs::io_error);
  ASSERT_THROW(irs::DirectoryReader{dir}, irs::io_error);

  // check data
  auto reader = irs::DirectoryReader(dir);
  ASSERT_TRUE(reader);
  ASSERT_EQ(1, reader->size());
  ASSERT_EQ(2, reader->docs_count());
  ASSERT_EQ(1, reader->live_docs_count());

  // validate index
  tests::index_t expected_index;
  expected_index.emplace_back(::default_feature_info());
  expected_index.back().insert(*doc1);
  expected_index.back().insert(*doc2);
  tests::assert_index(reader.GetImpl(), expected_index, all_features);

  dir.register_failure(failing_directory::Failure::REOPEN,
                       "_1.csd");  // regiseter reopen failure in columnstore
  dir.register_failure(failing_directory::Failure::REOPEN,
                       "_1.csi");  // regiseter reopen failure in columnstore
  dir.register_failure(failing_directory::Failure::REOPEN_NULL,
                       "_1.csd");  // regiseter reopen failure in columnstore
  dir.register_failure(failing_directory::Failure::REOPEN_NULL,
                       "_1.csi");  // regiseter reopen failure in columnstore

  // validate columnstore
  auto& segment = reader[0];  // assume 0 is id of first/only segment
  const auto* column = segment.column("name");
  ASSERT_NE(nullptr, column);
  ASSERT_THROW(column->iterator(irs::ColumnHint::kNormal),
               irs::io_error);  // failed to reopen csd
  ASSERT_THROW(column->iterator(irs::ColumnHint::kNormal),
               irs::io_error);  // failed to reopen csd (nullptr)
  auto values = column->iterator(irs::ColumnHint::kNormal);
  ASSERT_NE(nullptr, values);
  auto* actual_value = irs::get<irs::payload>(*values);
  ASSERT_NE(nullptr, actual_value);
  ASSERT_EQ(2, segment.docs_count());       // total count of documents
  ASSERT_EQ(1, segment.live_docs_count());  // total count of documents
  auto terms = segment.field("same");
  ASSERT_NE(nullptr, terms);
  auto termItr = terms->iterator(irs::SeekMode::NORMAL);
  ASSERT_TRUE(termItr->next());
  auto docsItr = termItr->postings(irs::IndexFeatures::NONE);
  ASSERT_TRUE(docsItr->next());
  ASSERT_EQ(docsItr->value(),
            values->seek(docsItr->value()));  // successful attempt
  ASSERT_EQ("A", irs::to_string<std::string_view>(
                   actual_value->value.data()));  // 'name' value in doc3
  ASSERT_TRUE(docsItr->next());
  ASSERT_EQ(docsItr->value(), values->seek(docsItr->value()));
  ASSERT_EQ("B", irs::to_string<std::string_view>(
                   actual_value->value.data()));  // 'name' value in doc3
  ASSERT_FALSE(docsItr->next());

  // validate live docs
  auto live_docs = segment.docs_iterator();
  ASSERT_TRUE(live_docs->next());
  ASSERT_EQ(1, live_docs->value());
  ASSERT_FALSE(live_docs->next());
  ASSERT_EQ(irs::doc_limits::eof(), live_docs->value());
}

TEST(index_death_test_formats_14, fails_in_dup) {
  constexpr irs::IndexFeatures all_features =
    irs::IndexFeatures::FREQ | irs::IndexFeatures::POS |
    irs::IndexFeatures::OFFS | irs::IndexFeatures::PAY;

  tests::json_doc_generator gen(test_base::resource("simple_sequential.json"),
                                &tests::payloaded_json_field_factory);
  const auto* doc1 = gen.next();
  const auto* doc2 = gen.next();
  const auto* doc3 = gen.next();
  const auto* doc4 = gen.next();

  auto codec = irs::formats::get("1_4");
  ASSERT_NE(nullptr, codec);

  // create source segment
  irs::memory_directory impl;
  failing_directory dir(impl);

  // write index
  {
    auto writer = irs::IndexWriter::Make(dir, codec, irs::OM_CREATE);
    ASSERT_NE(nullptr, writer);

    ASSERT_TRUE(insert(*writer, doc1->indexed.begin(), doc1->indexed.end(),
                       doc1->stored.begin(), doc1->stored.end()));

    ASSERT_TRUE(insert(*writer, doc2->indexed.begin(), doc2->indexed.end()));

    ASSERT_TRUE(insert(*writer, doc3->indexed.begin(), doc3->indexed.end(),
                       doc3->stored.begin(), doc3->stored.end()));

    ASSERT_TRUE(insert(*writer, doc4->indexed.begin(), doc4->indexed.end()));

    ASSERT_TRUE(writer->Commit());
    tests::AssertSnapshotEquality(writer->GetSnapshot(),
                                  irs::DirectoryReader(dir));
  }
  // regiseter open failure in columnstore
  dir.register_failure(failing_directory::Failure::OPEN, "_1.csi");
  // regiseter open failure in columnstore
  dir.register_failure(failing_directory::Failure::OPEN, "_1.csd");
  ASSERT_THROW(irs::DirectoryReader{dir}, irs::io_error);
  ASSERT_THROW(irs::DirectoryReader{dir}, irs::io_error);

  // check data
  auto reader = irs::DirectoryReader(dir);
  ASSERT_TRUE(reader);
  ASSERT_EQ(1, reader->size());
  ASSERT_EQ(4, reader->docs_count());
  ASSERT_EQ(4, reader->live_docs_count());

  // validate index
  tests::index_t expected_index;
  expected_index.emplace_back(::default_feature_info());
  expected_index.back().insert(*doc1);
  expected_index.back().insert(*doc2);
  expected_index.back().insert(*doc3);
  expected_index.back().insert(*doc4);
  tests::assert_index(reader.GetImpl(), expected_index, all_features);
  // regiseter dup failure in columnstore
  dir.register_failure(failing_directory::Failure::DUP, "_1.csd");
  // regiseter dup failure in columnstore
  dir.register_failure(failing_directory::Failure::DUP_NULL, "_1.csd");

  auto& segment = reader[0];  // assume 0 is id of first/only segment
  const auto* column = segment.column("name");
  ASSERT_NE(nullptr, column);
  // failed to reopen csd
  ASSERT_THROW(column->iterator(irs::ColumnHint::kNormal), irs::io_error);
  // failed to reopen csd (nullptr)
  ASSERT_THROW(column->iterator(irs::ColumnHint::kNormal), irs::io_error);
  ASSERT_TRUE(dir.no_failures());

  auto values = column->iterator(irs::ColumnHint::kNormal);
  ASSERT_NE(nullptr, values);
  auto* actual_value = irs::get<irs::payload>(*values);
  ASSERT_NE(nullptr, actual_value);
  ASSERT_EQ(4, segment.docs_count());       // total count of documents
  ASSERT_EQ(4, segment.live_docs_count());  // total count of documents
  auto terms = segment.field("same");
  ASSERT_NE(nullptr, terms);
  auto termItr = terms->iterator(irs::SeekMode::NORMAL);
  ASSERT_TRUE(termItr->next());
  auto docsItr = termItr->postings(irs::IndexFeatures::NONE);
  ASSERT_TRUE(docsItr->next());

  ASSERT_EQ(docsItr->value(), values->seek(docsItr->value()));  // '1' and '1'
  // 'name' value in doc1
  ASSERT_EQ("A", irs::to_string<std::string_view>(actual_value->value.data()));
  ASSERT_TRUE(docsItr->next());

  ASSERT_EQ(docsItr->value(), 2);
  // because 2nd document is not stored
  ASSERT_EQ(values->seek(docsItr->value()), 3);

  // 'name' value in doc3. because
  // 2nd document is not stored
  ASSERT_EQ("C", irs::to_string<std::string_view>(actual_value->value.data()));
  ASSERT_TRUE(docsItr->next());

  ASSERT_EQ(docsItr->value(), values->seek(docsItr->value()));  // '3' and '3'
  ASSERT_EQ("C", irs::to_string<std::string_view>(actual_value->value.data()));
  ASSERT_TRUE(docsItr->next());

  ASSERT_EQ(docsItr->value(), 4);
  // because 4th document is not stored
  ASSERT_NE(values->seek(docsItr->value()), 4);
  ASSERT_FALSE(docsItr->next());
}

TEST(index_death_test_formats_10, postings_reopen_fail) {
  constexpr irs::IndexFeatures all_features =
    irs::IndexFeatures::FREQ | irs::IndexFeatures::POS |
    irs::IndexFeatures::OFFS | irs::IndexFeatures::PAY;

  constexpr irs::IndexFeatures positions =
    irs::IndexFeatures::FREQ | irs::IndexFeatures::POS;

  constexpr irs::IndexFeatures positions_offsets = irs::IndexFeatures::FREQ |
                                                   irs::IndexFeatures::POS |
                                                   irs::IndexFeatures::OFFS;

  constexpr irs::IndexFeatures positions_payload = irs::IndexFeatures::FREQ |
                                                   irs::IndexFeatures::POS |
                                                   irs::IndexFeatures::PAY;

  tests::json_doc_generator gen(test_base::resource("simple_sequential.json"),
                                &tests::payloaded_json_field_factory);
  const auto* doc1 = gen.next();
  const auto* doc2 = gen.next();
  auto query_doc2 = MakeByTerm("name", "B");

  auto codec = irs::formats::get("1_0");
  ASSERT_NE(nullptr, codec);

  // create source segment
  irs::memory_directory impl;
  failing_directory dir(impl);

  // write index
  {
    auto writer = irs::IndexWriter::Make(dir, codec, irs::OM_CREATE);
    ASSERT_NE(nullptr, writer);

    ASSERT_TRUE(insert(*writer, doc1->indexed.begin(), doc1->indexed.end(),
                       doc1->stored.begin(), doc1->stored.end()));

    ASSERT_TRUE(insert(*writer, doc2->indexed.begin(), doc2->indexed.end(),
                       doc2->stored.begin(), doc2->stored.end()));

    writer->GetBatch().Remove(*query_doc2);

    ASSERT_TRUE(writer->Commit());
    tests::AssertSnapshotEquality(writer->GetSnapshot(),
                                  irs::DirectoryReader(dir));
  }

  // check data
  auto reader = irs::DirectoryReader(dir);
  ASSERT_TRUE(reader);
  ASSERT_EQ(1, reader->size());
  ASSERT_EQ(2, reader->docs_count());
  ASSERT_EQ(1, reader->live_docs_count());

  // validate index
  tests::index_t expected_index;
  expected_index.emplace_back(::default_feature_info());
  expected_index.back().insert(*doc1);
  expected_index.back().insert(*doc2);
  tests::assert_index(reader.GetImpl(), expected_index, all_features);

  // validate columnstore
  auto& segment = reader[0];  // assume 0 is id of first/only segment
  const auto* column = segment.column("name");
  ASSERT_NE(nullptr, column);
  auto values = column->iterator(irs::ColumnHint::kNormal);
  ASSERT_NE(nullptr, values);
  auto* actual_value = irs::get<irs::payload>(*values);
  ASSERT_NE(nullptr, actual_value);
  ASSERT_EQ(2, segment.docs_count());       // total count of documents
  ASSERT_EQ(1, segment.live_docs_count());  // total count of documents
  auto terms = segment.field("same_anl_pay");
  ASSERT_NE(nullptr, terms);

  // regiseter reopen failure in term dictionary
  {
    dir.register_failure(failing_directory::Failure::REOPEN, "_1.tm");
    auto termItr =
      terms->iterator(irs::SeekMode::NORMAL);  // successful attempt
    ASSERT_NE(nullptr, termItr);
    ASSERT_THROW(termItr->next(), irs::io_error);
  }

  // regiseter reopen failure in term dictionary (nullptr)
  {
    dir.register_failure(failing_directory::Failure::REOPEN_NULL, "_1.tm");
    auto termItr =
      terms->iterator(irs::SeekMode::NORMAL);  // successful attempt
    ASSERT_NE(nullptr, termItr);
    ASSERT_THROW(termItr->next(), irs::io_error);
  }

  auto termItr = terms->iterator(irs::SeekMode::NORMAL);  // successful attempt
  ASSERT_NE(nullptr, termItr);
  ASSERT_TRUE(termItr->next());

  // regiseter reopen failure in postings
  dir.register_failure(failing_directory::Failure::REOPEN, "_1.doc");
  // can't reopen document input
  ASSERT_THROW((void)termItr->postings(irs::IndexFeatures::NONE),
               irs::io_error);
  // regiseter reopen failure in postings (nullptr)
  dir.register_failure(failing_directory::Failure::REOPEN_NULL, "_1.doc");
  // can't reopen document input (nullptr)
  ASSERT_THROW((void)termItr->postings(irs::IndexFeatures::NONE),
               irs::io_error);
  // regiseter reopen failure in positions
  dir.register_failure(failing_directory::Failure::REOPEN, "_1.pos");
  // can't reopen position input
  ASSERT_THROW((void)termItr->postings(positions), irs::io_error);
  // regiseter reopen failure in positions (nullptr)
  dir.register_failure(failing_directory::Failure::REOPEN_NULL, "_1.pos");
  // can't reopen position (nullptr)
  ASSERT_THROW((void)termItr->postings(positions), irs::io_error);
  // regiseter reopen failure in payload
  dir.register_failure(failing_directory::Failure::REOPEN, "_1.pay");
  // can't reopen offset input
  ASSERT_THROW((void)termItr->postings(positions_offsets), irs::io_error);

  // regiseter reopen failure in payload (nullptr)
  dir.register_failure(failing_directory::Failure::REOPEN_NULL, "_1.pay");

  // can't reopen position (nullptr)
  ASSERT_THROW((void)termItr->postings(positions_offsets), irs::io_error);
  // regiseter reopen failure in payload
  // can't reopen offset input
  dir.register_failure(failing_directory::Failure::REOPEN, "_1.pay");
  ASSERT_THROW((void)termItr->postings(positions_payload), irs::io_error);
  // regiseter reopen failure in payload (nullptr)
  dir.register_failure(failing_directory::Failure::REOPEN_NULL, "_1.pay");
  // can't reopen position (nullptr)
  ASSERT_THROW((void)termItr->postings(positions_payload), irs::io_error);

  // regiseter reopen failure in postings
  dir.register_failure(failing_directory::Failure::REOPEN, "_1.doc");
  // regiseter reopen failure in postings
  dir.register_failure(failing_directory::Failure::REOPEN_NULL, "_1.doc");
  // regiseter reopen failure in positions
  dir.register_failure(failing_directory::Failure::REOPEN, "_1.pos");
  // regiseter reopen failure in positions
  dir.register_failure(failing_directory::Failure::REOPEN_NULL, "_1.pos");
  // regiseter reopen failure in payload
  dir.register_failure(failing_directory::Failure::REOPEN, "_1.pay");
  // regiseter reopen failure in payload
  dir.register_failure(failing_directory::Failure::REOPEN_NULL, "_1.pay");
  ASSERT_THROW((void)termItr->postings(all_features), irs::io_error);
  ASSERT_THROW((void)termItr->postings(all_features), irs::io_error);
  ASSERT_THROW((void)termItr->postings(all_features), irs::io_error);
  ASSERT_THROW((void)termItr->postings(all_features), irs::io_error);
  ASSERT_THROW((void)termItr->postings(all_features), irs::io_error);
  ASSERT_THROW((void)termItr->postings(all_features), irs::io_error);

  ASSERT_TRUE(dir.no_failures());
  // successful attempt
  auto docsItr = termItr->postings(irs::IndexFeatures::NONE);
  ASSERT_TRUE(docsItr->next());
  // successful attempt
  ASSERT_EQ(docsItr->value(), values->seek(docsItr->value()));
  // 'name' value in doc3
  ASSERT_EQ("A", irs::to_string<std::string_view>(actual_value->value.data()));
  ASSERT_TRUE(docsItr->next());
  ASSERT_EQ(docsItr->value(), values->seek(docsItr->value()));
  // 'name' value in doc3
  ASSERT_EQ("B", irs::to_string<std::string_view>(actual_value->value.data()));
  ASSERT_FALSE(docsItr->next());

  // validate live docs
  auto live_docs = segment.docs_iterator();
  ASSERT_TRUE(live_docs->next());
  ASSERT_EQ(1, live_docs->value());
  ASSERT_FALSE(live_docs->next());
  ASSERT_EQ(irs::doc_limits::eof(), live_docs->value());
}
