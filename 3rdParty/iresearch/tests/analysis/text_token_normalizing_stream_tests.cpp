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
#include "analysis/text_token_normalizing_stream.hpp"

NS_LOCAL

class text_token_normalizing_stream_tests: public ::testing::Test {
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

TEST_F(text_token_normalizing_stream_tests, test_normalizing) {
  typedef irs::analysis::text_token_normalizing_stream::options_t options_t;

  // test default normalization
  {
    options_t options;

    options.locale = "en.utf8";

    irs::string_ref data("rUnNiNg\xd0\x81");
    irs::analysis::text_token_normalizing_stream stream(options);

    auto& offset = stream.attributes().get<irs::offset>();
    auto& payload = stream.attributes().get<irs::payload>();
    auto& term = stream.attributes().get<irs::term_attribute>();

    ASSERT_TRUE(stream.reset(data));

    ASSERT_TRUE(stream.next());
    ASSERT_EQ(0, offset->start);
    ASSERT_EQ(9, offset->end);
    ASSERT_EQ(data, irs::ref_cast<char>(payload->value));
    ASSERT_EQ(data, irs::ref_cast<char>(term->value()));
    ASSERT_FALSE(stream.next());
  }

  // test accent removal
  {
    options_t options;

    options.locale = "en.utf8";
    options.no_accent = true;

    irs::string_ref data("rUnNiNg\xd0\x81");
    irs::string_ref expected("rUnNiNg\xd0\x95");
    irs::analysis::text_token_normalizing_stream stream(options);

    auto& offset = stream.attributes().get<irs::offset>();
    auto& payload = stream.attributes().get<irs::payload>();
    auto& term = stream.attributes().get<irs::term_attribute>();

    ASSERT_TRUE(stream.reset(data));

    ASSERT_TRUE(stream.next());
    ASSERT_EQ(0, offset->start);
    ASSERT_EQ(9, offset->end);
    ASSERT_EQ(data, irs::ref_cast<char>(payload->value));
    ASSERT_EQ(expected, irs::ref_cast<char>(term->value()));
    ASSERT_FALSE(stream.next());
  }

  // test lower case
  {
    options_t options;

    options.locale = "en.utf8";
    options.case_convert = options_t::case_convert_t::LOWER;

    irs::string_ref data("rUnNiNg\xd0\x81");
    irs::string_ref expected("running\xd1\x91");
    irs::analysis::text_token_normalizing_stream stream(options);

    auto& offset = stream.attributes().get<irs::offset>();
    auto& payload = stream.attributes().get<irs::payload>();
    auto& term = stream.attributes().get<irs::term_attribute>();

    ASSERT_TRUE(stream.reset(data));

    ASSERT_TRUE(stream.next());
    ASSERT_EQ(0, offset->start);
    ASSERT_EQ(9, offset->end);
    ASSERT_EQ(data, irs::ref_cast<char>(payload->value));
    ASSERT_EQ(expected, irs::ref_cast<char>(term->value()));
    ASSERT_FALSE(stream.next());
  }

  // test upper case
  {
    options_t options;

    options.locale = "en.utf8";
    options.case_convert = options_t::case_convert_t::UPPER;

    irs::string_ref data("rUnNiNg\xd1\x91");
    irs::string_ref expected("RUNNING\xd0\x81");
    irs::analysis::text_token_normalizing_stream stream(options);

    auto& offset = stream.attributes().get<irs::offset>();
    auto& payload = stream.attributes().get<irs::payload>();
    auto& term = stream.attributes().get<irs::term_attribute>();

    ASSERT_TRUE(stream.reset(data));

    ASSERT_TRUE(stream.next());
    ASSERT_EQ(0, offset->start);
    ASSERT_EQ(9, offset->end);
    ASSERT_EQ(data, irs::ref_cast<char>(payload->value));
    ASSERT_EQ(expected, irs::ref_cast<char>(term->value()));
    ASSERT_FALSE(stream.next());
  }
}

#endif // IRESEARCH_DLL

TEST_F(text_token_normalizing_stream_tests, test_load) {
  // load jSON string
  {
    irs::string_ref data("running");
    auto stream = irs::analysis::analyzers::get("text-token-normalize", irs::text_format::json, "\"en\"");

    ASSERT_NE(nullptr, stream);
    ASSERT_TRUE(stream->reset(data));

    auto& offset = stream->attributes().get<irs::offset>();
    auto& payload = stream->attributes().get<irs::payload>();
    auto& term = stream->attributes().get<irs::term_attribute>();

    ASSERT_TRUE(stream->next());
    ASSERT_EQ(0, offset->start);
    ASSERT_EQ(7, offset->end);
    ASSERT_EQ("running", irs::ref_cast<char>(payload->value));
    ASSERT_EQ("running", irs::ref_cast<char>(term->value()));
    ASSERT_FALSE(stream->next());
  }

  // load jSON object
  {
    irs::string_ref data("running");
    auto stream = irs::analysis::analyzers::get("text-token-normalize", irs::text_format::json, "{\"locale\":\"en\"}");

    ASSERT_NE(nullptr, stream);
    ASSERT_TRUE(stream->reset(data));

    auto& offset = stream->attributes().get<irs::offset>();
    auto& payload = stream->attributes().get<irs::payload>();
    auto& term = stream->attributes().get<irs::term_attribute>();

    ASSERT_TRUE(stream->next());
    ASSERT_EQ(0, offset->start);
    ASSERT_EQ(7, offset->end);
    ASSERT_EQ("running", irs::ref_cast<char>(payload->value));
    ASSERT_EQ("running", irs::ref_cast<char>(term->value()));
    ASSERT_FALSE(stream->next());
  }

  // load jSON invalid
  {
    ASSERT_EQ(nullptr, irs::analysis::analyzers::get("text-token-normalize", irs::text_format::json, irs::string_ref::NIL));
    ASSERT_EQ(nullptr, irs::analysis::analyzers::get("text-token-normalize", irs::text_format::json, "1"));
    ASSERT_EQ(nullptr, irs::analysis::analyzers::get("text-token-normalize", irs::text_format::json, "[]"));
    ASSERT_EQ(nullptr, irs::analysis::analyzers::get("text-token-normalize", irs::text_format::json, "{}"));
    ASSERT_EQ(nullptr, irs::analysis::analyzers::get("text-token-normalize", irs::text_format::json, "{\"locale\":1}"));
    ASSERT_EQ(nullptr, irs::analysis::analyzers::get("text-token-normalize", irs::text_format::json, "{\"locale\":\"en\", \"case_convert\":42}"));
    ASSERT_EQ(nullptr, irs::analysis::analyzers::get("text-token-normalize", irs::text_format::json, "{\"locale\":\"en\", \"no_accent\":42}"));
  }

  // load text
  {
    irs::string_ref data("running");
    auto stream = irs::analysis::analyzers::get("text-token-normalize", irs::text_format::text, "en");

    ASSERT_NE(nullptr, stream);
    ASSERT_TRUE(stream->reset(data));

    auto& offset = stream->attributes().get<irs::offset>();
    auto& payload = stream->attributes().get<irs::payload>();
    auto& term = stream->attributes().get<irs::term_attribute>();

    ASSERT_TRUE(stream->next());
    ASSERT_EQ(0, offset->start);
    ASSERT_EQ(7, offset->end);
    ASSERT_EQ("running", irs::ref_cast<char>(payload->value));
    ASSERT_EQ("running", irs::ref_cast<char>(term->value()));
    ASSERT_FALSE(stream->next());
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------