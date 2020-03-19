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

NS_BEGIN(tests)

void analyzed_json_field_factory(
    tests::document& doc,
    const std::string& name,
    const tests::json_doc_generator::json_value& data) {
  typedef templates::text_field<std::string> text_field;
 
  class string_field : public templates::string_field {
   public:
    string_field(const irs::string_ref& name, const irs::string_ref& value)
      : templates::string_field(name, value) {
    }

    const irs::flags& features() const {
      static irs::flags features{ irs::frequency::type() };
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
      irs::string_ref(name),
      data.str
    ));
  }
}

NS_END

class phrase_filter_test_case : public tests::filter_test_case_base {
 protected:
  void sequential() {
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
      auto& doc = docs->attributes().get<irs::document>();
      ASSERT_TRUE(bool(doc));
      ASSERT_EQ(docs->value(), doc->value);
      ASSERT_TRUE(irs::type_limits<irs::type_t::doc_id_t>::eof(docs->value()));
    }

    // empty phrase
    {
      irs::by_phrase q;
      q.field("phrase_anl");

      auto prepared = q.prepare(rdr);
      auto sub = rdr.begin();

      auto docs = prepared->execute(*sub);
      auto& doc = docs->attributes().get<irs::document>();
      ASSERT_TRUE(bool(doc));
      ASSERT_EQ(docs->value(), doc->value);
      ASSERT_TRUE(irs::type_limits<irs::type_t::doc_id_t>::eof(docs->value()));
    }

    // equals to term_filter "fox"
    {
      irs::bytes_ref actual_value;

      irs::by_phrase q;
      q.field("phrase_anl")
       .push_back(irs::by_phrase::simple_term{irs::ref_cast<irs::byte_type>(irs::string_ref("fox"))});

      auto prepared = q.prepare(rdr);
      auto sub = rdr.begin();
      auto column = sub->column_reader("name");
      ASSERT_NE(nullptr, column);
      auto values = column->values();

      auto docs = prepared->execute(*sub);
      auto& doc = docs->attributes().get<irs::document>();
      ASSERT_TRUE(bool(doc));
      ASSERT_EQ(docs->value(), doc->value);
      ASSERT_FALSE(irs::type_limits<irs::type_t::doc_id_t>::valid(docs->value()));
      auto docs_seek = prepared->execute(*sub);
      ASSERT_FALSE(irs::type_limits<irs::type_t::doc_id_t>::valid(docs_seek->value()));

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
      ASSERT_TRUE(irs::type_limits<irs::type_t::doc_id_t>::eof(docs->value()));
    }

    // prefix_filter "fo*"
    {
      irs::bytes_ref actual_value;

      irs::by_phrase q;
      irs::by_phrase::prefix_term pt;
      pt.term = irs::ref_cast<irs::byte_type>(irs::string_ref("fo"));
      q.field("phrase_anl").push_back(std::move(pt));

      auto prepared = q.prepare(rdr);
      auto sub = rdr.begin();
      auto column = sub->column_reader("name");
      ASSERT_NE(nullptr, column);
      auto values = column->values();

      auto docs = prepared->execute(*sub);
      auto& doc = docs->attributes().get<irs::document>();
      ASSERT_TRUE(bool(doc));
      ASSERT_EQ(docs->value(), doc->value);
      ASSERT_FALSE(irs::type_limits<irs::type_t::doc_id_t>::valid(docs->value()));
      auto docs_seek = prepared->execute(*sub);
      ASSERT_FALSE(irs::type_limits<irs::type_t::doc_id_t>::valid(docs_seek->value()));

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
      ASSERT_TRUE(irs::type_limits<irs::type_t::doc_id_t>::eof(docs->value()));
    }

    // wildcard_filter "fo%"
    {
      irs::bytes_ref actual_value;

      irs::by_phrase q;
      irs::by_phrase::wildcard_term wt;
      wt.term = irs::ref_cast<irs::byte_type>(irs::string_ref("fo%"));
      q.field("phrase_anl").push_back(std::move(wt));

      auto prepared = q.prepare(rdr);
      auto sub = rdr.begin();
      auto column = sub->column_reader("name");
      ASSERT_NE(nullptr, column);
      auto values = column->values();

      auto docs = prepared->execute(*sub);
      auto& doc = docs->attributes().get<irs::document>();
      ASSERT_TRUE(bool(doc));
      ASSERT_EQ(docs->value(), doc->value);
      ASSERT_FALSE(irs::type_limits<irs::type_t::doc_id_t>::valid(docs->value()));
      auto docs_seek = prepared->execute(*sub);
      ASSERT_FALSE(irs::type_limits<irs::type_t::doc_id_t>::valid(docs_seek->value()));

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
      ASSERT_TRUE(irs::type_limits<irs::type_t::doc_id_t>::eof(docs->value()));
    }

    // wildcard_filter "%ox"
    {
      irs::bytes_ref actual_value;

      irs::by_phrase q;
      irs::by_phrase::wildcard_term wt;
      wt.term = irs::ref_cast<irs::byte_type>(irs::string_ref("%ox"));
      q.field("phrase_anl").push_back(std::move(wt));

      auto prepared = q.prepare(rdr);
      auto sub = rdr.begin();
      auto column = sub->column_reader("name");
      ASSERT_NE(nullptr, column);
      auto values = column->values();

      auto docs = prepared->execute(*sub);
      auto& doc = docs->attributes().get<irs::document>();
      ASSERT_TRUE(bool(doc));
      ASSERT_EQ(docs->value(), doc->value);
      ASSERT_FALSE(irs::type_limits<irs::type_t::doc_id_t>::valid(docs->value()));
      auto docs_seek = prepared->execute(*sub);
      ASSERT_FALSE(irs::type_limits<irs::type_t::doc_id_t>::valid(docs_seek->value()));

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
      ASSERT_TRUE(irs::type_limits<irs::type_t::doc_id_t>::eof(docs->value()));
    }

    // wildcard_filter "f%x"
    {
      irs::bytes_ref actual_value;

      irs::by_phrase q;
      irs::by_phrase::wildcard_term wt;
      wt.term = irs::ref_cast<irs::byte_type>(irs::string_ref("f%x"));
      q.field("phrase_anl").push_back(std::move(wt));

      auto prepared = q.prepare(rdr);
      auto sub = rdr.begin();
      auto column = sub->column_reader("name");
      ASSERT_NE(nullptr, column);
      auto values = column->values();

      auto docs = prepared->execute(*sub);
      auto& doc = docs->attributes().get<irs::document>();
      ASSERT_TRUE(bool(doc));
      ASSERT_EQ(docs->value(), doc->value);
      ASSERT_FALSE(irs::type_limits<irs::type_t::doc_id_t>::valid(docs->value()));
      auto docs_seek = prepared->execute(*sub);
      ASSERT_FALSE(irs::type_limits<irs::type_t::doc_id_t>::valid(docs_seek->value()));

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
      ASSERT_TRUE(irs::type_limits<irs::type_t::doc_id_t>::eof(docs->value()));
    }

    // wildcard_filter "_ox"
    {
      irs::bytes_ref actual_value;

      irs::by_phrase q;
      irs::by_phrase::wildcard_term wt;
      wt.term = irs::ref_cast<irs::byte_type>(irs::string_ref("_ox"));
      q.field("phrase_anl").push_back(std::move(wt));

      auto prepared = q.prepare(rdr);
      auto sub = rdr.begin();
      auto column = sub->column_reader("name");
      ASSERT_NE(nullptr, column);
      auto values = column->values();

      auto docs = prepared->execute(*sub);
      auto& doc = docs->attributes().get<irs::document>();
      ASSERT_TRUE(bool(doc));
      ASSERT_EQ(docs->value(), doc->value);
      ASSERT_FALSE(irs::type_limits<irs::type_t::doc_id_t>::valid(docs->value()));
      auto docs_seek = prepared->execute(*sub);
      ASSERT_FALSE(irs::type_limits<irs::type_t::doc_id_t>::valid(docs_seek->value()));

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
      ASSERT_TRUE(irs::type_limits<irs::type_t::doc_id_t>::eof(docs->value()));
    }

    // wildcard_filter "f_x"
    {
      irs::bytes_ref actual_value;

      irs::by_phrase q;
      irs::by_phrase::wildcard_term wt;
      wt.term = irs::ref_cast<irs::byte_type>(irs::string_ref("f_x"));
      q.field("phrase_anl").push_back(std::move(wt));

      auto prepared = q.prepare(rdr);
      auto sub = rdr.begin();
      auto column = sub->column_reader("name");
      ASSERT_NE(nullptr, column);
      auto values = column->values();

      auto docs = prepared->execute(*sub);
      auto& doc = docs->attributes().get<irs::document>();
      ASSERT_TRUE(bool(doc));
      ASSERT_EQ(docs->value(), doc->value);
      ASSERT_FALSE(irs::type_limits<irs::type_t::doc_id_t>::valid(docs->value()));
      auto docs_seek = prepared->execute(*sub);
      ASSERT_FALSE(irs::type_limits<irs::type_t::doc_id_t>::valid(docs_seek->value()));

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
      ASSERT_TRUE(irs::type_limits<irs::type_t::doc_id_t>::eof(docs->value()));
    }

    // wildcard_filter "fo_"
    {
      irs::bytes_ref actual_value;

      irs::by_phrase q;
      irs::by_phrase::wildcard_term wt;
      wt.term = irs::ref_cast<irs::byte_type>(irs::string_ref("fo_"));
      q.field("phrase_anl").push_back(std::move(wt));

      auto prepared = q.prepare(rdr);
      auto sub = rdr.begin();
      auto column = sub->column_reader("name");
      ASSERT_NE(nullptr, column);
      auto values = column->values();

      auto docs = prepared->execute(*sub);
      auto& doc = docs->attributes().get<irs::document>();
      ASSERT_TRUE(bool(doc));
      ASSERT_EQ(docs->value(), doc->value);
      ASSERT_FALSE(irs::type_limits<irs::type_t::doc_id_t>::valid(docs->value()));
      auto docs_seek = prepared->execute(*sub);
      ASSERT_FALSE(irs::type_limits<irs::type_t::doc_id_t>::valid(docs_seek->value()));

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
      ASSERT_TRUE(irs::type_limits<irs::type_t::doc_id_t>::eof(docs->value()));
    }

    // wildcard_filter "fox"
    {
      irs::bytes_ref actual_value;

      irs::by_phrase q;
      irs::by_phrase::wildcard_term wt;
      wt.term = irs::ref_cast<irs::byte_type>(irs::string_ref("fox"));
      q.field("phrase_anl").push_back(std::move(wt));

      auto prepared = q.prepare(rdr);
      auto sub = rdr.begin();
      auto column = sub->column_reader("name");
      ASSERT_NE(nullptr, column);
      auto values = column->values();

      auto docs = prepared->execute(*sub);
      auto& doc = docs->attributes().get<irs::document>();
      ASSERT_TRUE(bool(doc));
      ASSERT_EQ(docs->value(), doc->value);
      ASSERT_FALSE(irs::type_limits<irs::type_t::doc_id_t>::valid(docs->value()));
      auto docs_seek = prepared->execute(*sub);
      ASSERT_FALSE(irs::type_limits<irs::type_t::doc_id_t>::valid(docs_seek->value()));

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
      ASSERT_TRUE(irs::type_limits<irs::type_t::doc_id_t>::eof(docs->value()));
    }

    // set_term "fox"
    {
      irs::bytes_ref actual_value;

      irs::by_phrase q;
      q.field("phrase_anl").push_back(irs::by_phrase::set_term{{irs::ref_cast<irs::byte_type>(irs::string_ref("fox"))}});

      auto prepared = q.prepare(rdr);
      auto sub = rdr.begin();
      auto column = sub->column_reader("name");
      ASSERT_NE(nullptr, column);
      auto values = column->values();

      auto docs = prepared->execute(*sub);
      auto& doc = docs->attributes().get<irs::document>();
      ASSERT_TRUE(bool(doc));
      ASSERT_EQ(docs->value(), doc->value);
      ASSERT_FALSE(irs::type_limits<irs::type_t::doc_id_t>::valid(docs->value()));
      auto docs_seek = prepared->execute(*sub);
      ASSERT_FALSE(irs::type_limits<irs::type_t::doc_id_t>::valid(docs_seek->value()));

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
      ASSERT_TRUE(irs::type_limits<irs::type_t::doc_id_t>::eof(docs->value()));
    }

    // set_term "fox|that"
    {
      irs::bytes_ref actual_value;

      irs::by_phrase q;
      q.field("phrase_anl").push_back(irs::by_phrase::set_term{{irs::ref_cast<irs::byte_type>(irs::string_ref("fox")), irs::ref_cast<irs::byte_type>(irs::string_ref("that"))}});

      auto prepared = q.prepare(rdr);
      auto sub = rdr.begin();
      auto column = sub->column_reader("name");
      ASSERT_NE(nullptr, column);
      auto values = column->values();

      auto docs = prepared->execute(*sub);
      auto& doc = docs->attributes().get<irs::document>();
      ASSERT_TRUE(bool(doc));
      ASSERT_EQ(docs->value(), doc->value);
      ASSERT_FALSE(irs::type_limits<irs::type_t::doc_id_t>::valid(docs->value()));
      auto docs_seek = prepared->execute(*sub);
      ASSERT_FALSE(irs::type_limits<irs::type_t::doc_id_t>::valid(docs_seek->value()));

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
      ASSERT_TRUE(irs::type_limits<irs::type_t::doc_id_t>::eof(docs->value()));
    }

    // wildcard_filter "%"
    {
      irs::bytes_ref actual_value;

      irs::by_phrase q;
      irs::by_phrase::wildcard_term wt;
      wt.term = irs::ref_cast<irs::byte_type>(irs::string_ref("%"));
      q.field("phrase_anl").push_back(std::move(wt));

      auto prepared = q.prepare(rdr);
      auto sub = rdr.begin();
      auto column = sub->column_reader("name");
      ASSERT_NE(nullptr, column);
      auto values = column->values();

      auto docs = prepared->execute(*sub);
      auto& doc = docs->attributes().get<irs::document>();
      ASSERT_TRUE(bool(doc));
      ASSERT_EQ(docs->value(), doc->value);
      ASSERT_FALSE(irs::type_limits<irs::type_t::doc_id_t>::valid(docs->value()));
      auto docs_seek = prepared->execute(*sub);
      ASSERT_FALSE(irs::type_limits<irs::type_t::doc_id_t>::valid(docs_seek->value()));

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
      ASSERT_EQ("C", irs::to_string<irs::string_ref>(actual_value.c_str()));
      ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
      ASSERT_TRUE(values(docs->value(), actual_value));
      ASSERT_EQ("C", irs::to_string<irs::string_ref>(actual_value.c_str()));

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
      ASSERT_EQ("E", irs::to_string<irs::string_ref>(actual_value.c_str()));
      ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
      ASSERT_TRUE(values(docs->value(), actual_value));
      ASSERT_EQ("E", irs::to_string<irs::string_ref>(actual_value.c_str()));

      ASSERT_TRUE(docs->next());
      ASSERT_EQ(docs->value(), doc->value);
      ASSERT_TRUE(values(docs->value(), actual_value));
      ASSERT_EQ("F", irs::to_string<irs::string_ref>(actual_value.c_str()));
      ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
      ASSERT_TRUE(values(docs->value(), actual_value));
      ASSERT_EQ("F", irs::to_string<irs::string_ref>(actual_value.c_str()));

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
      ASSERT_EQ("M", irs::to_string<irs::string_ref>(actual_value.c_str()));
      ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
      ASSERT_TRUE(values(docs->value(), actual_value));
      ASSERT_EQ("M", irs::to_string<irs::string_ref>(actual_value.c_str()));

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
      ASSERT_EQ("O", irs::to_string<irs::string_ref>(actual_value.c_str()));
      ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
      ASSERT_TRUE(values(docs->value(), actual_value));
      ASSERT_EQ("O", irs::to_string<irs::string_ref>(actual_value.c_str()));

      ASSERT_TRUE(docs->next());
      ASSERT_EQ(docs->value(), doc->value);
      ASSERT_TRUE(values(docs->value(), actual_value));
      ASSERT_EQ("P", irs::to_string<irs::string_ref>(actual_value.c_str()));
      ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
      ASSERT_TRUE(values(docs->value(), actual_value));
      ASSERT_EQ("P", irs::to_string<irs::string_ref>(actual_value.c_str()));

      ASSERT_TRUE(docs->next());
      ASSERT_EQ(docs->value(), doc->value);
      ASSERT_TRUE(values(docs->value(), actual_value));
      ASSERT_EQ("Q", irs::to_string<irs::string_ref>(actual_value.c_str()));
      ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
      ASSERT_TRUE(values(docs->value(), actual_value));
      ASSERT_EQ("Q", irs::to_string<irs::string_ref>(actual_value.c_str()));

      ASSERT_TRUE(docs->next());
      ASSERT_EQ(docs->value(), doc->value);
      ASSERT_TRUE(values(docs->value(), actual_value));
      ASSERT_EQ("R", irs::to_string<irs::string_ref>(actual_value.c_str()));
      ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
      ASSERT_TRUE(values(docs->value(), actual_value));
      ASSERT_EQ("R", irs::to_string<irs::string_ref>(actual_value.c_str()));

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

      ASSERT_TRUE(docs->next());
      ASSERT_EQ(docs->value(), doc->value);
      ASSERT_TRUE(values(docs->value(), actual_value));
      ASSERT_EQ("Z", irs::to_string<irs::string_ref>(actual_value.c_str()));
      ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
      ASSERT_TRUE(values(docs->value(), actual_value));
      ASSERT_EQ("Z", irs::to_string<irs::string_ref>(actual_value.c_str()));

      ASSERT_TRUE(docs->next());
      ASSERT_EQ(docs->value(), doc->value);
      ASSERT_TRUE(values(docs->value(), actual_value));
      ASSERT_EQ("PHW0", irs::to_string<irs::string_ref>(actual_value.c_str()));
      ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
      ASSERT_TRUE(values(docs->value(), actual_value));
      ASSERT_EQ("PHW0", irs::to_string<irs::string_ref>(actual_value.c_str()));

      ASSERT_TRUE(docs->next());
      ASSERT_EQ(docs->value(), doc->value);
      ASSERT_TRUE(values(docs->value(), actual_value));
      ASSERT_EQ("PHW1", irs::to_string<irs::string_ref>(actual_value.c_str()));
      ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
      ASSERT_TRUE(values(docs->value(), actual_value));
      ASSERT_EQ("PHW1", irs::to_string<irs::string_ref>(actual_value.c_str()));

      ASSERT_TRUE(docs->next());
      ASSERT_EQ(docs->value(), doc->value);
      ASSERT_TRUE(values(docs->value(), actual_value));
      ASSERT_EQ("PHW2", irs::to_string<irs::string_ref>(actual_value.c_str()));
      ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
      ASSERT_TRUE(values(docs->value(), actual_value));
      ASSERT_EQ("PHW2", irs::to_string<irs::string_ref>(actual_value.c_str()));

      ASSERT_TRUE(docs->next());
      ASSERT_EQ(docs->value(), doc->value);
      ASSERT_TRUE(values(docs->value(), actual_value));
      ASSERT_EQ("PHW3", irs::to_string<irs::string_ref>(actual_value.c_str()));
      ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
      ASSERT_TRUE(values(docs->value(), actual_value));
      ASSERT_EQ("PHW3", irs::to_string<irs::string_ref>(actual_value.c_str()));

      ASSERT_TRUE(docs->next());
      ASSERT_EQ(docs->value(), doc->value);
      ASSERT_TRUE(values(docs->value(), actual_value));
      ASSERT_EQ("PHW4", irs::to_string<irs::string_ref>(actual_value.c_str()));
      ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
      ASSERT_TRUE(values(docs->value(), actual_value));
      ASSERT_EQ("PHW4", irs::to_string<irs::string_ref>(actual_value.c_str()));

      ASSERT_TRUE(docs->next());
      ASSERT_EQ(docs->value(), doc->value);
      ASSERT_TRUE(values(docs->value(), actual_value));
      ASSERT_EQ("PHW5", irs::to_string<irs::string_ref>(actual_value.c_str()));
      ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
      ASSERT_TRUE(values(docs->value(), actual_value));
      ASSERT_EQ("PHW5", irs::to_string<irs::string_ref>(actual_value.c_str()));

      ASSERT_FALSE(docs->next());
      ASSERT_EQ(docs->value(), doc->value);
      ASSERT_TRUE(irs::type_limits<irs::type_t::doc_id_t>::eof(docs->value()));
    }

    // levenshtein_filter "fox"
    {
      irs::bytes_ref actual_value;

      irs::by_phrase q;
      irs::by_phrase::levenshtein_term lt;
      lt.max_distance = 0;
      lt.term = irs::ref_cast<irs::byte_type>(irs::string_ref("fox"));
      q.field("phrase_anl").push_back(std::move(lt));

      auto prepared = q.prepare(rdr);
      auto sub = rdr.begin();
      auto column = sub->column_reader("name");
      ASSERT_NE(nullptr, column);
      auto values = column->values();

      auto docs = prepared->execute(*sub);
      auto& doc = docs->attributes().get<irs::document>();
      ASSERT_TRUE(bool(doc));
      ASSERT_EQ(docs->value(), doc->value);
      ASSERT_FALSE(irs::type_limits<irs::type_t::doc_id_t>::valid(docs->value()));
      auto docs_seek = prepared->execute(*sub);
      ASSERT_FALSE(irs::type_limits<irs::type_t::doc_id_t>::valid(docs_seek->value()));

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
      ASSERT_TRUE(irs::type_limits<irs::type_t::doc_id_t>::eof(docs->value()));
    }

    // levenshtein_filter "fol"
    {
      irs::bytes_ref actual_value;

      irs::by_phrase q;
      irs::by_phrase::levenshtein_term lt;
      lt.max_distance = 1;
      lt.term = irs::ref_cast<irs::byte_type>(irs::string_ref("fol"));
      q.field("phrase_anl").push_back(std::move(lt));

      auto prepared = q.prepare(rdr);
      auto sub = rdr.begin();
      auto column = sub->column_reader("name");
      ASSERT_NE(nullptr, column);
      auto values = column->values();

      auto docs = prepared->execute(*sub);
      auto& doc = docs->attributes().get<irs::document>();
      ASSERT_TRUE(bool(doc));
      ASSERT_EQ(docs->value(), doc->value);
      ASSERT_FALSE(irs::type_limits<irs::type_t::doc_id_t>::valid(docs->value()));
      auto docs_seek = prepared->execute(*sub);
      ASSERT_FALSE(irs::type_limits<irs::type_t::doc_id_t>::valid(docs_seek->value()));

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
      ASSERT_TRUE(irs::type_limits<irs::type_t::doc_id_t>::eof(docs->value()));
    }

    // search "fox" on field without positions
    // which is ok for single word phrases
    {
      irs::by_phrase q;
      q.field("phrase").push_back(irs::by_phrase::simple_term{irs::ref_cast<irs::byte_type>(irs::string_ref("fox"))});

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
      auto& doc = docs->attributes().get<irs::document>();
      ASSERT_TRUE(bool(doc));
      ASSERT_EQ(docs->value(), doc->value);
      ASSERT_FALSE(irs::type_limits<irs::type_t::doc_id_t>::valid(docs->value()));
      auto docs_seek = prepared->execute(*sub);
      ASSERT_FALSE(irs::type_limits<irs::type_t::doc_id_t>::valid(docs_seek->value()));

      ASSERT_TRUE(docs->next());
      ASSERT_EQ(docs->value(), doc->value);
      ASSERT_TRUE(values(docs->value(), actual_value));
      ASSERT_EQ("K", irs::to_string<irs::string_ref>(actual_value.c_str()));
      ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
      ASSERT_TRUE(values(docs->value(), actual_value));
      ASSERT_EQ("K", irs::to_string<irs::string_ref>(actual_value.c_str()));

      ASSERT_FALSE(docs->next());
      ASSERT_EQ(docs->value(), doc->value);
      ASSERT_TRUE(irs::type_limits<irs::type_t::doc_id_t>::eof(docs->value()));
    }

    // search "fo*" on field without positions
    // which is ok for the first word in phrase
    {
      irs::by_phrase q;
      irs::by_phrase::prefix_term pt;
      pt.term = irs::ref_cast<irs::byte_type>(irs::string_ref("fo"));
      q.field("phrase").push_back(std::move(pt));

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
      auto& doc = docs->attributes().get<irs::document>();
      ASSERT_TRUE(bool(doc));
      ASSERT_EQ(docs->value(), doc->value);
      ASSERT_FALSE(irs::type_limits<irs::type_t::doc_id_t>::valid(docs->value()));
      auto docs_seek = prepared->execute(*sub);
      ASSERT_FALSE(irs::type_limits<irs::type_t::doc_id_t>::valid(docs_seek->value()));

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
      ASSERT_TRUE(irs::type_limits<irs::type_t::doc_id_t>::eof(docs->value()));
    }

    // search "fo%" on field without positions
    // which is ok for first word in phrase
    {
      irs::by_phrase q;
      irs::by_phrase::wildcard_term wt;
      wt.term = irs::ref_cast<irs::byte_type>(irs::string_ref("fo%"));
      q.field("phrase").push_back(std::move(wt));

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
      auto& doc = docs->attributes().get<irs::document>();
      ASSERT_TRUE(bool(doc));
      ASSERT_EQ(docs->value(), doc->value);
      ASSERT_FALSE(irs::type_limits<irs::type_t::doc_id_t>::valid(docs->value()));
      auto docs_seek = prepared->execute(*sub);
      ASSERT_FALSE(irs::type_limits<irs::type_t::doc_id_t>::valid(docs_seek->value()));

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
      ASSERT_TRUE(irs::type_limits<irs::type_t::doc_id_t>::eof(docs->value()));
    }

    // search "f_x%" on field without positions
    // which is ok for first word in phrase
    {
      irs::by_phrase q;
      irs::by_phrase::wildcard_term wt;
      wt.term = irs::ref_cast<irs::byte_type>(irs::string_ref("f_x%"));
      q.field("phrase").push_back(std::move(wt));

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
      auto& doc = docs->attributes().get<irs::document>();
      ASSERT_TRUE(bool(doc));
      ASSERT_EQ(docs->value(), doc->value);
      ASSERT_FALSE(irs::type_limits<irs::type_t::doc_id_t>::valid(docs->value()));
      auto docs_seek = prepared->execute(*sub);
      ASSERT_FALSE(irs::type_limits<irs::type_t::doc_id_t>::valid(docs_seek->value()));

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
      ASSERT_TRUE(irs::type_limits<irs::type_t::doc_id_t>::eof(docs->value()));
    }

    // search "fxo" on field without positions
    // which is ok for single word phrases
    {
      irs::by_phrase q;
      irs::by_phrase::levenshtein_term lt;
      lt.max_distance = 1;
      lt.with_transpositions = true;
      lt.term = irs::ref_cast<irs::byte_type>(irs::string_ref("fxo"));
      q.field("phrase").push_back(std::move(lt));

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
      auto& doc = docs->attributes().get<irs::document>();
      ASSERT_TRUE(bool(doc));
      ASSERT_EQ(docs->value(), doc->value);
      ASSERT_FALSE(irs::type_limits<irs::type_t::doc_id_t>::valid(docs->value()));
      auto docs_seek = prepared->execute(*sub);
      ASSERT_FALSE(irs::type_limits<irs::type_t::doc_id_t>::valid(docs_seek->value()));

      ASSERT_TRUE(docs->next());
      ASSERT_EQ(docs->value(), doc->value);
      ASSERT_TRUE(values(docs->value(), actual_value));
      ASSERT_EQ("K", irs::to_string<irs::string_ref>(actual_value.c_str()));
      ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
      ASSERT_TRUE(values(docs->value(), actual_value));
      ASSERT_EQ("K", irs::to_string<irs::string_ref>(actual_value.c_str()));

      ASSERT_FALSE(docs->next());
      ASSERT_EQ(docs->value(), doc->value);
      ASSERT_TRUE(irs::type_limits<irs::type_t::doc_id_t>::eof(docs->value()));
    }

    // term_filter "fox" with phrase offset
    // which does not matter
    {
      irs::bytes_ref actual_value;

      irs::by_phrase q;
      q.field("phrase_anl")
       .push_back(irs::by_phrase::simple_term{irs::ref_cast<irs::byte_type>(irs::string_ref("fox"))}, irs::integer_traits<size_t>::const_max);

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
      auto& doc = docs->attributes().get<irs::document>();
      ASSERT_TRUE(bool(doc));
      ASSERT_EQ(docs->value(), doc->value);
      ASSERT_FALSE(irs::type_limits<irs::type_t::doc_id_t>::valid(docs->value()));
      auto docs_seek = prepared->execute(*sub);
      ASSERT_FALSE(irs::type_limits<irs::type_t::doc_id_t>::valid(docs_seek->value()));

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
      ASSERT_TRUE(irs::type_limits<irs::type_t::doc_id_t>::eof(docs->value()));
    }

    // prefix_filter "fo*" with phrase offset
    // which does not matter
    {
      irs::bytes_ref actual_value;

      irs::by_phrase q;
      irs::by_phrase::prefix_term pt;
      pt.term = irs::ref_cast<irs::byte_type>(irs::string_ref("fo"));
      q.field("phrase_anl").push_back(std::move(pt), irs::integer_traits<size_t>::const_max);

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
      auto& doc = docs->attributes().get<irs::document>();
      ASSERT_TRUE(bool(doc));
      ASSERT_EQ(docs->value(), doc->value);
      ASSERT_FALSE(irs::type_limits<irs::type_t::doc_id_t>::valid(docs->value()));
      auto docs_seek = prepared->execute(*sub);
      ASSERT_FALSE(irs::type_limits<irs::type_t::doc_id_t>::valid(docs_seek->value()));

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
      ASSERT_TRUE(irs::type_limits<irs::type_t::doc_id_t>::eof(docs->value()));
    }

    // wildcard_filter "fo%" with phrase offset
    // which does not matter
    {
      irs::bytes_ref actual_value;

      irs::by_phrase q;
      irs::by_phrase::wildcard_term wt;
      wt.term = irs::ref_cast<irs::byte_type>(irs::string_ref("fo%"));
      q.field("phrase_anl").push_back(std::move(wt), irs::integer_traits<size_t>::const_max);

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
      auto& doc = docs->attributes().get<irs::document>();
      ASSERT_TRUE(bool(doc));
      ASSERT_EQ(docs->value(), doc->value);
      ASSERT_FALSE(irs::type_limits<irs::type_t::doc_id_t>::valid(docs->value()));
      auto docs_seek = prepared->execute(*sub);
      ASSERT_FALSE(irs::type_limits<irs::type_t::doc_id_t>::valid(docs_seek->value()));

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
      ASSERT_TRUE(irs::type_limits<irs::type_t::doc_id_t>::eof(docs->value()));
    }

    // wildcard_filter "f%x" with phrase offset
    // which does not matter
    {
      irs::bytes_ref actual_value;

      irs::by_phrase q;
      irs::by_phrase::wildcard_term wt;
      wt.term = irs::ref_cast<irs::byte_type>(irs::string_ref("f%x"));
      q.field("phrase_anl").push_back(std::move(wt), irs::integer_traits<size_t>::const_max);

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
      auto& doc = docs->attributes().get<irs::document>();
      ASSERT_TRUE(bool(doc));
      ASSERT_EQ(docs->value(), doc->value);
      ASSERT_FALSE(irs::type_limits<irs::type_t::doc_id_t>::valid(docs->value()));
      auto docs_seek = prepared->execute(*sub);
      ASSERT_FALSE(irs::type_limits<irs::type_t::doc_id_t>::valid(docs_seek->value()));

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
      ASSERT_TRUE(irs::type_limits<irs::type_t::doc_id_t>::eof(docs->value()));
    }

    // wildcard_filter "f%x" with phrase offset
    // which does not matter
    {
      irs::bytes_ref actual_value;

      irs::by_phrase q;
      irs::by_phrase::levenshtein_term lt;
      lt.max_distance = 1;
      lt.term = irs::ref_cast<irs::byte_type>(irs::string_ref("fkx"));
      q.field("phrase_anl").push_back(std::move(lt), irs::integer_traits<size_t>::const_max);

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
      auto& doc = docs->attributes().get<irs::document>();
      ASSERT_TRUE(bool(doc));
      ASSERT_EQ(docs->value(), doc->value);
      ASSERT_FALSE(irs::type_limits<irs::type_t::doc_id_t>::valid(docs->value()));
      auto docs_seek = prepared->execute(*sub);
      ASSERT_FALSE(irs::type_limits<irs::type_t::doc_id_t>::valid(docs_seek->value()));

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
      ASSERT_TRUE(irs::type_limits<irs::type_t::doc_id_t>::eof(docs->value()));
    }

    // "quick brown fox"
    {
      irs::bytes_ref actual_value;

      irs::by_phrase q;
      q.field("phrase_anl")
       .push_back(irs::by_phrase::simple_term{irs::ref_cast<irs::byte_type>(irs::string_ref("quick"))})
       .push_back(irs::by_phrase::simple_term{irs::ref_cast<irs::byte_type>(irs::string_ref("brown"))})
       .push_back(irs::by_phrase::simple_term{irs::ref_cast<irs::byte_type>(irs::string_ref("fox"))});

      auto prepared = q.prepare(rdr);
      auto sub = rdr.begin();
      auto column = sub->column_reader("name");
      ASSERT_NE(nullptr, column);
      auto values = column->values();

      auto docs = prepared->execute(*sub);
      auto& doc = docs->attributes().get<irs::document>();
      ASSERT_TRUE(bool(doc));
      ASSERT_EQ(docs->value(), doc->value);
      ASSERT_FALSE(irs::type_limits<irs::type_t::doc_id_t>::valid(docs->value()));
      auto docs_seek = prepared->execute(*sub);
      ASSERT_FALSE(irs::type_limits<irs::type_t::doc_id_t>::valid(docs_seek->value()));

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
      ASSERT_TRUE(irs::type_limits<irs::type_t::doc_id_t>::eof(docs->value()));
      ASSERT_TRUE(irs::type_limits<irs::type_t::doc_id_t>::eof(docs_seek->seek(irs::type_limits<irs::type_t::doc_id_t>::eof())));
    }

    // "qui* brown fox"
    {
      irs::bytes_ref actual_value;

      irs::by_phrase q;
      irs::by_phrase::prefix_term pt;
      pt.term = irs::ref_cast<irs::byte_type>(irs::string_ref("qui"));
      q.field("phrase_anl").push_back(std::move(pt))
       .push_back(irs::by_phrase::simple_term{irs::ref_cast<irs::byte_type>(irs::string_ref("brown"))})
       .push_back(irs::by_phrase::simple_term{irs::ref_cast<irs::byte_type>(irs::string_ref("fox"))});

      auto prepared = q.prepare(rdr);
      auto sub = rdr.begin();
      auto column = sub->column_reader("name");
      ASSERT_NE(nullptr, column);
      auto values = column->values();

      auto docs = prepared->execute(*sub);
      auto& doc = docs->attributes().get<irs::document>();
      ASSERT_TRUE(bool(doc));
      ASSERT_EQ(docs->value(), doc->value);
      ASSERT_FALSE(irs::type_limits<irs::type_t::doc_id_t>::valid(docs->value()));
      auto docs_seek = prepared->execute(*sub);
      ASSERT_FALSE(irs::type_limits<irs::type_t::doc_id_t>::valid(docs_seek->value()));

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
      ASSERT_TRUE(irs::type_limits<irs::type_t::doc_id_t>::eof(docs->value()));
      ASSERT_TRUE(irs::type_limits<irs::type_t::doc_id_t>::eof(docs_seek->seek(irs::type_limits<irs::type_t::doc_id_t>::eof())));
    }

    // "qui% brown fox"
    {
      irs::bytes_ref actual_value;

      irs::by_phrase q;
      irs::by_phrase::wildcard_term wt;
      wt.term = irs::ref_cast<irs::byte_type>(irs::string_ref("qui%"));
      q.field("phrase_anl").push_back(std::move(wt))
       .push_back(irs::by_phrase::simple_term{irs::ref_cast<irs::byte_type>(irs::string_ref("brown"))})
       .push_back(irs::by_phrase::simple_term{irs::ref_cast<irs::byte_type>(irs::string_ref("fox"))});

      auto prepared = q.prepare(rdr);
      auto sub = rdr.begin();
      auto column = sub->column_reader("name");
      ASSERT_NE(nullptr, column);
      auto values = column->values();

      auto docs = prepared->execute(*sub);
      auto& doc = docs->attributes().get<irs::document>();
      ASSERT_TRUE(bool(doc));
      ASSERT_EQ(docs->value(), doc->value);
      ASSERT_FALSE(irs::type_limits<irs::type_t::doc_id_t>::valid(docs->value()));
      auto docs_seek = prepared->execute(*sub);
      ASSERT_FALSE(irs::type_limits<irs::type_t::doc_id_t>::valid(docs_seek->value()));

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
      ASSERT_TRUE(irs::type_limits<irs::type_t::doc_id_t>::eof(docs->value()));
      ASSERT_TRUE(irs::type_limits<irs::type_t::doc_id_t>::eof(docs_seek->seek(irs::type_limits<irs::type_t::doc_id_t>::eof())));
    }

    // "q%ck brown fox"
    {
      irs::bytes_ref actual_value;

      irs::by_phrase q;
      irs::by_phrase::wildcard_term wt;
      wt.term = irs::ref_cast<irs::byte_type>(irs::string_ref("q%ck"));
      q.field("phrase_anl").push_back(std::move(wt))
       .push_back(irs::by_phrase::simple_term{irs::ref_cast<irs::byte_type>(irs::string_ref("brown"))})
       .push_back(irs::by_phrase::simple_term{irs::ref_cast<irs::byte_type>(irs::string_ref("fox"))});

      auto prepared = q.prepare(rdr);
      auto sub = rdr.begin();
      auto column = sub->column_reader("name");
      ASSERT_NE(nullptr, column);
      auto values = column->values();

      auto docs = prepared->execute(*sub);
      auto& doc = docs->attributes().get<irs::document>();
      ASSERT_TRUE(bool(doc));
      ASSERT_EQ(docs->value(), doc->value);
      ASSERT_FALSE(irs::type_limits<irs::type_t::doc_id_t>::valid(docs->value()));
      auto docs_seek = prepared->execute(*sub);
      ASSERT_FALSE(irs::type_limits<irs::type_t::doc_id_t>::valid(docs_seek->value()));

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
      ASSERT_TRUE(irs::type_limits<irs::type_t::doc_id_t>::eof(docs->value()));
      ASSERT_TRUE(irs::type_limits<irs::type_t::doc_id_t>::eof(docs_seek->seek(irs::type_limits<irs::type_t::doc_id_t>::eof())));
    }

    // "quck brown fox"
    {
      irs::bytes_ref actual_value;

      irs::by_phrase q;
      irs::by_phrase::levenshtein_term lt;
      lt.max_distance = 1;
      lt.term = irs::ref_cast<irs::byte_type>(irs::string_ref("quck"));
      q.field("phrase_anl").push_back(std::move(lt))
       .push_back(irs::by_phrase::simple_term{irs::ref_cast<irs::byte_type>(irs::string_ref("brown"))})
       .push_back(irs::by_phrase::simple_term{irs::ref_cast<irs::byte_type>(irs::string_ref("fox"))});

      auto prepared = q.prepare(rdr);
      auto sub = rdr.begin();
      auto column = sub->column_reader("name");
      ASSERT_NE(nullptr, column);
      auto values = column->values();

      auto docs = prepared->execute(*sub);
      auto& doc = docs->attributes().get<irs::document>();
      ASSERT_TRUE(bool(doc));
      ASSERT_EQ(docs->value(), doc->value);
      ASSERT_FALSE(irs::type_limits<irs::type_t::doc_id_t>::valid(docs->value()));
      auto docs_seek = prepared->execute(*sub);
      ASSERT_FALSE(irs::type_limits<irs::type_t::doc_id_t>::valid(docs_seek->value()));

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
      ASSERT_TRUE(irs::type_limits<irs::type_t::doc_id_t>::eof(docs->value()));
      ASSERT_TRUE(irs::type_limits<irs::type_t::doc_id_t>::eof(docs_seek->seek(irs::type_limits<irs::type_t::doc_id_t>::eof())));
    }

    // "quick bro* fox"
    {
      irs::bytes_ref actual_value;

      irs::by_phrase q;
      irs::by_phrase::prefix_term pt;
      pt.term = irs::ref_cast<irs::byte_type>(irs::string_ref("bro"));
      q.field("phrase_anl")
       .push_back(irs::by_phrase::simple_term{irs::ref_cast<irs::byte_type>(irs::string_ref("quick"))})
       .push_back(std::move(pt))
       .push_back(irs::by_phrase::simple_term{irs::ref_cast<irs::byte_type>(irs::string_ref("fox"))});

      auto prepared = q.prepare(rdr);
      auto sub = rdr.begin();
      auto column = sub->column_reader("name");
      ASSERT_NE(nullptr, column);
      auto values = column->values();

      auto docs = prepared->execute(*sub);
      auto& doc = docs->attributes().get<irs::document>();
      ASSERT_TRUE(bool(doc));
      ASSERT_EQ(docs->value(), doc->value);
      ASSERT_FALSE(irs::type_limits<irs::type_t::doc_id_t>::valid(docs->value()));
      auto docs_seek = prepared->execute(*sub);
      ASSERT_FALSE(irs::type_limits<irs::type_t::doc_id_t>::valid(docs_seek->value()));

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
      ASSERT_TRUE(irs::type_limits<irs::type_t::doc_id_t>::eof(docs->value()));
      ASSERT_TRUE(irs::type_limits<irs::type_t::doc_id_t>::eof(docs_seek->seek(irs::type_limits<irs::type_t::doc_id_t>::eof())));
    }

    // "quick bro% fox"
    {
      irs::bytes_ref actual_value;

      irs::by_phrase q;
      irs::by_phrase::wildcard_term wt;
      wt.term = irs::ref_cast<irs::byte_type>(irs::string_ref("bro%"));
      q.field("phrase_anl")
       .push_back(irs::by_phrase::simple_term{irs::ref_cast<irs::byte_type>(irs::string_ref("quick"))})
       .push_back(std::move(wt))
       .push_back(irs::by_phrase::simple_term{irs::ref_cast<irs::byte_type>(irs::string_ref("fox"))});

      auto prepared = q.prepare(rdr);
      auto sub = rdr.begin();
      auto column = sub->column_reader("name");
      ASSERT_NE(nullptr, column);
      auto values = column->values();

      auto docs = prepared->execute(*sub);
      auto& doc = docs->attributes().get<irs::document>();
      ASSERT_TRUE(bool(doc));
      ASSERT_EQ(docs->value(), doc->value);
      ASSERT_FALSE(irs::type_limits<irs::type_t::doc_id_t>::valid(docs->value()));
      auto docs_seek = prepared->execute(*sub);
      ASSERT_FALSE(irs::type_limits<irs::type_t::doc_id_t>::valid(docs_seek->value()));

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
      ASSERT_TRUE(irs::type_limits<irs::type_t::doc_id_t>::eof(docs->value()));
      ASSERT_TRUE(irs::type_limits<irs::type_t::doc_id_t>::eof(docs_seek->seek(irs::type_limits<irs::type_t::doc_id_t>::eof())));
    }

    // "quick b%w_ fox"
    {
      irs::bytes_ref actual_value;

      irs::by_phrase q;
      irs::by_phrase::wildcard_term wt;
      wt.term = irs::ref_cast<irs::byte_type>(irs::string_ref("b%w_"));
      q.field("phrase_anl")
       .push_back(irs::by_phrase::simple_term{irs::ref_cast<irs::byte_type>(irs::string_ref("quick"))})
       .push_back(std::move(wt))
       .push_back(irs::by_phrase::simple_term{irs::ref_cast<irs::byte_type>(irs::string_ref("fox"))});

      auto prepared = q.prepare(rdr);
      auto sub = rdr.begin();
      auto column = sub->column_reader("name");
      ASSERT_NE(nullptr, column);
      auto values = column->values();

      auto docs = prepared->execute(*sub);
      auto& doc = docs->attributes().get<irs::document>();
      ASSERT_TRUE(bool(doc));
      ASSERT_EQ(docs->value(), doc->value);
      ASSERT_FALSE(irs::type_limits<irs::type_t::doc_id_t>::valid(docs->value()));
      auto docs_seek = prepared->execute(*sub);
      ASSERT_FALSE(irs::type_limits<irs::type_t::doc_id_t>::valid(docs_seek->value()));

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
      ASSERT_TRUE(irs::type_limits<irs::type_t::doc_id_t>::eof(docs->value()));
      ASSERT_TRUE(irs::type_limits<irs::type_t::doc_id_t>::eof(docs_seek->seek(irs::type_limits<irs::type_t::doc_id_t>::eof())));
    }

    // "quick brkln fox"
    {
      irs::bytes_ref actual_value;

      irs::by_phrase q;
      irs::by_phrase::levenshtein_term lt;
      lt.max_distance = 2;
      lt.term = irs::ref_cast<irs::byte_type>(irs::string_ref("brkln"));
      q.field("phrase_anl")
       .push_back(irs::by_phrase::simple_term{irs::ref_cast<irs::byte_type>(irs::string_ref("quick"))})
       .push_back(std::move(lt))
       .push_back(irs::by_phrase::simple_term{irs::ref_cast<irs::byte_type>(irs::string_ref("fox"))});

      auto prepared = q.prepare(rdr);
      auto sub = rdr.begin();
      auto column = sub->column_reader("name");
      ASSERT_NE(nullptr, column);
      auto values = column->values();

      auto docs = prepared->execute(*sub);
      auto& doc = docs->attributes().get<irs::document>();
      ASSERT_TRUE(bool(doc));
      ASSERT_EQ(docs->value(), doc->value);
      ASSERT_FALSE(irs::type_limits<irs::type_t::doc_id_t>::valid(docs->value()));
      auto docs_seek = prepared->execute(*sub);
      ASSERT_FALSE(irs::type_limits<irs::type_t::doc_id_t>::valid(docs_seek->value()));

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
      ASSERT_TRUE(irs::type_limits<irs::type_t::doc_id_t>::eof(docs->value()));
      ASSERT_TRUE(irs::type_limits<irs::type_t::doc_id_t>::eof(docs_seek->seek(irs::type_limits<irs::type_t::doc_id_t>::eof())));
    }

    // "quick brown fo*"
    {
      irs::bytes_ref actual_value;

      irs::by_phrase q;
      irs::by_phrase::prefix_term pt;
      pt.term = irs::ref_cast<irs::byte_type>(irs::string_ref("fo"));
      q.field("phrase_anl")
       .push_back(irs::by_phrase::simple_term{irs::ref_cast<irs::byte_type>(irs::string_ref("quick"))})
       .push_back(irs::by_phrase::simple_term{irs::ref_cast<irs::byte_type>(irs::string_ref("brown"))})
       .push_back(std::move(pt));

      auto prepared = q.prepare(rdr);
      auto sub = rdr.begin();
      auto column = sub->column_reader("name");
      ASSERT_NE(nullptr, column);
      auto values = column->values();

      auto docs = prepared->execute(*sub);
      auto& doc = docs->attributes().get<irs::document>();
      ASSERT_TRUE(bool(doc));
      ASSERT_EQ(docs->value(), doc->value);
      ASSERT_FALSE(irs::type_limits<irs::type_t::doc_id_t>::valid(docs->value()));
      auto docs_seek = prepared->execute(*sub);
      ASSERT_FALSE(irs::type_limits<irs::type_t::doc_id_t>::valid(docs_seek->value()));

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
      ASSERT_TRUE(irs::type_limits<irs::type_t::doc_id_t>::eof(docs->value()));
      ASSERT_TRUE(irs::type_limits<irs::type_t::doc_id_t>::eof(docs_seek->seek(irs::type_limits<irs::type_t::doc_id_t>::eof())));
    }

    // "quick brown fo%"
    {
      irs::bytes_ref actual_value;

      irs::by_phrase q;
      irs::by_phrase::wildcard_term wt;
      wt.term = irs::ref_cast<irs::byte_type>(irs::string_ref("fo%"));
      q.field("phrase_anl")
       .push_back(irs::by_phrase::simple_term{irs::ref_cast<irs::byte_type>(irs::string_ref("quick"))})
       .push_back(irs::by_phrase::simple_term{irs::ref_cast<irs::byte_type>(irs::string_ref("brown"))})
       .push_back(std::move(wt));

      auto prepared = q.prepare(rdr);
      auto sub = rdr.begin();
      auto column = sub->column_reader("name");
      ASSERT_NE(nullptr, column);
      auto values = column->values();

      auto docs = prepared->execute(*sub);
      auto& doc = docs->attributes().get<irs::document>();
      ASSERT_TRUE(bool(doc));
      ASSERT_EQ(docs->value(), doc->value);
      ASSERT_FALSE(irs::type_limits<irs::type_t::doc_id_t>::valid(docs->value()));
      auto docs_seek = prepared->execute(*sub);
      ASSERT_FALSE(irs::type_limits<irs::type_t::doc_id_t>::valid(docs_seek->value()));

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
      ASSERT_TRUE(irs::type_limits<irs::type_t::doc_id_t>::eof(docs->value()));
      ASSERT_TRUE(irs::type_limits<irs::type_t::doc_id_t>::eof(docs_seek->seek(irs::type_limits<irs::type_t::doc_id_t>::eof())));
    }

    // "quick brown f_x"
    {
      irs::bytes_ref actual_value;

      irs::by_phrase q;
      irs::by_phrase::wildcard_term wt;
      wt.term = irs::ref_cast<irs::byte_type>(irs::string_ref("f_x"));
      q.field("phrase_anl")
       .push_back(irs::by_phrase::simple_term{irs::ref_cast<irs::byte_type>(irs::string_ref("quick"))})
       .push_back(irs::by_phrase::simple_term{irs::ref_cast<irs::byte_type>(irs::string_ref("brown"))})
       .push_back(std::move(wt));

      auto prepared = q.prepare(rdr);
      auto sub = rdr.begin();
      auto column = sub->column_reader("name");
      ASSERT_NE(nullptr, column);
      auto values = column->values();

      auto docs = prepared->execute(*sub);
      auto& doc = docs->attributes().get<irs::document>();
      ASSERT_TRUE(bool(doc));
      ASSERT_EQ(docs->value(), doc->value);
      ASSERT_FALSE(irs::type_limits<irs::type_t::doc_id_t>::valid(docs->value()));
      auto docs_seek = prepared->execute(*sub);
      ASSERT_FALSE(irs::type_limits<irs::type_t::doc_id_t>::valid(docs_seek->value()));

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
      ASSERT_TRUE(irs::type_limits<irs::type_t::doc_id_t>::eof(docs->value()));
      ASSERT_TRUE(irs::type_limits<irs::type_t::doc_id_t>::eof(docs_seek->seek(irs::type_limits<irs::type_t::doc_id_t>::eof())));
    }

    // "quick brown fxo"
    {
      irs::bytes_ref actual_value;

      irs::by_phrase q;
      irs::by_phrase::levenshtein_term lt;
      lt.max_distance = 1;
      lt.with_transpositions = true;
      lt.term = irs::ref_cast<irs::byte_type>(irs::string_ref("fxo"));
      q.field("phrase_anl")
       .push_back(irs::by_phrase::simple_term{irs::ref_cast<irs::byte_type>(irs::string_ref("quick"))})
       .push_back(irs::by_phrase::simple_term{irs::ref_cast<irs::byte_type>(irs::string_ref("brown"))})
       .push_back(std::move(lt));

      auto prepared = q.prepare(rdr);
      auto sub = rdr.begin();
      auto column = sub->column_reader("name");
      ASSERT_NE(nullptr, column);
      auto values = column->values();

      auto docs = prepared->execute(*sub);
      auto& doc = docs->attributes().get<irs::document>();
      ASSERT_TRUE(bool(doc));
      ASSERT_EQ(docs->value(), doc->value);
      ASSERT_FALSE(irs::type_limits<irs::type_t::doc_id_t>::valid(docs->value()));
      auto docs_seek = prepared->execute(*sub);
      ASSERT_FALSE(irs::type_limits<irs::type_t::doc_id_t>::valid(docs_seek->value()));

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
      ASSERT_TRUE(irs::type_limits<irs::type_t::doc_id_t>::eof(docs->value()));
      ASSERT_TRUE(irs::type_limits<irs::type_t::doc_id_t>::eof(docs_seek->seek(irs::type_limits<irs::type_t::doc_id_t>::eof())));
    }

    // "qui* bro* fox"
    {
      irs::bytes_ref actual_value;

      irs::by_phrase q;
      irs::by_phrase::prefix_term pt1;
      pt1.term = irs::ref_cast<irs::byte_type>(irs::string_ref("qui"));
      irs::by_phrase::prefix_term pt2;
      pt2.term = irs::ref_cast<irs::byte_type>(irs::string_ref("bro"));
      q.field("phrase_anl").push_back(std::move(pt1)).push_back(std::move(pt2))
       .push_back(irs::by_phrase::simple_term{irs::ref_cast<irs::byte_type>(irs::string_ref("fox"))});

      auto prepared = q.prepare(rdr);
      auto sub = rdr.begin();
      auto column = sub->column_reader("name");
      ASSERT_NE(nullptr, column);
      auto values = column->values();

      auto docs = prepared->execute(*sub);
      auto& doc = docs->attributes().get<irs::document>();
      ASSERT_TRUE(bool(doc));
      ASSERT_EQ(docs->value(), doc->value);
      ASSERT_FALSE(irs::type_limits<irs::type_t::doc_id_t>::valid(docs->value()));
      auto docs_seek = prepared->execute(*sub);
      ASSERT_FALSE(irs::type_limits<irs::type_t::doc_id_t>::valid(docs_seek->value()));

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
      ASSERT_TRUE(irs::type_limits<irs::type_t::doc_id_t>::eof(docs->value()));
      ASSERT_TRUE(irs::type_limits<irs::type_t::doc_id_t>::eof(docs_seek->seek(irs::type_limits<irs::type_t::doc_id_t>::eof())));
    }

    // "qui% bro% fox"
    {
      irs::bytes_ref actual_value;

      irs::by_phrase q;
      irs::by_phrase::wildcard_term wt1;
      wt1.term = irs::ref_cast<irs::byte_type>(irs::string_ref("qui%"));
      irs::by_phrase::wildcard_term wt2;
      wt2.term = irs::ref_cast<irs::byte_type>(irs::string_ref("bro%"));
      q.field("phrase_anl").push_back(std::move(wt1)).push_back(std::move(wt2))
       .push_back(irs::by_phrase::simple_term{irs::ref_cast<irs::byte_type>(irs::string_ref("fox"))});

      auto prepared = q.prepare(rdr);
      auto sub = rdr.begin();
      auto column = sub->column_reader("name");
      ASSERT_NE(nullptr, column);
      auto values = column->values();

      auto docs = prepared->execute(*sub);
      auto& doc = docs->attributes().get<irs::document>();
      ASSERT_TRUE(bool(doc));
      ASSERT_EQ(docs->value(), doc->value);
      ASSERT_FALSE(irs::type_limits<irs::type_t::doc_id_t>::valid(docs->value()));
      auto docs_seek = prepared->execute(*sub);
      ASSERT_FALSE(irs::type_limits<irs::type_t::doc_id_t>::valid(docs_seek->value()));

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
      ASSERT_TRUE(irs::type_limits<irs::type_t::doc_id_t>::eof(docs->value()));
      ASSERT_TRUE(irs::type_limits<irs::type_t::doc_id_t>::eof(docs_seek->seek(irs::type_limits<irs::type_t::doc_id_t>::eof())));
    }

    // "qui% b%o__ fox"
    {
      irs::bytes_ref actual_value;

      irs::by_phrase q;
      irs::by_phrase::wildcard_term wt1;
      wt1.term = irs::ref_cast<irs::byte_type>(irs::string_ref("qui%"));
      irs::by_phrase::wildcard_term wt2;
      wt2.term = irs::ref_cast<irs::byte_type>(irs::string_ref("b%o__"));
      q.field("phrase_anl").push_back(std::move(wt1)).push_back(std::move(wt2))
       .push_back(irs::by_phrase::simple_term{irs::ref_cast<irs::byte_type>(irs::string_ref("fox"))});

      auto prepared = q.prepare(rdr);
      auto sub = rdr.begin();
      auto column = sub->column_reader("name");
      ASSERT_NE(nullptr, column);
      auto values = column->values();

      auto docs = prepared->execute(*sub);
      auto& doc = docs->attributes().get<irs::document>();
      ASSERT_TRUE(bool(doc));
      ASSERT_EQ(docs->value(), doc->value);
      ASSERT_FALSE(irs::type_limits<irs::type_t::doc_id_t>::valid(docs->value()));
      auto docs_seek = prepared->execute(*sub);
      ASSERT_FALSE(irs::type_limits<irs::type_t::doc_id_t>::valid(docs_seek->value()));

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
      ASSERT_TRUE(irs::type_limits<irs::type_t::doc_id_t>::eof(docs->value()));
      ASSERT_TRUE(irs::type_limits<irs::type_t::doc_id_t>::eof(docs_seek->seek(irs::type_limits<irs::type_t::doc_id_t>::eof())));
    }

    // "qui bro fox"
    {
      irs::bytes_ref actual_value;

      irs::by_phrase q;
      irs::by_phrase::levenshtein_term lt1;
      lt1.max_distance = 2;
      lt1.term = irs::ref_cast<irs::byte_type>(irs::string_ref("qui"));
      irs::by_phrase::levenshtein_term lt2;
      lt2.max_distance = 1;
      lt2.term = irs::ref_cast<irs::byte_type>(irs::string_ref("brow"));
      q.field("phrase_anl")
       .push_back(std::move(lt1)).push_back(std::move(lt2))
       .push_back(irs::by_phrase::simple_term{irs::ref_cast<irs::byte_type>(irs::string_ref("fox"))});

      auto prepared = q.prepare(rdr);
      auto sub = rdr.begin();
      auto column = sub->column_reader("name");
      ASSERT_NE(nullptr, column);
      auto values = column->values();

      auto docs = prepared->execute(*sub);
      auto& doc = docs->attributes().get<irs::document>();
      ASSERT_TRUE(bool(doc));
      ASSERT_EQ(docs->value(), doc->value);
      ASSERT_FALSE(irs::type_limits<irs::type_t::doc_id_t>::valid(docs->value()));
      auto docs_seek = prepared->execute(*sub);
      ASSERT_FALSE(irs::type_limits<irs::type_t::doc_id_t>::valid(docs_seek->value()));

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
      ASSERT_TRUE(irs::type_limits<irs::type_t::doc_id_t>::eof(docs->value()));
      ASSERT_TRUE(irs::type_limits<irs::type_t::doc_id_t>::eof(docs_seek->seek(irs::type_limits<irs::type_t::doc_id_t>::eof())));
    }

    // "qui* brown fo*"
    {
      irs::bytes_ref actual_value;

      irs::by_phrase q;
      irs::by_phrase::prefix_term pt1;
      pt1.term = irs::ref_cast<irs::byte_type>(irs::string_ref("qui"));
      irs::by_phrase::prefix_term pt2;
      pt2.term = irs::ref_cast<irs::byte_type>(irs::string_ref("fo"));
      q.field("phrase_anl").push_back(std::move(pt1))
       .push_back(irs::by_phrase::simple_term{irs::ref_cast<irs::byte_type>(irs::string_ref("brown"))})
       .push_back(std::move(pt2));

      auto prepared = q.prepare(rdr);
      auto sub = rdr.begin();
      auto column = sub->column_reader("name");
      ASSERT_NE(nullptr, column);
      auto values = column->values();

      auto docs = prepared->execute(*sub);
      auto& doc = docs->attributes().get<irs::document>();
      ASSERT_TRUE(bool(doc));
      ASSERT_EQ(docs->value(), doc->value);
      ASSERT_FALSE(irs::type_limits<irs::type_t::doc_id_t>::valid(docs->value()));
      auto docs_seek = prepared->execute(*sub);
      ASSERT_FALSE(irs::type_limits<irs::type_t::doc_id_t>::valid(docs_seek->value()));

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
      ASSERT_TRUE(irs::type_limits<irs::type_t::doc_id_t>::eof(docs->value()));
      ASSERT_TRUE(irs::type_limits<irs::type_t::doc_id_t>::eof(docs_seek->seek(irs::type_limits<irs::type_t::doc_id_t>::eof())));
    }

    // "qui% brown fo%"
    {
      irs::bytes_ref actual_value;

      irs::by_phrase q;
      irs::by_phrase::wildcard_term wt1;
      wt1.term = irs::ref_cast<irs::byte_type>(irs::string_ref("qui%"));
      irs::by_phrase::wildcard_term wt2;
      wt2.term = irs::ref_cast<irs::byte_type>(irs::string_ref("fo%"));
      q.field("phrase_anl").push_back(std::move(wt1))
       .push_back(irs::by_phrase::simple_term{irs::ref_cast<irs::byte_type>(irs::string_ref("brown"))})
       .push_back(std::move(wt2));

      auto prepared = q.prepare(rdr);
      auto sub = rdr.begin();
      auto column = sub->column_reader("name");
      ASSERT_NE(nullptr, column);
      auto values = column->values();

      auto docs = prepared->execute(*sub);
      auto& doc = docs->attributes().get<irs::document>();
      ASSERT_TRUE(bool(doc));
      ASSERT_EQ(docs->value(), doc->value);
      ASSERT_FALSE(irs::type_limits<irs::type_t::doc_id_t>::valid(docs->value()));
      auto docs_seek = prepared->execute(*sub);
      ASSERT_FALSE(irs::type_limits<irs::type_t::doc_id_t>::valid(docs_seek->value()));

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
      ASSERT_TRUE(irs::type_limits<irs::type_t::doc_id_t>::eof(docs->value()));
      ASSERT_TRUE(irs::type_limits<irs::type_t::doc_id_t>::eof(docs_seek->seek(irs::type_limits<irs::type_t::doc_id_t>::eof())));
    }

    // "q_i% brown f%x"
    {
      irs::bytes_ref actual_value;

      irs::by_phrase q;
      irs::by_phrase::wildcard_term wt1;
      wt1.term = irs::ref_cast<irs::byte_type>(irs::string_ref("q_i%"));
      irs::by_phrase::wildcard_term wt2;
      wt2.term = irs::ref_cast<irs::byte_type>(irs::string_ref("f%x"));
      q.field("phrase_anl").push_back(std::move(wt1))
       .push_back(irs::by_phrase::simple_term{irs::ref_cast<irs::byte_type>(irs::string_ref("brown"))})
       .push_back(std::move(wt2));

      auto prepared = q.prepare(rdr);
      auto sub = rdr.begin();
      auto column = sub->column_reader("name");
      ASSERT_NE(nullptr, column);
      auto values = column->values();

      auto docs = prepared->execute(*sub);
      auto& doc = docs->attributes().get<irs::document>();
      ASSERT_TRUE(bool(doc));
      ASSERT_EQ(docs->value(), doc->value);
      ASSERT_FALSE(irs::type_limits<irs::type_t::doc_id_t>::valid(docs->value()));
      auto docs_seek = prepared->execute(*sub);
      ASSERT_FALSE(irs::type_limits<irs::type_t::doc_id_t>::valid(docs_seek->value()));

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
      ASSERT_TRUE(irs::type_limits<irs::type_t::doc_id_t>::eof(docs->value()));
      ASSERT_TRUE(irs::type_limits<irs::type_t::doc_id_t>::eof(docs_seek->seek(irs::type_limits<irs::type_t::doc_id_t>::eof())));
    }

    // "qoick br__nn fix"
    {
      irs::bytes_ref actual_value;

      irs::by_phrase q;
      irs::by_phrase::levenshtein_term lt1;
      lt1.max_distance = 1;
      lt1.term = irs::ref_cast<irs::byte_type>(irs::string_ref("qoick"));
      irs::by_phrase::levenshtein_term lt2;
      lt2.max_distance = 1;
      lt2.term = irs::ref_cast<irs::byte_type>(irs::string_ref("fix"));
      irs::by_phrase::wildcard_term wt;
      wt.term = irs::ref_cast<irs::byte_type>(irs::string_ref("br__n"));
      q.field("phrase_anl").push_back(std::move(lt1)).push_back(std::move(wt)).push_back(std::move(lt2));

      auto prepared = q.prepare(rdr);
      auto sub = rdr.begin();
      auto column = sub->column_reader("name");
      ASSERT_NE(nullptr, column);
      auto values = column->values();

      auto docs = prepared->execute(*sub);
      auto& doc = docs->attributes().get<irs::document>();
      ASSERT_TRUE(bool(doc));
      ASSERT_EQ(docs->value(), doc->value);
      ASSERT_FALSE(irs::type_limits<irs::type_t::doc_id_t>::valid(docs->value()));
      auto docs_seek = prepared->execute(*sub);
      ASSERT_FALSE(irs::type_limits<irs::type_t::doc_id_t>::valid(docs_seek->value()));

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
      ASSERT_TRUE(irs::type_limits<irs::type_t::doc_id_t>::eof(docs->value()));
      ASSERT_TRUE(irs::type_limits<irs::type_t::doc_id_t>::eof(docs_seek->seek(irs::type_limits<irs::type_t::doc_id_t>::eof())));
    }

    // "quick bro* fo*"
    {
      irs::bytes_ref actual_value;

      irs::by_phrase q;
      irs::by_phrase::prefix_term pt1;
      pt1.term = irs::ref_cast<irs::byte_type>(irs::string_ref("bro"));
      irs::by_phrase::prefix_term pt2;
      pt2.term = irs::ref_cast<irs::byte_type>(irs::string_ref("fo"));
      q.field("phrase_anl")
       .push_back(irs::by_phrase::simple_term{irs::ref_cast<irs::byte_type>(irs::string_ref("quick"))})
       .push_back(std::move(pt1)).push_back(std::move(pt2));

      auto prepared = q.prepare(rdr);
      auto sub = rdr.begin();
      auto column = sub->column_reader("name");
      ASSERT_NE(nullptr, column);
      auto values = column->values();

      auto docs = prepared->execute(*sub);
      auto& doc = docs->attributes().get<irs::document>();
      ASSERT_TRUE(bool(doc));
      ASSERT_EQ(docs->value(), doc->value);
      ASSERT_FALSE(irs::type_limits<irs::type_t::doc_id_t>::valid(docs->value()));
      auto docs_seek = prepared->execute(*sub);
      ASSERT_FALSE(irs::type_limits<irs::type_t::doc_id_t>::valid(docs_seek->value()));

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
      ASSERT_TRUE(irs::type_limits<irs::type_t::doc_id_t>::eof(docs->value()));
      ASSERT_TRUE(irs::type_limits<irs::type_t::doc_id_t>::eof(docs_seek->seek(irs::type_limits<irs::type_t::doc_id_t>::eof())));
    }

    // "quick bro% fo%"
    {
      irs::bytes_ref actual_value;

      irs::by_phrase q;
      irs::by_phrase::wildcard_term wt1;
      wt1.term = irs::ref_cast<irs::byte_type>(irs::string_ref("bro%"));
      irs::by_phrase::wildcard_term wt2;
      wt2.term = irs::ref_cast<irs::byte_type>(irs::string_ref("fo%"));
      q.field("phrase_anl")
       .push_back(irs::by_phrase::simple_term{irs::ref_cast<irs::byte_type>(irs::string_ref("quick"))})
       .push_back(std::move(wt1)).push_back(std::move(wt2));

      auto prepared = q.prepare(rdr);
      auto sub = rdr.begin();
      auto column = sub->column_reader("name");
      ASSERT_NE(nullptr, column);
      auto values = column->values();

      auto docs = prepared->execute(*sub);
      auto& doc = docs->attributes().get<irs::document>();
      ASSERT_TRUE(bool(doc));
      ASSERT_EQ(docs->value(), doc->value);
      ASSERT_FALSE(irs::type_limits<irs::type_t::doc_id_t>::valid(docs->value()));
      auto docs_seek = prepared->execute(*sub);
      ASSERT_FALSE(irs::type_limits<irs::type_t::doc_id_t>::valid(docs_seek->value()));

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
      ASSERT_TRUE(irs::type_limits<irs::type_t::doc_id_t>::eof(docs->value()));
      ASSERT_TRUE(irs::type_limits<irs::type_t::doc_id_t>::eof(docs_seek->seek(irs::type_limits<irs::type_t::doc_id_t>::eof())));
    }

    // "quick b_o% f_%"
    {
      irs::bytes_ref actual_value;

      irs::by_phrase q;
      irs::by_phrase::wildcard_term wt1;
      wt1.term = irs::ref_cast<irs::byte_type>(irs::string_ref("b_o%"));
      irs::by_phrase::wildcard_term wt2;
      wt2.term = irs::ref_cast<irs::byte_type>(irs::string_ref("f_%"));
      q.field("phrase_anl")
       .push_back(irs::by_phrase::simple_term{irs::ref_cast<irs::byte_type>(irs::string_ref("quick"))})
       .push_back(std::move(wt1)).push_back(std::move(wt2));

      auto prepared = q.prepare(rdr);
      auto sub = rdr.begin();
      auto column = sub->column_reader("name");
      ASSERT_NE(nullptr, column);
      auto values = column->values();

      auto docs = prepared->execute(*sub);
      auto& doc = docs->attributes().get<irs::document>();
      ASSERT_TRUE(bool(doc));
      ASSERT_EQ(docs->value(), doc->value);
      ASSERT_FALSE(irs::type_limits<irs::type_t::doc_id_t>::valid(docs->value()));
      auto docs_seek = prepared->execute(*sub);
      ASSERT_FALSE(irs::type_limits<irs::type_t::doc_id_t>::valid(docs_seek->value()));

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
      ASSERT_TRUE(irs::type_limits<irs::type_t::doc_id_t>::eof(docs->value()));
      ASSERT_TRUE(irs::type_limits<irs::type_t::doc_id_t>::eof(docs_seek->seek(irs::type_limits<irs::type_t::doc_id_t>::eof())));
    }

    // "qui* bro* fo*"
    {
      irs::bytes_ref actual_value;

      irs::by_phrase q;
      irs::by_phrase::prefix_term pt1;
      pt1.term = irs::ref_cast<irs::byte_type>(irs::string_ref("qui"));
      irs::by_phrase::prefix_term pt2;
      pt2.term = irs::ref_cast<irs::byte_type>(irs::string_ref("bro"));
      irs::by_phrase::prefix_term pt3;
      pt3.term = irs::ref_cast<irs::byte_type>(irs::string_ref("fo"));
      q.field("phrase_anl").push_back(std::move(pt1)).push_back(std::move(pt2)).push_back(std::move(pt3));

      auto prepared = q.prepare(rdr);
      auto sub = rdr.begin();
      auto column = sub->column_reader("name");
      ASSERT_NE(nullptr, column);
      auto values = column->values();

      auto docs = prepared->execute(*sub);
      auto& doc = docs->attributes().get<irs::document>();
      ASSERT_TRUE(bool(doc));
      ASSERT_EQ(docs->value(), doc->value);
      ASSERT_FALSE(irs::type_limits<irs::type_t::doc_id_t>::valid(docs->value()));
      auto docs_seek = prepared->execute(*sub);
      ASSERT_FALSE(irs::type_limits<irs::type_t::doc_id_t>::valid(docs_seek->value()));

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
      ASSERT_TRUE(irs::type_limits<irs::type_t::doc_id_t>::eof(docs->value()));
      ASSERT_TRUE(irs::type_limits<irs::type_t::doc_id_t>::eof(docs_seek->seek(irs::type_limits<irs::type_t::doc_id_t>::eof())));
    }

    // "qui% bro% fo%"
    {
      irs::bytes_ref actual_value;

      irs::by_phrase q;
      irs::by_phrase::wildcard_term wt1;
      wt1.term = irs::ref_cast<irs::byte_type>(irs::string_ref("qui%"));
      irs::by_phrase::wildcard_term wt2;
      wt2.term = irs::ref_cast<irs::byte_type>(irs::string_ref("bro%"));
      irs::by_phrase::wildcard_term wt3;
      wt3.term = irs::ref_cast<irs::byte_type>(irs::string_ref("fo%"));
      q.field("phrase_anl").push_back(std::move(wt1)).push_back(std::move(wt2)).push_back(std::move(wt3));

      auto prepared = q.prepare(rdr);
      auto sub = rdr.begin();
      auto column = sub->column_reader("name");
      ASSERT_NE(nullptr, column);
      auto values = column->values();

      auto docs = prepared->execute(*sub);
      auto& doc = docs->attributes().get<irs::document>();
      ASSERT_TRUE(bool(doc));
      ASSERT_EQ(docs->value(), doc->value);
      ASSERT_FALSE(irs::type_limits<irs::type_t::doc_id_t>::valid(docs->value()));
      auto docs_seek = prepared->execute(*sub);
      ASSERT_FALSE(irs::type_limits<irs::type_t::doc_id_t>::valid(docs_seek->value()));

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
      ASSERT_TRUE(irs::type_limits<irs::type_t::doc_id_t>::eof(docs->value()));
      ASSERT_TRUE(irs::type_limits<irs::type_t::doc_id_t>::eof(docs_seek->seek(irs::type_limits<irs::type_t::doc_id_t>::eof())));
    }

    // "q%ic_ br_wn _%x"
    {
      irs::bytes_ref actual_value;

      irs::by_phrase q;
      irs::by_phrase::wildcard_term wt1;
      wt1.term = irs::ref_cast<irs::byte_type>(irs::string_ref("q%ic_"));
      irs::by_phrase::wildcard_term wt2;
      wt2.term = irs::ref_cast<irs::byte_type>(irs::string_ref("br_wn"));
      irs::by_phrase::wildcard_term wt3;
      wt3.term = irs::ref_cast<irs::byte_type>(irs::string_ref("_%x"));
      q.field("phrase_anl").push_back(std::move(wt1)).push_back(std::move(wt2)).push_back(std::move(wt3));

      auto prepared = q.prepare(rdr);
      auto sub = rdr.begin();
      auto column = sub->column_reader("name");
      ASSERT_NE(nullptr, column);
      auto values = column->values();

      auto docs = prepared->execute(*sub);
      auto& doc = docs->attributes().get<irs::document>();
      ASSERT_TRUE(bool(doc));
      ASSERT_EQ(docs->value(), doc->value);
      ASSERT_FALSE(irs::type_limits<irs::type_t::doc_id_t>::valid(docs->value()));
      auto docs_seek = prepared->execute(*sub);
      ASSERT_FALSE(irs::type_limits<irs::type_t::doc_id_t>::valid(docs_seek->value()));

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
      ASSERT_TRUE(irs::type_limits<irs::type_t::doc_id_t>::eof(docs->value()));
      ASSERT_TRUE(irs::type_limits<irs::type_t::doc_id_t>::eof(docs_seek->seek(irs::type_limits<irs::type_t::doc_id_t>::eof())));
    }

    // "quick|quilt|hhh brown|brother fox"
    {
      irs::bytes_ref actual_value;
      irs::by_phrase q;
      q.field("phrase_anl")
       .push_back(irs::by_phrase::set_term{{irs::ref_cast<irs::byte_type>(irs::string_ref("quick")),
                                            irs::ref_cast<irs::byte_type>(irs::string_ref("quilt")),
                                            irs::ref_cast<irs::byte_type>(irs::string_ref("hhh"))}})
       .push_back(irs::by_phrase::set_term{{irs::ref_cast<irs::byte_type>(irs::string_ref("brown")),
                                            irs::ref_cast<irs::byte_type>(irs::string_ref("brother"))}})
       .push_back(irs::by_phrase::set_term{{irs::ref_cast<irs::byte_type>(irs::string_ref("fox"))}});

      auto prepared = q.prepare(rdr);
      auto sub = rdr.begin();
      auto column = sub->column_reader("name");
      ASSERT_NE(nullptr, column);
      auto values = column->values();

      auto docs = prepared->execute(*sub);
      auto& doc = docs->attributes().get<irs::document>();
      ASSERT_TRUE(bool(doc));
      ASSERT_EQ(docs->value(), doc->value);
      ASSERT_FALSE(irs::type_limits<irs::type_t::doc_id_t>::valid(docs->value()));
      auto docs_seek = prepared->execute(*sub);
      ASSERT_FALSE(irs::type_limits<irs::type_t::doc_id_t>::valid(docs_seek->value()));

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
      ASSERT_TRUE(irs::type_limits<irs::type_t::doc_id_t>::eof(docs->value()));
      ASSERT_TRUE(irs::type_limits<irs::type_t::doc_id_t>::eof(docs_seek->seek(irs::type_limits<irs::type_t::doc_id_t>::eof())));
    }

    // "quick brown fox" with order
    {
      irs::bytes_ref actual_value;

      irs::by_phrase q;
      q.field("phrase_anl")
       .push_back(irs::by_phrase::simple_term{irs::ref_cast<irs::byte_type>(irs::string_ref("quick"))})
       .push_back(irs::by_phrase::simple_term{irs::ref_cast<irs::byte_type>(irs::string_ref("brown"))})
       .push_back(irs::by_phrase::simple_term{irs::ref_cast<irs::byte_type>(irs::string_ref("fox"))});

      size_t collect_field_count = 0;
      size_t collect_term_count = 0;
      size_t finish_count = 0;
      irs::order ord;
      auto& sort = ord.add<tests::sort::custom_sort>(false);

      sort.collector_collect_field = [&collect_field_count](const irs::sub_reader&, const irs::term_reader&)->void{
        ++collect_field_count;
      };
      sort.collector_collect_term = [&collect_term_count](const irs::sub_reader&, const irs::term_reader&, const irs::attribute_view&)->void{
        ++collect_term_count;
      };
      sort.collectors_collect_ = [&finish_count](irs::byte_type*, const irs::index_reader&, const irs::sort::field_collector*, const irs::sort::term_collector*)->void {
        ++finish_count;
      };
      sort.prepare_field_collector_ = [&sort]()->irs::sort::field_collector::ptr {
        return irs::memory::make_unique<tests::sort::custom_sort::prepared::collector>(sort);
      };
      sort.prepare_term_collector_ = [&sort]()->irs::sort::term_collector::ptr {
        return irs::memory::make_unique<tests::sort::custom_sort::prepared::collector>(sort);
      };
      sort.scorer_add = [](irs::doc_id_t& dst, const irs::doc_id_t& src)->void {
        ASSERT_TRUE(
          irs::type_limits<irs::type_t::doc_id_t>::invalid() == dst
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
      auto column = sub->column_reader("name");
      ASSERT_NE(nullptr, column);
      auto values = column->values();
      auto docs = prepared->execute(*sub, pord);
      auto& doc = docs->attributes().get<irs::document>();
      ASSERT_TRUE(bool(doc));
      ASSERT_EQ(docs->value(), doc->value);
      ASSERT_FALSE(irs::type_limits<irs::type_t::doc_id_t>::valid(docs->value()));
      auto docs_seek = prepared->execute(*sub);
      ASSERT_FALSE(irs::type_limits<irs::type_t::doc_id_t>::valid(docs_seek->value()));
      auto& score = docs->attributes().get<irs::score>();
      ASSERT_FALSE(!score);

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
      ASSERT_TRUE(irs::type_limits<irs::type_t::doc_id_t>::eof(docs->value()));
      ASSERT_TRUE(irs::type_limits<irs::type_t::doc_id_t>::eof(docs_seek->seek(irs::type_limits<irs::type_t::doc_id_t>::eof())));
    }

    // "fox ... quick"
    {
      irs::bytes_ref actual_value;

      irs::by_phrase q;
      q.field("phrase_anl")
       .push_back(irs::by_phrase::simple_term{irs::ref_cast<irs::byte_type>(irs::string_ref("fox"))})
       .push_back(irs::by_phrase::simple_term{irs::ref_cast<irs::byte_type>(irs::string_ref("quick"))}, 1);

      auto prepared = q.prepare(rdr);

      auto sub = rdr.begin();
      auto column = sub->column_reader("name");
      ASSERT_NE(nullptr, column);
      auto values = column->values();
      auto docs = prepared->execute(*sub);
      auto& doc = docs->attributes().get<irs::document>();
      ASSERT_TRUE(bool(doc));
      ASSERT_EQ(docs->value(), doc->value);
      ASSERT_FALSE(irs::type_limits<irs::type_t::doc_id_t>::valid(docs->value()));
      auto docs_seek = prepared->execute(*sub);
      ASSERT_FALSE(irs::type_limits<irs::type_t::doc_id_t>::valid(docs_seek->value()));

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
      ASSERT_TRUE(irs::type_limits<irs::type_t::doc_id_t>::eof(docs->value()));
      ASSERT_TRUE(irs::type_limits<irs::type_t::doc_id_t>::eof(docs_seek->seek(irs::type_limits<irs::type_t::doc_id_t>::eof())));
    }

    // "fo* ... quick"
    {
      irs::bytes_ref actual_value;

      irs::by_phrase q;
      irs::by_phrase::prefix_term pt;
      pt.term = irs::ref_cast<irs::byte_type>(irs::string_ref("fo"));
      q.field("phrase_anl").push_back(std::move(pt))
       .push_back(irs::by_phrase::simple_term{irs::ref_cast<irs::byte_type>(irs::string_ref("quick"))}, 1);

      auto prepared = q.prepare(rdr);

      auto sub = rdr.begin();
      auto column = sub->column_reader("name");
      ASSERT_NE(nullptr, column);
      auto values = column->values();
      auto docs = prepared->execute(*sub);
      auto& doc = docs->attributes().get<irs::document>();
      ASSERT_TRUE(bool(doc));
      ASSERT_EQ(docs->value(), doc->value);
      ASSERT_FALSE(irs::type_limits<irs::type_t::doc_id_t>::valid(docs->value()));
      auto docs_seek = prepared->execute(*sub);
      ASSERT_FALSE(irs::type_limits<irs::type_t::doc_id_t>::valid(docs_seek->value()));

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
      ASSERT_TRUE(irs::type_limits<irs::type_t::doc_id_t>::eof(docs->value()));
      ASSERT_TRUE(irs::type_limits<irs::type_t::doc_id_t>::eof(docs_seek->seek(irs::type_limits<irs::type_t::doc_id_t>::eof())));
    }

    // "f_x ... quick"
    {
      irs::bytes_ref actual_value;

      irs::by_phrase q;
      irs::by_phrase::wildcard_term wt;
      wt.term = irs::ref_cast<irs::byte_type>(irs::string_ref("f_x"));
      q.field("phrase_anl").push_back(std::move(wt))
       .push_back(irs::by_phrase::simple_term{irs::ref_cast<irs::byte_type>(irs::string_ref("quick"))}, 1);

      auto prepared = q.prepare(rdr);

      auto sub = rdr.begin();
      auto column = sub->column_reader("name");
      ASSERT_NE(nullptr, column);
      auto values = column->values();
      auto docs = prepared->execute(*sub);
      auto& doc = docs->attributes().get<irs::document>();
      ASSERT_TRUE(bool(doc));
      ASSERT_EQ(docs->value(), doc->value);
      ASSERT_FALSE(irs::type_limits<irs::type_t::doc_id_t>::valid(docs->value()));
      auto docs_seek = prepared->execute(*sub);
      ASSERT_FALSE(irs::type_limits<irs::type_t::doc_id_t>::valid(docs_seek->value()));

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
      ASSERT_TRUE(irs::type_limits<irs::type_t::doc_id_t>::eof(docs->value()));
      ASSERT_TRUE(irs::type_limits<irs::type_t::doc_id_t>::eof(docs_seek->seek(irs::type_limits<irs::type_t::doc_id_t>::eof())));
    }

    // "fpx ... quick"
    {
      irs::bytes_ref actual_value;

      irs::by_phrase q;
      irs::by_phrase::levenshtein_term lt;
      lt.max_distance = 1;
      lt.term = irs::ref_cast<irs::byte_type>(irs::string_ref("fpx"));
      q.field("phrase_anl").push_back(std::move(lt))
       .push_back(irs::by_phrase::simple_term{irs::ref_cast<irs::byte_type>(irs::string_ref("quick"))}, 1);

      auto prepared = q.prepare(rdr);

      auto sub = rdr.begin();
      auto column = sub->column_reader("name");
      ASSERT_NE(nullptr, column);
      auto values = column->values();
      auto docs = prepared->execute(*sub);
      auto& doc = docs->attributes().get<irs::document>();
      ASSERT_TRUE(bool(doc));
      ASSERT_EQ(docs->value(), doc->value);
      ASSERT_FALSE(irs::type_limits<irs::type_t::doc_id_t>::valid(docs->value()));
      auto docs_seek = prepared->execute(*sub);
      ASSERT_FALSE(irs::type_limits<irs::type_t::doc_id_t>::valid(docs_seek->value()));

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
      ASSERT_TRUE(irs::type_limits<irs::type_t::doc_id_t>::eof(docs->value()));
      ASSERT_TRUE(irs::type_limits<irs::type_t::doc_id_t>::eof(docs_seek->seek(irs::type_limits<irs::type_t::doc_id_t>::eof())));
    }

    // "fox ... qui*"
    {
      irs::bytes_ref actual_value;

      irs::by_phrase q;
      irs::by_phrase::prefix_term pt;
      pt.term = irs::ref_cast<irs::byte_type>(irs::string_ref("qui"));
      q.field("phrase_anl")
       .push_back(irs::by_phrase::simple_term{irs::ref_cast<irs::byte_type>(irs::string_ref("fox"))})
       .push_back(std::move(pt), 1);

      auto prepared = q.prepare(rdr);

      auto sub = rdr.begin();
      auto column = sub->column_reader("name");
      ASSERT_NE(nullptr, column);
      auto values = column->values();
      auto docs = prepared->execute(*sub);
      auto& doc = docs->attributes().get<irs::document>();
      ASSERT_TRUE(bool(doc));
      ASSERT_EQ(docs->value(), doc->value);
      ASSERT_FALSE(irs::type_limits<irs::type_t::doc_id_t>::valid(docs->value()));
      auto docs_seek = prepared->execute(*sub);
      ASSERT_FALSE(irs::type_limits<irs::type_t::doc_id_t>::valid(docs_seek->value()));

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
      ASSERT_TRUE(irs::type_limits<irs::type_t::doc_id_t>::eof(docs->value()));
      ASSERT_TRUE(irs::type_limits<irs::type_t::doc_id_t>::eof(docs_seek->seek(irs::type_limits<irs::type_t::doc_id_t>::eof())));
    }

    // "fox ... qui%ck"
    {
      irs::bytes_ref actual_value;

      irs::by_phrase q;
      irs::by_phrase::wildcard_term wt;
      wt.term = irs::ref_cast<irs::byte_type>(irs::string_ref("qui%ck"));
      q.field("phrase_anl")
       .push_back(irs::by_phrase::simple_term{irs::ref_cast<irs::byte_type>(irs::string_ref("fox"))})
       .push_back(std::move(wt), 1);

      auto prepared = q.prepare(rdr);

      auto sub = rdr.begin();
      auto column = sub->column_reader("name");
      ASSERT_NE(nullptr, column);
      auto values = column->values();
      auto docs = prepared->execute(*sub);
      auto& doc = docs->attributes().get<irs::document>();
      ASSERT_TRUE(bool(doc));
      ASSERT_EQ(docs->value(), doc->value);
      ASSERT_FALSE(irs::type_limits<irs::type_t::doc_id_t>::valid(docs->value()));
      auto docs_seek = prepared->execute(*sub);
      ASSERT_FALSE(irs::type_limits<irs::type_t::doc_id_t>::valid(docs_seek->value()));

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
      ASSERT_TRUE(irs::type_limits<irs::type_t::doc_id_t>::eof(docs->value()));
      ASSERT_TRUE(irs::type_limits<irs::type_t::doc_id_t>::eof(docs_seek->seek(irs::type_limits<irs::type_t::doc_id_t>::eof())));
    }

    // "fo* ... qui*"
    {
      irs::bytes_ref actual_value;

      irs::by_phrase q;
      irs::by_phrase::prefix_term pt1;
      pt1.term = irs::ref_cast<irs::byte_type>(irs::string_ref("fo"));
      irs::by_phrase::prefix_term pt2;
      pt2.term = irs::ref_cast<irs::byte_type>(irs::string_ref("qui"));
      q.field("phrase_anl").push_back(std::move(pt1)).push_back(std::move(pt2), 1);

      auto prepared = q.prepare(rdr);

      auto sub = rdr.begin();
      auto column = sub->column_reader("name");
      ASSERT_NE(nullptr, column);
      auto values = column->values();
      auto docs = prepared->execute(*sub);
      auto& doc = docs->attributes().get<irs::document>();
      ASSERT_TRUE(bool(doc));
      ASSERT_EQ(docs->value(), doc->value);
      ASSERT_FALSE(irs::type_limits<irs::type_t::doc_id_t>::valid(docs->value()));
      auto docs_seek = prepared->execute(*sub);
      ASSERT_FALSE(irs::type_limits<irs::type_t::doc_id_t>::valid(docs_seek->value()));

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
      ASSERT_TRUE(irs::type_limits<irs::type_t::doc_id_t>::eof(docs->value()));
      ASSERT_TRUE(irs::type_limits<irs::type_t::doc_id_t>::eof(docs_seek->seek(irs::type_limits<irs::type_t::doc_id_t>::eof())));
    }

    // "f%x ... qui%ck"
    {
      irs::bytes_ref actual_value;

      irs::by_phrase q;
      irs::by_phrase::wildcard_term wt1;
      wt1.term = irs::ref_cast<irs::byte_type>(irs::string_ref("f%x"));
      irs::by_phrase::wildcard_term wt2;
      wt2.term = irs::ref_cast<irs::byte_type>(irs::string_ref("qui%ck"));
      q.field("phrase_anl").push_back(std::move(wt1)).push_back(std::move(wt2), 1);

      auto prepared = q.prepare(rdr);

      auto sub = rdr.begin();
      auto column = sub->column_reader("name");
      ASSERT_NE(nullptr, column);
      auto values = column->values();
      auto docs = prepared->execute(*sub);
      auto& doc = docs->attributes().get<irs::document>();
      ASSERT_TRUE(bool(doc));
      ASSERT_EQ(docs->value(), doc->value);
      ASSERT_FALSE(irs::type_limits<irs::type_t::doc_id_t>::valid(docs->value()));
      auto docs_seek = prepared->execute(*sub);
      ASSERT_FALSE(irs::type_limits<irs::type_t::doc_id_t>::valid(docs_seek->value()));

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
      ASSERT_TRUE(irs::type_limits<irs::type_t::doc_id_t>::eof(docs->value()));
      ASSERT_TRUE(irs::type_limits<irs::type_t::doc_id_t>::eof(docs_seek->seek(irs::type_limits<irs::type_t::doc_id_t>::eof())));
    }

    // "fx ... quik"
    {
      irs::bytes_ref actual_value;

      irs::by_phrase q;
      irs::by_phrase::levenshtein_term lt1;
      lt1.max_distance = 1;
      lt1.term = irs::ref_cast<irs::byte_type>(irs::string_ref("fx"));
      irs::by_phrase::levenshtein_term lt2;
      lt2.max_distance = 1;
      lt2.term = irs::ref_cast<irs::byte_type>(irs::string_ref("quik"));
      q.field("phrase_anl").push_back(std::move(lt1)).push_back(std::move(lt2), 1);

      auto prepared = q.prepare(rdr);

      auto sub = rdr.begin();
      auto column = sub->column_reader("name");
      ASSERT_NE(nullptr, column);
      auto values = column->values();
      auto docs = prepared->execute(*sub);
      auto& doc = docs->attributes().get<irs::document>();
      ASSERT_TRUE(bool(doc));
      ASSERT_EQ(docs->value(), doc->value);
      ASSERT_FALSE(irs::type_limits<irs::type_t::doc_id_t>::valid(docs->value()));
      auto docs_seek = prepared->execute(*sub);
      ASSERT_FALSE(irs::type_limits<irs::type_t::doc_id_t>::valid(docs_seek->value()));

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
      ASSERT_TRUE(irs::type_limits<irs::type_t::doc_id_t>::eof(docs->value()));
      ASSERT_TRUE(irs::type_limits<irs::type_t::doc_id_t>::eof(docs_seek->seek(irs::type_limits<irs::type_t::doc_id_t>::eof())));
    }

    // =============================
    // "fo* ... qui*" with scorer
    {
      irs::bytes_ref actual_value;

      irs::by_phrase q;
      irs::by_phrase::prefix_term pt1;
      pt1.term = irs::ref_cast<irs::byte_type>(irs::string_ref("fo"));
      irs::by_phrase::prefix_term pt2;
      pt2.term = irs::ref_cast<irs::byte_type>(irs::string_ref("qui"));
      q.field("phrase_anl").push_back(std::move(pt1)).push_back(std::move(pt2), 1);

      irs::order order;
      order.add(true, irs::scorers::get("bm25", irs::text_format::json, "{ \"b\" : 0 }"));
      auto prepared_order = order.prepare();

      auto prepared = q.prepare(rdr, prepared_order);

      auto sub = rdr.begin();
      auto column = sub->column_reader("name");
      ASSERT_NE(nullptr, column);
      auto values = column->values();
      auto docs = prepared->execute(*sub, prepared_order);
      auto& doc = docs->attributes().get<irs::document>();
      ASSERT_TRUE(bool(doc));
      ASSERT_EQ(docs->value(), doc->value);
      ASSERT_FALSE(irs::type_limits<irs::type_t::doc_id_t>::valid(docs->value()));
      auto docs_seek = prepared->execute(*sub);
      ASSERT_FALSE(irs::type_limits<irs::type_t::doc_id_t>::valid(docs_seek->value()));

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
      ASSERT_TRUE(irs::type_limits<irs::type_t::doc_id_t>::eof(docs->value()));
      ASSERT_TRUE(irs::type_limits<irs::type_t::doc_id_t>::eof(docs_seek->seek(irs::type_limits<irs::type_t::doc_id_t>::eof())));
    }

    // set_term "fox|that" with scorer
    {
      irs::bytes_ref actual_value;

      irs::by_phrase q;
      q.field("phrase_anl")
       .push_back(irs::by_phrase::set_term{{irs::ref_cast<irs::byte_type>(irs::string_ref("fox")),
                                            irs::ref_cast<irs::byte_type>(irs::string_ref("that"))}});

      irs::order order;
      order.add(true, irs::scorers::get("bm25", irs::text_format::json, "{ \"b\" : 0 }"));
      auto prepared_order = order.prepare();

      auto prepared = q.prepare(rdr, prepared_order);

      auto sub = rdr.begin();
      auto column = sub->column_reader("name");
      ASSERT_NE(nullptr, column);
      auto values = column->values();
      auto docs = prepared->execute(*sub, prepared_order);
      auto& doc = docs->attributes().get<irs::document>();
      ASSERT_TRUE(bool(doc));
      ASSERT_EQ(docs->value(), doc->value);
      ASSERT_FALSE(irs::type_limits<irs::type_t::doc_id_t>::valid(docs->value()));
      auto docs_seek = prepared->execute(*sub);
      ASSERT_FALSE(irs::type_limits<irs::type_t::doc_id_t>::valid(docs_seek->value()));

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
      ASSERT_TRUE(irs::type_limits<irs::type_t::doc_id_t>::eof(docs->value()));
    }

    // test disjunctions (unary, basic, small, disjunction)
    {
      irs::bytes_ref actual_value;

      irs::by_phrase q;
      irs::by_phrase::wildcard_term wt1;
      wt1.term = irs::ref_cast<irs::byte_type>(irs::string_ref("%las"));
      irs::by_phrase::wildcard_term wt2;
      wt2.term = irs::ref_cast<irs::byte_type>(irs::string_ref("%nd"));
      irs::by_phrase::prefix_term pt1;
      pt1.term = irs::ref_cast<irs::byte_type>(irs::string_ref("go"));
      irs::by_phrase::prefix_term pt2;
      pt2.term = irs::ref_cast<irs::byte_type>(irs::string_ref("like"));
      q.field("phrase_anl").push_back(std::move(wt1)).push_back(std::move(wt2))
       .push_back(std::move(pt1)).push_back(std::move(pt2));

      irs::order order;
      order.add(true, irs::scorers::get("bm25", irs::text_format::json, "{ \"b\" : 0 }"));
      auto prepared_order = order.prepare();

      auto prepared = q.prepare(rdr, prepared_order);

      auto sub = rdr.begin();
      auto column = sub->column_reader("name");
      ASSERT_NE(nullptr, column);
      auto values = column->values();
      auto docs = prepared->execute(*sub, prepared_order);
      auto& doc = docs->attributes().get<irs::document>();
      ASSERT_TRUE(bool(doc));
      ASSERT_EQ(docs->value(), doc->value);
      ASSERT_FALSE(irs::type_limits<irs::type_t::doc_id_t>::valid(docs->value()));
      auto docs_seek = prepared->execute(*sub);
      ASSERT_FALSE(irs::type_limits<irs::type_t::doc_id_t>::valid(docs_seek->value()));

      ASSERT_TRUE(docs->next());
      ASSERT_EQ(docs->value(), doc->value);
      ASSERT_TRUE(values(docs->value(), actual_value));
      ASSERT_EQ("Z", irs::to_string<irs::string_ref>(actual_value.c_str()));
      ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
      ASSERT_TRUE(values(docs->value(), actual_value));
      ASSERT_EQ("Z", irs::to_string<irs::string_ref>(actual_value.c_str()));

      ASSERT_FALSE(docs->next());
      ASSERT_EQ(docs->value(), doc->value);
      ASSERT_TRUE(irs::type_limits<irs::type_t::doc_id_t>::eof(docs->value()));
    }

    // =============================

    // "fox ... quick" with phrase offset
    // which is does not matter
    {
      irs::bytes_ref actual_value;

      irs::by_phrase q;
      q.field("phrase_anl")
       .push_back(irs::by_phrase::simple_term{irs::ref_cast<irs::byte_type>(irs::string_ref("fox"))}, irs::integer_traits<size_t>::const_max)
       .push_back(irs::by_phrase::simple_term{irs::ref_cast<irs::byte_type>(irs::string_ref("quick"))}, 1);

      auto prepared = q.prepare(rdr);

      auto sub = rdr.begin();
      auto column = sub->column_reader("name");
      ASSERT_NE(nullptr, column);
      auto values = column->values();
      auto docs = prepared->execute(*sub);
      auto& doc = docs->attributes().get<irs::document>();
      ASSERT_TRUE(bool(doc));
      ASSERT_EQ(docs->value(), doc->value);
      ASSERT_FALSE(irs::type_limits<irs::type_t::doc_id_t>::valid( docs->value()));
      auto docs_seek = prepared->execute(*sub);
      ASSERT_FALSE(irs::type_limits<irs::type_t::doc_id_t>::valid(docs_seek->value()));

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
      ASSERT_TRUE(irs::type_limits<irs::type_t::doc_id_t>::eof(docs->value()));
      ASSERT_TRUE(irs::type_limits<irs::type_t::doc_id_t>::eof(docs_seek->seek(irs::type_limits<irs::type_t::doc_id_t>::eof())));
    }

    // "fox quick"
    // const_max and zero offset
    {
      irs::bytes_ref actual_value;

      irs::by_phrase q;
      q.field("phrase_anl")
       .push_back(irs::by_phrase::simple_term{irs::ref_cast<irs::byte_type>(irs::string_ref("fox"))}, irs::integer_traits<size_t>::const_max)
       .push_back(irs::by_phrase::simple_term{irs::ref_cast<irs::byte_type>(irs::string_ref("quick"))}, 0);

      auto prepared = q.prepare(rdr);

      auto sub = rdr.begin();
      auto column = sub->column_reader("name");
      ASSERT_NE(nullptr, column);
      auto values = column->values();
      auto docs = prepared->execute(*sub);
      auto& doc = docs->attributes().get<irs::document>();
      ASSERT_TRUE(bool(doc));
      ASSERT_EQ(docs->value(), doc->value);
      ASSERT_FALSE(irs::type_limits<irs::type_t::doc_id_t>::valid( docs->value()));
      auto docs_seek = prepared->execute(*sub);
      ASSERT_FALSE(irs::type_limits<irs::type_t::doc_id_t>::valid(docs_seek->value()));

      ASSERT_TRUE(docs->next());
      ASSERT_EQ(docs->value(), doc->value);
      ASSERT_TRUE(values(docs->value(), actual_value));
      ASSERT_EQ("N", irs::to_string<irs::string_ref>(actual_value.c_str()));
      ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
      ASSERT_TRUE(values(docs->value(), actual_value));
      ASSERT_EQ("N", irs::to_string<irs::string_ref>(actual_value.c_str()));

      ASSERT_FALSE(docs->next());
      ASSERT_TRUE(irs::type_limits<irs::type_t::doc_id_t>::eof(docs->value()));
      ASSERT_TRUE(irs::type_limits<irs::type_t::doc_id_t>::eof(docs_seek->seek(irs::type_limits<irs::type_t::doc_id_t>::eof())));
    }

    // "fox* quick*"
    // const_max and zero offset
    {
      irs::bytes_ref actual_value;

      irs::by_phrase q;
      irs::by_phrase::prefix_term pt1;
      pt1.term = irs::ref_cast<irs::byte_type>(irs::string_ref("fox"));
      irs::by_phrase::prefix_term pt2;
      pt2.term = irs::ref_cast<irs::byte_type>(irs::string_ref("quick"));
      q.field("phrase_anl")
       .push_back(std::move(pt1), irs::integer_traits<size_t>::const_max)
       .push_back(std::move(pt2), 0);

      auto prepared = q.prepare(rdr);

      auto sub = rdr.begin();
      auto column = sub->column_reader("name");
      ASSERT_NE(nullptr, column);
      auto values = column->values();
      auto docs = prepared->execute(*sub);
      auto& doc = docs->attributes().get<irs::document>();
      ASSERT_TRUE(bool(doc));
      ASSERT_EQ(docs->value(), doc->value);
      ASSERT_FALSE(irs::type_limits<irs::type_t::doc_id_t>::valid( docs->value()));
      auto docs_seek = prepared->execute(*sub);
      ASSERT_FALSE(irs::type_limits<irs::type_t::doc_id_t>::valid(docs_seek->value()));

      ASSERT_TRUE(docs->next());
      ASSERT_EQ(docs->value(), doc->value);
      ASSERT_TRUE(values(docs->value(), actual_value));
      ASSERT_EQ("N", irs::to_string<irs::string_ref>(actual_value.c_str()));
      ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
      ASSERT_TRUE(values(docs->value(), actual_value));
      ASSERT_EQ("N", irs::to_string<irs::string_ref>(actual_value.c_str()));

      ASSERT_FALSE(docs->next());
      ASSERT_TRUE(irs::type_limits<irs::type_t::doc_id_t>::eof(docs->value()));
      ASSERT_TRUE(irs::type_limits<irs::type_t::doc_id_t>::eof(docs_seek->seek(irs::type_limits<irs::type_t::doc_id_t>::eof())));
    }

    // "fo* ... quick" with phrase offset
    // which is does not matter
    {
      irs::bytes_ref actual_value;

      irs::by_phrase q;
      irs::by_phrase::prefix_term pt;
      pt.term = irs::ref_cast<irs::byte_type>(irs::string_ref("fo"));
      q.field("phrase_anl")
       .push_back(std::move(pt), irs::integer_traits<size_t>::const_max)
       .push_back(irs::by_phrase::simple_term{irs::ref_cast<irs::byte_type>(irs::string_ref("quick"))}, 1);

      auto prepared = q.prepare(rdr);

      auto sub = rdr.begin();
      auto column = sub->column_reader("name");
      ASSERT_NE(nullptr, column);
      auto values = column->values();
      auto docs = prepared->execute(*sub);
      auto& doc = docs->attributes().get<irs::document>();
      ASSERT_TRUE(bool(doc));
      ASSERT_EQ(docs->value(), doc->value);
      ASSERT_FALSE(irs::type_limits<irs::type_t::doc_id_t>::valid( docs->value()));
      auto docs_seek = prepared->execute(*sub);
      ASSERT_FALSE(irs::type_limits<irs::type_t::doc_id_t>::valid(docs_seek->value()));

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
      ASSERT_TRUE(irs::type_limits<irs::type_t::doc_id_t>::eof(docs->value()));
      ASSERT_TRUE(irs::type_limits<irs::type_t::doc_id_t>::eof(docs_seek->seek(irs::type_limits<irs::type_t::doc_id_t>::eof())));
    }

    // "f_x ... quick" with phrase offset
    // which is does not matter
    {
      irs::bytes_ref actual_value;

      irs::by_phrase q;
      irs::by_phrase::wildcard_term wt;
      wt.term = irs::ref_cast<irs::byte_type>(irs::string_ref("f_x"));
      q.field("phrase_anl")
       .push_back(std::move(wt), irs::integer_traits<size_t>::const_max)
       .push_back(irs::by_phrase::simple_term{irs::ref_cast<irs::byte_type>(irs::string_ref("quick"))}, 1);

      auto prepared = q.prepare(rdr);

      auto sub = rdr.begin();
      auto column = sub->column_reader("name");
      ASSERT_NE(nullptr, column);
      auto values = column->values();
      auto docs = prepared->execute(*sub);
      auto& doc = docs->attributes().get<irs::document>();
      ASSERT_TRUE(bool(doc));
      ASSERT_EQ(docs->value(), doc->value);
      ASSERT_FALSE(irs::type_limits<irs::type_t::doc_id_t>::valid( docs->value()));
      auto docs_seek = prepared->execute(*sub);
      ASSERT_FALSE(irs::type_limits<irs::type_t::doc_id_t>::valid(docs_seek->value()));

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
      ASSERT_TRUE(irs::type_limits<irs::type_t::doc_id_t>::eof(docs->value()));
      ASSERT_TRUE(irs::type_limits<irs::type_t::doc_id_t>::eof(docs_seek->seek(irs::type_limits<irs::type_t::doc_id_t>::eof())));
    }

    // "fox ... qui*" with phrase offset
    // which is does not matter
    {
      irs::bytes_ref actual_value;

      irs::by_phrase q;
      irs::by_phrase::prefix_term pt;
      pt.term = irs::ref_cast<irs::byte_type>(irs::string_ref("qui"));
      q.field("phrase_anl")
       .push_back(irs::by_phrase::simple_term{irs::ref_cast<irs::byte_type>(irs::string_ref("fox"))}, irs::integer_traits<size_t>::const_max)
       .push_back(std::move(pt), 1);

      auto prepared = q.prepare(rdr);

      auto sub = rdr.begin();
      auto column = sub->column_reader("name");
      ASSERT_NE(nullptr, column);
      auto values = column->values();
      auto docs = prepared->execute(*sub);
      auto& doc = docs->attributes().get<irs::document>();
      ASSERT_TRUE(bool(doc));
      ASSERT_EQ(docs->value(), doc->value);
      ASSERT_FALSE(irs::type_limits<irs::type_t::doc_id_t>::valid( docs->value()));
      auto docs_seek = prepared->execute(*sub);
      ASSERT_FALSE(irs::type_limits<irs::type_t::doc_id_t>::valid(docs_seek->value()));

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
      ASSERT_TRUE(irs::type_limits<irs::type_t::doc_id_t>::eof(docs->value()));
      ASSERT_TRUE(irs::type_limits<irs::type_t::doc_id_t>::eof(docs_seek->seek(irs::type_limits<irs::type_t::doc_id_t>::eof())));
    }

    // "fox ... qui%k" with phrase offset
    // which is does not matter
    {
      irs::bytes_ref actual_value;

      irs::by_phrase q;
      irs::by_phrase::wildcard_term wt;
      wt.term = irs::ref_cast<irs::byte_type>(irs::string_ref("qui%k"));
      q.field("phrase_anl")
       .push_back(irs::by_phrase::simple_term{irs::ref_cast<irs::byte_type>(irs::string_ref("fox"))}, irs::integer_traits<size_t>::const_max)
       .push_back(std::move(wt), 1);

      auto prepared = q.prepare(rdr);

      auto sub = rdr.begin();
      auto column = sub->column_reader("name");
      ASSERT_NE(nullptr, column);
      auto values = column->values();
      auto docs = prepared->execute(*sub);
      auto& doc = docs->attributes().get<irs::document>();
      ASSERT_TRUE(bool(doc));
      ASSERT_EQ(docs->value(), doc->value);
      ASSERT_FALSE(irs::type_limits<irs::type_t::doc_id_t>::valid( docs->value()));
      auto docs_seek = prepared->execute(*sub);
      ASSERT_FALSE(irs::type_limits<irs::type_t::doc_id_t>::valid(docs_seek->value()));

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
      ASSERT_TRUE(irs::type_limits<irs::type_t::doc_id_t>::eof(docs->value()));
      ASSERT_TRUE(irs::type_limits<irs::type_t::doc_id_t>::eof(docs_seek->seek(irs::type_limits<irs::type_t::doc_id_t>::eof())));
    }

    // "fo* ... qui*" with phrase offset
    // which is does not matter
    {
      irs::bytes_ref actual_value;

      irs::by_phrase q;
      irs::by_phrase::prefix_term pt1;
      pt1.term = irs::ref_cast<irs::byte_type>(irs::string_ref("fo"));
      irs::by_phrase::prefix_term pt2;
      pt2.term = irs::ref_cast<irs::byte_type>(irs::string_ref("qui"));
      q.field("phrase_anl").push_back(std::move(pt1), irs::integer_traits<size_t>::const_max)
       .push_back(std::move(pt2), 1);

      auto prepared = q.prepare(rdr);

      auto sub = rdr.begin();
      auto column = sub->column_reader("name");
      ASSERT_NE(nullptr, column);
      auto values = column->values();
      auto docs = prepared->execute(*sub);
      auto& doc = docs->attributes().get<irs::document>();
      ASSERT_TRUE(bool(doc));
      ASSERT_EQ(docs->value(), doc->value);
      ASSERT_FALSE(irs::type_limits<irs::type_t::doc_id_t>::valid( docs->value()));
      auto docs_seek = prepared->execute(*sub);
      ASSERT_FALSE(irs::type_limits<irs::type_t::doc_id_t>::valid(docs_seek->value()));

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
      ASSERT_TRUE(irs::type_limits<irs::type_t::doc_id_t>::eof(docs->value()));
      ASSERT_TRUE(irs::type_limits<irs::type_t::doc_id_t>::eof(docs_seek->seek(irs::type_limits<irs::type_t::doc_id_t>::eof())));
    }

    // "fo% ... qui%" with phrase offset
    // which is does not matter
    {
      irs::bytes_ref actual_value;

      irs::by_phrase q;
      irs::by_phrase::wildcard_term wt1;
      wt1.term = irs::ref_cast<irs::byte_type>(irs::string_ref("fo%"));
      irs::by_phrase::wildcard_term wt2;
      wt2.term = irs::ref_cast<irs::byte_type>(irs::string_ref("qui%"));
      q.field("phrase_anl").push_back(std::move(wt1), irs::integer_traits<size_t>::const_max)
       .push_back(std::move(wt2), 1);

      auto prepared = q.prepare(rdr);

      auto sub = rdr.begin();
      auto column = sub->column_reader("name");
      ASSERT_NE(nullptr, column);
      auto values = column->values();
      auto docs = prepared->execute(*sub);
      auto& doc = docs->attributes().get<irs::document>();
      ASSERT_TRUE(bool(doc));
      ASSERT_EQ(docs->value(), doc->value);
      ASSERT_FALSE(irs::type_limits<irs::type_t::doc_id_t>::valid( docs->value()));
      auto docs_seek = prepared->execute(*sub);
      ASSERT_FALSE(irs::type_limits<irs::type_t::doc_id_t>::valid(docs_seek->value()));

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
      ASSERT_TRUE(irs::type_limits<irs::type_t::doc_id_t>::eof(docs->value()));
      ASSERT_TRUE(irs::type_limits<irs::type_t::doc_id_t>::eof(docs_seek->seek(irs::type_limits<irs::type_t::doc_id_t>::eof())));
    }

    // "fo% ... quik" with phrase offset
    // which is does not matter
    {
      irs::bytes_ref actual_value;

      irs::by_phrase q;
      irs::by_phrase::wildcard_term wt;
      wt.term = irs::ref_cast<irs::byte_type>(irs::string_ref("fo%"));
      irs::by_phrase::levenshtein_term lt;
      lt.max_distance = 1;
      lt.term = irs::ref_cast<irs::byte_type>(irs::string_ref("quik"));
      q.field("phrase_anl").push_back(std::move(wt), irs::integer_traits<size_t>::const_max)
       .push_back(std::move(lt), 1);

      auto prepared = q.prepare(rdr);

      auto sub = rdr.begin();
      auto column = sub->column_reader("name");
      ASSERT_NE(nullptr, column);
      auto values = column->values();
      auto docs = prepared->execute(*sub);
      auto& doc = docs->attributes().get<irs::document>();
      ASSERT_TRUE(bool(doc));
      ASSERT_EQ(docs->value(), doc->value);
      ASSERT_FALSE(irs::type_limits<irs::type_t::doc_id_t>::valid( docs->value()));
      auto docs_seek = prepared->execute(*sub);
      ASSERT_FALSE(irs::type_limits<irs::type_t::doc_id_t>::valid(docs_seek->value()));

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
      ASSERT_TRUE(irs::type_limits<irs::type_t::doc_id_t>::eof(docs->value()));
      ASSERT_TRUE(irs::type_limits<irs::type_t::doc_id_t>::eof(docs_seek->seek(irs::type_limits<irs::type_t::doc_id_t>::eof())));
    }

    // "fox ... ... ... ... ... ... ... ... ... ... quick"
    {
      irs::by_phrase q;
      q.field("phrase_anl")
       .push_back(irs::by_phrase::simple_term{irs::ref_cast<irs::byte_type>(irs::string_ref("fox"))})
       .push_back(irs::by_phrase::simple_term{irs::ref_cast<irs::byte_type>(irs::string_ref("quick"))}, 10);

      auto prepared = q.prepare(rdr);

      auto sub = rdr.begin();
      auto docs = prepared->execute(*sub);
      auto& doc = docs->attributes().get<irs::document>();
      ASSERT_TRUE(bool(doc));
      ASSERT_EQ(docs->value(), doc->value);
      ASSERT_FALSE(irs::type_limits<irs::type_t::doc_id_t>::valid(docs->value()));
      ASSERT_FALSE(docs->next());
      ASSERT_TRUE(irs::type_limits<irs::type_t::doc_id_t>::eof(docs->value()));
    }

    // "fox ... ... ... ... ... ... ... ... ... ... qui*"
    {
      irs::by_phrase q;
      irs::by_phrase::prefix_term pt;
      pt.term = irs::ref_cast<irs::byte_type>(irs::string_ref("qui"));
      q.field("phrase_anl")
       .push_back(irs::by_phrase::simple_term{irs::ref_cast<irs::byte_type>(irs::string_ref("fox"))})
       .push_back(std::move(pt), 10);

      auto prepared = q.prepare(rdr);

      auto sub = rdr.begin();
      auto docs = prepared->execute(*sub);
      auto& doc = docs->attributes().get<irs::document>();
      ASSERT_TRUE(bool(doc));
      ASSERT_EQ(docs->value(), doc->value);
      ASSERT_FALSE(irs::type_limits<irs::type_t::doc_id_t>::valid(docs->value()));
      ASSERT_FALSE(docs->next());
      ASSERT_TRUE(irs::type_limits<irs::type_t::doc_id_t>::eof(docs->value()));
    }

    // "fox ... ... ... ... ... ... ... ... ... ... qu_ck"
    {
      irs::by_phrase q;
      irs::by_phrase::wildcard_term wt;
      wt.term = irs::ref_cast<irs::byte_type>(irs::string_ref("qu_ck"));
      q.field("phrase_anl")
       .push_back(irs::by_phrase::simple_term{irs::ref_cast<irs::byte_type>(irs::string_ref("fox"))})
       .push_back(std::move(wt), 10);

      auto prepared = q.prepare(rdr);

      auto sub = rdr.begin();
      auto docs = prepared->execute(*sub);
      auto& doc = docs->attributes().get<irs::document>();
      ASSERT_TRUE(bool(doc));
      ASSERT_EQ(docs->value(), doc->value);
      ASSERT_FALSE(irs::type_limits<irs::type_t::doc_id_t>::valid(docs->value()));
      ASSERT_FALSE(docs->next());
      ASSERT_TRUE(irs::type_limits<irs::type_t::doc_id_t>::eof(docs->value()));
    }

    // "fox ... ... ... ... ... ... ... ... ... ... quc"
    {
      irs::by_phrase q;
      irs::by_phrase::levenshtein_term lt;
      lt.max_distance = 2;
      lt.term = irs::ref_cast<irs::byte_type>(irs::string_ref("quc"));
      q.field("phrase_anl")
       .push_back(irs::by_phrase::simple_term{irs::ref_cast<irs::byte_type>(irs::string_ref("fox"))})
       .push_back(std::move(lt), 10);

      auto prepared = q.prepare(rdr);

      auto sub = rdr.begin();
      auto docs = prepared->execute(*sub);
      auto& doc = docs->attributes().get<irs::document>();
      ASSERT_TRUE(bool(doc));
      ASSERT_EQ(docs->value(), doc->value);
      ASSERT_FALSE(irs::type_limits<irs::type_t::doc_id_t>::valid(docs->value()));
      ASSERT_FALSE(docs->next());
      ASSERT_TRUE(irs::type_limits<irs::type_t::doc_id_t>::eof(docs->value()));
    }

    // "eye ... eye"
    {
      irs::bytes_ref actual_value;

      irs::by_phrase q;
      q.field("phrase_anl")
       .push_back(irs::by_phrase::simple_term{irs::ref_cast<irs::byte_type>(irs::string_ref("eye"))})
       .push_back(irs::by_phrase::simple_term{irs::ref_cast<irs::byte_type>(irs::string_ref("eye"))}, 1);

      auto prepared = q.prepare(rdr);

      auto sub = rdr.begin();
      auto column = sub->column_reader("name");
      ASSERT_NE(nullptr, column);
      auto values = column->values();
      auto docs = prepared->execute(*sub);
      auto& doc = docs->attributes().get<irs::document>();
      ASSERT_TRUE(bool(doc));
      ASSERT_EQ(docs->value(), doc->value);
      ASSERT_FALSE(irs::type_limits<irs::type_t::doc_id_t>::valid(docs->value()));
      auto docs_seek = prepared->execute(*sub);
      ASSERT_FALSE(irs::type_limits<irs::type_t::doc_id_t>::valid(docs_seek->value()));

      ASSERT_TRUE(docs->next());
      ASSERT_TRUE(values(docs->value(), actual_value));
      ASSERT_EQ("C", irs::to_string<irs::string_ref>(actual_value.c_str()));
      ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
      ASSERT_TRUE(values(docs->value(), actual_value));
      ASSERT_EQ("C", irs::to_string<irs::string_ref>(actual_value.c_str()));

      ASSERT_FALSE(docs->next());
      ASSERT_TRUE(irs::type_limits<irs::type_t::doc_id_t>::eof(docs->value()));
      ASSERT_TRUE(irs::type_limits<irs::type_t::doc_id_t>::eof(docs_seek->seek(irs::type_limits<irs::type_t::doc_id_t>::eof())));
    }

    // "as in the past we are looking forward"
    {
      irs::bytes_ref actual_value;

      irs::by_phrase q;
      q.field("phrase_anl")
       .push_back(irs::by_phrase::simple_term{irs::ref_cast<irs::byte_type>(irs::string_ref("as"))})
       .push_back(irs::by_phrase::simple_term{irs::ref_cast<irs::byte_type>(irs::string_ref("in"))})
       .push_back(irs::by_phrase::simple_term{irs::ref_cast<irs::byte_type>(irs::string_ref("the"))})
       .push_back(irs::by_phrase::simple_term{irs::ref_cast<irs::byte_type>(irs::string_ref("past"))})
       .push_back(irs::by_phrase::simple_term{irs::ref_cast<irs::byte_type>(irs::string_ref("we"))})
       .push_back(irs::by_phrase::simple_term{irs::ref_cast<irs::byte_type>(irs::string_ref("are"))})
       .push_back(irs::by_phrase::simple_term{irs::ref_cast<irs::byte_type>(irs::string_ref("looking"))})
       .push_back(irs::by_phrase::simple_term{irs::ref_cast<irs::byte_type>(irs::string_ref("forward"))});

      auto prepared = q.prepare(rdr);
      auto sub = rdr.begin();
      auto column = sub->column_reader("name");
      ASSERT_NE(nullptr, column);
      auto values = column->values();
      auto docs = prepared->execute(*sub);
      auto& doc = docs->attributes().get<irs::document>();
      ASSERT_TRUE(bool(doc));
      ASSERT_EQ(docs->value(), doc->value);
      ASSERT_FALSE(irs::type_limits<irs::type_t::doc_id_t>::valid(docs->value()));
      auto docs_seek = prepared->execute(*sub);
      ASSERT_FALSE(irs::type_limits<irs::type_t::doc_id_t>::valid(docs_seek->value()));

      ASSERT_TRUE(docs->next());
      ASSERT_TRUE(values(docs->value(), actual_value));
      ASSERT_EQ("H", irs::to_string<irs::string_ref>(actual_value.c_str()));
      ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
      ASSERT_TRUE(values(docs->value(), actual_value));
      ASSERT_EQ("H", irs::to_string<irs::string_ref>(actual_value.c_str()));

      ASSERT_FALSE(docs->next());
      ASSERT_TRUE(irs::type_limits<irs::type_t::doc_id_t>::eof(docs->value()));
      ASSERT_TRUE(irs::type_limits<irs::type_t::doc_id_t>::eof(docs_seek->seek(irs::type_limits<irs::type_t::doc_id_t>::eof())));
    }

    // "as in % past we ___ looking forward"
    {
      irs::bytes_ref actual_value;

      irs::by_phrase q;
      irs::by_phrase::levenshtein_term lt;
      lt.max_distance = 2;
      lt.term = irs::ref_cast<irs::byte_type>(irs::string_ref("ass"));
      irs::by_phrase::wildcard_term wt1;
      wt1.term = irs::ref_cast<irs::byte_type>(irs::string_ref("%"));
      irs::by_phrase::wildcard_term wt2;
      wt2.term = irs::ref_cast<irs::byte_type>(irs::string_ref("___"));
      irs::by_phrase::prefix_term pt;
      pt.term = irs::ref_cast<irs::byte_type>(irs::string_ref("fo"));
      q.field("phrase_anl")
       .push_back(std::move(lt))
       .push_back(irs::by_phrase::simple_term{irs::ref_cast<irs::byte_type>(irs::string_ref("in"))})
       .push_back(std::move(wt1))
       .push_back(irs::by_phrase::simple_term{irs::ref_cast<irs::byte_type>(irs::string_ref("past"))})
       .push_back(irs::by_phrase::simple_term{irs::ref_cast<irs::byte_type>(irs::string_ref("we"))})
       .push_back(std::move(wt2))
       .push_back(irs::by_phrase::set_term{{irs::ref_cast<irs::byte_type>(irs::string_ref("looking")), irs::ref_cast<irs::byte_type>(irs::string_ref("searching"))}})
       .push_back(std::move(pt));

      auto prepared = q.prepare(rdr);
      auto sub = rdr.begin();
      auto column = sub->column_reader("name");
      ASSERT_NE(nullptr, column);
      auto values = column->values();
      auto docs = prepared->execute(*sub);
      auto& doc = docs->attributes().get<irs::document>();
      ASSERT_TRUE(bool(doc));
      ASSERT_EQ(docs->value(), doc->value);
      ASSERT_FALSE(irs::type_limits<irs::type_t::doc_id_t>::valid(docs->value()));
      auto docs_seek = prepared->execute(*sub);
      ASSERT_FALSE(irs::type_limits<irs::type_t::doc_id_t>::valid(docs_seek->value()));

      ASSERT_TRUE(docs->next());
      ASSERT_TRUE(values(docs->value(), actual_value));
      ASSERT_EQ("H", irs::to_string<irs::string_ref>(actual_value.c_str()));
      ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
      ASSERT_TRUE(values(docs->value(), actual_value));
      ASSERT_EQ("H", irs::to_string<irs::string_ref>(actual_value.c_str()));

      ASSERT_FALSE(docs->next());
      ASSERT_TRUE(irs::type_limits<irs::type_t::doc_id_t>::eof(docs->value()));
      ASSERT_TRUE(irs::type_limits<irs::type_t::doc_id_t>::eof(docs_seek->seek(irs::type_limits<irs::type_t::doc_id_t>::eof())));
    }

    // "as in the past we are looking forward" with order
    {
      irs::bytes_ref actual_value;

      irs::by_phrase q;
      q.field("phrase_anl")
       .push_back(irs::by_phrase::simple_term{irs::ref_cast<irs::byte_type>(irs::string_ref("as"))})
       .push_back(irs::by_phrase::simple_term{irs::ref_cast<irs::byte_type>(irs::string_ref("in"))})
       .push_back(irs::by_phrase::simple_term{irs::ref_cast<irs::byte_type>(irs::string_ref("the"))})
       .push_back(irs::by_phrase::simple_term{irs::ref_cast<irs::byte_type>(irs::string_ref("past"))})
       .push_back(irs::by_phrase::simple_term{irs::ref_cast<irs::byte_type>(irs::string_ref("we"))})
       .push_back(irs::by_phrase::simple_term{irs::ref_cast<irs::byte_type>(irs::string_ref("are"))})
       .push_back(irs::by_phrase::simple_term{irs::ref_cast<irs::byte_type>(irs::string_ref("looking"))})
       .push_back(irs::by_phrase::simple_term{irs::ref_cast<irs::byte_type>(irs::string_ref("forward"))});

      irs::order ord;
      auto& sort = ord.add<tests::sort::custom_sort>(false);
      sort.scorer_add = [](irs::doc_id_t& dst, const irs::doc_id_t& src)->void {
        ASSERT_TRUE(
          irs::type_limits<irs::type_t::doc_id_t>::invalid() == dst
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
      auto& doc = docs->attributes().get<irs::document>();
      ASSERT_TRUE(bool(doc));
      ASSERT_EQ(docs->value(), doc->value);
      ASSERT_FALSE(irs::type_limits<irs::type_t::doc_id_t>::valid(docs->value()));
      auto docs_seek = prepared->execute(*sub);
      ASSERT_FALSE(irs::type_limits<irs::type_t::doc_id_t>::valid(docs_seek->value()));
      auto& score = docs->attributes().get<irs::score>();
      ASSERT_FALSE(!score);

      ASSERT_TRUE(docs->next());
      score->evaluate();
      ASSERT_EQ(docs->value(),pord.get<irs::doc_id_t>(score->c_str(), 0));
      ASSERT_TRUE(values(docs->value(), actual_value));
      ASSERT_EQ("H", irs::to_string<irs::string_ref>(actual_value.c_str()));
      ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
      ASSERT_TRUE(values(docs->value(), actual_value));
      ASSERT_EQ("H", irs::to_string<irs::string_ref>(actual_value.c_str()));

      ASSERT_FALSE(docs->next());
      ASSERT_TRUE(irs::type_limits<irs::type_t::doc_id_t>::eof(docs->value()));
      ASSERT_TRUE(irs::type_limits<irs::type_t::doc_id_t>::eof(docs_seek->seek(irs::type_limits<irs::type_t::doc_id_t>::eof())));
    }

    // "as in the p_st we are look* forward" with order
    {
      irs::bytes_ref actual_value;

      irs::by_phrase q;
      irs::by_phrase::wildcard_term wt;
      wt.term = irs::ref_cast<irs::byte_type>(irs::string_ref("p_st"));
      irs::by_phrase::prefix_term pt;
      pt.term = irs::ref_cast<irs::byte_type>(irs::string_ref("look"));
      q.field("phrase_anl")
       .push_back(irs::by_phrase::simple_term{irs::ref_cast<irs::byte_type>(irs::string_ref("as"))})
       .push_back(irs::by_phrase::simple_term{irs::ref_cast<irs::byte_type>(irs::string_ref("in"))})
       .push_back(irs::by_phrase::simple_term{irs::ref_cast<irs::byte_type>(irs::string_ref("the"))})
       .push_back(std::move(wt))
       .push_back(irs::by_phrase::simple_term{irs::ref_cast<irs::byte_type>(irs::string_ref("we"))})
       .push_back(irs::by_phrase::simple_term{irs::ref_cast<irs::byte_type>(irs::string_ref("are"))})
       .push_back(std::move(pt))
       .push_back(irs::by_phrase::simple_term{irs::ref_cast<irs::byte_type>(irs::string_ref("forward"))});

      irs::order ord;
      auto& sort = ord.add<tests::sort::custom_sort>(false);
      sort.scorer_add = [](irs::doc_id_t& dst, const irs::doc_id_t& src)->void {
        ASSERT_TRUE(
          irs::type_limits<irs::type_t::doc_id_t>::invalid() == dst
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
      auto& doc = docs->attributes().get<irs::document>();
      ASSERT_TRUE(bool(doc));
      ASSERT_EQ(docs->value(), doc->value);
      ASSERT_FALSE(irs::type_limits<irs::type_t::doc_id_t>::valid(docs->value()));
      auto docs_seek = prepared->execute(*sub);
      ASSERT_FALSE(irs::type_limits<irs::type_t::doc_id_t>::valid(docs_seek->value()));
      auto& score = docs->attributes().get<irs::score>();
      ASSERT_FALSE(!score);

      ASSERT_TRUE(docs->next());
      score->evaluate();
      ASSERT_EQ(docs->value(),pord.get<irs::doc_id_t>(score->c_str(), 0));
      ASSERT_TRUE(values(docs->value(), actual_value));
      ASSERT_EQ("H", irs::to_string<irs::string_ref>(actual_value.c_str()));
      ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
      ASSERT_TRUE(values(docs->value(), actual_value));
      ASSERT_EQ("H", irs::to_string<irs::string_ref>(actual_value.c_str()));

      ASSERT_FALSE(docs->next());
      ASSERT_TRUE(irs::type_limits<irs::type_t::doc_id_t>::eof(docs->value()));
      ASSERT_TRUE(irs::type_limits<irs::type_t::doc_id_t>::eof(docs_seek->seek(irs::type_limits<irs::type_t::doc_id_t>::eof())));
    }

    // fox quick
    {
      irs::bytes_ref actual_value;

      irs::by_phrase q;
      q.field("phrase_anl")
       .push_back(irs::by_phrase::simple_term{irs::ref_cast<irs::byte_type>(irs::string_ref("fox"))})
       .push_back(irs::by_phrase::simple_term{irs::ref_cast<irs::byte_type>(irs::string_ref("quick"))});

      auto prepared = q.prepare(rdr);

      auto sub = rdr.begin();
      auto column = sub->column_reader("name");
      ASSERT_NE(nullptr, column);
      auto values = column->values();
      auto docs = prepared->execute(*sub);
      auto& doc = docs->attributes().get<irs::document>();
      ASSERT_TRUE(bool(doc));
      ASSERT_EQ(docs->value(), doc->value);
      ASSERT_FALSE(irs::type_limits<irs::type_t::doc_id_t>::valid(docs->value()));
      auto docs_seek = prepared->execute(*sub);
      ASSERT_FALSE(irs::type_limits<irs::type_t::doc_id_t>::valid(docs_seek->value()));

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
      ASSERT_TRUE(irs::type_limits<irs::type_t::doc_id_t>::eof(docs->value()));
      ASSERT_TRUE(irs::type_limits<irs::type_t::doc_id_t>::eof(docs_seek->seek(irs::type_limits<irs::type_t::doc_id_t>::eof())));
    }

    // fox quick with order
    {
      irs::bytes_ref actual_value;

      irs::order ord;
      auto& sort = ord.add<tests::sort::custom_sort>(false);
      sort.scorer_add = [](irs::doc_id_t& dst, const irs::doc_id_t& src)->void {
        ASSERT_TRUE(
          irs::type_limits<irs::type_t::doc_id_t>::invalid() == dst
          || dst == src
        );
        dst = src;
      };
      auto pord = ord.prepare();

      irs::by_phrase q;
      q.field("phrase_anl")
       .push_back(irs::by_phrase::simple_term{irs::ref_cast<irs::byte_type>(irs::string_ref("fox"))})
       .push_back(irs::by_phrase::simple_term{irs::ref_cast<irs::byte_type>(irs::string_ref("quick"))});

      auto prepared = q.prepare(rdr, pord);

      auto sub = rdr.begin();
      auto column = sub->column_reader("name");
      ASSERT_NE(nullptr, column);
      auto values = column->values();
      auto docs = prepared->execute(*sub, pord);
      auto& doc = docs->attributes().get<irs::document>();
      ASSERT_TRUE(bool(doc));
      ASSERT_EQ(docs->value(), doc->value);
      ASSERT_FALSE(irs::type_limits<irs::type_t::doc_id_t>::valid(docs->value()));
      auto docs_seek = prepared->execute(*sub, pord);
      ASSERT_FALSE(irs::type_limits<irs::type_t::doc_id_t>::valid(docs_seek->value()));

      ASSERT_TRUE(docs->next());
      ASSERT_TRUE(values(docs->value(), actual_value));
      ASSERT_EQ("N", irs::to_string<irs::string_ref>(actual_value.c_str()));
      ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
      ASSERT_TRUE(values(docs->value(), actual_value));
      ASSERT_EQ("N", irs::to_string<irs::string_ref>(actual_value.c_str()));

      ASSERT_FALSE(docs->next());
      ASSERT_TRUE(irs::type_limits<irs::type_t::doc_id_t>::eof(docs->value()));
      ASSERT_TRUE(irs::type_limits<irs::type_t::doc_id_t>::eof(docs_seek->seek(irs::type_limits<irs::type_t::doc_id_t>::eof())));
    }

    // wildcard_filter "zo\\_%"
    {
      irs::bytes_ref actual_value;

      irs::by_phrase q;
      irs::by_phrase::wildcard_term wt;
      wt.term = irs::ref_cast<irs::byte_type>(irs::string_ref("zo\\_%"));
      q.field("phrase_anl").push_back(std::move(wt));

      auto prepared = q.prepare(rdr);
      auto sub = rdr.begin();
      auto column = sub->column_reader("name");
      ASSERT_NE(nullptr, column);
      auto values = column->values();

      auto docs = prepared->execute(*sub);
      auto& doc = docs->attributes().get<irs::document>();
      ASSERT_TRUE(bool(doc));
      ASSERT_EQ(docs->value(), doc->value);
      ASSERT_FALSE(irs::type_limits<irs::type_t::doc_id_t>::valid(docs->value()));
      auto docs_seek = prepared->execute(*sub);
      ASSERT_FALSE(irs::type_limits<irs::type_t::doc_id_t>::valid(docs_seek->value()));

      ASSERT_TRUE(docs->next());
      ASSERT_EQ(docs->value(), doc->value);
      ASSERT_TRUE(values(docs->value(), actual_value));
      ASSERT_EQ("PHW0", irs::to_string<irs::string_ref>(actual_value.c_str()));
      ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
      ASSERT_TRUE(values(docs->value(), actual_value));
      ASSERT_EQ("PHW0", irs::to_string<irs::string_ref>(actual_value.c_str()));

      ASSERT_FALSE(docs->next());
      ASSERT_EQ(docs->value(), doc->value);
      ASSERT_TRUE(irs::type_limits<irs::type_t::doc_id_t>::eof(docs->value()));
    }

    // wildcard_filter "\\_oo"
    {
      irs::bytes_ref actual_value;

      irs::by_phrase q;
      irs::by_phrase::wildcard_term wt;
      wt.term = irs::ref_cast<irs::byte_type>(irs::string_ref("\\_oo"));
      q.field("phrase_anl").push_back(std::move(wt));

      auto prepared = q.prepare(rdr);
      auto sub = rdr.begin();
      auto column = sub->column_reader("name");
      ASSERT_NE(nullptr, column);
      auto values = column->values();

      auto docs = prepared->execute(*sub);
      auto& doc = docs->attributes().get<irs::document>();
      ASSERT_TRUE(bool(doc));
      ASSERT_EQ(docs->value(), doc->value);
      ASSERT_FALSE(irs::type_limits<irs::type_t::doc_id_t>::valid(docs->value()));
      auto docs_seek = prepared->execute(*sub);
      ASSERT_FALSE(irs::type_limits<irs::type_t::doc_id_t>::valid(docs_seek->value()));

      ASSERT_TRUE(docs->next());
      ASSERT_EQ(docs->value(), doc->value);
      ASSERT_TRUE(values(docs->value(), actual_value));
      ASSERT_EQ("PHW1", irs::to_string<irs::string_ref>(actual_value.c_str()));
      ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
      ASSERT_TRUE(values(docs->value(), actual_value));
      ASSERT_EQ("PHW1", irs::to_string<irs::string_ref>(actual_value.c_str()));

      ASSERT_FALSE(docs->next());
      ASSERT_EQ(docs->value(), doc->value);
      ASSERT_TRUE(irs::type_limits<irs::type_t::doc_id_t>::eof(docs->value()));
    }

    // wildcard_filter "z\\_o"
    {
      irs::bytes_ref actual_value;

      irs::by_phrase q;
      irs::by_phrase::wildcard_term wt;
      wt.term = irs::ref_cast<irs::byte_type>(irs::string_ref("z\\_o"));
      q.field("phrase_anl").push_back(std::move(wt));

      auto prepared = q.prepare(rdr);
      auto sub = rdr.begin();
      auto column = sub->column_reader("name");
      ASSERT_NE(nullptr, column);
      auto values = column->values();

      auto docs = prepared->execute(*sub);
      auto& doc = docs->attributes().get<irs::document>();
      ASSERT_TRUE(bool(doc));
      ASSERT_EQ(docs->value(), doc->value);
      ASSERT_FALSE(irs::type_limits<irs::type_t::doc_id_t>::valid(docs->value()));
      auto docs_seek = prepared->execute(*sub);
      ASSERT_FALSE(irs::type_limits<irs::type_t::doc_id_t>::valid(docs_seek->value()));

      ASSERT_TRUE(docs->next());
      ASSERT_EQ(docs->value(), doc->value);
      ASSERT_TRUE(values(docs->value(), actual_value));
      ASSERT_EQ("PHW2", irs::to_string<irs::string_ref>(actual_value.c_str()));
      ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
      ASSERT_TRUE(values(docs->value(), actual_value));
      ASSERT_EQ("PHW2", irs::to_string<irs::string_ref>(actual_value.c_str()));

      ASSERT_FALSE(docs->next());
      ASSERT_EQ(docs->value(), doc->value);
      ASSERT_TRUE(irs::type_limits<irs::type_t::doc_id_t>::eof(docs->value()));
    }

    // wildcard_filter "elephant giraff\\_%"
    {
      irs::bytes_ref actual_value;

      irs::by_phrase q;
      irs::by_phrase::wildcard_term wt;
      wt.term = irs::ref_cast<irs::byte_type>(irs::string_ref("giraff\\_%"));
      q.field("phrase_anl").push_back(irs::by_phrase::simple_term{irs::ref_cast<irs::byte_type>(irs::string_ref("elephant"))})
       .push_back(std::move(wt));

      auto prepared = q.prepare(rdr);
      auto sub = rdr.begin();
      auto column = sub->column_reader("name");
      ASSERT_NE(nullptr, column);
      auto values = column->values();

      auto docs = prepared->execute(*sub);
      auto& doc = docs->attributes().get<irs::document>();
      ASSERT_TRUE(bool(doc));
      ASSERT_EQ(docs->value(), doc->value);
      ASSERT_FALSE(irs::type_limits<irs::type_t::doc_id_t>::valid(docs->value()));
      auto docs_seek = prepared->execute(*sub);
      ASSERT_FALSE(irs::type_limits<irs::type_t::doc_id_t>::valid(docs_seek->value()));

      ASSERT_TRUE(docs->next());
      ASSERT_EQ(docs->value(), doc->value);
      ASSERT_TRUE(values(docs->value(), actual_value));
      ASSERT_EQ("PHW3", irs::to_string<irs::string_ref>(actual_value.c_str()));
      ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
      ASSERT_TRUE(values(docs->value(), actual_value));
      ASSERT_EQ("PHW3", irs::to_string<irs::string_ref>(actual_value.c_str()));

      ASSERT_FALSE(docs->next());
      ASSERT_EQ(docs->value(), doc->value);
      ASSERT_TRUE(irs::type_limits<irs::type_t::doc_id_t>::eof(docs->value()));
    }

    // wildcard_filter "elephant \\_iraffe"
    {
      irs::bytes_ref actual_value;

      irs::by_phrase q;
      irs::by_phrase::wildcard_term wt;
      wt.term = irs::ref_cast<irs::byte_type>(irs::string_ref("\\_iraffe"));
      q.field("phrase_anl").push_back(irs::by_phrase::simple_term{irs::ref_cast<irs::byte_type>(irs::string_ref("elephant"))})
       .push_back(std::move(wt));

      auto prepared = q.prepare(rdr);
      auto sub = rdr.begin();
      auto column = sub->column_reader("name");
      ASSERT_NE(nullptr, column);
      auto values = column->values();

      auto docs = prepared->execute(*sub);
      auto& doc = docs->attributes().get<irs::document>();
      ASSERT_TRUE(bool(doc));
      ASSERT_EQ(docs->value(), doc->value);
      ASSERT_FALSE(irs::type_limits<irs::type_t::doc_id_t>::valid(docs->value()));
      auto docs_seek = prepared->execute(*sub);
      ASSERT_FALSE(irs::type_limits<irs::type_t::doc_id_t>::valid(docs_seek->value()));

      ASSERT_TRUE(docs->next());
      ASSERT_EQ(docs->value(), doc->value);
      ASSERT_TRUE(values(docs->value(), actual_value));
      ASSERT_EQ("PHW4", irs::to_string<irs::string_ref>(actual_value.c_str()));
      ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
      ASSERT_TRUE(values(docs->value(), actual_value));
      ASSERT_EQ("PHW4", irs::to_string<irs::string_ref>(actual_value.c_str()));

      ASSERT_FALSE(docs->next());
      ASSERT_EQ(docs->value(), doc->value);
      ASSERT_TRUE(irs::type_limits<irs::type_t::doc_id_t>::eof(docs->value()));
    }

    // wildcard_filter "elephant gira\\_fe"
    {
      irs::bytes_ref actual_value;

      irs::by_phrase q;
      irs::by_phrase::wildcard_term wt;
      wt.term = irs::ref_cast<irs::byte_type>(irs::string_ref("gira\\_fe"));
      q.field("phrase_anl").push_back(irs::by_phrase::simple_term{irs::ref_cast<irs::byte_type>(irs::string_ref("elephant"))})
       .push_back(std::move(wt));

      auto prepared = q.prepare(rdr);
      auto sub = rdr.begin();
      auto column = sub->column_reader("name");
      ASSERT_NE(nullptr, column);
      auto values = column->values();

      auto docs = prepared->execute(*sub);
      auto& doc = docs->attributes().get<irs::document>();
      ASSERT_TRUE(bool(doc));
      ASSERT_EQ(docs->value(), doc->value);
      ASSERT_FALSE(irs::type_limits<irs::type_t::doc_id_t>::valid(docs->value()));
      auto docs_seek = prepared->execute(*sub);
      ASSERT_FALSE(irs::type_limits<irs::type_t::doc_id_t>::valid(docs_seek->value()));

      ASSERT_TRUE(docs->next());
      ASSERT_EQ(docs->value(), doc->value);
      ASSERT_TRUE(values(docs->value(), actual_value));
      ASSERT_EQ("PHW5", irs::to_string<irs::string_ref>(actual_value.c_str()));
      ASSERT_EQ(docs->value(), docs_seek->seek(docs->value()));
      ASSERT_TRUE(values(docs->value(), actual_value));
      ASSERT_EQ("PHW5", irs::to_string<irs::string_ref>(actual_value.c_str()));

      ASSERT_FALSE(docs->next());
      ASSERT_EQ(docs->value(), doc->value);
      ASSERT_TRUE(irs::type_limits<irs::type_t::doc_id_t>::eof(docs->value()));
    }
  }
}; // phrase_filter_test_case

TEST_P(phrase_filter_test_case, by_phrase) {
  sequential();
}

TEST(by_phrase_test, ctor) {
  irs::by_phrase q;
  ASSERT_EQ(irs::by_phrase::type(), q.type());
  ASSERT_EQ("", q.field());
  ASSERT_TRUE(q.empty());
  ASSERT_EQ(0, q.size());
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
      q.field("field");

      auto prepared = q.prepare(irs::sub_reader::empty());
      ASSERT_EQ(irs::no_boost(), prepared->boost());
    }

    // single term
    {
      irs::by_phrase q;
      q.field("field").push_back(irs::by_phrase::simple_term{irs::ref_cast<irs::byte_type>(irs::string_ref("quick"))});

      auto prepared = q.prepare(irs::sub_reader::empty());
      ASSERT_EQ(irs::no_boost(), prepared->boost());
    }

    // multiple terms
    {
      irs::by_phrase q;
      q.field("field").push_back(irs::by_phrase::simple_term{irs::ref_cast<irs::byte_type>(irs::string_ref("quick"))})
       .push_back(irs::by_phrase::simple_term{irs::ref_cast<irs::byte_type>(irs::string_ref("brown"))});

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
      q.field("field");
      q.boost(boost);

      auto prepared = q.prepare(irs::sub_reader::empty());
      ASSERT_EQ(irs::no_boost(), prepared->boost());
    }

    // single term
    {
      irs::by_phrase q;
      q.field("field").push_back(irs::by_phrase::simple_term{irs::ref_cast<irs::byte_type>(irs::string_ref("quick"))});
      q.boost(boost);

      auto prepared = q.prepare(irs::sub_reader::empty());
      ASSERT_EQ(boost, prepared->boost());
    }
    
    // single multiple terms 
    {
      irs::by_phrase q;
      q.field("field").push_back(irs::by_phrase::simple_term{irs::ref_cast<irs::byte_type>(irs::string_ref("quick"))})
       .push_back(irs::by_phrase::simple_term{irs::ref_cast<irs::byte_type>(irs::string_ref("brown"))});
      q.boost(boost);

      auto prepared = q.prepare(irs::sub_reader::empty());
      ASSERT_EQ(boost, prepared->boost());
    }

    // prefix, wildcard, and levenshtein terms
    {
      irs::by_phrase q;
      irs::by_phrase::prefix_term pt;
      pt.term = irs::ref_cast<irs::byte_type>(irs::string_ref("qui"));
      irs::by_phrase::wildcard_term wt;
      wt.term = irs::ref_cast<irs::byte_type>(irs::string_ref("qu__k"));
      irs::by_phrase::levenshtein_term lt;
      lt.max_distance = 1;
      lt.term = irs::ref_cast<irs::byte_type>(irs::string_ref("brwn"));
      q.field("field").push_back(std::move(pt)).push_back(std::move(wt)).push_back(lt);
      q.boost(boost);

      auto prepared = q.prepare(irs::sub_reader::empty());
      ASSERT_EQ(boost, prepared->boost());
    }
  }
}

TEST(by_phrase_test, push_back_insert) {
  irs::by_phrase q;

  // push_back 
  {
    q.push_back(irs::by_phrase::simple_term{irs::ref_cast<irs::byte_type>(irs::string_ref("quick"))});
    q.push_back(irs::by_phrase::simple_term{irs::ref_cast<irs::byte_type>(irs::string_ref("brown"))}, 1);
    q.push_back(irs::by_phrase::simple_term{irs::ref_cast<irs::byte_type>(irs::string_ref("fox"))});
    ASSERT_FALSE(q.empty());
    ASSERT_EQ(3, q.size());

    // check elements via positions
    {
      const irs::by_phrase::simple_term* st1 = q.get<irs::by_phrase::simple_term>(0);
      ASSERT_TRUE(st1);
      ASSERT_EQ(irs::by_phrase::simple_term{irs::ref_cast<irs::byte_type>(irs::string_ref("quick"))}, *st1);
      const irs::by_phrase::simple_term* st2 = q.get<irs::by_phrase::simple_term>(2);
      ASSERT_TRUE(st2);
      ASSERT_EQ(irs::by_phrase::simple_term{irs::ref_cast<irs::byte_type>(irs::string_ref("brown"))}, *st2);
      const irs::by_phrase::simple_term* st3 = q.get<irs::by_phrase::simple_term>(3);
      ASSERT_TRUE(st3);
      ASSERT_EQ(irs::by_phrase::simple_term{irs::ref_cast<irs::byte_type>(irs::string_ref("fox"))}, *st3);
    }

    // push term 
    {
      irs::by_phrase::simple_term st1{irs::ref_cast<irs::byte_type>(irs::string_ref("squirrel"))};
      q.push_back(st1, 0);
      const irs::by_phrase::simple_term* st2 = q.get<irs::by_phrase::simple_term>(4);
      ASSERT_TRUE(st2);
      ASSERT_EQ(st1, *st2);

      irs::by_phrase::prefix_term pt1;
      pt1.term = irs::ref_cast<irs::byte_type>(irs::string_ref("cat"));
      q.push_back(pt1, 0);
      const irs::by_phrase::prefix_term* pt2 = q.get<irs::by_phrase::prefix_term>(5);
      ASSERT_TRUE(pt2);
      ASSERT_EQ(pt1, *pt2);

      irs::by_phrase::wildcard_term wt1;
      wt1.term = irs::ref_cast<irs::byte_type>(irs::string_ref("dog"));
      q.push_back(wt1, 0);
      const irs::by_phrase::wildcard_term* wt2 = q.get<irs::by_phrase::wildcard_term>(6);
      ASSERT_TRUE(wt2);
      ASSERT_EQ(wt1, *wt2);

      irs::by_phrase::levenshtein_term lt1;
      lt1.term = irs::ref_cast<irs::byte_type>(irs::string_ref("whale"));
      q.push_back(lt1, 0);
      const irs::by_phrase::levenshtein_term* lt2 = q.get<irs::by_phrase::levenshtein_term>(7);
      ASSERT_TRUE(lt2);
      ASSERT_EQ(lt1, *lt2);

      irs::by_phrase::set_term ct1;
      ct1.terms = {irs::ref_cast<irs::byte_type>(irs::string_ref("bird"))};
      q.push_back(ct1, 0);
      const irs::by_phrase::set_term* ct2 = q.get<irs::by_phrase::set_term>(8);
      ASSERT_TRUE(ct2);
      ASSERT_EQ(ct1, *ct2);
    }
    ASSERT_EQ(8, q.size());
  }

  // insert
  {
    q.insert(irs::by_phrase::simple_term{irs::ref_cast<irs::byte_type>(irs::string_ref("jumps"))}, 3);
    const irs::by_phrase::simple_term* st1 = q.get<irs::by_phrase::simple_term>(3);
    ASSERT_TRUE(st1);
    ASSERT_EQ(irs::by_phrase::simple_term{irs::ref_cast<irs::byte_type>(irs::string_ref("jumps"))}, *st1);
    ASSERT_EQ(8, q.size());

    q.insert(irs::by_phrase::simple_term{irs::ref_cast<irs::byte_type>(irs::string_ref("lazy"))}, 9);
    const irs::by_phrase::simple_term* st2 = q.get<irs::by_phrase::simple_term>(9);
    ASSERT_TRUE(st2);
    ASSERT_EQ(irs::by_phrase::simple_term{irs::ref_cast<irs::byte_type>(irs::string_ref("lazy"))}, *st2);
    ASSERT_EQ(9, q.size());

    q.insert(irs::by_phrase::simple_term{irs::ref_cast<irs::byte_type>(irs::string_ref("dog"))}, 28);
    const irs::by_phrase::simple_term* st3 = q.get<irs::by_phrase::simple_term>(28);
    ASSERT_TRUE(st3);
    ASSERT_EQ(irs::by_phrase::simple_term{irs::ref_cast<irs::byte_type>(irs::string_ref("dog"))}, *st3);
    ASSERT_EQ(10, q.size());
  }
}

TEST(by_phrase_test, equal) {
  ASSERT_EQ(irs::by_phrase(), irs::by_phrase());

  {
    irs::by_phrase q0;
    q0.field("name");
    q0.push_back(irs::by_phrase::simple_term{irs::ref_cast<irs::byte_type>(irs::string_ref("quick"))});
    q0.push_back(irs::by_phrase::simple_term{irs::ref_cast<irs::byte_type>(irs::string_ref("brown"))});

    irs::by_phrase q1;
    q1.field("name");
    q1.push_back(irs::by_phrase::simple_term{irs::ref_cast<irs::byte_type>(irs::string_ref("quick"))});
    q1.push_back(irs::by_phrase::simple_term{irs::ref_cast<irs::byte_type>(irs::string_ref("brown"))});
    ASSERT_EQ(q0, q1);
    ASSERT_EQ(q0.hash(), q1.hash());
  }

  {
    irs::by_phrase q0;
    irs::by_phrase::prefix_term pt1;
    pt1.term = irs::ref_cast<irs::byte_type>(irs::string_ref("qui"));
    irs::by_phrase::set_term ct1;
    ct1.terms = {irs::ref_cast<irs::byte_type>(irs::string_ref("light")),
                irs::ref_cast<irs::byte_type>(irs::string_ref("dark"))};
    irs::by_phrase::wildcard_term wt1;
    wt1.term = irs::ref_cast<irs::byte_type>(irs::string_ref("br_wn"));
    irs::by_phrase::levenshtein_term lt1;
    lt1.max_distance = 2;
    lt1.term = irs::ref_cast<irs::byte_type>(irs::string_ref("fo"));
    q0.field("name");
    q0.push_back(std::move(pt1));
    q0.push_back(std::move(ct1));
    q0.push_back(std::move(wt1));
    q0.push_back(std::move(lt1));

    irs::by_phrase q1;
    irs::by_phrase::prefix_term pt2;
    pt2.term = irs::ref_cast<irs::byte_type>(irs::string_ref("qui"));
    irs::by_phrase::set_term ct2;
    ct2.terms = {irs::ref_cast<irs::byte_type>(irs::string_ref("light")), irs::ref_cast<irs::byte_type>(irs::string_ref("dark"))};
    irs::by_phrase::wildcard_term wt2;
    wt2.term = irs::ref_cast<irs::byte_type>(irs::string_ref("br_wn"));
    irs::by_phrase::levenshtein_term lt2;
    lt2.max_distance = 2;
    lt2.term = irs::ref_cast<irs::byte_type>(irs::string_ref("fo"));
    q1.field("name");
    q1.push_back(std::move(pt2));
    q1.push_back(std::move(ct2));
    q1.push_back(std::move(wt2));
    q1.push_back(std::move(lt2));
    ASSERT_EQ(q0, q1);
    ASSERT_EQ(q0.hash(), q1.hash());
  }

  {
    irs::by_phrase q0;
    q0.field("name");
    q0.push_back(irs::by_phrase::simple_term{irs::ref_cast<irs::byte_type>(irs::string_ref("quick"))});
    q0.push_back(irs::by_phrase::simple_term{irs::ref_cast<irs::byte_type>(irs::string_ref("squirrel"))});

    irs::by_phrase q1;
    q1.field("name");
    q1.push_back(irs::by_phrase::simple_term{irs::ref_cast<irs::byte_type>(irs::string_ref("quick"))});
    q1.push_back(irs::by_phrase::simple_term{irs::ref_cast<irs::byte_type>(irs::string_ref("brown"))});
    ASSERT_NE(q0, q1);
  }

  {
    irs::by_phrase q0;
    q0.field("name1");
    q0.push_back(irs::by_phrase::simple_term{irs::ref_cast<irs::byte_type>(irs::string_ref("quick"))});
    q0.push_back(irs::by_phrase::simple_term{irs::ref_cast<irs::byte_type>(irs::string_ref("brown"))});

    irs::by_phrase q1;
    q1.field("name");
    q1.push_back(irs::by_phrase::simple_term{irs::ref_cast<irs::byte_type>(irs::string_ref("quick"))});
    q1.push_back(irs::by_phrase::simple_term{irs::ref_cast<irs::byte_type>(irs::string_ref("brown"))});
    ASSERT_NE(q0, q1);
  }

  {
    irs::by_phrase q0;
    q0.field("name");
    q0.push_back(irs::by_phrase::simple_term{irs::ref_cast<irs::byte_type>(irs::string_ref("quick"))});

    irs::by_phrase q1;
    q1.field("name");
    q1.push_back(irs::by_phrase::simple_term{irs::ref_cast<irs::byte_type>(irs::string_ref("quick"))});
    q1.push_back(irs::by_phrase::simple_term{irs::ref_cast<irs::byte_type>(irs::string_ref("brown"))});
    ASSERT_NE(q0, q1);
  }

  {
    irs::by_phrase q0;
    irs::by_phrase::prefix_term pt1;
    pt1.term = irs::ref_cast<irs::byte_type>(irs::string_ref("quil"));
    irs::by_phrase::set_term ct1;
    ct1.terms = {irs::ref_cast<irs::byte_type>(irs::string_ref("light")),
                irs::ref_cast<irs::byte_type>(irs::string_ref("dark"))};
    irs::by_phrase::wildcard_term wt1;
    wt1.term = irs::ref_cast<irs::byte_type>(irs::string_ref("br_wn"));
    irs::by_phrase::levenshtein_term lt1;
    lt1.max_distance = 2;
    lt1.term = irs::ref_cast<irs::byte_type>(irs::string_ref("fo"));
    q0.field("name");
    q0.push_back(std::move(pt1));
    q0.push_back(std::move(ct1));
    q0.push_back(std::move(wt1));
    q0.push_back(std::move(lt1));

    irs::by_phrase q1;
    irs::by_phrase::prefix_term pt2;
    pt2.term = irs::ref_cast<irs::byte_type>(irs::string_ref("qui"));
    irs::by_phrase::set_term ct2;
    ct2.terms = {irs::ref_cast<irs::byte_type>(irs::string_ref("light")), irs::ref_cast<irs::byte_type>(irs::string_ref("dark"))};
    irs::by_phrase::wildcard_term wt2;
    wt2.term = irs::ref_cast<irs::byte_type>(irs::string_ref("br_wn"));
    irs::by_phrase::levenshtein_term lt2;
    lt2.max_distance = 2;
    lt2.term = irs::ref_cast<irs::byte_type>(irs::string_ref("fo"));
    q1.field("name");
    q1.push_back(std::move(pt2));
    q1.push_back(std::move(ct2));
    q1.push_back(std::move(wt2));
    q1.push_back(std::move(lt2));
    ASSERT_NE(q0, q1);
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
    ::testing::Values("1_0", "1_3")
  ),
  tests::to_string
);

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------
