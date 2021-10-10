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

#include "analysis/text_token_normalizing_stream.hpp"

#include "gtest/gtest.h"
#include "velocypack/Parser.h"
#include "velocypack/velocypack-aliases.h"

namespace {

class normalizing_token_stream_tests: public ::testing::Test { };

} // namespace {

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

#ifndef IRESEARCH_DLL

TEST_F(normalizing_token_stream_tests, consts) {
  static_assert("norm" == irs::type<irs::analysis::normalizing_token_stream>::name());
}

TEST_F(normalizing_token_stream_tests, test_normalizing) {
  typedef irs::analysis::normalizing_token_stream::options_t options_t;

  // test default normalization
  {
    options_t options;
    options.locale = icu::Locale::createFromName("en");

    irs::string_ref data("rUnNiNg\xd0\x81");
    irs::analysis::normalizing_token_stream stream(options);
    ASSERT_EQ(irs::type<irs::analysis::normalizing_token_stream>::id(), stream.type());

    auto* offset = irs::get<irs::offset>(stream);
    auto* payload = irs::get<irs::payload>(stream);
    ASSERT_EQ(nullptr, payload);
    auto* term = irs::get<irs::term_attribute>(stream);

    ASSERT_TRUE(stream.reset(data));

    ASSERT_TRUE(stream.next());
    ASSERT_EQ(0, offset->start);
    ASSERT_EQ(9, offset->end);
    ASSERT_EQ(data, irs::ref_cast<char>(term->value));
    ASSERT_FALSE(stream.next());
  }

  // test accent removal
  {
    options_t options;

    options.locale = icu::Locale::createFromName("en.utf8");
    options.accent = false;

    irs::string_ref data("rUnNiNg\xd0\x81");
    irs::string_ref expected("rUnNiNg\xd0\x95");
    irs::analysis::normalizing_token_stream stream(options);

    auto* offset = irs::get<irs::offset>(stream);
    auto* payload = irs::get<irs::payload>(stream);
    ASSERT_EQ(nullptr, payload);
    auto* term = irs::get<irs::term_attribute>(stream);

    ASSERT_TRUE(stream.reset(data));

    ASSERT_TRUE(stream.next());
    ASSERT_EQ(0, offset->start);
    ASSERT_EQ(9, offset->end);
    ASSERT_EQ(expected, irs::ref_cast<char>(term->value));
    ASSERT_FALSE(stream.next());
  }

  // test lower case
  {
    options_t options;

    options.locale = icu::Locale::createFromName("en.utf8");
    options.case_convert = irs::analysis::normalizing_token_stream::LOWER;

    irs::string_ref data("rUnNiNg\xd0\x81");
    irs::string_ref expected("running\xd1\x91");
    irs::analysis::normalizing_token_stream stream(options);

    auto* offset = irs::get<irs::offset>(stream);
    auto* payload = irs::get<irs::payload>(stream);
    ASSERT_EQ(nullptr, payload);
    auto* term = irs::get<irs::term_attribute>(stream);

    ASSERT_TRUE(stream.reset(data));

    ASSERT_TRUE(stream.next());
    ASSERT_EQ(0, offset->start);
    ASSERT_EQ(9, offset->end);
    ASSERT_EQ(expected, irs::ref_cast<char>(term->value));
    ASSERT_FALSE(stream.next());
  }

  // test upper case
  {
    options_t options;

    options.locale = icu::Locale::createFromName("en.utf8");
    options.case_convert = irs::analysis::normalizing_token_stream::UPPER;

    irs::string_ref data("rUnNiNg\xd1\x91");
    irs::string_ref expected("RUNNING\xd0\x81");
    irs::analysis::normalizing_token_stream stream(options);

    auto* offset = irs::get<irs::offset>(stream);
    auto* payload = irs::get<irs::payload>(stream);
    ASSERT_EQ(nullptr, payload);
    auto* term = irs::get<irs::term_attribute>(stream);

    ASSERT_TRUE(stream.reset(data));

    ASSERT_TRUE(stream.next());
    ASSERT_EQ(0, offset->start);
    ASSERT_EQ(9, offset->end);
    ASSERT_EQ(expected, irs::ref_cast<char>(term->value));
    ASSERT_FALSE(stream.next());
  }
}

#endif // IRESEARCH_DLL

TEST_F(normalizing_token_stream_tests, test_load) {
  // load jSON object
  {
    irs::string_ref data("running");
    auto stream = irs::analysis::analyzers::get(
      "norm", irs::type<irs::text_format::json>::get(), "{\"locale\":\"en\"}");

    ASSERT_NE(nullptr, stream);
    ASSERT_TRUE(stream->reset(data));

    auto* offset = irs::get<irs::offset>(*stream);
    auto* payload = irs::get<irs::payload>(*stream);
    ASSERT_EQ(nullptr, payload);
    auto* term = irs::get<irs::term_attribute>(*stream);

    ASSERT_TRUE(stream->next());
    ASSERT_EQ(0, offset->start);
    ASSERT_EQ(7, offset->end);
    ASSERT_EQ("running", irs::ref_cast<char>(term->value));
    ASSERT_FALSE(stream->next());
  }

  // with UPPER case 
  {
    irs::string_ref data("ruNNing");
    auto stream = irs::analysis::analyzers::get(
      "norm", irs::type<irs::text_format::json>::get(),
      "{\"locale\":\"en\", \"case\":\"upper\"}");

    ASSERT_NE(nullptr, stream);
    ASSERT_TRUE(stream->reset(data));

    auto* offset = irs::get<irs::offset>(*stream);
    auto* payload = irs::get<irs::payload>(*stream);
    ASSERT_EQ(nullptr, payload);
    auto* term = irs::get<irs::term_attribute>(*stream);

    ASSERT_TRUE(stream->next());
    ASSERT_EQ(0, offset->start);
    ASSERT_EQ(7, offset->end);
    ASSERT_EQ("RUNNING", irs::ref_cast<char>(term->value));
    ASSERT_FALSE(stream->next());
  }

  // with LOWER case 
  {
    irs::string_ref data("ruNNing");
    auto stream = irs::analysis::analyzers::get(
      "norm", irs::type<irs::text_format::json>::get(),
      "{\"locale\":\"en\", \"case\":\"lower\"}");

    ASSERT_NE(nullptr, stream);
    ASSERT_TRUE(stream->reset(data));

    auto* offset = irs::get<irs::offset>(*stream);
    auto* payload = irs::get<irs::payload>(*stream);
    ASSERT_EQ(nullptr, payload);
    auto* term = irs::get<irs::term_attribute>(*stream);

    ASSERT_TRUE(stream->next());
    ASSERT_EQ(0, offset->start);
    ASSERT_EQ(7, offset->end);
    ASSERT_EQ("running", irs::ref_cast<char>(term->value));
    ASSERT_FALSE(stream->next());
  }

  // with NONE case 
  {
    irs::string_ref data("ruNNing");
    auto stream = irs::analysis::analyzers::get(
      "norm", irs::type<irs::text_format::json>::get(),
      "{\"locale\":\"en\", \"case\":\"none\"}");

    ASSERT_NE(nullptr, stream);
    ASSERT_TRUE(stream->reset(data));

    auto* offset = irs::get<irs::offset>(*stream);
    auto* payload = irs::get<irs::payload>(*stream);
    ASSERT_EQ(nullptr, payload);
    auto* term = irs::get<irs::term_attribute>(*stream);

    ASSERT_TRUE(stream->next());
    ASSERT_EQ(0, offset->start);
    ASSERT_EQ(7, offset->end);
    ASSERT_EQ("ruNNing", irs::ref_cast<char>(term->value));
    ASSERT_FALSE(stream->next());
  }

  // remove accent 
  {
    const std::string data = u8"\u00F6\u00F5";

    auto stream = irs::analysis::analyzers::get(
      "norm", irs::type<irs::text_format::json>::get(),
      "{\"locale\":\"de_DE.UTF8\", \"case\":\"lower\", \"accent\":false}");
   
    ASSERT_NE(nullptr, stream);
    ASSERT_TRUE(stream->reset(data));
    auto* offset = irs::get<irs::offset>(*stream);
    auto* payload = irs::get<irs::payload>(*stream);
    ASSERT_EQ(nullptr, payload);
    auto* term = irs::get<irs::term_attribute>(*stream);

    ASSERT_TRUE(stream->next());
    
    ASSERT_EQ(0, offset->start);
    ASSERT_EQ(data.size(), offset->end);
    ASSERT_EQ(u8"\u006F\u006F", irs::ref_cast<char>(term->value));
    
    ASSERT_FALSE(stream->next());
  }

  // load jSON invalid
  {
    ASSERT_EQ(nullptr, irs::analysis::analyzers::get("norm", irs::type<irs::text_format::json>::get(), irs::string_ref::NIL));
    ASSERT_EQ(nullptr, irs::analysis::analyzers::get("norm", irs::type<irs::text_format::json>::get(), "1"));
    ASSERT_EQ(nullptr, irs::analysis::analyzers::get("norm", irs::type<irs::text_format::json>::get(), "[]"));
    ASSERT_EQ(nullptr, irs::analysis::analyzers::get("norm", irs::type<irs::text_format::json>::get(), "{}"));
    ASSERT_EQ(nullptr, irs::analysis::analyzers::get("norm", irs::type<irs::text_format::json>::get(), "{\"locale\":1}"));
    ASSERT_EQ(nullptr, irs::analysis::analyzers::get("norm", irs::type<irs::text_format::json>::get(), "{\"locale\":\"en\", \"case\":42}"));
    ASSERT_EQ(nullptr, irs::analysis::analyzers::get("norm", irs::type<irs::text_format::json>::get(), "{\"locale\":\"en\", \"accent\":42}"));
  }

  // load text
  {
    irs::string_ref data("running");
    auto stream = irs::analysis::analyzers::get("norm", irs::type<irs::text_format::json>::get(), R"({"locale":"en"})");

    ASSERT_NE(nullptr, stream);
    ASSERT_TRUE(stream->reset(data));

    auto* offset = irs::get<irs::offset>(*stream);
    auto* payload = irs::get<irs::payload>(*stream);
    ASSERT_EQ(nullptr, payload);
    auto* term = irs::get<irs::term_attribute>(*stream);

    ASSERT_TRUE(stream->next());
    ASSERT_EQ(0, offset->start);
    ASSERT_EQ(7, offset->end);
    ASSERT_EQ("running", irs::ref_cast<char>(term->value));
    ASSERT_FALSE(stream->next());
  }
}

TEST_F(normalizing_token_stream_tests, test_make_config_json) {
  //with unknown parameter
  {
    std::string config = "{\"locale\":\"ru_RU.UTF-8\",\"case\":\"lower\",\"invalid_parameter\":true,\"accent\":true}";
    std::string actual;
    ASSERT_TRUE(irs::analysis::analyzers::normalize(actual, "norm", irs::type<irs::text_format::json>::get(), config));
    ASSERT_EQ(VPackParser::fromJson("{\"locale\":\"ru_RU\",\"case\":\"lower\",\"accent\":true}")->toString(), actual);
  }

  // test vpack
  {
    std::string config = "{\"locale\":\"ru_RU.UTF-8\",\"case\":\"lower\",\"invalid_parameter\":true,\"accent\":true}";
    auto in_vpack = VPackParser::fromJson(config.c_str(), config.size());
    std::string in_str;
    in_str.assign(in_vpack->slice().startAs<char>(), in_vpack->slice().byteSize());
    std::string out_str;
    ASSERT_TRUE(irs::analysis::analyzers::normalize(out_str, "norm", irs::type<irs::text_format::vpack>::get(), in_str));
    VPackSlice out_slice(reinterpret_cast<const uint8_t*>(out_str.c_str()));
    ASSERT_EQ(VPackParser::fromJson("{\"locale\":\"ru_RU\",\"case\":\"lower\",\"accent\":true}")->toString(), out_slice.toString());
  }

  // test vpack with variant
  {
    std::string config = "{\"locale\":\"ru_RU_TRADITIONAL.UTF-8\",\"case\":\"lower\",\"invalid_parameter\":true,\"accent\":true}";
    auto in_vpack = VPackParser::fromJson(config.c_str(), config.size());
    std::string in_str;
    in_str.assign(in_vpack->slice().startAs<char>(), in_vpack->slice().byteSize());
    std::string out_str;
    ASSERT_TRUE(irs::analysis::analyzers::normalize(out_str, "norm", irs::type<irs::text_format::vpack>::get(), in_str));
    VPackSlice out_slice(reinterpret_cast<const uint8_t*>(out_str.c_str()));
    ASSERT_EQ(VPackParser::fromJson("{\"locale\":\"ru_RU_TRADITIONAL.UTF-8\",\"case\":\"lower\",\"accent\":true}")->toString(), out_slice.toString());
  }

  // test vpack with variant
  {
    std::string config = "{\"locale\":\"ru_RU.UTF-8@EURO\",\"case\":\"lower\",\"invalid_parameter\":true,\"accent\":true}";
    auto in_vpack = VPackParser::fromJson(config.c_str(), config.size());
    std::string in_str;
    in_str.assign(in_vpack->slice().startAs<char>(), in_vpack->slice().byteSize());
    std::string out_str;
    ASSERT_TRUE(irs::analysis::analyzers::normalize(out_str, "norm", irs::type<irs::text_format::vpack>::get(), in_str));
    VPackSlice out_slice(reinterpret_cast<const uint8_t*>(out_str.c_str()));
    ASSERT_EQ(VPackParser::fromJson("{\"locale\":\"ru_RU\",\"case\":\"lower\",\"accent\":true}")->toString(), out_slice.toString());
  }

  // no case convert in creation. Default value shown
  {
    std::string config = "{\"locale\":\"ru_RU.UTF-8\",\"accent\":true}";
    std::string actual;
    ASSERT_TRUE(irs::analysis::analyzers::normalize(actual, "norm", irs::type<irs::text_format::json>::get(), config));
    ASSERT_EQ(VPackParser::fromJson("{\"locale\":\"ru_RU\",\"case\":\"none\",\"accent\":true}")->toString(), actual);
  }

  // no accent in creation. Default value shown
  {
    std::string config = "{\"locale\":\"ru_RU.UTF-8\",\"case\":\"lower\"}";
    std::string actual;
    ASSERT_TRUE(irs::analysis::analyzers::normalize(actual, "norm", irs::type<irs::text_format::json>::get(), config));
    ASSERT_EQ(VPackParser::fromJson("{\"locale\":\"ru_RU\",\"case\":\"lower\",\"accent\":true}")->toString(), actual);
  }

  
  // non default values for accent and case
  {
    std::string config = "{\"locale\":\"ru_RU.utf-8\",\"case\":\"upper\",\"accent\":true}";
    std::string actual;
    ASSERT_TRUE(irs::analysis::analyzers::normalize(actual, "norm", irs::type<irs::text_format::json>::get(), config));
    ASSERT_EQ(VPackParser::fromJson("{\"locale\":\"ru_RU\",\"case\":\"upper\",\"accent\":true}")->toString(), actual);
  }

  // non default values for accent and case
  {
    std::string config = "{\"locale\":\"de_DE@collation=phonebook\",\"case\":\"upper\",\"accent\":true}";
    std::string actual;
    ASSERT_TRUE(irs::analysis::analyzers::normalize(actual, "norm", irs::type<irs::text_format::json>::get(), config));
    ASSERT_EQ(VPackParser::fromJson("{\"locale\":\"de_DE\",\"case\":\"upper\",\"accent\":true}")->toString(), actual);
  }
}

TEST_F(normalizing_token_stream_tests, test_make_config_text) {
  std::string config = "RU";
  std::string actual;
  ASSERT_FALSE(irs::analysis::analyzers::normalize(actual, "norm", irs::type<irs::text_format::text>::get(), config));
}

TEST_F(normalizing_token_stream_tests, test_invalid_locale) {
  auto stream = irs::analysis::analyzers::get(
      "norm", irs::type<irs::text_format::json>::get(), "{\"locale\":\"invalid12345.UTF-8\"}");
  ASSERT_EQ(nullptr, stream);
}
