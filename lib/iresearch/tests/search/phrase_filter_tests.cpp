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

#include "analysis/token_attributes.hpp"
#include "filter_test_case_base.hpp"
#include "search/multiterm_query.hpp"
#include "search/phrase_filter.hpp"
#include "search/phrase_query.hpp"
#include "search/term_query.hpp"
#include "tests_shared.hpp"

namespace tests {

void analyzed_json_field_factory(
  tests::document& doc, const std::string& name,
  const tests::json_doc_generator::json_value& data) {
  typedef text_field<std::string> text_field;

  class string_field : public tests::string_field {
   public:
    string_field(const std::string& name, const std::string_view& value)
      : tests::string_field(name, value, irs::IndexFeatures::FREQ) {}
  };

  if (data.is_string()) {
    // analyzed field
    doc.indexed.push_back(std::make_shared<text_field>(
      std::string(name.data()) + "_anl", data.str));

    // not analyzed field
    doc.insert(std::make_shared<string_field>(name, data.str));
  }
}

}  // namespace tests

class phrase_filter_test_case : public tests::FilterTestCaseBase {};

TEST_P(phrase_filter_test_case, sequential_one_term) {
  // add segment
  {
    tests::json_doc_generator gen(resource("phrase_sequential.json"),
                                  &tests::analyzed_json_field_factory);
    add_segment(gen);
  }

  // read segment
  auto rdr = open_reader();

  // empty field
  {
    irs::by_phrase q;

    auto prepared = q.prepare({.index = rdr});
    auto sub = rdr.begin();

    auto docs = prepared->execute({.segment = *sub});
    ASSERT_FALSE(irs::get<irs::frequency>(*docs));
    ASSERT_FALSE(irs::get<irs::filter_boost>(*docs));
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(irs::doc_limits::eof(docs->value()));
  }

  // empty phrase
  {
    irs::by_phrase q;
    *q.mutable_field() = "phrase_anl";

    auto prepared = q.prepare({.index = rdr});
    auto sub = rdr.begin();

    auto docs = prepared->execute({.segment = *sub});
    ASSERT_FALSE(irs::get<irs::frequency>(*docs));
    ASSERT_FALSE(irs::get<irs::filter_boost>(*docs));
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(irs::doc_limits::eof(docs->value()));
  }

  // equals to term_filter "fox"
  {
    irs::by_phrase q;
    *q.mutable_field() = "phrase_anl";
    q.mutable_options()->push_back<irs::by_term_options>().term =
      irs::ViewCast<irs::byte_type>(std::string_view("fox"));

    auto prepared = q.prepare({.index = rdr});
    auto sub = rdr.begin();
    auto column = sub->column("name");
    ASSERT_NE(nullptr, column);
    auto values = column->iterator(irs::ColumnHint::kNormal);
    ASSERT_NE(nullptr, values);
    auto* actual_value = irs::get<irs::payload>(*values);
    ASSERT_NE(nullptr, actual_value);

    auto docs = prepared->execute({.segment = *sub});
    ASSERT_FALSE(irs::get<irs::frequency>(*docs));
    ASSERT_FALSE(irs::get<irs::filter_boost>(*docs));
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::valid(docs->value()));
    auto docs_seek = prepared->execute({.segment = *sub});
    ASSERT_FALSE(irs::doc_limits::valid(docs_seek->value()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("A",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("A",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("G",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("G",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("I",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("I",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("K",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("K",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("L",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("L",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("N",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("N",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("S",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("S",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("T",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("T",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("V",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("V",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_FALSE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(irs::doc_limits::eof(docs->value()));
  }

  // prefix_filter "fo*"
  {
    irs::by_phrase q;
    *q.mutable_field() = "phrase_anl";
    q.mutable_options()->push_back<irs::by_prefix_options>().term =
      irs::ViewCast<irs::byte_type>(std::string_view("fo"));

    auto prepared = q.prepare({.index = rdr});
    auto sub = rdr.begin();
    auto column = sub->column("name");
    ASSERT_NE(nullptr, column);
    auto values = column->iterator(irs::ColumnHint::kNormal);
    ASSERT_NE(nullptr, values);
    auto* actual_value = irs::get<irs::payload>(*values);
    ASSERT_NE(nullptr, actual_value);

    auto docs = prepared->execute({.segment = *sub});
    ASSERT_FALSE(irs::get<irs::filter_boost>(*docs));
    ASSERT_FALSE(irs::get<irs::frequency>(*docs));
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::valid(docs->value()));
    auto docs_seek = prepared->execute({.segment = *sub});
    ASSERT_FALSE(irs::doc_limits::valid(docs_seek->value()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("A",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("A",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("D",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("D",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("G",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("G",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("H",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("H",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("I",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("I",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("K",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("K",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("L",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("L",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("N",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("N",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("S",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("S",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("T",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("T",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("U",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("U",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("V",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("V",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("W",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("W",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("X",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("X",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("Y",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("Y",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_FALSE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(irs::doc_limits::eof(docs->value()));
  }

  // wildcard_filter "fo%"
  {
    irs::by_phrase q;
    *q.mutable_field() = "phrase_anl";
    q.mutable_options()->push_back<irs::by_wildcard_options>().term =
      irs::ViewCast<irs::byte_type>(std::string_view("fo%"));

    auto prepared = q.prepare({.index = rdr});
    auto sub = rdr.begin();
    auto column = sub->column("name");
    ASSERT_NE(nullptr, column);
    auto values = column->iterator(irs::ColumnHint::kNormal);
    ASSERT_NE(nullptr, values);
    auto* actual_value = irs::get<irs::payload>(*values);
    ASSERT_NE(nullptr, actual_value);

    auto docs = prepared->execute({.segment = *sub});
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_FALSE(irs::get<irs::filter_boost>(*docs));
    ASSERT_FALSE(irs::get<irs::frequency>(*docs));
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::valid(docs->value()));
    auto docs_seek = prepared->execute({.segment = *sub});
    ASSERT_FALSE(irs::doc_limits::valid(docs_seek->value()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("A",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("A",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("D",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("D",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("G",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("G",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("H",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("H",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("I",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("I",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("K",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("K",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("L",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("L",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("N",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("N",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("S",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("S",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("T",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("T",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("U",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("U",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("V",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("V",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("W",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("W",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("X",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("X",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("Y",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("Y",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_FALSE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(irs::doc_limits::eof(docs->value()));
  }

  // wildcard_filter "%ox"
  {
    irs::by_phrase q;
    *q.mutable_field() = "phrase_anl";
    q.mutable_options()->push_back<irs::by_wildcard_options>().term =
      irs::ViewCast<irs::byte_type>(std::string_view("%ox"));

    auto prepared = q.prepare({.index = rdr});
    auto sub = rdr.begin();
    auto column = sub->column("name");
    ASSERT_NE(nullptr, column);
    auto values = column->iterator(irs::ColumnHint::kNormal);
    ASSERT_NE(nullptr, values);
    auto* actual_value = irs::get<irs::payload>(*values);
    ASSERT_NE(nullptr, actual_value);

    auto docs = prepared->execute({.segment = *sub});
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_FALSE(irs::get<irs::filter_boost>(*docs));
    ASSERT_FALSE(irs::get<irs::frequency>(*docs));
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::valid(docs->value()));
    auto docs_seek = prepared->execute({.segment = *sub});
    ASSERT_FALSE(irs::doc_limits::valid(docs_seek->value()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("A",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("A",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("G",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("G",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("I",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("I",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("K",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("K",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("L",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("L",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("N",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("N",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("S",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("S",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("T",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("T",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("V",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("V",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_FALSE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(irs::doc_limits::eof(docs->value()));
  }

  // wildcard_filter "f%x"
  {
    irs::by_phrase q;
    *q.mutable_field() = "phrase_anl";
    q.mutable_options()->push_back<irs::by_wildcard_options>().term =
      irs::ViewCast<irs::byte_type>(std::string_view("_ox"));

    auto prepared = q.prepare({.index = rdr});
    auto sub = rdr.begin();
    auto column = sub->column("name");
    ASSERT_NE(nullptr, column);
    auto values = column->iterator(irs::ColumnHint::kNormal);
    ASSERT_NE(nullptr, values);
    auto* actual_value = irs::get<irs::payload>(*values);
    ASSERT_NE(nullptr, actual_value);

    auto docs = prepared->execute({.segment = *sub});
    ASSERT_FALSE(irs::get<irs::filter_boost>(*docs));
    ASSERT_FALSE(irs::get<irs::frequency>(*docs));
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::valid(docs->value()));
    auto docs_seek = prepared->execute({.segment = *sub});
    ASSERT_FALSE(irs::doc_limits::valid(docs_seek->value()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("A",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("A",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("G",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("G",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("I",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("I",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("K",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("K",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("L",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("L",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("N",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("N",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("S",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("S",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("T",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("T",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("V",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("V",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_FALSE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(irs::doc_limits::eof(docs->value()));
  }

  // wildcard_filter "f_x"
  {
    irs::by_phrase q;
    *q.mutable_field() = "phrase_anl";
    q.mutable_options()->push_back<irs::by_wildcard_options>().term =
      irs::ViewCast<irs::byte_type>(std::string_view("f_x"));

    auto prepared = q.prepare({.index = rdr});
    auto sub = rdr.begin();
    auto column = sub->column("name");
    ASSERT_NE(nullptr, column);
    auto values = column->iterator(irs::ColumnHint::kNormal);
    ASSERT_NE(nullptr, values);
    auto* actual_value = irs::get<irs::payload>(*values);
    ASSERT_NE(nullptr, actual_value);

    auto docs = prepared->execute({.segment = *sub});
    ASSERT_FALSE(irs::get<irs::filter_boost>(*docs));
    ASSERT_FALSE(irs::get<irs::frequency>(*docs));
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::valid(docs->value()));
    auto docs_seek = prepared->execute({.segment = *sub});
    ASSERT_FALSE(irs::doc_limits::valid(docs_seek->value()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("A",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("A",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("G",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("G",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("I",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("I",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("K",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("K",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("L",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("L",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("N",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("N",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("S",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("S",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("T",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("T",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("V",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("V",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_FALSE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(irs::doc_limits::eof(docs->value()));
  }

  // wildcard_filter "fo_"
  {
    irs::by_phrase q;
    *q.mutable_field() = "phrase_anl";
    q.mutable_options()->push_back<irs::by_wildcard_options>().term =
      irs::ViewCast<irs::byte_type>(std::string_view("fo_"));

    auto prepared = q.prepare({.index = rdr});
    auto sub = rdr.begin();
    auto column = sub->column("name");
    ASSERT_NE(nullptr, column);
    auto values = column->iterator(irs::ColumnHint::kNormal);
    ASSERT_NE(nullptr, values);
    auto* actual_value = irs::get<irs::payload>(*values);
    ASSERT_NE(nullptr, actual_value);

    auto docs = prepared->execute({.segment = *sub});
    ASSERT_FALSE(irs::get<irs::filter_boost>(*docs));
    ASSERT_FALSE(irs::get<irs::frequency>(*docs));
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::valid(docs->value()));
    auto docs_seek = prepared->execute({.segment = *sub});
    ASSERT_FALSE(irs::doc_limits::valid(docs_seek->value()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("A",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("A",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("G",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("G",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("I",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("I",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("K",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("K",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("L",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("L",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("N",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("N",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("S",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("S",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("T",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("T",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("V",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("V",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_FALSE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(irs::doc_limits::eof(docs->value()));
  }

  // wildcard_filter "fox"
  {
    irs::by_phrase q;
    *q.mutable_field() = "phrase_anl";
    q.mutable_options()->push_back<irs::by_wildcard_options>().term =
      irs::ViewCast<irs::byte_type>(std::string_view("fox"));

    auto prepared = q.prepare({.index = rdr});
    auto sub = rdr.begin();
    auto column = sub->column("name");
    ASSERT_NE(nullptr, column);
    auto values = column->iterator(irs::ColumnHint::kNormal);
    ASSERT_NE(nullptr, values);
    auto* actual_value = irs::get<irs::payload>(*values);
    ASSERT_NE(nullptr, actual_value);

    auto docs = prepared->execute({.segment = *sub});
    ASSERT_FALSE(irs::get<irs::filter_boost>(*docs));
    ASSERT_FALSE(irs::get<irs::frequency>(*docs));
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::valid(docs->value()));
    auto docs_seek = prepared->execute({.segment = *sub});
    ASSERT_FALSE(irs::doc_limits::valid(docs_seek->value()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("A",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("A",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("G",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("G",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("I",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("I",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("K",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("K",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("L",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("L",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("N",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("N",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("S",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("S",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("T",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("T",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("V",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("V",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_FALSE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(irs::doc_limits::eof(docs->value()));
  }

  // levenshtein_filter "fox" max_distance = 0
  {
    irs::by_phrase q;
    *q.mutable_field() = "phrase_anl";
    auto& lt = q.mutable_options()->push_back<irs::by_edit_distance_options>();
    lt.max_distance = 0;
    lt.term = irs::ViewCast<irs::byte_type>(std::string_view("fox"));

    auto prepared = q.prepare({.index = rdr});
    auto sub = rdr.begin();
    auto column = sub->column("name");
    ASSERT_NE(nullptr, column);
    auto values = column->iterator(irs::ColumnHint::kNormal);
    ASSERT_NE(nullptr, values);
    auto* actual_value = irs::get<irs::payload>(*values);
    ASSERT_NE(nullptr, actual_value);

    auto docs = prepared->execute({.segment = *sub});
    ASSERT_FALSE(irs::get<irs::filter_boost>(*docs));
    ASSERT_FALSE(irs::get<irs::frequency>(*docs));
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::valid(docs->value()));
    auto docs_seek = prepared->execute({.segment = *sub});
    ASSERT_FALSE(irs::doc_limits::valid(docs_seek->value()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("A",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("A",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("G",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("G",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("I",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("I",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("K",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("K",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("L",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("L",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("N",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("N",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("S",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("S",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("T",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("T",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("V",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("V",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_FALSE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(irs::doc_limits::eof(docs->value()));
  }

  // levenshtein_filter "fol"
  {
    irs::by_phrase q;
    *q.mutable_field() = "phrase_anl";
    auto& lt = q.mutable_options()->push_back<irs::by_edit_distance_options>();
    lt.max_distance = 1;
    lt.term = irs::ViewCast<irs::byte_type>(std::string_view("fol"));

    auto prepared = q.prepare({.index = rdr});
    auto sub = rdr.begin();
    auto column = sub->column("name");
    ASSERT_NE(nullptr, column);
    auto values = column->iterator(irs::ColumnHint::kNormal);
    ASSERT_NE(nullptr, values);
    auto* actual_value = irs::get<irs::payload>(*values);
    ASSERT_NE(nullptr, actual_value);

    auto docs = prepared->execute({.segment = *sub});
    ASSERT_FALSE(irs::get<irs::filter_boost>(*docs));
    ASSERT_FALSE(irs::get<irs::frequency>(*docs));
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::valid(docs->value()));
    auto docs_seek = prepared->execute({.segment = *sub});
    ASSERT_FALSE(irs::doc_limits::valid(docs_seek->value()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("A",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("A",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("G",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("G",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("I",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("I",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("K",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("K",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("L",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("L",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("N",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("N",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("S",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("S",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("T",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("T",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("V",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("V",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_FALSE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(irs::doc_limits::eof(docs->value()));
  }

  // by_terms_options "fox"
  {
    irs::by_phrase q;
    *q.mutable_field() = "phrase_anl";
    auto& st = q.mutable_options()->push_back<irs::by_terms_options>();
    st.terms.emplace(irs::ViewCast<irs::byte_type>(std::string_view("fox")));

    auto prepared = q.prepare({.index = rdr});
    auto sub = rdr.begin();
    auto column = sub->column("name");
    ASSERT_NE(nullptr, column);
    auto values = column->iterator(irs::ColumnHint::kNormal);
    ASSERT_NE(nullptr, values);
    auto* actual_value = irs::get<irs::payload>(*values);
    ASSERT_NE(nullptr, actual_value);

    auto docs = prepared->execute({.segment = *sub});
    ASSERT_FALSE(irs::get<irs::filter_boost>(*docs));
    ASSERT_FALSE(irs::get<irs::frequency>(*docs));
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::valid(docs->value()));
    auto docs_seek = prepared->execute({.segment = *sub});
    ASSERT_FALSE(irs::doc_limits::valid(docs_seek->value()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("A",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("A",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("G",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("G",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("I",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("I",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("K",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("K",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("L",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("L",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("N",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("N",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("S",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("S",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("T",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("T",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("V",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("V",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_FALSE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(irs::doc_limits::eof(docs->value()));
  }

  // by_terms_options "fox|that"
  {
    irs::by_phrase q;
    *q.mutable_field() = "phrase_anl";
    auto& st = q.mutable_options()->push_back<irs::by_terms_options>();
    st.terms.emplace(irs::ViewCast<irs::byte_type>(std::string_view("fox")));
    st.terms.emplace(irs::ViewCast<irs::byte_type>(std::string_view("that")));

    auto prepared = q.prepare({.index = rdr});
    auto sub = rdr.begin();
    auto column = sub->column("name");
    ASSERT_NE(nullptr, column);
    auto values = column->iterator(irs::ColumnHint::kNormal);
    ASSERT_NE(nullptr, values);
    auto* actual_value = irs::get<irs::payload>(*values);
    ASSERT_NE(nullptr, actual_value);

    auto docs = prepared->execute({.segment = *sub});
    ASSERT_FALSE(irs::get<irs::filter_boost>(*docs));
    ASSERT_FALSE(irs::get<irs::frequency>(*docs));
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::valid(docs->value()));
    auto docs_seek = prepared->execute({.segment = *sub});
    ASSERT_FALSE(irs::doc_limits::valid(docs_seek->value()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("A",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("A",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("B",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("B",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("D",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("D",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("G",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("G",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("I",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("I",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("K",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("K",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("L",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("L",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("N",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("N",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("S",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("S",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("T",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("T",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("V",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("V",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_FALSE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(irs::doc_limits::eof(docs->value()));
  }

  // by_range_filter_options "[x0, x0]"
  {
    irs::by_phrase q;
    *q.mutable_field() = "phrase_anl";
    auto& rt = q.mutable_options()->push_back<irs::by_range_options>();
    rt.range.min = irs::ViewCast<irs::byte_type>(std::string_view("x0"));
    rt.range.max = irs::ViewCast<irs::byte_type>(std::string_view("x0"));
    rt.range.min_type = irs::BoundType::INCLUSIVE;
    rt.range.max_type = irs::BoundType::INCLUSIVE;

    auto prepared = q.prepare({.index = rdr});
    auto sub = rdr.begin();
    auto column = sub->column("name");
    ASSERT_NE(nullptr, column);
    auto values = column->iterator(irs::ColumnHint::kNormal);
    ASSERT_NE(nullptr, values);
    auto* actual_value = irs::get<irs::payload>(*values);
    ASSERT_NE(nullptr, actual_value);

    auto docs = prepared->execute({.segment = *sub});
    ASSERT_FALSE(irs::get<irs::filter_boost>(*docs));
    ASSERT_FALSE(irs::get<irs::frequency>(*docs));
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::valid(docs->value()));
    auto docs_seek = prepared->execute({.segment = *sub});
    ASSERT_FALSE(irs::doc_limits::valid(docs_seek->value()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("X0",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("X0",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("X4",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("X4",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_FALSE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(irs::doc_limits::eof(docs->value()));
  }

  // by_range_options "(x0, x0]"
  {
    irs::by_phrase q;
    *q.mutable_field() = "phrase_anl";
    auto& rt = q.mutable_options()->push_back<irs::by_range_options>();
    rt.range.min = irs::ViewCast<irs::byte_type>(std::string_view("x0"));
    rt.range.max = irs::ViewCast<irs::byte_type>(std::string_view("x0"));
    rt.range.min_type = irs::BoundType::EXCLUSIVE;
    rt.range.max_type = irs::BoundType::INCLUSIVE;

    auto prepared = q.prepare({.index = rdr});
    auto sub = rdr.begin();
    auto column = sub->column("name");
    ASSERT_NE(nullptr, column);
    auto values = column->iterator(irs::ColumnHint::kNormal);
    ASSERT_NE(nullptr, values);
    auto* actual_value = irs::get<irs::payload>(*values);
    ASSERT_NE(nullptr, actual_value);

    auto docs = prepared->execute({.segment = *sub});
    ASSERT_FALSE(irs::get<irs::filter_boost>(*docs));
    ASSERT_FALSE(irs::get<irs::frequency>(*docs));
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(irs::doc_limits::eof(docs->value()));
    auto docs_seek = prepared->execute({.segment = *sub});
    ASSERT_TRUE(irs::doc_limits::eof(docs_seek->value()));

    ASSERT_FALSE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(irs::doc_limits::eof(docs->value()));
  }

  // by_range_options "[x0, x0)"
  {
    irs::by_phrase q;
    *q.mutable_field() = "phrase_anl";
    auto& rt = q.mutable_options()->push_back<irs::by_range_options>();
    rt.range.min = irs::ViewCast<irs::byte_type>(std::string_view("x0"));
    rt.range.max = irs::ViewCast<irs::byte_type>(std::string_view("x0"));
    rt.range.min_type = irs::BoundType::INCLUSIVE;
    rt.range.max_type = irs::BoundType::EXCLUSIVE;

    auto prepared = q.prepare({.index = rdr});
    auto sub = rdr.begin();
    auto column = sub->column("name");
    ASSERT_NE(nullptr, column);
    auto values = column->iterator(irs::ColumnHint::kNormal);
    ASSERT_NE(nullptr, values);
    auto* actual_value = irs::get<irs::payload>(*values);
    ASSERT_NE(nullptr, actual_value);

    auto docs = prepared->execute({.segment = *sub});
    ASSERT_FALSE(irs::get<irs::filter_boost>(*docs));
    ASSERT_FALSE(irs::get<irs::frequency>(*docs));
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(irs::doc_limits::eof(docs->value()));
    auto docs_seek = prepared->execute({.segment = *sub});
    ASSERT_TRUE(irs::doc_limits::eof(docs_seek->value()));

    ASSERT_FALSE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(irs::doc_limits::eof(docs->value()));
  }

  // by_range_options "(x0, x0)"
  {
    irs::by_phrase q;
    *q.mutable_field() = "phrase_anl";
    auto& rt = q.mutable_options()->push_back<irs::by_range_options>();
    rt.range.min = irs::ViewCast<irs::byte_type>(std::string_view("x0"));
    rt.range.max = irs::ViewCast<irs::byte_type>(std::string_view("x0"));
    rt.range.min_type = irs::BoundType::EXCLUSIVE;
    rt.range.max_type = irs::BoundType::EXCLUSIVE;

    auto prepared = q.prepare({.index = rdr});
    auto sub = rdr.begin();
    auto column = sub->column("name");
    ASSERT_NE(nullptr, column);
    auto values = column->iterator(irs::ColumnHint::kNormal);
    ASSERT_NE(nullptr, values);
    auto* actual_value = irs::get<irs::payload>(*values);
    ASSERT_NE(nullptr, actual_value);

    auto docs = prepared->execute({.segment = *sub});
    ASSERT_FALSE(irs::get<irs::filter_boost>(*docs));
    ASSERT_FALSE(irs::get<irs::frequency>(*docs));
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(irs::doc_limits::eof(docs->value()));
    auto docs_seek = prepared->execute({.segment = *sub});
    ASSERT_TRUE(irs::doc_limits::eof(docs_seek->value()));

    ASSERT_FALSE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(irs::doc_limits::eof(docs->value()));
  }

  // by_range_filter_options "[x0, x2]"
  {
    irs::by_phrase q;
    *q.mutable_field() = "phrase_anl";
    auto& rt = q.mutable_options()->push_back<irs::by_range_options>();
    rt.range.min = irs::ViewCast<irs::byte_type>(std::string_view("x0"));
    rt.range.max = irs::ViewCast<irs::byte_type>(std::string_view("x2"));
    rt.range.min_type = irs::BoundType::INCLUSIVE;
    rt.range.max_type = irs::BoundType::INCLUSIVE;

    auto prepared = q.prepare({.index = rdr});
    auto sub = rdr.begin();
    auto column = sub->column("name");
    ASSERT_NE(nullptr, column);
    auto values = column->iterator(irs::ColumnHint::kNormal);
    ASSERT_NE(nullptr, values);
    auto* actual_value = irs::get<irs::payload>(*values);
    ASSERT_NE(nullptr, actual_value);

    auto docs = prepared->execute({.segment = *sub});
    ASSERT_FALSE(irs::get<irs::filter_boost>(*docs));
    ASSERT_FALSE(irs::get<irs::frequency>(*docs));
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::valid(docs->value()));
    auto docs_seek = prepared->execute({.segment = *sub});
    ASSERT_FALSE(irs::doc_limits::valid(docs_seek->value()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("X0",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("X0",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("X1",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("X1",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("X2",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("X2",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("X3",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("X3",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("X4",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("X4",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("X5",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("X5",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_FALSE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(irs::doc_limits::eof(docs->value()));
  }

  // by_range_options "(x0, x2]"
  {
    irs::by_phrase q;
    *q.mutable_field() = "phrase_anl";
    auto& rt = q.mutable_options()->push_back<irs::by_range_options>();
    rt.range.min = irs::ViewCast<irs::byte_type>(std::string_view("x0"));
    rt.range.max = irs::ViewCast<irs::byte_type>(std::string_view("x2"));
    rt.range.min_type = irs::BoundType::EXCLUSIVE;
    rt.range.max_type = irs::BoundType::INCLUSIVE;

    auto prepared = q.prepare({.index = rdr});
    auto sub = rdr.begin();
    auto column = sub->column("name");
    ASSERT_NE(nullptr, column);
    auto values = column->iterator(irs::ColumnHint::kNormal);
    ASSERT_NE(nullptr, values);
    auto* actual_value = irs::get<irs::payload>(*values);
    ASSERT_NE(nullptr, actual_value);

    auto docs = prepared->execute({.segment = *sub});
    ASSERT_FALSE(irs::get<irs::filter_boost>(*docs));
    ASSERT_FALSE(irs::get<irs::frequency>(*docs));
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::valid(docs->value()));
    auto docs_seek = prepared->execute({.segment = *sub});
    ASSERT_FALSE(irs::doc_limits::valid(docs_seek->value()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("X1",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("X1",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("X2",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("X2",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("X3",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("X3",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("X4",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("X4",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("X5",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("X5",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_FALSE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(irs::doc_limits::eof(docs->value()));
  }

  // by_range_options "[x0, x2)"
  {
    irs::by_phrase q;
    *q.mutable_field() = "phrase_anl";
    auto& rt = q.mutable_options()->push_back<irs::by_range_options>();
    rt.range.min = irs::ViewCast<irs::byte_type>(std::string_view("x0"));
    rt.range.max = irs::ViewCast<irs::byte_type>(std::string_view("x2"));
    rt.range.min_type = irs::BoundType::INCLUSIVE;
    rt.range.max_type = irs::BoundType::EXCLUSIVE;

    auto prepared = q.prepare({.index = rdr});
    auto sub = rdr.begin();
    auto column = sub->column("name");
    ASSERT_NE(nullptr, column);
    auto values = column->iterator(irs::ColumnHint::kNormal);
    ASSERT_NE(nullptr, values);
    auto* actual_value = irs::get<irs::payload>(*values);
    ASSERT_NE(nullptr, actual_value);

    auto docs = prepared->execute({.segment = *sub});
    ASSERT_FALSE(irs::get<irs::filter_boost>(*docs));
    ASSERT_FALSE(irs::get<irs::frequency>(*docs));
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::valid(docs->value()));
    auto docs_seek = prepared->execute({.segment = *sub});
    ASSERT_FALSE(irs::doc_limits::valid(docs_seek->value()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("X0",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("X0",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("X1",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("X1",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("X3",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("X3",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("X4",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("X4",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_FALSE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(irs::doc_limits::eof(docs->value()));
  }

  // by_range_options "(x0, x2)"
  {
    irs::by_phrase q;
    *q.mutable_field() = "phrase_anl";
    auto& rt = q.mutable_options()->push_back<irs::by_range_options>();
    rt.range.min = irs::ViewCast<irs::byte_type>(std::string_view("x0"));
    rt.range.max = irs::ViewCast<irs::byte_type>(std::string_view("x2"));
    rt.range.min_type = irs::BoundType::EXCLUSIVE;
    rt.range.max_type = irs::BoundType::EXCLUSIVE;

    auto prepared = q.prepare({.index = rdr});
    auto sub = rdr.begin();
    auto column = sub->column("name");
    ASSERT_NE(nullptr, column);
    auto values = column->iterator(irs::ColumnHint::kNormal);
    ASSERT_NE(nullptr, values);
    auto* actual_value = irs::get<irs::payload>(*values);
    ASSERT_NE(nullptr, actual_value);

    auto docs = prepared->execute({.segment = *sub});
    ASSERT_FALSE(irs::get<irs::filter_boost>(*docs));
    ASSERT_FALSE(irs::get<irs::frequency>(*docs));
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::valid(docs->value()));
    auto docs_seek = prepared->execute({.segment = *sub});
    ASSERT_FALSE(irs::doc_limits::valid(docs_seek->value()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("X1",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("X1",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("X3",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("X3",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("X4",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("X4",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_FALSE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(irs::doc_limits::eof(docs->value()));
  }

  // search "fox" on field without positions
  // which is ok for single word phrases
  {
    irs::by_phrase q;
    *q.mutable_field() = "phrase";
    q.mutable_options()->push_back<irs::by_term_options>().term =
      irs::ViewCast<irs::byte_type>(std::string_view("fox"));

    auto prepared = q.prepare({.index = rdr});
    // check single word phrase optimization
    ASSERT_NE(nullptr, dynamic_cast<const irs::TermQuery*>(prepared.get()));
    auto sub = rdr.begin();
    auto column = sub->column("name");
    ASSERT_NE(nullptr, column);
    auto values = column->iterator(irs::ColumnHint::kNormal);
    ASSERT_NE(nullptr, values);
    auto* actual_value = irs::get<irs::payload>(*values);
    ASSERT_NE(nullptr, actual_value);

    auto docs = prepared->execute({.segment = *sub});
    ASSERT_FALSE(irs::get<irs::filter_boost>(*docs));
    ASSERT_FALSE(irs::get<irs::frequency>(*docs));
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::valid(docs->value()));
    auto docs_seek = prepared->execute({.segment = *sub});
    ASSERT_FALSE(irs::doc_limits::valid(docs_seek->value()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("K",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("K",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_FALSE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(irs::doc_limits::eof(docs->value()));
  }

  // search "fo*" on field without positions
  // which is ok for the first word in phrase
  {
    irs::by_phrase q;
    *q.mutable_field() = "phrase";
    auto& pt = q.mutable_options()->push_back<irs::by_prefix_options>();
    pt.term = irs::ViewCast<irs::byte_type>(std::string_view("fo"));

    auto prepared = q.prepare({.index = rdr});
    // check single word phrase optimization
    ASSERT_NE(nullptr,
              dynamic_cast<const irs::MultiTermQuery*>(prepared.get()));
    auto sub = rdr.begin();
    auto column = sub->column("name");
    ASSERT_NE(nullptr, column);
    auto values = column->iterator(irs::ColumnHint::kNormal);
    ASSERT_NE(nullptr, values);
    auto* actual_value = irs::get<irs::payload>(*values);
    ASSERT_NE(nullptr, actual_value);

    auto docs = prepared->execute({.segment = *sub});
    ASSERT_FALSE(irs::get<irs::filter_boost>(*docs));
    ASSERT_FALSE(irs::get<irs::frequency>(*docs));
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::valid(docs->value()));
    auto docs_seek = prepared->execute({.segment = *sub});
    ASSERT_FALSE(irs::doc_limits::valid(docs_seek->value()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("K",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("K",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("L",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("L",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("N",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("N",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_FALSE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(irs::doc_limits::eof(docs->value()));
  }

  // search "fo%" on field without positions
  // which is ok for first word in phrase
  {
    irs::by_phrase q;
    *q.mutable_field() = "phrase";
    auto& wt = q.mutable_options()->push_back<irs::by_wildcard_options>();
    wt.term = irs::ViewCast<irs::byte_type>(std::string_view("fo%"));

    auto prepared = q.prepare({.index = rdr});
    // check single word phrase optimization
    ASSERT_NE(nullptr,
              dynamic_cast<const irs::MultiTermQuery*>(prepared.get()));
    auto sub = rdr.begin();
    auto column = sub->column("name");
    ASSERT_NE(nullptr, column);
    auto values = column->iterator(irs::ColumnHint::kNormal);
    ASSERT_NE(nullptr, values);
    auto* actual_value = irs::get<irs::payload>(*values);
    ASSERT_NE(nullptr, actual_value);

    auto docs = prepared->execute({.segment = *sub});
    ASSERT_FALSE(irs::get<irs::filter_boost>(*docs));
    ASSERT_FALSE(irs::get<irs::frequency>(*docs));
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::valid(docs->value()));
    auto docs_seek = prepared->execute({.segment = *sub});
    ASSERT_FALSE(irs::doc_limits::valid(docs_seek->value()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("K",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("K",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("L",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("L",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("N",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("N",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_FALSE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(irs::doc_limits::eof(docs->value()));
  }

  // search "f_x%" on field without positions
  // which is ok for first word in phrase
  {
    irs::by_phrase q;
    *q.mutable_field() = "phrase";
    auto& wt = q.mutable_options()->push_back<irs::by_wildcard_options>();
    wt.term = irs::ViewCast<irs::byte_type>(std::string_view("f_x%"));

    auto prepared = q.prepare({.index = rdr});
    // check single word phrase optimization
    ASSERT_NE(nullptr,
              dynamic_cast<const irs::MultiTermQuery*>(prepared.get()));
    auto sub = rdr.begin();
    auto column = sub->column("name");
    ASSERT_NE(nullptr, column);
    auto values = column->iterator(irs::ColumnHint::kNormal);
    ASSERT_NE(nullptr, values);
    auto* actual_value = irs::get<irs::payload>(*values);
    ASSERT_NE(nullptr, actual_value);

    auto docs = prepared->execute({.segment = *sub});
    ASSERT_FALSE(irs::get<irs::filter_boost>(*docs));
    ASSERT_FALSE(irs::get<irs::frequency>(*docs));
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::valid(docs->value()));
    auto docs_seek = prepared->execute({.segment = *sub});
    ASSERT_FALSE(irs::doc_limits::valid(docs_seek->value()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("K",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("K",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("L",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("L",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("N",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("N",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_FALSE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(irs::doc_limits::eof(docs->value()));
  }

  // search "fxo" on field without positions
  // which is ok for single word phrases
  {
    irs::by_phrase q;
    *q.mutable_field() = "phrase";
    auto& lt = q.mutable_options()->push_back<irs::by_edit_distance_options>();
    lt.max_distance = 1;
    lt.with_transpositions = true;
    lt.term = irs::ViewCast<irs::byte_type>(std::string_view("fxo"));

    auto prepared = q.prepare({.index = rdr});
    // check single word phrase optimization
    ASSERT_NE(nullptr,
              dynamic_cast<const irs::MultiTermQuery*>(prepared.get()));
    auto sub = rdr.begin();
    auto column = sub->column("name");
    ASSERT_NE(nullptr, column);
    auto values = column->iterator(irs::ColumnHint::kNormal);
    ASSERT_NE(nullptr, values);
    auto* actual_value = irs::get<irs::payload>(*values);
    ASSERT_NE(nullptr, actual_value);

    auto docs = prepared->execute({.segment = *sub});
    ASSERT_FALSE(irs::get<irs::filter_boost>(*docs));
    ASSERT_FALSE(irs::get<irs::frequency>(*docs));
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::valid(docs->value()));
    auto docs_seek = prepared->execute({.segment = *sub});
    ASSERT_FALSE(irs::doc_limits::valid(docs_seek->value()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("K",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("K",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_FALSE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(irs::doc_limits::eof(docs->value()));
  }

  // search by_range_options "[x0, x1]" on field without positions
  // which is ok for first word in phrase
  {
    irs::by_phrase q;
    *q.mutable_field() = "phrase";
    auto& rt = q.mutable_options()->push_back<irs::by_range_options>();
    rt.range.min = irs::ViewCast<irs::byte_type>(std::string_view("x0"));
    rt.range.max = irs::ViewCast<irs::byte_type>(std::string_view("x1"));
    rt.range.min_type = irs::BoundType::INCLUSIVE;
    rt.range.max_type = irs::BoundType::INCLUSIVE;

    auto prepared = q.prepare({.index = rdr});
    // check single word phrase optimization
    ASSERT_NE(nullptr,
              dynamic_cast<const irs::MultiTermQuery*>(prepared.get()));
    auto sub = rdr.begin();
    auto column = sub->column("name");
    ASSERT_NE(nullptr, column);
    auto values = column->iterator(irs::ColumnHint::kNormal);
    ASSERT_NE(nullptr, values);
    auto* actual_value = irs::get<irs::payload>(*values);
    ASSERT_NE(nullptr, actual_value);

    auto docs = prepared->execute({.segment = *sub});
    ASSERT_FALSE(irs::get<irs::filter_boost>(*docs));
    ASSERT_FALSE(irs::get<irs::frequency>(*docs));
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::valid(docs->value()));
    auto docs_seek = prepared->execute({.segment = *sub});
    ASSERT_FALSE(irs::doc_limits::valid(docs_seek->value()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("X0",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("X0",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("X1",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("X1",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_FALSE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(irs::doc_limits::eof(docs->value()));
  }

  // term_filter "fox" with phrase offset
  // which does not matter
  {
    irs::by_phrase q;
    *q.mutable_field() = "phrase_anl";
    q.mutable_options()
      ->push_back<irs::by_term_options>(std::numeric_limits<size_t>::max())
      .term = irs::ViewCast<irs::byte_type>(std::string_view("fox"));

    auto prepared = q.prepare({.index = rdr});
    // check single word phrase optimization
    ASSERT_NE(nullptr, dynamic_cast<const irs::TermQuery*>(prepared.get()));
    auto sub = rdr.begin();
    auto column = sub->column("name");
    ASSERT_NE(nullptr, column);
    auto values = column->iterator(irs::ColumnHint::kNormal);
    ASSERT_NE(nullptr, values);
    auto* actual_value = irs::get<irs::payload>(*values);
    ASSERT_NE(nullptr, actual_value);

    auto docs = prepared->execute({.segment = *sub});
    ASSERT_FALSE(irs::get<irs::filter_boost>(*docs));
    ASSERT_FALSE(irs::get<irs::frequency>(*docs));
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::valid(docs->value()));
    auto docs_seek = prepared->execute({.segment = *sub});
    ASSERT_FALSE(irs::doc_limits::valid(docs_seek->value()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("A",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("A",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("G",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("G",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("I",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("I",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("K",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("K",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("L",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("L",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("N",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("N",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("S",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("S",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("T",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("T",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("V",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("V",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_FALSE(docs->next());
    ASSERT_TRUE(irs::doc_limits::eof(docs->value()));
  }

  // prefix_filter "fo*" with phrase offset
  // which does not matter
  {
    irs::by_phrase q;
    *q.mutable_field() = "phrase_anl";
    auto& pt = q.mutable_options()->push_back<irs::by_prefix_options>(
      std::numeric_limits<size_t>::max());
    pt.term = irs::ViewCast<irs::byte_type>(std::string_view("fo"));

    auto prepared = q.prepare({.index = rdr});
    // check single word phrase optimization
    ASSERT_NE(nullptr,
              dynamic_cast<const irs::MultiTermQuery*>(prepared.get()));
    auto sub = rdr.begin();
    auto column = sub->column("name");
    ASSERT_NE(nullptr, column);
    auto values = column->iterator(irs::ColumnHint::kNormal);
    ASSERT_NE(nullptr, values);
    auto* actual_value = irs::get<irs::payload>(*values);
    ASSERT_NE(nullptr, actual_value);

    auto docs = prepared->execute({.segment = *sub});
    ASSERT_FALSE(irs::get<irs::filter_boost>(*docs));
    ASSERT_FALSE(irs::get<irs::frequency>(*docs));
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::valid(docs->value()));
    auto docs_seek = prepared->execute({.segment = *sub});
    ASSERT_FALSE(irs::doc_limits::valid(docs_seek->value()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("A",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("A",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("D",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("D",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("G",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("G",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("H",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("H",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("I",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("I",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("K",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("K",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("L",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("L",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("N",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("N",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("S",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("S",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("T",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("T",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("U",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("U",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("V",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("V",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("W",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("W",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("X",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("X",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("Y",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("Y",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_FALSE(docs->next());
    ASSERT_TRUE(irs::doc_limits::eof(docs->value()));
  }

  // wildcard_filter "fo%" with phrase offset
  // which does not matter
  {
    irs::by_phrase q;
    *q.mutable_field() = "phrase_anl";
    auto& wt = q.mutable_options()->push_back<irs::by_wildcard_options>(
      std::numeric_limits<size_t>::max());
    wt.term = irs::ViewCast<irs::byte_type>(std::string_view("fo%"));

    auto prepared = q.prepare({.index = rdr});
    // check single word phrase optimization
    ASSERT_NE(nullptr,
              dynamic_cast<const irs::MultiTermQuery*>(prepared.get()));
    auto sub = rdr.begin();
    auto column = sub->column("name");
    ASSERT_NE(nullptr, column);
    auto values = column->iterator(irs::ColumnHint::kNormal);
    ASSERT_NE(nullptr, values);
    auto* actual_value = irs::get<irs::payload>(*values);
    ASSERT_NE(nullptr, actual_value);

    auto docs = prepared->execute({.segment = *sub});
    ASSERT_FALSE(irs::get<irs::filter_boost>(*docs));
    ASSERT_FALSE(irs::get<irs::frequency>(*docs));
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::valid(docs->value()));
    auto docs_seek = prepared->execute({.segment = *sub});
    ASSERT_FALSE(irs::doc_limits::valid(docs_seek->value()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("A",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("A",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("D",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("D",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("G",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("G",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("H",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("H",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("I",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("I",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("K",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("K",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("L",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("L",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("N",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("N",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("S",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("S",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("T",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("T",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("U",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("U",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("V",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("V",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("W",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("W",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("X",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("X",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("Y",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("Y",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_FALSE(docs->next());
    ASSERT_TRUE(irs::doc_limits::eof(docs->value()));
  }

  // wildcard_filter "f%x" with phrase offset
  // which does not matter
  {
    irs::by_phrase q;
    *q.mutable_field() = "phrase_anl";
    auto& wt = q.mutable_options()->push_back<irs::by_wildcard_options>(
      std::numeric_limits<size_t>::max());
    wt.term = irs::ViewCast<irs::byte_type>(std::string_view("f%x"));

    auto prepared = q.prepare({.index = rdr});
    // check single word phrase optimization
    ASSERT_NE(nullptr,
              dynamic_cast<const irs::MultiTermQuery*>(prepared.get()));
    auto sub = rdr.begin();
    auto column = sub->column("name");
    ASSERT_NE(nullptr, column);
    auto values = column->iterator(irs::ColumnHint::kNormal);
    ASSERT_NE(nullptr, values);
    auto* actual_value = irs::get<irs::payload>(*values);
    ASSERT_NE(nullptr, actual_value);

    auto docs = prepared->execute({.segment = *sub});
    ASSERT_FALSE(irs::get<irs::filter_boost>(*docs));
    ASSERT_FALSE(irs::get<irs::frequency>(*docs));
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::valid(docs->value()));
    auto docs_seek = prepared->execute({.segment = *sub});
    ASSERT_FALSE(irs::doc_limits::valid(docs_seek->value()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("A",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("A",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("G",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("G",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("I",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("I",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("K",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("K",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("L",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("L",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("N",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("N",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("S",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("S",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("T",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("T",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("V",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("V",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_FALSE(docs->next());
    ASSERT_TRUE(irs::doc_limits::eof(docs->value()));
  }

  // wildcard_filter "f%x" with phrase offset
  // which does not matter
  {
    irs::by_phrase q;
    *q.mutable_field() = "phrase_anl";
    auto& lt = q.mutable_options()->push_back<irs::by_edit_distance_options>(
      std::numeric_limits<size_t>::max());
    lt.max_distance = 1;
    lt.term = irs::ViewCast<irs::byte_type>(std::string_view("fkx"));

    auto prepared = q.prepare({.index = rdr});
    // check single word phrase optimization
    ASSERT_NE(nullptr,
              dynamic_cast<const irs::MultiTermQuery*>(prepared.get()));
    auto sub = rdr.begin();
    auto column = sub->column("name");
    ASSERT_NE(nullptr, column);
    auto values = column->iterator(irs::ColumnHint::kNormal);
    ASSERT_NE(nullptr, values);
    auto* actual_value = irs::get<irs::payload>(*values);
    ASSERT_NE(nullptr, actual_value);

    auto docs = prepared->execute({.segment = *sub});
    ASSERT_FALSE(irs::get<irs::filter_boost>(*docs));
    ASSERT_FALSE(irs::get<irs::frequency>(*docs));
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::valid(docs->value()));
    auto docs_seek = prepared->execute({.segment = *sub});
    ASSERT_FALSE(irs::doc_limits::valid(docs_seek->value()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("A",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("A",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("G",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("G",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("I",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("I",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("K",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("K",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("L",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("L",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("N",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("N",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("S",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("S",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("T",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("T",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("V",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("V",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_FALSE(docs->next());
    ASSERT_TRUE(irs::doc_limits::eof(docs->value()));
  }

  // search by_range_options "[x0, x1]" with phrase offset
  // which does not matter
  {
    irs::by_phrase q;
    *q.mutable_field() = "phrase_anl";
    auto& rt = q.mutable_options()->push_back<irs::by_range_options>(
      std::numeric_limits<size_t>::max());
    rt.range.min = irs::ViewCast<irs::byte_type>(std::string_view("x0"));
    rt.range.max = irs::ViewCast<irs::byte_type>(std::string_view("x1"));
    rt.range.min_type = irs::BoundType::INCLUSIVE;
    rt.range.max_type = irs::BoundType::INCLUSIVE;

    auto prepared = q.prepare({.index = rdr});
    // check single word phrase optimization
    ASSERT_NE(nullptr,
              dynamic_cast<const irs::MultiTermQuery*>(prepared.get()));
    auto sub = rdr.begin();
    auto column = sub->column("name");
    ASSERT_NE(nullptr, column);
    auto values = column->iterator(irs::ColumnHint::kNormal);
    ASSERT_NE(nullptr, values);
    auto* actual_value = irs::get<irs::payload>(*values);
    ASSERT_NE(nullptr, actual_value);

    auto docs = prepared->execute({.segment = *sub});
    ASSERT_FALSE(irs::get<irs::filter_boost>(*docs));
    ASSERT_FALSE(irs::get<irs::frequency>(*docs));
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::valid(docs->value()));
    auto docs_seek = prepared->execute({.segment = *sub});
    ASSERT_FALSE(irs::doc_limits::valid(docs_seek->value()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("X0",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("X0",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("X1",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("X1",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("X3",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("X3",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("X4",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("X4",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_FALSE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(irs::doc_limits::eof(docs->value()));
  }
}

TEST_P(phrase_filter_test_case, sequential_three_terms) {
  // add segment
  {
    tests::json_doc_generator gen(resource("phrase_sequential.json"),
                                  &tests::analyzed_json_field_factory);
    add_segment(gen);
  }

  // read segment
  auto rdr = open_reader();

  // "quick brown fox"
  {
    irs::by_phrase q;
    *q.mutable_field() = "phrase_anl";
    q.mutable_options()->push_back<irs::by_term_options>().term =
      irs::ViewCast<irs::byte_type>(std::string_view("quick"));
    q.mutable_options()->push_back<irs::by_term_options>().term =
      irs::ViewCast<irs::byte_type>(std::string_view("brown"));
    q.mutable_options()->push_back<irs::by_term_options>().term =
      irs::ViewCast<irs::byte_type>(std::string_view("fox"));

    auto prepared = q.prepare({.index = rdr});
    auto sub = rdr.begin();
    auto column = sub->column("name");
    ASSERT_NE(nullptr, column);
    auto values = column->iterator(irs::ColumnHint::kNormal);
    ASSERT_NE(nullptr, values);
    auto* actual_value = irs::get<irs::payload>(*values);
    ASSERT_NE(nullptr, actual_value);

    auto docs = prepared->execute({.segment = *sub});
    ASSERT_FALSE(irs::get<irs::filter_boost>(*docs));
    ASSERT_FALSE(irs::get<irs::frequency>(*docs));
    auto* score = irs::get<irs::score>(*docs);
    ASSERT_NE(nullptr, score);
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::valid(docs->value()));
    auto docs_seek = prepared->execute({.segment = *sub});
    ASSERT_FALSE(irs::doc_limits::valid(docs_seek->value()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("A",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("A",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("G",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("G",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("I",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("I",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_FALSE(docs->next());
    ASSERT_TRUE(irs::doc_limits::eof(docs->value()));
    ASSERT_TRUE(irs::doc_limits::eof(docs_seek->seek(irs::doc_limits::eof())));
  }

  // "qui* brown fox"
  {
    irs::by_phrase q;
    *q.mutable_field() = "phrase_anl";
    auto& pt = q.mutable_options()->push_back<irs::by_prefix_options>();
    pt.term = irs::ViewCast<irs::byte_type>(std::string_view("qui"));
    q.mutable_options()->push_back<irs::by_term_options>().term =
      irs::ViewCast<irs::byte_type>(std::string_view("brown"));
    q.mutable_options()->push_back<irs::by_term_options>().term =
      irs::ViewCast<irs::byte_type>(std::string_view("fox"));

    auto prepared = q.prepare({.index = rdr});
    auto sub = rdr.begin();
    auto column = sub->column("name");
    ASSERT_NE(nullptr, column);
    auto values = column->iterator(irs::ColumnHint::kNormal);
    ASSERT_NE(nullptr, values);
    auto* actual_value = irs::get<irs::payload>(*values);
    ASSERT_NE(nullptr, actual_value);

    auto docs = prepared->execute({.segment = *sub});
    ASSERT_FALSE(irs::get<irs::filter_boost>(*docs));
    ASSERT_FALSE(irs::get<irs::frequency>(*docs));
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::valid(docs->value()));
    auto docs_seek = prepared->execute({.segment = *sub});
    ASSERT_FALSE(irs::doc_limits::valid(docs_seek->value()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("A",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("A",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("G",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("G",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("I",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("I",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("S",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("S",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_FALSE(docs->next());
    ASSERT_TRUE(irs::doc_limits::eof(docs->value()));
    ASSERT_TRUE(irs::doc_limits::eof(docs_seek->seek(irs::doc_limits::eof())));
  }

  // "qui% brown fox"
  {
    irs::by_phrase q;
    *q.mutable_field() = "phrase_anl";
    auto& wt = q.mutable_options()->push_back<irs::by_wildcard_options>();
    wt.term = irs::ViewCast<irs::byte_type>(std::string_view("qui%"));
    q.mutable_options()->push_back<irs::by_term_options>().term =
      irs::ViewCast<irs::byte_type>(std::string_view("brown"));
    q.mutable_options()->push_back<irs::by_term_options>().term =
      irs::ViewCast<irs::byte_type>(std::string_view("fox"));

    auto prepared = q.prepare({.index = rdr});
    auto sub = rdr.begin();
    auto column = sub->column("name");
    ASSERT_NE(nullptr, column);
    auto values = column->iterator(irs::ColumnHint::kNormal);
    ASSERT_NE(nullptr, values);
    auto* actual_value = irs::get<irs::payload>(*values);
    ASSERT_NE(nullptr, actual_value);

    auto docs = prepared->execute({.segment = *sub});
    ASSERT_FALSE(irs::get<irs::filter_boost>(*docs));
    ASSERT_FALSE(irs::get<irs::frequency>(*docs));
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::valid(docs->value()));
    auto docs_seek = prepared->execute({.segment = *sub});
    ASSERT_FALSE(irs::doc_limits::valid(docs_seek->value()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("A",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("A",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("G",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("G",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("I",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("I",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("S",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("S",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_FALSE(docs->next());
    ASSERT_TRUE(irs::doc_limits::eof(docs->value()));
    ASSERT_TRUE(irs::doc_limits::eof(docs_seek->seek(irs::doc_limits::eof())));
  }

  // "q%ck brown fox"
  {
    irs::by_phrase q;
    *q.mutable_field() = "phrase_anl";
    auto& wt = q.mutable_options()->push_back<irs::by_wildcard_options>();
    wt.term = irs::ViewCast<irs::byte_type>(std::string_view("q%ck"));
    q.mutable_options()->push_back<irs::by_term_options>().term =
      irs::ViewCast<irs::byte_type>(std::string_view("brown"));
    q.mutable_options()->push_back<irs::by_term_options>().term =
      irs::ViewCast<irs::byte_type>(std::string_view("fox"));

    auto prepared = q.prepare({.index = rdr});
    auto sub = rdr.begin();
    auto column = sub->column("name");
    ASSERT_NE(nullptr, column);
    auto values = column->iterator(irs::ColumnHint::kNormal);
    ASSERT_NE(nullptr, values);
    auto* actual_value = irs::get<irs::payload>(*values);
    ASSERT_NE(nullptr, actual_value);

    auto docs = prepared->execute({.segment = *sub});
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::valid(docs->value()));
    auto docs_seek = prepared->execute({.segment = *sub});
    ASSERT_FALSE(irs::doc_limits::valid(docs_seek->value()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("A",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("A",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("G",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("G",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("I",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("I",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_FALSE(docs->next());
    ASSERT_TRUE(irs::doc_limits::eof(docs->value()));
    ASSERT_TRUE(irs::doc_limits::eof(docs_seek->seek(irs::doc_limits::eof())));
  }

  // "quick brown fox" simple term max_distance = 0
  {
    irs::by_phrase q;
    *q.mutable_field() = "phrase_anl";
    auto& lt = q.mutable_options()->push_back<irs::by_edit_distance_options>();
    lt.max_distance = 0;
    lt.term = irs::ViewCast<irs::byte_type>(std::string_view("quick"));
    q.mutable_options()->push_back<irs::by_term_options>().term =
      irs::ViewCast<irs::byte_type>(std::string_view("brown"));
    q.mutable_options()->push_back<irs::by_term_options>().term =
      irs::ViewCast<irs::byte_type>(std::string_view("fox"));

    auto prepared = q.prepare({.index = rdr});
    auto sub = rdr.begin();
    auto column = sub->column("name");
    ASSERT_NE(nullptr, column);
    auto values = column->iterator(irs::ColumnHint::kNormal);
    ASSERT_NE(nullptr, values);
    auto* actual_value = irs::get<irs::payload>(*values);
    ASSERT_NE(nullptr, actual_value);

    auto docs = prepared->execute({.segment = *sub});
    ASSERT_FALSE(irs::get<irs::filter_boost>(*docs));
    ASSERT_FALSE(irs::get<irs::frequency>(*docs));
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::valid(docs->value()));
    auto docs_seek = prepared->execute({.segment = *sub});
    ASSERT_FALSE(irs::doc_limits::valid(docs_seek->value()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("A",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("A",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("G",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("G",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("I",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("I",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_FALSE(docs->next());
    ASSERT_TRUE(irs::doc_limits::eof(docs->value()));
    ASSERT_TRUE(irs::doc_limits::eof(docs_seek->seek(irs::doc_limits::eof())));
  }

  // "quck brown fox"
  {
    irs::by_phrase q;
    *q.mutable_field() = "phrase_anl";
    auto& lt = q.mutable_options()->push_back<irs::by_edit_distance_options>();
    lt.max_distance = 1;
    lt.term = irs::ViewCast<irs::byte_type>(std::string_view("quck"));
    q.mutable_options()->push_back<irs::by_term_options>().term =
      irs::ViewCast<irs::byte_type>(std::string_view("brown"));
    q.mutable_options()->push_back<irs::by_term_options>().term =
      irs::ViewCast<irs::byte_type>(std::string_view("fox"));

    auto prepared = q.prepare({.index = rdr});
    auto sub = rdr.begin();
    auto column = sub->column("name");
    ASSERT_NE(nullptr, column);
    auto values = column->iterator(irs::ColumnHint::kNormal);
    ASSERT_NE(nullptr, values);
    auto* actual_value = irs::get<irs::payload>(*values);
    ASSERT_NE(nullptr, actual_value);

    auto docs = prepared->execute({.segment = *sub});
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::valid(docs->value()));
    auto docs_seek = prepared->execute({.segment = *sub});
    ASSERT_FALSE(irs::doc_limits::valid(docs_seek->value()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("A",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("A",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("G",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("G",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("I",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("I",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_FALSE(docs->next());
    ASSERT_TRUE(irs::doc_limits::eof(docs->value()));
    ASSERT_TRUE(irs::doc_limits::eof(docs_seek->seek(irs::doc_limits::eof())));
  }

  // "[x0, x1] x0 x2
  {
    irs::by_phrase q;
    *q.mutable_field() = "phrase_anl";
    auto& rt = q.mutable_options()->push_back<irs::by_range_options>();
    rt.range.min = irs::ViewCast<irs::byte_type>(std::string_view("x0"));
    rt.range.max = irs::ViewCast<irs::byte_type>(std::string_view("x1"));
    rt.range.min_type = irs::BoundType::INCLUSIVE;
    rt.range.max_type = irs::BoundType::INCLUSIVE;
    q.mutable_options()->push_back<irs::by_term_options>().term =
      irs::ViewCast<irs::byte_type>(std::string_view("x0"));
    q.mutable_options()->push_back<irs::by_term_options>().term =
      irs::ViewCast<irs::byte_type>(std::string_view("x2"));

    auto prepared = q.prepare({.index = rdr});
    auto sub = rdr.begin();
    auto column = sub->column("name");
    ASSERT_NE(nullptr, column);
    auto values = column->iterator(irs::ColumnHint::kNormal);
    ASSERT_NE(nullptr, values);
    auto* actual_value = irs::get<irs::payload>(*values);
    ASSERT_NE(nullptr, actual_value);

    auto docs = prepared->execute({.segment = *sub});
    ASSERT_FALSE(irs::get<irs::filter_boost>(*docs));
    ASSERT_FALSE(irs::get<irs::frequency>(*docs));
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::valid(docs->value()));
    auto docs_seek = prepared->execute({.segment = *sub});
    ASSERT_FALSE(irs::doc_limits::valid(docs_seek->value()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("X4",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("X4",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_FALSE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(irs::doc_limits::eof(docs->value()));
  }

  // "quick bro* fox"
  {
    irs::by_phrase q;
    *q.mutable_field() = "phrase_anl";
    q.mutable_options()->push_back<irs::by_term_options>().term =
      irs::ViewCast<irs::byte_type>(std::string_view("quick"));
    auto& pt = q.mutable_options()->push_back<irs::by_prefix_options>();
    pt.term = irs::ViewCast<irs::byte_type>(std::string_view("bro"));
    q.mutable_options()->push_back<irs::by_term_options>().term =
      irs::ViewCast<irs::byte_type>(std::string_view("fox"));

    auto prepared = q.prepare({.index = rdr});
    auto sub = rdr.begin();
    auto column = sub->column("name");
    ASSERT_NE(nullptr, column);
    auto values = column->iterator(irs::ColumnHint::kNormal);
    ASSERT_NE(nullptr, values);
    auto* actual_value = irs::get<irs::payload>(*values);
    ASSERT_NE(nullptr, actual_value);

    auto docs = prepared->execute({.segment = *sub});
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::valid(docs->value()));
    auto docs_seek = prepared->execute({.segment = *sub});
    ASSERT_FALSE(irs::doc_limits::valid(docs_seek->value()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("A",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("A",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("G",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("G",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("I",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("I",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("T",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("T",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_FALSE(docs->next());
    ASSERT_TRUE(irs::doc_limits::eof(docs->value()));
    ASSERT_TRUE(irs::doc_limits::eof(docs_seek->seek(irs::doc_limits::eof())));
  }

  // "quick bro% fox"
  {
    irs::by_phrase q;
    *q.mutable_field() = "phrase_anl";
    q.mutable_options()->push_back<irs::by_term_options>().term =
      irs::ViewCast<irs::byte_type>(std::string_view("quick"));
    auto& wt = q.mutable_options()->push_back<irs::by_wildcard_options>();
    wt.term = irs::ViewCast<irs::byte_type>(std::string_view("bro%"));
    q.mutable_options()->push_back<irs::by_term_options>().term =
      irs::ViewCast<irs::byte_type>(std::string_view("fox"));

    auto prepared = q.prepare({.index = rdr});
    auto sub = rdr.begin();
    auto column = sub->column("name");
    ASSERT_NE(nullptr, column);
    auto values = column->iterator(irs::ColumnHint::kNormal);
    ASSERT_NE(nullptr, values);
    auto* actual_value = irs::get<irs::payload>(*values);
    ASSERT_NE(nullptr, actual_value);

    auto docs = prepared->execute({.segment = *sub});
    ASSERT_FALSE(irs::get<irs::filter_boost>(*docs));
    ASSERT_FALSE(irs::get<irs::frequency>(*docs));
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::valid(docs->value()));
    auto docs_seek = prepared->execute({.segment = *sub});
    ASSERT_FALSE(irs::doc_limits::valid(docs_seek->value()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("A",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("A",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("G",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("G",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("I",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("I",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("T",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("T",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_FALSE(docs->next());
    ASSERT_TRUE(irs::doc_limits::eof(docs->value()));
    ASSERT_TRUE(irs::doc_limits::eof(docs_seek->seek(irs::doc_limits::eof())));
  }

  // "quick b%w_ fox"
  {
    irs::by_phrase q;
    *q.mutable_field() = "phrase_anl";
    q.mutable_options()->push_back<irs::by_term_options>().term =
      irs::ViewCast<irs::byte_type>(std::string_view("quick"));
    auto& wt = q.mutable_options()->push_back<irs::by_wildcard_options>();
    wt.term = irs::ViewCast<irs::byte_type>(std::string_view("b%w_"));
    q.mutable_options()->push_back<irs::by_term_options>().term =
      irs::ViewCast<irs::byte_type>(std::string_view("fox"));

    auto prepared = q.prepare({.index = rdr});
    auto sub = rdr.begin();
    auto column = sub->column("name");
    ASSERT_NE(nullptr, column);
    auto values = column->iterator(irs::ColumnHint::kNormal);
    ASSERT_NE(nullptr, values);
    auto* actual_value = irs::get<irs::payload>(*values);
    ASSERT_NE(nullptr, actual_value);

    auto docs = prepared->execute({.segment = *sub});
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::valid(docs->value()));
    auto docs_seek = prepared->execute({.segment = *sub});
    ASSERT_FALSE(irs::doc_limits::valid(docs_seek->value()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("A",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("A",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("G",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("G",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("I",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("I",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_FALSE(docs->next());
    ASSERT_TRUE(irs::doc_limits::eof(docs->value()));
    ASSERT_TRUE(irs::doc_limits::eof(docs_seek->seek(irs::doc_limits::eof())));
  }

  // "quick brkln fox"
  {
    irs::by_phrase q;
    *q.mutable_field() = "phrase_anl";
    q.mutable_options()->push_back<irs::by_term_options>().term =
      irs::ViewCast<irs::byte_type>(std::string_view("quick"));
    auto& lt = q.mutable_options()->push_back<irs::by_edit_distance_options>();
    lt.max_distance = 2;
    lt.term = irs::ViewCast<irs::byte_type>(std::string_view("brkln"));
    q.mutable_options()->push_back<irs::by_term_options>().term =
      irs::ViewCast<irs::byte_type>(std::string_view("fox"));

    auto prepared = q.prepare({.index = rdr});
    auto sub = rdr.begin();
    auto column = sub->column("name");
    ASSERT_NE(nullptr, column);
    auto values = column->iterator(irs::ColumnHint::kNormal);
    ASSERT_NE(nullptr, values);
    auto* actual_value = irs::get<irs::payload>(*values);
    ASSERT_NE(nullptr, actual_value);

    auto docs = prepared->execute({.segment = *sub});
    ASSERT_FALSE(irs::get<irs::filter_boost>(*docs));
    ASSERT_FALSE(irs::get<irs::frequency>(*docs));
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::valid(docs->value()));
    auto docs_seek = prepared->execute({.segment = *sub});
    ASSERT_FALSE(irs::doc_limits::valid(docs_seek->value()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("A",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("A",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("G",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("G",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("I",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("I",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_FALSE(docs->next());
    ASSERT_TRUE(irs::doc_limits::eof(docs->value()));
    ASSERT_TRUE(irs::doc_limits::eof(docs_seek->seek(irs::doc_limits::eof())));
  }

  // "x1 [x0, x1] x2"
  {
    irs::by_phrase q;
    *q.mutable_field() = "phrase_anl";
    q.mutable_options()->push_back<irs::by_term_options>().term =
      irs::ViewCast<irs::byte_type>(std::string_view("x1"));
    auto& rt = q.mutable_options()->push_back<irs::by_range_options>();
    rt.range.min = irs::ViewCast<irs::byte_type>(std::string_view("x0"));
    rt.range.max = irs::ViewCast<irs::byte_type>(std::string_view("x1"));
    rt.range.min_type = irs::BoundType::INCLUSIVE;
    rt.range.max_type = irs::BoundType::INCLUSIVE;
    q.mutable_options()->push_back<irs::by_term_options>().term =
      irs::ViewCast<irs::byte_type>(std::string_view("x2"));

    auto prepared = q.prepare({.index = rdr});
    auto sub = rdr.begin();
    auto column = sub->column("name");
    ASSERT_NE(nullptr, column);
    auto values = column->iterator(irs::ColumnHint::kNormal);
    ASSERT_NE(nullptr, values);
    auto* actual_value = irs::get<irs::payload>(*values);
    ASSERT_NE(nullptr, actual_value);

    auto docs = prepared->execute({.segment = *sub});
    ASSERT_FALSE(irs::get<irs::filter_boost>(*docs));
    ASSERT_FALSE(irs::get<irs::frequency>(*docs));
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::valid(docs->value()));
    auto docs_seek = prepared->execute({.segment = *sub});
    ASSERT_FALSE(irs::doc_limits::valid(docs_seek->value()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("X4",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("X4",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_FALSE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(irs::doc_limits::eof(docs->value()));
  }

  // "quick brown fo*"
  {
    irs::by_phrase q;
    *q.mutable_field() = "phrase_anl";
    q.mutable_options()->push_back<irs::by_term_options>().term =
      irs::ViewCast<irs::byte_type>(std::string_view("quick"));
    q.mutable_options()->push_back<irs::by_term_options>().term =
      irs::ViewCast<irs::byte_type>(std::string_view("brown"));
    auto& pt = q.mutable_options()->push_back<irs::by_prefix_options>();
    pt.term = irs::ViewCast<irs::byte_type>(std::string_view("fo"));

    auto prepared = q.prepare({.index = rdr});
    auto sub = rdr.begin();
    auto column = sub->column("name");
    ASSERT_NE(nullptr, column);
    auto values = column->iterator(irs::ColumnHint::kNormal);
    ASSERT_NE(nullptr, values);
    auto* actual_value = irs::get<irs::payload>(*values);
    ASSERT_NE(nullptr, actual_value);

    auto docs = prepared->execute({.segment = *sub});
    ASSERT_FALSE(irs::get<irs::filter_boost>(*docs));
    ASSERT_FALSE(irs::get<irs::frequency>(*docs));
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::valid(docs->value()));
    auto docs_seek = prepared->execute({.segment = *sub});
    ASSERT_FALSE(irs::doc_limits::valid(docs_seek->value()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("A",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("A",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("G",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("G",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("I",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("I",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("U",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("U",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_FALSE(docs->next());
    ASSERT_TRUE(irs::doc_limits::eof(docs->value()));
    ASSERT_TRUE(irs::doc_limits::eof(docs_seek->seek(irs::doc_limits::eof())));
  }

  // "quick brown fo%"
  {
    irs::by_phrase q;
    *q.mutable_field() = "phrase_anl";
    q.mutable_options()->push_back<irs::by_term_options>().term =
      irs::ViewCast<irs::byte_type>(std::string_view("quick"));
    q.mutable_options()->push_back<irs::by_term_options>().term =
      irs::ViewCast<irs::byte_type>(std::string_view("brown"));
    auto& wt = q.mutable_options()->push_back<irs::by_wildcard_options>();
    wt.term = irs::ViewCast<irs::byte_type>(std::string_view("fo%"));

    auto prepared = q.prepare({.index = rdr});
    auto sub = rdr.begin();
    auto column = sub->column("name");
    ASSERT_NE(nullptr, column);
    auto values = column->iterator(irs::ColumnHint::kNormal);
    ASSERT_NE(nullptr, values);
    auto* actual_value = irs::get<irs::payload>(*values);
    ASSERT_NE(nullptr, actual_value);

    auto docs = prepared->execute({.segment = *sub});
    ASSERT_FALSE(irs::get<irs::filter_boost>(*docs));
    ASSERT_FALSE(irs::get<irs::frequency>(*docs));
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::valid(docs->value()));
    auto docs_seek = prepared->execute({.segment = *sub});
    ASSERT_FALSE(irs::doc_limits::valid(docs_seek->value()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("A",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("A",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("G",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("G",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("I",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("I",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("U",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("U",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_FALSE(docs->next());
    ASSERT_TRUE(irs::doc_limits::eof(docs->value()));
    ASSERT_TRUE(irs::doc_limits::eof(docs_seek->seek(irs::doc_limits::eof())));
  }

  // "quick brown f_x"
  {
    irs::by_phrase q;
    *q.mutable_field() = "phrase_anl";
    q.mutable_options()->push_back<irs::by_term_options>().term =
      irs::ViewCast<irs::byte_type>(std::string_view("quick"));
    q.mutable_options()->push_back<irs::by_term_options>().term =
      irs::ViewCast<irs::byte_type>(std::string_view("brown"));
    auto& wt = q.mutable_options()->push_back<irs::by_wildcard_options>();
    wt.term = irs::ViewCast<irs::byte_type>(std::string_view("f_x"));

    auto prepared = q.prepare({.index = rdr});
    auto sub = rdr.begin();
    auto column = sub->column("name");
    ASSERT_NE(nullptr, column);
    auto values = column->iterator(irs::ColumnHint::kNormal);
    ASSERT_NE(nullptr, values);
    auto* actual_value = irs::get<irs::payload>(*values);
    ASSERT_NE(nullptr, actual_value);

    auto docs = prepared->execute({.segment = *sub});
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::valid(docs->value()));
    auto docs_seek = prepared->execute({.segment = *sub});
    ASSERT_FALSE(irs::doc_limits::valid(docs_seek->value()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("A",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("A",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("G",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("G",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("I",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("I",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_FALSE(docs->next());
    ASSERT_TRUE(irs::doc_limits::eof(docs->value()));
    ASSERT_TRUE(irs::doc_limits::eof(docs_seek->seek(irs::doc_limits::eof())));
  }

  // "quick brown fxo"
  {
    irs::by_phrase q;
    *q.mutable_field() = "phrase_anl";
    q.mutable_options()->push_back<irs::by_term_options>().term =
      irs::ViewCast<irs::byte_type>(std::string_view("quick"));
    q.mutable_options()->push_back<irs::by_term_options>().term =
      irs::ViewCast<irs::byte_type>(std::string_view("brown"));
    auto& lt = q.mutable_options()->push_back<irs::by_edit_distance_options>();
    lt.max_distance = 1;
    lt.with_transpositions = true;
    lt.term = irs::ViewCast<irs::byte_type>(std::string_view("fxo"));

    auto prepared = q.prepare({.index = rdr});
    auto sub = rdr.begin();
    auto column = sub->column("name");
    ASSERT_NE(nullptr, column);
    auto values = column->iterator(irs::ColumnHint::kNormal);
    ASSERT_NE(nullptr, values);
    auto* actual_value = irs::get<irs::payload>(*values);
    ASSERT_NE(nullptr, actual_value);

    auto docs = prepared->execute({.segment = *sub});
    ASSERT_FALSE(irs::get<irs::filter_boost>(*docs));
    ASSERT_FALSE(irs::get<irs::frequency>(*docs));
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::valid(docs->value()));
    auto docs_seek = prepared->execute({.segment = *sub});
    ASSERT_FALSE(irs::doc_limits::valid(docs_seek->value()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("A",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("A",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("G",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("G",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("I",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("I",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_FALSE(docs->next());
    ASSERT_TRUE(irs::doc_limits::eof(docs->value()));
    ASSERT_TRUE(irs::doc_limits::eof(docs_seek->seek(irs::doc_limits::eof())));
  }

  // "x1 x0 [x1, x2]"
  {
    irs::by_phrase q;
    *q.mutable_field() = "phrase_anl";
    q.mutable_options()->push_back<irs::by_term_options>().term =
      irs::ViewCast<irs::byte_type>(std::string_view("x1"));
    q.mutable_options()->push_back<irs::by_term_options>().term =
      irs::ViewCast<irs::byte_type>(std::string_view("x0"));
    auto& rt = q.mutable_options()->push_back<irs::by_range_options>();
    rt.range.min = irs::ViewCast<irs::byte_type>(std::string_view("x1"));
    rt.range.max = irs::ViewCast<irs::byte_type>(std::string_view("x2"));
    rt.range.min_type = irs::BoundType::INCLUSIVE;
    rt.range.max_type = irs::BoundType::INCLUSIVE;

    auto prepared = q.prepare({.index = rdr});
    auto sub = rdr.begin();
    auto column = sub->column("name");
    ASSERT_NE(nullptr, column);
    auto values = column->iterator(irs::ColumnHint::kNormal);
    ASSERT_NE(nullptr, values);
    auto* actual_value = irs::get<irs::payload>(*values);
    ASSERT_NE(nullptr, actual_value);

    auto docs = prepared->execute({.segment = *sub});
    ASSERT_FALSE(irs::get<irs::filter_boost>(*docs));
    ASSERT_FALSE(irs::get<irs::frequency>(*docs));
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::valid(docs->value()));
    auto docs_seek = prepared->execute({.segment = *sub});
    ASSERT_FALSE(irs::doc_limits::valid(docs_seek->value()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("X4",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("X4",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_FALSE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(irs::doc_limits::eof(docs->value()));
  }

  // "qui* bro* fox"
  {
    irs::by_phrase q;
    *q.mutable_field() = "phrase_anl";
    auto& pt1 = q.mutable_options()->push_back<irs::by_prefix_options>();
    pt1.term = irs::ViewCast<irs::byte_type>(std::string_view("qui"));
    auto& pt2 = q.mutable_options()->push_back<irs::by_prefix_options>();
    pt2.term = irs::ViewCast<irs::byte_type>(std::string_view("bro"));
    q.mutable_options()->push_back<irs::by_term_options>().term =
      irs::ViewCast<irs::byte_type>(std::string_view("fox"));

    auto prepared = q.prepare({.index = rdr});
    auto sub = rdr.begin();
    auto column = sub->column("name");
    ASSERT_NE(nullptr, column);
    auto values = column->iterator(irs::ColumnHint::kNormal);
    ASSERT_NE(nullptr, values);
    auto* actual_value = irs::get<irs::payload>(*values);
    ASSERT_NE(nullptr, actual_value);

    auto docs = prepared->execute({.segment = *sub});
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::valid(docs->value()));
    auto docs_seek = prepared->execute({.segment = *sub});
    ASSERT_FALSE(irs::doc_limits::valid(docs_seek->value()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("A",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("A",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("G",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("G",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("I",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("I",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("S",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("S",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("T",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("T",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("V",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("V",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_FALSE(docs->next());
    ASSERT_TRUE(irs::doc_limits::eof(docs->value()));
    ASSERT_TRUE(irs::doc_limits::eof(docs_seek->seek(irs::doc_limits::eof())));
  }

  // "qui% bro% fox"
  {
    irs::by_phrase q;
    *q.mutable_field() = "phrase_anl";
    auto& wt1 = q.mutable_options()->push_back<irs::by_wildcard_options>();
    wt1.term = irs::ViewCast<irs::byte_type>(std::string_view("qui%"));
    auto& wt2 = q.mutable_options()->push_back<irs::by_wildcard_options>();
    wt2.term = irs::ViewCast<irs::byte_type>(std::string_view("bro%"));
    q.mutable_options()->push_back<irs::by_term_options>().term =
      irs::ViewCast<irs::byte_type>(std::string_view("fox"));

    auto prepared = q.prepare({.index = rdr});
    auto sub = rdr.begin();
    auto column = sub->column("name");
    ASSERT_NE(nullptr, column);
    auto values = column->iterator(irs::ColumnHint::kNormal);
    ASSERT_NE(nullptr, values);
    auto* actual_value = irs::get<irs::payload>(*values);
    ASSERT_NE(nullptr, actual_value);

    auto docs = prepared->execute({.segment = *sub});
    ASSERT_FALSE(irs::get<irs::filter_boost>(*docs));
    ASSERT_FALSE(irs::get<irs::frequency>(*docs));
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::valid(docs->value()));
    auto docs_seek = prepared->execute({.segment = *sub});
    ASSERT_FALSE(irs::doc_limits::valid(docs_seek->value()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("A",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("A",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("G",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("G",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("I",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("I",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("S",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("S",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("T",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("T",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("V",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("V",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_FALSE(docs->next());
    ASSERT_TRUE(irs::doc_limits::eof(docs->value()));
    ASSERT_TRUE(irs::doc_limits::eof(docs_seek->seek(irs::doc_limits::eof())));
  }

  // "qui% b%o__ fox"
  {
    irs::by_phrase q;
    *q.mutable_field() = "phrase_anl";
    auto& wt1 = q.mutable_options()->push_back<irs::by_wildcard_options>();
    wt1.term = irs::ViewCast<irs::byte_type>(std::string_view("qui%"));
    auto& wt2 = q.mutable_options()->push_back<irs::by_wildcard_options>();
    wt2.term = irs::ViewCast<irs::byte_type>(std::string_view("b%o__"));
    q.mutable_options()->push_back<irs::by_term_options>().term =
      irs::ViewCast<irs::byte_type>(std::string_view("fox"));

    auto prepared = q.prepare({.index = rdr});
    auto sub = rdr.begin();
    auto column = sub->column("name");
    ASSERT_NE(nullptr, column);
    auto values = column->iterator(irs::ColumnHint::kNormal);
    ASSERT_NE(nullptr, values);
    auto* actual_value = irs::get<irs::payload>(*values);
    ASSERT_NE(nullptr, actual_value);

    auto docs = prepared->execute({.segment = *sub});
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::valid(docs->value()));
    auto docs_seek = prepared->execute({.segment = *sub});
    ASSERT_FALSE(irs::doc_limits::valid(docs_seek->value()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("A",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("A",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("G",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("G",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("I",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("I",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("S",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("S",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_FALSE(docs->next());
    ASSERT_TRUE(irs::doc_limits::eof(docs->value()));
    ASSERT_TRUE(irs::doc_limits::eof(docs_seek->seek(irs::doc_limits::eof())));
  }

  // "qui bro fox"
  {
    irs::by_phrase q;
    *q.mutable_field() = "phrase_anl";
    auto& lt1 = q.mutable_options()->push_back<irs::by_edit_distance_options>();
    lt1.max_distance = 2;
    lt1.term = irs::ViewCast<irs::byte_type>(std::string_view("qui"));
    auto& lt2 = q.mutable_options()->push_back<irs::by_edit_distance_options>();
    lt2.max_distance = 1;
    lt2.term = irs::ViewCast<irs::byte_type>(std::string_view("brow"));
    q.mutable_options()->push_back<irs::by_term_options>().term =
      irs::ViewCast<irs::byte_type>(std::string_view("fox"));

    auto prepared = q.prepare({.index = rdr});
    auto sub = rdr.begin();
    auto column = sub->column("name");
    ASSERT_NE(nullptr, column);
    auto values = column->iterator(irs::ColumnHint::kNormal);
    ASSERT_NE(nullptr, values);
    auto* actual_value = irs::get<irs::payload>(*values);
    ASSERT_NE(nullptr, actual_value);

    auto docs = prepared->execute({.segment = *sub});
    ASSERT_FALSE(irs::get<irs::filter_boost>(*docs));
    ASSERT_FALSE(irs::get<irs::frequency>(*docs));
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::valid(docs->value()));
    auto docs_seek = prepared->execute({.segment = *sub});
    ASSERT_FALSE(irs::doc_limits::valid(docs_seek->value()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("A",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("A",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("G",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("G",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("I",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("I",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("S",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("S",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_FALSE(docs->next());
    ASSERT_TRUE(irs::doc_limits::eof(docs->value()));
    ASSERT_TRUE(irs::doc_limits::eof(docs_seek->seek(irs::doc_limits::eof())));
  }

  // "[x0, x1] [x0, x1] x2"
  {
    irs::by_phrase q;
    *q.mutable_field() = "phrase_anl";
    auto& rt1 = q.mutable_options()->push_back<irs::by_range_options>();
    rt1.range.min = irs::ViewCast<irs::byte_type>(std::string_view("x0"));
    rt1.range.max = irs::ViewCast<irs::byte_type>(std::string_view("x1"));
    rt1.range.min_type = irs::BoundType::INCLUSIVE;
    rt1.range.max_type = irs::BoundType::INCLUSIVE;
    auto& rt2 = q.mutable_options()->push_back<irs::by_range_options>();
    rt2.range.min = irs::ViewCast<irs::byte_type>(std::string_view("x0"));
    rt2.range.max = irs::ViewCast<irs::byte_type>(std::string_view("x1"));
    rt2.range.min_type = irs::BoundType::INCLUSIVE;
    rt2.range.max_type = irs::BoundType::INCLUSIVE;
    q.mutable_options()->push_back<irs::by_term_options>().term =
      irs::ViewCast<irs::byte_type>(std::string_view("x2"));

    auto prepared = q.prepare({.index = rdr});
    auto sub = rdr.begin();
    auto column = sub->column("name");
    ASSERT_NE(nullptr, column);
    auto values = column->iterator(irs::ColumnHint::kNormal);
    ASSERT_NE(nullptr, values);
    auto* actual_value = irs::get<irs::payload>(*values);
    ASSERT_NE(nullptr, actual_value);

    auto docs = prepared->execute({.segment = *sub});
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::valid(docs->value()));
    auto docs_seek = prepared->execute({.segment = *sub});
    ASSERT_FALSE(irs::doc_limits::valid(docs_seek->value()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("X4",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("X4",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_FALSE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(irs::doc_limits::eof(docs->value()));
  }

  // "qui* brown fo*"
  {
    irs::by_phrase q;
    *q.mutable_field() = "phrase_anl";
    auto& pt1 = q.mutable_options()->push_back<irs::by_prefix_options>();
    pt1.term = irs::ViewCast<irs::byte_type>(std::string_view("qui"));
    q.mutable_options()->push_back<irs::by_term_options>().term =
      irs::ViewCast<irs::byte_type>(std::string_view("brown"));
    auto& pt2 = q.mutable_options()->push_back<irs::by_prefix_options>();
    pt2.term = irs::ViewCast<irs::byte_type>(std::string_view("fo"));

    auto prepared = q.prepare({.index = rdr});
    auto sub = rdr.begin();
    auto column = sub->column("name");
    ASSERT_NE(nullptr, column);
    auto values = column->iterator(irs::ColumnHint::kNormal);
    ASSERT_NE(nullptr, values);
    auto* actual_value = irs::get<irs::payload>(*values);
    ASSERT_NE(nullptr, actual_value);

    auto docs = prepared->execute({.segment = *sub});
    ASSERT_FALSE(irs::get<irs::filter_boost>(*docs));
    ASSERT_FALSE(irs::get<irs::frequency>(*docs));
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::valid(docs->value()));
    auto docs_seek = prepared->execute({.segment = *sub});
    ASSERT_FALSE(irs::doc_limits::valid(docs_seek->value()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("A",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("A",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("G",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("G",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("I",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("I",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("S",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("S",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("U",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("U",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("W",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("W",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_FALSE(docs->next());
    ASSERT_TRUE(irs::doc_limits::eof(docs->value()));
    ASSERT_TRUE(irs::doc_limits::eof(docs_seek->seek(irs::doc_limits::eof())));
  }

  // "qui% brown fo%"
  {
    irs::by_phrase q;
    *q.mutable_field() = "phrase_anl";
    auto& wt1 = q.mutable_options()->push_back<irs::by_wildcard_options>();
    wt1.term = irs::ViewCast<irs::byte_type>(std::string_view("qui%"));
    q.mutable_options()->push_back<irs::by_term_options>().term =
      irs::ViewCast<irs::byte_type>(std::string_view("brown"));
    auto& wt2 = q.mutable_options()->push_back<irs::by_wildcard_options>();
    wt2.term = irs::ViewCast<irs::byte_type>(std::string_view("fo%"));

    auto prepared = q.prepare({.index = rdr});
    auto sub = rdr.begin();
    auto column = sub->column("name");
    ASSERT_NE(nullptr, column);
    auto values = column->iterator(irs::ColumnHint::kNormal);
    ASSERT_NE(nullptr, values);
    auto* actual_value = irs::get<irs::payload>(*values);
    ASSERT_NE(nullptr, actual_value);

    auto docs = prepared->execute({.segment = *sub});
    ASSERT_FALSE(irs::get<irs::filter_boost>(*docs));
    ASSERT_FALSE(irs::get<irs::frequency>(*docs));
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::valid(docs->value()));
    auto docs_seek = prepared->execute({.segment = *sub});
    ASSERT_FALSE(irs::doc_limits::valid(docs_seek->value()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("A",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("A",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("G",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("G",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("I",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("I",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("S",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("S",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("U",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("U",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("W",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("W",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_FALSE(docs->next());
    ASSERT_TRUE(irs::doc_limits::eof(docs->value()));
    ASSERT_TRUE(irs::doc_limits::eof(docs_seek->seek(irs::doc_limits::eof())));
  }

  // "q_i% brown f%x"
  {
    irs::by_phrase q;
    *q.mutable_field() = "phrase_anl";
    auto& wt1 = q.mutable_options()->push_back<irs::by_wildcard_options>();
    wt1.term = irs::ViewCast<irs::byte_type>(std::string_view("q_i%"));
    q.mutable_options()->push_back<irs::by_term_options>().term =
      irs::ViewCast<irs::byte_type>(std::string_view("brown"));
    auto& wt2 = q.mutable_options()->push_back<irs::by_wildcard_options>();
    wt2.term = irs::ViewCast<irs::byte_type>(std::string_view("f%x"));

    auto prepared = q.prepare({.index = rdr});
    auto sub = rdr.begin();
    auto column = sub->column("name");
    ASSERT_NE(nullptr, column);
    auto values = column->iterator(irs::ColumnHint::kNormal);
    ASSERT_NE(nullptr, values);
    auto* actual_value = irs::get<irs::payload>(*values);
    ASSERT_NE(nullptr, actual_value);

    auto docs = prepared->execute({.segment = *sub});
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::valid(docs->value()));
    auto docs_seek = prepared->execute({.segment = *sub});
    ASSERT_FALSE(irs::doc_limits::valid(docs_seek->value()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("A",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("A",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("G",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("G",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("I",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("I",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("S",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("S",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_FALSE(docs->next());
    ASSERT_TRUE(irs::doc_limits::eof(docs->value()));
    ASSERT_TRUE(irs::doc_limits::eof(docs_seek->seek(irs::doc_limits::eof())));
  }

  // "[x0, x1] x0 [x1, x2]"
  {
    irs::by_phrase q;
    *q.mutable_field() = "phrase_anl";
    auto& rt1 = q.mutable_options()->push_back<irs::by_range_options>();
    rt1.range.min = irs::ViewCast<irs::byte_type>(std::string_view("x0"));
    rt1.range.max = irs::ViewCast<irs::byte_type>(std::string_view("x1"));
    rt1.range.min_type = irs::BoundType::INCLUSIVE;
    rt1.range.max_type = irs::BoundType::INCLUSIVE;
    q.mutable_options()->push_back<irs::by_term_options>().term =
      irs::ViewCast<irs::byte_type>(std::string_view("x0"));
    auto& rt2 = q.mutable_options()->push_back<irs::by_range_options>();
    rt2.range.min = irs::ViewCast<irs::byte_type>(std::string_view("x1"));
    rt2.range.max = irs::ViewCast<irs::byte_type>(std::string_view("x2"));
    rt2.range.min_type = irs::BoundType::INCLUSIVE;
    rt2.range.max_type = irs::BoundType::INCLUSIVE;

    auto prepared = q.prepare({.index = rdr});
    auto sub = rdr.begin();
    auto column = sub->column("name");
    ASSERT_NE(nullptr, column);
    auto values = column->iterator(irs::ColumnHint::kNormal);
    ASSERT_NE(nullptr, values);
    auto* actual_value = irs::get<irs::payload>(*values);
    ASSERT_NE(nullptr, actual_value);

    auto docs = prepared->execute({.segment = *sub});
    ASSERT_FALSE(irs::get<irs::filter_boost>(*docs));
    ASSERT_FALSE(irs::get<irs::frequency>(*docs));
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::valid(docs->value()));
    auto docs_seek = prepared->execute({.segment = *sub});
    ASSERT_FALSE(irs::doc_limits::valid(docs_seek->value()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("X4",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("X4",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_FALSE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(irs::doc_limits::eof(docs->value()));
  }

  // "qoick br__nn fix"
  {
    irs::by_phrase q;
    *q.mutable_field() = "phrase_anl";
    auto& lt1 = q.mutable_options()->push_back<irs::by_edit_distance_options>();
    lt1.max_distance = 1;
    lt1.term = irs::ViewCast<irs::byte_type>(std::string_view("qoick"));
    auto& wt = q.mutable_options()->push_back<irs::by_wildcard_options>();
    wt.term = irs::ViewCast<irs::byte_type>(std::string_view("br__n"));
    auto& lt2 = q.mutable_options()->push_back<irs::by_edit_distance_options>();
    lt2.max_distance = 1;
    lt2.term = irs::ViewCast<irs::byte_type>(std::string_view("fix"));

    auto prepared = q.prepare({.index = rdr});
    auto sub = rdr.begin();
    auto column = sub->column("name");
    ASSERT_NE(nullptr, column);
    auto values = column->iterator(irs::ColumnHint::kNormal);
    ASSERT_NE(nullptr, values);
    auto* actual_value = irs::get<irs::payload>(*values);
    ASSERT_NE(nullptr, actual_value);

    auto docs = prepared->execute({.segment = *sub});
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::valid(docs->value()));
    auto docs_seek = prepared->execute({.segment = *sub});
    ASSERT_FALSE(irs::doc_limits::valid(docs_seek->value()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("A",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("A",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("G",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("G",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("I",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("I",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_FALSE(docs->next());
    ASSERT_TRUE(irs::doc_limits::eof(docs->value()));
    ASSERT_TRUE(irs::doc_limits::eof(docs_seek->seek(irs::doc_limits::eof())));
  }

  // "quick bro* fo*"
  {
    irs::by_phrase q;
    *q.mutable_field() = "phrase_anl";
    q.mutable_options()->push_back<irs::by_term_options>().term =
      irs::ViewCast<irs::byte_type>(std::string_view("quick"));
    auto& pt1 = q.mutable_options()->push_back<irs::by_prefix_options>();
    pt1.term = irs::ViewCast<irs::byte_type>(std::string_view("bro"));
    auto& pt2 = q.mutable_options()->push_back<irs::by_prefix_options>();
    pt2.term = irs::ViewCast<irs::byte_type>(std::string_view("fo"));

    auto prepared = q.prepare({.index = rdr});
    auto sub = rdr.begin();
    auto column = sub->column("name");
    ASSERT_NE(nullptr, column);
    auto values = column->iterator(irs::ColumnHint::kNormal);
    ASSERT_NE(nullptr, values);
    auto* actual_value = irs::get<irs::payload>(*values);
    ASSERT_NE(nullptr, actual_value);

    auto docs = prepared->execute({.segment = *sub});
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::valid(docs->value()));
    auto docs_seek = prepared->execute({.segment = *sub});
    ASSERT_FALSE(irs::doc_limits::valid(docs_seek->value()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("A",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("A",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("G",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("G",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("I",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("I",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("T",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("T",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("U",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("U",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("X",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("X",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_FALSE(docs->next());
    ASSERT_TRUE(irs::doc_limits::eof(docs->value()));
    ASSERT_TRUE(irs::doc_limits::eof(docs_seek->seek(irs::doc_limits::eof())));
  }

  // "quick bro% fo%"
  {
    irs::by_phrase q;
    *q.mutable_field() = "phrase_anl";
    q.mutable_options()->push_back<irs::by_term_options>().term =
      irs::ViewCast<irs::byte_type>(std::string_view("quick"));
    auto& wt1 = q.mutable_options()->push_back<irs::by_wildcard_options>();
    wt1.term = irs::ViewCast<irs::byte_type>(std::string_view("bro%"));
    auto& wt2 = q.mutable_options()->push_back<irs::by_wildcard_options>();
    wt2.term = irs::ViewCast<irs::byte_type>(std::string_view("fo%"));

    auto prepared = q.prepare({.index = rdr});
    auto sub = rdr.begin();
    auto column = sub->column("name");
    ASSERT_NE(nullptr, column);
    auto values = column->iterator(irs::ColumnHint::kNormal);
    ASSERT_NE(nullptr, values);
    auto* actual_value = irs::get<irs::payload>(*values);
    ASSERT_NE(nullptr, actual_value);

    auto docs = prepared->execute({.segment = *sub});
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::valid(docs->value()));
    auto docs_seek = prepared->execute({.segment = *sub});
    ASSERT_FALSE(irs::doc_limits::valid(docs_seek->value()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("A",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("A",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("G",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("G",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("I",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("I",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("T",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("T",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("U",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("U",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("X",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("X",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_FALSE(docs->next());
    ASSERT_TRUE(irs::doc_limits::eof(docs->value()));
    ASSERT_TRUE(irs::doc_limits::eof(docs_seek->seek(irs::doc_limits::eof())));
  }

  // "quick b_o% f_%"
  {
    irs::by_phrase q;
    *q.mutable_field() = "phrase_anl";
    q.mutable_options()->push_back<irs::by_term_options>().term =
      irs::ViewCast<irs::byte_type>(std::string_view("quick"));
    auto& wt1 = q.mutable_options()->push_back<irs::by_wildcard_options>();
    wt1.term = irs::ViewCast<irs::byte_type>(std::string_view("b_o%"));
    auto& wt2 = q.mutable_options()->push_back<irs::by_wildcard_options>();
    wt2.term = irs::ViewCast<irs::byte_type>(std::string_view("f_%"));

    auto prepared = q.prepare({.index = rdr});
    auto sub = rdr.begin();
    auto column = sub->column("name");
    ASSERT_NE(nullptr, column);
    auto values = column->iterator(irs::ColumnHint::kNormal);
    ASSERT_NE(nullptr, values);
    auto* actual_value = irs::get<irs::payload>(*values);
    ASSERT_NE(nullptr, actual_value);

    auto docs = prepared->execute({.segment = *sub});
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::valid(docs->value()));
    auto docs_seek = prepared->execute({.segment = *sub});
    ASSERT_FALSE(irs::doc_limits::valid(docs_seek->value()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("A",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("A",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("G",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("G",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("I",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("I",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("T",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("T",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("U",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("U",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("X",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("X",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_FALSE(docs->next());
    ASSERT_TRUE(irs::doc_limits::eof(docs->value()));
    ASSERT_TRUE(irs::doc_limits::eof(docs_seek->seek(irs::doc_limits::eof())));
  }

  // "x1 [x0, x1] [x1, x2]"
  {
    irs::by_phrase q;
    *q.mutable_field() = "phrase_anl";
    q.mutable_options()->push_back<irs::by_term_options>().term =
      irs::ViewCast<irs::byte_type>(std::string_view("x1"));
    auto& rt1 = q.mutable_options()->push_back<irs::by_range_options>();
    rt1.range.min = irs::ViewCast<irs::byte_type>(std::string_view("x0"));
    rt1.range.max = irs::ViewCast<irs::byte_type>(std::string_view("x1"));
    rt1.range.min_type = irs::BoundType::INCLUSIVE;
    rt1.range.max_type = irs::BoundType::INCLUSIVE;
    auto& rt2 = q.mutable_options()->push_back<irs::by_range_options>();
    rt2.range.min = irs::ViewCast<irs::byte_type>(std::string_view("x1"));
    rt2.range.max = irs::ViewCast<irs::byte_type>(std::string_view("x2"));
    rt2.range.min_type = irs::BoundType::INCLUSIVE;
    rt2.range.max_type = irs::BoundType::INCLUSIVE;

    auto prepared = q.prepare({.index = rdr});
    auto sub = rdr.begin();
    auto column = sub->column("name");
    ASSERT_NE(nullptr, column);
    auto values = column->iterator(irs::ColumnHint::kNormal);
    ASSERT_NE(nullptr, values);
    auto* actual_value = irs::get<irs::payload>(*values);
    ASSERT_NE(nullptr, actual_value);

    auto docs = prepared->execute({.segment = *sub});
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::valid(docs->value()));
    auto docs_seek = prepared->execute({.segment = *sub});
    ASSERT_FALSE(irs::doc_limits::valid(docs_seek->value()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("X4",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("X4",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_FALSE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(irs::doc_limits::eof(docs->value()));
  }

  // "qui* bro* fo*"
  {
    irs::by_phrase q;
    *q.mutable_field() = "phrase_anl";
    auto& pt1 = q.mutable_options()->push_back<irs::by_prefix_options>();
    auto& pt2 = q.mutable_options()->push_back<irs::by_prefix_options>();
    auto& pt3 = q.mutable_options()->push_back<irs::by_prefix_options>();
    pt1.term = irs::ViewCast<irs::byte_type>(std::string_view("qui"));
    pt2.term = irs::ViewCast<irs::byte_type>(std::string_view("bro"));
    pt3.term = irs::ViewCast<irs::byte_type>(std::string_view("fo"));

    auto prepared = q.prepare({.index = rdr});
    auto sub = rdr.begin();
    auto column = sub->column("name");
    ASSERT_NE(nullptr, column);
    auto values = column->iterator(irs::ColumnHint::kNormal);
    ASSERT_NE(nullptr, values);
    auto* actual_value = irs::get<irs::payload>(*values);
    ASSERT_NE(nullptr, actual_value);

    auto docs = prepared->execute({.segment = *sub});
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::valid(docs->value()));
    auto docs_seek = prepared->execute({.segment = *sub});
    ASSERT_FALSE(irs::doc_limits::valid(docs_seek->value()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("A",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("A",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("G",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("G",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("I",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("I",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("S",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("S",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("T",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("T",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("U",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("U",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("V",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("V",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("W",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("W",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("X",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("X",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("Y",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("Y",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_FALSE(docs->next());
    ASSERT_TRUE(irs::doc_limits::eof(docs->value()));
    ASSERT_TRUE(irs::doc_limits::eof(docs_seek->seek(irs::doc_limits::eof())));
  }

  // "qui% bro% fo%"
  {
    irs::by_phrase q;
    *q.mutable_field() = "phrase_anl";
    auto& wt1 = q.mutable_options()->push_back<irs::by_wildcard_options>();
    auto& wt2 = q.mutable_options()->push_back<irs::by_wildcard_options>();
    auto& wt3 = q.mutable_options()->push_back<irs::by_wildcard_options>();
    wt1.term = irs::ViewCast<irs::byte_type>(std::string_view("qui%"));
    wt2.term = irs::ViewCast<irs::byte_type>(std::string_view("bro%"));
    wt3.term = irs::ViewCast<irs::byte_type>(std::string_view("fo%"));

    size_t collect_field_count = 0;
    size_t collect_term_count = 0;
    size_t finish_count = 0;

    tests::sort::custom_sort sort;

    sort.collector_collect_field = [&collect_field_count](
                                     const irs::SubReader&,
                                     const irs::term_reader&) -> void {
      ++collect_field_count;
    };
    sort.collector_collect_term =
      [&collect_term_count](const irs::SubReader&, const irs::term_reader&,
                            const irs::attribute_provider&) -> void {
      ++collect_term_count;
    };
    sort.collectors_collect_ =
      [&finish_count](irs::byte_type*, const irs::FieldCollector*,
                      const irs::TermCollector*) -> void { ++finish_count; };
    sort.prepare_field_collector_ = [&sort]() -> irs::FieldCollector::ptr {
      return std::make_unique<tests::sort::custom_sort::field_collector>(sort);
    };
    sort.prepare_term_collector_ = [&sort]() -> irs::TermCollector::ptr {
      return std::make_unique<tests::sort::custom_sort::term_collector>(sort);
    };
    sort.scorer_score = [](irs::doc_id_t doc, irs::score_t* score) {
      ASSERT_NE(nullptr, score);
      *score = doc;
    };

    auto pord = irs::Scorers::Prepare(sort);
    auto prepared = q.prepare({.index = rdr, .scorers = pord});
    ASSERT_EQ(1, collect_field_count);  // 1 field in 1 segment
    ASSERT_EQ(6, collect_term_count);   // 6 different terms
    ASSERT_EQ(6, finish_count);         // 6 sub-terms in phrase

    auto sub = rdr.begin();
    auto column = sub->column("name");
    ASSERT_NE(nullptr, column);
    auto values = column->iterator(irs::ColumnHint::kNormal);
    ASSERT_NE(nullptr, values);
    auto* actual_value = irs::get<irs::payload>(*values);
    ASSERT_NE(nullptr, actual_value);

    // no order passed - no frequency
    {
      auto docs = prepared->execute({.segment = *sub});
      ASSERT_FALSE(irs::get<irs::frequency>(*docs));
      ASSERT_FALSE(irs::get<irs::filter_boost>(*docs));
    }

    auto docs = prepared->execute({.segment = *sub, .scorers = pord});
    auto* freq = irs::get<irs::frequency>(*docs);
    ASSERT_TRUE(freq);
    ASSERT_FALSE(irs::get<irs::filter_boost>(*docs));
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::valid(docs->value()));
    auto docs_seek = prepared->execute({.segment = *sub, .scorers = pord});
    ASSERT_FALSE(irs::doc_limits::valid(docs_seek->value()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(1, freq->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("A",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(freq->value, irs::get<irs::frequency>(*docs_seek)->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("A",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(1, freq->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("G",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(freq->value, irs::get<irs::frequency>(*docs_seek)->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("G",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(1, freq->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("I",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(freq->value, irs::get<irs::frequency>(*docs_seek)->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("I",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(1, freq->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("S",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(freq->value, irs::get<irs::frequency>(*docs_seek)->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("S",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(1, freq->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("T",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(freq->value, irs::get<irs::frequency>(*docs_seek)->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("T",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(1, freq->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("U",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(freq->value, irs::get<irs::frequency>(*docs_seek)->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("U",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(1, freq->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("V",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(freq->value, irs::get<irs::frequency>(*docs_seek)->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("V",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(1, freq->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("W",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(freq->value, irs::get<irs::frequency>(*docs_seek)->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("W",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(1, freq->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("X",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(freq->value, irs::get<irs::frequency>(*docs_seek)->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("X",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(1, freq->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("Y",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(freq->value, irs::get<irs::frequency>(*docs_seek)->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("Y",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_FALSE(docs->next());
    ASSERT_TRUE(irs::doc_limits::eof(docs->value()));
    ASSERT_TRUE(irs::doc_limits::eof(docs_seek->seek(irs::doc_limits::eof())));
  }

  // "q%ic_ br_wn _%x"
  {
    irs::by_phrase q;
    *q.mutable_field() = "phrase_anl";
    auto& wt1 = q.mutable_options()->push_back<irs::by_wildcard_options>();
    auto& wt2 = q.mutable_options()->push_back<irs::by_wildcard_options>();
    auto& wt3 = q.mutable_options()->push_back<irs::by_wildcard_options>();
    wt1.term = irs::ViewCast<irs::byte_type>(std::string_view("q%ic_"));
    wt2.term = irs::ViewCast<irs::byte_type>(std::string_view("br_wn"));
    wt3.term = irs::ViewCast<irs::byte_type>(std::string_view("_%x"));

    auto prepared = q.prepare({.index = rdr});

    auto sub = rdr.begin();
    auto column = sub->column("name");
    ASSERT_NE(nullptr, column);
    auto values = column->iterator(irs::ColumnHint::kNormal);
    ASSERT_NE(nullptr, values);
    auto* actual_value = irs::get<irs::payload>(*values);
    ASSERT_NE(nullptr, actual_value);

    auto docs = prepared->execute({.segment = *sub});
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::valid(docs->value()));
    auto docs_seek = prepared->execute({.segment = *sub});
    ASSERT_FALSE(irs::doc_limits::valid(docs_seek->value()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("A",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("A",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("G",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("G",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("I",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("I",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_FALSE(docs->next());
    ASSERT_TRUE(irs::doc_limits::eof(docs->value()));
    ASSERT_TRUE(irs::doc_limits::eof(docs_seek->seek(irs::doc_limits::eof())));
  }

  // "quick|quilt|hhh brown|brother fox"
  {
    irs::by_phrase q;
    *q.mutable_field() = "phrase_anl";
    auto& st1 = q.mutable_options()->push_back<irs::by_terms_options>();
    st1.terms.emplace(irs::ViewCast<irs::byte_type>(std::string_view("quick")));
    st1.terms.emplace(irs::ViewCast<irs::byte_type>(std::string_view("quilt")));
    st1.terms.emplace(irs::ViewCast<irs::byte_type>(std::string_view("hhh")));
    auto& st2 = q.mutable_options()->push_back<irs::by_terms_options>();
    st2.terms.emplace(irs::ViewCast<irs::byte_type>(std::string_view("brown")));
    st2.terms.emplace(
      irs::ViewCast<irs::byte_type>(std::string_view("brother")));
    auto& st3 = q.mutable_options()->push_back<irs::by_terms_options>();
    st3.terms.emplace(irs::ViewCast<irs::byte_type>(std::string_view("fox")));

    auto prepared = q.prepare({.index = rdr});
    auto sub = rdr.begin();
    auto column = sub->column("name");
    ASSERT_NE(nullptr, column);
    auto values = column->iterator(irs::ColumnHint::kNormal);
    ASSERT_NE(nullptr, values);
    auto* actual_value = irs::get<irs::payload>(*values);
    ASSERT_NE(nullptr, actual_value);

    auto docs = prepared->execute({.segment = *sub});
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::valid(docs->value()));
    auto docs_seek = prepared->execute({.segment = *sub});
    ASSERT_FALSE(irs::doc_limits::valid(docs_seek->value()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("A",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("A",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("G",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("G",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("I",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("I",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("S",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("S",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("T",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("T",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("V",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("V",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_FALSE(docs->next());
    ASSERT_TRUE(irs::doc_limits::eof(docs->value()));
    ASSERT_TRUE(irs::doc_limits::eof(docs_seek->seek(irs::doc_limits::eof())));
  }

  // "[x0, x1] [x0, x1] [x1, x2]"
  {
    irs::by_phrase q;
    *q.mutable_field() = "phrase_anl";
    auto& rt1 = q.mutable_options()->push_back<irs::by_range_options>();
    auto& rt2 = q.mutable_options()->push_back<irs::by_range_options>();
    auto& rt3 = q.mutable_options()->push_back<irs::by_range_options>();
    rt1.range.min = irs::ViewCast<irs::byte_type>(std::string_view("x0"));
    rt1.range.max = irs::ViewCast<irs::byte_type>(std::string_view("x1"));
    rt1.range.min_type = irs::BoundType::INCLUSIVE;
    rt1.range.max_type = irs::BoundType::INCLUSIVE;
    rt2.range.min = irs::ViewCast<irs::byte_type>(std::string_view("x0"));
    rt2.range.max = irs::ViewCast<irs::byte_type>(std::string_view("x1"));
    rt2.range.min_type = irs::BoundType::INCLUSIVE;
    rt2.range.max_type = irs::BoundType::INCLUSIVE;
    rt3.range.min = irs::ViewCast<irs::byte_type>(std::string_view("x1"));
    rt3.range.max = irs::ViewCast<irs::byte_type>(std::string_view("x2"));
    rt3.range.min_type = irs::BoundType::INCLUSIVE;
    rt3.range.max_type = irs::BoundType::INCLUSIVE;

    auto prepared = q.prepare({.index = rdr});
    auto sub = rdr.begin();
    auto column = sub->column("name");
    ASSERT_NE(nullptr, column);
    auto values = column->iterator(irs::ColumnHint::kNormal);
    ASSERT_NE(nullptr, values);
    auto* actual_value = irs::get<irs::payload>(*values);
    ASSERT_NE(nullptr, actual_value);

    auto docs = prepared->execute({.segment = *sub});
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::valid(docs->value()));
    auto docs_seek = prepared->execute({.segment = *sub});
    ASSERT_FALSE(irs::doc_limits::valid(docs_seek->value()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("X4",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("X4",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_FALSE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(irs::doc_limits::eof(docs->value()));
  }

  // "quick brown fox" with order
  {
    irs::by_phrase q;
    *q.mutable_field() = "phrase_anl";
    q.mutable_options()->push_back<irs::by_term_options>().term =
      irs::ViewCast<irs::byte_type>(std::string_view("quick"));
    q.mutable_options()->push_back<irs::by_term_options>().term =
      irs::ViewCast<irs::byte_type>(std::string_view("brown"));
    q.mutable_options()->push_back<irs::by_term_options>().term =
      irs::ViewCast<irs::byte_type>(std::string_view("fox"));

    size_t collect_field_count = 0;
    size_t collect_term_count = 0;
    size_t finish_count = 0;

    tests::sort::custom_sort sort;

    sort.collector_collect_field = [&collect_field_count](
                                     const irs::SubReader&,
                                     const irs::term_reader&) -> void {
      ++collect_field_count;
    };
    sort.collector_collect_term =
      [&collect_term_count](const irs::SubReader&, const irs::term_reader&,
                            const irs::attribute_provider&) -> void {
      ++collect_term_count;
    };
    sort.collectors_collect_ =
      [&finish_count](irs::byte_type*, const irs::FieldCollector*,
                      const irs::TermCollector*) -> void { ++finish_count; };
    sort.prepare_field_collector_ = [&sort]() -> irs::FieldCollector::ptr {
      return std::make_unique<tests::sort::custom_sort::field_collector>(sort);
    };
    sort.prepare_term_collector_ = [&sort]() -> irs::TermCollector::ptr {
      return std::make_unique<tests::sort::custom_sort::term_collector>(sort);
    };
    sort.scorer_score = [](irs::doc_id_t doc, irs::score_t* score) {
      ASSERT_NE(nullptr, score);
      *score = doc;
    };

    auto pord = irs::Scorers::Prepare(sort);
    auto prepared = q.prepare({.index = rdr, .scorers = pord});
    ASSERT_EQ(1, collect_field_count);  // 1 field in 1 segment
    ASSERT_EQ(3, collect_term_count);   // 3 different terms
    ASSERT_EQ(3, finish_count);         // 3 sub-terms in phrase
    auto sub = rdr.begin();

    // no order passed - no frequency
    {
      auto docs = prepared->execute({.segment = *sub});
      ASSERT_FALSE(irs::get<irs::frequency>(*docs));
      ASSERT_FALSE(irs::get<irs::filter_boost>(*docs));
    }

    auto column = sub->column("name");
    ASSERT_NE(nullptr, column);
    auto values = column->iterator(irs::ColumnHint::kNormal);
    ASSERT_NE(nullptr, values);
    auto* actual_value = irs::get<irs::payload>(*values);
    ASSERT_NE(nullptr, actual_value);
    auto docs = prepared->execute({.segment = *sub, .scorers = pord});
    auto* freq = irs::get<irs::frequency>(*docs);
    ASSERT_TRUE(freq);
    ASSERT_FALSE(irs::get<irs::filter_boost>(*docs));
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::valid(docs->value()));
    auto docs_seek = prepared->execute({.segment = *sub, .scorers = pord});
    ASSERT_FALSE(irs::doc_limits::valid(docs_seek->value()));
    auto* score = irs::get<irs::score>(*docs);
    ASSERT_FALSE(!score);

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(1, freq->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("A",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(freq->value, irs::get<irs::frequency>(*docs_seek)->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("A",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(1, freq->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("G",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(freq->value, irs::get<irs::frequency>(*docs_seek)->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("G",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(1, freq->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("I",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(freq->value, irs::get<irs::frequency>(*docs_seek)->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("I",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_FALSE(docs->next());
    ASSERT_TRUE(irs::doc_limits::eof(docs->value()));
    ASSERT_TRUE(irs::doc_limits::eof(docs_seek->seek(irs::doc_limits::eof())));
  }
}

TEST_P(phrase_filter_test_case, sequential_several_terms) {
  // add segment
  {
    tests::json_doc_generator gen(resource("phrase_sequential.json"),
                                  &tests::analyzed_json_field_factory);
    add_segment(gen);
  }

  // read segment
  auto rdr = open_reader();

  // "fox ... quick"
  {
    irs::by_phrase q;
    *q.mutable_field() = "phrase_anl";
    q.mutable_options()->push_back<irs::by_term_options>().term =
      irs::ViewCast<irs::byte_type>(std::string_view("fox"));
    q.mutable_options()->push_back<irs::by_term_options>(1).term =
      irs::ViewCast<irs::byte_type>(std::string_view("quick"));

    auto prepared = q.prepare({.index = rdr});

    auto sub = rdr.begin();
    auto column = sub->column("name");
    ASSERT_NE(nullptr, column);
    auto values = column->iterator(irs::ColumnHint::kNormal);
    ASSERT_NE(nullptr, values);
    auto* actual_value = irs::get<irs::payload>(*values);
    ASSERT_NE(nullptr, actual_value);
    auto docs = prepared->execute({.segment = *sub});
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::valid(docs->value()));
    auto docs_seek = prepared->execute({.segment = *sub});
    ASSERT_FALSE(irs::doc_limits::valid(docs_seek->value()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("L",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("L",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("N",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("N",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_FALSE(docs->next());
    ASSERT_TRUE(irs::doc_limits::eof(docs->value()));
    ASSERT_TRUE(irs::doc_limits::eof(docs_seek->seek(irs::doc_limits::eof())));
  }

  // "fo* ... quick"
  {
    irs::by_phrase q;
    *q.mutable_field() = "phrase_anl";
    auto& pt = q.mutable_options()->push_back<irs::by_prefix_options>();
    pt.term = irs::ViewCast<irs::byte_type>(std::string_view("fo"));
    q.mutable_options()->push_back<irs::by_term_options>(1).term =
      irs::ViewCast<irs::byte_type>(std::string_view("quick"));

    auto prepared = q.prepare({.index = rdr});

    auto sub = rdr.begin();
    auto column = sub->column("name");
    ASSERT_NE(nullptr, column);
    auto values = column->iterator(irs::ColumnHint::kNormal);
    ASSERT_NE(nullptr, values);
    auto* actual_value = irs::get<irs::payload>(*values);
    ASSERT_NE(nullptr, actual_value);
    auto docs = prepared->execute({.segment = *sub});
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::valid(docs->value()));
    auto docs_seek = prepared->execute({.segment = *sub});
    ASSERT_FALSE(irs::doc_limits::valid(docs_seek->value()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("L",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("L",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("N",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("N",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_FALSE(docs->next());
    ASSERT_TRUE(irs::doc_limits::eof(docs->value()));
    ASSERT_TRUE(irs::doc_limits::eof(docs_seek->seek(irs::doc_limits::eof())));
  }

  // "f_x ... quick"
  {
    irs::by_phrase q;
    *q.mutable_field() = "phrase_anl";
    auto& wt = q.mutable_options()->push_back<irs::by_wildcard_options>();
    wt.term = irs::ViewCast<irs::byte_type>(std::string_view("f_x"));
    q.mutable_options()->push_back<irs::by_term_options>(1).term =
      irs::ViewCast<irs::byte_type>(std::string_view("quick"));

    auto prepared = q.prepare({.index = rdr});

    auto sub = rdr.begin();
    auto column = sub->column("name");
    ASSERT_NE(nullptr, column);
    auto values = column->iterator(irs::ColumnHint::kNormal);
    ASSERT_NE(nullptr, values);
    auto* actual_value = irs::get<irs::payload>(*values);
    ASSERT_NE(nullptr, actual_value);
    auto docs = prepared->execute({.segment = *sub});
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::valid(docs->value()));
    auto docs_seek = prepared->execute({.segment = *sub});
    ASSERT_FALSE(irs::doc_limits::valid(docs_seek->value()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("L",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("L",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("N",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("N",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_FALSE(docs->next());
    ASSERT_TRUE(irs::doc_limits::eof(docs->value()));
    ASSERT_TRUE(irs::doc_limits::eof(docs_seek->seek(irs::doc_limits::eof())));
  }

  // "fpx ... quick"
  {
    irs::by_phrase q;
    *q.mutable_field() = "phrase_anl";
    auto& lt = q.mutable_options()->push_back<irs::by_edit_distance_options>();
    lt.max_distance = 1;
    lt.term = irs::ViewCast<irs::byte_type>(std::string_view("fpx"));
    q.mutable_options()->push_back<irs::by_term_options>(1).term =
      irs::ViewCast<irs::byte_type>(std::string_view("quick"));

    auto prepared = q.prepare({.index = rdr});

    auto sub = rdr.begin();
    auto column = sub->column("name");
    ASSERT_NE(nullptr, column);
    auto values = column->iterator(irs::ColumnHint::kNormal);
    ASSERT_NE(nullptr, values);
    auto* actual_value = irs::get<irs::payload>(*values);
    ASSERT_NE(nullptr, actual_value);
    auto docs = prepared->execute({.segment = *sub});
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::valid(docs->value()));
    auto docs_seek = prepared->execute({.segment = *sub});
    ASSERT_FALSE(irs::doc_limits::valid(docs_seek->value()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("L",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("L",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("N",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("N",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_FALSE(docs->next());
    ASSERT_TRUE(irs::doc_limits::eof(docs->value()));
    ASSERT_TRUE(irs::doc_limits::eof(docs_seek->seek(irs::doc_limits::eof())));
  }

  // "fox ... qui*"
  {
    irs::by_phrase q;
    *q.mutable_field() = "phrase_anl";
    q.mutable_options()->push_back<irs::by_term_options>().term =
      irs::ViewCast<irs::byte_type>(std::string_view("fox"));
    auto& pt = q.mutable_options()->push_back<irs::by_prefix_options>(1);
    pt.term = irs::ViewCast<irs::byte_type>(std::string_view("qui"));

    auto prepared = q.prepare({.index = rdr});

    auto sub = rdr.begin();
    auto column = sub->column("name");
    ASSERT_NE(nullptr, column);
    auto values = column->iterator(irs::ColumnHint::kNormal);
    ASSERT_NE(nullptr, values);
    auto* actual_value = irs::get<irs::payload>(*values);
    ASSERT_NE(nullptr, actual_value);
    auto docs = prepared->execute({.segment = *sub});
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::valid(docs->value()));
    auto docs_seek = prepared->execute({.segment = *sub});
    ASSERT_FALSE(irs::doc_limits::valid(docs_seek->value()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("L",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("L",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("N",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("N",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_FALSE(docs->next());
    ASSERT_TRUE(irs::doc_limits::eof(docs->value()));
    ASSERT_TRUE(irs::doc_limits::eof(docs_seek->seek(irs::doc_limits::eof())));
  }

  // "fox ... qui%ck"
  {
    irs::by_phrase q;
    *q.mutable_field() = "phrase_anl";
    q.mutable_options()->push_back<irs::by_term_options>().term =
      irs::ViewCast<irs::byte_type>(std::string_view("fox"));
    auto& wt = q.mutable_options()->push_back<irs::by_wildcard_options>(1);
    wt.term = irs::ViewCast<irs::byte_type>(std::string_view("qui%ck"));

    auto prepared = q.prepare({.index = rdr});

    auto sub = rdr.begin();
    auto column = sub->column("name");
    ASSERT_NE(nullptr, column);
    auto values = column->iterator(irs::ColumnHint::kNormal);
    ASSERT_NE(nullptr, values);
    auto* actual_value = irs::get<irs::payload>(*values);
    ASSERT_NE(nullptr, actual_value);
    auto docs = prepared->execute({.segment = *sub});
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::valid(docs->value()));
    auto docs_seek = prepared->execute({.segment = *sub});
    ASSERT_FALSE(irs::doc_limits::valid(docs_seek->value()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("L",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("L",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("N",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("N",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_FALSE(docs->next());
    ASSERT_TRUE(irs::doc_limits::eof(docs->value()));
    ASSERT_TRUE(irs::doc_limits::eof(docs_seek->seek(irs::doc_limits::eof())));
  }

  // "fo* ... qui*"
  {
    irs::by_phrase q;
    *q.mutable_field() = "phrase_anl";
    auto& pt1 = q.mutable_options()->push_back<irs::by_prefix_options>();
    auto& pt2 = q.mutable_options()->push_back<irs::by_prefix_options>(1);
    pt1.term = irs::ViewCast<irs::byte_type>(std::string_view("fo"));
    pt2.term = irs::ViewCast<irs::byte_type>(std::string_view("qui"));

    auto prepared = q.prepare({.index = rdr});

    auto sub = rdr.begin();
    auto column = sub->column("name");
    ASSERT_NE(nullptr, column);
    auto values = column->iterator(irs::ColumnHint::kNormal);
    ASSERT_NE(nullptr, values);
    auto* actual_value = irs::get<irs::payload>(*values);
    ASSERT_NE(nullptr, actual_value);
    auto docs = prepared->execute({.segment = *sub});
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::valid(docs->value()));
    auto docs_seek = prepared->execute({.segment = *sub});
    ASSERT_FALSE(irs::doc_limits::valid(docs_seek->value()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("L",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("L",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("N",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("N",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_FALSE(docs->next());
    ASSERT_TRUE(irs::doc_limits::eof(docs->value()));
    ASSERT_TRUE(irs::doc_limits::eof(docs_seek->seek(irs::doc_limits::eof())));
  }

  // "f%x ... qui%ck"
  {
    irs::by_phrase q;
    *q.mutable_field() = "phrase_anl";
    auto& wt1 = q.mutable_options()->push_back<irs::by_wildcard_options>();
    auto& wt2 = q.mutable_options()->push_back<irs::by_wildcard_options>(1);
    wt1.term = irs::ViewCast<irs::byte_type>(std::string_view("f%x"));
    wt2.term = irs::ViewCast<irs::byte_type>(std::string_view("qui%ck"));

    auto prepared = q.prepare({.index = rdr});

    auto sub = rdr.begin();
    auto column = sub->column("name");
    ASSERT_NE(nullptr, column);
    auto values = column->iterator(irs::ColumnHint::kNormal);
    ASSERT_NE(nullptr, values);
    auto* actual_value = irs::get<irs::payload>(*values);
    ASSERT_NE(nullptr, actual_value);
    auto docs = prepared->execute({.segment = *sub});
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::valid(docs->value()));
    auto docs_seek = prepared->execute({.segment = *sub});
    ASSERT_FALSE(irs::doc_limits::valid(docs_seek->value()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("L",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("L",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("N",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("N",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_FALSE(docs->next());
    ASSERT_TRUE(irs::doc_limits::eof(docs->value()));
    ASSERT_TRUE(irs::doc_limits::eof(docs_seek->seek(irs::doc_limits::eof())));
  }

  // "fx ... quik"
  {
    irs::by_phrase q;
    *q.mutable_field() = "phrase_anl";
    auto& lt1 = q.mutable_options()->push_back<irs::by_edit_distance_options>();
    auto& lt2 =
      q.mutable_options()->push_back<irs::by_edit_distance_options>(1);
    lt1.max_distance = 1;
    lt1.term = irs::ViewCast<irs::byte_type>(std::string_view("fx"));
    lt2.max_distance = 1;
    lt2.term = irs::ViewCast<irs::byte_type>(std::string_view("quik"));

    auto prepared = q.prepare({.index = rdr});

    auto sub = rdr.begin();
    auto column = sub->column("name");
    ASSERT_NE(nullptr, column);
    auto values = column->iterator(irs::ColumnHint::kNormal);
    ASSERT_NE(nullptr, values);
    auto* actual_value = irs::get<irs::payload>(*values);
    ASSERT_NE(nullptr, actual_value);
    auto docs = prepared->execute({.segment = *sub});
    ASSERT_FALSE(irs::get<irs::frequency>(*docs));
    ASSERT_FALSE(irs::get<irs::filter_boost>(*docs));
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::valid(docs->value()));
    auto docs_seek = prepared->execute({.segment = *sub});
    ASSERT_FALSE(irs::doc_limits::valid(docs_seek->value()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("L",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("L",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("N",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("N",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_FALSE(docs->next());
    ASSERT_TRUE(irs::doc_limits::eof(docs->value()));
    ASSERT_TRUE(irs::doc_limits::eof(docs_seek->seek(irs::doc_limits::eof())));
  }

  // "fx ... quik"
  {
    irs::by_phrase q;
    *q.mutable_field() = "phrase_anl";
    auto& lt1 = q.mutable_options()->push_back<irs::by_edit_distance_options>();
    auto& lt2 =
      q.mutable_options()->push_back<irs::by_edit_distance_options>(1);
    lt1.max_distance = 1;
    lt1.term = irs::ViewCast<irs::byte_type>(std::string_view("fx"));
    lt2.max_distance = 1;
    lt2.term = irs::ViewCast<irs::byte_type>(std::string_view("quik"));

    auto scorer = irs::scorers::get(
      "bm25", irs::type<irs::text_format::json>::get(), "{ \"b\" : 0 }");
    auto prepared_order = irs::Scorers::Prepare(*scorer);

    auto prepared = q.prepare({.index = rdr, .scorers = prepared_order});

    auto sub = rdr.begin();
    auto column = sub->column("name");
    ASSERT_NE(nullptr, column);
    auto values = column->iterator(irs::ColumnHint::kNormal);
    ASSERT_NE(nullptr, values);
    auto* actual_value = irs::get<irs::payload>(*values);
    ASSERT_NE(nullptr, actual_value);
    auto docs = prepared->execute({.segment = *sub, .scorers = prepared_order});
    auto* freq = irs::get<irs::frequency>(*docs);
    ASSERT_TRUE(freq);
    auto* boost = irs::get<irs::filter_boost>(*docs);
    ASSERT_TRUE(boost);
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::valid(docs->value()));
    auto docs_seek =
      prepared->execute({.segment = *sub, .scorers = prepared_order});
    ASSERT_FALSE(irs::doc_limits::valid(docs_seek->value()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(1, freq->value);
    ASSERT_FLOAT_EQ((0.5f + 0.75f) / 2, boost->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("L",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(freq->value, irs::get<irs::frequency>(*docs_seek)->value);
    ASSERT_FLOAT_EQ(boost->value,
                    irs::get<irs::filter_boost>(*docs_seek)->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("L",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(2, freq->value);
    ASSERT_FLOAT_EQ((0.5f + 0.75f) / 2, boost->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("N",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(freq->value, irs::get<irs::frequency>(*docs_seek)->value);
    ASSERT_FLOAT_EQ(boost->value,
                    irs::get<irs::filter_boost>(*docs_seek)->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("N",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_FALSE(docs->next());
    ASSERT_TRUE(irs::doc_limits::eof(docs->value()));
    ASSERT_TRUE(irs::doc_limits::eof(docs_seek->seek(irs::doc_limits::eof())));
  }

  // =============================
  // "fo* ... qui*" with scorer
  {
    irs::by_phrase q;
    *q.mutable_field() = "phrase_anl";
    auto& pt1 = q.mutable_options()->push_back<irs::by_prefix_options>();
    auto& pt2 = q.mutable_options()->push_back<irs::by_prefix_options>(1);
    pt1.term = irs::ViewCast<irs::byte_type>(std::string_view("fo"));
    pt2.term = irs::ViewCast<irs::byte_type>(std::string_view("qui"));

    auto scorer = irs::scorers::get(
      "bm25", irs::type<irs::text_format::json>::get(), "{ \"b\" : 0 }");
    auto prepared_order = irs::Scorers::Prepare(*scorer);

    auto prepared = q.prepare({.index = rdr, .scorers = prepared_order});

    auto sub = rdr.begin();
    auto column = sub->column("name");
    ASSERT_NE(nullptr, column);
    auto values = column->iterator(irs::ColumnHint::kNormal);
    ASSERT_NE(nullptr, values);
    auto* actual_value = irs::get<irs::payload>(*values);
    ASSERT_NE(nullptr, actual_value);
    auto docs = prepared->execute({.segment = *sub, .scorers = prepared_order});
    auto* freq = irs::get<irs::frequency>(*docs);
    ASSERT_TRUE(freq);
    ASSERT_FALSE(irs::get<irs::filter_boost>(*docs));
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::valid(docs->value()));
    auto docs_seek =
      prepared->execute({.segment = *sub, .scorers = prepared_order});
    ASSERT_FALSE(irs::doc_limits::valid(docs_seek->value()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(1, freq->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("L",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(freq->value, irs::get<irs::frequency>(*docs_seek)->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("L",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(2, freq->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("N",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(freq->value, irs::get<irs::frequency>(*docs_seek)->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("N",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_FALSE(docs->next());
    ASSERT_TRUE(irs::doc_limits::eof(docs->value()));
    ASSERT_TRUE(irs::doc_limits::eof(docs_seek->seek(irs::doc_limits::eof())));
  }

  // =============================
  // jumps ... (jumps|hotdog|the) with scorer
  {
    irs::by_phrase q;
    *q.mutable_field() = "phrase_anl";
    auto& pos0 = q.mutable_options()->push_back<irs::by_terms_options>();
    pos0.terms.emplace(
      irs::ViewCast<irs::byte_type>(std::string_view("jumps")));
    auto& pos1 = q.mutable_options()->push_back<irs::by_terms_options>(1);
    pos1.terms.emplace(irs::ViewCast<irs::byte_type>(std::string_view("jumps")),
                       0.25f);
    pos1.terms.emplace(
      irs::ViewCast<irs::byte_type>(std::string_view("hotdog")), 0.5f);
    pos1.terms.emplace(irs::ViewCast<irs::byte_type>(std::string_view("the")),
                       0.75f);

    auto scorer = irs::scorers::get(
      "bm25", irs::type<irs::text_format::json>::get(), "{ \"b\" : 0 }");
    auto prepared_order = irs::Scorers::Prepare(*scorer);

    auto prepared = q.prepare({.index = rdr, .scorers = prepared_order});

    auto sub = rdr.begin();
    auto column = sub->column("name");
    ASSERT_NE(nullptr, column);
    auto values = column->iterator(irs::ColumnHint::kNormal);
    ASSERT_NE(nullptr, values);
    auto* actual_value = irs::get<irs::payload>(*values);
    ASSERT_NE(nullptr, actual_value);
    auto docs = prepared->execute({.segment = *sub, .scorers = prepared_order});
    auto* freq = irs::get<irs::frequency>(*docs);
    ASSERT_TRUE(freq);
    auto* boost = irs::get<irs::filter_boost>(*docs);
    ASSERT_TRUE(boost);
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::valid(docs->value()));
    auto docs_seek =
      prepared->execute({.segment = *sub, .scorers = prepared_order});
    ASSERT_FALSE(irs::doc_limits::valid(docs_seek->value()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(1, freq->value);
    ASSERT_FLOAT_EQ((1.f + 0.75f) / 2, boost->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("A",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(freq->value, irs::get<irs::frequency>(*docs_seek)->value);
    ASSERT_EQ(boost->value, irs::get<irs::filter_boost>(*docs_seek)->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("A",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(2, freq->value);
    ASSERT_FLOAT_EQ(((1.f + 0.25f) / 2 + (1.f + 0.5f) / 2) / 2, boost->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("O",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(freq->value, irs::get<irs::frequency>(*docs_seek)->value);
    ASSERT_EQ(boost->value, irs::get<irs::filter_boost>(*docs_seek)->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("O",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(4, freq->value);
    ASSERT_FLOAT_EQ((1.f + 0.25f) / 2, boost->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("P",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(freq->value, irs::get<irs::frequency>(*docs_seek)->value);
    ASSERT_EQ(boost->value, irs::get<irs::filter_boost>(*docs_seek)->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("P",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(3, freq->value);
    ASSERT_FLOAT_EQ((1.f + 0.25f) / 2, boost->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("Q",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(freq->value, irs::get<irs::frequency>(*docs_seek)->value);
    ASSERT_EQ(boost->value, irs::get<irs::filter_boost>(*docs_seek)->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("Q",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(2, freq->value);
    ASSERT_FLOAT_EQ((1.f + 0.25f) / 2, boost->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("R",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(freq->value, irs::get<irs::frequency>(*docs_seek)->value);
    ASSERT_EQ(boost->value, irs::get<irs::filter_boost>(*docs_seek)->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("R",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_FALSE(docs->next());
    ASSERT_TRUE(irs::doc_limits::eof(docs->value()));
    ASSERT_TRUE(irs::doc_limits::eof(docs_seek->seek(irs::doc_limits::eof())));
  }

  // by_terms_options "fox|that" with scorer
  {
    irs::by_phrase q;
    *q.mutable_field() = "phrase_anl";
    auto& st = q.mutable_options()->push_back<irs::by_terms_options>();
    st.terms.emplace(irs::ViewCast<irs::byte_type>(std::string_view("fox")));
    st.terms.emplace(irs::ViewCast<irs::byte_type>(std::string_view("that")));

    auto scorer = irs::scorers::get(
      "bm25", irs::type<irs::text_format::json>::get(), "{ \"b\" : 0 }");
    auto prepared_order = irs::Scorers::Prepare(*scorer);

    auto prepared = q.prepare({.index = rdr, .scorers = prepared_order});

    auto sub = rdr.begin();
    auto column = sub->column("name");
    ASSERT_NE(nullptr, column);
    auto values = column->iterator(irs::ColumnHint::kNormal);
    ASSERT_NE(nullptr, values);
    auto* actual_value = irs::get<irs::payload>(*values);
    ASSERT_NE(nullptr, actual_value);
    auto docs = prepared->execute({.segment = *sub, .scorers = prepared_order});
    auto* freq = irs::get<irs::frequency>(*docs);
    ASSERT_TRUE(freq);
    ASSERT_FALSE(irs::get<irs::filter_boost>(*docs));
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::valid(docs->value()));
    auto docs_seek =
      prepared->execute({.segment = *sub, .scorers = prepared_order});
    ASSERT_FALSE(irs::doc_limits::valid(docs_seek->value()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(1, freq->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("A",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(freq->value, irs::get<irs::frequency>(*docs_seek)->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("A",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(1, freq->value);
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("B",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(freq->value, irs::get<irs::frequency>(*docs_seek)->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("B",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(1, freq->value);
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("D",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(freq->value, irs::get<irs::frequency>(*docs_seek)->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("D",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(1, freq->value);
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("G",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(freq->value, irs::get<irs::frequency>(*docs_seek)->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("G",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(1, freq->value);
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("I",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(freq->value, irs::get<irs::frequency>(*docs_seek)->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("I",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(1, freq->value);
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("K",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(freq->value, irs::get<irs::frequency>(*docs_seek)->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("K",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(1, freq->value);
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("L",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(freq->value, irs::get<irs::frequency>(*docs_seek)->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("L",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(4, freq->value);
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("N",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(freq->value, irs::get<irs::frequency>(*docs_seek)->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("N",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(1, freq->value);
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("S",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(freq->value, irs::get<irs::frequency>(*docs_seek)->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("S",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(1, freq->value);
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("T",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(freq->value, irs::get<irs::frequency>(*docs_seek)->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("T",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(1, freq->value);
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("V",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(freq->value, irs::get<irs::frequency>(*docs_seek)->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("V",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_FALSE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(irs::doc_limits::eof(docs->value()));
  }

  // by_terms_options "fox|that" with scorer and boost
  {
    irs::by_phrase q;
    *q.mutable_field() = "phrase_anl";
    auto& st = q.mutable_options()->push_back<irs::by_terms_options>();
    st.terms.emplace(irs::ViewCast<irs::byte_type>(std::string_view("fox")),
                     0.5f);
    st.terms.emplace(irs::ViewCast<irs::byte_type>(std::string_view("that")));

    auto scorer = irs::scorers::get(
      "bm25", irs::type<irs::text_format::json>::get(), "{ \"b\" : 0 }");
    auto prepared_order = irs::Scorers::Prepare(*scorer);

    auto prepared = q.prepare({.index = rdr, .scorers = prepared_order});

    auto sub = rdr.begin();
    auto column = sub->column("name");
    ASSERT_NE(nullptr, column);
    auto values = column->iterator(irs::ColumnHint::kNormal);
    ASSERT_NE(nullptr, values);
    auto* actual_value = irs::get<irs::payload>(*values);
    ASSERT_NE(nullptr, actual_value);
    auto docs = prepared->execute({.segment = *sub, .scorers = prepared_order});
    auto* freq = irs::get<irs::frequency>(*docs);
    ASSERT_TRUE(freq);
    auto* boost = irs::get<irs::filter_boost>(*docs);
    ASSERT_TRUE(boost);
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::valid(docs->value()));
    auto docs_seek =
      prepared->execute({.segment = *sub, .scorers = prepared_order});
    ASSERT_FALSE(irs::doc_limits::valid(docs_seek->value()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(1, freq->value);
    ASSERT_EQ(0.5f, boost->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("A",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(freq->value, irs::get<irs::frequency>(*docs_seek)->value);
    ASSERT_EQ(boost->value, irs::get<irs::filter_boost>(*docs_seek)->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("A",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(1, freq->value);
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("B",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(freq->value, irs::get<irs::frequency>(*docs_seek)->value);
    ASSERT_EQ(boost->value, irs::get<irs::filter_boost>(*docs_seek)->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("B",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(1, freq->value);
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(irs::kNoBoost, boost->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("D",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(freq->value, irs::get<irs::frequency>(*docs_seek)->value);
    ASSERT_EQ(boost->value, irs::get<irs::filter_boost>(*docs_seek)->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("D",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(1, freq->value);
    ASSERT_EQ(0.5f, boost->value);
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("G",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(freq->value, irs::get<irs::frequency>(*docs_seek)->value);
    ASSERT_EQ(boost->value, irs::get<irs::filter_boost>(*docs_seek)->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("G",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(1, freq->value);
    ASSERT_EQ(0.5f, boost->value);
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("I",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(freq->value, irs::get<irs::frequency>(*docs_seek)->value);
    ASSERT_EQ(boost->value, irs::get<irs::filter_boost>(*docs_seek)->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("I",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(1, freq->value);
    ASSERT_EQ(0.5f, boost->value);
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("K",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(freq->value, irs::get<irs::frequency>(*docs_seek)->value);
    ASSERT_EQ(boost->value, irs::get<irs::filter_boost>(*docs_seek)->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("K",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(1, freq->value);
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("L",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(freq->value, irs::get<irs::frequency>(*docs_seek)->value);
    ASSERT_EQ(boost->value, irs::get<irs::filter_boost>(*docs_seek)->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("L",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(4, freq->value);
    ASSERT_EQ(0.5f, boost->value);
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("N",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(freq->value, irs::get<irs::frequency>(*docs_seek)->value);
    ASSERT_EQ(boost->value, irs::get<irs::filter_boost>(*docs_seek)->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("N",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(1, freq->value);
    ASSERT_EQ(0.5f, boost->value);
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("S",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(freq->value, irs::get<irs::frequency>(*docs_seek)->value);
    ASSERT_EQ(boost->value, irs::get<irs::filter_boost>(*docs_seek)->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("S",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(1, freq->value);
    ASSERT_EQ(0.5f, boost->value);
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("T",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(freq->value, irs::get<irs::frequency>(*docs_seek)->value);
    ASSERT_EQ(boost->value, irs::get<irs::filter_boost>(*docs_seek)->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("T",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(1, freq->value);
    ASSERT_EQ(0.5f, boost->value);
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("V",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(freq->value, irs::get<irs::frequency>(*docs_seek)->value);
    ASSERT_EQ(boost->value, irs::get<irs::filter_boost>(*docs_seek)->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("V",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_FALSE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(irs::doc_limits::eof(docs->value()));
  }

  // test disjunctions (unary, basic, small, disjunction)
  {
    irs::by_phrase q;
    *q.mutable_field() = "phrase_anl";
    auto& wt1 = q.mutable_options()->push_back<irs::by_wildcard_options>();
    auto& wt2 = q.mutable_options()->push_back<irs::by_wildcard_options>();
    auto& pt1 = q.mutable_options()->push_back<irs::by_prefix_options>();
    auto& pt2 = q.mutable_options()->push_back<irs::by_prefix_options>();
    wt1.term = irs::ViewCast<irs::byte_type>(std::string_view("%las"));
    wt2.term = irs::ViewCast<irs::byte_type>(std::string_view("%nd"));
    pt1.term = irs::ViewCast<irs::byte_type>(std::string_view("go"));
    pt2.term = irs::ViewCast<irs::byte_type>(std::string_view("like"));

    auto scorer = irs::scorers::get(
      "bm25", irs::type<irs::text_format::json>::get(), "{ \"b\" : 0 }");
    auto prepared_order = irs::Scorers::Prepare(*scorer);

    auto prepared = q.prepare({.index = rdr, .scorers = prepared_order});

    auto sub = rdr.begin();
    auto column = sub->column("name");
    ASSERT_NE(nullptr, column);
    auto values = column->iterator(irs::ColumnHint::kNormal);
    ASSERT_NE(nullptr, values);
    auto* actual_value = irs::get<irs::payload>(*values);
    ASSERT_NE(nullptr, actual_value);
    auto docs = prepared->execute({.segment = *sub, .scorers = prepared_order});
    auto* freq = irs::get<irs::frequency>(*docs);
    ASSERT_TRUE(freq);
    ASSERT_FALSE(irs::get<irs::filter_boost>(*docs));
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::valid(docs->value()));
    auto docs_seek =
      prepared->execute({.segment = *sub, .scorers = prepared_order});
    ASSERT_FALSE(irs::doc_limits::valid(docs_seek->value()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(1, freq->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("Z",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(freq->value, irs::get<irs::frequency>(*docs_seek)->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("Z",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_FALSE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(irs::doc_limits::eof(docs->value()));
  }

  // =============================

  // "fox ... quick" with phrase offset
  // which is does not matter
  {
    irs::by_phrase q;
    *q.mutable_field() = "phrase_anl";
    q.mutable_options()
      ->push_back<irs::by_term_options>(std::numeric_limits<size_t>::max())
      .term = irs::ViewCast<irs::byte_type>(std::string_view("fox"));
    q.mutable_options()->push_back<irs::by_term_options>(1).term =
      irs::ViewCast<irs::byte_type>(std::string_view("quick"));

    auto prepared = q.prepare({.index = rdr});

    auto sub = rdr.begin();
    auto column = sub->column("name");
    ASSERT_NE(nullptr, column);
    auto values = column->iterator(irs::ColumnHint::kNormal);
    ASSERT_NE(nullptr, values);
    auto* actual_value = irs::get<irs::payload>(*values);
    ASSERT_NE(nullptr, actual_value);
    auto docs = prepared->execute({.segment = *sub});
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::valid(docs->value()));
    auto docs_seek = prepared->execute({.segment = *sub});
    ASSERT_FALSE(irs::doc_limits::valid(docs_seek->value()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("L",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("L",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("N",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("N",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_FALSE(docs->next());
    ASSERT_TRUE(irs::doc_limits::eof(docs->value()));
    ASSERT_TRUE(irs::doc_limits::eof(docs_seek->seek(irs::doc_limits::eof())));
  }

  // "fox quick"
  // const_max and zero offset
  {
    irs::by_phrase q;
    *q.mutable_field() = "phrase_anl";
    q.mutable_options()
      ->push_back<irs::by_term_options>(std::numeric_limits<size_t>::max())
      .term = irs::ViewCast<irs::byte_type>(std::string_view("fox"));
    q.mutable_options()->push_back<irs::by_term_options>(0).term =
      irs::ViewCast<irs::byte_type>(std::string_view("quick"));

    auto prepared = q.prepare({.index = rdr});

    auto sub = rdr.begin();
    auto column = sub->column("name");
    ASSERT_NE(nullptr, column);
    auto values = column->iterator(irs::ColumnHint::kNormal);
    ASSERT_NE(nullptr, values);
    auto* actual_value = irs::get<irs::payload>(*values);
    ASSERT_NE(nullptr, actual_value);
    auto docs = prepared->execute({.segment = *sub});
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::valid(docs->value()));
    auto docs_seek = prepared->execute({.segment = *sub});
    ASSERT_FALSE(irs::doc_limits::valid(docs_seek->value()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("N",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("N",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_FALSE(docs->next());
    ASSERT_TRUE(irs::doc_limits::eof(docs->value()));
    ASSERT_TRUE(irs::doc_limits::eof(docs_seek->seek(irs::doc_limits::eof())));
  }

  // "fox* quick*"
  // const_max and zero offset
  {
    irs::by_phrase q;
    *q.mutable_field() = "phrase_anl";
    auto& pt1 = q.mutable_options()->push_back<irs::by_prefix_options>(
      std::numeric_limits<size_t>::max());
    auto& pt2 = q.mutable_options()->push_back<irs::by_prefix_options>(0);
    pt1.term = irs::ViewCast<irs::byte_type>(std::string_view("fox"));
    pt2.term = irs::ViewCast<irs::byte_type>(std::string_view("quick"));

    auto prepared = q.prepare({.index = rdr});

    auto sub = rdr.begin();
    auto column = sub->column("name");
    ASSERT_NE(nullptr, column);
    auto values = column->iterator(irs::ColumnHint::kNormal);
    ASSERT_NE(nullptr, values);
    auto* actual_value = irs::get<irs::payload>(*values);
    ASSERT_NE(nullptr, actual_value);
    auto docs = prepared->execute({.segment = *sub});
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::valid(docs->value()));
    auto docs_seek = prepared->execute({.segment = *sub});
    ASSERT_FALSE(irs::doc_limits::valid(docs_seek->value()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("N",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("N",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_FALSE(docs->next());
    ASSERT_TRUE(irs::doc_limits::eof(docs->value()));
    ASSERT_TRUE(irs::doc_limits::eof(docs_seek->seek(irs::doc_limits::eof())));
  }

  // "fo* ... quick" with phrase offset
  // which is does not matter
  {
    irs::by_phrase q;
    *q.mutable_field() = "phrase_anl";
    auto& pt = q.mutable_options()->push_back<irs::by_prefix_options>(
      std::numeric_limits<size_t>::max());
    pt.term = irs::ViewCast<irs::byte_type>(std::string_view("fo"));
    q.mutable_options()->push_back<irs::by_term_options>(1).term =
      irs::ViewCast<irs::byte_type>(std::string_view("quick"));

    auto prepared = q.prepare({.index = rdr});

    auto sub = rdr.begin();
    auto column = sub->column("name");
    ASSERT_NE(nullptr, column);
    auto values = column->iterator(irs::ColumnHint::kNormal);
    ASSERT_NE(nullptr, values);
    auto* actual_value = irs::get<irs::payload>(*values);
    ASSERT_NE(nullptr, actual_value);
    auto docs = prepared->execute({.segment = *sub});
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::valid(docs->value()));
    auto docs_seek = prepared->execute({.segment = *sub});
    ASSERT_FALSE(irs::doc_limits::valid(docs_seek->value()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("L",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("L",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("N",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("N",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_FALSE(docs->next());
    ASSERT_TRUE(irs::doc_limits::eof(docs->value()));
    ASSERT_TRUE(irs::doc_limits::eof(docs_seek->seek(irs::doc_limits::eof())));
  }

  // "f_x ... quick" with phrase offset
  // which is does not matter
  {
    irs::by_phrase q;
    *q.mutable_field() = "phrase_anl";
    auto& wt = q.mutable_options()->push_back<irs::by_wildcard_options>(
      std::numeric_limits<size_t>::max());
    wt.term = irs::ViewCast<irs::byte_type>(std::string_view("f_x"));
    q.mutable_options()->push_back<irs::by_term_options>(1).term =
      irs::ViewCast<irs::byte_type>(std::string_view("quick"));

    auto prepared = q.prepare({.index = rdr});

    auto sub = rdr.begin();
    auto column = sub->column("name");
    ASSERT_NE(nullptr, column);
    auto values = column->iterator(irs::ColumnHint::kNormal);
    ASSERT_NE(nullptr, values);
    auto* actual_value = irs::get<irs::payload>(*values);
    ASSERT_NE(nullptr, actual_value);
    auto docs = prepared->execute({.segment = *sub});
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::valid(docs->value()));
    auto docs_seek = prepared->execute({.segment = *sub});
    ASSERT_FALSE(irs::doc_limits::valid(docs_seek->value()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("L",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("L",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("N",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("N",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_FALSE(docs->next());
    ASSERT_TRUE(irs::doc_limits::eof(docs->value()));
    ASSERT_TRUE(irs::doc_limits::eof(docs_seek->seek(irs::doc_limits::eof())));
  }

  // "fox ... qui*" with phrase offset
  // which is does not matter
  {
    irs::by_phrase q;
    *q.mutable_field() = "phrase_anl";
    q.mutable_options()
      ->push_back<irs::by_term_options>(std::numeric_limits<size_t>::max())
      .term = irs::ViewCast<irs::byte_type>(std::string_view("fox"));
    auto& pt = q.mutable_options()->push_back<irs::by_prefix_options>(1);
    pt.term = irs::ViewCast<irs::byte_type>(std::string_view("qui"));

    auto prepared = q.prepare({.index = rdr});

    auto sub = rdr.begin();
    auto column = sub->column("name");
    ASSERT_NE(nullptr, column);
    auto values = column->iterator(irs::ColumnHint::kNormal);
    ASSERT_NE(nullptr, values);
    auto* actual_value = irs::get<irs::payload>(*values);
    ASSERT_NE(nullptr, actual_value);
    auto docs = prepared->execute({.segment = *sub});
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::valid(docs->value()));
    auto docs_seek = prepared->execute({.segment = *sub});
    ASSERT_FALSE(irs::doc_limits::valid(docs_seek->value()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("L",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("L",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("N",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("N",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_FALSE(docs->next());
    ASSERT_TRUE(irs::doc_limits::eof(docs->value()));
    ASSERT_TRUE(irs::doc_limits::eof(docs_seek->seek(irs::doc_limits::eof())));
  }

  // "fox ... qui%k" with phrase offset
  // which is does not matter
  {
    irs::by_phrase q;
    *q.mutable_field() = "phrase_anl";
    q.mutable_options()
      ->push_back<irs::by_term_options>(std::numeric_limits<size_t>::max())
      .term = irs::ViewCast<irs::byte_type>(std::string_view("fox"));
    auto& wt = q.mutable_options()->push_back<irs::by_wildcard_options>(1);
    wt.term = irs::ViewCast<irs::byte_type>(std::string_view("qui%k"));

    auto prepared = q.prepare({.index = rdr});

    auto sub = rdr.begin();
    auto column = sub->column("name");
    ASSERT_NE(nullptr, column);
    auto values = column->iterator(irs::ColumnHint::kNormal);
    ASSERT_NE(nullptr, values);
    auto* actual_value = irs::get<irs::payload>(*values);
    ASSERT_NE(nullptr, actual_value);
    auto docs = prepared->execute({.segment = *sub});
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::valid(docs->value()));
    auto docs_seek = prepared->execute({.segment = *sub});
    ASSERT_FALSE(irs::doc_limits::valid(docs_seek->value()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("L",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("L",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("N",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("N",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_FALSE(docs->next());
    ASSERT_TRUE(irs::doc_limits::eof(docs->value()));
    ASSERT_TRUE(irs::doc_limits::eof(docs_seek->seek(irs::doc_limits::eof())));
  }

  // "fo* ... qui*" with phrase offset
  // which is does not matter
  {
    irs::by_phrase q;
    *q.mutable_field() = "phrase_anl";
    auto& pt1 = q.mutable_options()->push_back<irs::by_prefix_options>(
      std::numeric_limits<size_t>::max());
    auto& pt2 = q.mutable_options()->push_back<irs::by_prefix_options>(1);
    pt1.term = irs::ViewCast<irs::byte_type>(std::string_view("fo"));
    pt2.term = irs::ViewCast<irs::byte_type>(std::string_view("qui"));

    auto prepared = q.prepare({.index = rdr});

    auto sub = rdr.begin();
    auto column = sub->column("name");
    ASSERT_NE(nullptr, column);
    auto values = column->iterator(irs::ColumnHint::kNormal);
    ASSERT_NE(nullptr, values);
    auto* actual_value = irs::get<irs::payload>(*values);
    ASSERT_NE(nullptr, actual_value);
    auto docs = prepared->execute({.segment = *sub});
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::valid(docs->value()));
    auto docs_seek = prepared->execute({.segment = *sub});
    ASSERT_FALSE(irs::doc_limits::valid(docs_seek->value()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("L",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("L",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("N",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("N",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_FALSE(docs->next());
    ASSERT_TRUE(irs::doc_limits::eof(docs->value()));
    ASSERT_TRUE(irs::doc_limits::eof(docs_seek->seek(irs::doc_limits::eof())));
  }

  // "fo% ... qui%" with phrase offset
  // which is does not matter
  {
    irs::by_phrase q;
    *q.mutable_field() = "phrase_anl";
    auto& wt1 = q.mutable_options()->push_back<irs::by_wildcard_options>(
      std::numeric_limits<size_t>::max());
    auto& wt2 = q.mutable_options()->push_back<irs::by_wildcard_options>(1);
    wt1.term = irs::ViewCast<irs::byte_type>(std::string_view("fo%"));
    wt2.term = irs::ViewCast<irs::byte_type>(std::string_view("qui%"));

    auto prepared = q.prepare({.index = rdr});

    auto sub = rdr.begin();
    auto column = sub->column("name");
    ASSERT_NE(nullptr, column);
    auto values = column->iterator(irs::ColumnHint::kNormal);
    ASSERT_NE(nullptr, values);
    auto* actual_value = irs::get<irs::payload>(*values);
    ASSERT_NE(nullptr, actual_value);
    auto docs = prepared->execute({.segment = *sub});
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::valid(docs->value()));
    auto docs_seek = prepared->execute({.segment = *sub});
    ASSERT_FALSE(irs::doc_limits::valid(docs_seek->value()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("L",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("L",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("N",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("N",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_FALSE(docs->next());
    ASSERT_TRUE(irs::doc_limits::eof(docs->value()));
    ASSERT_TRUE(irs::doc_limits::eof(docs_seek->seek(irs::doc_limits::eof())));
  }

  // "fo% ... quik" with phrase offset
  // which is does not matter
  {
    irs::by_phrase q;
    *q.mutable_field() = "phrase_anl";
    auto& wt = q.mutable_options()->push_back<irs::by_wildcard_options>(
      std::numeric_limits<size_t>::max());
    auto& lt = q.mutable_options()->push_back<irs::by_edit_distance_options>(1);
    wt.term = irs::ViewCast<irs::byte_type>(std::string_view("fo%"));
    lt.max_distance = 1;
    lt.term = irs::ViewCast<irs::byte_type>(std::string_view("quik"));

    auto prepared = q.prepare({.index = rdr});

    auto sub = rdr.begin();
    auto column = sub->column("name");
    ASSERT_NE(nullptr, column);
    auto values = column->iterator(irs::ColumnHint::kNormal);
    ASSERT_NE(nullptr, values);
    auto* actual_value = irs::get<irs::payload>(*values);
    ASSERT_NE(nullptr, actual_value);
    auto docs = prepared->execute({.segment = *sub});
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::valid(docs->value()));
    auto docs_seek = prepared->execute({.segment = *sub});
    ASSERT_FALSE(irs::doc_limits::valid(docs_seek->value()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("L",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("L",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("N",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("N",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_FALSE(docs->next());
    ASSERT_TRUE(irs::doc_limits::eof(docs->value()));
    ASSERT_TRUE(irs::doc_limits::eof(docs_seek->seek(irs::doc_limits::eof())));
  }

  // "fox ... ... ... ... ... ... ... ... ... ... quick"
  {
    irs::by_phrase q;
    *q.mutable_field() = "phrase_anl";
    q.mutable_options()->push_back<irs::by_term_options>().term =
      irs::ViewCast<irs::byte_type>(std::string_view("fox"));
    q.mutable_options()->push_back<irs::by_term_options>(10).term =
      irs::ViewCast<irs::byte_type>(std::string_view("quick"));

    auto prepared = q.prepare({.index = rdr});

    auto sub = rdr.begin();
    auto docs = prepared->execute({.segment = *sub});
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::valid(docs->value()));
    ASSERT_FALSE(docs->next());
    ASSERT_TRUE(irs::doc_limits::eof(docs->value()));
  }

  // "fox ... ... ... ... ... ... ... ... ... ... qui*"
  {
    irs::by_phrase q;
    *q.mutable_field() = "phrase_anl";
    q.mutable_options()->push_back<irs::by_term_options>().term =
      irs::ViewCast<irs::byte_type>(std::string_view("fox"));
    auto& pt = q.mutable_options()->push_back<irs::by_prefix_options>(10);
    pt.term = irs::ViewCast<irs::byte_type>(std::string_view("qui"));

    auto prepared = q.prepare({.index = rdr});

    auto sub = rdr.begin();
    auto docs = prepared->execute({.segment = *sub});
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::valid(docs->value()));
    ASSERT_FALSE(docs->next());
    ASSERT_TRUE(irs::doc_limits::eof(docs->value()));
  }

  // "fox ... ... ... ... ... ... ... ... ... ... qu_ck"
  {
    irs::by_phrase q;
    *q.mutable_field() = "phrase_anl";
    q.mutable_options()->push_back<irs::by_term_options>().term =
      irs::ViewCast<irs::byte_type>(std::string_view("fox"));
    auto& wt = q.mutable_options()->push_back<irs::by_wildcard_options>(10);
    wt.term = irs::ViewCast<irs::byte_type>(std::string_view("qu_ck"));

    auto prepared = q.prepare({.index = rdr});

    auto sub = rdr.begin();
    auto docs = prepared->execute({.segment = *sub});
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::valid(docs->value()));
    ASSERT_FALSE(docs->next());
    ASSERT_TRUE(irs::doc_limits::eof(docs->value()));
  }

  // "fox ... ... ... ... ... ... ... ... ... ... quc"
  {
    irs::by_phrase q;
    *q.mutable_field() = "phrase_anl";
    q.mutable_options()->push_back<irs::by_term_options>().term =
      irs::ViewCast<irs::byte_type>(std::string_view("fox"));
    auto& lt =
      q.mutable_options()->push_back<irs::by_edit_distance_options>(10);
    lt.max_distance = 2;
    lt.term = irs::ViewCast<irs::byte_type>(std::string_view("quc"));

    auto prepared = q.prepare({.index = rdr});

    auto sub = rdr.begin();
    auto docs = prepared->execute({.segment = *sub});
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::valid(docs->value()));
    ASSERT_FALSE(docs->next());
    ASSERT_TRUE(irs::doc_limits::eof(docs->value()));
  }

  // "eye ... eye"
  {
    irs::by_phrase q;
    *q.mutable_field() = "phrase_anl";
    q.mutable_options()->push_back<irs::by_term_options>().term =
      irs::ViewCast<irs::byte_type>(std::string_view("eye"));
    q.mutable_options()->push_back<irs::by_term_options>(1).term =
      irs::ViewCast<irs::byte_type>(std::string_view("eye"));

    auto prepared = q.prepare({.index = rdr});

    auto sub = rdr.begin();
    auto column = sub->column("name");
    ASSERT_NE(nullptr, column);
    auto values = column->iterator(irs::ColumnHint::kNormal);
    ASSERT_NE(nullptr, values);
    auto* actual_value = irs::get<irs::payload>(*values);
    ASSERT_NE(nullptr, actual_value);
    auto docs = prepared->execute({.segment = *sub});
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::valid(docs->value()));
    auto docs_seek = prepared->execute({.segment = *sub});
    ASSERT_FALSE(irs::doc_limits::valid(docs_seek->value()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("C",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("C",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_FALSE(docs->next());
    ASSERT_TRUE(irs::doc_limits::eof(docs->value()));
    ASSERT_TRUE(irs::doc_limits::eof(docs_seek->seek(irs::doc_limits::eof())));
  }

  // "as in the past we are looking forward"
  {
    irs::by_phrase q;
    *q.mutable_field() = "phrase_anl";
    q.mutable_options()->push_back<irs::by_term_options>().term =
      irs::ViewCast<irs::byte_type>(std::string_view("as"));
    q.mutable_options()->push_back<irs::by_term_options>().term =
      irs::ViewCast<irs::byte_type>(std::string_view("in"));
    q.mutable_options()->push_back<irs::by_term_options>().term =
      irs::ViewCast<irs::byte_type>(std::string_view("the"));
    q.mutable_options()->push_back<irs::by_term_options>().term =
      irs::ViewCast<irs::byte_type>(std::string_view("past"));
    q.mutable_options()->push_back<irs::by_term_options>().term =
      irs::ViewCast<irs::byte_type>(std::string_view("we"));
    q.mutable_options()->push_back<irs::by_term_options>().term =
      irs::ViewCast<irs::byte_type>(std::string_view("are"));
    q.mutable_options()->push_back<irs::by_term_options>().term =
      irs::ViewCast<irs::byte_type>(std::string_view("looking"));
    q.mutable_options()->push_back<irs::by_term_options>().term =
      irs::ViewCast<irs::byte_type>(std::string_view("forward"));

    auto prepared = q.prepare({.index = rdr});
    auto sub = rdr.begin();
    auto column = sub->column("name");
    ASSERT_NE(nullptr, column);
    auto values = column->iterator(irs::ColumnHint::kNormal);
    ASSERT_NE(nullptr, values);
    auto* actual_value = irs::get<irs::payload>(*values);
    ASSERT_NE(nullptr, actual_value);
    auto docs = prepared->execute({.segment = *sub});
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::valid(docs->value()));
    auto docs_seek = prepared->execute({.segment = *sub});
    ASSERT_FALSE(irs::doc_limits::valid(docs_seek->value()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("H",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("H",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_FALSE(docs->next());
    ASSERT_TRUE(irs::doc_limits::eof(docs->value()));
    ASSERT_TRUE(irs::doc_limits::eof(docs_seek->seek(irs::doc_limits::eof())));
  }

  // "as in % past we ___ looking forward"
  {
    irs::by_phrase q;
    *q.mutable_field() = "phrase_anl";
    auto& lt = q.mutable_options()->push_back<irs::by_edit_distance_options>();
    lt.max_distance = 2;
    lt.term = irs::ViewCast<irs::byte_type>(std::string_view("ass"));
    q.mutable_options()->push_back<irs::by_term_options>().term =
      irs::ViewCast<irs::byte_type>(std::string_view("in"));
    auto& wt1 = q.mutable_options()->push_back<irs::by_wildcard_options>();
    wt1.term = irs::ViewCast<irs::byte_type>(std::string_view("%"));
    q.mutable_options()->push_back<irs::by_term_options>().term =
      irs::ViewCast<irs::byte_type>(std::string_view("past"));
    q.mutable_options()->push_back<irs::by_term_options>().term =
      irs::ViewCast<irs::byte_type>(std::string_view("we"));
    auto& wt2 = q.mutable_options()->push_back<irs::by_wildcard_options>();
    wt2.term = irs::ViewCast<irs::byte_type>(std::string_view("___"));
    auto& st = q.mutable_options()->push_back<irs::by_terms_options>();
    st.terms.emplace(
      irs::ViewCast<irs::byte_type>(std::string_view("looking")));
    st.terms.emplace(
      irs::ViewCast<irs::byte_type>(std::string_view("searching")));
    auto& pt = q.mutable_options()->push_back<irs::by_prefix_options>();
    pt.term = irs::ViewCast<irs::byte_type>(std::string_view("fo"));

    auto prepared = q.prepare({.index = rdr});
    auto sub = rdr.begin();
    auto column = sub->column("name");
    ASSERT_NE(nullptr, column);
    auto values = column->iterator(irs::ColumnHint::kNormal);
    ASSERT_NE(nullptr, values);
    auto* actual_value = irs::get<irs::payload>(*values);
    ASSERT_NE(nullptr, actual_value);
    auto docs = prepared->execute({.segment = *sub});
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::valid(docs->value()));
    auto docs_seek = prepared->execute({.segment = *sub});
    ASSERT_FALSE(irs::doc_limits::valid(docs_seek->value()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("H",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("H",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_FALSE(docs->next());
    ASSERT_TRUE(irs::doc_limits::eof(docs->value()));
    ASSERT_TRUE(irs::doc_limits::eof(docs_seek->seek(irs::doc_limits::eof())));
  }

  // "as in the past we are looking forward" with order
  {
    irs::by_phrase q;
    *q.mutable_field() = "phrase_anl";
    q.mutable_options()->push_back<irs::by_term_options>().term =
      irs::ViewCast<irs::byte_type>(std::string_view("as"));
    q.mutable_options()->push_back<irs::by_term_options>().term =
      irs::ViewCast<irs::byte_type>(std::string_view("in"));
    q.mutable_options()->push_back<irs::by_term_options>().term =
      irs::ViewCast<irs::byte_type>(std::string_view("the"));
    q.mutable_options()->push_back<irs::by_term_options>().term =
      irs::ViewCast<irs::byte_type>(std::string_view("past"));
    q.mutable_options()->push_back<irs::by_term_options>().term =
      irs::ViewCast<irs::byte_type>(std::string_view("we"));
    q.mutable_options()->push_back<irs::by_term_options>().term =
      irs::ViewCast<irs::byte_type>(std::string_view("are"));
    q.mutable_options()->push_back<irs::by_term_options>().term =
      irs::ViewCast<irs::byte_type>(std::string_view("looking"));
    q.mutable_options()->push_back<irs::by_term_options>().term =
      irs::ViewCast<irs::byte_type>(std::string_view("forward"));

    tests::sort::custom_sort sort;
    sort.scorer_score = [](irs::doc_id_t doc, irs::score_t* score) {
      ASSERT_NE(nullptr, score);
      *score = doc;
    };
    auto pord = irs::Scorers::Prepare(sort);

    auto prepared = q.prepare({.index = rdr, .scorers = pord});
    auto sub = rdr.begin();
    auto column = sub->column("name");
    ASSERT_NE(nullptr, column);
    auto values = column->iterator(irs::ColumnHint::kNormal);
    ASSERT_NE(nullptr, values);
    auto* actual_value = irs::get<irs::payload>(*values);
    ASSERT_NE(nullptr, actual_value);
    auto docs = prepared->execute({.segment = *sub, .scorers = pord});
    auto* freq = irs::get<irs::frequency>(*docs);
    ASSERT_TRUE(freq);
    ASSERT_FALSE(irs::get<irs::filter_boost>(*docs));
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::valid(docs->value()));
    auto docs_seek = prepared->execute({.segment = *sub, .scorers = pord});
    ASSERT_FALSE(irs::doc_limits::valid(docs_seek->value()));
    auto* score = irs::get<irs::score>(*docs);
    ASSERT_FALSE(!score);

    ASSERT_TRUE(docs->next());
    irs::score_t score_value;
    (*score)(&score_value);
    ASSERT_EQ(docs->value(), score_value);
    ASSERT_EQ(1, freq->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("H",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(freq->value, irs::get<irs::frequency>(*docs_seek)->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("H",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_FALSE(docs->next());
    ASSERT_TRUE(irs::doc_limits::eof(docs->value()));
    ASSERT_TRUE(irs::doc_limits::eof(docs_seek->seek(irs::doc_limits::eof())));
  }

  // "as in the p_st we are look* forward" with order
  {
    irs::by_phrase q;
    *q.mutable_field() = "phrase_anl";
    q.mutable_options()->push_back<irs::by_term_options>().term =
      irs::ViewCast<irs::byte_type>(std::string_view("as"));
    q.mutable_options()->push_back<irs::by_term_options>().term =
      irs::ViewCast<irs::byte_type>(std::string_view("in"));
    q.mutable_options()->push_back<irs::by_term_options>().term =
      irs::ViewCast<irs::byte_type>(std::string_view("the"));
    auto& wt = q.mutable_options()->push_back<irs::by_wildcard_options>();
    wt.term = irs::ViewCast<irs::byte_type>(std::string_view("p_st"));
    q.mutable_options()->push_back<irs::by_term_options>().term =
      irs::ViewCast<irs::byte_type>(std::string_view("we"));
    q.mutable_options()->push_back<irs::by_term_options>().term =
      irs::ViewCast<irs::byte_type>(std::string_view("are"));
    auto& pt = q.mutable_options()->push_back<irs::by_prefix_options>();
    pt.term = irs::ViewCast<irs::byte_type>(std::string_view("look"));
    q.mutable_options()->push_back<irs::by_term_options>().term =
      irs::ViewCast<irs::byte_type>(std::string_view("forward"));

    tests::sort::custom_sort sort;
    sort.scorer_score = [](irs::doc_id_t doc, irs::score_t* score) {
      ASSERT_NE(nullptr, score);
      *score = doc;
    };
    auto pord = irs::Scorers::Prepare(sort);

    auto prepared = q.prepare({.index = rdr, .scorers = pord});
    auto sub = rdr.begin();
    auto column = sub->column("name");
    ASSERT_NE(nullptr, column);
    auto values = column->iterator(irs::ColumnHint::kNormal);
    ASSERT_NE(nullptr, values);
    auto* actual_value = irs::get<irs::payload>(*values);
    ASSERT_NE(nullptr, actual_value);
    auto docs = prepared->execute({.segment = *sub, .scorers = pord});
    auto* freq = irs::get<irs::frequency>(*docs);
    ASSERT_TRUE(freq);
    ASSERT_FALSE(irs::get<irs::filter_boost>(*docs));
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::valid(docs->value()));
    auto docs_seek = prepared->execute({.segment = *sub, .scorers = pord});
    ASSERT_FALSE(irs::doc_limits::valid(docs_seek->value()));
    auto* score = irs::get<irs::score>(*docs);
    ASSERT_FALSE(!score);

    ASSERT_TRUE(docs->next());
    irs::score_t score_value;
    (*score)(&score_value);
    ASSERT_EQ(docs->value(), score_value);
    ASSERT_EQ(1, freq->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("H",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(freq->value, irs::get<irs::frequency>(*docs_seek)->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("H",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_FALSE(docs->next());
    ASSERT_TRUE(irs::doc_limits::eof(docs->value()));
    ASSERT_TRUE(irs::doc_limits::eof(docs_seek->seek(irs::doc_limits::eof())));
  }

  // fox quick
  {
    irs::by_phrase q;
    *q.mutable_field() = "phrase_anl";
    q.mutable_options()->push_back<irs::by_term_options>().term =
      irs::ViewCast<irs::byte_type>(std::string_view("fox"));
    q.mutable_options()->push_back<irs::by_term_options>().term =
      irs::ViewCast<irs::byte_type>(std::string_view("quick"));

    auto prepared = q.prepare({.index = rdr});

    auto sub = rdr.begin();
    auto column = sub->column("name");
    ASSERT_NE(nullptr, column);
    auto values = column->iterator(irs::ColumnHint::kNormal);
    ASSERT_NE(nullptr, values);
    auto* actual_value = irs::get<irs::payload>(*values);
    ASSERT_NE(nullptr, actual_value);
    auto docs = prepared->execute({.segment = *sub});
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::valid(docs->value()));
    auto docs_seek = prepared->execute({.segment = *sub});
    ASSERT_FALSE(irs::doc_limits::valid(docs_seek->value()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("N",
              irs::to_string<std::string_view>(actual_value->value.data()));
    // Check repeatable seek to the same document given frequency of the phrase
    // within the document = 2
    auto v = docs->value();
    ASSERT_EQ(v, docs->seek(docs->value()));
    ASSERT_EQ(v, docs->seek(docs->value()));
    ASSERT_EQ("N",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("N",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_FALSE(docs->next());
    ASSERT_TRUE(irs::doc_limits::eof(docs->value()));
    ASSERT_TRUE(irs::doc_limits::eof(docs_seek->seek(irs::doc_limits::eof())));
  }

  // fox quick with order
  {
    irs::by_phrase q;
    *q.mutable_field() = "phrase_anl";
    q.mutable_options()->push_back<irs::by_term_options>().term =
      irs::ViewCast<irs::byte_type>(std::string_view("fox"));
    q.mutable_options()->push_back<irs::by_term_options>().term =
      irs::ViewCast<irs::byte_type>(std::string_view("quick"));

    tests::sort::custom_sort sort;
    sort.scorer_score = [](irs::doc_id_t doc, irs::score_t* score) {
      ASSERT_NE(nullptr, score);
      *score = doc;
    };
    auto pord = irs::Scorers::Prepare(sort);

    auto prepared = q.prepare({.index = rdr, .scorers = pord});

    auto sub = rdr.begin();
    auto column = sub->column("name");
    ASSERT_NE(nullptr, column);
    auto values = column->iterator(irs::ColumnHint::kNormal);
    ASSERT_NE(nullptr, values);
    auto* actual_value = irs::get<irs::payload>(*values);
    ASSERT_NE(nullptr, actual_value);
    auto docs = prepared->execute({.segment = *sub, .scorers = pord});
    auto* freq = irs::get<irs::frequency>(*docs);
    ASSERT_TRUE(freq);
    ASSERT_FALSE(irs::get<irs::filter_boost>(*docs));
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::valid(docs->value()));
    auto docs_seek = prepared->execute({.segment = *sub, .scorers = pord});
    ASSERT_FALSE(irs::doc_limits::valid(docs_seek->value()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(2, freq->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("N",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(freq->value, irs::get<irs::frequency>(*docs_seek)->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("N",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_FALSE(docs->next());
    ASSERT_TRUE(irs::doc_limits::eof(docs->value()));
    ASSERT_TRUE(irs::doc_limits::eof(docs_seek->seek(irs::doc_limits::eof())));
  }

  // wildcard_filter "zo\\_%"
  {
    irs::by_phrase q;
    *q.mutable_field() = "phrase_anl";
    auto& wt = q.mutable_options()->push_back<irs::by_wildcard_options>();
    wt.term = irs::ViewCast<irs::byte_type>(std::string_view("zo\\_%"));

    auto prepared = q.prepare({.index = rdr});
    auto sub = rdr.begin();
    auto column = sub->column("name");
    ASSERT_NE(nullptr, column);
    auto values = column->iterator(irs::ColumnHint::kNormal);
    ASSERT_NE(nullptr, values);
    auto* actual_value = irs::get<irs::payload>(*values);
    ASSERT_NE(nullptr, actual_value);

    auto docs = prepared->execute({.segment = *sub});
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::valid(docs->value()));
    auto docs_seek = prepared->execute({.segment = *sub});
    ASSERT_FALSE(irs::doc_limits::valid(docs_seek->value()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("PHW0",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("PHW0",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_FALSE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(irs::doc_limits::eof(docs->value()));
  }

  // wildcard_filter "\\_oo"
  {
    irs::by_phrase q;
    *q.mutable_field() = "phrase_anl";
    auto& wt = q.mutable_options()->push_back<irs::by_wildcard_options>();
    wt.term = irs::ViewCast<irs::byte_type>(std::string_view("\\_oo"));

    auto prepared = q.prepare({.index = rdr});
    auto sub = rdr.begin();
    auto column = sub->column("name");
    ASSERT_NE(nullptr, column);
    auto values = column->iterator(irs::ColumnHint::kNormal);
    ASSERT_NE(nullptr, values);
    auto* actual_value = irs::get<irs::payload>(*values);
    ASSERT_NE(nullptr, actual_value);

    auto docs = prepared->execute({.segment = *sub});
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::valid(docs->value()));
    auto docs_seek = prepared->execute({.segment = *sub});
    ASSERT_FALSE(irs::doc_limits::valid(docs_seek->value()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("PHW1",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("PHW1",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_FALSE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(irs::doc_limits::eof(docs->value()));
  }

  // wildcard_filter "z\\_o"
  {
    irs::by_phrase q;
    *q.mutable_field() = "phrase_anl";
    auto& wt = q.mutable_options()->push_back<irs::by_wildcard_options>();
    wt.term = irs::ViewCast<irs::byte_type>(std::string_view("z\\_o"));

    auto prepared = q.prepare({.index = rdr});
    auto sub = rdr.begin();
    auto column = sub->column("name");
    ASSERT_NE(nullptr, column);
    auto values = column->iterator(irs::ColumnHint::kNormal);
    ASSERT_NE(nullptr, values);
    auto* actual_value = irs::get<irs::payload>(*values);
    ASSERT_NE(nullptr, actual_value);

    auto docs = prepared->execute({.segment = *sub});
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::valid(docs->value()));
    auto docs_seek = prepared->execute({.segment = *sub});
    ASSERT_FALSE(irs::doc_limits::valid(docs_seek->value()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("PHW2",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("PHW2",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_FALSE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(irs::doc_limits::eof(docs->value()));
  }

  // wildcard_filter "elephant giraff\\_%"
  {
    irs::by_phrase q;
    *q.mutable_field() = "phrase_anl";
    q.mutable_options()->push_back<irs::by_term_options>().term =
      irs::ViewCast<irs::byte_type>(std::string_view("elephant"));
    auto& wt = q.mutable_options()->push_back<irs::by_wildcard_options>();
    wt.term = irs::ViewCast<irs::byte_type>(std::string_view("giraff\\_%"));

    auto prepared = q.prepare({.index = rdr});
    auto sub = rdr.begin();
    auto column = sub->column("name");
    ASSERT_NE(nullptr, column);
    auto values = column->iterator(irs::ColumnHint::kNormal);
    ASSERT_NE(nullptr, values);
    auto* actual_value = irs::get<irs::payload>(*values);
    ASSERT_NE(nullptr, actual_value);

    auto docs = prepared->execute({.segment = *sub});
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::valid(docs->value()));
    auto docs_seek = prepared->execute({.segment = *sub});
    ASSERT_FALSE(irs::doc_limits::valid(docs_seek->value()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("PHW3",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("PHW3",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_FALSE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(irs::doc_limits::eof(docs->value()));
  }

  // wildcard_filter "elephant \\_iraffe"
  {
    irs::by_phrase q;
    *q.mutable_field() = "phrase_anl";
    q.mutable_options()->push_back<irs::by_term_options>().term =
      irs::ViewCast<irs::byte_type>(std::string_view("elephant"));
    auto& wt = q.mutable_options()->push_back<irs::by_wildcard_options>();
    wt.term = irs::ViewCast<irs::byte_type>(std::string_view("\\_iraffe"));

    auto prepared = q.prepare({.index = rdr});
    auto sub = rdr.begin();
    auto column = sub->column("name");
    ASSERT_NE(nullptr, column);
    auto values = column->iterator(irs::ColumnHint::kNormal);
    ASSERT_NE(nullptr, values);
    auto* actual_value = irs::get<irs::payload>(*values);
    ASSERT_NE(nullptr, actual_value);

    auto docs = prepared->execute({.segment = *sub});
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::valid(docs->value()));
    auto docs_seek = prepared->execute({.segment = *sub});
    ASSERT_FALSE(irs::doc_limits::valid(docs_seek->value()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("PHW4",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("PHW4",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_FALSE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(irs::doc_limits::eof(docs->value()));
  }

  // wildcard_filter "elephant gira\\_fe"
  {
    irs::by_phrase q;
    *q.mutable_field() = "phrase_anl";
    q.mutable_options()->push_back<irs::by_term_options>().term =
      irs::ViewCast<irs::byte_type>(std::string_view("elephant"));
    auto& wt = q.mutable_options()->push_back<irs::by_wildcard_options>();
    wt.term = irs::ViewCast<irs::byte_type>(std::string_view("gira\\_fe"));

    auto prepared = q.prepare({.index = rdr});
    auto sub = rdr.begin();
    auto column = sub->column("name");
    ASSERT_NE(nullptr, column);
    auto values = column->iterator(irs::ColumnHint::kNormal);
    ASSERT_NE(nullptr, values);
    auto* actual_value = irs::get<irs::payload>(*values);
    ASSERT_NE(nullptr, actual_value);

    auto docs = prepared->execute({.segment = *sub});
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::valid(docs->value()));
    auto docs_seek = prepared->execute({.segment = *sub});
    ASSERT_FALSE(irs::doc_limits::valid(docs_seek->value()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("PHW5",
              irs::to_string<std::string_view>(actual_value->value.data()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(docs->value(), values->seek(docs->value()));
    ASSERT_EQ("PHW5",
              irs::to_string<std::string_view>(actual_value->value.data()));

    ASSERT_FALSE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(irs::doc_limits::eof(docs->value()));
  }
}

TEST(by_phrase_test, options) {
  irs::by_phrase_options opts;
  ASSERT_TRUE(opts.simple());
  ASSERT_TRUE(opts.empty());
  ASSERT_EQ(0, opts.size());
  ASSERT_EQ(opts.begin(), opts.end());
}

TEST(by_phrase_test, options_clear) {
  irs::by_phrase_options opts;
  ASSERT_TRUE(opts.simple());
  ASSERT_TRUE(opts.empty());
  ASSERT_EQ(0, opts.size());
  opts.push_back<irs::by_term_options>();
  ASSERT_EQ(1, opts.size());
  ASSERT_FALSE(opts.empty());
  ASSERT_TRUE(opts.simple());
  opts.push_back<irs::by_term_options>();
  ASSERT_EQ(2, opts.size());
  ASSERT_FALSE(opts.empty());
  ASSERT_TRUE(opts.simple());
  opts.push_back<irs::by_prefix_options>();
  ASSERT_EQ(3, opts.size());
  ASSERT_FALSE(opts.empty());
  ASSERT_FALSE(opts.simple());
  opts.clear();
  ASSERT_TRUE(opts.simple());
  ASSERT_TRUE(opts.empty());
  ASSERT_EQ(0, opts.size());
}

TEST(by_phrase_test, ctor) {
  irs::by_phrase q;
  ASSERT_EQ(irs::type<irs::by_phrase>::id(), q.type());
  ASSERT_EQ("", q.field());
  ASSERT_EQ(irs::by_phrase_options{}, q.options());
  ASSERT_EQ(irs::kNoBoost, q.boost());

  static_assert((irs::IndexFeatures::FREQ | irs::IndexFeatures::POS) ==
                irs::FixedPhraseQuery::kRequiredFeatures);
  static_assert((irs::IndexFeatures::FREQ | irs::IndexFeatures::POS) ==
                irs::VariadicPhraseQuery::kRequiredFeatures);
}

TEST(by_phrase_test, boost) {
  {
    irs::by_phrase q;
    *q.mutable_field() = "field";

    auto prepared = q.prepare({.index = irs::SubReader::empty()});
    ASSERT_EQ(irs::kNoBoost, prepared->boost());
  }

  // single term
  {
    irs::by_phrase q;
    *q.mutable_field() = "field";
    q.mutable_options()->push_back<irs::by_term_options>().term =
      irs::ViewCast<irs::byte_type>(std::string_view("quick"));

    auto prepared = q.prepare({.index = irs::SubReader::empty()});
    ASSERT_EQ(irs::kNoBoost, prepared->boost());
  }

  // multiple terms
  {
    irs::by_phrase q;
    *q.mutable_field() = "field";
    q.mutable_options()->push_back<irs::by_term_options>().term =
      irs::ViewCast<irs::byte_type>(std::string_view("quick"));
    q.mutable_options()->push_back<irs::by_term_options>().term =
      irs::ViewCast<irs::byte_type>(std::string_view("brown"));

    auto prepared = q.prepare({.index = irs::SubReader::empty()});
    ASSERT_EQ(irs::kNoBoost, prepared->boost());
  }

  // with boost
  {
    MaxMemoryCounter counter;
    irs::score_t boost = 1.5f;

    // no terms, return empty query
    {
      irs::by_phrase q;
      *q.mutable_field() = "field";
      q.boost(boost);

      auto prepared = q.prepare({.index = irs::SubReader::empty()});
      ASSERT_EQ(irs::kNoBoost, prepared->boost());
    }

    // single term
    {
      irs::by_phrase q;
      *q.mutable_field() = "field";
      q.mutable_options()->push_back<irs::by_term_options>().term =
        irs::ViewCast<irs::byte_type>(std::string_view("quick"));
      q.boost(boost);

      auto prepared = q.prepare({
        .index = irs::SubReader::empty(),
        .memory = counter,
      });
      ASSERT_EQ(boost, prepared->boost());
    }
    EXPECT_EQ(counter.current, 0);
    EXPECT_GT(counter.max, 0);
    counter.Reset();

    // single multiple terms
    {
      irs::by_phrase q;
      *q.mutable_field() = "field";
      q.mutable_options()->push_back<irs::by_term_options>().term =
        irs::ViewCast<irs::byte_type>(std::string_view("quick"));
      q.mutable_options()->push_back<irs::by_term_options>().term =
        irs::ViewCast<irs::byte_type>(std::string_view("brown"));
      q.boost(boost);

      auto prepared = q.prepare({.index = irs::SubReader::empty()});
      ASSERT_EQ(boost, prepared->boost());
    }

    // prefix, wildcard, levenshtein, set, range
    {
      irs::by_phrase q;
      *q.mutable_field() = "field";
      auto& pt = q.mutable_options()->push_back<irs::by_prefix_options>();
      pt.term = irs::ViewCast<irs::byte_type>(std::string_view("qui"));
      auto& wt = q.mutable_options()->push_back<irs::by_wildcard_options>();
      wt.term = irs::ViewCast<irs::byte_type>(std::string_view("qu__k"));
      auto& lt =
        q.mutable_options()->push_back<irs::by_edit_distance_options>();
      lt.max_distance = 1;
      lt.term = irs::ViewCast<irs::byte_type>(std::string_view("brwn"));
      q.boost(boost);
      auto& st = q.mutable_options()->push_back<irs::by_terms_options>();
      st.terms.emplace(irs::ViewCast<irs::byte_type>(std::string_view("fox")));
      st.terms.emplace(irs::ViewCast<irs::byte_type>(std::string_view("dob")));
      auto& rt = q.mutable_options()->push_back<irs::by_range_options>();
      rt.range.min = irs::ViewCast<irs::byte_type>(std::string_view("forward"));
      rt.range.max = irs::ViewCast<irs::byte_type>(std::string_view("forward"));
      rt.range.min_type = irs::BoundType::INCLUSIVE;
      rt.range.max_type = irs::BoundType::INCLUSIVE;

      auto prepared = q.prepare({.index = irs::SubReader::empty()});
      ASSERT_EQ(boost, prepared->boost());
    }
  }
}

TEST(by_phrase_test, push_back_insert) {
  irs::by_phrase_options q;

  // push_back
  {
    q.push_back<irs::by_term_options>().term =
      irs::ViewCast<irs::byte_type>(std::string_view("quick"));
    q.push_back<irs::by_term_options>(1).term =
      irs::ViewCast<irs::byte_type>(std::string_view("brown"));
    q.push_back<irs::by_term_options>().term =
      irs::ViewCast<irs::byte_type>(std::string_view("fox"));
    ASSERT_FALSE(q.empty());
    ASSERT_EQ(3, q.size());

    // check elements via positions
    {
      const irs::by_term_options* st1 = q.get<irs::by_term_options>(0);
      ASSERT_TRUE(st1);
      ASSERT_EQ(irs::ViewCast<irs::byte_type>(std::string_view("quick")),
                st1->term);
      const irs::by_term_options* st2 = q.get<irs::by_term_options>(2);
      ASSERT_TRUE(st2);
      ASSERT_EQ(irs::ViewCast<irs::byte_type>(std::string_view("brown")),
                st2->term);
      const irs::by_term_options* st3 = q.get<irs::by_term_options>(3);
      ASSERT_TRUE(st3);
      ASSERT_EQ(irs::ViewCast<irs::byte_type>(std::string_view("fox")),
                st3->term);
    }

    // push term
    {
      irs::by_term_options st1;
      st1.term = irs::ViewCast<irs::byte_type>(std::string_view("squirrel"));
      q.push_back(st1, 0);
      const irs::by_term_options* st2 = q.get<irs::by_term_options>(4);
      ASSERT_TRUE(st2);
      ASSERT_EQ(st1, *st2);

      irs::by_prefix_options pt1;
      pt1.term = irs::ViewCast<irs::byte_type>(std::string_view("cat"));
      q.push_back(pt1, 0);
      const irs::by_prefix_options* pt2 = q.get<irs::by_prefix_options>(5);
      ASSERT_TRUE(pt2);
      ASSERT_EQ(pt1, *pt2);

      irs::by_wildcard_options wt1;
      wt1.term = irs::ViewCast<irs::byte_type>(std::string_view("dog"));
      q.push_back(wt1, 0);
      const irs::by_wildcard_options* wt2 = q.get<irs::by_wildcard_options>(6);
      ASSERT_TRUE(wt2);
      ASSERT_EQ(wt1, *wt2);

      irs::by_edit_distance_options lt1;
      lt1.term = irs::ViewCast<irs::byte_type>(std::string_view("whale"));
      q.push_back(lt1, 0);
      const irs::by_edit_distance_options* lt2 =
        q.get<irs::by_edit_distance_options>(7);
      ASSERT_TRUE(lt2);
      ASSERT_EQ(lt1, *lt2);

      irs::by_terms_options ct1;
      ct1.terms.emplace(
        irs::ViewCast<irs::byte_type>(std::string_view("bird")));
      q.push_back(ct1, 0);
      const irs::by_terms_options* ct2 = q.get<irs::by_terms_options>(8);
      ASSERT_TRUE(ct2);
      ASSERT_EQ(ct1, *ct2);

      irs::by_range_options rt1;
      rt1.range.min =
        irs::ViewCast<irs::byte_type>(std::string_view("elephant"));
      rt1.range.max =
        irs::ViewCast<irs::byte_type>(std::string_view("elephant"));
      rt1.range.min_type = irs::BoundType::INCLUSIVE;
      rt1.range.max_type = irs::BoundType::INCLUSIVE;
      q.push_back(rt1, 0);
      const irs::by_range_options* rt2 = q.get<irs::by_range_options>(9);
      ASSERT_TRUE(rt2);
      ASSERT_EQ(rt1, *rt2);
    }
    ASSERT_EQ(9, q.size());
  }

  // insert + move
  {
    {
      irs::by_term_options st;
      st.term = irs::ViewCast<irs::byte_type>(std::string_view("jumps"));

      q.insert(std::move(st), 3);
      const irs::by_term_options* st1 = q.get<irs::by_term_options>(3);
      ASSERT_TRUE(st1);
      ASSERT_EQ(irs::ViewCast<irs::byte_type>(std::string_view("jumps")),
                st1->term);
      ASSERT_EQ(9, q.size());
    }

    {
      irs::by_term_options st;
      st.term = irs::ViewCast<irs::byte_type>(std::string_view("lazy"));

      q.insert(std::move(st), 9);
      const irs::by_term_options* st2 = q.get<irs::by_term_options>(9);
      ASSERT_TRUE(st2);
      ASSERT_EQ(irs::ViewCast<irs::byte_type>(std::string_view("lazy")),
                st2->term);
      ASSERT_EQ(9, q.size());
    }

    {
      irs::by_term_options st;
      st.term = irs::ViewCast<irs::byte_type>(std::string_view("dog"));

      q.insert(std::move(st), 28);
      const irs::by_term_options* st3 = q.get<irs::by_term_options>(28);
      ASSERT_TRUE(st3);
      ASSERT_EQ(irs::ViewCast<irs::byte_type>(std::string_view("dog")),
                st3->term);
      ASSERT_EQ(10, q.size());
    }

    {
      irs::by_term_options st1;
      st1.term = irs::ViewCast<irs::byte_type>(std::string_view("squirrel"));
      q.insert(st1, 5);
      const irs::by_term_options* st2 = q.get<irs::by_term_options>(5);
      ASSERT_TRUE(st2);
      ASSERT_EQ(st1, *st2);
      ASSERT_EQ(10, q.size());

      irs::by_prefix_options pt1;
      pt1.term = irs::ViewCast<irs::byte_type>(std::string_view("cat"));
      q.insert(pt1, 7);
      const irs::by_prefix_options* pt2 = q.get<irs::by_prefix_options>(7);
      ASSERT_TRUE(pt2);
      ASSERT_EQ(pt1, *pt2);
      ASSERT_EQ(10, q.size());

      irs::by_wildcard_options wt1;
      wt1.term = irs::ViewCast<irs::byte_type>(std::string_view("dog"));
      q.insert(wt1, 9);
      const irs::by_wildcard_options* wt2 = q.get<irs::by_wildcard_options>(9);
      ASSERT_TRUE(wt2);
      ASSERT_EQ(wt1, *wt2);
      ASSERT_EQ(10, q.size());

      irs::by_edit_distance_options lt1;
      lt1.term = irs::ViewCast<irs::byte_type>(std::string_view("whale"));
      q.insert(lt1, 29);
      const irs::by_edit_distance_options* lt2 =
        q.get<irs::by_edit_distance_options>(29);
      ASSERT_TRUE(lt2);
      ASSERT_EQ(lt1, *lt2);
      ASSERT_EQ(11, q.size());

      irs::by_terms_options ct1;
      ct1.terms.emplace(
        irs::ViewCast<irs::byte_type>(std::string_view("bird")));
      q.insert(ct1, 29);
      const irs::by_terms_options* ct2 = q.get<irs::by_terms_options>(29);
      ASSERT_TRUE(ct2);
      ASSERT_EQ(ct1, *ct2);
      ASSERT_EQ(11, q.size());

      irs::by_range_options rt1;
      rt1.range.min =
        irs::ViewCast<irs::byte_type>(std::string_view("elephant"));
      rt1.range.max =
        irs::ViewCast<irs::byte_type>(std::string_view("elephant"));
      rt1.range.min_type = irs::BoundType::INCLUSIVE;
      rt1.range.max_type = irs::BoundType::INCLUSIVE;
      q.insert(rt1, 10);
      const irs::by_range_options* rt2 = q.get<irs::by_range_options>(10);
      ASSERT_TRUE(rt2);
      ASSERT_EQ(rt1, *rt2);
      ASSERT_EQ(12, q.size());
    }
  }
}

TEST(by_phrase_test, equal) {
  ASSERT_EQ(irs::by_phrase(), irs::by_phrase());

  {
    irs::by_phrase q0;
    *q0.mutable_field() = "name";
    q0.mutable_options()->push_back<irs::by_term_options>().term =
      irs::ViewCast<irs::byte_type>(std::string_view("quick"));
    q0.mutable_options()->push_back<irs::by_term_options>().term =
      irs::ViewCast<irs::byte_type>(std::string_view("brown"));

    irs::by_phrase q1;
    *q1.mutable_field() = "name";
    q1.mutable_options()->push_back<irs::by_term_options>().term =
      irs::ViewCast<irs::byte_type>(std::string_view("quick"));
    q1.mutable_options()->push_back<irs::by_term_options>().term =
      irs::ViewCast<irs::byte_type>(std::string_view("brown"));
    ASSERT_EQ(q0, q1);
  }

  {
    irs::by_phrase q0;
    {
      *q0.mutable_field() = "name";
      auto& pt1 = q0.mutable_options()->push_back<irs::by_prefix_options>();
      pt1.term = irs::ViewCast<irs::byte_type>(std::string_view("qui"));
      auto& ct1 = q0.mutable_options()->push_back<irs::by_terms_options>();
      ct1.terms.emplace(
        irs::ViewCast<irs::byte_type>(std::string_view("light")));
      ct1.terms.emplace(
        irs::ViewCast<irs::byte_type>(std::string_view("dark")));
      auto& wt1 = q0.mutable_options()->push_back<irs::by_wildcard_options>();
      wt1.term = irs::ViewCast<irs::byte_type>(std::string_view("br_wn"));
      auto& lt1 =
        q0.mutable_options()->push_back<irs::by_edit_distance_options>();
      lt1.max_distance = 2;
      lt1.term = irs::ViewCast<irs::byte_type>(std::string_view("fo"));
      auto& rt1 = q0.mutable_options()->push_back<irs::by_range_options>();
      rt1.range.min =
        irs::ViewCast<irs::byte_type>(std::string_view("elephant"));
      rt1.range.max =
        irs::ViewCast<irs::byte_type>(std::string_view("elephant"));
      rt1.range.min_type = irs::BoundType::INCLUSIVE;
      rt1.range.max_type = irs::BoundType::INCLUSIVE;
    }

    irs::by_phrase q1;
    {
      *q1.mutable_field() = "name";
      auto& pt1 = q1.mutable_options()->push_back<irs::by_prefix_options>();
      pt1.term = irs::ViewCast<irs::byte_type>(std::string_view("qui"));
      auto& ct1 = q1.mutable_options()->push_back<irs::by_terms_options>();
      ct1.terms.emplace(
        irs::ViewCast<irs::byte_type>(std::string_view("light")));
      ct1.terms.emplace(
        irs::ViewCast<irs::byte_type>(std::string_view("dark")));
      auto& wt1 = q1.mutable_options()->push_back<irs::by_wildcard_options>();
      wt1.term = irs::ViewCast<irs::byte_type>(std::string_view("br_wn"));
      auto& lt1 =
        q1.mutable_options()->push_back<irs::by_edit_distance_options>();
      lt1.max_distance = 2;
      lt1.term = irs::ViewCast<irs::byte_type>(std::string_view("fo"));
      auto& rt1 = q1.mutable_options()->push_back<irs::by_range_options>();
      rt1.range.min =
        irs::ViewCast<irs::byte_type>(std::string_view("elephant"));
      rt1.range.max =
        irs::ViewCast<irs::byte_type>(std::string_view("elephant"));
      rt1.range.min_type = irs::BoundType::INCLUSIVE;
      rt1.range.max_type = irs::BoundType::INCLUSIVE;
    }

    ASSERT_EQ(q0, q1);
  }

  {
    irs::by_phrase q0;
    *q0.mutable_field() = "name";
    q0.mutable_options()->push_back<irs::by_term_options>().term =
      irs::ViewCast<irs::byte_type>(std::string_view("quick"));
    q0.mutable_options()->push_back<irs::by_term_options>().term =
      irs::ViewCast<irs::byte_type>(std::string_view("squirrel"));

    irs::by_phrase q1;
    *q1.mutable_field() = "name";
    q1.mutable_options()->push_back<irs::by_term_options>().term =
      irs::ViewCast<irs::byte_type>(std::string_view("quick"));
    q1.mutable_options()->push_back<irs::by_term_options>().term =
      irs::ViewCast<irs::byte_type>(std::string_view("brown"));
    ASSERT_NE(q0, q1);
  }

  {
    irs::by_phrase q0;
    *q0.mutable_field() = "name1";
    q0.mutable_options()->push_back<irs::by_term_options>().term =
      irs::ViewCast<irs::byte_type>(std::string_view("quick"));
    q0.mutable_options()->push_back<irs::by_term_options>().term =
      irs::ViewCast<irs::byte_type>(std::string_view("brown"));

    irs::by_phrase q1;
    *q1.mutable_field() = "name";
    q1.mutable_options()->push_back<irs::by_term_options>().term =
      irs::ViewCast<irs::byte_type>(std::string_view("quick"));
    q1.mutable_options()->push_back<irs::by_term_options>().term =
      irs::ViewCast<irs::byte_type>(std::string_view("brown"));
    ASSERT_NE(q0, q1);
  }

  {
    irs::by_phrase q0;
    *q0.mutable_field() = "name";
    q0.mutable_options()->push_back<irs::by_term_options>().term =
      irs::ViewCast<irs::byte_type>(std::string_view("quick"));

    irs::by_phrase q1;
    *q1.mutable_field() = "name";
    q1.mutable_options()->push_back<irs::by_term_options>().term =
      irs::ViewCast<irs::byte_type>(std::string_view("quick"));
    q1.mutable_options()->push_back<irs::by_term_options>().term =
      irs::ViewCast<irs::byte_type>(std::string_view("brown"));
    ASSERT_NE(q0, q1);
  }

  {
    irs::by_phrase q0;
    {
      *q0.mutable_field() = "name";
      auto& pt1 = q0.mutable_options()->push_back<irs::by_prefix_options>();
      pt1.term = irs::ViewCast<irs::byte_type>(std::string_view("quil"));
      auto& ct1 = q0.mutable_options()->push_back<irs::by_terms_options>();
      ct1.terms.emplace(
        irs::ViewCast<irs::byte_type>(std::string_view("light")));
      ct1.terms.emplace(
        irs::ViewCast<irs::byte_type>(std::string_view("dark")));
      auto& wt1 = q0.mutable_options()->push_back<irs::by_wildcard_options>();
      wt1.term = irs::ViewCast<irs::byte_type>(std::string_view("br_wn"));
      auto& lt1 =
        q0.mutable_options()->push_back<irs::by_edit_distance_options>();
      lt1.max_distance = 2;
      lt1.term = irs::ViewCast<irs::byte_type>(std::string_view("fo"));
      auto& rt1 = q0.mutable_options()->push_back<irs::by_range_options>();
      rt1.range.min =
        irs::ViewCast<irs::byte_type>(std::string_view("elephant"));
      rt1.range.max =
        irs::ViewCast<irs::byte_type>(std::string_view("elephant"));
      rt1.range.min_type = irs::BoundType::INCLUSIVE;
      rt1.range.max_type = irs::BoundType::INCLUSIVE;
    }

    irs::by_phrase q1;
    {
      *q1.mutable_field() = "name";
      auto& pt1 = q1.mutable_options()->push_back<irs::by_prefix_options>();
      pt1.term = irs::ViewCast<irs::byte_type>(std::string_view("qui"));
      auto& ct1 = q1.mutable_options()->push_back<irs::by_terms_options>();
      ct1.terms.emplace(
        irs::ViewCast<irs::byte_type>(std::string_view("light")));
      ct1.terms.emplace(
        irs::ViewCast<irs::byte_type>(std::string_view("dark")));
      auto& wt1 = q1.mutable_options()->push_back<irs::by_wildcard_options>();
      wt1.term = irs::ViewCast<irs::byte_type>(std::string_view("br_wn"));
      auto& lt1 =
        q1.mutable_options()->push_back<irs::by_edit_distance_options>();
      lt1.max_distance = 2;
      lt1.term = irs::ViewCast<irs::byte_type>(std::string_view("fo"));
      auto& rt1 = q1.mutable_options()->push_back<irs::by_range_options>();
      rt1.range.min =
        irs::ViewCast<irs::byte_type>(std::string_view("elephant"));
      rt1.range.max =
        irs::ViewCast<irs::byte_type>(std::string_view("elephant"));
      rt1.range.min_type = irs::BoundType::INCLUSIVE;
      rt1.range.max_type = irs::BoundType::INCLUSIVE;
    }

    ASSERT_NE(q0, q1);
  }
}

TEST(by_phrase_test, copy_move) {
  {
    irs::by_term_options st;
    st.term = irs::ViewCast<irs::byte_type>(std::string_view("very"));
    irs::by_prefix_options pt;
    pt.term = irs::ViewCast<irs::byte_type>(std::string_view("qui"));
    irs::by_terms_options ct;
    ct.terms.emplace(irs::ViewCast<irs::byte_type>(std::string_view("light")));
    ct.terms.emplace(irs::ViewCast<irs::byte_type>(std::string_view("dark")));
    irs::by_wildcard_options wt;
    wt.term = irs::ViewCast<irs::byte_type>(std::string_view("br_wn"));
    irs::by_edit_distance_options lt;
    lt.max_distance = 2;
    lt.term = irs::ViewCast<irs::byte_type>(std::string_view("fo"));
    irs::by_range_options rt;
    rt.range.min = irs::ViewCast<irs::byte_type>(std::string_view("elephant"));
    rt.range.max = irs::ViewCast<irs::byte_type>(std::string_view("elephant"));
    rt.range.min_type = irs::BoundType::INCLUSIVE;
    rt.range.max_type = irs::BoundType::INCLUSIVE;

    irs::by_phrase q0;
    *q0.mutable_field() = "name";
    q0.mutable_options()->push_back(st);
    q0.mutable_options()->push_back(pt);
    q0.mutable_options()->push_back(ct);
    q0.mutable_options()->push_back(wt);
    q0.mutable_options()->push_back(lt);
    q0.mutable_options()->push_back(rt);
    q0.mutable_options()->push_back(std::move(st));
    q0.mutable_options()->push_back(std::move(pt));
    q0.mutable_options()->push_back(std::move(ct));
    q0.mutable_options()->push_back(std::move(wt));
    q0.mutable_options()->push_back(std::move(lt));
    q0.mutable_options()->push_back(std::move(rt));

    irs::by_phrase q1 = q0;
    ASSERT_EQ(q0, q1);
    irs::by_phrase q2 = q0;
    irs::by_phrase q3 = std::move(q2);
    ASSERT_EQ(q0, q3);
  }
}

static constexpr auto kTestDirs = tests::getDirectories<tests::kTypesDefault>();

INSTANTIATE_TEST_SUITE_P(
  phrase_filter_test, phrase_filter_test_case,
  ::testing::Combine(::testing::ValuesIn(kTestDirs),
                     ::testing::Values(tests::format_info{"1_0"},
                                       tests::format_info{"1_3", "1_0"})),
  phrase_filter_test_case::to_string);
