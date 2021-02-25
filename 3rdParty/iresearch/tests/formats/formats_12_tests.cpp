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

#include "tests_shared.hpp"
#include "formats_test_case_base.hpp"
#include "store/directory_attributes.hpp"

namespace {

// -----------------------------------------------------------------------------
// --SECTION--                                          format 12 specific tests
// -----------------------------------------------------------------------------

class format_12_test_case : public tests::directory_test_case_base {
};

TEST_P(format_12_test_case, read_zero_block_encryption) {
  tests::json_doc_generator gen(
    resource("simple_sequential.json"),
    &tests::generic_json_field_factory
  );

  tests::document const* doc1 = gen.next();

  // replace encryption
  ASSERT_TRUE(dir().attributes().contains<tests::rot13_encryption>());

  // write segment with format10
  {
    auto codec = irs::formats::get("1_2", "1_0");
    ASSERT_NE(nullptr, codec);
    auto writer = irs::index_writer::make(dir(), codec, irs::OM_CREATE);
    ASSERT_NE(nullptr, writer);

    ASSERT_TRUE(insert(*writer,
      doc1->indexed.begin(), doc1->indexed.end(),
      doc1->stored.begin(), doc1->stored.end()
    ));

    writer->commit();
  }

  // replace encryption
  ASSERT_TRUE(dir().attributes().remove<tests::rot13_encryption>());
  dir().attributes().emplace<tests::rot13_encryption>(6);

  // can't open encrypted index without encryption
  ASSERT_THROW(irs::directory_reader::open(dir()), irs::index_error);
}

TEST_P(format_12_test_case, write_zero_block_encryption) {
  tests::json_doc_generator gen(
    resource("simple_sequential.json"),
    &tests::generic_json_field_factory
  );

  tests::document const* doc1 = gen.next();

  // replace encryption
  ASSERT_TRUE(dir().attributes().remove<tests::rot13_encryption>());
  dir().attributes().emplace<tests::rot13_encryption>(0);

  // write segment with format10
  auto codec = irs::formats::get("1_2", "1_0");
  ASSERT_NE(nullptr, codec);
  auto writer = irs::index_writer::make(dir(), codec, irs::OM_CREATE);
  ASSERT_NE(nullptr, writer);

  ASSERT_THROW(insert(*writer,
    doc1->indexed.begin(), doc1->indexed.end(),
    doc1->stored.begin(), doc1->stored.end()
  ), irs::index_error);
}

TEST_P(format_12_test_case, fields_read_write_wrong_encryption) {
  // create sorted && unsorted terms
  typedef std::set<irs::bytes_ref> sorted_terms_t;
  typedef std::vector<irs::bytes_ref> unsorted_terms_t;
  sorted_terms_t sorted_terms;
  unsorted_terms_t unsorted_terms;

  tests::json_doc_generator gen(
    resource("fst_prefixes.json"),
    [&sorted_terms, &unsorted_terms] (tests::document& doc, const std::string& name, const tests::json_doc_generator::json_value& data) {
      doc.insert(std::make_shared<tests::templates::string_field>(
        irs::string_ref(name),
        data.str
      ));

      auto ref = irs::ref_cast<irs::byte_type>((doc.indexed.end() - 1).as<tests::templates::string_field>().value());
      sorted_terms.emplace(ref);
      unsorted_terms.emplace_back(ref);
  });

  // define field
  irs::field_meta field;
  field.name = "field";
  field.norm = 5;

  auto codec = irs::formats::get("1_2", "1_0");
  ASSERT_NE(nullptr, codec);
  ASSERT_TRUE(dir().attributes().contains<tests::rot13_encryption>());

  // write fields
  {
    irs::flush_state state;
    state.dir = &dir();
    state.doc_count = 100;
    state.name = "segment_name";
    state.features = &field.features;

    // should use sorted terms on write
    tests::format_test_case::terms<sorted_terms_t::iterator> terms(
      sorted_terms.begin(), sorted_terms.end()
    );

    auto writer = codec->get_field_writer(false);
    ASSERT_NE(nullptr, writer);
    writer->prepare(state);
    writer->write(field.name, field.norm, field.features, terms);
    writer->end();
  }

  irs::segment_meta meta;
  meta.name = "segment_name";
  irs::document_mask docs_mask;

  auto reader = codec->get_field_reader();
  ASSERT_NE(nullptr, reader);

  // can't open encrypted index without encryption
  ASSERT_TRUE(dir().attributes().remove<tests::rot13_encryption>());
  ASSERT_THROW(reader->prepare(dir(), meta, docs_mask), irs::index_error);

  // can't open encrypted index with wrong encryption
  dir().attributes().emplace<tests::rot13_encryption>(6);
  ASSERT_THROW(reader->prepare(dir(), meta, docs_mask), irs::index_error);
}

TEST_P(format_12_test_case, column_meta_read_write_wrong_encryption) {
  auto codec = irs::formats::get("1_2", "1_0");
  ASSERT_NE(nullptr, codec);

  ASSERT_TRUE(dir().attributes().contains<tests::rot13_encryption>());

  irs::segment_meta meta;
  meta.name = "_1";

  // write meta
  {
    auto writer = codec->get_column_meta_writer();
    irs::segment_meta meta1;

    // write segment _1
    writer->prepare(dir(), meta);
    writer->write("_1_column1", 1);
    writer->write("_1_column2", 2);
    writer->write("_1_column0", 0);
    writer->flush();
  }

  size_t count = 0;
  irs::field_id max_id = 0;

  auto reader = codec->get_column_meta_reader();
  ASSERT_NE(nullptr, reader);

  // can't open encrypted index without encryption
  ASSERT_TRUE(dir().attributes().remove<tests::rot13_encryption>());
  ASSERT_THROW(reader->prepare(dir(), meta, count, max_id), irs::index_error);

  // can't open encrypted index with wrong encryption
  dir().attributes().emplace<tests::rot13_encryption>(6);
  ASSERT_THROW(reader->prepare(dir(), meta, count, max_id), irs::index_error);
}

TEST_P(format_12_test_case, open_ecnrypted_with_wrong_encryption) {
  tests::json_doc_generator gen(
    resource("simple_sequential.json"),
    &tests::generic_json_field_factory
  );

  tests::document const* doc1 = gen.next();

  ASSERT_TRUE(dir().attributes().contains<tests::rot13_encryption>());

  // write segment with format10
  {
    auto codec = irs::formats::get("1_2", "1_0");
    ASSERT_NE(nullptr, codec);
    auto writer = irs::index_writer::make(dir(), codec, irs::OM_CREATE);
    ASSERT_NE(nullptr, writer);

    ASSERT_TRUE(insert(*writer,
      doc1->indexed.begin(), doc1->indexed.end(),
      doc1->stored.begin(), doc1->stored.end()
    ));

    writer->commit();
  }

  // can't open encrypted index with wrong encryption
  ASSERT_TRUE(dir().attributes().remove<tests::rot13_encryption>());
  dir().attributes().emplace<tests::rot13_encryption>(6);
  ASSERT_THROW(irs::directory_reader::open(dir()), irs::index_error);
}

TEST_P(format_12_test_case, open_ecnrypted_with_non_encrypted) {
  tests::json_doc_generator gen(
    resource("simple_sequential.json"),
    &tests::generic_json_field_factory
  );

  tests::document const* doc1 = gen.next();

  ASSERT_TRUE(dir().attributes().contains<tests::rot13_encryption>());

  // write segment with format11
  {
    auto codec = irs::formats::get("1_2", "1_0");
    ASSERT_NE(nullptr, codec);
    auto writer = irs::index_writer::make(dir(), codec, irs::OM_CREATE);
    ASSERT_NE(nullptr, writer);

    ASSERT_TRUE(insert(*writer,
      doc1->indexed.begin(), doc1->indexed.end(),
      doc1->stored.begin(), doc1->stored.end()
    ));

    writer->commit();
  }

  // remove encryption
  dir().attributes().remove<tests::rot13_encryption>();

  // can't open encrypted index without encryption
  ASSERT_THROW(irs::directory_reader::open(dir()), irs::index_error);
}

TEST_P(format_12_test_case, open_non_ecnrypted_with_encrypted) {
  tests::json_doc_generator gen(
    resource("simple_sequential.json"),
    &tests::generic_json_field_factory
  );

  tests::document const* doc1 = gen.next();

  ASSERT_TRUE(dir().attributes().remove<tests::rot13_encryption>());

  // write segment with format11
  {
    auto codec = irs::formats::get("1_2", "1_0");
    ASSERT_NE(nullptr, codec);
    auto writer = irs::index_writer::make(dir(), codec, irs::OM_CREATE);
    ASSERT_NE(nullptr, writer);

    ASSERT_TRUE(insert(*writer,
      doc1->indexed.begin(), doc1->indexed.end(),
      doc1->stored.begin(), doc1->stored.end()
    ));

    writer->commit();
  }

  // add cipher
  dir().attributes().emplace<tests::rot13_encryption>(7);

  // check index
  auto index = irs::directory_reader::open(dir());
  ASSERT_TRUE(index);
  ASSERT_EQ(1, index->size());
  ASSERT_EQ(1, index->docs_count());
  ASSERT_EQ(1, index->live_docs_count());

  // check segment 0
  {
    auto& segment = index[0];
    ASSERT_EQ(1, segment.size());
    ASSERT_EQ(1, segment.docs_count());
    ASSERT_EQ(1, segment.live_docs_count());

    std::unordered_set<irs::string_ref> expectedName = { "A" };
    const auto* column = segment.column_reader("name");
    ASSERT_NE(nullptr, column);
    auto values = column->values();
    ASSERT_EQ(expectedName.size(), segment.docs_count()); // total count of documents
    auto terms = segment.field("same");
    ASSERT_NE(nullptr, terms);
    auto termItr = terms->iterator();
    ASSERT_TRUE(termItr->next());

    irs::bytes_ref actual_value;
    for (auto docsItr = termItr->postings(iresearch::flags()); docsItr->next();) {
      ASSERT_TRUE(values(docsItr->value(), actual_value));
      ASSERT_EQ(1, expectedName.erase(irs::to_string<irs::string_ref>(actual_value.c_str())));
    }

    ASSERT_TRUE(expectedName.empty());
  }
}

TEST_P(format_12_test_case, open_10_with_12) {
  tests::json_doc_generator gen(
    resource("simple_sequential.json"),
    &tests::generic_json_field_factory
  );

  tests::document const* doc1 = gen.next();

  // write segment with format10
  {
    auto codec = irs::formats::get("1_0");
    ASSERT_NE(nullptr, codec);
    auto writer = irs::index_writer::make(dir(), codec, irs::OM_CREATE);
    ASSERT_NE(nullptr, writer);

    ASSERT_TRUE(insert(*writer,
      doc1->indexed.begin(), doc1->indexed.end(),
      doc1->stored.begin(), doc1->stored.end()
    ));

    writer->commit();
  }

  // check index
  auto codec = irs::formats::get("1_2", "1_0");
  ASSERT_NE(nullptr, codec);
  auto index = irs::directory_reader::open(dir(), codec);
  ASSERT_TRUE(index);
  ASSERT_EQ(1, index->size());
  ASSERT_EQ(1, index->docs_count());
  ASSERT_EQ(1, index->live_docs_count());

  // check segment 0
  {
    auto& segment = index[0];
    ASSERT_EQ(1, segment.size());
    ASSERT_EQ(1, segment.docs_count());
    ASSERT_EQ(1, segment.live_docs_count());

    std::unordered_set<irs::string_ref> expectedName = { "A" };
    const auto* column = segment.column_reader("name");
    ASSERT_NE(nullptr, column);
    auto values = column->values();
    ASSERT_EQ(expectedName.size(), segment.docs_count()); // total count of documents
    auto terms = segment.field("same");
    ASSERT_NE(nullptr, terms);
    auto termItr = terms->iterator();
    ASSERT_TRUE(termItr->next());

    irs::bytes_ref actual_value;
    for (auto docsItr = termItr->postings(iresearch::flags()); docsItr->next();) {
      ASSERT_TRUE(values(docsItr->value(), actual_value));
      ASSERT_EQ(1, expectedName.erase(irs::to_string<irs::string_ref>(actual_value.c_str())));
    }

    ASSERT_TRUE(expectedName.empty());
  }
}

TEST_P(format_12_test_case, formats_10_12) {
  tests::json_doc_generator gen(
    resource("simple_sequential.json"),
    &tests::generic_json_field_factory
  );

  tests::document const* doc1 = gen.next();
  tests::document const* doc2 = gen.next();

  // write segment with format10
  {
    auto codec = irs::formats::get("1_0");
    ASSERT_NE(nullptr, codec);
    auto writer = irs::index_writer::make(dir(), codec, irs::OM_CREATE);
    ASSERT_NE(nullptr, writer);

    ASSERT_TRUE(insert(*writer,
      doc1->indexed.begin(), doc1->indexed.end(),
      doc1->stored.begin(), doc1->stored.end()
    ));

    writer->commit();
  }

  // write segment with format11
  {
    auto codec = irs::formats::get("1_2", "1_0");
    ASSERT_NE(nullptr, codec);
    auto writer = irs::index_writer::make(dir(), codec, irs::OM_APPEND);
    ASSERT_NE(nullptr, writer);

    ASSERT_TRUE(insert(*writer,
      doc2->indexed.begin(), doc2->indexed.end(),
      doc2->stored.begin(), doc2->stored.end()
    ));

    writer->commit();
  }

  // check index
  auto index = irs::directory_reader::open(dir());
  ASSERT_TRUE(index);
  ASSERT_EQ(2, index->size());
  ASSERT_EQ(2, index->docs_count());
  ASSERT_EQ(2, index->live_docs_count());

  // check segment 0
  {
    auto& segment = index[0];
    ASSERT_EQ(1, segment.size());
    ASSERT_EQ(1, segment.docs_count());
    ASSERT_EQ(1, segment.live_docs_count());

    std::unordered_set<irs::string_ref> expectedName = { "A" };
    const auto* column = segment.column_reader("name");
    ASSERT_NE(nullptr, column);
    auto values = column->values();
    ASSERT_EQ(expectedName.size(), segment.docs_count()); // total count of documents
    auto terms = segment.field("same");
    ASSERT_NE(nullptr, terms);
    auto termItr = terms->iterator();
    ASSERT_TRUE(termItr->next());

    irs::bytes_ref actual_value;
    for (auto docsItr = termItr->postings(iresearch::flags()); docsItr->next();) {
      ASSERT_TRUE(values(docsItr->value(), actual_value));
      ASSERT_EQ(1, expectedName.erase(irs::to_string<irs::string_ref>(actual_value.c_str())));
    }

    ASSERT_TRUE(expectedName.empty());
  }

  // check segment 1
  {
    auto& segment = index[1];
    ASSERT_EQ(1, segment.size());
    ASSERT_EQ(1, segment.docs_count());
    ASSERT_EQ(1, segment.live_docs_count());

    std::unordered_set<irs::string_ref> expectedName = { "B" };
    const auto* column = segment.column_reader("name");
    ASSERT_NE(nullptr, column);
    auto values = column->values();
    ASSERT_EQ(expectedName.size(), segment.docs_count()); // total count of documents
    auto terms = segment.field("same");
    ASSERT_NE(nullptr, terms);
    auto termItr = terms->iterator();
    ASSERT_TRUE(termItr->next());

    irs::bytes_ref actual_value;
    for (auto docsItr = termItr->postings(iresearch::flags()); docsItr->next();) {
      ASSERT_TRUE(values(docsItr->value(), actual_value));
      ASSERT_EQ(1, expectedName.erase(irs::to_string<irs::string_ref>(actual_value.c_str())));
    }

    ASSERT_TRUE(expectedName.empty());
  }
}

INSTANTIATE_TEST_CASE_P(
  format_12_test,
  format_12_test_case,
  ::testing::Values(
    &tests::rot13_cipher_directory<&tests::memory_directory, 16>,
    &tests::rot13_cipher_directory<&tests::fs_directory, 16>,
    &tests::rot13_cipher_directory<&tests::mmap_directory, 16>
  ),
  tests::directory_test_case_base::to_string
);

// -----------------------------------------------------------------------------
// --SECTION--                                                     generic tests
// -----------------------------------------------------------------------------

using tests::format_test_case;

INSTANTIATE_TEST_CASE_P(
  format_12_test,
  format_test_case,
  ::testing::Combine(
    ::testing::Values(
      &tests::rot13_cipher_directory<&tests::memory_directory, 16>,
      &tests::rot13_cipher_directory<&tests::fs_directory, 16>,
      &tests::rot13_cipher_directory<&tests::mmap_directory, 16>,
      &tests::rot13_cipher_directory<&tests::memory_directory, 7>,
      &tests::rot13_cipher_directory<&tests::fs_directory, 7>,
      &tests::rot13_cipher_directory<&tests::mmap_directory, 7>
    ),
    ::testing::Values(tests::format_info{"1_2", "1_0"})
  ),
  tests::to_string
);

}
