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
#include "index/index_reader.hpp"
#include "formats/formats_10.hpp"
#include "index/index_writer.hpp"
#include "store/memory_directory.hpp"
#include "index/doc_generator.hpp"
#include "index/index_tests.hpp"
#include "utils/version_utils.hpp"
#include "utils/utf8_path.hpp"

using namespace std::chrono_literals;

namespace {

irs::format* codec0;
irs::format* codec1;

irs::format::ptr get_codec0() { return irs::format::ptr(codec0, [](irs::format*)->void{}); }
irs::format::ptr get_codec1() { return irs::format::ptr(codec1, [](irs::format*)->void{}); }

}

// ----------------------------------------------------------------------------
// --SECTION--                                           Composite index reader
// ----------------------------------------------------------------------------

TEST(directory_reader_test, open_empty_directory) {
  irs::memory_directory dir;
  auto codec = irs::formats::get("1_0");
  ASSERT_NE(nullptr, codec);

  /* no index */
  ASSERT_THROW(irs::directory_reader::open(dir, codec), irs::index_not_found);
}

TEST(directory_reader_test, open_empty_index) {
  irs::memory_directory dir;
  auto codec = irs::formats::get("1_0");
  ASSERT_NE(nullptr, codec);

  /* create empty index */
  irs::index_writer::make(dir, codec, irs::OM_CREATE)->commit();

  /* open reader */
  auto rdr = irs::directory_reader::open(dir, codec);
  ASSERT_FALSE(!rdr);
  ASSERT_EQ(0, rdr.docs_count());
  ASSERT_EQ(0, rdr.live_docs_count());
  ASSERT_EQ(0, rdr.size());
  ASSERT_EQ(rdr.end(), rdr.begin());
}

TEST(directory_reader_test, open_newest_index) {
  struct test_index_meta_reader: public irs::index_meta_reader {
    virtual bool last_segments_file(const irs::directory&, std::string& out) const override {
      out = segments_file;
      return true;
    }
    virtual void read(const irs::directory& dir, irs::index_meta& meta, const irs::string_ref& filename = irs::string_ref::NIL) override {
      read_file.assign(filename.c_str(), filename.size());
    };
    std::string segments_file;
    std::string read_file;
  };
  class test_format: public irs::format {
   public:
    mutable test_index_meta_reader index_meta_reader;
    test_format(const irs::type_info& type): irs::format(type) {}
    virtual irs::index_meta_writer::ptr get_index_meta_writer() const override { return nullptr; }
    virtual irs::index_meta_reader::ptr get_index_meta_reader() const override {
      return irs::memory::to_managed<irs::index_meta_reader, false>(&index_meta_reader);
    }
    virtual irs::segment_meta_writer::ptr get_segment_meta_writer() const override { return nullptr; }
    virtual irs::segment_meta_reader::ptr get_segment_meta_reader() const override { return nullptr; }
    virtual irs::document_mask_writer::ptr get_document_mask_writer() const override { return nullptr; }
    virtual irs::document_mask_reader::ptr get_document_mask_reader() const override { return nullptr; }
    virtual irs::field_writer::ptr get_field_writer(bool volatile_attributes) const override { return nullptr; }
    virtual irs::field_reader::ptr get_field_reader() const override { return nullptr; }
    virtual irs::column_meta_writer::ptr get_column_meta_writer() const override { return nullptr; }
    virtual irs::column_meta_reader::ptr get_column_meta_reader() const override { return nullptr; }
    virtual irs::columnstore_writer::ptr get_columnstore_writer() const override { return nullptr; }
    virtual irs::columnstore_reader::ptr get_columnstore_reader() const override { return nullptr; }
  };

  struct test_format0 { };
  struct test_format1 { };

  test_format test_codec0(irs::type<test_format0>::get());
  test_format test_codec1(irs::type<test_format1>::get());
  irs::format_registrar test_format0_registrar(irs::type<test_format0>::get(), irs::string_ref::NIL, &get_codec0);
  irs::format_registrar test_format1_registrar(irs::type<test_format1>::get(), irs::string_ref::NIL, &get_codec1);
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
  std::this_thread::sleep_for(1s); // wait 1 sec to ensure index file timestamps differ
  ASSERT_FALSE(!dir.create(codec0_file1));
  ASSERT_FALSE(!dir.create(codec1_file1));

  test_reader0.read_file.clear();
  test_reader1.read_file.clear();
  test_reader0.segments_file = codec0_file0;
  test_reader1.segments_file = codec1_file1;
  irs::directory_reader::open(dir);
  ASSERT_TRUE(test_reader0.read_file.empty()); // file not read from codec0
  ASSERT_EQ(codec1_file1, test_reader1.read_file);  // check file read from codec1

  test_reader0.read_file.clear();
  test_reader1.read_file.clear();
  test_reader0.segments_file = codec0_file1;
  test_reader1.segments_file = codec1_file0;
  irs::directory_reader::open(dir);
  ASSERT_EQ(codec0_file1, test_reader0.read_file); // check file read from codec0
  ASSERT_TRUE(test_reader1.read_file.empty()); // file not read from codec1

  codec0 = nullptr;
  codec1 = nullptr;
}

TEST(directory_reader_test, open) {
  tests::json_doc_generator gen(
    test_base::resource("simple_sequential.json"),
    [] (tests::document& doc, const std::string& name, const tests::json_doc_generator::json_value& data) {
    if (tests::json_doc_generator::ValueType::STRING == data.vt) {
      doc.insert(std::make_shared<tests::templates::string_field>(
        irs::string_ref(name),
        data.str
      ));
    }
  });

  tests::document const* doc1 = gen.next();
  tests::document const* doc2 = gen.next();
  tests::document const* doc3 = gen.next();
  tests::document const* doc4 = gen.next();
  tests::document const* doc5 = gen.next();
  tests::document const* doc6 = gen.next();
  tests::document const* doc7 = gen.next();
  tests::document const* doc8 = gen.next();
  tests::document const* doc9 = gen.next(); 

  irs::memory_directory dir;
  auto codec_ptr = irs::formats::get("1_0");
  ASSERT_NE(nullptr, codec_ptr);

  // create index
  {
    // open writer
    auto writer = irs::index_writer::make(dir, codec_ptr, irs::OM_CREATE);

    // add first segment
    ASSERT_TRUE(insert(*writer,
      doc1->indexed.begin(), doc1->indexed.end(),
      doc1->stored.begin(), doc1->stored.end()
    ));
    ASSERT_TRUE(insert(*writer,
      doc2->indexed.begin(), doc2->indexed.end(),
      doc2->stored.begin(), doc2->stored.end()
    ));
    ASSERT_TRUE(insert(*writer,
      doc3->indexed.begin(), doc3->indexed.end(),
      doc3->stored.begin(), doc3->stored.end()
    ));
    writer->commit();

    // add second segment
    ASSERT_TRUE(insert(*writer,
      doc4->indexed.begin(), doc4->indexed.end(),
      doc4->stored.begin(), doc4->stored.end()
    ));
    ASSERT_TRUE(insert(*writer,
      doc5->indexed.begin(), doc5->indexed.end(),
      doc5->stored.begin(), doc5->stored.end()
    ));
    ASSERT_TRUE(insert(*writer,
      doc6->indexed.begin(), doc6->indexed.end(),
      doc6->stored.begin(), doc6->stored.end()
    ));
    ASSERT_TRUE(insert(*writer,
      doc7->indexed.begin(), doc7->indexed.end(),
      doc7->stored.begin(), doc7->stored.end()
    ));
    writer->commit();

    // add third segment
    ASSERT_TRUE(insert(*writer,
      doc8->indexed.begin(), doc8->indexed.end(),
      doc8->stored.begin(), doc8->stored.end()
    ));
    ASSERT_TRUE(insert(*writer,
      doc9->indexed.begin(), doc9->indexed.end(),
      doc9->stored.begin(), doc9->stored.end()
    ));
    writer->commit();
  }

  // open reader
  auto rdr = irs::directory_reader::open(dir, codec_ptr);
  ASSERT_FALSE(!rdr);
  ASSERT_EQ(9, rdr.docs_count());
  ASSERT_EQ(9, rdr.live_docs_count());  
  ASSERT_EQ(3, rdr.size());
  ASSERT_EQ("segments_3", rdr.meta().filename);
  ASSERT_EQ(rdr.size(), rdr.meta().meta.size());

  // check subreaders
  auto sub = rdr.begin();

  irs::bytes_ref actual_value;

  // first segment
  {
    ASSERT_NE(rdr.end(), sub);
    ASSERT_EQ(1, sub->size());
    ASSERT_EQ(3, sub->docs_count());
    ASSERT_EQ(3, sub->live_docs_count());

    const auto* column = sub->column_reader("name");
    ASSERT_NE(nullptr, column);
    auto values = column->values();

    // read documents
    ASSERT_TRUE(values(1, actual_value));
    ASSERT_EQ("A", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc1
    ASSERT_TRUE(values(2, actual_value));
    ASSERT_EQ("B", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc2
    ASSERT_TRUE(values(3, actual_value));
    ASSERT_EQ("C", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc3

    // read invalid document
    ASSERT_FALSE(values(4, actual_value));
  }

  // second segment
  {
    ++sub;
    ASSERT_NE(rdr.end(), sub);
    ASSERT_EQ(1, sub->size());
    ASSERT_EQ(4, sub->docs_count());
    ASSERT_EQ(4, sub->live_docs_count());

    const auto* column = sub->column_reader("name");
    ASSERT_NE(nullptr, column);
    auto values = column->values();

    // read documents
    ASSERT_TRUE(values(1, actual_value));
    ASSERT_EQ("D", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc4
    ASSERT_TRUE(values(2, actual_value));
    ASSERT_EQ("E", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc5
    ASSERT_TRUE(values(3, actual_value));
    ASSERT_EQ("F", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc6
    ASSERT_TRUE(values(4, actual_value));
    ASSERT_EQ("G", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc7

    // read invalid document
    ASSERT_FALSE(values(5, actual_value));
  }

  // third segment
  {
    ++sub;
    ASSERT_NE(rdr.end(), sub);
    ASSERT_EQ(1, sub->size());
    ASSERT_EQ(2, sub->docs_count());
    ASSERT_EQ(2, sub->live_docs_count());

    const auto* column = sub->column_reader("name");
    ASSERT_NE(nullptr, column);
    auto values = column->values();

    // read documents
    ASSERT_TRUE(values(1, actual_value));
    ASSERT_EQ("H", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc8
    ASSERT_TRUE(values(2, actual_value));
    ASSERT_EQ("I", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc9

    // read invalid document
    ASSERT_FALSE(values(3, actual_value));
  }

  ++sub;
  ASSERT_EQ(rdr.end(), sub);
}

// ----------------------------------------------------------------------------
// --SECTION--                                                   Segment reader 
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
    irs::segment_meta expected;

    writer->write(dir, filename, expected);

    irs::segment_meta meta;

    reader->read(dir, meta, filename);

    ASSERT_EQ(expected, meta);
    ASSERT_FALSE(irs::segment_reader::has<irs::columnstore_reader>(meta));
    ASSERT_FALSE(irs::segment_reader::has<irs::document_mask_reader>(meta));
  }

  // has column store
  {
    irs::memory_directory dir;
    auto writer = codec->get_segment_meta_writer();
    auto reader = codec->get_segment_meta_reader();
    irs::segment_meta expected;

    expected.column_store = true;
    writer->write(dir, filename, expected);

    irs::segment_meta meta;

    reader->read(dir, meta, filename);

    ASSERT_EQ(expected, meta);
    ASSERT_TRUE(irs::segment_reader::has<irs::columnstore_reader>(meta));
    ASSERT_FALSE(irs::segment_reader::has<irs::document_mask_reader>(meta));
  }

  // has document mask
  {
    irs::memory_directory dir;
    auto writer = codec->get_segment_meta_writer();
    auto reader = codec->get_segment_meta_reader();
    auto docs_mask_writer = codec->get_document_mask_writer();
    irs::segment_meta expected;

    expected.docs_count = 43;
    expected.live_docs_count = 42;
    expected.version = 0;
    docs_mask_writer->write(dir, expected, {0});
    writer->write(dir, filename, expected);

    irs::segment_meta meta;

    reader->read(dir, meta, filename);

    ASSERT_EQ(expected, meta);
    ASSERT_FALSE(irs::segment_reader::has<irs::columnstore_reader>(meta));
    ASSERT_TRUE(irs::segment_reader::has<irs::document_mask_reader>(meta));
  }

  // has all
  {
    irs::memory_directory dir;
    auto writer = codec->get_segment_meta_writer();
    auto reader = codec->get_segment_meta_reader();
    auto docs_mask_writer = codec->get_document_mask_writer();
    irs::segment_meta expected;

    expected.docs_count = 43;
    expected.live_docs_count = 42;
    expected.column_store = true;
    expected.version = 1;
    docs_mask_writer->write(dir, expected, {0});
    writer->write(dir, filename, expected);

    irs::segment_meta meta;

    reader->read(dir, meta, filename);

    ASSERT_EQ(expected, meta);
    ASSERT_TRUE(irs::segment_reader::has<irs::columnstore_reader>(meta));
    ASSERT_TRUE(irs::segment_reader::has<irs::document_mask_reader>(meta));
  }
}

TEST(segment_reader_test, open_invalid_segment) {
  irs::memory_directory dir;
  auto codec_ptr = irs::formats::get("1_0");
  ASSERT_NE(nullptr, codec_ptr);

  /* open invalid segment */
  {
    irs::segment_meta meta;
    meta.codec = codec_ptr;
    meta.name = "invalid_segment_name";

    ASSERT_THROW(irs::segment_reader::open(dir, meta), irs::io_error);
  }
}

TEST(segment_reader_test, open) {
  tests::json_doc_generator gen(
    test_base::resource("simple_sequential.json"),
    &tests::generic_json_field_factory);
  tests::document const* doc1 = gen.next();
  tests::document const* doc2 = gen.next();
  tests::document const* doc3 = gen.next();
  tests::document const* doc4 = gen.next();
  tests::document const* doc5 = gen.next();
  tests::document const* doc6 = gen.next();
  tests::document const* doc7 = gen.next();
  tests::document const* doc8 = gen.next();
  tests::document const* doc9 = gen.next(); 

  irs::memory_directory dir;
  auto codec_ptr = irs::formats::get("1_0");
  ASSERT_NE(nullptr, codec_ptr);
  {
    // open writer
    auto writer = irs::index_writer::make(dir, codec_ptr, irs::OM_CREATE);

    // add first segment
    ASSERT_TRUE(insert(*writer,
      doc1->indexed.begin(), doc1->indexed.end(),
      doc1->stored.begin(), doc1->stored.end()
    ));
    ASSERT_TRUE(insert(*writer,
      doc2->indexed.begin(), doc2->indexed.end(),
      doc2->stored.begin(), doc2->stored.end()
    ));
    ASSERT_TRUE(insert(*writer,
      doc3->indexed.begin(), doc3->indexed.end(),
      doc3->stored.begin(), doc3->stored.end()
    ));
    ASSERT_TRUE(insert(*writer,
      doc4->indexed.begin(), doc4->indexed.end(),
      doc4->stored.begin(), doc4->stored.end()
    ));
    ASSERT_TRUE(insert(*writer,
      doc5->indexed.begin(), doc5->indexed.end(),
      doc5->stored.begin(), doc5->stored.end()
    ));
    writer->commit();
  }

  // check segment
  {
    irs::segment_meta meta;
    meta.codec = codec_ptr;
    meta.column_store = true;
    meta.docs_count = 5;
    meta.name = "_1";
    meta.version = IRESEARCH_VERSION;

    auto rdr = irs::segment_reader::open(dir, meta);
    ASSERT_FALSE(!rdr);
    ASSERT_EQ(1, rdr.size());
    ASSERT_EQ(meta.docs_count, rdr.docs_count());
    ASSERT_EQ(meta.docs_count, rdr.live_docs_count());

    irs::bytes_ref actual_value;

    auto& segment = *rdr.begin();
    const auto* column = segment.column_reader("name");
    ASSERT_NE(nullptr, column);
    auto values = column->values();

    // read documents
    ASSERT_TRUE(values(1, actual_value));
    ASSERT_EQ("A", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc1
    ASSERT_TRUE(values(2, actual_value));
    ASSERT_EQ("B", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc2
    ASSERT_TRUE(values(3, actual_value));
    ASSERT_EQ("C", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc3
    ASSERT_TRUE(values(4, actual_value));
    ASSERT_EQ("D", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc4
    ASSERT_TRUE(values(5, actual_value));
    ASSERT_EQ("E", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc5

    ASSERT_FALSE(values(6, actual_value)); // read invalid document

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
        const irs::string_ref name = "name";
        auto field = rdr.field(name);
        ASSERT_EQ(name, field->meta().name);

        // check terms
        auto terms = rdr.field(name);
        ASSERT_NE(nullptr, terms);

        ASSERT_EQ(5, terms->size());
        ASSERT_EQ(5, terms->docs_count());
        ASSERT_EQ(irs::ref_cast<irs::byte_type>(irs::string_ref("A")), (terms->min)());
        ASSERT_EQ(irs::ref_cast<irs::byte_type>(irs::string_ref("E")), (terms->max)());

        auto term = terms->iterator();

        // check term: A
        {
          ASSERT_TRUE(term->next());
          ASSERT_EQ(irs::ref_cast<irs::byte_type>(irs::string_ref("A")), term->value());

          // check docs
          {
            auto docs = term->postings(irs::flags::empty_instance());
            ASSERT_TRUE(docs->next());
            ASSERT_EQ(1, docs->value());
            ASSERT_FALSE(docs->next());
          }
        }

        // check term: B
        {
          ASSERT_TRUE(term->next());
          ASSERT_EQ(irs::ref_cast<irs::byte_type>(irs::string_ref("B")), term->value());

          // check docs
          {
            auto docs = term->postings(irs::flags::empty_instance());
            ASSERT_TRUE(docs->next());
            ASSERT_EQ(2, docs->value());
            ASSERT_FALSE(docs->next());
            ASSERT_FALSE(docs->next());
          }
        }

        // check term: C
        {
          ASSERT_TRUE(term->next());
          ASSERT_EQ(irs::ref_cast<irs::byte_type>(irs::string_ref("C")), term->value());

          // check docs
          {
            auto docs = term->postings(irs::flags::empty_instance());
            ASSERT_TRUE(docs->next());
            ASSERT_EQ(3, docs->value());
            ASSERT_FALSE(docs->next());
            ASSERT_FALSE(docs->next());
          }
        }

        // check term: D
        {
          ASSERT_TRUE(term->next());
          ASSERT_EQ(irs::ref_cast<irs::byte_type>(irs::string_ref("D")), term->value());

          // check docs
          {
            auto docs = term->postings(irs::flags::empty_instance());
            ASSERT_TRUE(docs->next());
            ASSERT_EQ(4, docs->value());
            ASSERT_FALSE(docs->next());
            ASSERT_FALSE(docs->next());
          }
        }

        // check term: E
        {
          ASSERT_TRUE(term->next());
          ASSERT_EQ(irs::ref_cast<irs::byte_type>(irs::string_ref("E")), term->value());

          // check docs
          {
            auto docs = term->postings(irs::flags::empty_instance());
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
        const irs::string_ref name = "seq";
        auto field = rdr.field(name);
        ASSERT_EQ(name, field->meta().name);

        // check terms
        auto terms = rdr.field(name);
        ASSERT_NE(nullptr, terms);
      }

      // check field
      {
        const irs::string_ref name = "same";
        auto field = rdr.field(name);
        ASSERT_EQ(name, field->meta().name);

        // check terms
        auto terms = rdr.field(name);
        ASSERT_NE(nullptr, terms);
        ASSERT_EQ(1, terms->size());
        ASSERT_EQ(5, terms->docs_count());
        ASSERT_EQ(irs::ref_cast<irs::byte_type>(irs::string_ref("xyz")), (terms->min)());
        ASSERT_EQ(irs::ref_cast<irs::byte_type>(irs::string_ref("xyz")), (terms->max)());

        auto term = terms->iterator();

        // check term: xyz
        {
          ASSERT_TRUE(term->next());
          ASSERT_EQ(irs::ref_cast<irs::byte_type>(irs::string_ref("xyz")), term->value());

          /* check docs */
          {
            auto docs = term->postings(irs::flags::empty_instance());
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
        const irs::string_ref name = "duplicated";
        auto field = rdr.field(name);
        ASSERT_EQ(name, field->meta().name);

        // check terms
        auto terms = rdr.field(name);
        ASSERT_NE(nullptr, terms);
        ASSERT_EQ(2, terms->size());
        ASSERT_EQ(4, terms->docs_count());
        ASSERT_EQ(irs::ref_cast<irs::byte_type>(irs::string_ref("abcd")), (terms->min)());
        ASSERT_EQ(irs::ref_cast<irs::byte_type>(irs::string_ref("vczc")), (terms->max)());

        auto term = terms->iterator();

        // check term: abcd
        {
          ASSERT_TRUE(term->next());
          ASSERT_EQ(irs::ref_cast<irs::byte_type>(irs::string_ref("abcd")), term->value());

          // check docs
          {
            auto docs = term->postings(irs::flags::empty_instance());
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
          ASSERT_EQ(irs::ref_cast<irs::byte_type>(irs::string_ref("vczc")), term->value());

          // check docs
          {
            auto docs = term->postings(irs::flags::empty_instance());
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
        const irs::string_ref name = "prefix";
        auto field = rdr.field(name);
        ASSERT_EQ(name, field->meta().name);

        // check terms
        auto terms = rdr.field(name);
        ASSERT_NE(nullptr, terms);
        ASSERT_EQ(2, terms->size());
        ASSERT_EQ(2, terms->docs_count());
        ASSERT_EQ(irs::ref_cast<irs::byte_type>(irs::string_ref("abcd")), (terms->min)());
        ASSERT_EQ(irs::ref_cast<irs::byte_type>(irs::string_ref("abcde")), (terms->max)());

        auto term = terms->iterator();

        // check term: abcd
        {
          ASSERT_TRUE(term->next());
          ASSERT_EQ(irs::ref_cast<irs::byte_type>(irs::string_ref("abcd")), term->value());

          // check docs
          {
            auto docs = term->postings(irs::flags::empty_instance());
            ASSERT_TRUE(docs->next());
            ASSERT_EQ(1, docs->value());
            ASSERT_FALSE(docs->next());
            ASSERT_FALSE(docs->next());
          }
        }

        // check term: abcde
        {
          ASSERT_TRUE(term->next());
          ASSERT_EQ(irs::ref_cast<irs::byte_type>(irs::string_ref("abcde")), term->value());

          // check docs
          {
            auto docs = term->postings(irs::flags::empty_instance());
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
        const irs::string_ref name = "invalid_field";
        ASSERT_EQ(nullptr, rdr.field(name));
      }
    }
  }
}
