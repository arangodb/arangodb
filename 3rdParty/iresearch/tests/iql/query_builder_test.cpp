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

#include "gtest/gtest.h"
#include "tests_config.hpp"
#include "tests_shared.hpp"
#include "analysis/analyzers.hpp"
#include "analysis/token_streams.hpp"
#include "formats/formats_10.hpp"
#include "store/memory_directory.hpp"
#include "index/doc_generator.hpp"
#include "index/index_reader.hpp"
#include "index/index_writer.hpp"
#include "index/index_tests.hpp"
#include "iql/query_builder.hpp"
#include "search/boolean_filter.hpp"
#include "search/scorers.hpp"
#include "search/term_filter.hpp"
#include "utils/runtime_utils.hpp"

namespace tests {
  class test_sort: public iresearch::sort {
   public:
    DECLARE_FACTORY();

    class prepared : public sort::prepared {
     public:
      prepared() { }
      virtual void collect(
        irs::byte_type*,
        const irs::index_reader&,
        const irs::sort::field_collector*,
        const irs::sort::term_collector*) const override {
        // do not need to collect stats
      }
      virtual irs::sort::field_collector::ptr prepare_field_collector() const override {
        return nullptr; // do not need to collect stats
      }
      virtual irs::score_function prepare_scorer(
          const iresearch::sub_reader&,
          const iresearch::term_reader&,
          const irs::byte_type*,
          irs::byte_type*,
          const irs::attribute_provider&,
          irs::boost_t) const override {
        return { nullptr, nullptr };
      }
      virtual irs::sort::term_collector::ptr prepare_term_collector() const override {
        return nullptr; // do not need to collect stats
      }
      virtual const iresearch::flags& features() const override { 
        return iresearch::flags::empty_instance();
      }
      virtual bool less(const iresearch::byte_type*, const iresearch::byte_type*) const override {
        throw std::bad_function_call();
      }
      std::pair<size_t, size_t> score_size() const override {
        return std::make_pair(size_t(0), size_t(0));
      }
      std::pair<size_t, size_t> stats_size() const override {
        return std::make_pair(size_t(0), size_t(0));
      }
    };

    test_sort():sort(irs::type<test_sort>::get()) {}
    virtual sort::prepared::ptr prepare() const { return std::make_unique<test_sort::prepared>(); }
  };

  DEFINE_FACTORY_DEFAULT(test_sort)

  class IqlQueryBuilderTestSuite: public ::testing::Test {
    virtual void SetUp() {
      // Code here will be called immediately after the constructor (right before each test).
      // use the following code to enble parser debug outut
      //::iresearch::iql::debug(parser, [true|false]);

      // ensure stopwords are loaded/cached for the 'en' locale used for text analysis below
      {
        // same env variable name as iresearch::analysis::text_token_stream::STOPWORD_PATH_ENV_VARIABLE
        const auto text_stopword_path_var = "IRESEARCH_TEXT_STOPWORD_PATH";
        const char* czOldStopwordPath = iresearch::getenv(text_stopword_path_var);
        std::string sOldStopwordPath = czOldStopwordPath == nullptr ? "" : czOldStopwordPath;

        iresearch::setenv(text_stopword_path_var, IResearch_test_resource_dir, true);

        auto locale = irs::locale_utils::locale("en");
        const std::string tmp_str;

        irs::analysis::analyzers::get("text", irs::type<irs::text_format::text>::get(), "en"); // stream needed only to load stopwords

        if (czOldStopwordPath) {
          iresearch::setenv(text_stopword_path_var, sOldStopwordPath.c_str(), true);
        }
      }
    }

    virtual void TearDown() {
      // Code here will be called immediately after each test (right before the destructor).
    }
  };

  class analyzed_string_field: public templates::string_field {
   public:
    analyzed_string_field(const iresearch::string_ref& name, const iresearch::string_ref& value)
      : templates::string_field(name, value),
        token_stream_(irs::analysis::analyzers::get("text", irs::type<irs::text_format::text>::get(), "en")) {
      if (!token_stream_) {
        throw std::runtime_error("Failed to get 'text' analyzer for args: en");
      }
    }
    virtual ~analyzed_string_field() {}
    virtual iresearch::token_stream& get_tokens() const override {
      const iresearch::string_ref& value = this->value();
      token_stream_->reset(value);
      return *token_stream_;
    }
      
   private:
    static std::unordered_set<std::string> ignore_set_; // there are no stopwords for the 'c' locale in tests
    iresearch::analysis::analyzer::ptr token_stream_;
  };

  std::unordered_set<std::string> analyzed_string_field::ignore_set_;

  iresearch::directory_reader load_json(
    iresearch::directory& dir,
    const std::string json_resource,
    bool analyze_text = false
  ) {
    static auto analyzed_field_factory = [](
        tests::document& doc,
        const std::string& name,
        const tests::json_doc_generator::json_value& value) {
      if (value.is_string()) {
        doc.insert(std::make_shared<analyzed_string_field>(
          iresearch::string_ref(name),
          value.str
        ));
      } else if (value.is_null()) {
        doc.insert(std::make_shared<analyzed_string_field>(
          iresearch::string_ref(name),
          "null"
        ));
      } else if (value.is_bool() && value.b) {
        doc.insert(std::make_shared<analyzed_string_field>(
          iresearch::string_ref(name),
          "true"
        ));
      } else if (value.is_bool() && !value.b) {
        doc.insert(std::make_shared<analyzed_string_field>(
          iresearch::string_ref(name),
          "false"
        ));
      } else if (value.is_number()) {
        const auto str = std::to_string(value.as_number<uint64_t>());
        doc.insert(std::make_shared<analyzed_string_field>(
          iresearch::string_ref(name),
          str
        ));
      }
    };

    static auto generic_field_factory = [](
        tests::document& doc,
        const std::string& name,
        const tests::json_doc_generator::json_value& data) {
      if (data.is_string()) {
        doc.insert(std::make_shared<templates::string_field>(
          irs::string_ref(name),
          data.str
        ));
      } else if (data.is_null()) {
        doc.insert(std::make_shared<tests::binary_field>());
        auto& field = (doc.indexed.end() - 1).as<tests::binary_field>();
        field.name(iresearch::string_ref(name));
        field.value(irs::ref_cast<irs::byte_type>(irs::null_token_stream::value_null()));
      } else if (data.is_bool() && data.b) {
        doc.insert(std::make_shared<tests::binary_field>());
        auto& field = (doc.indexed.end() - 1).as<tests::binary_field>();
        field.name(iresearch::string_ref(name));
        field.value(irs::ref_cast<irs::byte_type>(irs::boolean_token_stream::value_true()));
      } else if (data.is_bool() && !data.b) {
        doc.insert(std::make_shared<tests::binary_field>());
        auto& field = (doc.indexed.end() - 1).as<tests::binary_field>();
        field.name(iresearch::string_ref(name));
        field.value(irs::ref_cast<irs::byte_type>(irs::boolean_token_stream::value_true()));
      } else if (data.is_number()) {
        const double dValue = data.as_number<double_t>();

        // 'value' can be interpreted as a double
        doc.insert(std::make_shared<tests::double_field>());
        auto& field = (doc.indexed.end() - 1).as<tests::double_field>();
        field.name(iresearch::string_ref(name));
        field.value(dValue);
      }
    };

    auto codec_ptr = irs::formats::get("1_0");

    auto writer =
      iresearch::index_writer::make(dir, codec_ptr, iresearch::OM_CREATE);
    json_doc_generator generator(
      test_base::resource(json_resource), 
      analyze_text ? analyzed_field_factory : &tests::generic_json_field_factory);
    const document* doc;

    while ((doc = generator.next()) != nullptr) {
      insert(*writer,
        doc->indexed.begin(), doc->indexed.end(),
        doc->stored.begin(), doc->stored.end()
      );
    }

    writer->commit();

    return iresearch::directory_reader::open(dir, codec_ptr);
  }
}

using namespace tests;
using namespace iresearch::iql;

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

TEST_F(IqlQueryBuilderTestSuite, test_query_builder) {
  sequence_function::contextual_function_t fnNum = [](
    sequence_function::contextual_buffer_t& buf,
    const std::locale& locale,
    void* cookie,
    const sequence_function::contextual_function_args_t& args
  )->bool {
    iresearch::bstring value;
    bool bValue;

    args[0].value(value, bValue, locale, cookie);

    double dValue = strtod(iresearch::ref_cast<char>(value).c_str(), nullptr);
    iresearch::numeric_token_stream stream;
    stream.reset((double_t)dValue);
    auto* term = irs::get<irs::term_attribute>(stream);

    while (stream.next()) {
      buf.append(term->value);
    }

    return true;
  };
  iresearch::memory_directory dir;
  auto reader = load_json(dir, "simple_sequential.json");
  ASSERT_EQ(1, reader.size());

  auto& segment = reader[0]; // assume 0 is id of first/only segment

  // single string term
  {
    irs::bytes_ref actual_value;
    auto column = segment.column_reader("name");
    ASSERT_NE(nullptr, column);
    auto values = column->values();

    auto query = query_builder().build("name==A", std::locale::classic());
    ASSERT_NE(nullptr, query.filter.get());
    ASSERT_EQ(nullptr, query.error);
    ASSERT_EQ(nullptr, query.limit);

    auto pQuery = query.filter->prepare(reader, iresearch::order::prepared::unordered());
    ASSERT_NE(nullptr, pQuery.get());

    auto docsItr = pQuery->execute(segment);
    ASSERT_NE(nullptr, docsItr.get());
    ASSERT_TRUE(docsItr->next());

    ASSERT_TRUE(values(docsItr->value(), actual_value));
    ASSERT_EQ("A", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_FALSE(docsItr->next());
  }

/* FIXME reenable once bug is fixed
  // single numeric term
  {
    sequence_function seq_function(fnNum, 1);
    sequence_functions seq_functions = {
      { "#", seq_function },
    };
    functions functions(seq_functions);
    auto query = query_builder(functions).build("seq==#(0)", std::locale::classic());
    ASSERT_NE(nullptr, query.filter.get());
    ASSERT_EQ(nullptr, query.error);
    ASSERT_EQ(nullptr, query.limit);

    auto pQuery = query.filter->prepare(reader);
    ASSERT_NE(nullptr, pQuery.get());

    auto docsItr = pQuery->execute(segment);
    ASSERT_NE(nullptr, docsItr.get());
    ASSERT_TRUE(docsItr->next());
    tests::field_visitor visitor("name");
    segment.document(docsItr->value(), visitor);
    ASSERT_EQ(std::string("A"), visitor.v_string);
    ASSERT_FALSE(docsItr->next());
  }
*/
  // term negation
  {
    irs::bytes_ref actual_value;
    irs::bytes_ref_input in;
    auto column = segment.column_reader("name");
    ASSERT_NE(nullptr, column);
    auto values = column->values();

    auto query = query_builder().build("name!=A", std::locale::classic());
    ASSERT_NE(nullptr, query.filter.get());
    ASSERT_EQ(nullptr, query.error);
    ASSERT_EQ(nullptr, query.limit);

    auto pQuery = query.filter->prepare(reader, iresearch::order::prepared::unordered());
    ASSERT_NE(nullptr, pQuery.get());

    auto docsItr = pQuery->execute(segment);
    ASSERT_NE(nullptr, docsItr.get());

    for (size_t count = segment.docs_count() - 1; count > 0; --count) {
      ASSERT_TRUE(docsItr->next());
      ASSERT_TRUE(values(docsItr->value(), actual_value)); in.reset(actual_value);
      ASSERT_NE("A", irs::read_string<std::string>(in));
    }

    ASSERT_FALSE(docsItr->next());
  }

  // term union
  {
    irs::bytes_ref actual_value;
    auto column = segment.column_reader("name");
    ASSERT_NE(nullptr, column);
    auto values = column->values();

    auto query = query_builder().build("name==A || name==B OR name==C", std::locale::classic());
    ASSERT_NE(nullptr, query.filter.get());
    ASSERT_EQ(nullptr, query.error);
    ASSERT_EQ(nullptr, query.limit);

    auto pQuery = query.filter->prepare(reader, iresearch::order::prepared::unordered());
    ASSERT_NE(nullptr, pQuery.get());

    auto docsItr = pQuery->execute(segment);
    ASSERT_NE(nullptr, docsItr.get());
    ASSERT_TRUE(docsItr->next());
    ASSERT_TRUE(values(docsItr->value(), actual_value));
    ASSERT_EQ("A", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_TRUE(docsItr->next());
    ASSERT_TRUE(values(docsItr->value(), actual_value));
    ASSERT_EQ("B", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_TRUE(docsItr->next());
    ASSERT_TRUE(values(docsItr->value(), actual_value));
    ASSERT_EQ("C", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_FALSE(docsItr->next());
  }
/* FIXME reenable once bug is fixed
  // term intersection
  {
    sequence_function seq_function(fnNum, 1);
    sequence_functions seq_functions = {
      { "#", seq_function },
    };
    functions functions(seq_functions);
    auto query = query_builder(functions).build("name==A && seq==#(0) AND same==xyz", std::locale::classic());
    ASSERT_NE(nullptr, query.filter.get());
    ASSERT_EQ(nullptr, query.error);
    ASSERT_EQ(nullptr, query.limit);

    auto pQuery = query.filter->prepare(reader);
    ASSERT_NE(nullptr, pQuery.get());

    auto docsItr = pQuery->execute(segment);
    ASSERT_NE(nullptr, docsItr.get());
    ASSERT_TRUE(docsItr->next());
    tests::field_visitor visitor("name");
    segment.document(docsItr->value(), visitor);
    ASSERT_EQ(std::string("A"), visitor.v_string);
    ASSERT_FALSE(docsItr->next());
  }
*/
  // single term greater ranges
  {
    irs::bytes_ref actual_value;
    irs::bytes_ref_input in;
    std::unordered_set<irs::string_ref> expected = { "N", "O", "P", "Q", "R", "S", "T", "U", "V", "W", "X", "Y", "Z" };

    double_t seq;
    auto seq_column = segment.column_reader("seq");
    ASSERT_NE(nullptr, seq_column);
    auto seq_values = seq_column->values();
    auto name_column = segment.column_reader("name");
    ASSERT_NE(nullptr, name_column);
    auto name_values = name_column->values();

    auto query = query_builder().build("name > M", std::locale::classic());
    ASSERT_NE(nullptr, query.filter.get());
    ASSERT_EQ(nullptr, query.error);
    ASSERT_EQ(nullptr, query.limit);

    auto pQuery = query.filter->prepare(reader, iresearch::order::prepared::unordered());
    ASSERT_NE(nullptr, pQuery.get());

    auto docsItr = pQuery->execute(segment);
    ASSERT_NE(nullptr, docsItr.get());

    while (docsItr->next()) {
      ASSERT_TRUE(seq_values(docsItr->value(), actual_value)); in.reset(actual_value);
      seq = irs::read_zvdouble(in);

      if (seq < 26) { // validate only first 26 records [A-Z]
        ASSERT_TRUE(name_values(docsItr->value(), actual_value)); in.reset(actual_value);
        ASSERT_EQ(1, expected.erase(irs::to_string<irs::string_ref>(actual_value.c_str())));
      }
    }

    ASSERT_TRUE(expected.empty());
  }

  // single term greater-equal ranges
  {
    irs::bytes_ref actual_value;
    irs::bytes_ref_input in;
    std::unordered_set<irs::string_ref> expected = { "M", "N", "O", "P", "Q", "R", "S", "T", "U", "V", "W", "X", "Y", "Z" };

    double_t seq;
    auto seq_column = segment.column_reader("seq");
    ASSERT_NE(nullptr, seq_column);
    auto seq_values = seq_column->values();
    auto name_column = segment.column_reader("name");
    ASSERT_NE(nullptr, name_column);
    auto name_values = name_column->values();

    auto query = query_builder().build("name >= M", std::locale::classic());
    ASSERT_NE(nullptr, query.filter.get());
    ASSERT_EQ(nullptr, query.error);
    ASSERT_EQ(nullptr, query.limit);

    auto pQuery = query.filter->prepare(reader, iresearch::order::prepared::unordered());
    ASSERT_NE(nullptr, pQuery.get());

    auto docsItr = pQuery->execute(segment);
    ASSERT_NE(nullptr, docsItr.get());

    while (docsItr->next()) {
      ASSERT_TRUE(seq_values(docsItr->value(), actual_value)); in.reset(actual_value);
      seq = irs::read_zvdouble(in);

      if (seq < 26) { // validate only first 26 records [A-Z]
        ASSERT_TRUE(name_values(docsItr->value(), actual_value)); in.reset(actual_value);
        ASSERT_EQ(1, expected.erase(irs::to_string<irs::string_ref>(actual_value.c_str())));
      }
    }

    ASSERT_TRUE(expected.empty());
  }

  // single term lesser-equal ranges
  {
    irs::bytes_ref actual_value;
    irs::bytes_ref_input in;
    std::unordered_set<irs::string_ref> expected = { "A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K", "L", "M", "N" };

    double_t seq;
    auto seq_column = segment.column_reader("seq");
    ASSERT_NE(nullptr, seq_column);
    auto seq_values = seq_column->values();
    auto name_column = segment.column_reader("name");
    ASSERT_NE(nullptr, name_column);
    auto name_values = name_column->values();

    auto query = query_builder().build("name <= N", std::locale::classic());
    ASSERT_NE(nullptr, query.filter.get());
    ASSERT_EQ(nullptr, query.error);
    ASSERT_EQ(nullptr, query.limit);

    auto pQuery = query.filter->prepare(reader, iresearch::order::prepared::unordered());
    ASSERT_NE(nullptr, pQuery.get());

    auto docsItr = pQuery->execute(segment);
    ASSERT_NE(nullptr, docsItr.get());

    while (docsItr->next()) {
      ASSERT_TRUE(seq_values(docsItr->value(), actual_value)); in.reset(actual_value);
      seq = irs::read_zvdouble(in);

      if (seq < 26) { // validate only first 26 records [A-Z]
        ASSERT_TRUE(name_values(docsItr->value(), actual_value)); in.reset(actual_value);
        ASSERT_EQ(1, expected.erase(irs::to_string<irs::string_ref>(actual_value.c_str())));
      }
    }

    ASSERT_TRUE(expected.empty());
  }

  // single term lesser ranges
  {
    irs::bytes_ref actual_value;
    irs::bytes_ref_input in;
    std::unordered_set<irs::string_ref> expected = { "A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K", "L", "M" };

    double_t seq;
    auto seq_column = segment.column_reader("seq");
    ASSERT_NE(nullptr, seq_column);
    auto seq_values = seq_column->values();
    auto name_column = segment.column_reader("name");
    ASSERT_NE(nullptr, name_column);
    auto name_values = name_column->values();

    auto query = query_builder().build("name < N", std::locale::classic());
    ASSERT_NE(nullptr, query.filter.get());
    ASSERT_EQ(nullptr, query.error);
    ASSERT_EQ(nullptr, query.limit);

    auto pQuery = query.filter->prepare(reader, iresearch::order::prepared::unordered());
    ASSERT_NE(nullptr, pQuery.get());

    auto docsItr = pQuery->execute(segment);
    ASSERT_NE(nullptr, docsItr.get());

    while (docsItr->next()) {
      ASSERT_TRUE(seq_values(docsItr->value(), actual_value)); in.reset(actual_value);
      seq = irs::read_zvdouble(in);

      if (seq < 26) { // validate only first 26 records [A-Z]
        ASSERT_TRUE(name_values(docsItr->value(), actual_value)); in.reset(actual_value);
        ASSERT_EQ(1, expected.erase(irs::to_string<irs::string_ref>(actual_value.c_str())));
      }
    }

    ASSERT_TRUE(expected.empty());
  }

  // limit
  {
    irs::bytes_ref actual_value;
    auto column = segment.column_reader("name");
    ASSERT_NE(nullptr, column);
    auto values = column->values();

    auto query = query_builder().build("name==A limit 42", std::locale::classic());
    ASSERT_NE(nullptr, query.filter.get());
    ASSERT_EQ(nullptr, query.error);
    ASSERT_NE(nullptr, query.limit);

    auto pQuery = query.filter->prepare(reader, iresearch::order::prepared::unordered());
    ASSERT_NE(nullptr, pQuery.get());

    auto docsItr = pQuery->execute(segment);
    ASSERT_NE(nullptr, docsItr.get());
    ASSERT_TRUE(docsItr->next());
    ASSERT_TRUE(values(docsItr->value(), actual_value));
    ASSERT_EQ("A", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_FALSE(docsItr->next());

    ASSERT_EQ(42, *(query.limit));
  }

  // ...........................................................................
  // invalid
  // ...........................................................................

  // invalid query
  {
    auto query = query_builder().build("name==A bcd", std::locale::classic());
    ASSERT_NE(nullptr, query.filter.get());
    ASSERT_NE(nullptr, query.error);
    ASSERT_EQ(nullptr, query.limit);

    auto pQuery = query.filter->prepare(reader, iresearch::order::prepared::unordered());
    ASSERT_EQ(nullptr, pQuery.get());

    ASSERT_EQ(0, query.error->find("@([8 - 11], 11): syntax error"));
  }

  // valid query with missing dependencies (e.g. missing functions)
  {
    auto query = query_builder().build("name(A)", std::locale::classic());
    ASSERT_NE(nullptr, query.filter.get());
    ASSERT_NE(nullptr, query.error);
    ASSERT_EQ(nullptr, query.limit);

    auto pQuery = query.filter->prepare(reader, iresearch::order::prepared::unordered());
    ASSERT_EQ(nullptr, pQuery.get());

    ASSERT_EQ(std::string("@(7): parse error"), *(query.error));
  }

  // unsupported functionality by iResearch queries (e.g. like, ranges)
  {
    query_builder::branch_builder_function_t fnFail = [](
      boolean_function::contextual_buffer_t&,
      const std::locale&,
      const iresearch::string_ref&,
      void* cookie,
      const boolean_function::contextual_function_args_t&
    )->bool {
      return false;
    };
    query_builder::branch_builders builders(&fnFail, nullptr, nullptr, nullptr, nullptr);
    auto query = query_builder(builders).build("name==(A, bcd)", std::locale::classic());
    ASSERT_NE(nullptr, query.filter.get());
    ASSERT_NE(nullptr, query.error);
    ASSERT_EQ(nullptr, query.limit);

    auto pQuery = query.filter->prepare(reader, iresearch::order::prepared::unordered());
    ASSERT_EQ(nullptr, pQuery.get());

    ASSERT_EQ(std::string("filter conversion error, node: @5\n('name'@2 == ('A'@3, 'bcd'@4)@5)@6"), *(query.error));
  }
}

TEST_F(IqlQueryBuilderTestSuite, test_query_builder_builders_default) {
  irs::bytes_ref actual_value;
  iresearch::memory_directory dir;
  auto reader = load_json(dir, "simple_sequential.json");
  ASSERT_EQ(1, reader.size());
  auto& segment = reader[0]; // assume 0 is id of first/only segment
  auto column = segment.column_reader("name");
  ASSERT_NE(nullptr, column);
  auto values = column->values();

  // default range builder functr ()
  {
    query_builder::branch_builders builders;
    auto query = query_builder(builders).build("name==(A, C)", std::locale::classic());
    ASSERT_NE(nullptr, query.filter.get());
    ASSERT_EQ(nullptr, query.error);
    ASSERT_EQ(nullptr, query.limit);

    auto pQuery = query.filter->prepare(reader);
    ASSERT_NE(nullptr, pQuery.get());

    auto docsItr = pQuery->execute(segment);
    ASSERT_NE(nullptr, docsItr.get());
    ASSERT_TRUE(docsItr->next());
    ASSERT_TRUE(values(docsItr->value(), actual_value));
    ASSERT_EQ("B", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_FALSE(docsItr->next());
  }

  // default range builder functr (]
  {
    query_builder::branch_builders builders;
    auto query = query_builder(builders).build("name==(A, B]", std::locale::classic());
    ASSERT_NE(nullptr, query.filter.get());
    ASSERT_EQ(nullptr, query.error);
    ASSERT_EQ(nullptr, query.limit);

    auto pQuery = query.filter->prepare(reader);
    ASSERT_NE(nullptr, pQuery.get());

    auto docsItr = pQuery->execute(segment);
    ASSERT_NE(nullptr, docsItr.get());
    ASSERT_TRUE(docsItr->next());
    ASSERT_TRUE(values(docsItr->value(), actual_value));
    ASSERT_EQ("B", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_FALSE(docsItr->next());
  }

  // default range builder functr [)
  {
    query_builder::branch_builders builders;
    auto query = query_builder(builders).build("name==[A, B)", std::locale::classic());
    ASSERT_NE(nullptr, query.filter.get());
    ASSERT_EQ(nullptr, query.error);
    ASSERT_EQ(nullptr, query.limit);

    auto pQuery = query.filter->prepare(reader);
    ASSERT_NE(nullptr, pQuery.get());

    auto docsItr = pQuery->execute(segment);
    ASSERT_NE(nullptr, docsItr.get());
    ASSERT_TRUE(docsItr->next());
    ASSERT_TRUE(values(docsItr->value(), actual_value));
    ASSERT_EQ("A", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_FALSE(docsItr->next());
  }

  // default range builder functr []
  {
    query_builder::branch_builders builders;
    auto query = query_builder(builders).build("name==[A, B]", std::locale::classic());
    ASSERT_NE(nullptr, query.filter.get());
    ASSERT_EQ(nullptr, query.error);
    ASSERT_EQ(nullptr, query.limit);

    auto pQuery = query.filter->prepare(reader);
    ASSERT_NE(nullptr, pQuery.get());

    auto docsItr = pQuery->execute(segment);
    ASSERT_NE(nullptr, docsItr.get());
    ASSERT_TRUE(docsItr->next());
    ASSERT_TRUE(values(docsItr->value(), actual_value));
    ASSERT_EQ("A", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_TRUE(docsItr->next());
    ASSERT_TRUE(values(docsItr->value(), actual_value));
    ASSERT_EQ("B", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_FALSE(docsItr->next());
  }

  // default similar '~=' operator
  {
    iresearch::memory_directory analyzed_dir;
    auto analyzed_reader = load_json(analyzed_dir, "simple_sequential.json", true);

    ASSERT_EQ(1, analyzed_reader.size());
    auto& analyzed_segment = analyzed_reader[0]; // assume 0 is id of first/only segment
    auto column = analyzed_segment.column_reader("name");
    ASSERT_NE(nullptr, column);
    auto analyzed_segment_values = column->values();

    query_builder::branch_builders builders;
    auto locale = irs::locale_utils::locale("en"); // a locale that exists in tests
    auto query = query_builder(builders).build("name~=B", locale);
    ASSERT_NE(nullptr, query.filter.get());
    ASSERT_EQ(nullptr, query.error);
    ASSERT_EQ(nullptr, query.limit);

    auto pQuery = query.filter->prepare(analyzed_reader);
    ASSERT_NE(nullptr, pQuery.get());

    auto docsItr = pQuery->execute(analyzed_segment);
    ASSERT_NE(nullptr, docsItr.get());
    ASSERT_TRUE(docsItr->next());
    ASSERT_TRUE(analyzed_segment_values(docsItr->value(), actual_value));
    ASSERT_EQ("B", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_FALSE(docsItr->next());
  }
}

TEST_F(IqlQueryBuilderTestSuite, test_query_builder_builders_custom) {
  irs::bytes_ref actual_value;

  query_builder::branch_builder_function_t fnFail = [](
      boolean_function::contextual_buffer_t&,
      const std::locale&,
      const iresearch::string_ref&,
      void* cookie,
      const boolean_function::contextual_function_args_t&)->bool {
    std::cerr << "File: " << __FILE__ << " Line: " << __LINE__ << " Failed" << std::endl;
    throw "Fail";
  };
  query_builder::branch_builder_function_t fnEqual = [](
      boolean_function::contextual_buffer_t& node,
      const std::locale& locale,
      const iresearch::string_ref& field,
      void* cookie,
      const boolean_function::contextual_function_args_t& args)->bool {
    iresearch::bstring value;
    bool bValue;
    args[0].value(value, bValue, locale, cookie);
    auto& filter = node.proxy<iresearch::by_term>();
    *filter.mutable_field() = field;
    filter.mutable_options()->term = std::move(value);
    return true;
  };
  iresearch::memory_directory dir;
  auto reader = load_json(dir, "simple_sequential.json");
  ASSERT_EQ(1, reader.size());
  auto& segment = reader[0]; // assume 0 is id of first/only segment
  auto column = segment.column_reader("name");
  ASSERT_NE(nullptr, column);
  auto values = column->values();

  // custom range builder functr ()
  {
    query_builder::branch_builders builders(&fnEqual, &fnFail, &fnFail, &fnFail, &fnFail);
    auto query = query_builder(builders).build("name==(A, B)", std::locale::classic());
    ASSERT_NE(nullptr, query.filter.get());
    ASSERT_EQ(nullptr, query.error);
    ASSERT_EQ(nullptr, query.limit);

    auto pQuery = query.filter->prepare(reader);
    ASSERT_NE(nullptr, pQuery.get());

    auto docsItr = pQuery->execute(segment);
    ASSERT_NE(nullptr, docsItr.get());
    ASSERT_TRUE(docsItr->next());
    ASSERT_TRUE(values(docsItr->value(), actual_value));
    ASSERT_EQ("A", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_FALSE(docsItr->next());
    ASSERT_FALSE(docsItr->next());
  }

  // custom range builder functr (]
  {
    query_builder::branch_builders builders(&fnFail, &fnEqual, &fnFail, &fnFail, &fnFail);
    auto query = query_builder(builders).build("name==(A, B]", std::locale::classic());
    ASSERT_NE(nullptr, query.filter.get());
    ASSERT_EQ(nullptr, query.error);
    ASSERT_EQ(nullptr, query.limit);

    auto pQuery = query.filter->prepare(reader);
    ASSERT_NE(nullptr, pQuery.get());

    auto docsItr = pQuery->execute(segment);
    ASSERT_NE(nullptr, docsItr.get());
    ASSERT_TRUE(docsItr->next());
    ASSERT_TRUE(values(docsItr->value(), actual_value));
    ASSERT_EQ("A", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_FALSE(docsItr->next());
  }

  // custom range builder functr [)
  {
    query_builder::branch_builders builders(&fnFail, &fnFail, &fnEqual, &fnFail, &fnFail);
    auto query = query_builder(builders).build("name==[A, B)", std::locale::classic());
    ASSERT_NE(nullptr, query.filter.get());
    ASSERT_EQ(nullptr, query.error);
    ASSERT_EQ(nullptr, query.limit);

    auto pQuery = query.filter->prepare(reader);
    ASSERT_NE(nullptr, pQuery.get());

    auto docsItr = pQuery->execute(segment);
    ASSERT_NE(nullptr, docsItr.get());
    ASSERT_TRUE(docsItr->next());
    ASSERT_TRUE(values(docsItr->value(), actual_value));
    ASSERT_EQ("A", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_FALSE(docsItr->next());
  }

  // custom range builder functr []
  {
    query_builder::branch_builders builders(&fnFail, &fnFail, &fnFail, &fnEqual, &fnFail);
    auto query = query_builder(builders).build("name==[A, B]", std::locale::classic());
    ASSERT_NE(nullptr, query.filter.get());
    ASSERT_EQ(nullptr, query.error);
    ASSERT_EQ(nullptr, query.limit);

    auto pQuery = query.filter->prepare(reader);
    ASSERT_NE(nullptr, pQuery.get());

    auto docsItr = pQuery->execute(segment);
    ASSERT_NE(nullptr, docsItr.get());
    ASSERT_TRUE(docsItr->next());
    ASSERT_TRUE(values(docsItr->value(), actual_value));
    ASSERT_EQ("A", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_FALSE(docsItr->next());
  }

  // custom similar '~=' operator
  {
    query_builder::branch_builders builders(&fnFail, &fnFail, &fnFail, &fnFail, &fnEqual);
    auto query = query_builder(builders).build("name~=A", std::locale::classic());
    ASSERT_NE(nullptr, query.filter.get());
    ASSERT_EQ(nullptr, query.error);
    ASSERT_EQ(nullptr, query.limit);

    auto pQuery = query.filter->prepare(reader);
    ASSERT_NE(nullptr, pQuery.get());

    auto docsItr = pQuery->execute(segment);
    ASSERT_NE(nullptr, docsItr.get());
    ASSERT_TRUE(docsItr->next());
    ASSERT_TRUE(values(docsItr->value(), actual_value));
    ASSERT_EQ("A", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_FALSE(docsItr->next());
  }
}

TEST_F(IqlQueryBuilderTestSuite, test_query_builder_bool_fns) {
  boolean_function::contextual_function_t fnEqual = [](
    boolean_function::contextual_buffer_t& node,
    const std::locale& locale,
    void* cookie,
    const boolean_function::contextual_function_args_t& args
  )->bool {
    iresearch::bstring field;
    iresearch::bstring value;
    bool bField;
    bool bValue;
    args[0].value(field, bField, locale, cookie);
    args[1].value(value, bValue, locale, cookie);
    auto& filter = node.proxy<iresearch::by_term>();
    *filter.mutable_field() = iresearch::ref_cast<char>(field);
    filter.mutable_options()->term = std::move(value);
    return true;
  };
  iresearch::memory_directory dir;
  auto reader = load_json(dir, "simple_sequential.json");
  ASSERT_EQ(1, reader.size());
  auto& segment = reader[0]; // assume 0 is id of first/only segment

  // user supplied boolean_function
  {
    irs::bytes_ref actual_value;
    auto column = segment.column_reader("name");
    ASSERT_NE(nullptr, column);
    auto values = column->values();

    boolean_function bool_function(fnEqual, 2);
    boolean_functions bool_functions = {
      { "~", bool_function },
    };
    functions functions(bool_functions);
    auto query = query_builder(functions).build("~(name, A)", std::locale::classic());
    ASSERT_NE(nullptr, query.filter.get());
    ASSERT_EQ(nullptr, query.error);
    ASSERT_EQ(nullptr, query.limit);

    auto pQuery = query.filter->prepare(reader);
    ASSERT_NE(nullptr, pQuery.get());

    auto docsItr = pQuery->execute(segment);
    ASSERT_NE(nullptr, docsItr.get());
    ASSERT_TRUE(docsItr->next());
    ASSERT_TRUE(values(docsItr->value(), actual_value));
    ASSERT_EQ("A", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_FALSE(docsItr->next());
  }

  // user supplied negated boolean_function
  {
    irs::bytes_ref actual_value;
    irs::bytes_ref_input in;
    auto column = segment.column_reader("name");
    ASSERT_NE(nullptr, column);
    auto values = column->values();

    boolean_function bool_function(fnEqual, 2);
    boolean_functions bool_functions = {
      { "~", bool_function },
    };
    functions functions(bool_functions);
    auto query = query_builder(functions).build("!~(name, A)", std::locale::classic());
    ASSERT_NE(nullptr, query.filter.get());
    ASSERT_EQ(nullptr, query.error);
    ASSERT_EQ(nullptr, query.limit);

    auto pQuery = query.filter->prepare(reader);
    ASSERT_NE(nullptr, pQuery.get());

    auto docsItr = pQuery->execute(segment);
    ASSERT_NE(nullptr, docsItr.get());
    for (size_t count = segment.docs_count() - 1; count > 0; --count) {
      ASSERT_TRUE(docsItr->next());
      ASSERT_TRUE(values(docsItr->value(), actual_value)); in.reset(actual_value);
      ASSERT_NE("A", irs::read_string<std::string>(in));
    }

    ASSERT_FALSE(docsItr->next());
    ASSERT_FALSE(docsItr->next());
  }

  // user supplied boolean_function with expression args
  {
    irs::bytes_ref actual_value;
    boolean_function::contextual_function_t fnCplx = [](
      boolean_function::contextual_buffer_t& node,
      const std::locale& locale,
      void* cookie,
      const boolean_function::contextual_function_args_t& args
    )->bool {
      auto& root = node.proxy<iresearch::Or>();
      iresearch::bstring value;
      bool bValueNil;

      if (args.size() != 3 ||
          !args[0].value(value, bValueNil, locale, cookie) ||
          bValueNil ||
          !args[1].branch(root.add<iresearch::iql::proxy_filter>(), locale, cookie) ||
          !args[2].branch(root.add<iresearch::iql::proxy_filter>(), locale, cookie)) {
        return false;
      }

      auto& filter = root.add<iresearch::by_term>();
      *filter.mutable_field() = "name";
      filter.mutable_options()->term = std::move(value);

      return true;
    };
    boolean_function bool_function(fnCplx, 0, true);
    boolean_function expr_function(fnEqual, 2);
    boolean_functions bool_functions = {
      { "cplx", bool_function },
      { "expr", expr_function },
    };
    functions functions(bool_functions);
    auto query = query_builder(functions).build("cplx(A, (name==C || name==D), expr(name, E))", std::locale::classic());
    ASSERT_NE(nullptr, query.filter.get());
    ASSERT_EQ(nullptr, query.error);
    ASSERT_EQ(nullptr, query.limit);

    auto pQuery = query.filter->prepare(reader);
    ASSERT_NE(nullptr, pQuery.get());

    auto docsItr = pQuery->execute(segment);
    ASSERT_NE(nullptr, docsItr.get());
    std::unordered_set<irs::string_ref> expected = { "A", "C", "D", "E" };
    auto column = segment.column_reader("name");
    ASSERT_NE(nullptr, column);
    auto values = column->values();

    while (docsItr->next()) {
      ASSERT_TRUE(values(docsItr->value(), actual_value));
      ASSERT_EQ(1, expected.erase(irs::to_string<irs::string_ref>(actual_value.c_str())));
    }

    ASSERT_TRUE(expected.empty());
  }
}

TEST_F(IqlQueryBuilderTestSuite, test_query_builder_sequence_fns) {
  irs::bytes_ref actual_value;

  sequence_function::contextual_function_t fnValue = [](
    sequence_function::contextual_buffer_t& buf,
    const std::locale&,
    void*,
    const sequence_function::contextual_function_args_t& args
  )->bool {
    iresearch::bytes_ref value(reinterpret_cast<const iresearch::byte_type*>("A"), 1);
    buf.append(value);
    return true;
  };
  iresearch::memory_directory dir;
  auto reader = load_json(dir, "simple_sequential.json");
  ASSERT_EQ(1, reader.size());
  auto& segment = reader[0]; // assume 0 is id of first/only segment
  auto column = segment.column_reader("name");
  ASSERT_NE(nullptr, column);
  auto values = column->values();

  // user supplied sequence_function
  {
    sequence_function seq_function(fnValue);
    sequence_functions seq_functions = {
      { "valueA", seq_function },
    };
    functions functions(seq_functions);
    auto query = query_builder(functions).build("name==valueA()", std::locale::classic());
    ASSERT_NE(nullptr, query.filter.get());
    ASSERT_EQ(nullptr, query.error);
    ASSERT_EQ(nullptr, query.limit);

    auto pQuery = query.filter->prepare(reader);
    ASSERT_NE(nullptr, pQuery.get());

    auto docsItr = pQuery->execute(segment);
    ASSERT_NE(nullptr, docsItr.get());
    ASSERT_TRUE(docsItr->next());
    ASSERT_TRUE(values(docsItr->value(), actual_value));
    ASSERT_EQ("A", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_FALSE(docsItr->next());
  }
}

TEST_F(IqlQueryBuilderTestSuite, test_query_builder_order) {
  iresearch::memory_directory dir;
  auto reader = load_json(dir, "simple_sequential.json");
  ASSERT_EQ(1, reader.size());
  auto& segment = reader[0]; // assume 0 is id of first/only segment
/* FIXME field-value order is not yet supported by iresearch::search
  // order by sequence
  {
    auto query = query_builder().build("name==A || name == B order seq desc", std::locale::classic());
    ASSERT_NE(nullptr, query.filter.get());
    ASSERT_NE(nullptr, query.order);
    ASSERT_EQ(nullptr, query.error);
    ASSERT_EQ(nullptr, query.limit);

    auto pQuery = query.filter->prepare(reader);

    ASSERT_NE(nullptr, pQuery.get());

    auto docsItr = pQuery->execute(segment);
    ASSERT_NE(nullptr, docsItr.get());

    tests::field_visitor visitor("name");
    ASSERT_TRUE(docsItr->next());
    segment.document(docsItr->value(), visitor);
    ASSERT_EQ(std::string("B"), visitor.v_string);
    ASSERT_TRUE(docsItr->next());
    segment.document(docsItr->value(), visitor);
    ASSERT_EQ(std::string("A"), visitor.v_string);
    ASSERT_FALSE(docsItr->next());
  }

  // custom deterministic order function
  {
    order_function::deterministic_function_t fnTest = [](
      order_function::deterministic_buffer_t& buf,
      const order_function::deterministic_function_args_t& args
    )->bool {
      buf.append("name");
      return true;
    };
    order_function order_function(fnTest);
    order_functions order_functions = {
      { "c", order_function },
    };
    functions functions(order_functions);
    auto query = query_builder(functions).build("name==A || name == B order c() desc", std::locale::classic());
    ASSERT_NE(nullptr, query.filter.get());
    ASSERT_NE(nullptr, query.order);
    ASSERT_EQ(nullptr, query.error);
    ASSERT_EQ(nullptr, query.limit);

    auto pQuery = query.filter->prepare(reader);

    ASSERT_NE(nullptr, pQuery.get());

    auto docsItr = pQuery->execute(segment);
    ASSERT_NE(nullptr, docsItr.get());

    tests::field_visitor visitor("name");
    ASSERT_TRUE(docsItr->next());
    segment.document(docsItr->value(), visitor);
    ASSERT_EQ(std::string("B"), visitor.v_string);
    ASSERT_TRUE(docsItr->next());
    segment.document(docsItr->value(), visitor);
    ASSERT_EQ(std::string("A"), visitor.v_string);
    ASSERT_FALSE(docsItr->next());
  }
*/
  // custom contextual order function
  {
    irs::bytes_ref actual_value;
    auto column = segment.column_reader("name");
    ASSERT_NE(nullptr, column);
    auto values = column->values();

    std::vector<std::pair<bool, std::string>> direction;
    sequence_function::deterministic_function_t fnTestSeq = [](
      sequence_function::deterministic_buffer_t& buf,
      const order_function::deterministic_function_args_t&
    )->bool {
      buf.append("xyz");
      return true;
    };
    order_function::contextual_function_t fnTest = [&direction](
      order_function::contextual_buffer_t& buf,
      const std::locale& locale,
      void* cookie,
      const bool& ascending,
      const order_function::contextual_function_args_t& args
    )->bool {
      std::stringstream out;

      for (auto& arg: args) {
        iresearch::bstring value;
        bool bValue;
        arg.value(value, bValue, locale, cookie);
        out << iresearch::ref_cast<char>(value) << "|";
      }

      direction.emplace_back(ascending, out.str());
      buf.add<test_sort>(false);

      return true;
    };
    order_function order_function0(fnTest, 0);
    order_function order_function2(fnTest, 2);
    order_functions order_functions = {
      { "c", order_function0 },
      { "d", order_function2 },
    };
    sequence_function sequence_function(fnTestSeq);
    sequence_functions sequence_functions = {
      { "e", sequence_function },
    };
    functions functions(sequence_functions, order_functions);
    auto query = query_builder(functions).build("name==A order c() desc, d(e(), f) asc", std::locale::classic());
    ASSERT_NE(nullptr, query.filter.get());
    ASSERT_NE(nullptr, query.order);
    ASSERT_EQ(nullptr, query.error);
    ASSERT_EQ(nullptr, query.limit);

    auto pQuery = query.filter->prepare(reader);

    ASSERT_NE(nullptr, pQuery.get());

    auto docsItr = pQuery->execute(segment);
    ASSERT_NE(nullptr, docsItr.get());

    ASSERT_TRUE(docsItr->next());
    ASSERT_TRUE(values(docsItr->value(), actual_value));
    ASSERT_EQ("A", irs::to_string<irs::string_ref>(actual_value.c_str()));
    ASSERT_FALSE(docsItr->next());

    ASSERT_EQ(2, direction.size());
    ASSERT_FALSE(direction[0].first);
    ASSERT_EQ("", direction[0].second);
    ASSERT_TRUE(direction[1].first);
    ASSERT_EQ("xyz|f|", direction[1].second);
  }

  // ...........................................................................
  // invalid
  // ...........................................................................

  // non-existent function
  {
    auto query = query_builder().build("name==A order b()", std::locale::classic());
    ASSERT_NE(nullptr, query.filter.get());
    ASSERT_EQ(nullptr, query.order);
    ASSERT_NE(nullptr, query.error);
    ASSERT_EQ(nullptr, query.limit);

    auto pQuery = query.filter->prepare(reader);
    ASSERT_EQ(nullptr, pQuery.get());

    ASSERT_EQ(0, query.error->find("@(17): parse error"));
  }

  // failed deterministic function
  {
    order_function::deterministic_function_t fnFail = [](
      order_function::deterministic_buffer_t&,
      const order_function::deterministic_function_args_t&
    )->bool {
      return false;
    };
    order_function order_function(fnFail);
    order_functions order_functions = {
      { "b", order_function },
    };
    functions functions(order_functions);
    auto query = query_builder(functions).build("name==A order b()", std::locale::classic());
    ASSERT_NE(nullptr, query.filter.get());
    ASSERT_EQ(nullptr, query.order);
    ASSERT_NE(nullptr, query.error);
    ASSERT_EQ(nullptr, query.limit);

    auto pQuery = query.filter->prepare(reader);
    ASSERT_EQ(nullptr, pQuery.get());

    ASSERT_EQ(0, query.error->find("order conversion error, node: @7\n'b'()@7 ASC"));
  }

  // failed contextual function
  {
    order_function::contextual_function_t fnFail = [](
      order_function::contextual_buffer_t&,
      const std::locale&,
      void*,
      const bool&,
      const order_function::contextual_function_args_t&
    )->bool {
      return false;
    };
    order_function order_function(fnFail);
    order_functions order_functions = {
      { "b", order_function },
    };
    functions functions(order_functions);
    auto query = query_builder(functions).build("name==A order b()", std::locale::classic());
    ASSERT_NE(nullptr, query.filter.get());
    ASSERT_EQ(nullptr, query.order);
    ASSERT_NE(nullptr, query.error);
    ASSERT_EQ(nullptr, query.limit);

    auto pQuery = query.filter->prepare(reader);
    ASSERT_EQ(nullptr, pQuery.get());

    ASSERT_EQ(0, query.error->find("order conversion error, node: @7\n'b'()@7 ASC"));
  }

  // failed nested function
  {
    sequence_function::deterministic_function_t fnFail = [](
      sequence_function::deterministic_buffer_t&,
      const sequence_function::deterministic_function_args_t&
    )->bool {
      return false;
    };
    order_function::contextual_function_t fnTest = [](
      order_function::contextual_buffer_t&,
      const std::locale& locale,
      void* cookie,
      const bool&,
      const order_function::contextual_function_args_t& args
    )->bool {
      iresearch::bstring buf;
      bool bNil;
      return args[0].value(buf, bNil, locale, cookie); // expect false from above
    };
    order_function order_function(fnTest);
    order_functions order_functions = {
      { "b", order_function },
    };
    sequence_function sequence_function(fnFail);
    sequence_functions sequence_functions = {
      { "c", sequence_function },
    };
    functions functions(sequence_functions, order_functions);
    auto query = query_builder(functions).build("name==A order b(c())", std::locale::classic());
    ASSERT_NE(nullptr, query.filter.get());
    ASSERT_EQ(nullptr, query.order);
    ASSERT_NE(nullptr, query.error);
    ASSERT_EQ(nullptr, query.limit);

    auto pQuery = query.filter->prepare(reader);
    ASSERT_EQ(nullptr, pQuery.get());

    ASSERT_EQ(0, query.error->find("order conversion error, node: @9\n'b'('c'()@8)@9 ASC"));
  }
}
