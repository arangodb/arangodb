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
#include "utils/locale_utils.hpp"

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

    options.locale = irs::locale_utils::locale("en.utf8");

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

    options.locale = irs::locale_utils::locale("en.utf8");
    options.accent = false;

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

    options.locale = irs::locale_utils::locale("en.utf8");
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

    options.locale = irs::locale_utils::locale("en.utf8");
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
    auto stream = irs::analysis::analyzers::get("norm", irs::text_format::json, "\"en\"");

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
    auto stream = irs::analysis::analyzers::get("norm", irs::text_format::json, "{\"locale\":\"en\"}");

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

  // with UPPER case 
  {
    irs::string_ref data("ruNNing");
    auto stream = irs::analysis::analyzers::get("norm", irs::text_format::json, "{\"locale\":\"en\", \"case\":\"upper\"}");

    ASSERT_NE(nullptr, stream);
    ASSERT_TRUE(stream->reset(data));

    auto& offset = stream->attributes().get<irs::offset>();
    auto& payload = stream->attributes().get<irs::payload>();
    auto& term = stream->attributes().get<irs::term_attribute>();

    ASSERT_TRUE(stream->next());
    ASSERT_EQ(0, offset->start);
    ASSERT_EQ(7, offset->end);
    ASSERT_EQ("ruNNing", irs::ref_cast<char>(payload->value));
    ASSERT_EQ("RUNNING", irs::ref_cast<char>(term->value()));
    ASSERT_FALSE(stream->next());
  }

  // with LOWER case 
  {
    irs::string_ref data("ruNNing");
    auto stream = irs::analysis::analyzers::get("norm", irs::text_format::json, "{\"locale\":\"en\", \"case\":\"lower\"}");

    ASSERT_NE(nullptr, stream);
    ASSERT_TRUE(stream->reset(data));

    auto& offset = stream->attributes().get<irs::offset>();
    auto& payload = stream->attributes().get<irs::payload>();
    auto& term = stream->attributes().get<irs::term_attribute>();

    ASSERT_TRUE(stream->next());
    ASSERT_EQ(0, offset->start);
    ASSERT_EQ(7, offset->end);
    ASSERT_EQ("ruNNing", irs::ref_cast<char>(payload->value));
    ASSERT_EQ("running", irs::ref_cast<char>(term->value()));
    ASSERT_FALSE(stream->next());
  }

  // with NONE case 
  {
    irs::string_ref data("ruNNing");
    auto stream = irs::analysis::analyzers::get("norm", irs::text_format::json, "{\"locale\":\"en\", \"case\":\"none\"}");

    ASSERT_NE(nullptr, stream);
    ASSERT_TRUE(stream->reset(data));

    auto& offset = stream->attributes().get<irs::offset>();
    auto& payload = stream->attributes().get<irs::payload>();
    auto& term = stream->attributes().get<irs::term_attribute>();

    ASSERT_TRUE(stream->next());
    ASSERT_EQ(0, offset->start);
    ASSERT_EQ(7, offset->end);
    ASSERT_EQ("ruNNing", irs::ref_cast<char>(payload->value));
    ASSERT_EQ("ruNNing", irs::ref_cast<char>(term->value()));
    ASSERT_FALSE(stream->next());
  }

  // remove accent 
  {
    std::wstring unicodeData = L"\u00F6\u00F5";
    
    std::string data;
    auto locale = irs::locale_utils::locale(irs::string_ref::NIL, "utf8", true); 
    ASSERT_TRUE(irs::locale_utils::append_external<wchar_t>(data, unicodeData, locale));
    
    auto stream = irs::analysis::analyzers::get("norm", irs::text_format::json, "{\"locale\":\"de_DE.UTF8\", \"case\":\"lower\", \"accent\":false}");
   
    ASSERT_NE(nullptr, stream);
    ASSERT_TRUE(stream->reset(data));
    auto& offset = stream->attributes().get<irs::offset>();
    auto& payload = stream->attributes().get<irs::payload>();
    auto& term = stream->attributes().get<irs::term_attribute>();

    ASSERT_TRUE(stream->next());
    
    ASSERT_EQ(0, offset->start);
    ASSERT_EQ(data.size(), offset->end);
    ASSERT_EQ(data, irs::ref_cast<char>(payload->value));

    std::basic_string<wchar_t> unicodeTerm;
    ASSERT_TRUE(irs::locale_utils::append_internal<wchar_t>(unicodeTerm, irs::ref_cast<char>(term->value()), locale));
    ASSERT_EQ(L"\u006F\u006F", unicodeTerm);
    
    ASSERT_FALSE(stream->next());
  }

  // load jSON invalid
  {
    ASSERT_EQ(nullptr, irs::analysis::analyzers::get("norm", irs::text_format::json, irs::string_ref::NIL));
    ASSERT_EQ(nullptr, irs::analysis::analyzers::get("norm", irs::text_format::json, "1"));
    ASSERT_EQ(nullptr, irs::analysis::analyzers::get("norm", irs::text_format::json, "[]"));
    ASSERT_EQ(nullptr, irs::analysis::analyzers::get("norm", irs::text_format::json, "{}"));
    ASSERT_EQ(nullptr, irs::analysis::analyzers::get("norm", irs::text_format::json, "{\"locale\":1}"));
    ASSERT_EQ(nullptr, irs::analysis::analyzers::get("norm", irs::text_format::json, "{\"locale\":\"en\", \"case\":42}"));
    ASSERT_EQ(nullptr, irs::analysis::analyzers::get("norm", irs::text_format::json, "{\"locale\":\"en\", \"accent\":42}"));
  }

  // load text
  {
    irs::string_ref data("running");
    auto stream = irs::analysis::analyzers::get("norm", irs::text_format::text, "en");

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

TEST_F(text_token_normalizing_stream_tests, test_make_config_json) {
  //with unknown parameter
  {
    std::string config = "{\"locale\":\"ru_RU.UTF-8\",\"case\":\"lower\",\"invalid_parameter\":true,\"accent\":true}";
    std::string actual;
    ASSERT_TRUE(irs::analysis::analyzers::normalize(actual, "norm", irs::text_format::json, config));
    ASSERT_EQ("{\"locale\":\"ru_RU.utf-8\",\"case\":\"lower\",\"accent\":true}", actual);
  }

  // no case convert in creation. Default value shown
  {
    std::string config = "{\"locale\":\"ru_RU.UTF-8\",\"accent\":true}";
    std::string actual;
    ASSERT_TRUE(irs::analysis::analyzers::normalize(actual, "norm", irs::text_format::json, config));
    ASSERT_EQ("{\"locale\":\"ru_RU.utf-8\",\"case\":\"none\",\"accent\":true}", actual);
  }

  // no accent in creation. Default value shown
  {
    std::string config = "{\"locale\":\"ru_RU.UTF-8\",\"case\":\"lower\"}";
    std::string actual;
    ASSERT_TRUE(irs::analysis::analyzers::normalize(actual, "norm", irs::text_format::json, config));
    ASSERT_EQ("{\"locale\":\"ru_RU.utf-8\",\"case\":\"lower\",\"accent\":true}", actual);
  }

  
  // non default values for accent and case
  {
    std::string config = "{\"locale\":\"ru_RU.utf-8\",\"case\":\"upper\",\"accent\":true}";
    std::string actual;
    ASSERT_TRUE(irs::analysis::analyzers::normalize(actual, "norm", irs::text_format::json, config));
    ASSERT_EQ(config, actual);
  }
}

TEST_F(text_token_normalizing_stream_tests, test_make_config_text) {
  std::string config = "RU";
  std::string actual;
  ASSERT_TRUE(irs::analysis::analyzers::normalize(actual, "norm", irs::text_format::text, config));
  ASSERT_EQ("ru", actual);
}

TEST_F(text_token_normalizing_stream_tests, test_make_config_invalid_format) {
  std::string config = "ru_RU.UTF-8";
  std::string actual;
  ASSERT_FALSE(irs::analysis::analyzers::normalize(actual, "norm", irs::text_format::csv, config));
}

TEST_F(text_token_normalizing_stream_tests, test_invalid_locale) {
  auto stream = irs::analysis::analyzers::get(
      "norm", irs::text_format::json, "{\"locale\":\"invalid12345.UTF-8\"}");
  ASSERT_EQ(nullptr, stream);
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------
