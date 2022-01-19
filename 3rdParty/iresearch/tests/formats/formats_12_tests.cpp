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
#include "index/norm.hpp"
#include "store/directory_attributes.hpp"

namespace {

using tests::format_test_case;
using tests::format_test_case_with_encryption;

class format_12_test_case : public format_test_case_with_encryption { };

TEST_P(format_12_test_case, open_10_with_12) {
  tests::json_doc_generator gen(
    resource("simple_sequential.json"),
    &tests::generic_json_field_factory);

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
    const auto* column = segment.column("name");
    ASSERT_NE(nullptr, column);
    auto values = column->iterator(false);
    ASSERT_NE(nullptr, values);
    auto* actual_value = irs::get<irs::payload>(*values);
    ASSERT_NE(nullptr, actual_value);
    ASSERT_EQ(expectedName.size(), segment.docs_count()); // total count of documents
    auto terms = segment.field("same");
    ASSERT_NE(nullptr, terms);
    auto termItr = terms->iterator(irs::SeekMode::NORMAL);
    ASSERT_TRUE(termItr->next());

    for (auto docsItr = termItr->postings(irs::IndexFeatures::NONE); docsItr->next();) {
      ASSERT_EQ(docsItr->value(), values->seek(docsItr->value()));
      ASSERT_EQ(1, expectedName.erase(irs::to_string<irs::string_ref>(actual_value->value.c_str())));
    }

    ASSERT_TRUE(expectedName.empty());
  }
}

TEST_P(format_12_test_case, formats_10_12) {
  tests::json_doc_generator gen(
    resource("simple_sequential.json"),
    &tests::generic_json_field_factory);

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
      doc1->stored.begin(), doc1->stored.end()));

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
    const auto* column = segment.column("name");
    ASSERT_NE(nullptr, column);
    auto values = column->iterator(false);
    ASSERT_NE(nullptr, values);
    auto* actual_value = irs::get<irs::payload>(*values);
    ASSERT_NE(nullptr, actual_value);
    ASSERT_EQ(expectedName.size(), segment.docs_count()); // total count of documents
    auto terms = segment.field("same");
    ASSERT_NE(nullptr, terms);
    auto termItr = terms->iterator(irs::SeekMode::NORMAL);
    ASSERT_TRUE(termItr->next());

    for (auto docsItr = termItr->postings(irs::IndexFeatures::NONE); docsItr->next();) {
      ASSERT_EQ(docsItr->value(), values->seek(docsItr->value()));
      ASSERT_EQ(1, expectedName.erase(irs::to_string<irs::string_ref>(actual_value->value.c_str())));
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
    const auto* column = segment.column("name");
    ASSERT_NE(nullptr, column);
    auto values = column->iterator(false);
    ASSERT_NE(nullptr, values);
    auto* actual_value = irs::get<irs::payload>(*values);
    ASSERT_NE(nullptr, actual_value);
    ASSERT_EQ(expectedName.size(), segment.docs_count()); // total count of documents
    auto terms = segment.field("same");
    ASSERT_NE(nullptr, terms);
    auto termItr = terms->iterator(irs::SeekMode::NORMAL);
    ASSERT_TRUE(termItr->next());

    for (auto docsItr = termItr->postings(irs::IndexFeatures::NONE); docsItr->next();) {
      ASSERT_EQ(docsItr->value(), values->seek(docsItr->value()));
      ASSERT_EQ(1, expectedName.erase(irs::to_string<irs::string_ref>(actual_value->value.c_str())));
    }

    ASSERT_TRUE(expectedName.empty());
  }
}

TEST_P(format_12_test_case, write_zero_block_encryption) {
  tests::json_doc_generator gen(
    resource("simple_sequential.json"),
    &tests::generic_json_field_factory);

  tests::document const* doc1 = gen.next();

  // replace encryption
  ASSERT_NE(nullptr, dir().attributes().encryption());
  dir().attributes() = irs::directory_attributes{
    0, std::make_unique<tests::rot13_encryption>(0) };

  auto writer = irs::index_writer::make(dir(), codec(), irs::OM_CREATE);
  ASSERT_NE(nullptr, writer);

  ASSERT_THROW(insert(*writer,
    doc1->indexed.begin(), doc1->indexed.end(),
    doc1->stored.begin(), doc1->stored.end()), irs::index_error);
}

const auto kTestValues =
  ::testing::Combine(
    ::testing::Values(
      &tests::rot13_directory<&tests::memory_directory, 16>,
      &tests::rot13_directory<&tests::fs_directory, 16>,
      &tests::rot13_directory<&tests::mmap_directory, 16>,
      &tests::rot13_directory<&tests::memory_directory, 7>,
      &tests::rot13_directory<&tests::fs_directory, 7>,
      &tests::rot13_directory<&tests::mmap_directory, 7>),
    ::testing::Values(tests::format_info{"1_2", "1_0"}));

// 1.2 specific tests
INSTANTIATE_TEST_SUITE_P(
    format_12_test,
    format_12_test_case,
    kTestValues,
    format_12_test_case::to_string);

// Generic tests
INSTANTIATE_TEST_SUITE_P(
    format_12_test,
    format_test_case_with_encryption,
    kTestValues,
    format_12_test_case::to_string);

INSTANTIATE_TEST_SUITE_P(
    format_12_test,
    format_test_case,
    kTestValues,
    format_12_test_case::to_string);

}
