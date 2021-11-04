////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2021 ArangoDB GmbH, Cologne, Germany
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
/// @author Alex Geenen
/// @author Andrey Abramov
////////////////////////////////////////////////////////////////////////////////

#include "tests_shared.hpp"

#include "analysis/nearest_neighbors_stream.hpp"

#include "velocypack/Parser.h"
#include "velocypack/velocypack-aliases.h"

namespace {

std::string_view EXPECTED_MODEL;

irs::analysis::nearest_neighbors_stream::model_ptr null_provider(std::string_view model) {
  EXPECT_EQ(EXPECTED_MODEL, model);
  return nullptr;
}

irs::analysis::nearest_neighbors_stream::model_ptr throwing_provider(std::string_view model) {
  EXPECT_EQ(EXPECTED_MODEL, model);
  throw std::exception();
}

}

TEST(nearest_neighbors_stream_test, consts) {
  static_assert("nearest_neighbors" == irs::type<irs::analysis::nearest_neighbors_stream>::name());
}

TEST(nearest_neighbors_stream_test, test_custom_provider) {
#ifdef  WIN32
    const auto model_loc = test_base::resource("model_cooking.bin").generic_string();
#else
    const auto model_loc = test_base::resource("model_cooking.bin").u8string();
#endif
  EXPECTED_MODEL = model_loc;

  const auto input_json = "{\"model_location\": \"" + model_loc + "\", \"top_k\": 2}";

  ASSERT_EQ(nullptr, irs::analysis::nearest_neighbors_stream::set_model_provider(&::null_provider));
  ASSERT_EQ(nullptr, irs::analysis::analyzers::get("nearest_neighbors",
                                                   irs::type<irs::text_format::json>::get(),
                                                   input_json));

  ASSERT_EQ(&::null_provider, irs::analysis::nearest_neighbors_stream::set_model_provider(&::throwing_provider));
  ASSERT_EQ(nullptr, irs::analysis::analyzers::get("nearest_neighbors",
                                                   irs::type<irs::text_format::json>::get(),
                                                   input_json));

  ASSERT_EQ(&::throwing_provider, irs::analysis::nearest_neighbors_stream::set_model_provider(nullptr));
}

TEST(nearest_neighbors_stream_test, test_load) {
  // load json string
  {
#ifdef  WIN32
    auto model_loc = test_base::resource("model_cooking.bin").generic_string();
#else
    auto model_loc = test_base::resource("model_cooking.bin").u8string();
#endif
    irs::string_ref data{"salt"};
    auto input_json = "{\"model_location\": \"" + model_loc + "\"}";
    auto stream = irs::analysis::analyzers::get("nearest_neighbors", irs::type<irs::text_format::json>::get(), input_json);

    ASSERT_NE(nullptr, stream);
    ASSERT_FALSE(stream->next());
    ASSERT_TRUE(stream->reset(data));

    auto* offset = irs::get<irs::offset>(*stream);
    ASSERT_NE(nullptr, offset);
    auto* term = irs::get<irs::term_attribute>(*stream);
    ASSERT_NE(nullptr, term);
    auto* inc = irs::get<irs::increment>(*stream);
    ASSERT_NE(nullptr, inc);

    ASSERT_TRUE(stream->next());
    ASSERT_EQ(1, inc->value);
    ASSERT_EQ(0, offset->start);
    ASSERT_EQ(4, offset->end);
    ASSERT_EQ("homogenized", irs::ref_cast<char>(term->value));
    ASSERT_FALSE(stream->next());
    ASSERT_FALSE(stream->next());
  }

  {
#ifdef  WIN32
    auto model_loc = test_base::resource("model_cooking.bin").generic_string();
#else
    auto model_loc = test_base::resource("model_cooking.bin").u8string();
#endif
    auto input_json = "{\"model_location\": \"" + model_loc + "\", \"top_k\": 2}";
    auto stream = irs::analysis::analyzers::get("nearest_neighbors", irs::type<irs::text_format::json>::get(), input_json);

    ASSERT_NE(nullptr, stream);
    ASSERT_FALSE(stream->next());
    ASSERT_FALSE(stream->next());

    auto* offset = irs::get<irs::offset>(*stream);
    ASSERT_NE(nullptr, offset);
    auto* term = irs::get<irs::term_attribute>(*stream);
    ASSERT_NE(nullptr, term);
    auto* inc = irs::get<irs::increment>(*stream);
    ASSERT_NE(nullptr, inc);

    ASSERT_TRUE(stream->reset("salt"));
    ASSERT_TRUE(stream->next());
    ASSERT_EQ(1, inc->value);
    ASSERT_EQ(0, offset->start);
    ASSERT_EQ(4, offset->end);
    ASSERT_EQ("homogenized", irs::ref_cast<char>(term->value));
    ASSERT_TRUE(stream->next());
    ASSERT_EQ(0, inc->value);
    ASSERT_EQ(0, offset->start);
    ASSERT_EQ(4, offset->end);
    ASSERT_EQ("teach", irs::ref_cast<char>(term->value));
    ASSERT_FALSE(stream->next());
    ASSERT_FALSE(stream->next());

    ASSERT_TRUE(stream->reset("pizza"));
    ASSERT_TRUE(stream->next());
    ASSERT_EQ(1, inc->value);
    ASSERT_EQ(0, offset->start);
    ASSERT_EQ(5, offset->end);
    ASSERT_EQ("\"prepared\"", irs::ref_cast<char>(term->value));
    ASSERT_TRUE(stream->next());
    ASSERT_EQ(0, offset->start);
    ASSERT_EQ(5, offset->end);
    ASSERT_EQ(0, inc->value);
    ASSERT_EQ("tinfoil", irs::ref_cast<char>(term->value));
    ASSERT_FALSE(stream->next());
    ASSERT_FALSE(stream->next());
  }

  // test longer string
  {
#ifdef  WIN32
    auto model_loc = test_base::resource("model_cooking.bin").generic_string();
#else
    auto model_loc = test_base::resource("model_cooking.bin").u8string();
#endif
    irs::string_ref data{"salt oil"};
    auto input_json = "{\"model_location\": \"" + model_loc + "\", \"top_k\": 2}";
    auto stream = irs::analysis::analyzers::get("nearest_neighbors", irs::type<irs::text_format::json>::get(), input_json);

    ASSERT_NE(nullptr, stream);
    ASSERT_FALSE(stream->next());
    ASSERT_TRUE(stream->reset(data));

    auto* offset = irs::get<irs::offset>(*stream);
    ASSERT_NE(nullptr, offset);
    auto* term = irs::get<irs::term_attribute>(*stream);
    ASSERT_NE(nullptr, term);
    auto* inc = irs::get<irs::increment>(*stream);
    ASSERT_NE(nullptr, inc);

    ASSERT_TRUE(stream->next());
    ASSERT_EQ(1, inc->value);
    ASSERT_EQ(0, offset->start);
    ASSERT_EQ(8, offset->end);
    ASSERT_EQ("homogenized", irs::ref_cast<char>(term->value));
    ASSERT_TRUE(stream->next());
    ASSERT_EQ(0, inc->value);
    ASSERT_EQ("teach", irs::ref_cast<char>(term->value));
    ASSERT_TRUE(stream->next());
    ASSERT_EQ(1, inc->value);
    ASSERT_EQ("tube\"", irs::ref_cast<char>(term->value));
    ASSERT_TRUE(stream->next());
    ASSERT_EQ(0, inc->value);
    ASSERT_EQ("\"breather", irs::ref_cast<char>(term->value));
    ASSERT_FALSE(stream->next());
    ASSERT_FALSE(stream->next());
  }

  // failing cases
  {
#ifdef  WIN32
    auto model_loc = test_base::resource("model_cooking.bin").generic_string();
#else
    auto model_loc = test_base::resource("model_cooking.bin").u8string();
#endif

    ASSERT_EQ(nullptr, irs::analysis::analyzers::get("nearest_neighbors",
                                                     irs::type<irs::text_format::json>::get(),
                                                     R"([])"));
    ASSERT_EQ(nullptr, irs::analysis::analyzers::get("nearest_neighbors",
                                                     irs::type<irs::text_format::json>::get(),
                                                     R"({"model_location": "invalid_localtion" })"));
    ASSERT_EQ(nullptr, irs::analysis::analyzers::get("nearest_neighbors",
                                                     irs::type<irs::text_format::json>::get(),
                                                     R"({"model_location": bool })"));
    ASSERT_EQ(nullptr, irs::analysis::analyzers::get("nearest_neighbors",
                                                     irs::type<irs::text_format::json>::get(),
                                                     R"({"model_location": {} })"));
    ASSERT_EQ(nullptr, irs::analysis::analyzers::get("nearest_neighbors",
                                                     irs::type<irs::text_format::json>::get(),
                                                     R"({"model_location": 42 })"));
    ASSERT_EQ(nullptr, irs::analysis::analyzers::get("nearest_neighbors",
                                                     irs::type<irs::text_format::json>::get(),
                                                     "{\"model_location\": \"" + model_loc + "\", \"top_k\": false}"));
    ASSERT_EQ(nullptr, irs::analysis::analyzers::get("nearest_neighbors",
                                                     irs::type<irs::text_format::json>::get(),
                                                     "{\"model_location\": \"" + model_loc + "\", \"top_k\": 2147483648}"));
  }
}


TEST(nearest_neighbors_stream_test, test_make_config_json) {
  // random extra param
  {
#ifdef  WIN32
    auto model_loc = test_base::resource("model_cooking.bin").generic_string();
#else
    auto model_loc = test_base::resource("model_cooking.bin").u8string();
#endif
    std::string config = "{\"model_location\": \"" + model_loc + "\", \"not_valid\": false}";
    std::string expected_conf = "{\"model_location\": \"" + model_loc + "\", \"top_k\": 1}";
    std::string actual;
    ASSERT_TRUE(irs::analysis::analyzers::normalize(actual, "nearest_neighbors", irs::type<irs::text_format::json>::get(), config));
    ASSERT_EQ(VPackParser::fromJson(expected_conf)->toString(), actual);
  }

  // test top k
  {
#ifdef  WIN32
    auto model_loc = test_base::resource("model_cooking.bin").generic_string();
#else
    auto model_loc = test_base::resource("model_cooking.bin").u8string();
#endif
    std::string config = "{\"model_location\": \"" + model_loc + "\", \"top_k\": 2}";
    std::string expected_conf = "{\"model_location\": \"" + model_loc + "\", \"top_k\": 2}";
    std::string actual;
    ASSERT_TRUE(irs::analysis::analyzers::normalize(actual, "nearest_neighbors", irs::type<irs::text_format::json>::get(), config));
    ASSERT_EQ(VPackParser::fromJson(expected_conf)->toString(), actual);
  }

  // test VPack
  {
#ifdef  WIN32
    auto model_loc = test_base::resource("model_cooking.bin").generic_string();
#else
    auto model_loc = test_base::resource("model_cooking.bin").u8string();
#endif
    std::string config = "{\"model_location\":\"" + model_loc + "\", \"not_valid\": false}";
    auto expected_conf = "{\"model_location\": \"" + model_loc + "\", \"top_k\": 1}";
    auto in_vpack = VPackParser::fromJson(config);
    std::string in_str;
    in_str.assign(in_vpack->slice().startAs<char>(), in_vpack->slice().byteSize());
    std::string out_str;
    ASSERT_TRUE(irs::analysis::analyzers::normalize(out_str, "nearest_neighbors", irs::type<irs::text_format::vpack>::get(), in_str));
    VPackSlice out_slice(reinterpret_cast<const uint8_t*>(out_str.c_str()));
    ASSERT_EQ(VPackParser::fromJson(expected_conf)->toString(), out_slice.toString());
  }

  // failing cases
  {
    std::string out;
#ifdef  WIN32
    auto model_loc = test_base::resource("model_cooking.bin").generic_string();
#else
    auto model_loc = test_base::resource("model_cooking.bin").u8string();
#endif

    ASSERT_FALSE(irs::analysis::analyzers::normalize(
      out, "nearest_neighbors",
      irs::type<irs::text_format::json>::get(),
      R"([])"));
    ASSERT_FALSE(irs::analysis::analyzers::normalize(
      out, "nearest_neighbors",
      irs::type<irs::text_format::json>::get(),
      R"({"model_location": bool })"));
    ASSERT_FALSE(irs::analysis::analyzers::normalize(
      out, "nearest_neighbors",
      irs::type<irs::text_format::json>::get(),
      R"({"model_location": {} })"));
    ASSERT_FALSE(irs::analysis::analyzers::normalize(
      out, "nearest_neighbors",
      irs::type<irs::text_format::json>::get(),
      R"({"model_location": 42 })"));
    ASSERT_FALSE(irs::analysis::analyzers::normalize(
      out, "nearest_neighbors",
      irs::type<irs::text_format::json>::get(),
      "{\"model_location\": \"" + model_loc + "\", \"top_k\": false}"));
    ASSERT_FALSE(irs::analysis::analyzers::normalize(
      out, "nearest_neighbors",
      irs::type<irs::text_format::json>::get(),
      "{\"model_location\": \"" + model_loc + "\", \"top_k\": 2147483648}"));
  }
}

