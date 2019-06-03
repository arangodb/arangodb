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

#include "analysis/text_token_stream.hpp"
#include "analysis/token_attributes.hpp"
#include "analysis/token_stream.hpp"
#include "utils/locale_utils.hpp"
#include "utils/runtime_utils.hpp"

NS_LOCAL

std::basic_string<wchar_t> utf_to_utf(const irs::bytes_ref& value) {
  auto locale = irs::locale_utils::locale(irs::string_ref::NIL, "utf8", true); // utf8 internal and external
  std::basic_string<wchar_t> result;

  if (!irs::locale_utils::append_internal<wchar_t>(result, irs::ref_cast<char>(value), locale)) {
    throw irs::illegal_state(); // cannot use ASSERT_TRUE(...) here, therefore throw
  }

  return result;
}

NS_END // NS_LOCAL

namespace tests {
  class TextAnalyzerParserTestSuite: public ::testing::Test {

    virtual void SetUp() {
      // Code here will be called immediately after the constructor (right before each test).
    }

    virtual void TearDown() {
      // Code here will be called immediately after each test (right before the destructor).
    }
  };
}

using namespace tests;
using namespace iresearch::analysis;

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

TEST_F(TextAnalyzerParserTestSuite, test_nbsp_whitespace) {
  irs::analysis::text_token_stream::options_t options;

  options.locale = "C.UTF-8"; // utf8 encoding used bellow

  std::string sField = "test field";
  std::wstring sDataUCS2 = L"1,24\u00A0prosenttia"; // 00A0 == non-breaking whitespace
  auto locale = irs::locale_utils::locale(irs::string_ref::NIL, "utf8", true); // utf8 internal and external
  std::string data;
  ASSERT_TRUE(irs::locale_utils::append_external<wchar_t>(data, sDataUCS2, locale));
  irs::analysis::text_token_stream stream(options);

  ASSERT_TRUE(stream.reset(data));

  auto pStream = &stream;

  ASSERT_NE(nullptr, pStream);

  auto& pOffset = pStream->attributes().get<iresearch::offset>();
  auto& pPayload = pStream->attributes().get<iresearch::payload>();
  auto& pValue = pStream->attributes().get<iresearch::term_attribute>();

  ASSERT_TRUE(pStream->next());
  ASSERT_EQ("1,24", std::string((char*)(pValue->value().c_str()), pValue->value().size()));
  ASSERT_TRUE(pStream->next());
  ASSERT_EQ("prosenttia", std::string((char*)(pValue->value().c_str()), pValue->value().size()));
  ASSERT_FALSE(pStream->next());
}

TEST_F(TextAnalyzerParserTestSuite, test_text_analyzer) {
  std::unordered_set<std::string> emptySet;
  std::string sField = "test field";

  // default behaviour
  {
    irs::analysis::text_token_stream::options_t options;

    options.locale = "en_US.UTF-8";

    std::string data = " A  hErd of   quIck brown  foXes ran    and Jumped over  a     runninG dog";
    irs::analysis::text_token_stream stream(options);

    auto testFunc = [](const irs::string_ref& data, analyzer* pStream) {
      ASSERT_TRUE(pStream->reset(data));

      auto& pOffset = pStream->attributes().get<iresearch::offset>();
      auto& pPayload = pStream->attributes().get<iresearch::payload>();
      auto& pValue = pStream->attributes().get<iresearch::term_attribute>();

      ASSERT_TRUE(pStream->next());
      ASSERT_EQ("a", std::string((char*)(pValue->value().c_str()), pValue->value().size()));
      ASSERT_TRUE(pStream->next());
      ASSERT_EQ("herd", std::string((char*)(pValue->value().c_str()), pValue->value().size()));
      ASSERT_TRUE(pStream->next());
      ASSERT_EQ("of", std::string((char*)(pValue->value().c_str()), pValue->value().size()));
      ASSERT_TRUE(pStream->next());
      ASSERT_EQ("quick", std::string((char*)(pValue->value().c_str()), pValue->value().size()));
      ASSERT_TRUE(pStream->next());
      ASSERT_EQ("brown", std::string((char*)(pValue->value().c_str()), pValue->value().size()));
      ASSERT_TRUE(pStream->next());
      ASSERT_EQ("fox", std::string((char*)(pValue->value().c_str()), pValue->value().size()));
      ASSERT_TRUE(pStream->next());
      ASSERT_EQ("ran", std::string((char*)(pValue->value().c_str()), pValue->value().size()));
      ASSERT_TRUE(pStream->next());
      ASSERT_EQ("and", std::string((char*)(pValue->value().c_str()), pValue->value().size()));
      ASSERT_TRUE(pStream->next());
      ASSERT_EQ("jump", std::string((char*)(pValue->value().c_str()), pValue->value().size()));
      ASSERT_TRUE(pStream->next());
      ASSERT_EQ("over", std::string((char*)(pValue->value().c_str()), pValue->value().size()));
      ASSERT_TRUE(pStream->next());
      ASSERT_EQ("a", std::string((char*)(pValue->value().c_str()), pValue->value().size()));
      ASSERT_TRUE(pStream->next());
      ASSERT_EQ("run", std::string((char*)(pValue->value().c_str()), pValue->value().size()));
      ASSERT_TRUE(pStream->next());
      ASSERT_EQ("dog", std::string((char*)(pValue->value().c_str()), pValue->value().size()));
      ASSERT_FALSE(pStream->next());
    };

    {
      irs::analysis::text_token_stream::options_t options;
      options.locale = "en_US.UTF-8";
      irs::analysis::text_token_stream stream(options);
      testFunc(data, &stream);
    }
    {
      // stopwords  should be set to empty - or default values will interfere with test data
      auto stream = irs::analysis::analyzers::get("text", irs::text_format::json, "{\"locale\":\"en_US.UTF-8\", \"stopwords\":[]}"); 
      ASSERT_NE(nullptr, stream);
      testFunc(data, stream.get());
    }
  }

  // case convert (lower)
  {

    std::string data = "A qUiCk brOwn FoX";
    
    auto testFunc = [](const irs::string_ref& data, analyzer* pStream) {

      ASSERT_TRUE(pStream->reset(data));

      auto& value = pStream->attributes().get<irs::term_attribute>();

      ASSERT_TRUE(pStream->next());
      ASSERT_EQ("a", irs::ref_cast<char>(value->value()));
      ASSERT_TRUE(pStream->next());
      ASSERT_EQ("quick", irs::ref_cast<char>(value->value()));
      ASSERT_TRUE(pStream->next());
      ASSERT_EQ("brown", irs::ref_cast<char>(value->value()));
      ASSERT_TRUE(pStream->next());
      ASSERT_EQ("fox", irs::ref_cast<char>(value->value()));
      ASSERT_FALSE(pStream->next());
    };

    {
      irs::analysis::text_token_stream::options_t options;
      options.case_convert = irs::analysis::text_token_stream::options_t::case_convert_t::LOWER;
      options.locale = "en_US.UTF-8";
      irs::analysis::text_token_stream stream(options);
      testFunc(data, &stream);
    }
    {
      // stopwords  should be set to empty - or default values will interfere with test data
      auto stream = irs::analysis::analyzers::get("text", irs::text_format::json, "{\"locale\":\"en_US.UTF-8\", \"stopwords\":[], \"caseConvert\":\"lower\"}");
      ASSERT_NE(nullptr, stream);
      testFunc(data, stream.get());
    }
  }

  // case convert (upper)
  {
    std::string data = "A qUiCk brOwn FoX";

    auto testFunc = [](const irs::string_ref& data, analyzer* pStream) {
      ASSERT_TRUE(pStream->reset(data));

      auto& value = pStream->attributes().get<irs::term_attribute>();

      ASSERT_TRUE(pStream->next());
      ASSERT_EQ("A", irs::ref_cast<char>(value->value()));
      ASSERT_TRUE(pStream->next());
      ASSERT_EQ("QUICK", irs::ref_cast<char>(value->value()));
      ASSERT_TRUE(pStream->next());
      ASSERT_EQ("BROWN", irs::ref_cast<char>(value->value()));
      ASSERT_TRUE(pStream->next());
      ASSERT_EQ("FOX", irs::ref_cast<char>(value->value()));
      ASSERT_FALSE(pStream->next());
    };

    {
      irs::analysis::text_token_stream::options_t options;
      options.case_convert = irs::analysis::text_token_stream::options_t::case_convert_t::UPPER;
      options.locale = "en_US.UTF-8";
      irs::analysis::text_token_stream stream(options);
      testFunc(data, &stream);
    }
    {
      // stopwords  should be set to empty - or default values will interfere with test data
      auto stream = irs::analysis::analyzers::get("text", irs::text_format::json, "{\"locale\":\"en_US.UTF-8\", \"stopwords\":[], \"caseConvert\":\"upper\"}");
      ASSERT_NE(nullptr, stream);
      testFunc(data, stream.get());
    }
  }

  // case convert (none)
  {
   
    std::string data = "A qUiCk brOwn FoX";

    auto testFunc = [](const irs::string_ref& data, analyzer* pStream) {
      ASSERT_TRUE(pStream->reset(data));

      auto& value = pStream->attributes().get<irs::term_attribute>();

      ASSERT_TRUE(pStream->next());
      ASSERT_EQ("A", irs::ref_cast<char>(value->value()));
      ASSERT_TRUE(pStream->next());
      ASSERT_EQ("qUiCk", irs::ref_cast<char>(value->value()));
      ASSERT_TRUE(pStream->next());
      ASSERT_EQ("brOwn", irs::ref_cast<char>(value->value()));
      ASSERT_TRUE(pStream->next());
      ASSERT_EQ("FoX", irs::ref_cast<char>(value->value()));
      ASSERT_FALSE(pStream->next());
    };

    {
      irs::analysis::text_token_stream::options_t options;
      options.case_convert = irs::analysis::text_token_stream::options_t::case_convert_t::NONE;
      options.locale = "en_US.UTF-8";
      irs::analysis::text_token_stream stream(options);
      testFunc(data, &stream);
    }
    {
      // stopwords  should be set to empty - or default values will interfere with test data
      auto stream = irs::analysis::analyzers::get("text", irs::text_format::json, "{\"locale\":\"en_US.UTF-8\", \"stopwords\":[], \"caseConvert\":\"none\"}");
      ASSERT_NE(nullptr, stream);
      testFunc(data, stream.get());
    }
  }

  // ignored words
  {
   

    std::string data = " A thing of some KIND and ANoTher ";
 
    auto testFunc = [](const irs::string_ref& data, analyzer* pStream) {
      ASSERT_TRUE(pStream->reset(data));
     
      auto& pOffset = pStream->attributes().get<iresearch::offset>();
      auto& pPayload = pStream->attributes().get<iresearch::payload>();
      auto& pValue = pStream->attributes().get<iresearch::term_attribute>();

      ASSERT_TRUE(pStream->next());
      ASSERT_EQ("thing", std::string((char*)(pValue->value().c_str()), pValue->value().size()));
      ASSERT_TRUE(pStream->next());
      ASSERT_EQ("some", std::string((char*)(pValue->value().c_str()), pValue->value().size()));
      ASSERT_TRUE(pStream->next());
      ASSERT_EQ("kind", std::string((char*)(pValue->value().c_str()), pValue->value().size()));
      ASSERT_TRUE(pStream->next());
      ASSERT_EQ("anoth", std::string((char*)(pValue->value().c_str()), pValue->value().size()));
      ASSERT_FALSE(pStream->next());
    };

    {
      irs::analysis::text_token_stream::options_t options;
      options.ignored_words = std::unordered_set<std::string>({ "a", "of", "and" });
      options.locale = "en_US.UTF-8";
      irs::analysis::text_token_stream stream(options);
      testFunc(data, &stream);
    }
    {
      auto stream = irs::analysis::analyzers::get("text", irs::text_format::json, "{\"locale\":\"en_US.UTF-8\", \"stopwords\":[\"a\", \"of\", \"and\"]}");
      ASSERT_NE(nullptr, stream);
      testFunc(data, stream.get());
    }

  }

  // alternate locale
  {
   

    std::wstring sDataUCS2 = L"\u041F\u043E \u0432\u0435\u0447\u0435\u0440\u0430\u043C \u0401\u0436\u0438\u043A \u0445\u043E\u0434\u0438\u043B \u043A \u041C\u0435\u0434\u0432\u0435\u0436\u043E\u043D\u043A\u0443 \u0441\u0447\u0438\u0442\u0430\u0442\u044C \u0437\u0432\u0451\u0437\u0434\u044B";
    auto locale = irs::locale_utils::locale(irs::string_ref::NIL, "utf8", true); // utf8 internal and external
    std::string data;
    ASSERT_TRUE(irs::locale_utils::append_external<wchar_t>(data, sDataUCS2, locale));
   

    auto testFunc = [](const irs::string_ref& data, analyzer* pStream) {
      ASSERT_TRUE(pStream->reset(data));

      auto& pOffset = pStream->attributes().get<iresearch::offset>();
      auto& pPayload = pStream->attributes().get<iresearch::payload>();
      auto& pValue = pStream->attributes().get<iresearch::term_attribute>();

      ASSERT_TRUE(pStream->next());
      ASSERT_EQ(L"\u043F\u043E", utf_to_utf(pValue->value()));
      ASSERT_TRUE(pStream->next());
      ASSERT_EQ(L"\u0432\u0435\u0447\u0435\u0440", utf_to_utf(pValue->value()));
      ASSERT_TRUE(pStream->next());
      ASSERT_EQ(L"\u0435\u0436\u0438\u043A", utf_to_utf(pValue->value()));
      ASSERT_TRUE(pStream->next());
      ASSERT_EQ(L"\u0445\u043E\u0434", utf_to_utf(pValue->value()));
      ASSERT_TRUE(pStream->next());
      ASSERT_EQ(L"\u043A", utf_to_utf(pValue->value()));
      ASSERT_TRUE(pStream->next());
      ASSERT_EQ(L"\u043C\u0435\u0434\u0432\u0435\u0436\u043E\u043D\u043A", utf_to_utf(pValue->value()));
      ASSERT_TRUE(pStream->next());
      ASSERT_EQ(L"\u0441\u0447\u0438\u0442\u0430", utf_to_utf(pValue->value()));
      ASSERT_TRUE(pStream->next());
      ASSERT_EQ(L"\u0437\u0432\u0435\u0437\u0434", utf_to_utf(pValue->value()));
      ASSERT_FALSE(pStream->next());
    };

    {
      irs::analysis::text_token_stream::options_t options;
      options.locale = "ru_RU.UTF-8";
      irs::analysis::text_token_stream stream(options);
      testFunc(data, &stream);
    }
    {
      // stopwords  should be set to empty - or default values will interfere with test data
      auto stream = irs::analysis::analyzers::get("text", irs::text_format::json, "{\"locale\":\"ru_RU.UTF-8\", \"stopwords\":[]}");
      ASSERT_NE(nullptr, stream);
      testFunc(data, stream.get());
    }
  }

  // skip accent removal
  {
   

    std::wstring sDataUCS2 = L"\u041F\u043E \u0432\u0435\u0447\u0435\u0440\u0430\u043C \u0401\u0436\u0438\u043A \u0445\u043E\u0434\u0438\u043B \u043A \u041C\u0435\u0434\u0432\u0435\u0436\u043E\u043D\u043A\u0443 \u0441\u0447\u0438\u0442\u0430\u0442\u044C \u0437\u0432\u0451\u0437\u0434\u044B";
    auto locale = irs::locale_utils::locale(irs::string_ref::NIL, "utf8", true); // utf8 internal and external
    std::string data;
    ASSERT_TRUE(irs::locale_utils::append_external<wchar_t>(data, sDataUCS2, locale));
   
    auto testFunc = [](const irs::string_ref& data, analyzer* pStream) {
      ASSERT_TRUE(pStream->reset(data));

      auto& value = pStream->attributes().get<iresearch::term_attribute>();

      ASSERT_TRUE(pStream->next());
      ASSERT_EQ(L"\u043F\u043E", utf_to_utf(value->value()));
      ASSERT_TRUE(pStream->next());
      ASSERT_EQ(L"\u0432\u0435\u0447\u0435\u0440", utf_to_utf(value->value()));
      ASSERT_TRUE(pStream->next());
      ASSERT_EQ(L"\u0451\u0436\u0438\u043A", utf_to_utf(value->value()));
      ASSERT_TRUE(pStream->next());
      ASSERT_EQ(L"\u0445\u043E\u0434", utf_to_utf(value->value()));
      ASSERT_TRUE(pStream->next());
      ASSERT_EQ(L"\u043A", utf_to_utf(value->value()));
      ASSERT_TRUE(pStream->next());
      ASSERT_EQ(L"\u043C\u0435\u0434\u0432\u0435\u0436\u043E\u043D\u043A", utf_to_utf(value->value()));
      ASSERT_TRUE(pStream->next());
      ASSERT_EQ(L"\u0441\u0447\u0438\u0442\u0430", utf_to_utf(value->value()));
      ASSERT_TRUE(pStream->next());
      ASSERT_EQ(L"\u0437\u0432\u0451\u0437\u0434\u044B", utf_to_utf(value->value())); // for some reason snowball doesn't remove the last letter if accents were not removed
      ASSERT_FALSE(pStream->next());
    };

    {
      irs::analysis::text_token_stream::options_t options;

      options.locale = "ru_RU.UTF-8";
      options.no_accent = false;
      irs::analysis::text_token_stream stream(options);
      testFunc(data, &stream);
    }
    {
      // stopwords  should be set to empty - or default values will interfere with test data
      auto stream = irs::analysis::analyzers::get("text", irs::text_format::json, "{\"locale\":\"ru_RU.UTF-8\", \"stopwords\":[], \"noAccent\":false}");
      ASSERT_NE(nullptr, stream);
      testFunc(data, stream.get());
    }
  }

  // skip stemming
  {
    std::wstring sDataUCS2 = L"\u041F\u043E \u0432\u0435\u0447\u0435\u0440\u0430\u043C \u0401\u0436\u0438\u043A \u0445\u043E\u0434\u0438\u043B \u043A \u041C\u0435\u0434\u0432\u0435\u0436\u043E\u043D\u043A\u0443 \u0441\u0447\u0438\u0442\u0430\u0442\u044C \u0437\u0432\u0451\u0437\u0434\u044B";
    auto locale = irs::locale_utils::locale(irs::string_ref::NIL, "utf8", true); // utf8 internal and external
    std::string data;
    ASSERT_TRUE(irs::locale_utils::append_external<wchar_t>(data, sDataUCS2, locale));
   
    auto testFunc = [](const irs::string_ref& data, analyzer* pStream) {

      ASSERT_TRUE(pStream->reset(data));

      auto& value = pStream->attributes().get<iresearch::term_attribute>();

      ASSERT_TRUE(pStream->next());
      ASSERT_EQ(L"\u043F\u043E", utf_to_utf(value->value()));
      ASSERT_TRUE(pStream->next());
      ASSERT_EQ(L"\u0432\u0435\u0447\u0435\u0440\u0430\u043C", utf_to_utf(value->value()));
      ASSERT_TRUE(pStream->next());
      ASSERT_EQ(L"\u0435\u0436\u0438\u043A", utf_to_utf(value->value()));
      ASSERT_TRUE(pStream->next());
      ASSERT_EQ(L"\u0445\u043E\u0434\u0438\u043B", utf_to_utf(value->value()));
      ASSERT_TRUE(pStream->next());
      ASSERT_EQ(L"\u043A", utf_to_utf(value->value()));
      ASSERT_TRUE(pStream->next());
      ASSERT_EQ(L"\u043C\u0435\u0434\u0432\u0435\u0436\u043E\u043D\u043A\u0443", utf_to_utf(value->value()));
      ASSERT_TRUE(pStream->next());
      ASSERT_EQ(L"\u0441\u0447\u0438\u0442\u0430\u0442\u044C", utf_to_utf(value->value()));
      ASSERT_TRUE(pStream->next());
      ASSERT_EQ(L"\u0437\u0432\u0435\u0437\u0434\u044B", utf_to_utf(value->value()));
      ASSERT_FALSE(pStream->next());
    };

    {
      irs::analysis::text_token_stream::options_t options;
      options.locale = "ru_RU.UTF-8";
      options.no_stem = true;
      irs::analysis::text_token_stream stream(options);
      testFunc(data, &stream);
    }
    {
      // stopwords  should be set to empty - or default values will interfere with test data
      auto stream = irs::analysis::analyzers::get("text", irs::text_format::json, "{\"locale\":\"ru_RU.UTF-8\", \"stopwords\":[], \"noStem\":true}");
      ASSERT_NE(nullptr, stream);
      testFunc(data, stream.get());
    }
  }

  // locale-sensitive case conversion
  {
  

    std::wstring sDataUCS2 = L"\u0130I";
    auto locale = irs::locale_utils::locale(irs::string_ref::NIL, "utf8", true); // utf8 internal and external
    std::string data;
    ASSERT_TRUE(irs::locale_utils::append_external<wchar_t>(data, sDataUCS2, locale));
   

    auto testFunc = [](const irs::string_ref& data, analyzer* pStream) {
      ASSERT_TRUE(pStream->reset(data));

      auto& pOffset = pStream->attributes().get<iresearch::offset>();
      auto& pPayload = pStream->attributes().get<iresearch::payload>();
      auto& pValue = pStream->attributes().get<iresearch::term_attribute>();

      ASSERT_TRUE(pStream->next());
      ASSERT_EQ(L"i\u0131", utf_to_utf(pValue->value()));
      ASSERT_FALSE(pStream->next());
    };

    {
      irs::analysis::text_token_stream::options_t options;
      options.locale = "tr-TR.UTF-8";
      irs::analysis::text_token_stream stream(options);
      testFunc(data, &stream);
    }
    {
      // stopwords  should be set to empty - or default values will interfere with test data
      auto stream = irs::analysis::analyzers::get("text", irs::text_format::json, "{\"locale\":\"tr-TR.UTF-8\", \"stopwords\":[]}");
      ASSERT_NE(nullptr, stream);
      testFunc(data, stream.get());
    }
  }

  {
    // there is no Snowball stemmer for Chinese
   
    std::wstring sDataUCS2 = L"\u4ECA\u5929\u4E0B\u5348\u7684\u592A\u9633\u5F88\u6E29\u6696\u3002";
    auto locale = irs::locale_utils::locale(irs::string_ref::NIL, "utf8", true); // utf8 internal and external
    std::string data;
    ASSERT_TRUE(irs::locale_utils::append_external<wchar_t>(data, sDataUCS2, locale));
 

    auto testFunc = [](const irs::string_ref& data, analyzer* pStream) {
      ASSERT_TRUE(pStream->reset(data));

      auto& pOffset = pStream->attributes().get<iresearch::offset>();
      auto& pPayload = pStream->attributes().get<iresearch::payload>();
      auto& pValue = pStream->attributes().get<iresearch::term_attribute>();

      ASSERT_TRUE(pStream->next());
      ASSERT_EQ(L"\u4ECA\u5929", utf_to_utf(pValue->value()));
      ASSERT_TRUE(pStream->next());
      ASSERT_EQ(L"\u4E0B\u5348", utf_to_utf(pValue->value()));
      ASSERT_TRUE(pStream->next());
      ASSERT_EQ(L"\u7684", utf_to_utf(pValue->value()));
      ASSERT_TRUE(pStream->next());
      ASSERT_EQ(L"\u592A\u9633", utf_to_utf(pValue->value()));
      ASSERT_TRUE(pStream->next());
      ASSERT_EQ(L"\u5F88", utf_to_utf(pValue->value()));
      ASSERT_TRUE(pStream->next());
      ASSERT_EQ(L"\u6E29\u6696", utf_to_utf(pValue->value()));
      ASSERT_FALSE(pStream->next());
    };

    {
      irs::analysis::text_token_stream::options_t options;
      options.locale = "zh_CN.UTF-8";
      irs::analysis::text_token_stream stream(options);
      testFunc(data, &stream);
    }
    {
      // stopwords  should be set to empty - or default values will interfere with test data
      auto stream = irs::analysis::analyzers::get("text", irs::text_format::json, "{\"locale\":\"zh_CN.UTF-8\", \"stopwords\":[]}");
      ASSERT_NE(nullptr, stream);
      testFunc(data, stream.get());
    }
  }

  // ...........................................................................
  // invalid
  // ...........................................................................
  {
    std::string data = "abc";
    {
      // ICU locale will fail initialization for an invalid std:locale
      irs::analysis::text_token_stream::options_t options;
      options.locale = "invalid12345.UTF-8";
      irs::analysis::text_token_stream stream(options);
      ASSERT_FALSE(stream.reset(data));
    }
    {
      // stopwords  should be set to empty - or default values will interfere with test data
      auto stream = irs::analysis::analyzers::get("text", irs::text_format::json, "{\"locale\":\"invalid12345.UTF-8\", \"stopwords\":[]}");
      ASSERT_NE(nullptr, stream);
      ASSERT_FALSE(stream->reset(data));
    }
  }
}

TEST_F(TextAnalyzerParserTestSuite, test_load_stopwords) {
  std::unordered_set<std::string> emptySet;
  std::string sField = "test field";
  const char* czOldStopwordPath = iresearch::getenv(text_token_stream::STOPWORD_PATH_ENV_VARIABLE);
  std::string sOldStopwordPath = czOldStopwordPath == nullptr ? "" : czOldStopwordPath;
  auto reset_stopword_path = irs::make_finally([czOldStopwordPath, sOldStopwordPath]()->void {
    if (czOldStopwordPath) {
      irs::setenv(text_token_stream::STOPWORD_PATH_ENV_VARIABLE, sOldStopwordPath.c_str(), true);
    }
  });

  iresearch::setenv(text_token_stream::STOPWORD_PATH_ENV_VARIABLE, IResearch_test_resource_dir, true);

  {
    auto locale = "en_US.UTF-8";
    std::string sDataASCII = "A E I O U";

    auto testFunc = [](const irs::string_ref& data, analyzer::ptr pStream) {
      ASSERT_TRUE(pStream->reset(data));
      auto& pOffset = pStream->attributes().get<iresearch::offset>();
      auto& pPayload = pStream->attributes().get<iresearch::payload>();
      auto& pValue = pStream->attributes().get<iresearch::term_attribute>();

      ASSERT_TRUE(pStream->next());
      ASSERT_EQ("e", std::string((char*)(pValue->value().c_str()), pValue->value().size()));
      ASSERT_TRUE(pStream->next());
      ASSERT_EQ("u", std::string((char*)(pValue->value().c_str()), pValue->value().size()));
      ASSERT_FALSE(pStream->next());
    };

    {
      auto stream = text_token_stream::make(locale);
      ASSERT_NE(nullptr, stream);
      testFunc(sDataASCII, stream);
    }
    {

      auto stream = irs::analysis::analyzers::get("text", irs::text_format::json, "{\"locale\":\"en_US.UTF-8\"}");
      ASSERT_NE(nullptr, stream);
      testFunc(sDataASCII, stream);
    }
   
  }

  // ...........................................................................
  // invalid
  // ...........................................................................
  std::string sDataASCII = "abc";
  {
    {
      // load stopwords for the 'C' locale that does not have stopwords defined in tests
      auto locale = std::locale::classic().name();

      auto stream = text_token_stream::make(locale);
      auto pStream = stream.get();

      ASSERT_EQ(nullptr, pStream);
    }
    {
      auto stream = irs::analysis::analyzers::get("text", irs::text_format::json, "{\"locale\":\"C\"}");
      ASSERT_EQ(nullptr, stream);
    }
  }
}

TEST_F(TextAnalyzerParserTestSuite, test_load_stopwords_path_override) {
  std::string sDataASCII = "A E I O U";

  auto testFunc = [](const irs::string_ref& data, analyzer::ptr pStream) {
    ASSERT_TRUE(pStream->reset(data));
    auto& pOffset = pStream->attributes().get<iresearch::offset>();
    auto& pPayload = pStream->attributes().get<iresearch::payload>();
    auto& pValue = pStream->attributes().get<iresearch::term_attribute>();

    ASSERT_TRUE(pStream->next());
    ASSERT_EQ("e", std::string((char*)(pValue->value().c_str()), pValue->value().size()));
    ASSERT_TRUE(pStream->next());
    ASSERT_EQ("u", std::string((char*)(pValue->value().c_str()), pValue->value().size()));
    ASSERT_FALSE(pStream->next());
  };

  const char* czOldStopwordPath = iresearch::getenv(text_token_stream::STOPWORD_PATH_ENV_VARIABLE);
  std::string sOldStopwordPath = czOldStopwordPath == nullptr ? "" : czOldStopwordPath;
  auto reset_stopword_path = irs::make_finally([czOldStopwordPath, sOldStopwordPath]()->void {
    if (czOldStopwordPath) {
      irs::setenv(text_token_stream::STOPWORD_PATH_ENV_VARIABLE, sOldStopwordPath.c_str(), true);
    }
  });

  iresearch::setenv(text_token_stream::STOPWORD_PATH_ENV_VARIABLE, "some invalid path", true);

  // overriding ignored words path
  auto stream = irs::analysis::analyzers::get("text", irs::text_format::json, "{\"locale\":\"en_US.UTF-8\", \"stopwordsPath\":\"" IResearch_test_resource_dir "\"}");
  ASSERT_NE(nullptr, stream);
  testFunc(sDataASCII, stream);
}
// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------