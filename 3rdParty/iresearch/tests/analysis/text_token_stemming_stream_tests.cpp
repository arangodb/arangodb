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

#include "gtest/gtest.h"
#include "analysis/text_token_stemming_stream.hpp"

NS_LOCAL

class text_token_stemming_stream_tests: public ::testing::Test {
  virtual void SetUp() {
    // Code here will be called immediately after the constructor (right before each test).
  }

  virtual void TearDown() {
    // Code here will be called immediately after each test (right before the destructor).
  }
};

NS_END // NS_LOCAL

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

#ifndef IRESEARCH_DLL

TEST_F(text_token_stemming_stream_tests, test_stemming) {
  // test stemming (locale irs::string_ref::NIL)
  // there is no Snowball stemmer for "C" locale
  {
    irs::string_ref data("running");
    irs::analysis::text_token_stemming_stream stream(irs::string_ref::NIL);

    auto& offset = stream.attributes().get<irs::offset>();
    auto& payload = stream.attributes().get<irs::payload>();
    auto& term = stream.attributes().get<irs::term_attribute>();

    ASSERT_TRUE(stream.reset(data));

    ASSERT_TRUE(stream.next());
    ASSERT_EQ(0, offset->start);
    ASSERT_EQ(7, offset->end);
    ASSERT_EQ("running", irs::ref_cast<char>(payload->value));
    ASSERT_EQ("running", irs::ref_cast<char>(term->value()));
    ASSERT_FALSE(stream.next());
  }

  // test stemming (stemmer exists)
  {
    irs::string_ref data("running");
    irs::analysis::text_token_stemming_stream stream("en");

    auto& offset = stream.attributes().get<irs::offset>();
    auto& payload = stream.attributes().get<irs::payload>();
    auto& term = stream.attributes().get<irs::term_attribute>();

    ASSERT_TRUE(stream.reset(data));

    ASSERT_TRUE(stream.next());
    ASSERT_EQ(0, offset->start);
    ASSERT_EQ(7, offset->end);
    ASSERT_EQ("running", irs::ref_cast<char>(payload->value));
    ASSERT_EQ("run", irs::ref_cast<char>(term->value()));
    ASSERT_FALSE(stream.next());
  }

  // test stemming (stemmer does not exist)
  // there is no Snowball stemmer for Chinese
  {
    irs::string_ref data("running");
    irs::analysis::text_token_stemming_stream stream("zh");

    ASSERT_TRUE(stream.reset(data));

    auto& offset = stream.attributes().get<irs::offset>();
    auto& payload = stream.attributes().get<irs::payload>();
    auto& term = stream.attributes().get<irs::term_attribute>();

    ASSERT_TRUE(stream.next());
    ASSERT_EQ(0, offset->start);
    ASSERT_EQ(7, offset->end);
    ASSERT_EQ("running", irs::ref_cast<char>(payload->value));
    ASSERT_EQ("running", irs::ref_cast<char>(term->value()));
    ASSERT_FALSE(stream.next());
  }
}

#endif // IRESEARCH_DLL

TEST_F(text_token_stemming_stream_tests, test_load) {
  // load jSON string
  {
    irs::string_ref data("running");
    auto stream = irs::analysis::analyzers::get("stem", irs::text_format::json, "\"en\"");

    ASSERT_NE(nullptr, stream);
    ASSERT_TRUE(stream->reset(data));

    auto& offset = stream->attributes().get<irs::offset>();
    auto& payload = stream->attributes().get<irs::payload>();
    auto& term = stream->attributes().get<irs::term_attribute>();

    ASSERT_TRUE(stream->next());
    ASSERT_EQ(0, offset->start);
    ASSERT_EQ(7, offset->end);
    ASSERT_EQ("running", irs::ref_cast<char>(payload->value));
    ASSERT_EQ("run", irs::ref_cast<char>(term->value()));
    ASSERT_FALSE(stream->next());
  }

  // load jSON object
  {
    irs::string_ref data("running");
    auto stream = irs::analysis::analyzers::get("stem", irs::text_format::json, "{\"locale\":\"en\"}");

    ASSERT_NE(nullptr, stream);
    ASSERT_TRUE(stream->reset(data));

    auto& offset = stream->attributes().get<irs::offset>();
    auto& payload = stream->attributes().get<irs::payload>();
    auto& term = stream->attributes().get<irs::term_attribute>();

    ASSERT_TRUE(stream->next());
    ASSERT_EQ(0, offset->start);
    ASSERT_EQ(7, offset->end);
    ASSERT_EQ("running", irs::ref_cast<char>(payload->value));
    ASSERT_EQ("run", irs::ref_cast<char>(term->value()));
    ASSERT_FALSE(stream->next());
  }

  // load jSON invalid
  {
    ASSERT_EQ(nullptr, irs::analysis::analyzers::get("stem", irs::text_format::json, irs::string_ref::NIL));
    ASSERT_EQ(nullptr, irs::analysis::analyzers::get("stem", irs::text_format::json, "1"));
    ASSERT_EQ(nullptr, irs::analysis::analyzers::get("stem", irs::text_format::json, "[]"));
    ASSERT_EQ(nullptr, irs::analysis::analyzers::get("stem", irs::text_format::json, "{}"));
    ASSERT_EQ(nullptr, irs::analysis::analyzers::get("stem", irs::text_format::json, "{\"locale\":1}"));
  }

  // load text
  {
    irs::string_ref data("running");
    auto stream = irs::analysis::analyzers::get("stem", irs::text_format::text, "en");

    ASSERT_NE(nullptr, stream);
    ASSERT_TRUE(stream->reset(data));

    auto& offset = stream->attributes().get<irs::offset>();
    auto& payload = stream->attributes().get<irs::payload>();
    auto& term = stream->attributes().get<irs::term_attribute>();

    ASSERT_TRUE(stream->next());
    ASSERT_EQ(0, offset->start);
    ASSERT_EQ(7, offset->end);
    ASSERT_EQ("running", irs::ref_cast<char>(payload->value));
    ASSERT_EQ("run", irs::ref_cast<char>(term->value()));
    ASSERT_FALSE(stream->next());
  }
}


TEST_F(text_token_stemming_stream_tests, test_make_config_json) {

  //with unknown parameter
  {
    std::string config = "{\"locale\":\"ru_RU.UTF-8\",\"invalid_parameter\":true}";
    auto stream = irs::analysis::analyzers::get("stem", irs::text_format::json, config.c_str());
    ASSERT_NE(nullptr, stream);

    std::string actual;
    ASSERT_TRUE(stream->to_string(::irs::text_format::json, actual));
    ASSERT_EQ("{\"locale\":\"ru_RU.utf-8\"}", actual);
  }

}

TEST_F(text_token_stemming_stream_tests, test_make_config_text) {
  std::string config = "ru_RU.utf-8";
  auto stream = irs::analysis::analyzers::get("stem", irs::text_format::text, config.c_str());
  ASSERT_NE(nullptr, stream);

  std::string actual;
  ASSERT_TRUE(stream->to_string(::irs::text_format::text, actual));
  ASSERT_EQ(config, actual);
}

TEST_F(text_token_stemming_stream_tests, test_make_config_invalid_format) {
  std::string config = "ru_RU.utfF-8";
  auto stream = irs::analysis::analyzers::get("stem", irs::text_format::text, config.c_str());
  ASSERT_NE(nullptr, stream);

  std::string actual;
  ASSERT_FALSE(stream->to_string(::irs::text_format::csv, actual));
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------