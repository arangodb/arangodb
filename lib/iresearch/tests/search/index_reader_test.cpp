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

#include "index/index_reader.hpp"

#include "formats/formats_10.hpp"
#include "index/doc_generator.hpp"
#include "index/index_tests.hpp"
#include "index/index_writer.hpp"
#include "store/memory_directory.hpp"
#include "tests_shared.hpp"
#include "utils/version_utils.hpp"

using namespace std::chrono_literals;

namespace {

irs::format* codec0;
irs::format* codec1;

irs::format::ptr get_codec0() {
  return irs::format::ptr(codec0, [](irs::format*) -> void {});
}
irs::format::ptr get_codec1() {
  return irs::format::ptr(codec1, [](irs::format*) -> void {});
}

}  // namespace

TEST(directory_reader_test, open_empty_directory) {
  irs::memory_directory dir;
  auto codec = irs::formats::get("1_0");
  ASSERT_NE(nullptr, codec);

  // No index
  ASSERT_THROW((irs::DirectoryReader{dir, codec}), irs::index_not_found);
}

TEST(directory_reader_test, open_empty_index) {
  irs::memory_directory dir;
  auto codec = irs::formats::get("1_0");
  ASSERT_NE(nullptr, codec);

  // Create empty index
  {
    auto writer = irs::IndexWriter::Make(dir, codec, irs::OM_CREATE);
    ASSERT_TRUE(writer->Commit());
  }

  auto rdr = irs::DirectoryReader(dir, codec);
  ASSERT_FALSE(!rdr);
  ASSERT_EQ(0, rdr.docs_count());
  ASSERT_EQ(0, rdr.live_docs_count());
  ASSERT_EQ(0, rdr.size());
  ASSERT_EQ(rdr.end(), rdr.begin());
}

TEST(directory_reader_test, open_newest_index) {
  struct test_index_meta_reader : public irs::index_meta_reader {
    bool last_segments_file(const irs::directory&,
                            std::string& out) const final {
      out = segments_file;
      return true;
    }
    void read(const irs::directory& /*dir*/, irs::IndexMeta& /*meta*/,
              std::string_view filename = std::string_view{}) final {
      read_file.assign(filename.data(), filename.size());
    }
    std::string segments_file;
    std::string read_file;
  };
  class test_format : public irs::format {
   public:
    explicit test_format(irs::type_info::type_id type) : type_{type} {}

    irs::index_meta_writer::ptr get_index_meta_writer() const final {
      return nullptr;
    }
    irs::index_meta_reader::ptr get_index_meta_reader() const final {
      return irs::memory::to_managed<irs::index_meta_reader>(index_meta_reader);
    }
    irs::segment_meta_writer::ptr get_segment_meta_writer() const final {
      return nullptr;
    }
    irs::segment_meta_reader::ptr get_segment_meta_reader() const final {
      return nullptr;
    }
    irs::document_mask_writer::ptr get_document_mask_writer() const final {
      return nullptr;
    }
    irs::document_mask_reader::ptr get_document_mask_reader() const final {
      return nullptr;
    }
    irs::field_writer::ptr get_field_writer(
      bool, irs::IResourceManager&) const final {
      return nullptr;
    }
    irs::field_reader::ptr get_field_reader(
      irs::IResourceManager&) const final {
      return nullptr;
    }
    irs::columnstore_writer::ptr get_columnstore_writer(
      bool, irs::IResourceManager&) const final {
      return nullptr;
    }
    irs::columnstore_reader::ptr get_columnstore_reader() const final {
      return nullptr;
    }
    irs::type_info::type_id type() const noexcept final { return type_; }

    mutable test_index_meta_reader index_meta_reader;
    irs::type_info::type_id type_;
  };

  struct test_format0 {};
  struct test_format1 {};

  test_format test_codec0(irs::type<test_format0>::id());
  test_format test_codec1(irs::type<test_format1>::id());
  irs::format_registrar test_format0_registrar(irs::type<test_format0>::get(),
                                               "", &get_codec0);
  irs::format_registrar test_format1_registrar(irs::type<test_format1>::get(),
                                               "", &get_codec1);
  test_index_meta_reader& test_reader0 = test_codec0.index_meta_reader;
  test_index_meta_reader& test_reader1 = test_codec1.index_meta_reader;
  codec0 = &test_codec0;
  codec1 = &test_codec1;

  irs::memory_directory dir;
  std::string codec0_file0("0seg0");
  std::string codec0_file1("0seg1");
  std::string codec1_file0("1seg0");
  std::string codec1_file1("1seg1");

  ASSERT_FALSE(!dir.create(codec0_file0));
  ASSERT_FALSE(!dir.create(codec1_file0));
  std::this_thread::sleep_for(
    1s);  // wait 1 sec to ensure index file timestamps differ
  ASSERT_FALSE(!dir.create(codec0_file1));
  ASSERT_FALSE(!dir.create(codec1_file1));

  test_reader0.read_file.clear();
  test_reader1.read_file.clear();
  test_reader0.segments_file = codec0_file0;
  test_reader1.segments_file = codec1_file1;
  irs::DirectoryReader{dir};
  ASSERT_TRUE(test_reader0.read_file.empty());  // file not read from codec0
  ASSERT_EQ(codec1_file1,
            test_reader1.read_file);  // check file read from codec1

  test_reader0.read_file.clear();
  test_reader1.read_file.clear();
  test_reader0.segments_file = codec0_file1;
  test_reader1.segments_file = codec1_file0;
  irs::DirectoryReader{dir};
  ASSERT_EQ(codec0_file1,
            test_reader0.read_file);            // check file read from codec0
  ASSERT_TRUE(test_reader1.read_file.empty());  // file not read from codec1

  codec0 = nullptr;
  codec1 = nullptr;
}

TEST(directory_reader_test, open) {
  tests::json_doc_generator gen(
    test_base::resource("simple_sequential.json"),
    [](tests::document& doc, const std::string& name,
       const tests::json_doc_generator::json_value& data) {
      if (tests::json_doc_generator::ValueType::STRING == data.vt) {
        doc.insert(std::make_shared<tests::string_field>(name, data.str));
      }
    });

  const tests::document* doc1 = gen.next();
  const tests::document* doc2 = gen.next();
  const tests::document* doc3 = gen.next();
  const tests::document* doc4 = gen.next();
  const tests::document* doc5 = gen.next();
  const tests::document* doc6 = gen.next();
  const tests::document* doc7 = gen.next();
  const tests::document* doc8 = gen.next();
  const tests::document* doc9 = gen.next();

  irs::memory_directory dir;
  auto codec_ptr = irs::formats::get("1_0");
  ASSERT_NE(nullptr, codec_ptr);

  // create index
  {
    // open writer
    auto writer = irs::IndexWriter::Make(dir, codec_ptr, irs::OM_CREATE);

    // add first segment
    ASSERT_TRUE(insert(*writer, doc1->indexed.begin(), doc1->indexed.end(),
                       doc1->stored.begin(), doc1->stored.end()));
    ASSERT_TRUE(insert(*writer, doc2->indexed.begin(), doc2->indexed.end(),
                       doc2->stored.begin(), doc2->stored.end()));
    ASSERT_TRUE(insert(*writer, doc3->indexed.begin(), doc3->indexed.end(),
                       doc3->stored.begin(), doc3->stored.end()));
    writer->Commit();
    tests::AssertSnapshotEquality(writer->GetSnapshot(),
                                  irs::DirectoryReader(dir, codec_ptr));

    // add second segment
    ASSERT_TRUE(insert(*writer, doc4->indexed.begin(), doc4->indexed.end(),
                       doc4->stored.begin(), doc4->stored.end()));
    ASSERT_TRUE(insert(*writer, doc5->indexed.begin(), doc5->indexed.end(),
                       doc5->stored.begin(), doc5->stored.end()));
    ASSERT_TRUE(insert(*writer, doc6->indexed.begin(), doc6->indexed.end(),
                       doc6->stored.begin(), doc6->stored.end()));
    ASSERT_TRUE(insert(*writer, doc7->indexed.begin(), doc7->indexed.end(),
                       doc7->stored.begin(), doc7->stored.end()));
    writer->Commit();
    tests::AssertSnapshotEquality(writer->GetSnapshot(),
                                  irs::DirectoryReader(dir, codec_ptr));

    // add third segment
    ASSERT_TRUE(insert(*writer, doc8->indexed.begin(), doc8->indexed.end(),
                       doc8->stored.begin(), doc8->stored.end()));
    ASSERT_TRUE(insert(*writer, doc9->indexed.begin(), doc9->indexed.end(),
                       doc9->stored.begin(), doc9->stored.end()));
    writer->Commit();
    tests::AssertSnapshotEquality(writer->GetSnapshot(),
                                  irs::DirectoryReader(dir, codec_ptr));
  }

  // open reader
  auto rdr = irs::DirectoryReader(dir, codec_ptr);
  ASSERT_FALSE(!rdr);
  ASSERT_EQ(9, rdr.docs_count());
  ASSERT_EQ(9, rdr.live_docs_count());
  ASSERT_EQ(3, rdr.size());
  ASSERT_EQ("segments_3", rdr.Meta().filename);
  ASSERT_EQ(rdr.size(), rdr.Meta().index_meta.segments.size());

  // check subreaders
  auto sub = rdr.begin();

  // first segment
  {
    ASSERT_NE(rdr.end(), sub);
    ASSERT_EQ(1, sub->size());
    ASSERT_EQ(3, sub->docs_count());
    ASSERT_EQ(3, sub->live_docs_count());

    const auto* column = sub->column("name");
    ASSERT_NE(nullptr, column);
    auto values = column->iterator(irs::ColumnHint::kNormal);
    ASSERT_NE(nullptr, values);
    auto* actual_value = irs::get<irs::payload>(*values);
    ASSERT_NE(nullptr, actual_value);

    // read documents
    ASSERT_EQ(1, values->seek(1));
    ASSERT_EQ("A", irs::to_string<std::string_view>(
                     actual_value->value.data()));  // 'name' value in doc1
    ASSERT_EQ(2, values->seek(2));
    ASSERT_EQ("B", irs::to_string<std::string_view>(
                     actual_value->value.data()));  // 'name' value in doc2
    ASSERT_EQ(3, values->seek(3));
    ASSERT_EQ("C", irs::to_string<std::string_view>(
                     actual_value->value.data()));  // 'name' value in doc3

    // read invalid document
    ASSERT_TRUE(irs::doc_limits::eof(values->seek(4)));
  }

  // second segment
  {
    ++sub;
    ASSERT_NE(rdr.end(), sub);
    ASSERT_EQ(1, sub->size());
    ASSERT_EQ(4, sub->docs_count());
    ASSERT_EQ(4, sub->live_docs_count());

    const auto* column = sub->column("name");
    ASSERT_NE(nullptr, column);
    auto values = column->iterator(irs::ColumnHint::kNormal);
    ASSERT_NE(nullptr, values);
    auto* actual_value = irs::get<irs::payload>(*values);
    ASSERT_NE(nullptr, actual_value);

    // read documents
    ASSERT_EQ(1, values->seek(1));
    ASSERT_EQ("D", irs::to_string<std::string_view>(
                     actual_value->value.data()));  // 'name' value in doc4
    ASSERT_EQ(2, values->seek(2));
    ASSERT_EQ("E", irs::to_string<std::string_view>(
                     actual_value->value.data()));  // 'name' value in doc5
    ASSERT_EQ(3, values->seek(3));
    ASSERT_EQ("F", irs::to_string<std::string_view>(
                     actual_value->value.data()));  // 'name' value in doc6
    ASSERT_EQ(4, values->seek(4));
    ASSERT_EQ("G", irs::to_string<std::string_view>(
                     actual_value->value.data()));  // 'name' value in doc7

    // read invalid document
    ASSERT_TRUE(irs::doc_limits::eof(values->seek(5)));
  }

  // third segment
  {
    ++sub;
    ASSERT_NE(rdr.end(), sub);
    ASSERT_EQ(1, sub->size());
    ASSERT_EQ(2, sub->docs_count());
    ASSERT_EQ(2, sub->live_docs_count());

    const auto* column = sub->column("name");
    ASSERT_NE(nullptr, column);
    auto values = column->iterator(irs::ColumnHint::kNormal);
    ASSERT_NE(nullptr, values);
    auto* actual_value = irs::get<irs::payload>(*values);
    ASSERT_NE(nullptr, actual_value);

    // read documents
    ASSERT_EQ(1, values->seek(1));
    ASSERT_EQ("H", irs::to_string<std::string_view>(
                     actual_value->value.data()));  // 'name' value in doc8
    ASSERT_EQ(2, values->seek(2));
    ASSERT_EQ("I", irs::to_string<std::string_view>(
                     actual_value->value.data()));  // 'name' value in doc9

    // read invalid document
    ASSERT_TRUE(irs::doc_limits::eof(values->seek(3)));
  }

  ++sub;
  ASSERT_EQ(rdr.end(), sub);
}

// ----------------------------------------------------------------------------
// --SECTION--                                                   Segment
// reader
// ----------------------------------------------------------------------------

TEST(segment_reader_test, segment_reader_has) {
  auto codec = irs::formats::get("1_0");
  ASSERT_NE(nullptr, codec);

  std::string filename;

  // has none (default)
  {
    irs::memory_directory dir;
    auto writer = codec->get_segment_meta_writer();
    auto reader = codec->get_segment_meta_reader();
    irs::SegmentMeta expected;

    writer->write(dir, filename, expected);

    irs::SegmentMeta meta;

    reader->read(dir, meta, filename);

    ASSERT_EQ(expected, meta);
    ASSERT_FALSE(meta.column_store);
    ASSERT_FALSE(irs::HasRemovals(meta));
  }

  // has column store
  {
    irs::memory_directory dir;
    auto writer = codec->get_segment_meta_writer();
    auto reader = codec->get_segment_meta_reader();
    irs::SegmentMeta expected;

    expected.column_store = true;
    writer->write(dir, filename, expected);

    irs::SegmentMeta meta;

    reader->read(dir, meta, filename);

    ASSERT_EQ(expected, meta);
    ASSERT_TRUE(meta.column_store);
    ASSERT_FALSE(irs::HasRemovals(meta));
  }

  // has document mask
  {
    irs::memory_directory dir;
    auto writer = codec->get_segment_meta_writer();
    auto reader = codec->get_segment_meta_reader();
    auto docs_mask_writer = codec->get_document_mask_writer();
    irs::SegmentMeta expected;

    expected.docs_count = 43;
    expected.live_docs_count = 42;
    expected.version = 0;
    irs::DocumentMask mask{{irs::IResourceManager::kNoop}};
    mask.insert(0);
    docs_mask_writer->write(dir, expected, mask);
    writer->write(dir, filename, expected);

    irs::SegmentMeta meta;

    reader->read(dir, meta, filename);

    ASSERT_EQ(expected, meta);
    ASSERT_FALSE(meta.column_store);
    ASSERT_TRUE(irs::HasRemovals(meta));
  }

  // has all
  {
    irs::memory_directory dir;
    auto writer = codec->get_segment_meta_writer();
    auto reader = codec->get_segment_meta_reader();
    auto docs_mask_writer = codec->get_document_mask_writer();
    irs::SegmentMeta expected;

    expected.docs_count = 43;
    expected.live_docs_count = 42;
    expected.column_store = true;
    expected.version = 1;
    irs::DocumentMask mask{{irs::IResourceManager::kNoop}};
    mask.insert(0);
    docs_mask_writer->write(dir, expected, mask);
    writer->write(dir, filename, expected);

    irs::SegmentMeta meta;

    reader->read(dir, meta, filename);

    ASSERT_EQ(expected, meta);
    ASSERT_TRUE(meta.column_store);
    ASSERT_TRUE(irs::HasRemovals(meta));
  }
}

TEST(segment_reader_test, open_invalid_segment) {
  irs::memory_directory dir;
  auto codec_ptr = irs::formats::get("1_0");
  ASSERT_NE(nullptr, codec_ptr);

  /* open invalid segment */
  {
    irs::SegmentMeta meta;
    meta.codec = codec_ptr;
    meta.name = "invalid_segment_name";

    ASSERT_THROW(irs::SegmentReader(dir, meta, irs::IndexReaderOptions{}),
                 irs::io_error);
  }
}

TEST(segment_reader_test, open) {
  tests::json_doc_generator gen(test_base::resource("simple_sequential.json"),
                                &tests::generic_json_field_factory);
  const tests::document* doc1 = gen.next();
  const tests::document* doc2 = gen.next();
  const tests::document* doc3 = gen.next();
  const tests::document* doc4 = gen.next();
  const tests::document* doc5 = gen.next();

  irs::memory_directory dir;
  auto codec_ptr = irs::formats::get("1_0");
  ASSERT_NE(nullptr, codec_ptr);
  irs::DirectoryReader writer_snapshot;
  {
    // open writer
    auto writer = irs::IndexWriter::Make(dir, codec_ptr, irs::OM_CREATE);

    // add first segment
    ASSERT_TRUE(insert(*writer, doc1->indexed.begin(), doc1->indexed.end(),
                       doc1->stored.begin(), doc1->stored.end()));
    ASSERT_TRUE(insert(*writer, doc2->indexed.begin(), doc2->indexed.end(),
                       doc2->stored.begin(), doc2->stored.end()));
    ASSERT_TRUE(insert(*writer, doc3->indexed.begin(), doc3->indexed.end(),
                       doc3->stored.begin(), doc3->stored.end()));
    ASSERT_TRUE(insert(*writer, doc4->indexed.begin(), doc4->indexed.end(),
                       doc4->stored.begin(), doc4->stored.end()));
    ASSERT_TRUE(insert(*writer, doc5->indexed.begin(), doc5->indexed.end(),
                       doc5->stored.begin(), doc5->stored.end()));
    writer->Commit();
    writer_snapshot = writer->GetSnapshot();
  }

  // check segment
  {
    irs::SegmentMeta meta;
    meta.codec = codec_ptr;
    meta.column_store = true;
    meta.docs_count = 5;
    meta.live_docs_count = 5;
    meta.name = "_1";
    meta.version = IRESEARCH_VERSION;

    auto rdr = irs::SegmentReader(dir, meta, irs::IndexReaderOptions{});
    ASSERT_FALSE(!rdr);
    ASSERT_EQ(1, rdr.size());
    ASSERT_EQ(meta.docs_count, rdr.docs_count());
    ASSERT_EQ(meta.live_docs_count, rdr.live_docs_count());

    auto& segment = *rdr.begin();
    const auto* column = segment.column("name");
    ASSERT_NE(nullptr, column);
    auto values = column->iterator(irs::ColumnHint::kNormal);
    ASSERT_NE(nullptr, values);
    auto* actual_value = irs::get<irs::payload>(*values);
    ASSERT_NE(nullptr, actual_value);

    // read documents
    ASSERT_EQ(1, values->seek(1));
    ASSERT_EQ("A", irs::to_string<std::string_view>(
                     actual_value->value.data()));  // 'name' value in doc1
    ASSERT_EQ(2, values->seek(2));
    ASSERT_EQ("B", irs::to_string<std::string_view>(
                     actual_value->value.data()));  // 'name' value in doc2
    ASSERT_EQ(3, values->seek(3));
    ASSERT_EQ("C", irs::to_string<std::string_view>(
                     actual_value->value.data()));  // 'name' value in doc3
    ASSERT_EQ(4, values->seek(4));
    ASSERT_EQ("D", irs::to_string<std::string_view>(
                     actual_value->value.data()));  // 'name' value in doc4
    ASSERT_EQ(5, values->seek(5));
    ASSERT_EQ("E", irs::to_string<std::string_view>(
                     actual_value->value.data()));  // 'name' value in doc5

    ASSERT_TRUE(
      irs::doc_limits::eof(values->seek(6)));  // read invalid document

    // check iterators
    {
      auto it = rdr.begin();
      ASSERT_EQ(&rdr, &*it); /* should return self */
      ASSERT_NE(rdr.end(), it);
      ++it;
      ASSERT_EQ(rdr.end(), it);
    }

    // check field names
    {
      auto it = rdr.fields();
      ASSERT_TRUE(it->next());
      ASSERT_EQ("duplicated", it->value().meta().name);
      ASSERT_TRUE(it->next());
      ASSERT_EQ("name", it->value().meta().name);
      ASSERT_TRUE(it->next());
      ASSERT_EQ("prefix", it->value().meta().name);
      ASSERT_TRUE(it->next());
      ASSERT_EQ("same", it->value().meta().name);
      ASSERT_TRUE(it->next());
      ASSERT_EQ("seq", it->value().meta().name);
      ASSERT_TRUE(it->next());
      ASSERT_EQ("value", it->value().meta().name);
      ASSERT_FALSE(it->next());
    }

    // check live docs
    {
      auto it = rdr.docs_iterator();
      ASSERT_TRUE(it->next());
      ASSERT_EQ(1, it->value());
      ASSERT_TRUE(it->next());
      ASSERT_EQ(2, it->value());
      ASSERT_TRUE(it->next());
      ASSERT_EQ(3, it->value());
      ASSERT_TRUE(it->next());
      ASSERT_EQ(4, it->value());
      ASSERT_TRUE(it->next());
      ASSERT_EQ(5, it->value());
      ASSERT_FALSE(it->next());
      ASSERT_FALSE(it->next());
    }

    // check field metadata
    {
      {
        auto it = rdr.fields();
        size_t size = 0;
        while (it->next()) {
          ++size;
        }
        ASSERT_EQ(6, size);
      }

      // check field
      {
        const std::string_view name = "name";
        auto field = rdr.field(name);
        ASSERT_EQ(name, field->meta().name);

        // check terms
        auto terms = rdr.field(name);
        ASSERT_NE(nullptr, terms);

        ASSERT_EQ(5, terms->size());
        ASSERT_EQ(5, terms->docs_count());
        ASSERT_EQ(irs::ViewCast<irs::byte_type>(std::string_view("A")),
                  (terms->min)());
        ASSERT_EQ(irs::ViewCast<irs::byte_type>(std::string_view("E")),
                  (terms->max)());

        auto term = terms->iterator(irs::SeekMode::NORMAL);

        // check term: A
        {
          ASSERT_TRUE(term->next());
          ASSERT_EQ(irs::ViewCast<irs::byte_type>(std::string_view("A")),
                    term->value());

          // check docs
          {
            auto docs = term->postings(irs::IndexFeatures::NONE);
            ASSERT_TRUE(docs->next());
            ASSERT_EQ(1, docs->value());
            ASSERT_FALSE(docs->next());
          }
        }

        // check term: B
        {
          ASSERT_TRUE(term->next());
          ASSERT_EQ(irs::ViewCast<irs::byte_type>(std::string_view("B")),
                    term->value());

          // check docs
          {
            auto docs = term->postings(irs::IndexFeatures::NONE);
            ASSERT_TRUE(docs->next());
            ASSERT_EQ(2, docs->value());
            ASSERT_FALSE(docs->next());
            ASSERT_FALSE(docs->next());
          }
        }

        // check term: C
        {
          ASSERT_TRUE(term->next());
          ASSERT_EQ(irs::ViewCast<irs::byte_type>(std::string_view("C")),
                    term->value());

          // check docs
          {
            auto docs = term->postings(irs::IndexFeatures::NONE);
            ASSERT_TRUE(docs->next());
            ASSERT_EQ(3, docs->value());
            ASSERT_FALSE(docs->next());
            ASSERT_FALSE(docs->next());
          }
        }

        // check term: D
        {
          ASSERT_TRUE(term->next());
          ASSERT_EQ(irs::ViewCast<irs::byte_type>(std::string_view("D")),
                    term->value());

          // check docs
          {
            auto docs = term->postings(irs::IndexFeatures::NONE);
            ASSERT_TRUE(docs->next());
            ASSERT_EQ(4, docs->value());
            ASSERT_FALSE(docs->next());
            ASSERT_FALSE(docs->next());
          }
        }

        // check term: E
        {
          ASSERT_TRUE(term->next());
          ASSERT_EQ(irs::ViewCast<irs::byte_type>(std::string_view("E")),
                    term->value());

          // check docs
          {
            auto docs = term->postings(irs::IndexFeatures::NONE);
            ASSERT_TRUE(docs->next());
            ASSERT_EQ(5, docs->value());
            ASSERT_FALSE(docs->next());
            ASSERT_FALSE(docs->next());
          }
        }

        ASSERT_FALSE(term->next());
      }

      // check field
      {
        const std::string_view name = "seq";
        auto field = rdr.field(name);
        ASSERT_EQ(name, field->meta().name);

        // check terms
        auto terms = rdr.field(name);
        ASSERT_NE(nullptr, terms);
      }

      // check field
      {
        const std::string_view name = "same";
        auto field = rdr.field(name);
        ASSERT_EQ(name, field->meta().name);

        // check terms
        auto terms = rdr.field(name);
        ASSERT_NE(nullptr, terms);
        ASSERT_EQ(1, terms->size());
        ASSERT_EQ(5, terms->docs_count());
        ASSERT_EQ(irs::ViewCast<irs::byte_type>(std::string_view("xyz")),
                  (terms->min)());
        ASSERT_EQ(irs::ViewCast<irs::byte_type>(std::string_view("xyz")),
                  (terms->max)());

        auto term = terms->iterator(irs::SeekMode::NORMAL);

        // check term: xyz
        {
          ASSERT_TRUE(term->next());
          ASSERT_EQ(irs::ViewCast<irs::byte_type>(std::string_view("xyz")),
                    term->value());

          /* check docs */
          {
            auto docs = term->postings(irs::IndexFeatures::NONE);
            ASSERT_TRUE(docs->next());
            ASSERT_EQ(1, docs->value());
            ASSERT_TRUE(docs->next());
            ASSERT_EQ(2, docs->value());
            ASSERT_TRUE(docs->next());
            ASSERT_EQ(3, docs->value());
            ASSERT_TRUE(docs->next());
            ASSERT_EQ(4, docs->value());
            ASSERT_TRUE(docs->next());
            ASSERT_EQ(5, docs->value());
            ASSERT_FALSE(docs->next());
            ASSERT_FALSE(docs->next());
          }
        }

        ASSERT_FALSE(term->next());
      }

      // check field
      {
        const std::string_view name = "duplicated";
        auto field = rdr.field(name);
        ASSERT_EQ(name, field->meta().name);

        // check terms
        auto terms = rdr.field(name);
        ASSERT_NE(nullptr, terms);
        ASSERT_EQ(2, terms->size());
        ASSERT_EQ(4, terms->docs_count());
        ASSERT_EQ(irs::ViewCast<irs::byte_type>(std::string_view("abcd")),
                  (terms->min)());
        ASSERT_EQ(irs::ViewCast<irs::byte_type>(std::string_view("vczc")),
                  (terms->max)());

        auto term = terms->iterator(irs::SeekMode::NORMAL);

        // check term: abcd
        {
          ASSERT_TRUE(term->next());
          ASSERT_EQ(irs::ViewCast<irs::byte_type>(std::string_view("abcd")),
                    term->value());

          // check docs
          {
            auto docs = term->postings(irs::IndexFeatures::NONE);
            ASSERT_TRUE(docs->next());
            ASSERT_EQ(1, docs->value());
            ASSERT_TRUE(docs->next());
            ASSERT_EQ(5, docs->value());
            ASSERT_FALSE(docs->next());
            ASSERT_FALSE(docs->next());
          }
        }

        // check term: vczc
        {
          ASSERT_TRUE(term->next());
          ASSERT_EQ(irs::ViewCast<irs::byte_type>(std::string_view("vczc")),
                    term->value());

          // check docs
          {
            auto docs = term->postings(irs::IndexFeatures::NONE);
            ASSERT_TRUE(docs->next());
            ASSERT_EQ(2, docs->value());
            ASSERT_TRUE(docs->next());
            ASSERT_EQ(3, docs->value());
            ASSERT_FALSE(docs->next());
            ASSERT_FALSE(docs->next());
          }
        }

        ASSERT_FALSE(term->next());
      }

      // check field
      {
        const std::string_view name = "prefix";
        auto field = rdr.field(name);
        ASSERT_EQ(name, field->meta().name);

        // check terms
        auto terms = rdr.field(name);
        ASSERT_NE(nullptr, terms);
        ASSERT_EQ(2, terms->size());
        ASSERT_EQ(2, terms->docs_count());
        ASSERT_EQ(irs::ViewCast<irs::byte_type>(std::string_view("abcd")),
                  (terms->min)());
        ASSERT_EQ(irs::ViewCast<irs::byte_type>(std::string_view("abcde")),
                  (terms->max)());

        auto term = terms->iterator(irs::SeekMode::NORMAL);

        // check term: abcd
        {
          ASSERT_TRUE(term->next());
          ASSERT_EQ(irs::ViewCast<irs::byte_type>(std::string_view("abcd")),
                    term->value());

          // check docs
          {
            auto docs = term->postings(irs::IndexFeatures::NONE);
            ASSERT_TRUE(docs->next());
            ASSERT_EQ(1, docs->value());
            ASSERT_FALSE(docs->next());
            ASSERT_FALSE(docs->next());
          }
        }

        // check term: abcde
        {
          ASSERT_TRUE(term->next());
          ASSERT_EQ(irs::ViewCast<irs::byte_type>(std::string_view("abcde")),
                    term->value());

          // check docs
          {
            auto docs = term->postings(irs::IndexFeatures::NONE);
            ASSERT_TRUE(docs->next());
            ASSERT_EQ(4, docs->value());
            ASSERT_FALSE(docs->next());
            ASSERT_FALSE(docs->next());
          }
        }

        ASSERT_FALSE(term->next());
      }

      // invalid field
      {
        const std::string_view name = "invalid_field";
        ASSERT_EQ(nullptr, rdr.field(name));
      }
    }
  }
}
