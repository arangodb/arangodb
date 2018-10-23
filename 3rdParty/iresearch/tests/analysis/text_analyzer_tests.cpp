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

#if defined (__GNUC__)
  #pragma GCC diagnostic push
  #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

  #include <boost/locale.hpp>

#if defined (__GNUC__)
  #pragma GCC diagnostic pop
#endif

#include "analysis/text_token_stream.hpp"
#include "analysis/token_attributes.hpp"
#include "analysis/token_stream.hpp"
#include "utils/locale_utils.hpp"
#include "utils/runtime_utils.hpp"

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
  std::unordered_set<std::string> emptySet;
  auto locale = irs::locale_utils::locale("C.UTF-8"); // utf8 encoding used bellow
  std::string sField = "test field";
  std::wstring sDataUCS2 = L"1,24\u00A0prosenttia"; // 00A0 == non-breaking whitespace
  std::string data(boost::locale::conv::utf_to_utf<char>(sDataUCS2));
  text_token_stream stream(locale, emptySet);

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

  {
    auto locale = irs::locale_utils::locale("en_US.UTF-8");
    std::string sDataASCII = " A  hErd of   quIck brown  foXes ran    and Jumped over  a     runninG dog";
    std::string data(sDataASCII);
    text_token_stream stream(locale, emptySet);

    ASSERT_TRUE(stream.reset(data));

    auto pStream = &stream;

    ASSERT_NE(nullptr, pStream);

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
    ASSERT_TRUE( pStream->next());
    ASSERT_EQ("over", std::string((char*)(pValue->value().c_str()), pValue->value().size()));
    ASSERT_TRUE(pStream->next());
    ASSERT_EQ("a", std::string((char*)(pValue->value().c_str()), pValue->value().size()));
    ASSERT_TRUE(pStream->next());
    ASSERT_EQ("run", std::string((char*)(pValue->value().c_str()), pValue->value().size()));
    ASSERT_TRUE(pStream->next());
    ASSERT_EQ("dog", std::string((char*)(pValue->value().c_str()), pValue->value().size()));
    ASSERT_FALSE(pStream->next());
  }

  {
    std::unordered_set<std::string> stopwordSet = { "a", "of", "and" };
    auto locale = irs::locale_utils::locale("en_US.UTF-8");
    std::string sDataASCII = " A thing of some KIND and ANoTher ";
    std::string data(sDataASCII);
    text_token_stream stream(locale, stopwordSet);

    ASSERT_TRUE(stream.reset(data));

    auto pStream = &stream;

    ASSERT_NE(nullptr, pStream);

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
  }

  {
    auto locale = irs::locale_utils::locale("ru_RU.UTF-8");
    std::wstring sDataUCS2 = L"\u041F\u043E \u0432\u0435\u0447\u0435\u0440\u0430\u043C \u0401\u0436\u0438\u043A \u0445\u043E\u0434\u0438\u043B \u043A \u041C\u0435\u0434\u0432\u0435\u0436\u043E\u043D\u043A\u0443 \u0441\u0447\u0438\u0442\u0430\u0442\u044C \u0437\u0432\u0451\u0437\u0434\u044B";
    std::string data(boost::locale::conv::utf_to_utf<char>(sDataUCS2));
    text_token_stream stream(locale, emptySet);

    ASSERT_TRUE(stream.reset(data));

    auto pStream = &stream;

    ASSERT_NE(nullptr, pStream);

    auto& pOffset = pStream->attributes().get<iresearch::offset>();
    auto& pPayload = pStream->attributes().get<iresearch::payload>();
    auto& pValue = pStream->attributes().get<iresearch::term_attribute>();

    ASSERT_TRUE(pStream->next());
    ASSERT_EQ(L"\u043F\u043E", boost::locale::conv::utf_to_utf<wchar_t>(pValue->value().c_str(), pValue->value().c_str() + pValue->value().size()));
    ASSERT_TRUE(pStream->next());
    ASSERT_EQ(L"\u0432\u0435\u0447\u0435\u0440", boost::locale::conv::utf_to_utf<wchar_t>(pValue->value().c_str(), pValue->value().c_str() + pValue->value().size()));
    ASSERT_TRUE(pStream->next());
    ASSERT_EQ(L"\u0435\u0436\u0438\u043A", boost::locale::conv::utf_to_utf<wchar_t>(pValue->value().c_str(), pValue->value().c_str() + pValue->value().size()));
    ASSERT_TRUE( pStream->next());
    ASSERT_EQ(L"\u0445\u043E\u0434", boost::locale::conv::utf_to_utf<wchar_t>(pValue->value().c_str(), pValue->value().c_str() + pValue->value().size()));
    ASSERT_TRUE(pStream->next());
    ASSERT_EQ(L"\u043A", boost::locale::conv::utf_to_utf<wchar_t>(pValue->value().c_str(), pValue->value().c_str() + pValue->value().size()));
    ASSERT_TRUE(pStream->next());
    ASSERT_EQ(L"\u043C\u0435\u0434\u0432\u0435\u0436\u043E\u043D\u043A", boost::locale::conv::utf_to_utf<wchar_t>(pValue->value().c_str(), pValue->value().c_str() + pValue->value().size()));
    ASSERT_TRUE(pStream->next());
    ASSERT_EQ(L"\u0441\u0447\u0438\u0442\u0430", boost::locale::conv::utf_to_utf<wchar_t>(pValue->value().c_str(), pValue->value().c_str() + pValue->value().size()));
    ASSERT_TRUE(pStream->next());
    ASSERT_EQ(L"\u0437\u0432\u0435\u0437\u0434", boost::locale::conv::utf_to_utf<wchar_t>(pValue->value().c_str(), pValue->value().c_str() + pValue->value().size()));
    ASSERT_FALSE(pStream->next());
  }

  {
    auto locale = irs::locale_utils::locale("tr-TR.UTF-8");
    std::wstring sDataUCS2 = L"\u0130I";
    std::string data(boost::locale::conv::utf_to_utf<char>(sDataUCS2));
    text_token_stream stream(locale, emptySet);

    ASSERT_TRUE(stream.reset(data));

    auto pStream = &stream;

    ASSERT_NE(nullptr, pStream);

    auto& pOffset = pStream->attributes().get<iresearch::offset>();
    auto& pPayload = pStream->attributes().get<iresearch::payload>();
    auto& pValue = pStream->attributes().get<iresearch::term_attribute>();

    ASSERT_TRUE(pStream->next());
    ASSERT_EQ(L"i\u0131", boost::locale::conv::utf_to_utf<wchar_t>(pValue->value().c_str(), pValue->value().c_str() + pValue->value().size()));
    ASSERT_FALSE(pStream->next());
  }

  {
    // there is no Snowball stemmer for Chinese
    auto locale = irs::locale_utils::locale("zh_CN.UTF-8");
    std::wstring sDataUCS2 = L"\u4ECA\u5929\u4E0B\u5348\u7684\u592A\u9633\u5F88\u6E29\u6696\u3002";
    std::string data(boost::locale::conv::utf_to_utf<char>(sDataUCS2));
    text_token_stream stream(locale, emptySet);

    ASSERT_TRUE(stream.reset(data));

    auto pStream = &stream;

    ASSERT_NE(nullptr, pStream);

    auto& pOffset = pStream->attributes().get<iresearch::offset>();
    auto& pPayload = pStream->attributes().get<iresearch::payload>();
    auto& pValue = pStream->attributes().get<iresearch::term_attribute>();

    ASSERT_TRUE(pStream->next());
    ASSERT_EQ(L"\u4ECA\u5929", boost::locale::conv::utf_to_utf<wchar_t>(pValue->value().c_str(), pValue->value().c_str() + pValue->value().size()));
    ASSERT_TRUE(pStream->next());
    ASSERT_EQ(L"\u4E0B\u5348", boost::locale::conv::utf_to_utf<wchar_t>(pValue->value().c_str(), pValue->value().c_str() + pValue->value().size()));
    ASSERT_TRUE(pStream->next());
    ASSERT_EQ(L"\u7684", boost::locale::conv::utf_to_utf<wchar_t>(pValue->value().c_str(), pValue->value().c_str() + pValue->value().size()));
    ASSERT_TRUE( pStream->next());
    ASSERT_EQ(L"\u592A\u9633", boost::locale::conv::utf_to_utf<wchar_t>(pValue->value().c_str(), pValue->value().c_str() + pValue->value().size()));
    ASSERT_TRUE(pStream->next());
    ASSERT_EQ(L"\u5F88", boost::locale::conv::utf_to_utf<wchar_t>(pValue->value().c_str(), pValue->value().c_str() + pValue->value().size()));
    ASSERT_TRUE(pStream->next());
    ASSERT_EQ(L"\u6E29\u6696", boost::locale::conv::utf_to_utf<wchar_t>(pValue->value().c_str(), pValue->value().c_str() + pValue->value().size()));
    ASSERT_FALSE(pStream->next());
  }

  // ...........................................................................
  // invalid
  // ...........................................................................

  {
    // ICU locale will fail initialization for an invalid std:locale
    auto locale = irs::locale_utils::locale("invalid12345.UTF-8");
    std::string sDataASCII = "abc";
    std::string data(sDataASCII);
    text_token_stream stream(locale, emptySet);

    ASSERT_FALSE(stream.reset(data));
  }
}

TEST_F(TextAnalyzerParserTestSuite, test_load_stopwords) {
  std::unordered_set<std::string> emptySet;
  std::string sField = "test field";
  const char* czOldStopwordPath = iresearch::getenv(text_token_stream::STOPWORD_PATH_ENV_VARIABLE);
  std::string sOldStopwordPath = czOldStopwordPath == nullptr ? "" : czOldStopwordPath;

  iresearch::setenv(text_token_stream::STOPWORD_PATH_ENV_VARIABLE, IResearch_test_resource_dir, true);

  {
    auto locale = irs::locale_utils::locale("en_US.UTF-8");
    std::string sDataASCII = "A E I O U";
    auto stream = text_token_stream::make(locale);

    ASSERT_TRUE(stream->reset(sDataASCII));

    auto pStream = stream.get();

    ASSERT_NE(nullptr, pStream);

    auto& pOffset = pStream->attributes().get<iresearch::offset>();
    auto& pPayload = pStream->attributes().get<iresearch::payload>();
    auto& pValue = pStream->attributes().get<iresearch::term_attribute>();

    ASSERT_TRUE(pStream->next());
    ASSERT_EQ("e", std::string((char*)(pValue->value().c_str()), pValue->value().size()));
    ASSERT_TRUE(pStream->next());
    ASSERT_EQ("u", std::string((char*)(pValue->value().c_str()), pValue->value().size()));
    ASSERT_FALSE(pStream->next());
  }

  // ...........................................................................
  // invalid
  // ...........................................................................

  {
    // load stopwords for the 'C' locale that does not have stopwords defined in tests
    std::locale locale = std::locale::classic();
    std::string sDataASCII = "abc";
    auto stream = text_token_stream::make(locale);
    auto pStream = stream.get();

    ASSERT_EQ(nullptr, pStream);
  }

  if (czOldStopwordPath) {
    iresearch::setenv(text_token_stream::STOPWORD_PATH_ENV_VARIABLE, sOldStopwordPath.c_str(), true);
  }
}
// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------