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
#include "filter_test_case_base.hpp"
#include "analysis/token_attributes.hpp"
#include "search/phrase_filter.hpp"
#ifndef IRESEARCH_DLL
#include "search/multiterm_query.hpp"
#include "search/term_query.hpp"
#endif

namespace tests {

void analyzed_json_field_factory(
    tests::document& doc,
    const std::string& name,
    const tests::json_doc_generator::json_value& data) {
  typedef templates::text_field<std::string> text_field;
 
  class string_field : public templates::string_field {
   public:
    string_field(const std::string& name, const irs::string_ref& value)
      : templates::string_field(name, value) {
    }

    const irs::flags& features() const {
      static irs::flags features{ irs::type<irs::frequency>::get() };
      return features;
    }
  }; // string_field

  if (data.is_string()) {
    // analyzed field
    doc.indexed.push_back(std::make_shared<text_field>(
      std::string(name.c_str()) + "_anl",
      data.str
    ));

    // not analyzed field
    doc.insert(std::make_shared<string_field>(
      name,
      data.str
    ));
  }
}

}

class phrase_filter_test_case : public tests::filter_test_case_base { };

TEST_P(phrase_filter_test_case, sequential_one_term) {
  // add segment
  {
    tests::json_doc_generator gen(
      resource("phrase_sequential.json"),
      &tests::analyzed_json_field_factory);
    add_segment(gen);
  }

  // read segment
  auto rdr = open_reader();

  // empty field
  {
    irs::by_phrase q;

    auto prepared = q.prepare(rdr);
    auto sub = rdr.begin();

    auto docs = prepared->execute(*sub);
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

    auto prepared = q.prepare(rdr);
    auto sub = rdr.begin();

    auto docs = prepared->execute(*sub);
    ASSERT_FALSE(irs::get<irs::frequency>(*docs));
    ASSERT_FALSE(irs::get<irs::filter_boost>(*docs));
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(irs::doc_limits::eof(docs->value()));
  }

  // equals to term_filter "fox"
  {
    irs::bytes_ref actual_value;

    irs::by_phrase q;
    *q.mutable_field() = "phrase_anl";
    q.mutable_options()->push_back<irs::by_term_options>().term = irs::ref_cast<irs::byte_type>(irs::string_ref("fox"));

    auto prepared = q.prepare(rdr);
    auto sub = rdr.begin();
    auto column = sub->column_reader("name");
    ASSERT_NE(nullptr, column);
    auto values = column->values();

    auto docs = prepared->execute(*sub);
    ASSERT_FALSE(irs::get<irs::frequency>(*docs));
    ASSERT_FALSE(irs::get<irs::filter_boost>(*docs));
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::valid(docs->value()));
    auto docs_seek = prepared->execute(*sub);
    ASSERT_FALSE(irs::doc_limits::valid(docs_seek->value()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("A", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("A", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("G", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("G", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("I", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("I", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("K", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("K", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("L", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("L", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("N", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("N", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("S", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("S", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("T", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("T", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("V", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("V", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_FALSE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(irs::doc_limits::eof(docs->value()));
  }

  // prefix_filter "fo*"
  {
    irs::bytes_ref actual_value;

    irs::by_phrase q;
    *q.mutable_field() = "phrase_anl";
    q.mutable_options()->push_back<irs::by_prefix_options>().term = irs::ref_cast<irs::byte_type>(irs::string_ref("fo"));

    auto prepared = q.prepare(rdr);
    auto sub = rdr.begin();
    auto column = sub->column_reader("name");
    ASSERT_NE(nullptr, column);
    auto values = column->values();

    auto docs = prepared->execute(*sub);
    ASSERT_FALSE(irs::get<irs::filter_boost>(*docs));
    ASSERT_FALSE(irs::get<irs::frequency>(*docs));
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::valid(docs->value()));
    auto docs_seek = prepared->execute(*sub);
    ASSERT_FALSE(irs::doc_limits::valid(docs_seek->value()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("A", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("A", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("D", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("D", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("G", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("G", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("H", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("H", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("I", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("I", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("K", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("K", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("L", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("L", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("N", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("N", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("S", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("S", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("T", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("T", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("U", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("U", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("V", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("V", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("W", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("W", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("X", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("X", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("Y", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("Y", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_FALSE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(irs::doc_limits::eof(docs->value()));
  }

  // wildcard_filter "fo%"
  {
    irs::bytes_ref actual_value;

    irs::by_phrase q;
    *q.mutable_field() = "phrase_anl";
    q.mutable_options()->push_back<irs::by_wildcard_options>().term = irs::ref_cast<irs::byte_type>(irs::string_ref("fo%"));

    auto prepared = q.prepare(rdr);
    auto sub = rdr.begin();
    auto column = sub->column_reader("name");
    ASSERT_NE(nullptr, column);
    auto values = column->values();

    auto docs = prepared->execute(*sub);
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_FALSE(irs::get<irs::filter_boost>(*docs));
    ASSERT_FALSE(irs::get<irs::frequency>(*docs));
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::valid(docs->value()));
    auto docs_seek = prepared->execute(*sub);
    ASSERT_FALSE(irs::doc_limits::valid(docs_seek->value()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("A", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("A", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("D", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("D", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("G", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("G", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("H", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("H", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("I", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("I", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("K", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("K", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("L", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("L", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("N", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("N", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("S", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("S", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("T", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("T", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("U", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("U", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("V", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("V", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("W", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("W", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("X", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("X", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("Y", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("Y", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_FALSE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(irs::doc_limits::eof(docs->value()));
  }

  // wildcard_filter "%ox"
  {
    irs::bytes_ref actual_value;

    irs::by_phrase q;
    *q.mutable_field() = "phrase_anl";
    q.mutable_options()->push_back<irs::by_wildcard_options>().term = irs::ref_cast<irs::byte_type>(irs::string_ref("%ox"));

    auto prepared = q.prepare(rdr);
    auto sub = rdr.begin();
    auto column = sub->column_reader("name");
    ASSERT_NE(nullptr, column);
    auto values = column->values();

    auto docs = prepared->execute(*sub);
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_FALSE(irs::get<irs::filter_boost>(*docs));
    ASSERT_FALSE(irs::get<irs::frequency>(*docs));
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::valid(docs->value()));
    auto docs_seek = prepared->execute(*sub);
    ASSERT_FALSE(irs::doc_limits::valid(docs_seek->value()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("A", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("A", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("G", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("G", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("I", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("I", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("K", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("K", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("L", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("L", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("N", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("N", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("S", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("S", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("T", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("T", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("V", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("V", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_FALSE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(irs::doc_limits::eof(docs->value()));
  }

  // wildcard_filter "f%x"
  {
    irs::bytes_ref actual_value;

    irs::by_phrase q;
    *q.mutable_field() = "phrase_anl";
    q.mutable_options()->push_back<irs::by_wildcard_options>().term = irs::ref_cast<irs::byte_type>(irs::string_ref("f%x"));

    auto prepared = q.prepare(rdr);
    auto sub = rdr.begin();
    auto column = sub->column_reader("name");
    ASSERT_NE(nullptr, column);
    auto values = column->values();

    auto docs = prepared->execute(*sub);
    ASSERT_FALSE(irs::get<irs::filter_boost>(*docs));
    ASSERT_FALSE(irs::get<irs::frequency>(*docs));
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::valid(docs->value()));
    auto docs_seek = prepared->execute(*sub);
    ASSERT_FALSE(irs::doc_limits::valid(docs_seek->value()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("A", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("A", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("G", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("G", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("I", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("I", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("K", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("K", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("L", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("L", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("N", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("N", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("S", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("S", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("T", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("T", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("V", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("V", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_FALSE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(irs::doc_limits::eof(docs->value()));
  }

  // wildcard_filter "_ox"
  {
    irs::bytes_ref actual_value;

    irs::by_phrase q;
    *q.mutable_field() = "phrase_anl";
    q.mutable_options()->push_back<irs::by_wildcard_options>().term = irs::ref_cast<irs::byte_type>(irs::string_ref("_ox"));

    auto prepared = q.prepare(rdr);
    auto sub = rdr.begin();
    auto column = sub->column_reader("name");
    ASSERT_NE(nullptr, column);
    auto values = column->values();

    auto docs = prepared->execute(*sub);
    ASSERT_FALSE(irs::get<irs::filter_boost>(*docs));
    ASSERT_FALSE(irs::get<irs::frequency>(*docs));
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::valid(docs->value()));
    auto docs_seek = prepared->execute(*sub);
    ASSERT_FALSE(irs::doc_limits::valid(docs_seek->value()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("A", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("A", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("G", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("G", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("I", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("I", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("K", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("K", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("L", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("L", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("N", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("N", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("S", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("S", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("T", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("T", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("V", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("V", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_FALSE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(irs::doc_limits::eof(docs->value()));
  }

  // wildcard_filter "f_x"
  {
    irs::bytes_ref actual_value;

    irs::by_phrase q;
    *q.mutable_field() = "phrase_anl";
    q.mutable_options()->push_back<irs::by_wildcard_options>().term = irs::ref_cast<irs::byte_type>(irs::string_ref("f_x"));

    auto prepared = q.prepare(rdr);
    auto sub = rdr.begin();
    auto column = sub->column_reader("name");
    ASSERT_NE(nullptr, column);
    auto values = column->values();

    auto docs = prepared->execute(*sub);
    ASSERT_FALSE(irs::get<irs::filter_boost>(*docs));
    ASSERT_FALSE(irs::get<irs::frequency>(*docs));
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::valid(docs->value()));
    auto docs_seek = prepared->execute(*sub);
    ASSERT_FALSE(irs::doc_limits::valid(docs_seek->value()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("A", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("A", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("G", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("G", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("I", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("I", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("K", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("K", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("L", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("L", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("N", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("N", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("S", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("S", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("T", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("T", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("V", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("V", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_FALSE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(irs::doc_limits::eof(docs->value()));
  }

  // wildcard_filter "fo_"
  {
    irs::bytes_ref actual_value;

    irs::by_phrase q;
    *q.mutable_field() = "phrase_anl";
    q.mutable_options()->push_back<irs::by_wildcard_options>().term = irs::ref_cast<irs::byte_type>(irs::string_ref("fo_"));

    auto prepared = q.prepare(rdr);
    auto sub = rdr.begin();
    auto column = sub->column_reader("name");
    ASSERT_NE(nullptr, column);
    auto values = column->values();

    auto docs = prepared->execute(*sub);
    ASSERT_FALSE(irs::get<irs::filter_boost>(*docs));
    ASSERT_FALSE(irs::get<irs::frequency>(*docs));
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::valid(docs->value()));
    auto docs_seek = prepared->execute(*sub);
    ASSERT_FALSE(irs::doc_limits::valid(docs_seek->value()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("A", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("A", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("G", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("G", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("I", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("I", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("K", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("K", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("L", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("L", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("N", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("N", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("S", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("S", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("T", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("T", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("V", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("V", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_FALSE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(irs::doc_limits::eof(docs->value()));
  }

  // wildcard_filter "fox"
  {
    irs::bytes_ref actual_value;

    irs::by_phrase q;
    *q.mutable_field() = "phrase_anl";
    q.mutable_options()->push_back<irs::by_wildcard_options>().term = irs::ref_cast<irs::byte_type>(irs::string_ref("fox"));

    auto prepared = q.prepare(rdr);
    auto sub = rdr.begin();
    auto column = sub->column_reader("name");
    ASSERT_NE(nullptr, column);
    auto values = column->values();

    auto docs = prepared->execute(*sub);
    ASSERT_FALSE(irs::get<irs::filter_boost>(*docs));
    ASSERT_FALSE(irs::get<irs::frequency>(*docs));
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::valid(docs->value()));
    auto docs_seek = prepared->execute(*sub);
    ASSERT_FALSE(irs::doc_limits::valid(docs_seek->value()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("A", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("A", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("G", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("G", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("I", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("I", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("K", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("K", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("L", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("L", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("N", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("N", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("S", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("S", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("T", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("T", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("V", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("V", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_FALSE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(irs::doc_limits::eof(docs->value()));
  }

  // levenshtein_filter "fox" max_distance = 0
  {
    irs::bytes_ref actual_value;

    irs::by_phrase q;
    *q.mutable_field() = "phrase_anl";
    auto& lt = q.mutable_options()->push_back<irs::by_edit_distance_filter_options>();
    lt.max_distance = 0;
    lt.term = irs::ref_cast<irs::byte_type>(irs::string_ref("fox"));

    auto prepared = q.prepare(rdr);
    auto sub = rdr.begin();
    auto column = sub->column_reader("name");
    ASSERT_NE(nullptr, column);
    auto values = column->values();

    auto docs = prepared->execute(*sub);
    ASSERT_FALSE(irs::get<irs::filter_boost>(*docs));
    ASSERT_FALSE(irs::get<irs::frequency>(*docs));
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::valid(docs->value()));
    auto docs_seek = prepared->execute(*sub);
    ASSERT_FALSE(irs::doc_limits::valid(docs_seek->value()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("A", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("A", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("G", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("G", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("I", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("I", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("K", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("K", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("L", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("L", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("N", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("N", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("S", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("S", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("T", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("T", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("V", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("V", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_FALSE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(irs::doc_limits::eof(docs->value()));
  }

  // levenshtein_filter "fol"
  {
    irs::bytes_ref actual_value;

    irs::by_phrase q;
    *q.mutable_field() = "phrase_anl";
    auto& lt = q.mutable_options()->push_back<irs::by_edit_distance_filter_options>();
    lt.max_distance = 1;
    lt.term = irs::ref_cast<irs::byte_type>(irs::string_ref("fol"));

    auto prepared = q.prepare(rdr);
    auto sub = rdr.begin();
    auto column = sub->column_reader("name");
    ASSERT_NE(nullptr, column);
    auto values = column->values();

    auto docs = prepared->execute(*sub);
    ASSERT_FALSE(irs::get<irs::filter_boost>(*docs));
    ASSERT_FALSE(irs::get<irs::frequency>(*docs));
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::valid(docs->value()));
    auto docs_seek = prepared->execute(*sub);
    ASSERT_FALSE(irs::doc_limits::valid(docs_seek->value()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("A", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("A", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("G", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("G", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("I", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("I", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("K", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("K", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("L", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("L", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("N", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("N", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("S", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("S", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("T", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("T", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("V", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("V", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_FALSE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(irs::doc_limits::eof(docs->value()));
  }

  // by_terms_options "fox"
  {
    irs::bytes_ref actual_value;

    irs::by_phrase q;
    *q.mutable_field() = "phrase_anl";
    auto& st = q.mutable_options()->push_back<irs::by_terms_options>();
    st.terms.emplace(irs::ref_cast<irs::byte_type>(irs::string_ref("fox")));

    auto prepared = q.prepare(rdr);
    auto sub = rdr.begin();
    auto column = sub->column_reader("name");
    ASSERT_NE(nullptr, column);
    auto values = column->values();

    auto docs = prepared->execute(*sub);
    ASSERT_FALSE(irs::get<irs::filter_boost>(*docs));
    ASSERT_FALSE(irs::get<irs::frequency>(*docs));
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::valid(docs->value()));
    auto docs_seek = prepared->execute(*sub);
    ASSERT_FALSE(irs::doc_limits::valid(docs_seek->value()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("A", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("A", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("G", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("G", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("I", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("I", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("K", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("K", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("L", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("L", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("N", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("N", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("S", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("S", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("T", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("T", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("V", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("V", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_FALSE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(irs::doc_limits::eof(docs->value()));
  }

  // by_terms_options "fox|that"
  {
    irs::bytes_ref actual_value;

    irs::by_phrase q;
    *q.mutable_field() = "phrase_anl";
    auto& st = q.mutable_options()->push_back<irs::by_terms_options>();
    st.terms.emplace(irs::ref_cast<irs::byte_type>(irs::string_ref("fox")));
    st.terms.emplace(irs::ref_cast<irs::byte_type>(irs::string_ref("that")));

    auto prepared = q.prepare(rdr);
    auto sub = rdr.begin();
    auto column = sub->column_reader("name");
    ASSERT_NE(nullptr, column);
    auto values = column->values();

    auto docs = prepared->execute(*sub);
    ASSERT_FALSE(irs::get<irs::filter_boost>(*docs));
    ASSERT_FALSE(irs::get<irs::frequency>(*docs));
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::valid(docs->value()));
    auto docs_seek = prepared->execute(*sub);
    ASSERT_FALSE(irs::doc_limits::valid(docs_seek->value()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("A", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("A", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("B", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("B", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("D", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("D", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("G", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("G", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("I", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("I", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("K", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("K", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("L", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("L", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("N", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("N", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("S", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("S", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("T", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("T", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("V", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("V", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_FALSE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(irs::doc_limits::eof(docs->value()));
  }

  // by_range_filter_options "[x0, x0]"
  {
    irs::bytes_ref actual_value;

    irs::by_phrase q;
    *q.mutable_field() = "phrase_anl";
    auto& rt = q.mutable_options()->push_back<irs::by_range_options>();
    rt.range.min = irs::ref_cast<irs::byte_type>(irs::string_ref("x0"));
    rt.range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("x0"));
    rt.range.min_type = irs::BoundType::INCLUSIVE;
    rt.range.max_type = irs::BoundType::INCLUSIVE;

    auto prepared = q.prepare(rdr);
    auto sub = rdr.begin();
    auto column = sub->column_reader("name");
    ASSERT_NE(nullptr, column);
    auto values = column->values();

    auto docs = prepared->execute(*sub);
    ASSERT_FALSE(irs::get<irs::filter_boost>(*docs));
    ASSERT_FALSE(irs::get<irs::frequency>(*docs));
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::valid(docs->value()));
    auto docs_seek = prepared->execute(*sub);
    ASSERT_FALSE(irs::doc_limits::valid(docs_seek->value()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("X0", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("X0", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("X4", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("X4", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_FALSE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(irs::doc_limits::eof(docs->value()));
  }

  // by_range_options "(x0, x0]"
  {
    irs::by_phrase q;
    *q.mutable_field() = "phrase_anl";
    auto& rt = q.mutable_options()->push_back<irs::by_range_options>();
    rt.range.min = irs::ref_cast<irs::byte_type>(irs::string_ref("x0"));
    rt.range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("x0"));
    rt.range.min_type = irs::BoundType::EXCLUSIVE;
    rt.range.max_type = irs::BoundType::INCLUSIVE;

    auto prepared = q.prepare(rdr);
    auto sub = rdr.begin();
    auto column = sub->column_reader("name");
    ASSERT_NE(nullptr, column);
    auto values = column->values();

    auto docs = prepared->execute(*sub);
    ASSERT_FALSE(irs::get<irs::filter_boost>(*docs));
    ASSERT_FALSE(irs::get<irs::frequency>(*docs));
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(irs::doc_limits::eof(docs->value()));
    auto docs_seek = prepared->execute(*sub);
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
    rt.range.min = irs::ref_cast<irs::byte_type>(irs::string_ref("x0"));
    rt.range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("x0"));
    rt.range.min_type = irs::BoundType::INCLUSIVE;
    rt.range.max_type = irs::BoundType::EXCLUSIVE;

    auto prepared = q.prepare(rdr);
    auto sub = rdr.begin();
    auto column = sub->column_reader("name");
    ASSERT_NE(nullptr, column);
    auto values = column->values();

    auto docs = prepared->execute(*sub);
    ASSERT_FALSE(irs::get<irs::filter_boost>(*docs));
    ASSERT_FALSE(irs::get<irs::frequency>(*docs));
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(irs::doc_limits::eof(docs->value()));
    auto docs_seek = prepared->execute(*sub);
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
    rt.range.min = irs::ref_cast<irs::byte_type>(irs::string_ref("x0"));
    rt.range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("x0"));
    rt.range.min_type = irs::BoundType::EXCLUSIVE;
    rt.range.max_type = irs::BoundType::EXCLUSIVE;

    auto prepared = q.prepare(rdr);
    auto sub = rdr.begin();
    auto column = sub->column_reader("name");
    ASSERT_NE(nullptr, column);
    auto values = column->values();

    auto docs = prepared->execute(*sub);
    ASSERT_FALSE(irs::get<irs::filter_boost>(*docs));
    ASSERT_FALSE(irs::get<irs::frequency>(*docs));
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(irs::doc_limits::eof(docs->value()));
    auto docs_seek = prepared->execute(*sub);
    ASSERT_TRUE(irs::doc_limits::eof(docs_seek->value()));

    ASSERT_FALSE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(irs::doc_limits::eof(docs->value()));
  }

  // by_range_filter_options "[x0, x2]"
  {
    irs::bytes_ref actual_value;

    irs::by_phrase q;
    *q.mutable_field() = "phrase_anl";
    auto& rt = q.mutable_options()->push_back<irs::by_range_options>();
    rt.range.min = irs::ref_cast<irs::byte_type>(irs::string_ref("x0"));
    rt.range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("x2"));
    rt.range.min_type = irs::BoundType::INCLUSIVE;
    rt.range.max_type = irs::BoundType::INCLUSIVE;

    auto prepared = q.prepare(rdr);
    auto sub = rdr.begin();
    auto column = sub->column_reader("name");
    ASSERT_NE(nullptr, column);
    auto values = column->values();

    auto docs = prepared->execute(*sub);
    ASSERT_FALSE(irs::get<irs::filter_boost>(*docs));
    ASSERT_FALSE(irs::get<irs::frequency>(*docs));
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::valid(docs->value()));
    auto docs_seek = prepared->execute(*sub);
    ASSERT_FALSE(irs::doc_limits::valid(docs_seek->value()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("X0", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("X0", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("X1", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("X1", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("X2", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("X2", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("X3", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("X3", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("X4", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("X4", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("X5", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("X5", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_FALSE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(irs::doc_limits::eof(docs->value()));
  }

  // by_range_options "(x0, x2]"
  {
    irs::bytes_ref actual_value;

    irs::by_phrase q;
    *q.mutable_field() = "phrase_anl";
    auto& rt = q.mutable_options()->push_back<irs::by_range_options>();
    rt.range.min = irs::ref_cast<irs::byte_type>(irs::string_ref("x0"));
    rt.range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("x2"));
    rt.range.min_type = irs::BoundType::EXCLUSIVE;
    rt.range.max_type = irs::BoundType::INCLUSIVE;

    auto prepared = q.prepare(rdr);
    auto sub = rdr.begin();
    auto column = sub->column_reader("name");
    ASSERT_NE(nullptr, column);
    auto values = column->values();

    auto docs = prepared->execute(*sub);
    ASSERT_FALSE(irs::get<irs::filter_boost>(*docs));
    ASSERT_FALSE(irs::get<irs::frequency>(*docs));
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::valid(docs->value()));
    auto docs_seek = prepared->execute(*sub);
    ASSERT_FALSE(irs::doc_limits::valid(docs_seek->value()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("X1", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("X1", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("X2", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("X2", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("X3", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("X3", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("X4", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("X4", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("X5", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("X5", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_FALSE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(irs::doc_limits::eof(docs->value()));
  }

  // by_range_options "[x0, x2)"
  {
    irs::bytes_ref actual_value;

    irs::by_phrase q;
    *q.mutable_field() = "phrase_anl";
    auto& rt = q.mutable_options()->push_back<irs::by_range_options>();
    rt.range.min = irs::ref_cast<irs::byte_type>(irs::string_ref("x0"));
    rt.range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("x2"));
    rt.range.min_type = irs::BoundType::INCLUSIVE;
    rt.range.max_type = irs::BoundType::EXCLUSIVE;

    auto prepared = q.prepare(rdr);
    auto sub = rdr.begin();
    auto column = sub->column_reader("name");
    ASSERT_NE(nullptr, column);
    auto values = column->values();

    auto docs = prepared->execute(*sub);
    ASSERT_FALSE(irs::get<irs::filter_boost>(*docs));
    ASSERT_FALSE(irs::get<irs::frequency>(*docs));
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::valid(docs->value()));
    auto docs_seek = prepared->execute(*sub);
    ASSERT_FALSE(irs::doc_limits::valid(docs_seek->value()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("X0", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("X0", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("X1", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("X1", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("X3", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("X3", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("X4", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("X4", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_FALSE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(irs::doc_limits::eof(docs->value()));
  }

  // by_range_options "(x0, x2)"
  {
    irs::bytes_ref actual_value;

    irs::by_phrase q;
    *q.mutable_field() = "phrase_anl";
    auto& rt = q.mutable_options()->push_back<irs::by_range_options>();
    rt.range.min = irs::ref_cast<irs::byte_type>(irs::string_ref("x0"));
    rt.range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("x2"));
    rt.range.min_type = irs::BoundType::EXCLUSIVE;
    rt.range.max_type = irs::BoundType::EXCLUSIVE;

    auto prepared = q.prepare(rdr);
    auto sub = rdr.begin();
    auto column = sub->column_reader("name");
    ASSERT_NE(nullptr, column);
    auto values = column->values();

    auto docs = prepared->execute(*sub);
    ASSERT_FALSE(irs::get<irs::filter_boost>(*docs));
    ASSERT_FALSE(irs::get<irs::frequency>(*docs));
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::valid(docs->value()));
    auto docs_seek = prepared->execute(*sub);
    ASSERT_FALSE(irs::doc_limits::valid(docs_seek->value()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("X1", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("X1", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("X3", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("X3", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("X4", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("X4", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_FALSE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(irs::doc_limits::eof(docs->value()));
  }

  // search "fox" on field without positions
  // which is ok for single word phrases
  {
    irs::by_phrase q;
    *q.mutable_field() = "phrase";
    q.mutable_options()->push_back<irs::by_term_options>().term = irs::ref_cast<irs::byte_type>(irs::string_ref("fox"));

    auto prepared = q.prepare(rdr);
 #ifndef IRESEARCH_DLL
    // check single word phrase optimization
    ASSERT_NE(nullptr, dynamic_cast<const irs::term_query*>(prepared.get()));
 #endif
    irs::bytes_ref actual_value;
    auto sub = rdr.begin();
    auto column = sub->column_reader("name");
    ASSERT_NE(nullptr, column);
    auto values = column->values();

    auto docs = prepared->execute(*sub);
    ASSERT_FALSE(irs::get<irs::filter_boost>(*docs));
    ASSERT_FALSE(irs::get<irs::frequency>(*docs));
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::valid(docs->value()));
    auto docs_seek = prepared->execute(*sub);
    ASSERT_FALSE(irs::doc_limits::valid(docs_seek->value()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("K", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("K", irs::to_string<irs::string_ref>(actual_value.c_str()));

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
    pt.term = irs::ref_cast<irs::byte_type>(irs::string_ref("fo"));

    auto prepared = q.prepare(rdr);
 #ifndef IRESEARCH_DLL
    // check single word phrase optimization
    ASSERT_NE(nullptr, dynamic_cast<const irs::multiterm_query*>(prepared.get()));
 #endif
    irs::bytes_ref actual_value;
    auto sub = rdr.begin();
    auto column = sub->column_reader("name");
    ASSERT_NE(nullptr, column);
    auto values = column->values();

    auto docs = prepared->execute(*sub);
    ASSERT_FALSE(irs::get<irs::filter_boost>(*docs));
    ASSERT_FALSE(irs::get<irs::frequency>(*docs));
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::valid(docs->value()));
    auto docs_seek = prepared->execute(*sub);
    ASSERT_FALSE(irs::doc_limits::valid(docs_seek->value()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("K", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("K", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("L", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("L", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("N", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("N", irs::to_string<irs::string_ref>(actual_value.c_str()));

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
    wt.term = irs::ref_cast<irs::byte_type>(irs::string_ref("fo%"));

    auto prepared = q.prepare(rdr);
 #ifndef IRESEARCH_DLL
    // check single word phrase optimization
    ASSERT_NE(nullptr, dynamic_cast<const irs::multiterm_query*>(prepared.get()));
 #endif
    irs::bytes_ref actual_value;
    auto sub = rdr.begin();
    auto column = sub->column_reader("name");
    ASSERT_NE(nullptr, column);
    auto values = column->values();

    auto docs = prepared->execute(*sub);
    ASSERT_FALSE(irs::get<irs::filter_boost>(*docs));
    ASSERT_FALSE(irs::get<irs::frequency>(*docs));
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::valid(docs->value()));
    auto docs_seek = prepared->execute(*sub);
    ASSERT_FALSE(irs::doc_limits::valid(docs_seek->value()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("K", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("K", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("L", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("L", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("N", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("N", irs::to_string<irs::string_ref>(actual_value.c_str()));

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
    wt.term = irs::ref_cast<irs::byte_type>(irs::string_ref("f_x%"));

    auto prepared = q.prepare(rdr);
 #ifndef IRESEARCH_DLL
    // check single word phrase optimization
    ASSERT_NE(nullptr, dynamic_cast<const irs::multiterm_query*>(prepared.get()));
 #endif
    irs::bytes_ref actual_value;
    auto sub = rdr.begin();
    auto column = sub->column_reader("name");
    ASSERT_NE(nullptr, column);
    auto values = column->values();

    auto docs = prepared->execute(*sub);
    ASSERT_FALSE(irs::get<irs::filter_boost>(*docs));
    ASSERT_FALSE(irs::get<irs::frequency>(*docs));
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::valid(docs->value()));
    auto docs_seek = prepared->execute(*sub);
    ASSERT_FALSE(irs::doc_limits::valid(docs_seek->value()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("K", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("K", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("L", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("L", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("N", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("N", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_FALSE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(irs::doc_limits::eof(docs->value()));
  }

  // search "fxo" on field without positions
  // which is ok for single word phrases
  {
    irs::by_phrase q;
    *q.mutable_field() = "phrase";
    auto& lt = q.mutable_options()->push_back<irs::by_edit_distance_filter_options>();
    lt.max_distance = 1;
    lt.with_transpositions = true;
    lt.term = irs::ref_cast<irs::byte_type>(irs::string_ref("fxo"));

    auto prepared = q.prepare(rdr);
 #ifndef IRESEARCH_DLL
    // check single word phrase optimization
    ASSERT_NE(nullptr, dynamic_cast<const irs::multiterm_query*>(prepared.get()));
 #endif
    irs::bytes_ref actual_value;
    auto sub = rdr.begin();
    auto column = sub->column_reader("name");
    ASSERT_NE(nullptr, column);
    auto values = column->values();

    auto docs = prepared->execute(*sub);
    ASSERT_FALSE(irs::get<irs::filter_boost>(*docs));
    ASSERT_FALSE(irs::get<irs::frequency>(*docs));
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::valid(docs->value()));
    auto docs_seek = prepared->execute(*sub);
    ASSERT_FALSE(irs::doc_limits::valid(docs_seek->value()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("K", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("K", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_FALSE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(irs::doc_limits::eof(docs->value()));
  }

  // search by_range_options "[x0, x1]" on field without positions
  // which is ok for first word in phrase
  {
    irs::bytes_ref actual_value;

    irs::by_phrase q;
    *q.mutable_field() = "phrase";
    auto& rt = q.mutable_options()->push_back<irs::by_range_options>();
    rt.range.min = irs::ref_cast<irs::byte_type>(irs::string_ref("x0"));
    rt.range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("x1"));
    rt.range.min_type = irs::BoundType::INCLUSIVE;
    rt.range.max_type = irs::BoundType::INCLUSIVE;

    auto prepared = q.prepare(rdr);
 #ifndef IRESEARCH_DLL
    // check single word phrase optimization
    ASSERT_NE(nullptr, dynamic_cast<const irs::multiterm_query*>(prepared.get()));
 #endif
    auto sub = rdr.begin();
    auto column = sub->column_reader("name");
    ASSERT_NE(nullptr, column);
    auto values = column->values();

    auto docs = prepared->execute(*sub);
    ASSERT_FALSE(irs::get<irs::filter_boost>(*docs));
    ASSERT_FALSE(irs::get<irs::frequency>(*docs));
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::valid(docs->value()));
    auto docs_seek = prepared->execute(*sub);
    ASSERT_FALSE(irs::doc_limits::valid(docs_seek->value()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("X0", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("X0", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("X1", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("X1", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_FALSE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(irs::doc_limits::eof(docs->value()));
  }

  // term_filter "fox" with phrase offset
  // which does not matter
  {
    irs::bytes_ref actual_value;

    irs::by_phrase q;
    *q.mutable_field() = "phrase_anl";
    q.mutable_options()->push_back<irs::by_term_options>(std::numeric_limits<size_t>::max()).term = irs::ref_cast<irs::byte_type>(irs::string_ref("fox"));

    auto prepared = q.prepare(rdr);
 #ifndef IRESEARCH_DLL
    // check single word phrase optimization
    ASSERT_NE(nullptr, dynamic_cast<const irs::term_query*>(prepared.get()));
 #endif
    auto sub = rdr.begin();
    auto column = sub->column_reader("name");
    ASSERT_NE(nullptr, column);
    auto values = column->values();

    auto docs = prepared->execute(*sub);
    ASSERT_FALSE(irs::get<irs::filter_boost>(*docs));
    ASSERT_FALSE(irs::get<irs::frequency>(*docs));
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::valid(docs->value()));
    auto docs_seek = prepared->execute(*sub);
    ASSERT_FALSE(irs::doc_limits::valid(docs_seek->value()));

    ASSERT_TRUE(docs->next());
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("A", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("A", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("G", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("G", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("I", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("I", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("K", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("K", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("L", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("L", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("N", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("N", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("S", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("S", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("T", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("T", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("V", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("V", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_FALSE(docs->next());
    ASSERT_TRUE(irs::doc_limits::eof(docs->value()));
  }

  // prefix_filter "fo*" with phrase offset
  // which does not matter
  {
    irs::bytes_ref actual_value;

    irs::by_phrase q;
    *q.mutable_field() = "phrase_anl";
    auto& pt = q.mutable_options()->push_back<irs::by_prefix_options>(std::numeric_limits<size_t>::max());
    pt.term = irs::ref_cast<irs::byte_type>(irs::string_ref("fo"));

    auto prepared = q.prepare(rdr);
 #ifndef IRESEARCH_DLL
    // check single word phrase optimization
    ASSERT_NE(nullptr, dynamic_cast<const irs::multiterm_query*>(prepared.get()));
 #endif
    auto sub = rdr.begin();
    auto column = sub->column_reader("name");
    ASSERT_NE(nullptr, column);
    auto values = column->values();

    auto docs = prepared->execute(*sub);
    ASSERT_FALSE(irs::get<irs::filter_boost>(*docs));
    ASSERT_FALSE(irs::get<irs::frequency>(*docs));
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::valid(docs->value()));
    auto docs_seek = prepared->execute(*sub);
    ASSERT_FALSE(irs::doc_limits::valid(docs_seek->value()));

    ASSERT_TRUE(docs->next());
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("A", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("A", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("D", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("D", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("G", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("G", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("H", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("H", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("I", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("I", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("K", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("K", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("L", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("L", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("N", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("N", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("S", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("S", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("T", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("T", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("U", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("U", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("V", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("V", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("W", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("W", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("X", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("X", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("Y", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("Y", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_FALSE(docs->next());
    ASSERT_TRUE(irs::doc_limits::eof(docs->value()));
  }

  // wildcard_filter "fo%" with phrase offset
  // which does not matter
  {
    irs::bytes_ref actual_value;

    irs::by_phrase q;
    *q.mutable_field() = "phrase_anl";
    auto& wt = q.mutable_options()->push_back<irs::by_wildcard_options>(std::numeric_limits<size_t>::max());
    wt.term = irs::ref_cast<irs::byte_type>(irs::string_ref("fo%"));

    auto prepared = q.prepare(rdr);
 #ifndef IRESEARCH_DLL
    // check single word phrase optimization
    ASSERT_NE(nullptr, dynamic_cast<const irs::multiterm_query*>(prepared.get()));
 #endif
    auto sub = rdr.begin();
    auto column = sub->column_reader("name");
    ASSERT_NE(nullptr, column);
    auto values = column->values();

    auto docs = prepared->execute(*sub);
    ASSERT_FALSE(irs::get<irs::filter_boost>(*docs));
    ASSERT_FALSE(irs::get<irs::frequency>(*docs));
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::valid(docs->value()));
    auto docs_seek = prepared->execute(*sub);
    ASSERT_FALSE(irs::doc_limits::valid(docs_seek->value()));

    ASSERT_TRUE(docs->next());
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("A", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("A", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("D", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("D", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("G", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("G", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("H", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("H", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("I", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("I", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("K", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("K", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("L", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("L", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("N", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("N", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("S", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("S", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("T", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("T", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("U", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("U", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("V", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("V", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("W", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("W", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("X", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("X", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("Y", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("Y", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_FALSE(docs->next());
    ASSERT_TRUE(irs::doc_limits::eof(docs->value()));
  }

  // wildcard_filter "f%x" with phrase offset
  // which does not matter
  {
    irs::bytes_ref actual_value;

    irs::by_phrase q;
    *q.mutable_field() = "phrase_anl";
    auto& wt = q.mutable_options()->push_back<irs::by_wildcard_options>(std::numeric_limits<size_t>::max());
    wt.term = irs::ref_cast<irs::byte_type>(irs::string_ref("f%x"));

    auto prepared = q.prepare(rdr);
#ifndef IRESEARCH_DLL
    // check single word phrase optimization
    ASSERT_NE(nullptr, dynamic_cast<const irs::multiterm_query*>(prepared.get()));
#endif
    auto sub = rdr.begin();
    auto column = sub->column_reader("name");
    ASSERT_NE(nullptr, column);
    auto values = column->values();

    auto docs = prepared->execute(*sub);
    ASSERT_FALSE(irs::get<irs::filter_boost>(*docs));
    ASSERT_FALSE(irs::get<irs::frequency>(*docs));
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::valid(docs->value()));
    auto docs_seek = prepared->execute(*sub);
    ASSERT_FALSE(irs::doc_limits::valid(docs_seek->value()));

    ASSERT_TRUE(docs->next());
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("A", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("A", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("G", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("G", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("I", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("I", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("K", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("K", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("L", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("L", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("N", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("N", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("S", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("S", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("T", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("T", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("V", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("V", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_FALSE(docs->next());
    ASSERT_TRUE(irs::doc_limits::eof(docs->value()));
  }

  // wildcard_filter "f%x" with phrase offset
  // which does not matter
  {
    irs::bytes_ref actual_value;

    irs::by_phrase q;
    *q.mutable_field() = "phrase_anl";
    auto& lt = q.mutable_options()->push_back<irs::by_edit_distance_filter_options>(std::numeric_limits<size_t>::max());
    lt.max_distance = 1;
    lt.term = irs::ref_cast<irs::byte_type>(irs::string_ref("fkx"));

    auto prepared = q.prepare(rdr);
#ifndef IRESEARCH_DLL
    // check single word phrase optimization
    ASSERT_NE(nullptr, dynamic_cast<const irs::multiterm_query*>(prepared.get()));
#endif
    auto sub = rdr.begin();
    auto column = sub->column_reader("name");
    ASSERT_NE(nullptr, column);
    auto values = column->values();

    auto docs = prepared->execute(*sub);
    ASSERT_FALSE(irs::get<irs::filter_boost>(*docs));
    ASSERT_FALSE(irs::get<irs::frequency>(*docs));
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::valid(docs->value()));
    auto docs_seek = prepared->execute(*sub);
    ASSERT_FALSE(irs::doc_limits::valid(docs_seek->value()));

    ASSERT_TRUE(docs->next());
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("A", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("A", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("G", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("G", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("I", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("I", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("K", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("K", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("L", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("L", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("N", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("N", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("S", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("S", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("T", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("T", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("V", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("V", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_FALSE(docs->next());
    ASSERT_TRUE(irs::doc_limits::eof(docs->value()));
  }

  // search by_range_options "[x0, x1]" with phrase offset
  // which does not matter
  {
    irs::bytes_ref actual_value;

    irs::by_phrase q;
    *q.mutable_field() = "phrase_anl";
    auto& rt = q.mutable_options()->push_back<irs::by_range_options>(std::numeric_limits<size_t>::max());
    rt.range.min = irs::ref_cast<irs::byte_type>(irs::string_ref("x0"));
    rt.range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("x1"));
    rt.range.min_type = irs::BoundType::INCLUSIVE;
    rt.range.max_type = irs::BoundType::INCLUSIVE;

    auto prepared = q.prepare(rdr);
#ifndef IRESEARCH_DLL
    // check single word phrase optimization
    ASSERT_NE(nullptr, dynamic_cast<const irs::multiterm_query*>(prepared.get()));
#endif
    auto sub = rdr.begin();
    auto column = sub->column_reader("name");
    ASSERT_NE(nullptr, column);
    auto values = column->values();

    auto docs = prepared->execute(*sub);
    ASSERT_FALSE(irs::get<irs::filter_boost>(*docs));
    ASSERT_FALSE(irs::get<irs::frequency>(*docs));
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::valid(docs->value()));
    auto docs_seek = prepared->execute(*sub);
    ASSERT_FALSE(irs::doc_limits::valid(docs_seek->value()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("X0", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("X0", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("X1", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("X1", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("X3", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("X3", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("X4", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("X4", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_FALSE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(irs::doc_limits::eof(docs->value()));
  }
}

TEST_P(phrase_filter_test_case, sequential_three_terms) {
  // add segment
  {
    tests::json_doc_generator gen(
      resource("phrase_sequential.json"),
      &tests::analyzed_json_field_factory);
    add_segment(gen);
  }

  // read segment
  auto rdr = open_reader();

  // "quick brown fox"
  {
    irs::bytes_ref actual_value;

    irs::by_phrase q;
    *q.mutable_field() = "phrase_anl";
    q.mutable_options()->push_back<irs::by_term_options>().term = irs::ref_cast<irs::byte_type>(irs::string_ref("quick"));
    q.mutable_options()->push_back<irs::by_term_options>().term = irs::ref_cast<irs::byte_type>(irs::string_ref("brown"));
    q.mutable_options()->push_back<irs::by_term_options>().term = irs::ref_cast<irs::byte_type>(irs::string_ref("fox"));

    auto prepared = q.prepare(rdr);
    auto sub = rdr.begin();
    auto column = sub->column_reader("name");
    ASSERT_NE(nullptr, column);
    auto values = column->values();

    auto docs = prepared->execute(*sub);
    ASSERT_FALSE(irs::get<irs::filter_boost>(*docs));
    ASSERT_FALSE(irs::get<irs::frequency>(*docs));
    auto* score = irs::get<irs::score>(*docs);
    ASSERT_NE(nullptr, score);
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::valid(docs->value()));
    auto docs_seek = prepared->execute(*sub);
    ASSERT_FALSE(irs::doc_limits::valid(docs_seek->value()));

    ASSERT_TRUE(docs->next());
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("A", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("A", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("G", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("G", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("I", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("I", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_FALSE(docs->next());
    ASSERT_TRUE(irs::doc_limits::eof(docs->value()));
    ASSERT_TRUE(irs::doc_limits::eof(docs_seek->seek(irs::doc_limits::eof())));
  }

  // "qui* brown fox"
  {
    irs::bytes_ref actual_value;

    irs::by_phrase q;
    *q.mutable_field() = "phrase_anl";
    auto& pt = q.mutable_options()->push_back<irs::by_prefix_options>();
    pt.term = irs::ref_cast<irs::byte_type>(irs::string_ref("qui"));
    q.mutable_options()->push_back<irs::by_term_options>().term = irs::ref_cast<irs::byte_type>(irs::string_ref("brown"));
    q.mutable_options()->push_back<irs::by_term_options>().term = irs::ref_cast<irs::byte_type>(irs::string_ref("fox"));

    auto prepared = q.prepare(rdr);
    auto sub = rdr.begin();
    auto column = sub->column_reader("name");
    ASSERT_NE(nullptr, column);
    auto values = column->values();

    auto docs = prepared->execute(*sub);
    ASSERT_FALSE(irs::get<irs::filter_boost>(*docs));
    ASSERT_FALSE(irs::get<irs::frequency>(*docs));
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::valid(docs->value()));
    auto docs_seek = prepared->execute(*sub);
    ASSERT_FALSE(irs::doc_limits::valid(docs_seek->value()));

    ASSERT_TRUE(docs->next());
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("A", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("A", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("G", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("G", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("I", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("I", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("S", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("S", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_FALSE(docs->next());
    ASSERT_TRUE(irs::doc_limits::eof(docs->value()));
    ASSERT_TRUE(irs::doc_limits::eof(docs_seek->seek(irs::doc_limits::eof())));
  }

  // "qui% brown fox"
  {
    irs::bytes_ref actual_value;

    irs::by_phrase q;
    *q.mutable_field() = "phrase_anl";
    auto& wt = q.mutable_options()->push_back<irs::by_wildcard_options>();
    wt.term = irs::ref_cast<irs::byte_type>(irs::string_ref("qui%"));
    q.mutable_options()->push_back<irs::by_term_options>().term = irs::ref_cast<irs::byte_type>(irs::string_ref("brown"));
    q.mutable_options()->push_back<irs::by_term_options>().term = irs::ref_cast<irs::byte_type>(irs::string_ref("fox"));

    auto prepared = q.prepare(rdr);
    auto sub = rdr.begin();
    auto column = sub->column_reader("name");
    ASSERT_NE(nullptr, column);
    auto values = column->values();

    auto docs = prepared->execute(*sub);
    ASSERT_FALSE(irs::get<irs::filter_boost>(*docs));
    ASSERT_FALSE(irs::get<irs::frequency>(*docs));
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::valid(docs->value()));
    auto docs_seek = prepared->execute(*sub);
    ASSERT_FALSE(irs::doc_limits::valid(docs_seek->value()));

    ASSERT_TRUE(docs->next());
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("A", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("A", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("G", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("G", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("I", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("I", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("S", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("S", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_FALSE(docs->next());
    ASSERT_TRUE(irs::doc_limits::eof(docs->value()));
    ASSERT_TRUE(irs::doc_limits::eof(docs_seek->seek(irs::doc_limits::eof())));
  }

  // "q%ck brown fox"
  {
    irs::bytes_ref actual_value;

    irs::by_phrase q;
    *q.mutable_field() = "phrase_anl";
    auto& wt = q.mutable_options()->push_back<irs::by_wildcard_options>();
    wt.term = irs::ref_cast<irs::byte_type>(irs::string_ref("q%ck"));
    q.mutable_options()->push_back<irs::by_term_options>().term = irs::ref_cast<irs::byte_type>(irs::string_ref("brown"));
    q.mutable_options()->push_back<irs::by_term_options>().term = irs::ref_cast<irs::byte_type>(irs::string_ref("fox"));

    auto prepared = q.prepare(rdr);
    auto sub = rdr.begin();
    auto column = sub->column_reader("name");
    ASSERT_NE(nullptr, column);
    auto values = column->values();

    auto docs = prepared->execute(*sub);
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::valid(docs->value()));
    auto docs_seek = prepared->execute(*sub);
    ASSERT_FALSE(irs::doc_limits::valid(docs_seek->value()));

    ASSERT_TRUE(docs->next());
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("A", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("A", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("G", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("G", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("I", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("I", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_FALSE(docs->next());
    ASSERT_TRUE(irs::doc_limits::eof(docs->value()));
    ASSERT_TRUE(irs::doc_limits::eof(docs_seek->seek(irs::doc_limits::eof())));
  }

  // "quick brown fox" simple term max_distance = 0
  {
    irs::bytes_ref actual_value;

    irs::by_phrase q;
    *q.mutable_field() = "phrase_anl";
    auto& lt = q.mutable_options()->push_back<irs::by_edit_distance_filter_options>();
    lt.max_distance = 0;
    lt.term = irs::ref_cast<irs::byte_type>(irs::string_ref("quick"));
    q.mutable_options()->push_back<irs::by_term_options>().term = irs::ref_cast<irs::byte_type>(irs::string_ref("brown"));
    q.mutable_options()->push_back<irs::by_term_options>().term = irs::ref_cast<irs::byte_type>(irs::string_ref("fox"));

    auto prepared = q.prepare(rdr);
    auto sub = rdr.begin();
    auto column = sub->column_reader("name");
    ASSERT_NE(nullptr, column);
    auto values = column->values();

    auto docs = prepared->execute(*sub);
    ASSERT_FALSE(irs::get<irs::filter_boost>(*docs));
    ASSERT_FALSE(irs::get<irs::frequency>(*docs));
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::valid(docs->value()));
    auto docs_seek = prepared->execute(*sub);
    ASSERT_FALSE(irs::doc_limits::valid(docs_seek->value()));

    ASSERT_TRUE(docs->next());
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("A", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("A", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("G", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("G", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("I", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("I", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_FALSE(docs->next());
    ASSERT_TRUE(irs::doc_limits::eof(docs->value()));
    ASSERT_TRUE(irs::doc_limits::eof(docs_seek->seek(irs::doc_limits::eof())));
  }

  // "quck brown fox"
  {
    irs::bytes_ref actual_value;

    irs::by_phrase q;
    *q.mutable_field() = "phrase_anl";
    auto& lt = q.mutable_options()->push_back<irs::by_edit_distance_filter_options>();
    lt.max_distance = 1;
    lt.term = irs::ref_cast<irs::byte_type>(irs::string_ref("quck"));
    q.mutable_options()->push_back<irs::by_term_options>().term = irs::ref_cast<irs::byte_type>(irs::string_ref("brown"));
    q.mutable_options()->push_back<irs::by_term_options>().term = irs::ref_cast<irs::byte_type>(irs::string_ref("fox"));

    auto prepared = q.prepare(rdr);
    auto sub = rdr.begin();
    auto column = sub->column_reader("name");
    ASSERT_NE(nullptr, column);
    auto values = column->values();

    auto docs = prepared->execute(*sub);
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::valid(docs->value()));
    auto docs_seek = prepared->execute(*sub);
    ASSERT_FALSE(irs::doc_limits::valid(docs_seek->value()));

    ASSERT_TRUE(docs->next());
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("A", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("A", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("G", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("G", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("I", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("I", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_FALSE(docs->next());
    ASSERT_TRUE(irs::doc_limits::eof(docs->value()));
    ASSERT_TRUE(irs::doc_limits::eof(docs_seek->seek(irs::doc_limits::eof())));
  }

  // "[x0, x1] x0 x2
  {
    irs::bytes_ref actual_value;

    irs::by_phrase q;
    *q.mutable_field() = "phrase_anl";
    auto& rt = q.mutable_options()->push_back<irs::by_range_options>();
    rt.range.min = irs::ref_cast<irs::byte_type>(irs::string_ref("x0"));
    rt.range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("x1"));
    rt.range.min_type = irs::BoundType::INCLUSIVE;
    rt.range.max_type = irs::BoundType::INCLUSIVE;
    q.mutable_options()->push_back<irs::by_term_options>().term = irs::ref_cast<irs::byte_type>(irs::string_ref("x0"));
    q.mutable_options()->push_back<irs::by_term_options>().term = irs::ref_cast<irs::byte_type>(irs::string_ref("x2"));

    auto prepared = q.prepare(rdr);
    auto sub = rdr.begin();
    auto column = sub->column_reader("name");
    ASSERT_NE(nullptr, column);
    auto values = column->values();

    auto docs = prepared->execute(*sub);
    ASSERT_FALSE(irs::get<irs::filter_boost>(*docs));
    ASSERT_FALSE(irs::get<irs::frequency>(*docs));
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::valid(docs->value()));
    auto docs_seek = prepared->execute(*sub);
    ASSERT_FALSE(irs::doc_limits::valid(docs_seek->value()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("X4", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("X4", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_FALSE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(irs::doc_limits::eof(docs->value()));
  }

  // "quick bro* fox"
  {
    irs::bytes_ref actual_value;

    irs::by_phrase q;
    *q.mutable_field() = "phrase_anl";
    q.mutable_options()->push_back<irs::by_term_options>().term = irs::ref_cast<irs::byte_type>(irs::string_ref("quick"));
    auto& pt = q.mutable_options()->push_back<irs::by_prefix_options>();
    pt.term = irs::ref_cast<irs::byte_type>(irs::string_ref("bro"));
    q.mutable_options()->push_back<irs::by_term_options>().term = irs::ref_cast<irs::byte_type>(irs::string_ref("fox"));

    auto prepared = q.prepare(rdr);
    auto sub = rdr.begin();
    auto column = sub->column_reader("name");
    ASSERT_NE(nullptr, column);
    auto values = column->values();

    auto docs = prepared->execute(*sub);
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::valid(docs->value()));
    auto docs_seek = prepared->execute(*sub);
    ASSERT_FALSE(irs::doc_limits::valid(docs_seek->value()));

    ASSERT_TRUE(docs->next());
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("A", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("A", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("G", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("G", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("I", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("I", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("T", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("T", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_FALSE(docs->next());
    ASSERT_TRUE(irs::doc_limits::eof(docs->value()));
    ASSERT_TRUE(irs::doc_limits::eof(docs_seek->seek(irs::doc_limits::eof())));
  }

  // "quick bro% fox"
  {
    irs::bytes_ref actual_value;

    irs::by_phrase q;
    *q.mutable_field() = "phrase_anl";
    q.mutable_options()->push_back<irs::by_term_options>().term = irs::ref_cast<irs::byte_type>(irs::string_ref("quick"));
    auto& wt = q.mutable_options()->push_back<irs::by_wildcard_options>();
    wt.term = irs::ref_cast<irs::byte_type>(irs::string_ref("bro%"));
    q.mutable_options()->push_back<irs::by_term_options>().term = irs::ref_cast<irs::byte_type>(irs::string_ref("fox"));

    auto prepared = q.prepare(rdr);
    auto sub = rdr.begin();
    auto column = sub->column_reader("name");
    ASSERT_NE(nullptr, column);
    auto values = column->values();

    auto docs = prepared->execute(*sub);
    ASSERT_FALSE(irs::get<irs::filter_boost>(*docs));
    ASSERT_FALSE(irs::get<irs::frequency>(*docs));
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::valid(docs->value()));
    auto docs_seek = prepared->execute(*sub);
    ASSERT_FALSE(irs::doc_limits::valid(docs_seek->value()));

    ASSERT_TRUE(docs->next());
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("A", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("A", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("G", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("G", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("I", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("I", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("T", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("T", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_FALSE(docs->next());
    ASSERT_TRUE(irs::doc_limits::eof(docs->value()));
    ASSERT_TRUE(irs::doc_limits::eof(docs_seek->seek(irs::doc_limits::eof())));
  }

  // "quick b%w_ fox"
  {
    irs::bytes_ref actual_value;

    irs::by_phrase q;
    *q.mutable_field() = "phrase_anl";
    q.mutable_options()->push_back<irs::by_term_options>().term = irs::ref_cast<irs::byte_type>(irs::string_ref("quick"));
    auto& wt = q.mutable_options()->push_back<irs::by_wildcard_options>();
    wt.term = irs::ref_cast<irs::byte_type>(irs::string_ref("b%w_"));
    q.mutable_options()->push_back<irs::by_term_options>().term = irs::ref_cast<irs::byte_type>(irs::string_ref("fox"));

    auto prepared = q.prepare(rdr);
    auto sub = rdr.begin();
    auto column = sub->column_reader("name");
    ASSERT_NE(nullptr, column);
    auto values = column->values();

    auto docs = prepared->execute(*sub);
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::valid(docs->value()));
    auto docs_seek = prepared->execute(*sub);
    ASSERT_FALSE(irs::doc_limits::valid(docs_seek->value()));

    ASSERT_TRUE(docs->next());
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("A", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("A", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("G", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("G", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("I", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("I", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_FALSE(docs->next());
    ASSERT_TRUE(irs::doc_limits::eof(docs->value()));
    ASSERT_TRUE(irs::doc_limits::eof(docs_seek->seek(irs::doc_limits::eof())));
  }

  // "quick brkln fox"
  {
    irs::bytes_ref actual_value;

    irs::by_phrase q;
    *q.mutable_field() = "phrase_anl";
    q.mutable_options()->push_back<irs::by_term_options>().term = irs::ref_cast<irs::byte_type>(irs::string_ref("quick"));
    auto& lt = q.mutable_options()->push_back<irs::by_edit_distance_filter_options>();
    lt.max_distance = 2;
    lt.term = irs::ref_cast<irs::byte_type>(irs::string_ref("brkln"));
    q.mutable_options()->push_back<irs::by_term_options>().term = irs::ref_cast<irs::byte_type>(irs::string_ref("fox"));

    auto prepared = q.prepare(rdr);
    auto sub = rdr.begin();
    auto column = sub->column_reader("name");
    ASSERT_NE(nullptr, column);
    auto values = column->values();

    auto docs = prepared->execute(*sub);
    ASSERT_FALSE(irs::get<irs::filter_boost>(*docs));
    ASSERT_FALSE(irs::get<irs::frequency>(*docs));
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::valid(docs->value()));
    auto docs_seek = prepared->execute(*sub);
    ASSERT_FALSE(irs::doc_limits::valid(docs_seek->value()));

    ASSERT_TRUE(docs->next());
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("A", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("A", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("G", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("G", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("I", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("I", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_FALSE(docs->next());
    ASSERT_TRUE(irs::doc_limits::eof(docs->value()));
    ASSERT_TRUE(irs::doc_limits::eof(docs_seek->seek(irs::doc_limits::eof())));
  }

  // "x1 [x0, x1] x2"
  {
    irs::bytes_ref actual_value;

    irs::by_phrase q;
    *q.mutable_field() = "phrase_anl";
    q.mutable_options()->push_back<irs::by_term_options>().term = irs::ref_cast<irs::byte_type>(irs::string_ref("x1"));
    auto& rt = q.mutable_options()->push_back<irs::by_range_options>();
    rt.range.min = irs::ref_cast<irs::byte_type>(irs::string_ref("x0"));
    rt.range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("x1"));
    rt.range.min_type = irs::BoundType::INCLUSIVE;
    rt.range.max_type = irs::BoundType::INCLUSIVE;
    q.mutable_options()->push_back<irs::by_term_options>().term = irs::ref_cast<irs::byte_type>(irs::string_ref("x2"));

    auto prepared = q.prepare(rdr);
    auto sub = rdr.begin();
    auto column = sub->column_reader("name");
    ASSERT_NE(nullptr, column);
    auto values = column->values();

    auto docs = prepared->execute(*sub);
    ASSERT_FALSE(irs::get<irs::filter_boost>(*docs));
    ASSERT_FALSE(irs::get<irs::frequency>(*docs));
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::valid(docs->value()));
    auto docs_seek = prepared->execute(*sub);
    ASSERT_FALSE(irs::doc_limits::valid(docs_seek->value()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("X4", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("X4", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_FALSE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(irs::doc_limits::eof(docs->value()));
  }

  // "quick brown fo*"
  {
    irs::bytes_ref actual_value;

    irs::by_phrase q;
    *q.mutable_field() = "phrase_anl";
    q.mutable_options()->push_back<irs::by_term_options>().term = irs::ref_cast<irs::byte_type>(irs::string_ref("quick"));
    q.mutable_options()->push_back<irs::by_term_options>().term = irs::ref_cast<irs::byte_type>(irs::string_ref("brown"));
    auto& pt = q.mutable_options()->push_back<irs::by_prefix_options>();
    pt.term = irs::ref_cast<irs::byte_type>(irs::string_ref("fo"));

    auto prepared = q.prepare(rdr);
    auto sub = rdr.begin();
    auto column = sub->column_reader("name");
    ASSERT_NE(nullptr, column);
    auto values = column->values();

    auto docs = prepared->execute(*sub);
    ASSERT_FALSE(irs::get<irs::filter_boost>(*docs));
    ASSERT_FALSE(irs::get<irs::frequency>(*docs));
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::valid(docs->value()));
    auto docs_seek = prepared->execute(*sub);
    ASSERT_FALSE(irs::doc_limits::valid(docs_seek->value()));

    ASSERT_TRUE(docs->next());
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("A", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("A", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("G", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("G", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("I", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("I", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("U", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("U", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_FALSE(docs->next());
    ASSERT_TRUE(irs::doc_limits::eof(docs->value()));
    ASSERT_TRUE(irs::doc_limits::eof(docs_seek->seek(irs::doc_limits::eof())));
  }

  // "quick brown fo%"
  {
    irs::bytes_ref actual_value;

    irs::by_phrase q;
    *q.mutable_field() = "phrase_anl";
    q.mutable_options()->push_back<irs::by_term_options>().term = irs::ref_cast<irs::byte_type>(irs::string_ref("quick"));
    q.mutable_options()->push_back<irs::by_term_options>().term = irs::ref_cast<irs::byte_type>(irs::string_ref("brown"));
    auto& wt = q.mutable_options()->push_back<irs::by_wildcard_options>();
    wt.term = irs::ref_cast<irs::byte_type>(irs::string_ref("fo%"));

    auto prepared = q.prepare(rdr);
    auto sub = rdr.begin();
    auto column = sub->column_reader("name");
    ASSERT_NE(nullptr, column);
    auto values = column->values();

    auto docs = prepared->execute(*sub);
    ASSERT_FALSE(irs::get<irs::filter_boost>(*docs));
    ASSERT_FALSE(irs::get<irs::frequency>(*docs));
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::valid(docs->value()));
    auto docs_seek = prepared->execute(*sub);
    ASSERT_FALSE(irs::doc_limits::valid(docs_seek->value()));

    ASSERT_TRUE(docs->next());
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("A", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("A", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("G", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("G", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("I", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("I", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("U", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("U", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_FALSE(docs->next());
    ASSERT_TRUE(irs::doc_limits::eof(docs->value()));
    ASSERT_TRUE(irs::doc_limits::eof(docs_seek->seek(irs::doc_limits::eof())));
  }

  // "quick brown f_x"
  {
    irs::bytes_ref actual_value;

    irs::by_phrase q;
    *q.mutable_field() = "phrase_anl";
    q.mutable_options()->push_back<irs::by_term_options>().term = irs::ref_cast<irs::byte_type>(irs::string_ref("quick"));
    q.mutable_options()->push_back<irs::by_term_options>().term = irs::ref_cast<irs::byte_type>(irs::string_ref("brown"));
    auto& wt = q.mutable_options()->push_back<irs::by_wildcard_options>();
    wt.term = irs::ref_cast<irs::byte_type>(irs::string_ref("f_x"));

    auto prepared = q.prepare(rdr);
    auto sub = rdr.begin();
    auto column = sub->column_reader("name");
    ASSERT_NE(nullptr, column);
    auto values = column->values();

    auto docs = prepared->execute(*sub);
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::valid(docs->value()));
    auto docs_seek = prepared->execute(*sub);
    ASSERT_FALSE(irs::doc_limits::valid(docs_seek->value()));

    ASSERT_TRUE(docs->next());
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("A", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("A", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("G", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("G", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("I", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("I", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_FALSE(docs->next());
    ASSERT_TRUE(irs::doc_limits::eof(docs->value()));
    ASSERT_TRUE(irs::doc_limits::eof(docs_seek->seek(irs::doc_limits::eof())));
  }

  // "quick brown fxo"
  {
    irs::bytes_ref actual_value;

    irs::by_phrase q;
    *q.mutable_field() = "phrase_anl";
    q.mutable_options()->push_back<irs::by_term_options>().term = irs::ref_cast<irs::byte_type>(irs::string_ref("quick"));
    q.mutable_options()->push_back<irs::by_term_options>().term = irs::ref_cast<irs::byte_type>(irs::string_ref("brown"));
    auto& lt = q.mutable_options()->push_back<irs::by_edit_distance_filter_options>();
    lt.max_distance = 1;
    lt.with_transpositions = true;
    lt.term = irs::ref_cast<irs::byte_type>(irs::string_ref("fxo"));

    auto prepared = q.prepare(rdr);
    auto sub = rdr.begin();
    auto column = sub->column_reader("name");
    ASSERT_NE(nullptr, column);
    auto values = column->values();

    auto docs = prepared->execute(*sub);
    ASSERT_FALSE(irs::get<irs::filter_boost>(*docs));
    ASSERT_FALSE(irs::get<irs::frequency>(*docs));
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::valid(docs->value()));
    auto docs_seek = prepared->execute(*sub);
    ASSERT_FALSE(irs::doc_limits::valid(docs_seek->value()));

    ASSERT_TRUE(docs->next());
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("A", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("A", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("G", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("G", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("I", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("I", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_FALSE(docs->next());
    ASSERT_TRUE(irs::doc_limits::eof(docs->value()));
    ASSERT_TRUE(irs::doc_limits::eof(docs_seek->seek(irs::doc_limits::eof())));
  }

  // "x1 x0 [x1, x2]"
  {
    irs::bytes_ref actual_value;

    irs::by_phrase q;
    *q.mutable_field() = "phrase_anl";
    q.mutable_options()->push_back<irs::by_term_options>().term = irs::ref_cast<irs::byte_type>(irs::string_ref("x1"));
    q.mutable_options()->push_back<irs::by_term_options>().term = irs::ref_cast<irs::byte_type>(irs::string_ref("x0"));
    auto& rt = q.mutable_options()->push_back<irs::by_range_options>();
    rt.range.min = irs::ref_cast<irs::byte_type>(irs::string_ref("x1"));
    rt.range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("x2"));
    rt.range.min_type = irs::BoundType::INCLUSIVE;
    rt.range.max_type = irs::BoundType::INCLUSIVE;

    auto prepared = q.prepare(rdr);
    auto sub = rdr.begin();
    auto column = sub->column_reader("name");
    ASSERT_NE(nullptr, column);
    auto values = column->values();

    auto docs = prepared->execute(*sub);
    ASSERT_FALSE(irs::get<irs::filter_boost>(*docs));
    ASSERT_FALSE(irs::get<irs::frequency>(*docs));
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::valid(docs->value()));
    auto docs_seek = prepared->execute(*sub);
    ASSERT_FALSE(irs::doc_limits::valid(docs_seek->value()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("X4", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("X4", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_FALSE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(irs::doc_limits::eof(docs->value()));
  }

  // "qui* bro* fox"
  {
    irs::bytes_ref actual_value;

    irs::by_phrase q;
    *q.mutable_field() = "phrase_anl";
    auto& pt1 = q.mutable_options()->push_back<irs::by_prefix_options>();
    pt1.term = irs::ref_cast<irs::byte_type>(irs::string_ref("qui"));
    auto& pt2 = q.mutable_options()->push_back<irs::by_prefix_options>();
    pt2.term = irs::ref_cast<irs::byte_type>(irs::string_ref("bro"));
    q.mutable_options()->push_back<irs::by_term_options>().term = irs::ref_cast<irs::byte_type>(irs::string_ref("fox"));

    auto prepared = q.prepare(rdr);
    auto sub = rdr.begin();
    auto column = sub->column_reader("name");
    ASSERT_NE(nullptr, column);
    auto values = column->values();

    auto docs = prepared->execute(*sub);
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::valid(docs->value()));
    auto docs_seek = prepared->execute(*sub);
    ASSERT_FALSE(irs::doc_limits::valid(docs_seek->value()));

    ASSERT_TRUE(docs->next());
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("A", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("A", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("G", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("G", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("I", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("I", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("S", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("S", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("T", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("T", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("V", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("V", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_FALSE(docs->next());
    ASSERT_TRUE(irs::doc_limits::eof(docs->value()));
    ASSERT_TRUE(irs::doc_limits::eof(docs_seek->seek(irs::doc_limits::eof())));
  }

  // "qui% bro% fox"
  {
    irs::bytes_ref actual_value;

    irs::by_phrase q;
    *q.mutable_field() = "phrase_anl";
    auto& wt1 = q.mutable_options()->push_back<irs::by_wildcard_options>();
    wt1.term = irs::ref_cast<irs::byte_type>(irs::string_ref("qui%"));
    auto& wt2 = q.mutable_options()->push_back<irs::by_wildcard_options>();
    wt2.term = irs::ref_cast<irs::byte_type>(irs::string_ref("bro%"));
    q.mutable_options()->push_back<irs::by_term_options>().term = irs::ref_cast<irs::byte_type>(irs::string_ref("fox"));

    auto prepared = q.prepare(rdr);
    auto sub = rdr.begin();
    auto column = sub->column_reader("name");
    ASSERT_NE(nullptr, column);
    auto values = column->values();

    auto docs = prepared->execute(*sub);
    ASSERT_FALSE(irs::get<irs::filter_boost>(*docs));
    ASSERT_FALSE(irs::get<irs::frequency>(*docs));
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::valid(docs->value()));
    auto docs_seek = prepared->execute(*sub);
    ASSERT_FALSE(irs::doc_limits::valid(docs_seek->value()));

    ASSERT_TRUE(docs->next());
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("A", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("A", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("G", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("G", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("I", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("I", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("S", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("S", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("T", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("T", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("V", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("V", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_FALSE(docs->next());
    ASSERT_TRUE(irs::doc_limits::eof(docs->value()));
    ASSERT_TRUE(irs::doc_limits::eof(docs_seek->seek(irs::doc_limits::eof())));
  }

  // "qui% b%o__ fox"
  {
    irs::bytes_ref actual_value;

    irs::by_phrase q;
    *q.mutable_field() = "phrase_anl";
    auto& wt1 = q.mutable_options()->push_back<irs::by_wildcard_options>();
    wt1.term = irs::ref_cast<irs::byte_type>(irs::string_ref("qui%"));
    auto& wt2 = q.mutable_options()->push_back<irs::by_wildcard_options>();
    wt2.term = irs::ref_cast<irs::byte_type>(irs::string_ref("b%o__"));
    q.mutable_options()->push_back<irs::by_term_options>().term = irs::ref_cast<irs::byte_type>(irs::string_ref("fox"));

    auto prepared = q.prepare(rdr);
    auto sub = rdr.begin();
    auto column = sub->column_reader("name");
    ASSERT_NE(nullptr, column);
    auto values = column->values();

    auto docs = prepared->execute(*sub);
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::valid(docs->value()));
    auto docs_seek = prepared->execute(*sub);
    ASSERT_FALSE(irs::doc_limits::valid(docs_seek->value()));

    ASSERT_TRUE(docs->next());
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("A", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("A", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("G", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("G", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("I", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("I", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("S", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("S", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_FALSE(docs->next());
    ASSERT_TRUE(irs::doc_limits::eof(docs->value()));
    ASSERT_TRUE(irs::doc_limits::eof(docs_seek->seek(irs::doc_limits::eof())));
  }

  // "qui bro fox"
  {
    irs::bytes_ref actual_value;

    irs::by_phrase q;
    *q.mutable_field() = "phrase_anl";
    auto& lt1 = q.mutable_options()->push_back<irs::by_edit_distance_filter_options>();
    lt1.max_distance = 2;
    lt1.term = irs::ref_cast<irs::byte_type>(irs::string_ref("qui"));
    auto& lt2 = q.mutable_options()->push_back<irs::by_edit_distance_filter_options>();
    lt2.max_distance = 1;
    lt2.term = irs::ref_cast<irs::byte_type>(irs::string_ref("brow"));
    q.mutable_options()->push_back<irs::by_term_options>().term = irs::ref_cast<irs::byte_type>(irs::string_ref("fox"));

    auto prepared = q.prepare(rdr);
    auto sub = rdr.begin();
    auto column = sub->column_reader("name");
    ASSERT_NE(nullptr, column);
    auto values = column->values();

    auto docs = prepared->execute(*sub);
    ASSERT_FALSE(irs::get<irs::filter_boost>(*docs));
    ASSERT_FALSE(irs::get<irs::frequency>(*docs));
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::valid(docs->value()));
    auto docs_seek = prepared->execute(*sub);
    ASSERT_FALSE(irs::doc_limits::valid(docs_seek->value()));

    ASSERT_TRUE(docs->next());
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("A", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("A", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("G", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("G", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("I", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("I", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("S", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("S", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_FALSE(docs->next());
    ASSERT_TRUE(irs::doc_limits::eof(docs->value()));
    ASSERT_TRUE(irs::doc_limits::eof(docs_seek->seek(irs::doc_limits::eof())));
  }

  // "[x0, x1] [x0, x1] x2"
  {
    irs::bytes_ref actual_value;

    irs::by_phrase q;
    *q.mutable_field() = "phrase_anl";
    auto& rt1 = q.mutable_options()->push_back<irs::by_range_options>();
    rt1.range.min = irs::ref_cast<irs::byte_type>(irs::string_ref("x0"));
    rt1.range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("x1"));
    rt1.range.min_type = irs::BoundType::INCLUSIVE;
    rt1.range.max_type = irs::BoundType::INCLUSIVE;
    auto& rt2 = q.mutable_options()->push_back<irs::by_range_options>();
    rt2.range.min = irs::ref_cast<irs::byte_type>(irs::string_ref("x0"));
    rt2.range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("x1"));
    rt2.range.min_type = irs::BoundType::INCLUSIVE;
    rt2.range.max_type = irs::BoundType::INCLUSIVE;
    q.mutable_options()->push_back<irs::by_term_options>().term = irs::ref_cast<irs::byte_type>(irs::string_ref("x2"));

    auto prepared = q.prepare(rdr);
    auto sub = rdr.begin();
    auto column = sub->column_reader("name");
    ASSERT_NE(nullptr, column);
    auto values = column->values();

    auto docs = prepared->execute(*sub);
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::valid(docs->value()));
    auto docs_seek = prepared->execute(*sub);
    ASSERT_FALSE(irs::doc_limits::valid(docs_seek->value()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("X4", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("X4", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_FALSE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(irs::doc_limits::eof(docs->value()));
  }

  // "qui* brown fo*"
  {
    irs::bytes_ref actual_value;

    irs::by_phrase q;
    *q.mutable_field() = "phrase_anl";
    auto& pt1 = q.mutable_options()->push_back<irs::by_prefix_options>();
    pt1.term = irs::ref_cast<irs::byte_type>(irs::string_ref("qui"));
    q.mutable_options()->push_back<irs::by_term_options>().term = irs::ref_cast<irs::byte_type>(irs::string_ref("brown"));
    auto& pt2 = q.mutable_options()->push_back<irs::by_prefix_options>();
    pt2.term = irs::ref_cast<irs::byte_type>(irs::string_ref("fo"));

    auto prepared = q.prepare(rdr);
    auto sub = rdr.begin();
    auto column = sub->column_reader("name");
    ASSERT_NE(nullptr, column);
    auto values = column->values();

    auto docs = prepared->execute(*sub);
    ASSERT_FALSE(irs::get<irs::filter_boost>(*docs));
    ASSERT_FALSE(irs::get<irs::frequency>(*docs));
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::valid(docs->value()));
    auto docs_seek = prepared->execute(*sub);
    ASSERT_FALSE(irs::doc_limits::valid(docs_seek->value()));

    ASSERT_TRUE(docs->next());
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("A", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("A", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("G", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("G", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("I", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("I", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("S", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("S", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("U", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("U", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("W", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("W", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_FALSE(docs->next());
    ASSERT_TRUE(irs::doc_limits::eof(docs->value()));
    ASSERT_TRUE(irs::doc_limits::eof(docs_seek->seek(irs::doc_limits::eof())));
  }

  // "qui% brown fo%"
  {
    irs::bytes_ref actual_value;

    irs::by_phrase q;
    *q.mutable_field() = "phrase_anl";
    auto& wt1 = q.mutable_options()->push_back<irs::by_wildcard_options>();
    wt1.term = irs::ref_cast<irs::byte_type>(irs::string_ref("qui%"));
    q.mutable_options()->push_back<irs::by_term_options>().term = irs::ref_cast<irs::byte_type>(irs::string_ref("brown"));
    auto& wt2 = q.mutable_options()->push_back<irs::by_wildcard_options>();
    wt2.term = irs::ref_cast<irs::byte_type>(irs::string_ref("fo%"));

    auto prepared = q.prepare(rdr);
    auto sub = rdr.begin();
    auto column = sub->column_reader("name");
    ASSERT_NE(nullptr, column);
    auto values = column->values();

    auto docs = prepared->execute(*sub);
    ASSERT_FALSE(irs::get<irs::filter_boost>(*docs));
    ASSERT_FALSE(irs::get<irs::frequency>(*docs));
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::valid(docs->value()));
    auto docs_seek = prepared->execute(*sub);
    ASSERT_FALSE(irs::doc_limits::valid(docs_seek->value()));

    ASSERT_TRUE(docs->next());
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("A", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("A", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("G", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("G", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("I", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("I", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("S", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("S", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("U", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("U", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("W", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("W", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_FALSE(docs->next());
    ASSERT_TRUE(irs::doc_limits::eof(docs->value()));
    ASSERT_TRUE(irs::doc_limits::eof(docs_seek->seek(irs::doc_limits::eof())));
  }

  // "q_i% brown f%x"
  {
    irs::bytes_ref actual_value;

    irs::by_phrase q;
    *q.mutable_field() = "phrase_anl";
    auto& wt1 = q.mutable_options()->push_back<irs::by_wildcard_options>();
    wt1.term = irs::ref_cast<irs::byte_type>(irs::string_ref("q_i%"));
    q.mutable_options()->push_back<irs::by_term_options>().term = irs::ref_cast<irs::byte_type>(irs::string_ref("brown"));
    auto& wt2 = q.mutable_options()->push_back<irs::by_wildcard_options>();
    wt2.term = irs::ref_cast<irs::byte_type>(irs::string_ref("f%x"));

    auto prepared = q.prepare(rdr);
    auto sub = rdr.begin();
    auto column = sub->column_reader("name");
    ASSERT_NE(nullptr, column);
    auto values = column->values();

    auto docs = prepared->execute(*sub);
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::valid(docs->value()));
    auto docs_seek = prepared->execute(*sub);
    ASSERT_FALSE(irs::doc_limits::valid(docs_seek->value()));

    ASSERT_TRUE(docs->next());
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("A", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("A", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("G", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("G", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("I", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("I", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("S", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("S", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_FALSE(docs->next());
    ASSERT_TRUE(irs::doc_limits::eof(docs->value()));
    ASSERT_TRUE(irs::doc_limits::eof(docs_seek->seek(irs::doc_limits::eof())));
  }

  // "[x0, x1] x0 [x1, x2]"
  {
    irs::bytes_ref actual_value;

    irs::by_phrase q;
    *q.mutable_field() = "phrase_anl";
    auto& rt1 = q.mutable_options()->push_back<irs::by_range_options>();
    rt1.range.min = irs::ref_cast<irs::byte_type>(irs::string_ref("x0"));
    rt1.range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("x1"));
    rt1.range.min_type = irs::BoundType::INCLUSIVE;
    rt1.range.max_type = irs::BoundType::INCLUSIVE;
    q.mutable_options()->push_back<irs::by_term_options>().term = irs::ref_cast<irs::byte_type>(irs::string_ref("x0"));
    auto& rt2 = q.mutable_options()->push_back<irs::by_range_options>();
    rt2.range.min = irs::ref_cast<irs::byte_type>(irs::string_ref("x1"));
    rt2.range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("x2"));
    rt2.range.min_type = irs::BoundType::INCLUSIVE;
    rt2.range.max_type = irs::BoundType::INCLUSIVE;

    auto prepared = q.prepare(rdr);
    auto sub = rdr.begin();
    auto column = sub->column_reader("name");
    ASSERT_NE(nullptr, column);
    auto values = column->values();

    auto docs = prepared->execute(*sub);
    ASSERT_FALSE(irs::get<irs::filter_boost>(*docs));
    ASSERT_FALSE(irs::get<irs::frequency>(*docs));
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::valid(docs->value()));
    auto docs_seek = prepared->execute(*sub);
    ASSERT_FALSE(irs::doc_limits::valid(docs_seek->value()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("X4", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("X4", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_FALSE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(irs::doc_limits::eof(docs->value()));
  }

  // "qoick br__nn fix"
  {
    irs::bytes_ref actual_value;

    irs::by_phrase q;
    *q.mutable_field() = "phrase_anl";
    auto& lt1 = q.mutable_options()->push_back<irs::by_edit_distance_filter_options>();
    lt1.max_distance = 1;
    lt1.term = irs::ref_cast<irs::byte_type>(irs::string_ref("qoick"));
    auto& wt = q.mutable_options()->push_back<irs::by_wildcard_options>();
    wt.term = irs::ref_cast<irs::byte_type>(irs::string_ref("br__n"));
    auto& lt2 = q.mutable_options()->push_back<irs::by_edit_distance_filter_options>();
    lt2.max_distance = 1;
    lt2.term = irs::ref_cast<irs::byte_type>(irs::string_ref("fix"));

    auto prepared = q.prepare(rdr);
    auto sub = rdr.begin();
    auto column = sub->column_reader("name");
    ASSERT_NE(nullptr, column);
    auto values = column->values();

    auto docs = prepared->execute(*sub);
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::valid(docs->value()));
    auto docs_seek = prepared->execute(*sub);
    ASSERT_FALSE(irs::doc_limits::valid(docs_seek->value()));

    ASSERT_TRUE(docs->next());
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("A", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("A", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("G", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("G", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("I", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("I", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_FALSE(docs->next());
    ASSERT_TRUE(irs::doc_limits::eof(docs->value()));
    ASSERT_TRUE(irs::doc_limits::eof(docs_seek->seek(irs::doc_limits::eof())));
  }

  // "quick bro* fo*"
  {
    irs::bytes_ref actual_value;

    irs::by_phrase q;
    *q.mutable_field() = "phrase_anl";
    q.mutable_options()->push_back<irs::by_term_options>().term = irs::ref_cast<irs::byte_type>(irs::string_ref("quick"));
    auto& pt1 = q.mutable_options()->push_back<irs::by_prefix_options>();
    pt1.term = irs::ref_cast<irs::byte_type>(irs::string_ref("bro"));
    auto& pt2 = q.mutable_options()->push_back<irs::by_prefix_options>();
    pt2.term = irs::ref_cast<irs::byte_type>(irs::string_ref("fo"));

    auto prepared = q.prepare(rdr);
    auto sub = rdr.begin();
    auto column = sub->column_reader("name");
    ASSERT_NE(nullptr, column);
    auto values = column->values();

    auto docs = prepared->execute(*sub);
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::valid(docs->value()));
    auto docs_seek = prepared->execute(*sub);
    ASSERT_FALSE(irs::doc_limits::valid(docs_seek->value()));

    ASSERT_TRUE(docs->next());
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("A", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("A", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("G", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("G", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("I", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("I", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("T", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("T", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("U", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("U", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("X", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("X", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_FALSE(docs->next());
    ASSERT_TRUE(irs::doc_limits::eof(docs->value()));
    ASSERT_TRUE(irs::doc_limits::eof(docs_seek->seek(irs::doc_limits::eof())));
  }

  // "quick bro% fo%"
  {
    irs::bytes_ref actual_value;

    irs::by_phrase q;
    *q.mutable_field() = "phrase_anl";
    q.mutable_options()->push_back<irs::by_term_options>().term = irs::ref_cast<irs::byte_type>(irs::string_ref("quick"));
    auto& wt1 = q.mutable_options()->push_back<irs::by_wildcard_options>();
    wt1.term = irs::ref_cast<irs::byte_type>(irs::string_ref("bro%"));
    auto& wt2 = q.mutable_options()->push_back<irs::by_wildcard_options>();
    wt2.term = irs::ref_cast<irs::byte_type>(irs::string_ref("fo%"));

    auto prepared = q.prepare(rdr);
    auto sub = rdr.begin();
    auto column = sub->column_reader("name");
    ASSERT_NE(nullptr, column);
    auto values = column->values();

    auto docs = prepared->execute(*sub);
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::valid(docs->value()));
    auto docs_seek = prepared->execute(*sub);
    ASSERT_FALSE(irs::doc_limits::valid(docs_seek->value()));

    ASSERT_TRUE(docs->next());
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("A", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("A", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("G", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("G", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("I", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("I", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("T", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("T", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("U", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("U", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("X", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("X", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_FALSE(docs->next());
    ASSERT_TRUE(irs::doc_limits::eof(docs->value()));
    ASSERT_TRUE(irs::doc_limits::eof(docs_seek->seek(irs::doc_limits::eof())));
  }

  // "quick b_o% f_%"
  {
    irs::bytes_ref actual_value;

    irs::by_phrase q;
    *q.mutable_field() = "phrase_anl";
    q.mutable_options()->push_back<irs::by_term_options>().term = irs::ref_cast<irs::byte_type>(irs::string_ref("quick"));
    auto& wt1 = q.mutable_options()->push_back<irs::by_wildcard_options>();
    wt1.term = irs::ref_cast<irs::byte_type>(irs::string_ref("b_o%"));
    auto& wt2 = q.mutable_options()->push_back<irs::by_wildcard_options>();
    wt2.term = irs::ref_cast<irs::byte_type>(irs::string_ref("f_%"));

    auto prepared = q.prepare(rdr);
    auto sub = rdr.begin();
    auto column = sub->column_reader("name");
    ASSERT_NE(nullptr, column);
    auto values = column->values();

    auto docs = prepared->execute(*sub);
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::valid(docs->value()));
    auto docs_seek = prepared->execute(*sub);
    ASSERT_FALSE(irs::doc_limits::valid(docs_seek->value()));

    ASSERT_TRUE(docs->next());
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("A", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("A", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("G", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("G", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("I", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("I", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("T", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("T", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("U", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("U", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("X", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("X", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_FALSE(docs->next());
    ASSERT_TRUE(irs::doc_limits::eof(docs->value()));
    ASSERT_TRUE(irs::doc_limits::eof(docs_seek->seek(irs::doc_limits::eof())));
  }

  // "x1 [x0, x1] [x1, x2]"
  {
    irs::bytes_ref actual_value;

    irs::by_phrase q;
    *q.mutable_field() = "phrase_anl";
    q.mutable_options()->push_back<irs::by_term_options>().term = irs::ref_cast<irs::byte_type>(irs::string_ref("x1"));
    auto& rt1 = q.mutable_options()->push_back<irs::by_range_options>();
    rt1.range.min = irs::ref_cast<irs::byte_type>(irs::string_ref("x0"));
    rt1.range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("x1"));
    rt1.range.min_type = irs::BoundType::INCLUSIVE;
    rt1.range.max_type = irs::BoundType::INCLUSIVE;
    auto& rt2 = q.mutable_options()->push_back<irs::by_range_options>();
    rt2.range.min = irs::ref_cast<irs::byte_type>(irs::string_ref("x1"));
    rt2.range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("x2"));
    rt2.range.min_type = irs::BoundType::INCLUSIVE;
    rt2.range.max_type = irs::BoundType::INCLUSIVE;

    auto prepared = q.prepare(rdr);
    auto sub = rdr.begin();
    auto column = sub->column_reader("name");
    ASSERT_NE(nullptr, column);
    auto values = column->values();

    auto docs = prepared->execute(*sub);
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::valid(docs->value()));
    auto docs_seek = prepared->execute(*sub);
    ASSERT_FALSE(irs::doc_limits::valid(docs_seek->value()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("X4", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("X4", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_FALSE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(irs::doc_limits::eof(docs->value()));
  }

  // "qui* bro* fo*"
  {
    irs::bytes_ref actual_value;

    irs::by_phrase q;
    *q.mutable_field() = "phrase_anl";
    auto& pt1 = q.mutable_options()->push_back<irs::by_prefix_options>();
    auto& pt2 = q.mutable_options()->push_back<irs::by_prefix_options>();
    auto& pt3 = q.mutable_options()->push_back<irs::by_prefix_options>();
    pt1.term = irs::ref_cast<irs::byte_type>(irs::string_ref("qui"));
    pt2.term = irs::ref_cast<irs::byte_type>(irs::string_ref("bro"));
    pt3.term = irs::ref_cast<irs::byte_type>(irs::string_ref("fo"));

    auto prepared = q.prepare(rdr);
    auto sub = rdr.begin();
    auto column = sub->column_reader("name");
    ASSERT_NE(nullptr, column);
    auto values = column->values();

    auto docs = prepared->execute(*sub);
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::valid(docs->value()));
    auto docs_seek = prepared->execute(*sub);
    ASSERT_FALSE(irs::doc_limits::valid(docs_seek->value()));

    ASSERT_TRUE(docs->next());
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("A", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("A", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("G", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("G", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("I", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("I", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("S", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("S", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("T", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("T", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("U", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("U", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("V", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("V", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("W", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("W", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("X", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("X", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("Y", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("Y", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_FALSE(docs->next());
    ASSERT_TRUE(irs::doc_limits::eof(docs->value()));
    ASSERT_TRUE(irs::doc_limits::eof(docs_seek->seek(irs::doc_limits::eof())));
  }

  // "qui% bro% fo%"
  {
    irs::bytes_ref actual_value;

    irs::by_phrase q;
    *q.mutable_field() = "phrase_anl";
    auto& wt1 = q.mutable_options()->push_back<irs::by_wildcard_options>();
    auto& wt2 = q.mutable_options()->push_back<irs::by_wildcard_options>();
    auto& wt3 = q.mutable_options()->push_back<irs::by_wildcard_options>();
    wt1.term = irs::ref_cast<irs::byte_type>(irs::string_ref("qui%"));
    wt2.term = irs::ref_cast<irs::byte_type>(irs::string_ref("bro%"));
    wt3.term = irs::ref_cast<irs::byte_type>(irs::string_ref("fo%"));

    size_t collect_field_count = 0;
    size_t collect_term_count = 0;
    size_t finish_count = 0;
    irs::order ord;
    auto& sort = ord.add<tests::sort::custom_sort>(false);

    sort.collector_collect_field = [&collect_field_count](
        const irs::sub_reader&, const irs::term_reader&)->void{
      ++collect_field_count;
    };
    sort.collector_collect_term = [&collect_term_count](
        const irs::sub_reader&,
        const irs::term_reader&,
        const irs::attribute_provider&)->void{
      ++collect_term_count;
    };
    sort.collectors_collect_ = [&finish_count](
        irs::byte_type*,
        const irs::index_reader&,
        const irs::sort::field_collector*,
        const irs::sort::term_collector*)->void {
      ++finish_count;
    };
    sort.prepare_field_collector_ = [&sort]()->irs::sort::field_collector::ptr {
      return irs::memory::make_unique<tests::sort::custom_sort::prepared::field_collector>(sort);
    };
    sort.prepare_term_collector_ = [&sort]()->irs::sort::term_collector::ptr {
      return irs::memory::make_unique<tests::sort::custom_sort::prepared::term_collector>(sort);
    };
    sort.scorer_add = [](irs::doc_id_t& dst, const irs::doc_id_t& src)->void {
      ASSERT_TRUE(
        irs::doc_limits::invalid() == dst
        || dst == src
      );
      dst = src;
    };

    auto pord = ord.prepare();
    auto prepared = q.prepare(rdr, pord);
    ASSERT_EQ(1, collect_field_count); // 1 field in 1 segment
    ASSERT_EQ(6, collect_term_count); // 6 different terms
    ASSERT_EQ(6, finish_count); // 6 sub-terms in phrase

    auto sub = rdr.begin();
    auto column = sub->column_reader("name");
    ASSERT_NE(nullptr, column);
    auto values = column->values();

    // no order passed - no frequency
    {
      auto docs = prepared->execute(*sub);
      ASSERT_FALSE(irs::get<irs::frequency>(*docs));
      ASSERT_FALSE(irs::get<irs::filter_boost>(*docs));
    }

    auto docs = prepared->execute(*sub, pord);
    auto* freq = irs::get<irs::frequency>(*docs);
    ASSERT_TRUE(freq);
    ASSERT_FALSE(irs::get<irs::filter_boost>(*docs));
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::valid(docs->value()));
    auto docs_seek = prepared->execute(*sub, pord);
    ASSERT_FALSE(irs::doc_limits::valid(docs_seek->value()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(1, freq->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("A", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(freq->value, irs::get<irs::frequency>(*docs_seek)->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("A", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(1, freq->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("G", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(freq->value, irs::get<irs::frequency>(*docs_seek)->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("G", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(1, freq->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("I", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(freq->value, irs::get<irs::frequency>(*docs_seek)->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("I", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(1, freq->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("S", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(freq->value, irs::get<irs::frequency>(*docs_seek)->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("S", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(1, freq->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("T", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(freq->value, irs::get<irs::frequency>(*docs_seek)->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("T", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(1, freq->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("U", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(freq->value, irs::get<irs::frequency>(*docs_seek)->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("U", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(1, freq->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("V", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(freq->value, irs::get<irs::frequency>(*docs_seek)->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("V", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(1, freq->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("W", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(freq->value, irs::get<irs::frequency>(*docs_seek)->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("W", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(1, freq->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("X", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(freq->value, irs::get<irs::frequency>(*docs_seek)->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("X", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(1, freq->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("Y", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(freq->value, irs::get<irs::frequency>(*docs_seek)->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("Y", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_FALSE(docs->next());
    ASSERT_TRUE(irs::doc_limits::eof(docs->value()));
    ASSERT_TRUE(irs::doc_limits::eof(docs_seek->seek(irs::doc_limits::eof())));
  }

  // "q%ic_ br_wn _%x"
  {
    irs::bytes_ref actual_value;

    irs::by_phrase q;
    *q.mutable_field() = "phrase_anl";
    auto& wt1 = q.mutable_options()->push_back<irs::by_wildcard_options>();
    auto& wt2 = q.mutable_options()->push_back<irs::by_wildcard_options>();
    auto& wt3 = q.mutable_options()->push_back<irs::by_wildcard_options>();
    wt1.term = irs::ref_cast<irs::byte_type>(irs::string_ref("q%ic_"));
    wt2.term = irs::ref_cast<irs::byte_type>(irs::string_ref("br_wn"));
    wt3.term = irs::ref_cast<irs::byte_type>(irs::string_ref("_%x"));

    auto prepared = q.prepare(rdr);

    auto sub = rdr.begin();
    auto column = sub->column_reader("name");
    ASSERT_NE(nullptr, column);
    auto values = column->values();

    auto docs = prepared->execute(*sub);
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::valid(docs->value()));
    auto docs_seek = prepared->execute(*sub);
    ASSERT_FALSE(irs::doc_limits::valid(docs_seek->value()));

    ASSERT_TRUE(docs->next());
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("A", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("A", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("G", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("G", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("I", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("I", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_FALSE(docs->next());
    ASSERT_TRUE(irs::doc_limits::eof(docs->value()));
    ASSERT_TRUE(irs::doc_limits::eof(docs_seek->seek(irs::doc_limits::eof())));
  }

  // "quick|quilt|hhh brown|brother fox"
  {
    irs::bytes_ref actual_value;
    irs::by_phrase q;
    *q.mutable_field() = "phrase_anl";
    auto& st1 = q.mutable_options()->push_back<irs::by_terms_options>();
    st1.terms.emplace(irs::ref_cast<irs::byte_type>(irs::string_ref("quick")));
    st1.terms.emplace(irs::ref_cast<irs::byte_type>(irs::string_ref("quilt")));
    st1.terms.emplace(irs::ref_cast<irs::byte_type>(irs::string_ref("hhh")));
    auto& st2 = q.mutable_options()->push_back<irs::by_terms_options>();
    st2.terms.emplace(irs::ref_cast<irs::byte_type>(irs::string_ref("brown")));
    st2.terms.emplace(irs::ref_cast<irs::byte_type>(irs::string_ref("brother")));
    auto& st3 = q.mutable_options()->push_back<irs::by_terms_options>();
    st3.terms.emplace(irs::ref_cast<irs::byte_type>(irs::string_ref("fox")));

    auto prepared = q.prepare(rdr);
    auto sub = rdr.begin();
    auto column = sub->column_reader("name");
    ASSERT_NE(nullptr, column);
    auto values = column->values();

    auto docs = prepared->execute(*sub);
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::valid(docs->value()));
    auto docs_seek = prepared->execute(*sub);
    ASSERT_FALSE(irs::doc_limits::valid(docs_seek->value()));

    ASSERT_TRUE(docs->next());
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("A", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("A", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("G", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("G", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("I", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("I", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("S", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("S", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("T", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("T", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("V", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("V", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_FALSE(docs->next());
    ASSERT_TRUE(irs::doc_limits::eof(docs->value()));
    ASSERT_TRUE(irs::doc_limits::eof(docs_seek->seek(irs::doc_limits::eof())));
  }

  // "[x0, x1] [x0, x1] [x1, x2]"
  {
    irs::bytes_ref actual_value;

    irs::by_phrase q;
    *q.mutable_field() = "phrase_anl";
    auto& rt1 = q.mutable_options()->push_back<irs::by_range_options>();
    auto& rt2 = q.mutable_options()->push_back<irs::by_range_options>();
    auto& rt3 = q.mutable_options()->push_back<irs::by_range_options>();
    rt1.range.min = irs::ref_cast<irs::byte_type>(irs::string_ref("x0"));
    rt1.range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("x1"));
    rt1.range.min_type = irs::BoundType::INCLUSIVE;
    rt1.range.max_type = irs::BoundType::INCLUSIVE;
    rt2.range.min = irs::ref_cast<irs::byte_type>(irs::string_ref("x0"));
    rt2.range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("x1"));
    rt2.range.min_type = irs::BoundType::INCLUSIVE;
    rt2.range.max_type = irs::BoundType::INCLUSIVE;
    rt3.range.min = irs::ref_cast<irs::byte_type>(irs::string_ref("x1"));
    rt3.range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("x2"));
    rt3.range.min_type = irs::BoundType::INCLUSIVE;
    rt3.range.max_type = irs::BoundType::INCLUSIVE;

    auto prepared = q.prepare(rdr);
    auto sub = rdr.begin();
    auto column = sub->column_reader("name");
    ASSERT_NE(nullptr, column);
    auto values = column->values();

    auto docs = prepared->execute(*sub);
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::valid(docs->value()));
    auto docs_seek = prepared->execute(*sub);
    ASSERT_FALSE(irs::doc_limits::valid(docs_seek->value()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("X4", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("X4", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_FALSE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(irs::doc_limits::eof(docs->value()));
  }

  // "quick brown fox" with order
  {
    irs::bytes_ref actual_value;

    irs::by_phrase q;
    *q.mutable_field() = "phrase_anl";
    q.mutable_options()->push_back<irs::by_term_options>().term = irs::ref_cast<irs::byte_type>(irs::string_ref("quick"));
    q.mutable_options()->push_back<irs::by_term_options>().term = irs::ref_cast<irs::byte_type>(irs::string_ref("brown"));
    q.mutable_options()->push_back<irs::by_term_options>().term = irs::ref_cast<irs::byte_type>(irs::string_ref("fox"));

    size_t collect_field_count = 0;
    size_t collect_term_count = 0;
    size_t finish_count = 0;
    irs::order ord;
    auto& sort = ord.add<tests::sort::custom_sort>(false);

    sort.collector_collect_field = [&collect_field_count](
        const irs::sub_reader&, const irs::term_reader&)->void{
      ++collect_field_count;
    };
    sort.collector_collect_term = [&collect_term_count](
        const irs::sub_reader&,
        const irs::term_reader&,
        const irs::attribute_provider&)->void{
      ++collect_term_count;
    };
    sort.collectors_collect_ = [&finish_count](
        irs::byte_type*,
        const irs::index_reader&,
        const irs::sort::field_collector*,
        const irs::sort::term_collector*)->void {
      ++finish_count;
    };
    sort.prepare_field_collector_ = [&sort]()->irs::sort::field_collector::ptr {
      return irs::memory::make_unique<tests::sort::custom_sort::prepared::field_collector>(sort);
    };
    sort.prepare_term_collector_ = [&sort]()->irs::sort::term_collector::ptr {
      return irs::memory::make_unique<tests::sort::custom_sort::prepared::term_collector>(sort);
    };
    sort.scorer_add = [](irs::doc_id_t& dst, const irs::doc_id_t& src)->void {
      ASSERT_TRUE(
        irs::doc_limits::invalid() == dst
        || dst == src
      );
      dst = src;
    };

    auto pord = ord.prepare();
    auto prepared = q.prepare(rdr, pord);
    ASSERT_EQ(1, collect_field_count); // 1 field in 1 segment
    ASSERT_EQ(3, collect_term_count); // 3 different terms
    ASSERT_EQ(3, finish_count); // 3 sub-terms in phrase
    auto sub = rdr.begin();

    // no order passed - no frequency
    {
      auto docs = prepared->execute(*sub);
      ASSERT_FALSE(irs::get<irs::frequency>(*docs));
      ASSERT_FALSE(irs::get<irs::filter_boost>(*docs));
    }

    auto column = sub->column_reader("name");
    ASSERT_NE(nullptr, column);
    auto values = column->values();
    auto docs = prepared->execute(*sub, pord);
    auto* freq = irs::get<irs::frequency>(*docs);
    ASSERT_TRUE(freq);
    ASSERT_FALSE(irs::get<irs::filter_boost>(*docs));
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::valid(docs->value()));
    auto docs_seek = prepared->execute(*sub, pord);
    ASSERT_FALSE(irs::doc_limits::valid(docs_seek->value()));
    auto* score = irs::get<irs::score>(*docs);
    ASSERT_FALSE(!score);

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(1, freq->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("A", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(freq->value, irs::get<irs::frequency>(*docs_seek)->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("A", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(1, freq->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("G", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(freq->value, irs::get<irs::frequency>(*docs_seek)->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("G", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(1, freq->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("I", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(freq->value, irs::get<irs::frequency>(*docs_seek)->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("I", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_FALSE(docs->next());
    ASSERT_TRUE(irs::doc_limits::eof(docs->value()));
    ASSERT_TRUE(irs::doc_limits::eof(docs_seek->seek(irs::doc_limits::eof())));
  }
}

TEST_P(phrase_filter_test_case, sequential_several_terms) {
  // add segment
  {
    tests::json_doc_generator gen(
      resource("phrase_sequential.json"),
      &tests::analyzed_json_field_factory);
    add_segment(gen);
  }

  // read segment
  auto rdr = open_reader();

  // "fox ... quick"
  {
    irs::bytes_ref actual_value;

    irs::by_phrase q;
    *q.mutable_field() = "phrase_anl";
    q.mutable_options()->push_back<irs::by_term_options>().term = irs::ref_cast<irs::byte_type>(irs::string_ref("fox"));
    q.mutable_options()->push_back<irs::by_term_options>(1).term = irs::ref_cast<irs::byte_type>(irs::string_ref("quick"));

    auto prepared = q.prepare(rdr);

    auto sub = rdr.begin();
    auto column = sub->column_reader("name");
    ASSERT_NE(nullptr, column);
    auto values = column->values();
    auto docs = prepared->execute(*sub);
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::valid(docs->value()));
    auto docs_seek = prepared->execute(*sub);
    ASSERT_FALSE(irs::doc_limits::valid(docs_seek->value()));

    ASSERT_TRUE(docs->next());
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("L", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("L", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("N", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("N", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_FALSE(docs->next());
    ASSERT_TRUE(irs::doc_limits::eof(docs->value()));
    ASSERT_TRUE(irs::doc_limits::eof(docs_seek->seek(irs::doc_limits::eof())));
  }

  // "fo* ... quick"
  {
    irs::bytes_ref actual_value;

    irs::by_phrase q;
    *q.mutable_field() = "phrase_anl";
    auto& pt = q.mutable_options()->push_back<irs::by_prefix_options>();
    pt.term = irs::ref_cast<irs::byte_type>(irs::string_ref("fo"));
    q.mutable_options()->push_back<irs::by_term_options>(1).term = irs::ref_cast<irs::byte_type>(irs::string_ref("quick"));

    auto prepared = q.prepare(rdr);

    auto sub = rdr.begin();
    auto column = sub->column_reader("name");
    ASSERT_NE(nullptr, column);
    auto values = column->values();
    auto docs = prepared->execute(*sub);
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::valid(docs->value()));
    auto docs_seek = prepared->execute(*sub);
    ASSERT_FALSE(irs::doc_limits::valid(docs_seek->value()));

    ASSERT_TRUE(docs->next());
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("L", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("L", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("N", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("N", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_FALSE(docs->next());
    ASSERT_TRUE(irs::doc_limits::eof(docs->value()));
    ASSERT_TRUE(irs::doc_limits::eof(docs_seek->seek(irs::doc_limits::eof())));
  }

  // "f_x ... quick"
  {
    irs::bytes_ref actual_value;

    irs::by_phrase q;
    *q.mutable_field() = "phrase_anl";
    auto& wt = q.mutable_options()->push_back<irs::by_wildcard_options>();
    wt.term = irs::ref_cast<irs::byte_type>(irs::string_ref("f_x"));
    q.mutable_options()->push_back<irs::by_term_options>(1).term = irs::ref_cast<irs::byte_type>(irs::string_ref("quick"));

    auto prepared = q.prepare(rdr);

    auto sub = rdr.begin();
    auto column = sub->column_reader("name");
    ASSERT_NE(nullptr, column);
    auto values = column->values();
    auto docs = prepared->execute(*sub);
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::valid(docs->value()));
    auto docs_seek = prepared->execute(*sub);
    ASSERT_FALSE(irs::doc_limits::valid(docs_seek->value()));

    ASSERT_TRUE(docs->next());
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("L", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("L", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("N", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("N", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_FALSE(docs->next());
    ASSERT_TRUE(irs::doc_limits::eof(docs->value()));
    ASSERT_TRUE(irs::doc_limits::eof(docs_seek->seek(irs::doc_limits::eof())));
  }

  // "fpx ... quick"
  {
    irs::bytes_ref actual_value;

    irs::by_phrase q;
    *q.mutable_field() = "phrase_anl";
    auto& lt = q.mutable_options()->push_back<irs::by_edit_distance_filter_options>();
    lt.max_distance = 1;
    lt.term = irs::ref_cast<irs::byte_type>(irs::string_ref("fpx"));
    q.mutable_options()->push_back<irs::by_term_options>(1).term = irs::ref_cast<irs::byte_type>(irs::string_ref("quick"));

    auto prepared = q.prepare(rdr);

    auto sub = rdr.begin();
    auto column = sub->column_reader("name");
    ASSERT_NE(nullptr, column);
    auto values = column->values();
    auto docs = prepared->execute(*sub);
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::valid(docs->value()));
    auto docs_seek = prepared->execute(*sub);
    ASSERT_FALSE(irs::doc_limits::valid(docs_seek->value()));

    ASSERT_TRUE(docs->next());
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("L", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("L", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("N", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("N", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_FALSE(docs->next());
    ASSERT_TRUE(irs::doc_limits::eof(docs->value()));
    ASSERT_TRUE(irs::doc_limits::eof(docs_seek->seek(irs::doc_limits::eof())));
  }

  // "fox ... qui*"
  {
    irs::bytes_ref actual_value;

    irs::by_phrase q;
    *q.mutable_field() = "phrase_anl";
    q.mutable_options()->push_back<irs::by_term_options>().term = irs::ref_cast<irs::byte_type>(irs::string_ref("fox"));
    auto& pt = q.mutable_options()->push_back<irs::by_prefix_options>(1);
    pt.term = irs::ref_cast<irs::byte_type>(irs::string_ref("qui"));

    auto prepared = q.prepare(rdr);

    auto sub = rdr.begin();
    auto column = sub->column_reader("name");
    ASSERT_NE(nullptr, column);
    auto values = column->values();
    auto docs = prepared->execute(*sub);
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::valid(docs->value()));
    auto docs_seek = prepared->execute(*sub);
    ASSERT_FALSE(irs::doc_limits::valid(docs_seek->value()));

    ASSERT_TRUE(docs->next());
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("L", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("L", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("N", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("N", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_FALSE(docs->next());
    ASSERT_TRUE(irs::doc_limits::eof(docs->value()));
    ASSERT_TRUE(irs::doc_limits::eof(docs_seek->seek(irs::doc_limits::eof())));
  }

  // "fox ... qui%ck"
  {
    irs::bytes_ref actual_value;

    irs::by_phrase q;
    *q.mutable_field() = "phrase_anl";
    q.mutable_options()->push_back<irs::by_term_options>().term = irs::ref_cast<irs::byte_type>(irs::string_ref("fox"));
    auto& wt = q.mutable_options()->push_back<irs::by_wildcard_options>(1);
    wt.term = irs::ref_cast<irs::byte_type>(irs::string_ref("qui%ck"));

    auto prepared = q.prepare(rdr);

    auto sub = rdr.begin();
    auto column = sub->column_reader("name");
    ASSERT_NE(nullptr, column);
    auto values = column->values();
    auto docs = prepared->execute(*sub);
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::valid(docs->value()));
    auto docs_seek = prepared->execute(*sub);
    ASSERT_FALSE(irs::doc_limits::valid(docs_seek->value()));

    ASSERT_TRUE(docs->next());
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("L", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("L", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("N", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("N", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_FALSE(docs->next());
    ASSERT_TRUE(irs::doc_limits::eof(docs->value()));
    ASSERT_TRUE(irs::doc_limits::eof(docs_seek->seek(irs::doc_limits::eof())));
  }

  // "fo* ... qui*"
  {
    irs::bytes_ref actual_value;

    irs::by_phrase q;
    *q.mutable_field() = "phrase_anl";
    auto& pt1 = q.mutable_options()->push_back<irs::by_prefix_options>();
    auto& pt2 = q.mutable_options()->push_back<irs::by_prefix_options>(1);
    pt1.term = irs::ref_cast<irs::byte_type>(irs::string_ref("fo"));
    pt2.term = irs::ref_cast<irs::byte_type>(irs::string_ref("qui"));

    auto prepared = q.prepare(rdr);

    auto sub = rdr.begin();
    auto column = sub->column_reader("name");
    ASSERT_NE(nullptr, column);
    auto values = column->values();
    auto docs = prepared->execute(*sub);
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::valid(docs->value()));
    auto docs_seek = prepared->execute(*sub);
    ASSERT_FALSE(irs::doc_limits::valid(docs_seek->value()));

    ASSERT_TRUE(docs->next());
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("L", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("L", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("N", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("N", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_FALSE(docs->next());
    ASSERT_TRUE(irs::doc_limits::eof(docs->value()));
    ASSERT_TRUE(irs::doc_limits::eof(docs_seek->seek(irs::doc_limits::eof())));
  }

  // "f%x ... qui%ck"
  {
    irs::bytes_ref actual_value;

    irs::by_phrase q;
    *q.mutable_field() = "phrase_anl";
    auto& wt1 = q.mutable_options()->push_back<irs::by_wildcard_options>();
    auto& wt2 = q.mutable_options()->push_back<irs::by_wildcard_options>(1);
    wt1.term = irs::ref_cast<irs::byte_type>(irs::string_ref("f%x"));
    wt2.term = irs::ref_cast<irs::byte_type>(irs::string_ref("qui%ck"));

    auto prepared = q.prepare(rdr);

    auto sub = rdr.begin();
    auto column = sub->column_reader("name");
    ASSERT_NE(nullptr, column);
    auto values = column->values();
    auto docs = prepared->execute(*sub);
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::valid(docs->value()));
    auto docs_seek = prepared->execute(*sub);
    ASSERT_FALSE(irs::doc_limits::valid(docs_seek->value()));

    ASSERT_TRUE(docs->next());
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("L", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("L", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("N", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("N", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_FALSE(docs->next());
    ASSERT_TRUE(irs::doc_limits::eof(docs->value()));
    ASSERT_TRUE(irs::doc_limits::eof(docs_seek->seek(irs::doc_limits::eof())));
  }

  // "fx ... quik"
  {
    irs::bytes_ref actual_value;

    irs::by_phrase q;
    *q.mutable_field() = "phrase_anl";
    auto& lt1 = q.mutable_options()->push_back<irs::by_edit_distance_filter_options>();
    auto& lt2 = q.mutable_options()->push_back<irs::by_edit_distance_filter_options>(1);
    lt1.max_distance = 1;
    lt1.term = irs::ref_cast<irs::byte_type>(irs::string_ref("fx"));
    lt2.max_distance = 1;
    lt2.term = irs::ref_cast<irs::byte_type>(irs::string_ref("quik"));

    auto prepared = q.prepare(rdr);

    auto sub = rdr.begin();
    auto column = sub->column_reader("name");
    ASSERT_NE(nullptr, column);
    auto values = column->values();
    auto docs = prepared->execute(*sub);
    ASSERT_FALSE(irs::get<irs::frequency>(*docs));
    ASSERT_FALSE(irs::get<irs::filter_boost>(*docs));
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::valid(docs->value()));
    auto docs_seek = prepared->execute(*sub);
    ASSERT_FALSE(irs::doc_limits::valid(docs_seek->value()));

    ASSERT_TRUE(docs->next());
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("L", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("L", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("N", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("N", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_FALSE(docs->next());
    ASSERT_TRUE(irs::doc_limits::eof(docs->value()));
    ASSERT_TRUE(irs::doc_limits::eof(docs_seek->seek(irs::doc_limits::eof())));
  }

  // "fx ... quik"
  {
    irs::bytes_ref actual_value;

    irs::by_phrase q;
    *q.mutable_field() = "phrase_anl";
    auto& lt1 = q.mutable_options()->push_back<irs::by_edit_distance_filter_options>();
    auto& lt2 = q.mutable_options()->push_back<irs::by_edit_distance_filter_options>(1);
    lt1.max_distance = 1;
    lt1.term = irs::ref_cast<irs::byte_type>(irs::string_ref("fx"));
    lt2.max_distance = 1;
    lt2.term = irs::ref_cast<irs::byte_type>(irs::string_ref("quik"));

    irs::order order;
    order.add(true, irs::scorers::get("bm25", irs::type<irs::text_format::json>::get(), "{ \"b\" : 0 }"));
    auto prepared_order = order.prepare();

    auto prepared = q.prepare(rdr, prepared_order);

    auto sub = rdr.begin();
    auto column = sub->column_reader("name");
    ASSERT_NE(nullptr, column);
    auto values = column->values();
    auto docs = prepared->execute(*sub, prepared_order);
    auto* freq = irs::get<irs::frequency>(*docs);
    ASSERT_TRUE(freq);
    auto* boost = irs::get<irs::filter_boost>(*docs);
    ASSERT_TRUE(boost);
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::valid(docs->value()));
    auto docs_seek = prepared->execute(*sub, prepared_order);
    ASSERT_FALSE(irs::doc_limits::valid(docs_seek->value()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(1, freq->value);
    ASSERT_FLOAT_EQ((0.5f+0.75f)/2, boost->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("L", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(freq->value, irs::get<irs::frequency>(*docs_seek)->value);
    ASSERT_FLOAT_EQ(boost->value, irs::get<irs::filter_boost>(*docs_seek)->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("L", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(2, freq->value);
    ASSERT_FLOAT_EQ((0.5f+0.75f)/2, boost->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("N", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(freq->value, irs::get<irs::frequency>(*docs_seek)->value);
    ASSERT_FLOAT_EQ(boost->value, irs::get<irs::filter_boost>(*docs_seek)->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("N", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_FALSE(docs->next());
    ASSERT_TRUE(irs::doc_limits::eof(docs->value()));
    ASSERT_TRUE(irs::doc_limits::eof(docs_seek->seek(irs::doc_limits::eof())));
  }

  // =============================
  // "fo* ... qui*" with scorer
  {
    irs::bytes_ref actual_value;

    irs::by_phrase q;
    *q.mutable_field() = "phrase_anl";
    auto& pt1 = q.mutable_options()->push_back<irs::by_prefix_options>();
    auto& pt2 = q.mutable_options()->push_back<irs::by_prefix_options>(1);
    pt1.term = irs::ref_cast<irs::byte_type>(irs::string_ref("fo"));
    pt2.term = irs::ref_cast<irs::byte_type>(irs::string_ref("qui"));

    irs::order order;
    order.add(true, irs::scorers::get("bm25", irs::type<irs::text_format::json>::get(), "{ \"b\" : 0 }"));
    auto prepared_order = order.prepare();

    auto prepared = q.prepare(rdr, prepared_order);

    auto sub = rdr.begin();
    auto column = sub->column_reader("name");
    ASSERT_NE(nullptr, column);
    auto values = column->values();
    auto docs = prepared->execute(*sub, prepared_order);
    auto* freq = irs::get<irs::frequency>(*docs);
    ASSERT_TRUE(freq);
    ASSERT_FALSE(irs::get<irs::filter_boost>(*docs));
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::valid(docs->value()));
    auto docs_seek = prepared->execute(*sub, prepared_order);
    ASSERT_FALSE(irs::doc_limits::valid(docs_seek->value()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(1, freq->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("L", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(freq->value, irs::get<irs::frequency>(*docs_seek)->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("L", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(2, freq->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("N", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(freq->value, irs::get<irs::frequency>(*docs_seek)->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("N", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_FALSE(docs->next());
    ASSERT_TRUE(irs::doc_limits::eof(docs->value()));
    ASSERT_TRUE(irs::doc_limits::eof(docs_seek->seek(irs::doc_limits::eof())));
  }

  // =============================
  // jumps ... (jumps|hotdog|the) with scorer
  {
    irs::bytes_ref actual_value;

    irs::by_phrase q;
    *q.mutable_field() = "phrase_anl";
    auto& pos0 = q.mutable_options()->push_back<irs::by_terms_options>();
    pos0.terms.emplace(irs::ref_cast<irs::byte_type>(irs::string_ref("jumps")));
    auto& pos1 = q.mutable_options()->push_back<irs::by_terms_options>(1);
    pos1.terms.emplace(irs::ref_cast<irs::byte_type>(irs::string_ref("jumps")), 0.25f);
    pos1.terms.emplace(irs::ref_cast<irs::byte_type>(irs::string_ref("hotdog")), 0.5f);
    pos1.terms.emplace(irs::ref_cast<irs::byte_type>(irs::string_ref("the")), 0.75f);

    irs::order order;
    order.add(true, irs::scorers::get("bm25", irs::type<irs::text_format::json>::get(), "{ \"b\" : 0 }"));
    auto prepared_order = order.prepare();

    auto prepared = q.prepare(rdr, prepared_order);

    auto sub = rdr.begin();
    auto column = sub->column_reader("name");
    ASSERT_NE(nullptr, column);
    auto values = column->values();
    auto docs = prepared->execute(*sub, prepared_order);
    auto* freq = irs::get<irs::frequency>(*docs);
    ASSERT_TRUE(freq);
    auto* boost = irs::get<irs::filter_boost>(*docs);
    ASSERT_TRUE(boost);
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::valid(docs->value()));
    auto docs_seek = prepared->execute(*sub, prepared_order);
    ASSERT_FALSE(irs::doc_limits::valid(docs_seek->value()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(1, freq->value);
    ASSERT_FLOAT_EQ((1.f+0.75f)/2, boost->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("A", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(freq->value, irs::get<irs::frequency>(*docs_seek)->value);
    ASSERT_EQ(boost->value, irs::get<irs::filter_boost>(*docs_seek)->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("A", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(2, freq->value);
    ASSERT_FLOAT_EQ(((1.f+0.25f)/2 + (1.f+0.5f)/2)/2, boost->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("O", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(freq->value, irs::get<irs::frequency>(*docs_seek)->value);
    ASSERT_EQ(boost->value, irs::get<irs::filter_boost>(*docs_seek)->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("O", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(4, freq->value);
    ASSERT_FLOAT_EQ((1.f+0.25f)/2, boost->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("P", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(freq->value, irs::get<irs::frequency>(*docs_seek)->value);
    ASSERT_EQ(boost->value, irs::get<irs::filter_boost>(*docs_seek)->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("P", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(3, freq->value);
    ASSERT_FLOAT_EQ((1.f+0.25f)/2, boost->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("Q", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(freq->value, irs::get<irs::frequency>(*docs_seek)->value);
    ASSERT_EQ(boost->value, irs::get<irs::filter_boost>(*docs_seek)->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("Q", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(2, freq->value);
    ASSERT_FLOAT_EQ((1.f+0.25f)/2, boost->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("R", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(freq->value, irs::get<irs::frequency>(*docs_seek)->value);
    ASSERT_EQ(boost->value, irs::get<irs::filter_boost>(*docs_seek)->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("R", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_FALSE(docs->next());
    ASSERT_TRUE(irs::doc_limits::eof(docs->value()));
    ASSERT_TRUE(irs::doc_limits::eof(docs_seek->seek(irs::doc_limits::eof())));
  }

  // by_terms_options "fox|that" with scorer
  {
    irs::bytes_ref actual_value;

    irs::by_phrase q;
    *q.mutable_field() = "phrase_anl";
    auto& st = q.mutable_options()->push_back<irs::by_terms_options>();
    st.terms.emplace(irs::ref_cast<irs::byte_type>(irs::string_ref("fox")));
    st.terms.emplace(irs::ref_cast<irs::byte_type>(irs::string_ref("that")));

    irs::order order;
    order.add(true, irs::scorers::get("bm25", irs::type<irs::text_format::json>::get(), "{ \"b\" : 0 }"));
    auto prepared_order = order.prepare();

    auto prepared = q.prepare(rdr, prepared_order);

    auto sub = rdr.begin();
    auto column = sub->column_reader("name");
    ASSERT_NE(nullptr, column);
    auto values = column->values();
    auto docs = prepared->execute(*sub, prepared_order);
    auto* freq = irs::get<irs::frequency>(*docs);
    ASSERT_TRUE(freq);
    ASSERT_FALSE(irs::get<irs::filter_boost>(*docs));
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::valid(docs->value()));
    auto docs_seek = prepared->execute(*sub, prepared_order);
    ASSERT_FALSE(irs::doc_limits::valid(docs_seek->value()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(1, freq->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("A", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(freq->value, irs::get<irs::frequency>(*docs_seek)->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("A", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(1, freq->value);
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("B", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(freq->value, irs::get<irs::frequency>(*docs_seek)->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("B", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(1, freq->value);
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("D", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(freq->value, irs::get<irs::frequency>(*docs_seek)->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("D", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(1, freq->value);
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("G", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(freq->value, irs::get<irs::frequency>(*docs_seek)->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("G", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(1, freq->value);
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("I", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(freq->value, irs::get<irs::frequency>(*docs_seek)->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("I", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(1, freq->value);
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("K", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(freq->value, irs::get<irs::frequency>(*docs_seek)->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("K", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(1, freq->value);
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("L", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(freq->value, irs::get<irs::frequency>(*docs_seek)->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("L", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(4, freq->value);
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("N", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(freq->value, irs::get<irs::frequency>(*docs_seek)->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("N", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(1, freq->value);
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("S", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(freq->value, irs::get<irs::frequency>(*docs_seek)->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("S", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(1, freq->value);
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("T", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(freq->value, irs::get<irs::frequency>(*docs_seek)->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("T", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(1, freq->value);
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("V", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(freq->value, irs::get<irs::frequency>(*docs_seek)->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("V", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_FALSE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(irs::doc_limits::eof(docs->value()));
  }

  // by_terms_options "fox|that" with scorer and boost
  {
    irs::bytes_ref actual_value;

    irs::by_phrase q;
    *q.mutable_field() = "phrase_anl";
    auto& st = q.mutable_options()->push_back<irs::by_terms_options>();
    st.terms.emplace(irs::ref_cast<irs::byte_type>(irs::string_ref("fox")), 0.5f);
    st.terms.emplace(irs::ref_cast<irs::byte_type>(irs::string_ref("that")));

    irs::order order;
    order.add(true, irs::scorers::get("bm25", irs::type<irs::text_format::json>::get(), "{ \"b\" : 0 }"));
    auto prepared_order = order.prepare();

    auto prepared = q.prepare(rdr, prepared_order);

    auto sub = rdr.begin();
    auto column = sub->column_reader("name");
    ASSERT_NE(nullptr, column);
    auto values = column->values();
    auto docs = prepared->execute(*sub, prepared_order);
    auto* freq = irs::get<irs::frequency>(*docs);
    ASSERT_TRUE(freq);
    auto* boost = irs::get<irs::filter_boost>(*docs);
    ASSERT_TRUE(boost);
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::valid(docs->value()));
    auto docs_seek = prepared->execute(*sub, prepared_order);
    ASSERT_FALSE(irs::doc_limits::valid(docs_seek->value()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(1, freq->value);
    ASSERT_EQ(0.5f, boost->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("A", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(freq->value, irs::get<irs::frequency>(*docs_seek)->value);
    ASSERT_EQ(boost->value, irs::get<irs::filter_boost>(*docs_seek)->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("A", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(1, freq->value);
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("B", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(freq->value, irs::get<irs::frequency>(*docs_seek)->value);
    ASSERT_EQ(boost->value, irs::get<irs::filter_boost>(*docs_seek)->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("B", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(1, freq->value);
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(irs::no_boost(), boost->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("D", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(freq->value, irs::get<irs::frequency>(*docs_seek)->value);
    ASSERT_EQ(boost->value, irs::get<irs::filter_boost>(*docs_seek)->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("D", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(1, freq->value);
    ASSERT_EQ(0.5f, boost->value);
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("G", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(freq->value, irs::get<irs::frequency>(*docs_seek)->value);
    ASSERT_EQ(boost->value, irs::get<irs::filter_boost>(*docs_seek)->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("G", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(1, freq->value);
    ASSERT_EQ(0.5f, boost->value);
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("I", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(freq->value, irs::get<irs::frequency>(*docs_seek)->value);
    ASSERT_EQ(boost->value, irs::get<irs::filter_boost>(*docs_seek)->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("I", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(1, freq->value);
    ASSERT_EQ(0.5f, boost->value);
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("K", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(freq->value, irs::get<irs::frequency>(*docs_seek)->value);
    ASSERT_EQ(boost->value, irs::get<irs::filter_boost>(*docs_seek)->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("K", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(1, freq->value);
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("L", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(freq->value, irs::get<irs::frequency>(*docs_seek)->value);
    ASSERT_EQ(boost->value, irs::get<irs::filter_boost>(*docs_seek)->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("L", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(4, freq->value);
    ASSERT_EQ(0.5f, boost->value);
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("N", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(freq->value, irs::get<irs::frequency>(*docs_seek)->value);
    ASSERT_EQ(boost->value, irs::get<irs::filter_boost>(*docs_seek)->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("N", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(1, freq->value);
    ASSERT_EQ(0.5f, boost->value);
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("S", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(freq->value, irs::get<irs::frequency>(*docs_seek)->value);
    ASSERT_EQ(boost->value, irs::get<irs::filter_boost>(*docs_seek)->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("S", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(1, freq->value);
    ASSERT_EQ(0.5f, boost->value);
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("T", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(freq->value, irs::get<irs::frequency>(*docs_seek)->value);
    ASSERT_EQ(boost->value, irs::get<irs::filter_boost>(*docs_seek)->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("T", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(1, freq->value);
    ASSERT_EQ(0.5f, boost->value);
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("V", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(freq->value, irs::get<irs::frequency>(*docs_seek)->value);
    ASSERT_EQ(boost->value, irs::get<irs::filter_boost>(*docs_seek)->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("V", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_FALSE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(irs::doc_limits::eof(docs->value()));
  }

  // test disjunctions (unary, basic, small, disjunction)
  {
    irs::bytes_ref actual_value;

    irs::by_phrase q;
    *q.mutable_field() = "phrase_anl";
    auto& wt1 = q.mutable_options()->push_back<irs::by_wildcard_options>();
    auto& wt2 = q.mutable_options()->push_back<irs::by_wildcard_options>();
    auto& pt1 = q.mutable_options()->push_back<irs::by_prefix_options>();
    auto& pt2 = q.mutable_options()->push_back<irs::by_prefix_options>();
    wt1.term = irs::ref_cast<irs::byte_type>(irs::string_ref("%las"));
    wt2.term = irs::ref_cast<irs::byte_type>(irs::string_ref("%nd"));
    pt1.term = irs::ref_cast<irs::byte_type>(irs::string_ref("go"));
    pt2.term = irs::ref_cast<irs::byte_type>(irs::string_ref("like"));

    irs::order order;
    order.add(true, irs::scorers::get("bm25", irs::type<irs::text_format::json>::get(), "{ \"b\" : 0 }"));
    auto prepared_order = order.prepare();

    auto prepared = q.prepare(rdr, prepared_order);

    auto sub = rdr.begin();
    auto column = sub->column_reader("name");
    ASSERT_NE(nullptr, column);
    auto values = column->values();
    auto docs = prepared->execute(*sub, prepared_order);
    auto* freq = irs::get<irs::frequency>(*docs);
    ASSERT_TRUE(freq);
    ASSERT_FALSE(irs::get<irs::filter_boost>(*docs));
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::valid(docs->value()));
    auto docs_seek = prepared->execute(*sub, prepared_order);
    ASSERT_FALSE(irs::doc_limits::valid(docs_seek->value()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_EQ(1, freq->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("Z", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(freq->value, irs::get<irs::frequency>(*docs_seek)->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("Z", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_FALSE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(irs::doc_limits::eof(docs->value()));
  }

  // =============================

  // "fox ... quick" with phrase offset
  // which is does not matter
  {
    irs::bytes_ref actual_value;

    irs::by_phrase q;
    *q.mutable_field() = "phrase_anl";
    q.mutable_options()->push_back<irs::by_term_options>(std::numeric_limits<size_t>::max()).term = irs::ref_cast<irs::byte_type>(irs::string_ref("fox"));
    q.mutable_options()->push_back<irs::by_term_options>(1).term = irs::ref_cast<irs::byte_type>(irs::string_ref("quick"));

    auto prepared = q.prepare(rdr);

    auto sub = rdr.begin();
    auto column = sub->column_reader("name");
    ASSERT_NE(nullptr, column);
    auto values = column->values();
    auto docs = prepared->execute(*sub);
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::valid( docs->value()));
    auto docs_seek = prepared->execute(*sub);
    ASSERT_FALSE(irs::doc_limits::valid(docs_seek->value()));

    ASSERT_TRUE(docs->next());
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("L", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("L", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("N", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("N", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_FALSE(docs->next());
    ASSERT_TRUE(irs::doc_limits::eof(docs->value()));
    ASSERT_TRUE(irs::doc_limits::eof(docs_seek->seek(irs::doc_limits::eof())));
  }

  // "fox quick"
  // const_max and zero offset
  {
    irs::bytes_ref actual_value;

    irs::by_phrase q;
    *q.mutable_field() = "phrase_anl";
    q.mutable_options()->push_back<irs::by_term_options>(std::numeric_limits<size_t>::max()).term = irs::ref_cast<irs::byte_type>(irs::string_ref("fox"));
    q.mutable_options()->push_back<irs::by_term_options>(0).term = irs::ref_cast<irs::byte_type>(irs::string_ref("quick"));

    auto prepared = q.prepare(rdr);

    auto sub = rdr.begin();
    auto column = sub->column_reader("name");
    ASSERT_NE(nullptr, column);
    auto values = column->values();
    auto docs = prepared->execute(*sub);
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::valid( docs->value()));
    auto docs_seek = prepared->execute(*sub);
    ASSERT_FALSE(irs::doc_limits::valid(docs_seek->value()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("N", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("N", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_FALSE(docs->next());
    ASSERT_TRUE(irs::doc_limits::eof(docs->value()));
    ASSERT_TRUE(irs::doc_limits::eof(docs_seek->seek(irs::doc_limits::eof())));
  }

  // "fox* quick*"
  // const_max and zero offset
  {
    irs::bytes_ref actual_value;

    irs::by_phrase q;
    *q.mutable_field() = "phrase_anl";
    auto& pt1 = q.mutable_options()->push_back<irs::by_prefix_options>(std::numeric_limits<size_t>::max());
    auto& pt2 = q.mutable_options()->push_back<irs::by_prefix_options>(0);
    pt1.term = irs::ref_cast<irs::byte_type>(irs::string_ref("fox"));
    pt2.term = irs::ref_cast<irs::byte_type>(irs::string_ref("quick"));

    auto prepared = q.prepare(rdr);

    auto sub = rdr.begin();
    auto column = sub->column_reader("name");
    ASSERT_NE(nullptr, column);
    auto values = column->values();
    auto docs = prepared->execute(*sub);
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::valid( docs->value()));
    auto docs_seek = prepared->execute(*sub);
    ASSERT_FALSE(irs::doc_limits::valid(docs_seek->value()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("N", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("N", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_FALSE(docs->next());
    ASSERT_TRUE(irs::doc_limits::eof(docs->value()));
    ASSERT_TRUE(irs::doc_limits::eof(docs_seek->seek(irs::doc_limits::eof())));
  }

  // "fo* ... quick" with phrase offset
  // which is does not matter
  {
    irs::bytes_ref actual_value;

    irs::by_phrase q;
    *q.mutable_field() = "phrase_anl";
    auto& pt = q.mutable_options()->push_back<irs::by_prefix_options>(std::numeric_limits<size_t>::max());
    pt.term = irs::ref_cast<irs::byte_type>(irs::string_ref("fo"));
    q.mutable_options()->push_back<irs::by_term_options>(1).term = irs::ref_cast<irs::byte_type>(irs::string_ref("quick"));

    auto prepared = q.prepare(rdr);

    auto sub = rdr.begin();
    auto column = sub->column_reader("name");
    ASSERT_NE(nullptr, column);
    auto values = column->values();
    auto docs = prepared->execute(*sub);
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::valid( docs->value()));
    auto docs_seek = prepared->execute(*sub);
    ASSERT_FALSE(irs::doc_limits::valid(docs_seek->value()));

    ASSERT_TRUE(docs->next());
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("L", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("L", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("N", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("N", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_FALSE(docs->next());
    ASSERT_TRUE(irs::doc_limits::eof(docs->value()));
    ASSERT_TRUE(irs::doc_limits::eof(docs_seek->seek(irs::doc_limits::eof())));
  }

  // "f_x ... quick" with phrase offset
  // which is does not matter
  {
    irs::bytes_ref actual_value;

    irs::by_phrase q;
    *q.mutable_field() = "phrase_anl";
    auto& wt = q.mutable_options()->push_back<irs::by_wildcard_options>(std::numeric_limits<size_t>::max());
    wt.term = irs::ref_cast<irs::byte_type>(irs::string_ref("f_x"));
    q.mutable_options()->push_back<irs::by_term_options>(1).term = irs::ref_cast<irs::byte_type>(irs::string_ref("quick"));

    auto prepared = q.prepare(rdr);

    auto sub = rdr.begin();
    auto column = sub->column_reader("name");
    ASSERT_NE(nullptr, column);
    auto values = column->values();
    auto docs = prepared->execute(*sub);
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::valid( docs->value()));
    auto docs_seek = prepared->execute(*sub);
    ASSERT_FALSE(irs::doc_limits::valid(docs_seek->value()));

    ASSERT_TRUE(docs->next());
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("L", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("L", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("N", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("N", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_FALSE(docs->next());
    ASSERT_TRUE(irs::doc_limits::eof(docs->value()));
    ASSERT_TRUE(irs::doc_limits::eof(docs_seek->seek(irs::doc_limits::eof())));
  }

  // "fox ... qui*" with phrase offset
  // which is does not matter
  {
    irs::bytes_ref actual_value;

    irs::by_phrase q;
    *q.mutable_field() = "phrase_anl";
    q.mutable_options()->push_back<irs::by_term_options>(std::numeric_limits<size_t>::max()).term = irs::ref_cast<irs::byte_type>(irs::string_ref("fox"));
    auto& pt = q.mutable_options()->push_back<irs::by_prefix_options>(1);
    pt.term = irs::ref_cast<irs::byte_type>(irs::string_ref("qui"));

    auto prepared = q.prepare(rdr);

    auto sub = rdr.begin();
    auto column = sub->column_reader("name");
    ASSERT_NE(nullptr, column);
    auto values = column->values();
    auto docs = prepared->execute(*sub);
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::valid( docs->value()));
    auto docs_seek = prepared->execute(*sub);
    ASSERT_FALSE(irs::doc_limits::valid(docs_seek->value()));

    ASSERT_TRUE(docs->next());
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("L", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("L", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("N", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("N", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_FALSE(docs->next());
    ASSERT_TRUE(irs::doc_limits::eof(docs->value()));
    ASSERT_TRUE(irs::doc_limits::eof(docs_seek->seek(irs::doc_limits::eof())));
  }

  // "fox ... qui%k" with phrase offset
  // which is does not matter
  {
    irs::bytes_ref actual_value;

    irs::by_phrase q;
    *q.mutable_field() = "phrase_anl";
    q.mutable_options()->push_back<irs::by_term_options>(std::numeric_limits<size_t>::max()).term = irs::ref_cast<irs::byte_type>(irs::string_ref("fox"));
    auto& wt = q.mutable_options()->push_back<irs::by_wildcard_options>(1);
    wt.term = irs::ref_cast<irs::byte_type>(irs::string_ref("qui%k"));

    auto prepared = q.prepare(rdr);

    auto sub = rdr.begin();
    auto column = sub->column_reader("name");
    ASSERT_NE(nullptr, column);
    auto values = column->values();
    auto docs = prepared->execute(*sub);
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::valid( docs->value()));
    auto docs_seek = prepared->execute(*sub);
    ASSERT_FALSE(irs::doc_limits::valid(docs_seek->value()));

    ASSERT_TRUE(docs->next());
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("L", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("L", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("N", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("N", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_FALSE(docs->next());
    ASSERT_TRUE(irs::doc_limits::eof(docs->value()));
    ASSERT_TRUE(irs::doc_limits::eof(docs_seek->seek(irs::doc_limits::eof())));
  }

  // "fo* ... qui*" with phrase offset
  // which is does not matter
  {
    irs::bytes_ref actual_value;

    irs::by_phrase q;
    *q.mutable_field() = "phrase_anl";
    auto& pt1 = q.mutable_options()->push_back<irs::by_prefix_options>(std::numeric_limits<size_t>::max());
    auto& pt2 = q.mutable_options()->push_back<irs::by_prefix_options>(1);
    pt1.term = irs::ref_cast<irs::byte_type>(irs::string_ref("fo"));
    pt2.term = irs::ref_cast<irs::byte_type>(irs::string_ref("qui"));

    auto prepared = q.prepare(rdr);

    auto sub = rdr.begin();
    auto column = sub->column_reader("name");
    ASSERT_NE(nullptr, column);
    auto values = column->values();
    auto docs = prepared->execute(*sub);
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::valid( docs->value()));
    auto docs_seek = prepared->execute(*sub);
    ASSERT_FALSE(irs::doc_limits::valid(docs_seek->value()));

    ASSERT_TRUE(docs->next());
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("L", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("L", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("N", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("N", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_FALSE(docs->next());
    ASSERT_TRUE(irs::doc_limits::eof(docs->value()));
    ASSERT_TRUE(irs::doc_limits::eof(docs_seek->seek(irs::doc_limits::eof())));
  }

  // "fo% ... qui%" with phrase offset
  // which is does not matter
  {
    irs::bytes_ref actual_value;

    irs::by_phrase q;
    *q.mutable_field() = "phrase_anl";
    auto& wt1 = q.mutable_options()->push_back<irs::by_wildcard_options>(std::numeric_limits<size_t>::max());
    auto& wt2 = q.mutable_options()->push_back<irs::by_wildcard_options>(1);
    wt1.term = irs::ref_cast<irs::byte_type>(irs::string_ref("fo%"));
    wt2.term = irs::ref_cast<irs::byte_type>(irs::string_ref("qui%"));

    auto prepared = q.prepare(rdr);

    auto sub = rdr.begin();
    auto column = sub->column_reader("name");
    ASSERT_NE(nullptr, column);
    auto values = column->values();
    auto docs = prepared->execute(*sub);
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::valid( docs->value()));
    auto docs_seek = prepared->execute(*sub);
    ASSERT_FALSE(irs::doc_limits::valid(docs_seek->value()));

    ASSERT_TRUE(docs->next());
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("L", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("L", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("N", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("N", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_FALSE(docs->next());
    ASSERT_TRUE(irs::doc_limits::eof(docs->value()));
    ASSERT_TRUE(irs::doc_limits::eof(docs_seek->seek(irs::doc_limits::eof())));
  }

  // "fo% ... quik" with phrase offset
  // which is does not matter
  {
    irs::bytes_ref actual_value;

    irs::by_phrase q;
    *q.mutable_field() = "phrase_anl";
    auto& wt = q.mutable_options()->push_back<irs::by_wildcard_options>(std::numeric_limits<size_t>::max());
    auto& lt = q.mutable_options()->push_back<irs::by_edit_distance_filter_options>(1);
    wt.term = irs::ref_cast<irs::byte_type>(irs::string_ref("fo%"));
    lt.max_distance = 1;
    lt.term = irs::ref_cast<irs::byte_type>(irs::string_ref("quik"));

    auto prepared = q.prepare(rdr);

    auto sub = rdr.begin();
    auto column = sub->column_reader("name");
    ASSERT_NE(nullptr, column);
    auto values = column->values();
    auto docs = prepared->execute(*sub);
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::valid( docs->value()));
    auto docs_seek = prepared->execute(*sub);
    ASSERT_FALSE(irs::doc_limits::valid(docs_seek->value()));

    ASSERT_TRUE(docs->next());
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("L", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("L", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_TRUE(docs->next());
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("N", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("N", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_FALSE(docs->next());
    ASSERT_TRUE(irs::doc_limits::eof(docs->value()));
    ASSERT_TRUE(irs::doc_limits::eof(docs_seek->seek(irs::doc_limits::eof())));
  }

  // "fox ... ... ... ... ... ... ... ... ... ... quick"
  {
    irs::by_phrase q;
    *q.mutable_field() = "phrase_anl";
    q.mutable_options()->push_back<irs::by_term_options>().term = irs::ref_cast<irs::byte_type>(irs::string_ref("fox"));
    q.mutable_options()->push_back<irs::by_term_options>(10).term = irs::ref_cast<irs::byte_type>(irs::string_ref("quick"));

    auto prepared = q.prepare(rdr);

    auto sub = rdr.begin();
    auto docs = prepared->execute(*sub);
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
    q.mutable_options()->push_back<irs::by_term_options>().term = irs::ref_cast<irs::byte_type>(irs::string_ref("fox"));
    auto& pt = q.mutable_options()->push_back<irs::by_prefix_options>(10);
    pt.term = irs::ref_cast<irs::byte_type>(irs::string_ref("qui"));

    auto prepared = q.prepare(rdr);

    auto sub = rdr.begin();
    auto docs = prepared->execute(*sub);
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
    q.mutable_options()->push_back<irs::by_term_options>().term = irs::ref_cast<irs::byte_type>(irs::string_ref("fox"));
    auto& wt = q.mutable_options()->push_back<irs::by_wildcard_options>(10);
    wt.term = irs::ref_cast<irs::byte_type>(irs::string_ref("qu_ck"));

    auto prepared = q.prepare(rdr);

    auto sub = rdr.begin();
    auto docs = prepared->execute(*sub);
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
    q.mutable_options()->push_back<irs::by_term_options>().term = irs::ref_cast<irs::byte_type>(irs::string_ref("fox"));
    auto& lt = q.mutable_options()->push_back<irs::by_edit_distance_filter_options>(10);
    lt.max_distance = 2;
    lt.term = irs::ref_cast<irs::byte_type>(irs::string_ref("quc"));

    auto prepared = q.prepare(rdr);

    auto sub = rdr.begin();
    auto docs = prepared->execute(*sub);
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::valid(docs->value()));
    ASSERT_FALSE(docs->next());
    ASSERT_TRUE(irs::doc_limits::eof(docs->value()));
  }

  // "eye ... eye"
  {
    irs::bytes_ref actual_value;

    irs::by_phrase q;
    *q.mutable_field() = "phrase_anl";
    q.mutable_options()->push_back<irs::by_term_options>().term = irs::ref_cast<irs::byte_type>(irs::string_ref("eye"));
    q.mutable_options()->push_back<irs::by_term_options>(1).term = irs::ref_cast<irs::byte_type>(irs::string_ref("eye"));

    auto prepared = q.prepare(rdr);

    auto sub = rdr.begin();
    auto column = sub->column_reader("name");
    ASSERT_NE(nullptr, column);
    auto values = column->values();
    auto docs = prepared->execute(*sub);
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::valid(docs->value()));
    auto docs_seek = prepared->execute(*sub);
    ASSERT_FALSE(irs::doc_limits::valid(docs_seek->value()));

    ASSERT_TRUE(docs->next());
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("C", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("C", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_FALSE(docs->next());
    ASSERT_TRUE(irs::doc_limits::eof(docs->value()));
    ASSERT_TRUE(irs::doc_limits::eof(docs_seek->seek(irs::doc_limits::eof())));
  }

  // "as in the past we are looking forward"
  {
    irs::bytes_ref actual_value;

    irs::by_phrase q;
    *q.mutable_field() = "phrase_anl";
    q.mutable_options()->push_back<irs::by_term_options>().term = irs::ref_cast<irs::byte_type>(irs::string_ref("as"));
    q.mutable_options()->push_back<irs::by_term_options>().term = irs::ref_cast<irs::byte_type>(irs::string_ref("in"));
    q.mutable_options()->push_back<irs::by_term_options>().term = irs::ref_cast<irs::byte_type>(irs::string_ref("the"));
    q.mutable_options()->push_back<irs::by_term_options>().term = irs::ref_cast<irs::byte_type>(irs::string_ref("past"));
    q.mutable_options()->push_back<irs::by_term_options>().term = irs::ref_cast<irs::byte_type>(irs::string_ref("we"));
    q.mutable_options()->push_back<irs::by_term_options>().term = irs::ref_cast<irs::byte_type>(irs::string_ref("are"));
    q.mutable_options()->push_back<irs::by_term_options>().term = irs::ref_cast<irs::byte_type>(irs::string_ref("looking"));
    q.mutable_options()->push_back<irs::by_term_options>().term = irs::ref_cast<irs::byte_type>(irs::string_ref("forward"));

    auto prepared = q.prepare(rdr);
    auto sub = rdr.begin();
    auto column = sub->column_reader("name");
    ASSERT_NE(nullptr, column);
    auto values = column->values();
    auto docs = prepared->execute(*sub);
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::valid(docs->value()));
    auto docs_seek = prepared->execute(*sub);
    ASSERT_FALSE(irs::doc_limits::valid(docs_seek->value()));

    ASSERT_TRUE(docs->next());
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("H", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("H", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_FALSE(docs->next());
    ASSERT_TRUE(irs::doc_limits::eof(docs->value()));
    ASSERT_TRUE(irs::doc_limits::eof(docs_seek->seek(irs::doc_limits::eof())));
  }

  // "as in % past we ___ looking forward"
  {
    irs::bytes_ref actual_value;

    irs::by_phrase q;
    *q.mutable_field() = "phrase_anl";
    auto& lt = q.mutable_options()->push_back<irs::by_edit_distance_filter_options>();
    lt.max_distance = 2;
    lt.term = irs::ref_cast<irs::byte_type>(irs::string_ref("ass"));
    q.mutable_options()->push_back<irs::by_term_options>().term = irs::ref_cast<irs::byte_type>(irs::string_ref("in"));
    auto& wt1 = q.mutable_options()->push_back<irs::by_wildcard_options>();
    wt1.term = irs::ref_cast<irs::byte_type>(irs::string_ref("%"));
    q.mutable_options()->push_back<irs::by_term_options>().term = irs::ref_cast<irs::byte_type>(irs::string_ref("past"));
    q.mutable_options()->push_back<irs::by_term_options>().term = irs::ref_cast<irs::byte_type>(irs::string_ref("we"));
    auto& wt2 = q.mutable_options()->push_back<irs::by_wildcard_options>();
    wt2.term = irs::ref_cast<irs::byte_type>(irs::string_ref("___"));
    auto& st = q.mutable_options()->push_back<irs::by_terms_options>();
    st.terms.emplace(irs::ref_cast<irs::byte_type>(irs::string_ref("looking")));
    st.terms.emplace(irs::ref_cast<irs::byte_type>(irs::string_ref("searching")));
    auto& pt = q.mutable_options()->push_back<irs::by_prefix_options>();
    pt.term = irs::ref_cast<irs::byte_type>(irs::string_ref("fo"));

    auto prepared = q.prepare(rdr);
    auto sub = rdr.begin();
    auto column = sub->column_reader("name");
    ASSERT_NE(nullptr, column);
    auto values = column->values();
    auto docs = prepared->execute(*sub);
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::valid(docs->value()));
    auto docs_seek = prepared->execute(*sub);
    ASSERT_FALSE(irs::doc_limits::valid(docs_seek->value()));

    ASSERT_TRUE(docs->next());
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("H", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("H", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_FALSE(docs->next());
    ASSERT_TRUE(irs::doc_limits::eof(docs->value()));
    ASSERT_TRUE(irs::doc_limits::eof(docs_seek->seek(irs::doc_limits::eof())));
  }

  // "as in the past we are looking forward" with order
  {
    irs::bytes_ref actual_value;

    irs::by_phrase q;
    *q.mutable_field() = "phrase_anl";
    q.mutable_options()->push_back<irs::by_term_options>().term = irs::ref_cast<irs::byte_type>(irs::string_ref("as"));
    q.mutable_options()->push_back<irs::by_term_options>().term = irs::ref_cast<irs::byte_type>(irs::string_ref("in"));
    q.mutable_options()->push_back<irs::by_term_options>().term = irs::ref_cast<irs::byte_type>(irs::string_ref("the"));
    q.mutable_options()->push_back<irs::by_term_options>().term = irs::ref_cast<irs::byte_type>(irs::string_ref("past"));
    q.mutable_options()->push_back<irs::by_term_options>().term = irs::ref_cast<irs::byte_type>(irs::string_ref("we"));
    q.mutable_options()->push_back<irs::by_term_options>().term = irs::ref_cast<irs::byte_type>(irs::string_ref("are"));
    q.mutable_options()->push_back<irs::by_term_options>().term = irs::ref_cast<irs::byte_type>(irs::string_ref("looking"));
    q.mutable_options()->push_back<irs::by_term_options>().term = irs::ref_cast<irs::byte_type>(irs::string_ref("forward"));

    irs::order ord;
    auto& sort = ord.add<tests::sort::custom_sort>(false);
    sort.scorer_add = [](irs::doc_id_t& dst, const irs::doc_id_t& src)->void {
      ASSERT_TRUE(
        irs::doc_limits::invalid() == dst
        || dst == src
      );
      dst = src;
    };

    auto pord = ord.prepare();
    auto prepared = q.prepare(rdr, pord);
    auto sub = rdr.begin();
    auto column = sub->column_reader("name");
    ASSERT_NE(nullptr, column);
    auto values = column->values();
    auto docs = prepared->execute(*sub, pord);
    auto* freq = irs::get<irs::frequency>(*docs);
    ASSERT_TRUE(freq);
    ASSERT_FALSE(irs::get<irs::filter_boost>(*docs));
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::valid(docs->value()));
    auto docs_seek = prepared->execute(*sub, pord);
    ASSERT_FALSE(irs::doc_limits::valid(docs_seek->value()));
    auto* score = irs::get<irs::score>(*docs);
    ASSERT_FALSE(!score);

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(),pord.get<irs::doc_id_t>(score->evaluate(), 0));
    ASSERT_EQ(1, freq->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("H", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(freq->value, irs::get<irs::frequency>(*docs_seek)->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("H", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_FALSE(docs->next());
    ASSERT_TRUE(irs::doc_limits::eof(docs->value()));
    ASSERT_TRUE(irs::doc_limits::eof(docs_seek->seek(irs::doc_limits::eof())));
  }

  // "as in the p_st we are look* forward" with order
  {
    irs::bytes_ref actual_value;

    irs::by_phrase q;
    *q.mutable_field() = "phrase_anl";
    q.mutable_options()->push_back<irs::by_term_options>().term = irs::ref_cast<irs::byte_type>(irs::string_ref("as"));
    q.mutable_options()->push_back<irs::by_term_options>().term = irs::ref_cast<irs::byte_type>(irs::string_ref("in"));
    q.mutable_options()->push_back<irs::by_term_options>().term = irs::ref_cast<irs::byte_type>(irs::string_ref("the"));
    auto& wt = q.mutable_options()->push_back<irs::by_wildcard_options>();
    wt.term = irs::ref_cast<irs::byte_type>(irs::string_ref("p_st"));
    q.mutable_options()->push_back<irs::by_term_options>().term = irs::ref_cast<irs::byte_type>(irs::string_ref("we"));
    q.mutable_options()->push_back<irs::by_term_options>().term = irs::ref_cast<irs::byte_type>(irs::string_ref("are"));
    auto& pt = q.mutable_options()->push_back<irs::by_prefix_options>();
    pt.term = irs::ref_cast<irs::byte_type>(irs::string_ref("look"));
    q.mutable_options()->push_back<irs::by_term_options>().term = irs::ref_cast<irs::byte_type>(irs::string_ref("forward"));

    irs::order ord;
    auto& sort = ord.add<tests::sort::custom_sort>(false);
    sort.scorer_add = [](irs::doc_id_t& dst, const irs::doc_id_t& src)->void {
      ASSERT_TRUE(
        irs::doc_limits::invalid() == dst
        || dst == src
      );
      dst = src;
    };

    auto pord = ord.prepare();
    auto prepared = q.prepare(rdr, pord);
    auto sub = rdr.begin();
    auto column = sub->column_reader("name");
    ASSERT_NE(nullptr, column);
    auto values = column->values();
    auto docs = prepared->execute(*sub, pord);
    auto* freq = irs::get<irs::frequency>(*docs);
    ASSERT_TRUE(freq);
    ASSERT_FALSE(irs::get<irs::filter_boost>(*docs));
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::valid(docs->value()));
    auto docs_seek = prepared->execute(*sub, pord);
    ASSERT_FALSE(irs::doc_limits::valid(docs_seek->value()));
    auto* score = irs::get<irs::score>(*docs);
    ASSERT_FALSE(!score);

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(),pord.get<irs::doc_id_t>(score->evaluate(), 0));
    ASSERT_EQ(1, freq->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("H", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(freq->value, irs::get<irs::frequency>(*docs_seek)->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("H", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_FALSE(docs->next());
    ASSERT_TRUE(irs::doc_limits::eof(docs->value()));
    ASSERT_TRUE(irs::doc_limits::eof(docs_seek->seek(irs::doc_limits::eof())));
  }

  // fox quick
  {
    irs::bytes_ref actual_value;

    irs::by_phrase q;
    *q.mutable_field() = "phrase_anl";
    q.mutable_options()->push_back<irs::by_term_options>().term = irs::ref_cast<irs::byte_type>(irs::string_ref("fox"));
    q.mutable_options()->push_back<irs::by_term_options>().term = irs::ref_cast<irs::byte_type>(irs::string_ref("quick"));

    auto prepared = q.prepare(rdr);

    auto sub = rdr.begin();
    auto column = sub->column_reader("name");
    ASSERT_NE(nullptr, column);
    auto values = column->values();
    auto docs = prepared->execute(*sub);
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::valid(docs->value()));
    auto docs_seek = prepared->execute(*sub);
    ASSERT_FALSE(irs::doc_limits::valid(docs_seek->value()));

    ASSERT_TRUE(docs->next());
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("N", irs::to_string<irs::string_ref>(actual_value.c_str()));
    // Check repeatable seek to the same document given frequency of the phrase within the document = 2
    auto v = docs->value();
    ASSERT_EQ(v, docs->seek(docs->value()));
    ASSERT_EQ(v, docs->seek(docs->value()));
    ASSERT_EQ("N", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("N", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_FALSE(docs->next());
    ASSERT_TRUE(irs::doc_limits::eof(docs->value()));
    ASSERT_TRUE(irs::doc_limits::eof(docs_seek->seek(irs::doc_limits::eof())));
  }

  // fox quick with order
  {
    irs::bytes_ref actual_value;

    irs::order ord;
    auto& sort = ord.add<tests::sort::custom_sort>(false);
    sort.scorer_add = [](irs::doc_id_t& dst, const irs::doc_id_t& src)->void {
      ASSERT_TRUE(
        irs::doc_limits::invalid() == dst
        || dst == src
      );
      dst = src;
    };
    auto pord = ord.prepare();

    irs::by_phrase q;
    *q.mutable_field() = "phrase_anl";
    q.mutable_options()->push_back<irs::by_term_options>().term = irs::ref_cast<irs::byte_type>(irs::string_ref("fox"));
    q.mutable_options()->push_back<irs::by_term_options>().term = irs::ref_cast<irs::byte_type>(irs::string_ref("quick"));

    auto prepared = q.prepare(rdr, pord);

    auto sub = rdr.begin();
    auto column = sub->column_reader("name");
    ASSERT_NE(nullptr, column);
    auto values = column->values();
    auto docs = prepared->execute(*sub, pord);
    auto* freq = irs::get<irs::frequency>(*docs);
    ASSERT_TRUE(freq);
    ASSERT_FALSE(irs::get<irs::filter_boost>(*docs));
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::valid(docs->value()));
    auto docs_seek = prepared->execute(*sub, pord);
    ASSERT_FALSE(irs::doc_limits::valid(docs_seek->value()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(2, freq->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("N", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_EQ(freq->value, irs::get<irs::frequency>(*docs_seek)->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("N", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_FALSE(docs->next());
    ASSERT_TRUE(irs::doc_limits::eof(docs->value()));
    ASSERT_TRUE(irs::doc_limits::eof(docs_seek->seek(irs::doc_limits::eof())));
  }

  // wildcard_filter "zo\\_%"
  {
    irs::bytes_ref actual_value;

    irs::by_phrase q;
    *q.mutable_field() = "phrase_anl";
    auto& wt = q.mutable_options()->push_back<irs::by_wildcard_options>();
    wt.term = irs::ref_cast<irs::byte_type>(irs::string_ref("zo\\_%"));

    auto prepared = q.prepare(rdr);
    auto sub = rdr.begin();
    auto column = sub->column_reader("name");
    ASSERT_NE(nullptr, column);
    auto values = column->values();

    auto docs = prepared->execute(*sub);
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::valid(docs->value()));
    auto docs_seek = prepared->execute(*sub);
    ASSERT_FALSE(irs::doc_limits::valid(docs_seek->value()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("PHW0", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("PHW0", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_FALSE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(irs::doc_limits::eof(docs->value()));
  }

  // wildcard_filter "\\_oo"
  {
    irs::bytes_ref actual_value;

    irs::by_phrase q;
    *q.mutable_field() = "phrase_anl";
    auto& wt = q.mutable_options()->push_back<irs::by_wildcard_options>();
    wt.term = irs::ref_cast<irs::byte_type>(irs::string_ref("\\_oo"));

    auto prepared = q.prepare(rdr);
    auto sub = rdr.begin();
    auto column = sub->column_reader("name");
    ASSERT_NE(nullptr, column);
    auto values = column->values();

    auto docs = prepared->execute(*sub);
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::valid(docs->value()));
    auto docs_seek = prepared->execute(*sub);
    ASSERT_FALSE(irs::doc_limits::valid(docs_seek->value()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("PHW1", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("PHW1", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_FALSE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(irs::doc_limits::eof(docs->value()));
  }

  // wildcard_filter "z\\_o"
  {
    irs::bytes_ref actual_value;

    irs::by_phrase q;
    *q.mutable_field() = "phrase_anl";
    auto& wt = q.mutable_options()->push_back<irs::by_wildcard_options>();
    wt.term = irs::ref_cast<irs::byte_type>(irs::string_ref("z\\_o"));

    auto prepared = q.prepare(rdr);
    auto sub = rdr.begin();
    auto column = sub->column_reader("name");
    ASSERT_NE(nullptr, column);
    auto values = column->values();

    auto docs = prepared->execute(*sub);
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::valid(docs->value()));
    auto docs_seek = prepared->execute(*sub);
    ASSERT_FALSE(irs::doc_limits::valid(docs_seek->value()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("PHW2", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("PHW2", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_FALSE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(irs::doc_limits::eof(docs->value()));
  }

  // wildcard_filter "elephant giraff\\_%"
  {
    irs::bytes_ref actual_value;

    irs::by_phrase q;
    *q.mutable_field() = "phrase_anl";
    q.mutable_options()->push_back<irs::by_term_options>().term = irs::ref_cast<irs::byte_type>(irs::string_ref("elephant"));
    auto& wt = q.mutable_options()->push_back<irs::by_wildcard_options>();
    wt.term = irs::ref_cast<irs::byte_type>(irs::string_ref("giraff\\_%"));

    auto prepared = q.prepare(rdr);
    auto sub = rdr.begin();
    auto column = sub->column_reader("name");
    ASSERT_NE(nullptr, column);
    auto values = column->values();

    auto docs = prepared->execute(*sub);
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::valid(docs->value()));
    auto docs_seek = prepared->execute(*sub);
    ASSERT_FALSE(irs::doc_limits::valid(docs_seek->value()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("PHW3", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("PHW3", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_FALSE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(irs::doc_limits::eof(docs->value()));
  }

  // wildcard_filter "elephant \\_iraffe"
  {
    irs::bytes_ref actual_value;

    irs::by_phrase q;
    *q.mutable_field() = "phrase_anl";
    q.mutable_options()->push_back<irs::by_term_options>().term = irs::ref_cast<irs::byte_type>(irs::string_ref("elephant"));
    auto& wt = q.mutable_options()->push_back<irs::by_wildcard_options>();
    wt.term = irs::ref_cast<irs::byte_type>(irs::string_ref("\\_iraffe"));

    auto prepared = q.prepare(rdr);
    auto sub = rdr.begin();
    auto column = sub->column_reader("name");
    ASSERT_NE(nullptr, column);
    auto values = column->values();

    auto docs = prepared->execute(*sub);
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::valid(docs->value()));
    auto docs_seek = prepared->execute(*sub);
    ASSERT_FALSE(irs::doc_limits::valid(docs_seek->value()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("PHW4", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("PHW4", irs::to_string<irs::string_ref>(actual_value.c_str()));

    ASSERT_FALSE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(irs::doc_limits::eof(docs->value()));
  }

  // wildcard_filter "elephant gira\\_fe"
  {
    irs::bytes_ref actual_value;

    irs::by_phrase q;
    *q.mutable_field() = "phrase_anl";
    q.mutable_options()->push_back<irs::by_term_options>().term = irs::ref_cast<irs::byte_type>(irs::string_ref("elephant"));
    auto& wt = q.mutable_options()->push_back<irs::by_wildcard_options>();
    wt.term = irs::ref_cast<irs::byte_type>(irs::string_ref("gira\\_fe"));

    auto prepared = q.prepare(rdr);
    auto sub = rdr.begin();
    auto column = sub->column_reader("name");
    ASSERT_NE(nullptr, column);
    auto values = column->values();

    auto docs = prepared->execute(*sub);
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::valid(docs->value()));
    auto docs_seek = prepared->execute(*sub);
    ASSERT_FALSE(irs::doc_limits::valid(docs_seek->value()));

    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("PHW5", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
    ASSERT_TRUE(values(docs->value(), actual_value));
    ASSERT_EQ("PHW5", irs::to_string<irs::string_ref>(actual_value.c_str()));

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
  ASSERT_EQ(irs::no_boost(), q.boost());

  auto& features = irs::by_phrase::required();
  ASSERT_EQ(2, features.size());
  ASSERT_TRUE(features.check<irs::frequency>());
  ASSERT_TRUE(features.check<irs::position>());
}

TEST(by_phrase_test, boost) {
  // no boost
  {
    // no terms
    {
      irs::by_phrase q;
      *q.mutable_field() = "field";

      auto prepared = q.prepare(irs::sub_reader::empty());
      ASSERT_EQ(irs::no_boost(), prepared->boost());
    }

    // single term
    {
      irs::by_phrase q;
      *q.mutable_field() = "field";
      q.mutable_options()->push_back<irs::by_term_options>().term = irs::ref_cast<irs::byte_type>(irs::string_ref("quick"));

      auto prepared = q.prepare(irs::sub_reader::empty());
      ASSERT_EQ(irs::no_boost(), prepared->boost());
    }

    // multiple terms
    {
      irs::by_phrase q;
      *q.mutable_field() = "field";
      q.mutable_options()->push_back<irs::by_term_options>().term = irs::ref_cast<irs::byte_type>(irs::string_ref("quick"));
      q.mutable_options()->push_back<irs::by_term_options>().term = irs::ref_cast<irs::byte_type>(irs::string_ref("brown"));

      auto prepared = q.prepare(irs::sub_reader::empty());
      ASSERT_EQ(irs::no_boost(), prepared->boost());
    }
  }

  // with boost
  {
    irs::boost_t boost = 1.5f;
    
    // no terms, return empty query
    {
      irs::by_phrase q;
      *q.mutable_field() = "field";
      q.boost(boost);

      auto prepared = q.prepare(irs::sub_reader::empty());
      ASSERT_EQ(irs::no_boost(), prepared->boost());
    }

    // single term
    {
      irs::by_phrase q;
      *q.mutable_field() = "field";
      q.mutable_options()->push_back<irs::by_term_options>().term = irs::ref_cast<irs::byte_type>(irs::string_ref("quick"));
      q.boost(boost);

      auto prepared = q.prepare(irs::sub_reader::empty());
      ASSERT_EQ(boost, prepared->boost());
    }
    
    // single multiple terms 
    {
      irs::by_phrase q;
      *q.mutable_field() = "field";
      q.mutable_options()->push_back<irs::by_term_options>().term = irs::ref_cast<irs::byte_type>(irs::string_ref("quick"));
      q.mutable_options()->push_back<irs::by_term_options>().term = irs::ref_cast<irs::byte_type>(irs::string_ref("brown"));
      q.boost(boost);

      auto prepared = q.prepare(irs::sub_reader::empty());
      ASSERT_EQ(boost, prepared->boost());
    }

    // prefix, wildcard, levenshtein, set, range
    {
      irs::by_phrase q;
      *q.mutable_field() = "field";
      auto& pt = q.mutable_options()->push_back<irs::by_prefix_options>();
      pt.term = irs::ref_cast<irs::byte_type>(irs::string_ref("qui"));
      auto& wt = q.mutable_options()->push_back<irs::by_wildcard_options>();
      wt.term = irs::ref_cast<irs::byte_type>(irs::string_ref("qu__k"));
      auto& lt = q.mutable_options()->push_back<irs::by_edit_distance_filter_options>();
      lt.max_distance = 1;
      lt.term = irs::ref_cast<irs::byte_type>(irs::string_ref("brwn"));
      q.boost(boost);
      auto& st = q.mutable_options()->push_back<irs::by_terms_options>();
      st.terms.emplace(irs::ref_cast<irs::byte_type>(irs::string_ref("fox")));
      st.terms.emplace(irs::ref_cast<irs::byte_type>(irs::string_ref("dob")));
      auto& rt = q.mutable_options()->push_back<irs::by_range_options>();
      rt.range.min = irs::ref_cast<irs::byte_type>(irs::string_ref("forward"));
      rt.range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("forward"));
      rt.range.min_type = irs::BoundType::INCLUSIVE;
      rt.range.max_type = irs::BoundType::INCLUSIVE;

      auto prepared = q.prepare(irs::sub_reader::empty());
      ASSERT_EQ(boost, prepared->boost());
    }
  }
}

TEST(by_phrase_test, push_back_insert) {
  irs::by_phrase_options q;

  // push_back 
  {
    q.push_back<irs::by_term_options>().term = irs::ref_cast<irs::byte_type>(irs::string_ref("quick"));
    q.push_back<irs::by_term_options>(1).term = irs::ref_cast<irs::byte_type>(irs::string_ref("brown"));
    q.push_back<irs::by_term_options>().term = irs::ref_cast<irs::byte_type>(irs::string_ref("fox"));
    ASSERT_FALSE(q.empty());
    ASSERT_EQ(3, q.size());

    // check elements via positions
    {
      const irs::by_term_options* st1 = q.get<irs::by_term_options>(0);
      ASSERT_TRUE(st1);
      ASSERT_EQ(irs::ref_cast<irs::byte_type>(irs::string_ref("quick")), st1->term);
      const irs::by_term_options* st2 = q.get<irs::by_term_options>(2);
      ASSERT_TRUE(st2);
      ASSERT_EQ(irs::ref_cast<irs::byte_type>(irs::string_ref("brown")), st2->term);
      const irs::by_term_options* st3 = q.get<irs::by_term_options>(3);
      ASSERT_TRUE(st3);
      ASSERT_EQ(irs::ref_cast<irs::byte_type>(irs::string_ref("fox")), st3->term);
    }

    // push term 
    {
      irs::by_term_options st1;
      st1.term = irs::ref_cast<irs::byte_type>(irs::string_ref("squirrel"));
      q.push_back(st1, 0);
      const irs::by_term_options* st2 = q.get<irs::by_term_options>(4);
      ASSERT_TRUE(st2);
      ASSERT_EQ(st1, *st2);

      irs::by_prefix_options pt1;
      pt1.term = irs::ref_cast<irs::byte_type>(irs::string_ref("cat"));
      q.push_back(pt1, 0);
      const irs::by_prefix_options* pt2 = q.get<irs::by_prefix_options>(5);
      ASSERT_TRUE(pt2);
      ASSERT_EQ(pt1, *pt2);

      irs::by_wildcard_options wt1;
      wt1.term = irs::ref_cast<irs::byte_type>(irs::string_ref("dog"));
      q.push_back(wt1, 0);
      const irs::by_wildcard_options* wt2 = q.get<irs::by_wildcard_options>(6);
      ASSERT_TRUE(wt2);
      ASSERT_EQ(wt1, *wt2);

      irs::by_edit_distance_filter_options lt1;
      lt1.term = irs::ref_cast<irs::byte_type>(irs::string_ref("whale"));
      q.push_back(lt1, 0);
      const irs::by_edit_distance_filter_options* lt2 = q.get<irs::by_edit_distance_filter_options>(7);
      ASSERT_TRUE(lt2);
      ASSERT_EQ(lt1, *lt2);

      irs::by_terms_options ct1;
      ct1.terms.emplace(irs::ref_cast<irs::byte_type>(irs::string_ref("bird")));
      q.push_back(ct1, 0);
      const irs::by_terms_options* ct2 = q.get<irs::by_terms_options>(8);
      ASSERT_TRUE(ct2);
      ASSERT_EQ(ct1, *ct2);

      irs::by_range_options rt1;
      rt1.range.min = irs::ref_cast<irs::byte_type>(irs::string_ref("elephant"));
      rt1.range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("elephant"));
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
      st.term = irs::ref_cast<irs::byte_type>(irs::string_ref("jumps"));

      q.insert(std::move(st), 3);
      const irs::by_term_options* st1 = q.get<irs::by_term_options>(3);
      ASSERT_TRUE(st1);
      ASSERT_EQ(irs::ref_cast<irs::byte_type>(irs::string_ref("jumps")), st1->term);
      ASSERT_EQ(9, q.size());
    }

    {
      irs::by_term_options st;
      st.term = irs::ref_cast<irs::byte_type>(irs::string_ref("lazy"));

      q.insert(std::move(st), 9);
      const irs::by_term_options* st2 = q.get<irs::by_term_options>(9);
      ASSERT_TRUE(st2);
      ASSERT_EQ(irs::ref_cast<irs::byte_type>(irs::string_ref("lazy")), st2->term);
      ASSERT_EQ(9, q.size());
    }

    {
      irs::by_term_options st;
      st.term = irs::ref_cast<irs::byte_type>(irs::string_ref("dog"));

      q.insert(std::move(st), 28);
      const irs::by_term_options* st3 = q.get<irs::by_term_options>(28);
      ASSERT_TRUE(st3);
      ASSERT_EQ(irs::ref_cast<irs::byte_type>(irs::string_ref("dog")), st3->term);
      ASSERT_EQ(10, q.size());
    }

    {
      irs::by_term_options st1;
      st1.term = irs::ref_cast<irs::byte_type>(irs::string_ref("squirrel"));
      q.insert(st1, 5);
      const irs::by_term_options* st2 = q.get<irs::by_term_options>(5);
      ASSERT_TRUE(st2);
      ASSERT_EQ(st1, *st2);
      ASSERT_EQ(10, q.size());

      irs::by_prefix_options pt1;
      pt1.term = irs::ref_cast<irs::byte_type>(irs::string_ref("cat"));
      q.insert(pt1, 7);
      const irs::by_prefix_options* pt2 = q.get<irs::by_prefix_options>(7);
      ASSERT_TRUE(pt2);
      ASSERT_EQ(pt1, *pt2);
      ASSERT_EQ(10, q.size());

      irs::by_wildcard_options wt1;
      wt1.term = irs::ref_cast<irs::byte_type>(irs::string_ref("dog"));
      q.insert(wt1, 9);
      const irs::by_wildcard_options* wt2 = q.get<irs::by_wildcard_options>(9);
      ASSERT_TRUE(wt2);
      ASSERT_EQ(wt1, *wt2);
      ASSERT_EQ(10, q.size());

      irs::by_edit_distance_filter_options lt1;
      lt1.term = irs::ref_cast<irs::byte_type>(irs::string_ref("whale"));
      q.insert(lt1, 29);
      const irs::by_edit_distance_filter_options* lt2 = q.get<irs::by_edit_distance_filter_options>(29);
      ASSERT_TRUE(lt2);
      ASSERT_EQ(lt1, *lt2);
      ASSERT_EQ(11, q.size());

      irs::by_terms_options ct1;
      ct1.terms.emplace(irs::ref_cast<irs::byte_type>(irs::string_ref("bird")));
      q.insert(ct1, 29);
      const irs::by_terms_options* ct2 = q.get<irs::by_terms_options>(29);
      ASSERT_TRUE(ct2);
      ASSERT_EQ(ct1, *ct2);
      ASSERT_EQ(11, q.size());

      irs::by_range_options rt1;
      rt1.range.min = irs::ref_cast<irs::byte_type>(irs::string_ref("elephant"));
      rt1.range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("elephant"));
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
    q0.mutable_options()->push_back<irs::by_term_options>().term = irs::ref_cast<irs::byte_type>(irs::string_ref("quick"));
    q0.mutable_options()->push_back<irs::by_term_options>().term = irs::ref_cast<irs::byte_type>(irs::string_ref("brown"));

    irs::by_phrase q1;
    *q1.mutable_field() = "name";
    q1.mutable_options()->push_back<irs::by_term_options>().term = irs::ref_cast<irs::byte_type>(irs::string_ref("quick"));
    q1.mutable_options()->push_back<irs::by_term_options>().term = irs::ref_cast<irs::byte_type>(irs::string_ref("brown"));
    ASSERT_EQ(q0, q1);
    ASSERT_EQ(q0.hash(), q1.hash());
  }

  {
    irs::by_phrase q0;
    {
      *q0.mutable_field() = "name";
      auto& pt1 = q0.mutable_options()->push_back<irs::by_prefix_options>();
      pt1.term = irs::ref_cast<irs::byte_type>(irs::string_ref("qui"));
      auto& ct1 = q0.mutable_options()->push_back<irs::by_terms_options>();
      ct1.terms.emplace(irs::ref_cast<irs::byte_type>(irs::string_ref("light")));
      ct1.terms.emplace(irs::ref_cast<irs::byte_type>(irs::string_ref("dark")));
      auto& wt1 = q0.mutable_options()->push_back<irs::by_wildcard_options>();
      wt1.term = irs::ref_cast<irs::byte_type>(irs::string_ref("br_wn"));
      auto& lt1 = q0.mutable_options()->push_back<irs::by_edit_distance_filter_options>();
      lt1.max_distance = 2;
      lt1.term = irs::ref_cast<irs::byte_type>(irs::string_ref("fo"));
      auto& rt1 = q0.mutable_options()->push_back<irs::by_range_options>();
      rt1.range.min = irs::ref_cast<irs::byte_type>(irs::string_ref("elephant"));
      rt1.range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("elephant"));
      rt1.range.min_type = irs::BoundType::INCLUSIVE;
      rt1.range.max_type = irs::BoundType::INCLUSIVE;
    }

    irs::by_phrase q1;
    {
      *q1.mutable_field() = "name";
      auto& pt1 = q1.mutable_options()->push_back<irs::by_prefix_options>();
      pt1.term = irs::ref_cast<irs::byte_type>(irs::string_ref("qui"));
      auto& ct1 = q1.mutable_options()->push_back<irs::by_terms_options>();
      ct1.terms.emplace(irs::ref_cast<irs::byte_type>(irs::string_ref("light")));
      ct1.terms.emplace(irs::ref_cast<irs::byte_type>(irs::string_ref("dark")));
      auto& wt1 = q1.mutable_options()->push_back<irs::by_wildcard_options>();
      wt1.term = irs::ref_cast<irs::byte_type>(irs::string_ref("br_wn"));
      auto& lt1 = q1.mutable_options()->push_back<irs::by_edit_distance_filter_options>();
      lt1.max_distance = 2;
      lt1.term = irs::ref_cast<irs::byte_type>(irs::string_ref("fo"));
      auto& rt1 = q1.mutable_options()->push_back<irs::by_range_options>();
      rt1.range.min = irs::ref_cast<irs::byte_type>(irs::string_ref("elephant"));
      rt1.range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("elephant"));
      rt1.range.min_type = irs::BoundType::INCLUSIVE;
      rt1.range.max_type = irs::BoundType::INCLUSIVE;
    }

    ASSERT_EQ(q0, q1);
    ASSERT_EQ(q0.hash(), q1.hash());
  }

  {
    irs::by_phrase q0;
    *q0.mutable_field() = "name";
    q0.mutable_options()->push_back<irs::by_term_options>().term = irs::ref_cast<irs::byte_type>(irs::string_ref("quick"));
    q0.mutable_options()->push_back<irs::by_term_options>().term = irs::ref_cast<irs::byte_type>(irs::string_ref("squirrel"));

    irs::by_phrase q1;
    *q1.mutable_field() = "name";
    q1.mutable_options()->push_back<irs::by_term_options>().term = irs::ref_cast<irs::byte_type>(irs::string_ref("quick"));
    q1.mutable_options()->push_back<irs::by_term_options>().term = irs::ref_cast<irs::byte_type>(irs::string_ref("brown"));
    ASSERT_NE(q0, q1);
  }

  {
    irs::by_phrase q0;
    *q0.mutable_field() = "name1";
    q0.mutable_options()->push_back<irs::by_term_options>().term = irs::ref_cast<irs::byte_type>(irs::string_ref("quick"));
    q0.mutable_options()->push_back<irs::by_term_options>().term = irs::ref_cast<irs::byte_type>(irs::string_ref("brown"));

    irs::by_phrase q1;
    *q1.mutable_field() = "name";
    q1.mutable_options()->push_back<irs::by_term_options>().term = irs::ref_cast<irs::byte_type>(irs::string_ref("quick"));
    q1.mutable_options()->push_back<irs::by_term_options>().term = irs::ref_cast<irs::byte_type>(irs::string_ref("brown"));
    ASSERT_NE(q0, q1);
  }

  {
    irs::by_phrase q0;
    *q0.mutable_field() = "name";
    q0.mutable_options()->push_back<irs::by_term_options>().term = irs::ref_cast<irs::byte_type>(irs::string_ref("quick"));

    irs::by_phrase q1;
    *q1.mutable_field() = "name";
    q1.mutable_options()->push_back<irs::by_term_options>().term = irs::ref_cast<irs::byte_type>(irs::string_ref("quick"));
    q1.mutable_options()->push_back<irs::by_term_options>().term = irs::ref_cast<irs::byte_type>(irs::string_ref("brown"));
    ASSERT_NE(q0, q1);
  }

  {
    irs::by_phrase q0;
    {
      *q0.mutable_field() = "name";
      auto& pt1 = q0.mutable_options()->push_back<irs::by_prefix_options>();
      pt1.term = irs::ref_cast<irs::byte_type>(irs::string_ref("quil"));
      auto& ct1 = q0.mutable_options()->push_back<irs::by_terms_options>();
      ct1.terms.emplace(irs::ref_cast<irs::byte_type>(irs::string_ref("light")));
      ct1.terms.emplace(irs::ref_cast<irs::byte_type>(irs::string_ref("dark")));
      auto& wt1 = q0.mutable_options()->push_back<irs::by_wildcard_options>();
      wt1.term = irs::ref_cast<irs::byte_type>(irs::string_ref("br_wn"));
      auto& lt1 = q0.mutable_options()->push_back<irs::by_edit_distance_filter_options>();
      lt1.max_distance = 2;
      lt1.term = irs::ref_cast<irs::byte_type>(irs::string_ref("fo"));
      auto& rt1 = q0.mutable_options()->push_back<irs::by_range_options>();
      rt1.range.min = irs::ref_cast<irs::byte_type>(irs::string_ref("elephant"));
      rt1.range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("elephant"));
      rt1.range.min_type = irs::BoundType::INCLUSIVE;
      rt1.range.max_type = irs::BoundType::INCLUSIVE;
    }

    irs::by_phrase q1;
    {
      *q1.mutable_field() = "name";
      auto& pt1 = q1.mutable_options()->push_back<irs::by_prefix_options>();
      pt1.term = irs::ref_cast<irs::byte_type>(irs::string_ref("qui"));
      auto& ct1 = q1.mutable_options()->push_back<irs::by_terms_options>();
      ct1.terms.emplace(irs::ref_cast<irs::byte_type>(irs::string_ref("light")));
      ct1.terms.emplace(irs::ref_cast<irs::byte_type>(irs::string_ref("dark")));
      auto& wt1 = q1.mutable_options()->push_back<irs::by_wildcard_options>();
      wt1.term = irs::ref_cast<irs::byte_type>(irs::string_ref("br_wn"));
      auto& lt1 = q1.mutable_options()->push_back<irs::by_edit_distance_filter_options>();
      lt1.max_distance = 2;
      lt1.term = irs::ref_cast<irs::byte_type>(irs::string_ref("fo"));
      auto& rt1 = q1.mutable_options()->push_back<irs::by_range_options>();
      rt1.range.min = irs::ref_cast<irs::byte_type>(irs::string_ref("elephant"));
      rt1.range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("elephant"));
      rt1.range.min_type = irs::BoundType::INCLUSIVE;
      rt1.range.max_type = irs::BoundType::INCLUSIVE;
    }

    ASSERT_NE(q0, q1);
  }
}

TEST(by_phrase_test, copy_move) {
  {
    irs::by_term_options st;
    st.term = irs::ref_cast<irs::byte_type>(irs::string_ref("very"));
    irs::by_prefix_options pt;
    pt.term = irs::ref_cast<irs::byte_type>(irs::string_ref("qui"));
    irs::by_terms_options ct;
    ct.terms.emplace(irs::ref_cast<irs::byte_type>(irs::string_ref("light")));
    ct.terms.emplace(irs::ref_cast<irs::byte_type>(irs::string_ref("dark")));
    irs::by_wildcard_options wt;
    wt.term = irs::ref_cast<irs::byte_type>(irs::string_ref("br_wn"));
    irs::by_edit_distance_filter_options lt;
    lt.max_distance = 2;
    lt.term = irs::ref_cast<irs::byte_type>(irs::string_ref("fo"));
    irs::by_range_options rt;
    rt.range.min = irs::ref_cast<irs::byte_type>(irs::string_ref("elephant"));
    rt.range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("elephant"));
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
    ASSERT_EQ(q0.hash(), q1.hash());
    irs::by_phrase q2 = q0;
    irs::by_phrase q3 = std::move(q2);
    ASSERT_EQ(q0, q3);
    ASSERT_EQ(q0.hash(), q3.hash());
  }
}

INSTANTIATE_TEST_CASE_P(
  phrase_filter_test,
  phrase_filter_test_case,
  ::testing::Combine(
    ::testing::Values(
      &tests::memory_directory,
      &tests::fs_directory,
      &tests::mmap_directory
    ),
    ::testing::Values(tests::format_info{"1_0"},
                      tests::format_info{"1_3", "1_0"})
  ),
  tests::to_string
);
