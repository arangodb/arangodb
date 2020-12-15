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
#include "utils/utf8_path.hpp"

#include <rapidjson/document.h> // for rapidjson::Document, rapidjson::Value

namespace {

std::basic_string<wchar_t> utf_to_utf(const irs::bytes_ref& value) {
  auto locale = irs::locale_utils::locale(irs::string_ref::NIL, "utf8", true); // utf8 internal and external
  std::basic_string<wchar_t> result;

  if (!irs::locale_utils::append_internal<wchar_t>(result, irs::ref_cast<char>(value), locale)) {
    throw irs::illegal_state(); // cannot use ASSERT_TRUE(...) here, therefore throw
  }

  return result;
}

} // namespace {

namespace tests {

class TextAnalyzerParserTestSuite : public ::testing::Test {
 protected:
  void SetStopwordsPath(const char* path) {
    stopwords_path_set_ = true;
    old_stopwords_path_set_ = false;

    const char* old_stopwords_path = irs::getenv(irs::analysis::text_token_stream::STOPWORD_PATH_ENV_VARIABLE);
    if (old_stopwords_path) {
      old_stopwords_path_ = old_stopwords_path;
      old_stopwords_path_set_ = true;
    }

    if (path) {
      irs::setenv(irs::analysis::text_token_stream::STOPWORD_PATH_ENV_VARIABLE, path, true);
    } else {
      irs::unsetenv(irs::analysis::text_token_stream::STOPWORD_PATH_ENV_VARIABLE);
      ASSERT_EQ(nullptr, irs::getenv(irs::analysis::text_token_stream::STOPWORD_PATH_ENV_VARIABLE));
    }
  }

  virtual void SetUp() {
    irs::analysis::text_token_stream::clear_cache();
  }

  virtual void TearDown() {
    if (stopwords_path_set_) {
      if (old_stopwords_path_set_) {
        irs::setenv(irs::analysis::text_token_stream::STOPWORD_PATH_ENV_VARIABLE, old_stopwords_path_.c_str(), true);
      } else {
        irs::unsetenv(irs::analysis::text_token_stream::STOPWORD_PATH_ENV_VARIABLE);
      }
    }
  }

 private:
  std::string old_stopwords_path_;
  bool old_stopwords_path_set_{false};
  bool stopwords_path_set_{false};
};

}

using namespace tests;
using namespace irs::analysis;

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

TEST_F(TextAnalyzerParserTestSuite, consts) {
  static_assert("text" == irs::type<irs::analysis::text_token_stream>::name());
}

TEST_F(TextAnalyzerParserTestSuite, test_nbsp_whitespace) {
  irs::analysis::text_token_stream::options_t options;

  options.locale = irs::locale_utils::locale("C.UTF-8"); // utf8 encoding used bellow

  std::string sField = "test field";
  std::wstring sDataUCS2 = L"1,24\u00A0prosenttia"; // 00A0 == non-breaking whitespace
  auto locale = irs::locale_utils::locale(irs::string_ref::NIL, "utf8", true); // utf8 internal and external
  std::string data;
  ASSERT_TRUE(irs::locale_utils::append_external<wchar_t>(data, sDataUCS2, locale));
  irs::analysis::text_token_stream stream(options, options.explicit_stopwords);
  ASSERT_EQ(irs::type<irs::analysis::text_token_stream >::id(), stream.type());

  ASSERT_TRUE(stream.reset(data));

  auto pStream = &stream;

  ASSERT_NE(nullptr, pStream);

  auto* pOffset = irs::get<irs::offset>(*pStream);
  ASSERT_NE(nullptr, pOffset);
  auto* pInc = irs::get<irs::increment>(*pStream);
  ASSERT_NE(nullptr, pInc);
  auto* pPayload = irs::get<irs::payload>(*pStream);
  ASSERT_EQ(nullptr, pPayload);
  auto* pValue = irs::get<irs::term_attribute>(*pStream);
  ASSERT_NE(nullptr, pValue);

  ASSERT_TRUE(pStream->next());
  ASSERT_EQ("1,24", std::string((char*)(pValue->value.c_str()), pValue->value.size()));
  ASSERT_EQ(1, pInc->value);
  ASSERT_EQ(0, pOffset->start);
  ASSERT_EQ(4, pOffset->end);
  ASSERT_TRUE(pStream->next());
  ASSERT_EQ("prosenttia", std::string((char*)(pValue->value.c_str()), pValue->value.size()));
  ASSERT_EQ(1, pInc->value);
  ASSERT_EQ(5, pOffset->start);
  ASSERT_EQ(15, pOffset->end);
  ASSERT_FALSE(pStream->next());
}

TEST_F(TextAnalyzerParserTestSuite, test_text_analyzer) {
  std::unordered_set<std::string> emptySet;
  std::string sField = "test field";

  // default behaviour
  {
    irs::analysis::text_token_stream::options_t options;

    options.locale = irs::locale_utils::locale("en_US.UTF-8");

    std::string data = " A  hErd of   quIck brown  foXes ran    and Jumped over  a     runninG dog";
    irs::analysis::text_token_stream stream(options, options.explicit_stopwords);

    auto testFunc = [](const irs::string_ref& data, analyzer* pStream) {
      ASSERT_TRUE(pStream->reset(data));

      auto* pOffset = irs::get<irs::offset>(*pStream);
      ASSERT_NE(nullptr, pOffset);
      auto* pInc = irs::get<irs::increment>(*pStream);
      ASSERT_NE(nullptr, pInc);
      auto* pPayload = irs::get<irs::payload>(*pStream);
      ASSERT_EQ(nullptr, pPayload);
      auto* pValue = irs::get<irs::term_attribute>(*pStream);
      ASSERT_NE(nullptr, pValue);

      ASSERT_TRUE(pStream->next());
      ASSERT_EQ("a", std::string((char*)(pValue->value.c_str()), pValue->value.size()));
      ASSERT_EQ(1, pInc->value);
      ASSERT_EQ(1, pOffset->start);
      ASSERT_EQ(2, pOffset->end);
      ASSERT_TRUE(pStream->next());
      ASSERT_EQ("herd", std::string((char*)(pValue->value.c_str()), pValue->value.size()));
      ASSERT_EQ(1, pInc->value);
      ASSERT_EQ(4, pOffset->start);
      ASSERT_EQ(8, pOffset->end);
      ASSERT_TRUE(pStream->next());
      ASSERT_EQ("of", std::string((char*)(pValue->value.c_str()), pValue->value.size()));
      ASSERT_EQ(1, pInc->value);
      ASSERT_EQ(9, pOffset->start);
      ASSERT_EQ(11, pOffset->end);
      ASSERT_TRUE(pStream->next());
      ASSERT_EQ("quick", std::string((char*)(pValue->value.c_str()), pValue->value.size()));
      ASSERT_EQ(1, pInc->value);
      ASSERT_EQ(14, pOffset->start);
      ASSERT_EQ(19, pOffset->end);
      ASSERT_TRUE(pStream->next());
      ASSERT_EQ("brown", std::string((char*)(pValue->value.c_str()), pValue->value.size()));
      ASSERT_EQ(1, pInc->value);
      ASSERT_EQ(20, pOffset->start);
      ASSERT_EQ(25, pOffset->end);
      ASSERT_TRUE(pStream->next());
      ASSERT_EQ("fox", std::string((char*)(pValue->value.c_str()), pValue->value.size()));
      ASSERT_EQ(1, pInc->value);
      ASSERT_EQ(27, pOffset->start);
      ASSERT_EQ(32, pOffset->end);
      ASSERT_TRUE(pStream->next());
      ASSERT_EQ("ran", std::string((char*)(pValue->value.c_str()), pValue->value.size()));
      ASSERT_EQ(1, pInc->value);
      ASSERT_EQ(33, pOffset->start);
      ASSERT_EQ(36, pOffset->end);
      ASSERT_TRUE(pStream->next());
      ASSERT_EQ("and", std::string((char*)(pValue->value.c_str()), pValue->value.size()));
      ASSERT_EQ(1, pInc->value);
      ASSERT_EQ(40, pOffset->start);
      ASSERT_EQ(43, pOffset->end);
      ASSERT_TRUE(pStream->next());
      ASSERT_EQ("jump", std::string((char*)(pValue->value.c_str()), pValue->value.size()));
      ASSERT_EQ(1, pInc->value);
      ASSERT_EQ(44, pOffset->start);
      ASSERT_EQ(50, pOffset->end);
      ASSERT_TRUE(pStream->next());
      ASSERT_EQ("over", std::string((char*)(pValue->value.c_str()), pValue->value.size()));
      ASSERT_EQ(1, pInc->value);
      ASSERT_EQ(51, pOffset->start);
      ASSERT_EQ(55, pOffset->end);
      ASSERT_TRUE(pStream->next());
      ASSERT_EQ("a", std::string((char*)(pValue->value.c_str()), pValue->value.size()));
      ASSERT_EQ(1, pInc->value);
      ASSERT_EQ(57, pOffset->start);
      ASSERT_EQ(58, pOffset->end);
      ASSERT_TRUE(pStream->next());
      ASSERT_EQ("run", std::string((char*)(pValue->value.c_str()), pValue->value.size()));
      ASSERT_EQ(1, pInc->value);
      ASSERT_EQ(63, pOffset->start);
      ASSERT_EQ(70, pOffset->end);
      ASSERT_TRUE(pStream->next());
      ASSERT_EQ("dog", std::string((char*)(pValue->value.c_str()), pValue->value.size()));
      ASSERT_EQ(1, pInc->value);
      ASSERT_EQ(71, pOffset->start);
      ASSERT_EQ(74, pOffset->end);
      ASSERT_FALSE(pStream->next());
    };

    {
      irs::analysis::text_token_stream::options_t options;
      options.locale = irs::locale_utils::locale("en_US.UTF-8");
      irs::analysis::text_token_stream stream(options, options.explicit_stopwords);
      testFunc(data, &stream);
    }
    {
      // stopwords  should be set to empty - or default values will interfere with test data
      auto stream = irs::analysis::analyzers::get("text", irs::type<irs::text_format::json>::get(), "{\"locale\":\"en_US.UTF-8\", \"stopwords\":[]}"); 
      ASSERT_NE(nullptr, stream);
      testFunc(data, stream.get());
    }
  }

  // case convert (lower)
  {

    std::string data = "A qUiCk brOwn FoX";
    
    auto testFunc = [](const irs::string_ref& data, analyzer* pStream) {

      ASSERT_TRUE(pStream->reset(data));

      auto* value = irs::get<irs::term_attribute>(*pStream);

      ASSERT_TRUE(pStream->next());
      ASSERT_EQ("a", irs::ref_cast<char>(value->value));
      ASSERT_TRUE(pStream->next());
      ASSERT_EQ("quick", irs::ref_cast<char>(value->value));
      ASSERT_TRUE(pStream->next());
      ASSERT_EQ("brown", irs::ref_cast<char>(value->value));
      ASSERT_TRUE(pStream->next());
      ASSERT_EQ("fox", irs::ref_cast<char>(value->value));
      ASSERT_FALSE(pStream->next());
    };

    {
      irs::analysis::text_token_stream::options_t options;
      options.case_convert = irs::analysis::text_token_stream::options_t::case_convert_t::LOWER;
      options.locale = irs::locale_utils::locale("en_US.UTF-8");
      irs::analysis::text_token_stream stream(options, options.explicit_stopwords);
      testFunc(data, &stream);
    }
    {
      // stopwords  should be set to empty - or default values will interfere with test data
      auto stream = irs::analysis::analyzers::get("text", irs::type<irs::text_format::json>::get(), "{\"locale\":\"en_US.UTF-8\", \"stopwords\":[], \"case\":\"lower\"}");
      ASSERT_NE(nullptr, stream);
      testFunc(data, stream.get());
    }
  }

  // case convert (upper)
  {
    std::string data = "A qUiCk brOwn FoX";

    auto testFunc = [](const irs::string_ref& data, analyzer* pStream) {
      ASSERT_TRUE(pStream->reset(data));

      auto* value = irs::get<irs::term_attribute>(*pStream);

      ASSERT_TRUE(pStream->next());
      ASSERT_EQ("A", irs::ref_cast<char>(value->value));
      ASSERT_TRUE(pStream->next());
      ASSERT_EQ("QUICK", irs::ref_cast<char>(value->value));
      ASSERT_TRUE(pStream->next());
      ASSERT_EQ("BROWN", irs::ref_cast<char>(value->value));
      ASSERT_TRUE(pStream->next());
      ASSERT_EQ("FOX", irs::ref_cast<char>(value->value));
      ASSERT_FALSE(pStream->next());
    };

    {
      irs::analysis::text_token_stream::options_t options;
      options.case_convert = irs::analysis::text_token_stream::options_t::case_convert_t::UPPER;
      options.locale = irs::locale_utils::locale("en_US.UTF-8");
      irs::analysis::text_token_stream stream(options, options.explicit_stopwords);
      testFunc(data, &stream);
    }
    {
      // stopwords  should be set to empty - or default values will interfere with test data
      auto stream = irs::analysis::analyzers::get("text", irs::type<irs::text_format::json>::get(), "{\"locale\":\"en_US.UTF-8\", \"stopwords\":[], \"case\":\"upper\"}");
      ASSERT_NE(nullptr, stream);
      testFunc(data, stream.get());
    }
  }

  // case convert (none)
  {
   
    std::string data = "A qUiCk brOwn FoX";

    auto testFunc = [](const irs::string_ref& data, analyzer* pStream) {
      ASSERT_TRUE(pStream->reset(data));

      auto* value = irs::get<irs::term_attribute>(*pStream);

      ASSERT_TRUE(pStream->next());
      ASSERT_EQ("A", irs::ref_cast<char>(value->value));
      ASSERT_TRUE(pStream->next());
      ASSERT_EQ("qUiCk", irs::ref_cast<char>(value->value));
      ASSERT_TRUE(pStream->next());
      ASSERT_EQ("brOwn", irs::ref_cast<char>(value->value));
      ASSERT_TRUE(pStream->next());
      ASSERT_EQ("FoX", irs::ref_cast<char>(value->value));
      ASSERT_FALSE(pStream->next());
    };

    {
      irs::analysis::text_token_stream::options_t options;
      options.case_convert = irs::analysis::text_token_stream::options_t::case_convert_t::NONE;
      options.locale = irs::locale_utils::locale("en_US.UTF-8");
      irs::analysis::text_token_stream stream(options, options.explicit_stopwords);
      testFunc(data, &stream);
    }
    {
      // stopwords  should be set to empty - or default values will interfere with test data
      auto stream = irs::analysis::analyzers::get("text", irs::type<irs::text_format::json>::get(), "{\"locale\":\"en_US.UTF-8\", \"stopwords\":[], \"case\":\"none\"}");
      ASSERT_NE(nullptr, stream);
      testFunc(data, stream.get());
    }
  }

  // ignored words
  {
   

    std::string data = " A thing of some KIND and ANoTher ";
 
    auto testFunc = [](const irs::string_ref& data, analyzer* pStream) {
      ASSERT_TRUE(pStream->reset(data));
     
      auto* pOffset = irs::get<irs::offset>(*pStream);
      ASSERT_NE(nullptr, pOffset);
      auto* pInc = irs::get<irs::increment>(*pStream);
      ASSERT_NE(nullptr, pInc);
      auto* pPayload = irs::get<irs::payload>(*pStream);
      ASSERT_EQ(nullptr, pPayload);
      auto* pValue = irs::get<irs::term_attribute>(*pStream);
      ASSERT_NE(nullptr, pValue);

      ASSERT_TRUE(pStream->next());
      ASSERT_EQ("thing", std::string((char*)(pValue->value.c_str()), pValue->value.size()));
      ASSERT_EQ(1, pInc->value);
      ASSERT_TRUE(pStream->next());
      ASSERT_EQ("some", std::string((char*)(pValue->value.c_str()), pValue->value.size()));
      ASSERT_EQ(1, pInc->value);
      ASSERT_TRUE(pStream->next());
      ASSERT_EQ("kind", std::string((char*)(pValue->value.c_str()), pValue->value.size()));
      ASSERT_EQ(1, pInc->value);
      ASSERT_TRUE(pStream->next());
      ASSERT_EQ("anoth", std::string((char*)(pValue->value.c_str()), pValue->value.size()));
      ASSERT_EQ(1, pInc->value);
      ASSERT_FALSE(pStream->next());
    };

    {
      irs::analysis::text_token_stream::options_t options;
      options.explicit_stopwords = std::unordered_set<std::string>({ "a", "of", "and" });
      options.locale = irs::locale_utils::locale("en_US.UTF-8");
      irs::analysis::text_token_stream stream(options, options.explicit_stopwords);
      testFunc(data, &stream);
    }
    {
      auto stream = irs::analysis::analyzers::get("text", irs::type<irs::text_format::json>::get(), "{\"locale\":\"en_US.UTF-8\", \"stopwords\":[\"a\", \"of\", \"and\"]}");
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

      auto* pOffset = irs::get<irs::offset>(*pStream);
      ASSERT_NE(nullptr, pOffset);
      auto* pPayload = irs::get<irs::payload>(*pStream);
      ASSERT_EQ(nullptr, pPayload);
      auto* pValue = irs::get<irs::term_attribute>(*pStream);
      ASSERT_NE(nullptr, pValue);
      auto* pInc = irs::get<irs::increment>(*pStream);
      ASSERT_NE(nullptr, pInc);

      ASSERT_TRUE(pStream->next());
      ASSERT_EQ(L"\u043F\u043E", utf_to_utf(pValue->value));
      ASSERT_EQ(1, pInc->value);
      ASSERT_EQ(0, pOffset->start);
      ASSERT_EQ(2, pOffset->end);
      ASSERT_TRUE(pStream->next());
      ASSERT_EQ(L"\u0432\u0435\u0447\u0435\u0440", utf_to_utf(pValue->value));
      ASSERT_EQ(1, pInc->value);
      ASSERT_TRUE(pStream->next());
      ASSERT_EQ(L"\u0435\u0436\u0438\u043A", utf_to_utf(pValue->value));
      ASSERT_EQ(1, pInc->value);
      ASSERT_TRUE(pStream->next());
      ASSERT_EQ(L"\u0445\u043E\u0434", utf_to_utf(pValue->value));
      ASSERT_EQ(1, pInc->value);
      ASSERT_TRUE(pStream->next());
      ASSERT_EQ(L"\u043A", utf_to_utf(pValue->value));
      ASSERT_EQ(1, pInc->value);
      ASSERT_TRUE(pStream->next());
      ASSERT_EQ(L"\u043C\u0435\u0434\u0432\u0435\u0436\u043E\u043D\u043A", utf_to_utf(pValue->value));
      ASSERT_EQ(1, pInc->value);
      ASSERT_TRUE(pStream->next());
      ASSERT_EQ(L"\u0441\u0447\u0438\u0442\u0430", utf_to_utf(pValue->value));
      ASSERT_EQ(1, pInc->value);
      ASSERT_TRUE(pStream->next());
      ASSERT_EQ(L"\u0437\u0432\u0435\u0437\u0434", utf_to_utf(pValue->value));
      ASSERT_EQ(1, pInc->value);
      ASSERT_FALSE(pStream->next());
    };

    {
      irs::analysis::text_token_stream::options_t options;
      options.locale = irs::locale_utils::locale("ru_RU.UTF-8");
      irs::analysis::text_token_stream stream(options, options.explicit_stopwords);
      testFunc(data, &stream);
    }
    {
      // stopwords  should be set to empty - or default values will interfere with test data
      auto stream = irs::analysis::analyzers::get("text", irs::type<irs::text_format::json>::get(), "{\"locale\":\"ru_RU.UTF-8\", \"stopwords\":[]}");
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

      auto* value = irs::get<irs::term_attribute>(*pStream);

      ASSERT_TRUE(pStream->next());
      ASSERT_EQ(L"\u043F\u043E", utf_to_utf(value->value));
      ASSERT_TRUE(pStream->next());
      ASSERT_EQ(L"\u0432\u0435\u0447\u0435\u0440", utf_to_utf(value->value));
      ASSERT_TRUE(pStream->next());
      ASSERT_EQ(L"\u0435\u0436\u0438\u043A", utf_to_utf(value->value));
      ASSERT_TRUE(pStream->next());
      ASSERT_EQ(L"\u0445\u043E\u0434", utf_to_utf(value->value));
      ASSERT_TRUE(pStream->next());
      ASSERT_EQ(L"\u043A", utf_to_utf(value->value));
      ASSERT_TRUE(pStream->next());
      ASSERT_EQ(L"\u043C\u0435\u0434\u0432\u0435\u0436\u043E\u043D\u043A", utf_to_utf(value->value));
      ASSERT_TRUE(pStream->next());
      ASSERT_EQ(L"\u0441\u0447\u0438\u0442\u0430", utf_to_utf(value->value));
      ASSERT_TRUE(pStream->next());
      ASSERT_EQ(L"\u0437\u0432\u0435\u0437\u0434", utf_to_utf(value->value));
      ASSERT_FALSE(pStream->next());
    };

    {
      irs::analysis::text_token_stream::options_t options;

      options.locale = irs::locale_utils::locale("ru_RU.UTF-8");
      options.accent = true;
      irs::analysis::text_token_stream stream(options, options.explicit_stopwords);
      testFunc(data, &stream);
    }
    {
      // stopwords  should be set to empty - or default values will interfere with test data
      auto stream = irs::analysis::analyzers::get("text", irs::type<irs::text_format::json>::get(), "{\"locale\":\"ru_RU.UTF-8\", \"stopwords\":[], \"accent\":true}");
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

      auto* value = irs::get<irs::term_attribute>(*pStream);

      ASSERT_TRUE(pStream->next());
      ASSERT_EQ(L"\u043F\u043E", utf_to_utf(value->value));
      ASSERT_TRUE(pStream->next());
      ASSERT_EQ(L"\u0432\u0435\u0447\u0435\u0440\u0430\u043C", utf_to_utf(value->value));
      ASSERT_TRUE(pStream->next());
      ASSERT_EQ(L"\u0435\u0436\u0438\u043A", utf_to_utf(value->value));
      ASSERT_TRUE(pStream->next());
      ASSERT_EQ(L"\u0445\u043E\u0434\u0438\u043B", utf_to_utf(value->value));
      ASSERT_TRUE(pStream->next());
      ASSERT_EQ(L"\u043A", utf_to_utf(value->value));
      ASSERT_TRUE(pStream->next());
      ASSERT_EQ(L"\u043C\u0435\u0434\u0432\u0435\u0436\u043E\u043D\u043A\u0443", utf_to_utf(value->value));
      ASSERT_TRUE(pStream->next());
      ASSERT_EQ(L"\u0441\u0447\u0438\u0442\u0430\u0442\u044C", utf_to_utf(value->value));
      ASSERT_TRUE(pStream->next());
      ASSERT_EQ(L"\u0437\u0432\u0435\u0437\u0434\u044B", utf_to_utf(value->value));
      ASSERT_FALSE(pStream->next());
    };

    {
      irs::analysis::text_token_stream::options_t options;
      options.locale = irs::locale_utils::locale("ru_RU.UTF-8");
      options.stemming = false;
      irs::analysis::text_token_stream stream(options, options.explicit_stopwords);
      testFunc(data, &stream);
    }
    {
      // stopwords  should be set to empty - or default values will interfere with test data
      auto stream = irs::analysis::analyzers::get("text", irs::type<irs::text_format::json>::get(), "{\"locale\":\"ru_RU.UTF-8\", \"stopwords\":[], \"stemming\":false}");
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

      auto* pOffset = irs::get<irs::offset>(*pStream);
      ASSERT_NE(nullptr, pOffset);
      auto* pPayload = irs::get<irs::payload>(*pStream);
      ASSERT_EQ(nullptr, pPayload);
      auto* pValue = irs::get<irs::term_attribute>(*pStream);
      ASSERT_NE(nullptr, pValue);
      auto* pInc = irs::get<irs::increment>(*pStream);
      ASSERT_NE(nullptr, pInc);

      ASSERT_TRUE(pStream->next());
      ASSERT_EQ(1, pInc->value);
      ASSERT_EQ(0, pOffset->start);
      ASSERT_EQ(2, pOffset->end);
      ASSERT_EQ(L"i\u0131", utf_to_utf(pValue->value));
      ASSERT_FALSE(pStream->next());
    };

    {
      irs::analysis::text_token_stream::options_t options;
      options.locale = irs::locale_utils::locale("tr-TR.UTF-8");
      irs::analysis::text_token_stream stream(options, options.explicit_stopwords);
      testFunc(data, &stream);
    }
    {
      // stopwords  should be set to empty - or default values will interfere with test data
      auto stream = irs::analysis::analyzers::get("text", irs::type<irs::text_format::json>::get(), "{\"locale\":\"tr-TR.UTF-8\", \"stopwords\":[]}");
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

      auto* pOffset = irs::get<irs::offset>(*pStream);
      ASSERT_NE(nullptr, pOffset);
      auto* pPayload = irs::get<irs::payload>(*pStream);
      ASSERT_EQ(nullptr, pPayload);
      auto* pValue = irs::get<irs::term_attribute>(*pStream);
      ASSERT_NE(nullptr, pValue);

      ASSERT_TRUE(pStream->next());
      ASSERT_EQ(L"\u4ECA\u5929", utf_to_utf(pValue->value));
      ASSERT_TRUE(pStream->next());
      ASSERT_EQ(L"\u4E0B\u5348", utf_to_utf(pValue->value));
      ASSERT_TRUE(pStream->next());
      ASSERT_EQ(L"\u7684", utf_to_utf(pValue->value));
      ASSERT_TRUE(pStream->next());
      ASSERT_EQ(L"\u592A\u9633", utf_to_utf(pValue->value));
      ASSERT_TRUE(pStream->next());
      ASSERT_EQ(L"\u5F88", utf_to_utf(pValue->value));
      ASSERT_TRUE(pStream->next());
      ASSERT_EQ(L"\u6E29\u6696", utf_to_utf(pValue->value));
      ASSERT_FALSE(pStream->next());
    };

    {
      irs::analysis::text_token_stream::options_t options;
      options.locale = irs::locale_utils::locale("zh_CN.UTF-8");
      irs::analysis::text_token_stream stream(options, options.explicit_stopwords);
      testFunc(data, &stream);
    }
    {
      // stopwords  should be set to empty - or default values will interfere with test data
      auto stream = irs::analysis::analyzers::get("text", irs::type<irs::text_format::json>::get(), "{\"locale\":\"zh_CN.UTF-8\", \"stopwords\":[]}");
      ASSERT_NE(nullptr, stream);
      testFunc(data, stream.get());
    }
  }

  // ...........................................................................
  // invalid
  // ...........................................................................
  {
    // stopwords  should be set to empty - or default values will interfere with test data
    auto stream = irs::analysis::analyzers::get("text", irs::type<irs::text_format::json>::get(), "{\"locale\":\"invalid12345.UTF-8\", \"stopwords\":[]}");
    ASSERT_EQ(nullptr, stream);
  }
}

TEST_F(TextAnalyzerParserTestSuite, test_fail_load_default_stopwords) {
  SetStopwordsPath("invalid stopwords path");

  // invalid custom stopwords path set -> fail
  {
    auto stream = irs::analysis::analyzers::get("text", irs::type<irs::text_format::json>::get(), "{\"locale\":\"en_US.UTF-8\"}");
    ASSERT_EQ(nullptr, stream);
  }
}

TEST_F(TextAnalyzerParserTestSuite, test_load_stopwords) {
  SetStopwordsPath(IResearch_test_resource_dir);

  {
    auto locale = "en_US.UTF-8";
    std::string sDataASCII = "A E I O U";

    auto testFunc = [](const irs::string_ref& data, analyzer::ptr pStream) {
      ASSERT_TRUE(pStream->reset(data));
      auto* pOffset = irs::get<irs::offset>(*pStream);
      ASSERT_NE(nullptr, pOffset);
      auto* pInc = irs::get<irs::increment>(*pStream);
      ASSERT_NE(nullptr, pInc);
      auto* pPayload = irs::get<irs::payload>(*pStream);
      ASSERT_EQ(nullptr, pPayload);
      auto* pValue = irs::get<irs::term_attribute>(*pStream);
      ASSERT_NE(nullptr, pValue);

      ASSERT_TRUE(pStream->next());
      ASSERT_EQ(1, pInc->value);
      ASSERT_EQ("e", std::string((char*)(pValue->value.c_str()), pValue->value.size()));
      ASSERT_EQ(2, pOffset->start);
      ASSERT_EQ(3, pOffset->end);
      ASSERT_TRUE(pStream->next());
      ASSERT_EQ(1, pInc->value);
      ASSERT_EQ(8, pOffset->start);
      ASSERT_EQ(9, pOffset->end);
      ASSERT_EQ("u", std::string((char*)(pValue->value.c_str()), pValue->value.size()));
      ASSERT_FALSE(pStream->next());
    };

    {
      auto stream = text_token_stream::make(locale);
      ASSERT_NE(nullptr, stream);
      testFunc(sDataASCII, stream);
    }

    // valid custom stopwords path -> ok
    {
      auto stream = irs::analysis::analyzers::get("text", irs::type<irs::text_format::json>::get(), "{\"locale\":\"en_US.UTF-8\"}");
      ASSERT_NE(nullptr, stream);
      testFunc(sDataASCII, stream);
    }

    // empty \"edgeNgram\" object
    {
      auto stream = irs::analysis::analyzers::get("text", irs::type<irs::text_format::json>::get(), "{\"locale\":\"en_US.UTF-8\", \"edgeNgram\": {}}");
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
      auto stream = irs::analysis::analyzers::get("text", irs::type<irs::text_format::json>::get(), "{\"locale\":\"C\"}");
      ASSERT_EQ(nullptr, stream);
    }
    {
      // min > max
      auto stream = irs::analysis::analyzers::get("text", irs::type<irs::text_format::json>::get(), "{\"locale\":\"ru_RU.UTF-8\", \"stopwords\":[], \"edgeNgram\" : {\"min\":2, \"max\":1, \"preserveOriginal\":false}}");
      ASSERT_EQ(nullptr, stream);
    }
  }
}

TEST_F(TextAnalyzerParserTestSuite, test_load_no_default_stopwords) {
  SetStopwordsPath(nullptr);

  {
    const std::string sDataASCII = "A E I O U";

    auto testFunc = [](const irs::string_ref& data, analyzer::ptr pStream) {
      ASSERT_TRUE(pStream->reset(data));
      auto* pOffset = irs::get<irs::offset>(*pStream);
      ASSERT_NE(nullptr, pOffset);
      auto* pInc = irs::get<irs::increment>(*pStream);
      ASSERT_NE(nullptr, pInc);
      auto* pPayload = irs::get<irs::payload>(*pStream);
      ASSERT_EQ(nullptr, pPayload);
      auto* pValue = irs::get<irs::term_attribute>(*pStream);
      ASSERT_NE(nullptr, pValue);

      ASSERT_TRUE(pStream->next());
      ASSERT_EQ(1, pInc->value);
      ASSERT_EQ("a", std::string((char*)(pValue->value.c_str()), pValue->value.size()));
      ASSERT_EQ(0, pOffset->start);
      ASSERT_EQ(1, pOffset->end);
      ASSERT_TRUE(pStream->next());
      ASSERT_EQ("e", std::string((char*)(pValue->value.c_str()), pValue->value.size()));
      ASSERT_EQ(1, pInc->value);
      ASSERT_EQ(2, pOffset->start);
      ASSERT_EQ(3, pOffset->end);
      ASSERT_TRUE(pStream->next());
      ASSERT_EQ("i", std::string((char*)(pValue->value.c_str()), pValue->value.size()));
      ASSERT_EQ(1, pInc->value);
      ASSERT_EQ(4, pOffset->start);
      ASSERT_EQ(5, pOffset->end);
      ASSERT_TRUE(pStream->next());
      ASSERT_EQ("o", std::string((char*)(pValue->value.c_str()), pValue->value.size()));
      ASSERT_EQ(1, pInc->value);
      ASSERT_EQ(6, pOffset->start);
      ASSERT_EQ(7, pOffset->end);
      ASSERT_TRUE(pStream->next());
      ASSERT_EQ("u", std::string((char*)(pValue->value.c_str()), pValue->value.size()));
      ASSERT_EQ(1, pInc->value);
      ASSERT_EQ(8, pOffset->start);
      ASSERT_EQ(9, pOffset->end);
      ASSERT_FALSE(pStream->next());
    };

    {
      auto stream = irs::analysis::analyzers::get("text", irs::type<irs::text_format::json>::get(), "{\"locale\":\"en_US.UTF-8\"}");
      ASSERT_NE(nullptr, stream);
      testFunc(sDataASCII, stream);
    }
  }
}

TEST_F(TextAnalyzerParserTestSuite, test_load_no_default_stopwords_fallback_cwd) {
  SetStopwordsPath(nullptr);

  // no stopwords, but valid CWD
  auto oldCWD = irs::utf8_path(true);
  auto newCWD = irs::utf8_path(IResearch_test_resource_dir);
  newCWD.chdir();
  auto reset_stopword_path = irs::make_finally([oldCWD]()->void {
    oldCWD.chdir();
  });

  {
    const std::string sDataASCII = "A E I O U";

    auto testFunc = [](const irs::string_ref& data, analyzer::ptr pStream) {
      ASSERT_TRUE(pStream->reset(data));
      auto* pOffset = irs::get<irs::offset>(*pStream);
      ASSERT_NE(nullptr, pOffset);
      auto* pInc = irs::get<irs::increment>(*pStream);
      ASSERT_NE(nullptr, pInc);
      auto* pPayload = irs::get<irs::payload>(*pStream);
      ASSERT_EQ(nullptr, pPayload);
      auto* pValue = irs::get<irs::term_attribute>(*pStream);
      ASSERT_NE(nullptr, pValue);

      ASSERT_TRUE(pStream->next());
      ASSERT_EQ("e", std::string((char*)(pValue->value.c_str()), pValue->value.size()));
      ASSERT_EQ(1, pInc->value);
      ASSERT_EQ(2, pOffset->start);
      ASSERT_EQ(3, pOffset->end);
      ASSERT_TRUE(pStream->next());
      ASSERT_EQ("u", std::string((char*)(pValue->value.c_str()), pValue->value.size()));
      ASSERT_EQ(1, pInc->value);
      ASSERT_EQ(8, pOffset->start);
      ASSERT_EQ(9, pOffset->end);
      ASSERT_FALSE(pStream->next());
    };

    {
      auto stream = irs::analysis::analyzers::get("text", irs::type<irs::text_format::json>::get(), "{\"locale\":\"en_US.UTF-8\"}");
      ASSERT_NE(nullptr, stream);
      testFunc(sDataASCII, stream);
    }
  }
}

TEST_F(TextAnalyzerParserTestSuite, test_load_stopwords_path_override) {
  SetStopwordsPath("some invalid path");

  std::string sDataASCII = "A E I O U";

  auto testFunc = [](const irs::string_ref& data, analyzer::ptr pStream) {
    ASSERT_TRUE(pStream->reset(data));
    auto* pOffset = irs::get<irs::offset>(*pStream);
    ASSERT_NE(nullptr, pOffset);
    auto* pInc = irs::get<irs::increment>(*pStream);
    ASSERT_NE(nullptr, pInc);
    auto* pPayload = irs::get<irs::payload>(*pStream);
    ASSERT_EQ(nullptr, pPayload);
    auto* pValue = irs::get<irs::term_attribute>(*pStream);
    ASSERT_NE(nullptr, pValue);

    ASSERT_TRUE(pStream->next());
    ASSERT_EQ(1, pInc->value);
    ASSERT_EQ("e", std::string((char*)(pValue->value.c_str()), pValue->value.size()));
    ASSERT_EQ(2, pOffset->start);
    ASSERT_EQ(3, pOffset->end);
    ASSERT_TRUE(pStream->next());
    ASSERT_EQ(1, pInc->value);
    ASSERT_EQ(8, pOffset->start);
    ASSERT_EQ(9, pOffset->end);
    ASSERT_EQ("u", std::string((char*)(pValue->value.c_str()), pValue->value.size()));
    ASSERT_FALSE(pStream->next());
  };

  // overriding ignored words path
  auto stream = irs::analysis::analyzers::get("text", irs::type<irs::text_format::json>::get(), "{\"locale\":\"en_US.UTF-8\", \"stopwordsPath\":\"" IResearch_test_resource_dir "\"}");
  ASSERT_NE(nullptr, stream);
  testFunc(sDataASCII, stream);
}

TEST_F(TextAnalyzerParserTestSuite, test_load_stopwords_path_override_emptypath) {
  // no stopwords, but empty stopwords path (we need to shift CWD to our test resources, to be able to load stopwords)
  auto oldCWD = irs::utf8_path(true);
  auto newCWD = irs::utf8_path(IResearch_test_resource_dir);
  newCWD.chdir();
  auto reset_stopword_path = irs::make_finally([oldCWD]()->void {
    oldCWD.chdir();
  });

  std::string config = "{\"locale\":\"en_US.UTF-8\",\"case\":\"lower\",\"accent\":false,\"stemming\":true,\"stopwordsPath\":\"\"}";
  auto stream = irs::analysis::analyzers::get("text", irs::type<irs::text_format::json>::get(), config);
  ASSERT_NE(nullptr, stream);

  // Checking that default stowords are loaded
  std::string sDataASCII = "A E I O U";
  ASSERT_TRUE(stream->reset(sDataASCII));
  auto* pOffset = irs::get<irs::offset>(*stream);
  ASSERT_NE(nullptr, pOffset);
  auto* pInc = irs::get<irs::increment>(*stream);
  ASSERT_NE(nullptr, pInc);
  auto* pPayload = irs::get<irs::payload>(*stream);
  ASSERT_EQ(nullptr, pPayload);
  auto* pValue = irs::get<irs::term_attribute>(*stream);
  ASSERT_NE(nullptr, pValue);

  ASSERT_TRUE(stream->next());
  ASSERT_EQ(1, pInc->value);
  ASSERT_EQ("e", std::string((char*)(pValue->value.c_str()), pValue->value.size()));
  ASSERT_EQ(2, pOffset->start);
  ASSERT_EQ(3, pOffset->end);
  ASSERT_TRUE(stream->next());
  ASSERT_EQ(1, pInc->value);
  ASSERT_EQ(8, pOffset->start);
  ASSERT_EQ(9, pOffset->end);
  ASSERT_EQ("u", std::string((char*)(pValue->value.c_str()), pValue->value.size()));
  ASSERT_FALSE(stream->next());
}

TEST_F(TextAnalyzerParserTestSuite, test_make_config_json) {
  
  //with unknown parameter
  {
    std::string config = "{\"locale\":\"ru_RU.UTF-8\",\"case\":\"lower\",\"invalid_parameter\":true,\"stopwords\":[],\"accent\":true,\"stemming\":false}";
    std::string actual;
    ASSERT_TRUE(irs::analysis::analyzers::normalize(actual, "text", irs::type<irs::text_format::json>::get(), config));
    ASSERT_EQ("{\"locale\":\"ru_RU.utf-8\",\"case\":\"lower\",\"stopwords\":[],\"accent\":true,\"stemming\":false}", actual);
  }

  // no case convert in creation. Default value shown
  {
    std::string config = "{\"locale\":\"ru_RU.UTF-8\",\"stopwords\":[],\"accent\":true,\"stemming\":false}";
    std::string actual;
    ASSERT_TRUE(irs::analysis::analyzers::normalize(actual, "text", irs::type<irs::text_format::json>::get(), config));
    ASSERT_EQ("{\"locale\":\"ru_RU.utf-8\",\"case\":\"lower\",\"stopwords\":[],\"accent\":true,\"stemming\":false}" , actual);
  }

  // no accent in creation. Default value shown
  {
    std::string config = "{\"locale\":\"ru_RU.UTF-8\",\"case\":\"lower\",\"stopwords\":[],\"stemming\":false}";
    std::string actual;
    ASSERT_TRUE(irs::analysis::analyzers::normalize(actual, "text", irs::type<irs::text_format::json>::get(), config));
    ASSERT_EQ("{\"locale\":\"ru_RU.utf-8\",\"case\":\"lower\",\"stopwords\":[],\"accent\":false,\"stemming\":false}", actual);
  }

  // no stem in creation. Default value shown
  {
    std::string config = "{\"locale\":\"ru_RU.UTF-8\",\"case\":\"lower\",\"stopwords\":[],\"accent\":true}";
    std::string actual;
    ASSERT_TRUE(irs::analysis::analyzers::normalize(actual, "text", irs::type<irs::text_format::json>::get(), config));
    ASSERT_EQ("{\"locale\":\"ru_RU.utf-8\",\"case\":\"lower\",\"stopwords\":[],\"accent\":true,\"stemming\":true}", actual);
  }

  // non default values for stem, accent and case
  {
    std::string config = "{\"locale\":\"ru_RU.utf-8\",\"case\":\"upper\",\"stopwords\":[],\"accent\":true,\"stemming\":false}";
    std::string actual;
    ASSERT_TRUE(irs::analysis::analyzers::normalize(actual, "text", irs::type<irs::text_format::json>::get(), config));
    ASSERT_EQ(config, actual);
  }

  // no stopwords no stopwords path
  {
    SetStopwordsPath(IResearch_test_resource_dir);

    std::string config = "{\"locale\":\"en_US.utf-8\",\"case\":\"lower\",\"accent\":false,\"stemming\":true}";
    std::string actual;
    ASSERT_TRUE(irs::analysis::analyzers::normalize(actual, "text", irs::type<irs::text_format::json>::get(), config));
    ASSERT_EQ(config, actual);
  }
  
  // empty stopwords, but stopwords path
  {
    std::string config = "{\"locale\":\"en_US.utf-8\",\"case\":\"upper\",\"stopwords\":[],\"accent\":false,\"stemming\":true,\"stopwordsPath\":\"" IResearch_test_resource_dir "\"}";
    std::string actual;
    ASSERT_TRUE(irs::analysis::analyzers::normalize(actual, "text", irs::type<irs::text_format::json>::get(), config));
    ASSERT_EQ(config, actual);
  }

  // no stopwords, but stopwords path
  {
    std::string config = "{\"locale\":\"en_US.utf-8\",\"case\":\"upper\",\"accent\":false,\"stemming\":true,\"stopwordsPath\":\"" IResearch_test_resource_dir "\"}";
    std::string actual;
    ASSERT_TRUE(irs::analysis::analyzers::normalize(actual, "text", irs::type<irs::text_format::json>::get(), config));
    ASSERT_EQ(config, actual);
  }

  // no stopwords, but empty stopwords path (we need to shift CWD to our test resources, to be able to load stopwords)
  {
    auto oldCWD = irs::utf8_path(true);
    auto newCWD = irs::utf8_path(IResearch_test_resource_dir);
    newCWD.chdir();
    auto reset_stopword_path = irs::make_finally([oldCWD]()->void {
      oldCWD.chdir();
    });

    std::string config = "{\"locale\":\"en_US.utf-8\",\"case\":\"lower\",\"accent\":false,\"stemming\":true,\"stopwordsPath\":\"\"}";
    std::string actual;
    ASSERT_TRUE(irs::analysis::analyzers::normalize(actual, "text", irs::type<irs::text_format::json>::get(), config));
    ASSERT_EQ(config, actual);
  }

  // non-empty stopwords with duplicates
  {
    std::string config = "{\"locale\":\"en_US.utf-8\",\"case\":\"upper\",\"stopwords\":[\"z\",\"a\",\"b\",\"a\"],\"accent\":false,\"stemming\":true,\"stopwordsPath\":\"" IResearch_test_resource_dir "\"}";
    std::string actual;
    ASSERT_TRUE(irs::analysis::analyzers::normalize(actual, "text", irs::type<irs::text_format::json>::get(), config));

    // stopwords order is not guaranteed. Need to deep check json
    rapidjson::Document json;
    ASSERT_FALSE(json.Parse(actual.c_str(), actual.size()).HasParseError());
    ASSERT_TRUE(json.HasMember("stopwords"));
    auto& stopwords = json["stopwords"]; 
    ASSERT_TRUE(stopwords.IsArray());

    std::unordered_set<std::string> expected_stopwords = { "z","a","b" };
    for (auto itr = stopwords.Begin(), end = stopwords.End();
      itr != end;
      ++itr) {
      ASSERT_TRUE(itr->IsString());
      expected_stopwords.erase(itr->GetString());
    }
    ASSERT_TRUE(expected_stopwords.empty());
  }

  // min, max, preserveOriginal
  {
    std::string config = "{\"locale\":\"ru_RU.UTF-8\",\"case\":\"lower\",\"stopwords\":[], \"edgeNgram\" : { \"min\":1,\"max\":1,\"preserveOriginal\":true }}";
    std::string actual;
    ASSERT_TRUE(irs::analysis::analyzers::normalize(actual, "text", irs::type<irs::text_format::json>::get(), config));
    ASSERT_EQ("{\"locale\":\"ru_RU.utf-8\",\"case\":\"lower\",\"stopwords\":[],\"accent\":false,\"stemming\":true,\"edgeNgram\":{\"min\":1,\"max\":1,\"preserveOriginal\":true}}", actual);
  }

  // without min (see above for without min, max, and preserveOriginal)
  {
    std::string config = "{\"locale\":\"ru_RU.UTF-8\",\"case\":\"lower\",\"stopwords\":[], \"edgeNgram\" : {\"max\":2,\"preserveOriginal\":false}}";
    std::string actual;
    ASSERT_TRUE(irs::analysis::analyzers::normalize(actual, "text", irs::type<irs::text_format::json>::get(), config));
    ASSERT_EQ("{\"locale\":\"ru_RU.utf-8\",\"case\":\"lower\",\"stopwords\":[],\"accent\":false,\"stemming\":true,\"edgeNgram\":{\"max\":2,\"preserveOriginal\":false}}", actual);
  }
}

TEST_F(TextAnalyzerParserTestSuite, test_make_config_text) {
  std::string config = "RU";
  std::string actual;
  ASSERT_TRUE(irs::analysis::analyzers::normalize(actual, "text", irs::type<irs::text_format::text>::get(), config));
  ASSERT_EQ("ru", actual);
}

TEST_F(TextAnalyzerParserTestSuite, test_make_config_invalid_format) {
  std::string config = "{\"locale\":\"ru_RU.utf-8\",\"case\":\"lower\",\"stopwords\":[],\"accent\":true}";
  std::string actual;
  ASSERT_FALSE(irs::analysis::analyzers::normalize(actual, "text", irs::type<irs::text_format::csv>::get(), config));
}

TEST_F(TextAnalyzerParserTestSuite, test_deterministic_stopwords_order) {
  std::string config = "{\"locale\":\"ru_RU.utf-8\",\"case\":\"lower\",\"stopwords\":[ \"ag\",\
      \"of\", \"plc\", \"the\", \"inc\", \"co\", \"ltd\"], \"accent\":true}";
  std::string normalized1;
  ASSERT_TRUE(irs::analysis::analyzers::normalize(normalized1, "text", irs::type<irs::text_format::json>::get(), config));
  std::string normalized2;
  ASSERT_TRUE(irs::analysis::analyzers::normalize(normalized2, "text", irs::type<irs::text_format::json>::get(), normalized1));
  ASSERT_EQ(normalized1, normalized2);
}

TEST_F(TextAnalyzerParserTestSuite, test_text_ngrams) {
  // text ngrams

  // default behaviour
  {
    std::string data = " A  hErd of   quIck ";

    auto testFunc = [](const irs::string_ref& data, analyzer* pStream) {
      ASSERT_TRUE(pStream->reset(data));

      auto* pValue = irs::get<irs::term_attribute>(*pStream);

      ASSERT_TRUE(pStream->next());
      ASSERT_EQ("he", irs::ref_cast<char>(pValue->value));
      ASSERT_TRUE(pStream->next());
      ASSERT_EQ("her", irs::ref_cast<char>(pValue->value));
      ASSERT_TRUE(pStream->next());
      ASSERT_EQ("of", irs::ref_cast<char>(pValue->value));
      ASSERT_TRUE(pStream->next());
      ASSERT_EQ("qu", irs::ref_cast<char>(pValue->value));
      ASSERT_TRUE(pStream->next());
      ASSERT_EQ("qui", irs::ref_cast<char>(pValue->value));
      ASSERT_FALSE(pStream->next());
    };

    {
      auto stream = irs::analysis::analyzers::get("text", irs::type<irs::text_format::json>::get(), "{\"locale\":\"en_US.UTF-8\", \"stopwords\":[\"a\"], \"edgeNgram\" : {\"min\":2, \"max\":3, \"preserveOriginal\":false}}");
      ASSERT_NE(nullptr, stream);
      testFunc(data, stream.get());
    }
  }

  // min == 0
  {
    std::string data = " A  hErd of   quIck ";

    auto testFunc = [](const irs::string_ref& data, analyzer* pStream) {
      ASSERT_TRUE(pStream->reset(data));

      auto* pValue = irs::get<irs::term_attribute>(*pStream);

      ASSERT_TRUE(pStream->next());
      ASSERT_EQ("h", irs::ref_cast<char>(pValue->value));
      ASSERT_TRUE(pStream->next());
      ASSERT_EQ("he", irs::ref_cast<char>(pValue->value));
      ASSERT_TRUE(pStream->next());
      ASSERT_EQ("her", irs::ref_cast<char>(pValue->value));
      ASSERT_TRUE(pStream->next());
      ASSERT_EQ("o", irs::ref_cast<char>(pValue->value));
      ASSERT_TRUE(pStream->next());
      ASSERT_EQ("of", irs::ref_cast<char>(pValue->value));
      ASSERT_TRUE(pStream->next());
      ASSERT_EQ("q", irs::ref_cast<char>(pValue->value));
      ASSERT_TRUE(pStream->next());
      ASSERT_EQ("qu", irs::ref_cast<char>(pValue->value));
      ASSERT_TRUE(pStream->next());
      ASSERT_EQ("qui", irs::ref_cast<char>(pValue->value));
      ASSERT_FALSE(pStream->next());

      ASSERT_TRUE(pStream->reset(data));
      ASSERT_TRUE(pStream->next());
      ASSERT_EQ("h", std::string(reinterpret_cast<const char*>((pValue->value.c_str())), pValue->value.size()));
    };

    {
      auto stream = irs::analysis::analyzers::get("text", irs::type<irs::text_format::json>::get(), "{\"locale\":\"en_US.UTF-8\", \"stopwords\":[\"a\"], \"edgeNgram\" : {\"min\":0, \"max\":3, \"preserveOriginal\":false}}");
      ASSERT_NE(nullptr, stream);
      testFunc(data, stream.get());
    }
  }

  // preserveOriginal == true
  {
    std::string data = " A  hErd of   quIck ";

    auto testFunc = [](const irs::string_ref& data, analyzer* pStream) {
      ASSERT_TRUE(pStream->reset(data));

      auto* pValue = irs::get<irs::term_attribute>(*pStream);

      ASSERT_TRUE(pStream->next());
      ASSERT_EQ("he", irs::ref_cast<char>(pValue->value));
      ASSERT_TRUE(pStream->next());
      ASSERT_EQ("her", irs::ref_cast<char>(pValue->value));
      ASSERT_TRUE(pStream->next());
      ASSERT_EQ("herd", irs::ref_cast<char>(pValue->value));
      ASSERT_TRUE(pStream->next());
      ASSERT_EQ("of", irs::ref_cast<char>(pValue->value));
      ASSERT_TRUE(pStream->next());
      ASSERT_EQ("qu", irs::ref_cast<char>(pValue->value));
      ASSERT_TRUE(pStream->next());
      ASSERT_EQ("qui", irs::ref_cast<char>(pValue->value));
      ASSERT_TRUE(pStream->next());
      ASSERT_EQ("quick", irs::ref_cast<char>(pValue->value));
      ASSERT_FALSE(pStream->next());
    };

    {
      auto stream = irs::analysis::analyzers::get("text", irs::type<irs::text_format::json>::get(), "{\"locale\":\"en_US.UTF-8\", \"stopwords\":[\"a\"], \"edgeNgram\" : {\"min\":2, \"max\":3, \"preserveOriginal\":true}}");
      ASSERT_NE(nullptr, stream);
      testFunc(data, stream.get());
    }
  }

  // min == max
  {
    std::string data = " A  hErd of   quIck ";

    auto testFunc = [](const irs::string_ref& data, analyzer* pStream) {
      ASSERT_TRUE(pStream->reset(data));

      auto* pValue = irs::get<irs::term_attribute>(*pStream);

      ASSERT_TRUE(pStream->next());
      ASSERT_EQ("her", irs::ref_cast<char>(pValue->value));
      ASSERT_TRUE(pStream->next());
      ASSERT_EQ("qui", irs::ref_cast<char>(pValue->value));
      ASSERT_FALSE(pStream->next());
    };

    {
      auto stream = irs::analysis::analyzers::get("text", irs::type<irs::text_format::json>::get(), "{\"locale\":\"en_US.UTF-8\", \"stopwords\":[\"a\"], \"edgeNgram\" : {\"min\":3, \"max\":3, \"preserveOriginal\":false}}");
      ASSERT_NE(nullptr, stream);
      testFunc(data, stream.get());
    }
  }

  // min > max and preserveOriginal == false
  {
    std::string data = " A  hErd of   quIck ";

    auto testFunc = [](const irs::string_ref& data, analyzer* pStream) {
      ASSERT_TRUE(pStream->reset(data));

      auto* pValue = irs::get<irs::term_attribute>(*pStream);
      ASSERT_NE(nullptr, pValue);

      ASSERT_FALSE(pStream->next());
    };

    {
      irs::analysis::text_token_stream::options_t options;
      options.locale = irs::locale_utils::locale("en_US.UTF-8");
      options.explicit_stopwords.emplace("a");
      options.min_gram = 4;
      options.min_gram_set = 4;
      options.max_gram = 3;
      options.max_gram_set = 3;
      options.preserve_original = false;
      irs::analysis::text_token_stream stream(options, options.explicit_stopwords);

      testFunc(data, &stream);
    }
  }

  // min > max and preserveOriginal == true
  {
    std::string data = " A  hErd of   quIck ";

    auto testFunc = [](const irs::string_ref& data, analyzer* pStream) {
      ASSERT_TRUE(pStream->reset(data));

      auto* pValue = irs::get<irs::term_attribute>(*pStream);

      ASSERT_TRUE(pStream->next());
      ASSERT_EQ("herd", irs::ref_cast<char>(pValue->value));
      ASSERT_TRUE(pStream->next());
      ASSERT_EQ("of", irs::ref_cast<char>(pValue->value));
      ASSERT_TRUE(pStream->next());
      ASSERT_EQ("quick", irs::ref_cast<char>(pValue->value));
      ASSERT_FALSE(pStream->next());
    };

    {
      irs::analysis::text_token_stream::options_t options;
      options.locale = irs::locale_utils::locale("en_US.UTF-8");
      options.explicit_stopwords.emplace("a");
      options.min_gram = 4;
      options.min_gram_set = 4;
      options.max_gram = 3;
      options.max_gram_set = 3;
      options.preserve_original = true;
      irs::analysis::text_token_stream stream(options, options.explicit_stopwords);

      testFunc(data, &stream);
    }
  }

  // min == max == 0 and no preserveOriginal
  {
    std::string data = " A  hErd of   quIck ";

    auto testFunc = [](const irs::string_ref& data, analyzer* pStream) {
      ASSERT_TRUE(pStream->reset(data));

      auto* pValue = irs::get<irs::term_attribute>(*pStream);
      ASSERT_NE(nullptr, pValue);

      ASSERT_FALSE(pStream->next());
    };

    {
      auto stream = irs::analysis::analyzers::get("text", irs::type<irs::text_format::json>::get(), "{\"locale\":\"en_US.UTF-8\", \"stopwords\":[\"a\"], \"edgeNgram\" : {\"min\":0, \"max\":0}}");
      ASSERT_NE(nullptr, stream);
      testFunc(data, stream.get());
    }
  }

  // no min and preserveOriginal == false
  {
    std::string data = " A  hErd of";

    auto testFunc = [](const irs::string_ref& data, analyzer* pStream) {
      ASSERT_TRUE(pStream->reset(data));

      auto* pValue = irs::get<irs::term_attribute>(*pStream);

      ASSERT_TRUE(pStream->next());
      ASSERT_EQ("h", irs::ref_cast<char>(pValue->value));
      ASSERT_TRUE(pStream->next());
      ASSERT_EQ("o", irs::ref_cast<char>(pValue->value));
      ASSERT_FALSE(pStream->next());
    };

    {
      auto stream = irs::analysis::analyzers::get("text", irs::type<irs::text_format::json>::get(), "{\"locale\":\"en_US.UTF-8\", \"stopwords\":[\"a\"], \"edgeNgram\" : {\"max\":1, \"preserveOriginal\":false}}");
      ASSERT_NE(nullptr, stream);
      testFunc(data, stream.get());
    }
  }

  // no min and preserveOriginal == true
  {
    std::string data = " A  hErd of";

    auto testFunc = [](const irs::string_ref& data, analyzer* pStream) {
      ASSERT_TRUE(pStream->reset(data));

      auto* pValue = irs::get<irs::term_attribute>(*pStream);

      ASSERT_TRUE(pStream->next());
      ASSERT_EQ("h", irs::ref_cast<char>(pValue->value));
      ASSERT_TRUE(pStream->next());
      ASSERT_EQ("herd", irs::ref_cast<char>(pValue->value));
      ASSERT_TRUE(pStream->next());
      ASSERT_EQ("o", irs::ref_cast<char>(pValue->value));
      ASSERT_TRUE(pStream->next());
      ASSERT_EQ("of", irs::ref_cast<char>(pValue->value));
      ASSERT_FALSE(pStream->next());
    };

    {
      auto stream = irs::analysis::analyzers::get("text", irs::type<irs::text_format::json>::get(), "{\"locale\":\"en_US.UTF-8\", \"stopwords\":[\"a\"], \"edgeNgram\" : {\"max\":1, \"preserveOriginal\":true}}");
      ASSERT_NE(nullptr, stream);
      testFunc(data, stream.get());
    }
  }

  // no max
  {
    std::string data = " A  hErd of";

    auto testFunc = [](const irs::string_ref& data, analyzer* pStream) {
      ASSERT_TRUE(pStream->reset(data));

      auto* pValue = irs::get<irs::term_attribute>(*pStream);

      ASSERT_TRUE(pStream->next());
      ASSERT_EQ("h", irs::ref_cast<char>(pValue->value));
      ASSERT_TRUE(pStream->next());
      ASSERT_EQ("he", irs::ref_cast<char>(pValue->value));
      ASSERT_TRUE(pStream->next());
      ASSERT_EQ("her", irs::ref_cast<char>(pValue->value));
      ASSERT_TRUE(pStream->next());
      ASSERT_EQ("herd", irs::ref_cast<char>(pValue->value));
      ASSERT_TRUE(pStream->next());
      ASSERT_EQ("o", irs::ref_cast<char>(pValue->value));
      ASSERT_TRUE(pStream->next());
      ASSERT_EQ("of", irs::ref_cast<char>(pValue->value));
      ASSERT_FALSE(pStream->next());
    };

    {
      auto stream = irs::analysis::analyzers::get("text", irs::type<irs::text_format::json>::get(), "{\"locale\":\"en_US.UTF-8\", \"stopwords\":[\"a\"], \"edgeNgram\" : {\"min\":1, \"preserveOriginal\":false}}");
      ASSERT_NE(nullptr, stream);
      testFunc(data, stream.get());
    }
  }

  // no min and no max and preserveOriginal == false
  {
    std::string data = " A  hErd of";

    auto testFunc = [](const irs::string_ref& data, analyzer* pStream) {
      ASSERT_TRUE(pStream->reset(data));

      auto* pValue = irs::get<irs::term_attribute>(*pStream);

      ASSERT_TRUE(pStream->next());
      ASSERT_EQ("h", irs::ref_cast<char>(pValue->value));
      ASSERT_TRUE(pStream->next());
      ASSERT_EQ("he", irs::ref_cast<char>(pValue->value));
      ASSERT_TRUE(pStream->next());
      ASSERT_EQ("her", irs::ref_cast<char>(pValue->value));
      ASSERT_TRUE(pStream->next());
      ASSERT_EQ("herd", irs::ref_cast<char>(pValue->value));
      ASSERT_TRUE(pStream->next());
      ASSERT_EQ("o", irs::ref_cast<char>(pValue->value));
      ASSERT_TRUE(pStream->next());
      ASSERT_EQ("of", irs::ref_cast<char>(pValue->value));
      ASSERT_FALSE(pStream->next());
    };

    {
      auto stream = irs::analysis::analyzers::get("text", irs::type<irs::text_format::json>::get(), "{\"locale\":\"en_US.UTF-8\", \"stopwords\":[\"a\"], \"edgeNgram\" : {\"preserveOriginal\":false}}");
      ASSERT_NE(nullptr, stream);
      testFunc(data, stream.get());
    }
  }

  // no min and no max and preserveOriginal == true
  {
    std::string data = " A  hErd of";

    auto testFunc = [](const irs::string_ref& data, analyzer* pStream) {
      ASSERT_TRUE(pStream->reset(data));

      auto* pValue = irs::get<irs::term_attribute>(*pStream);

      ASSERT_TRUE(pStream->next());
      ASSERT_EQ("h", irs::ref_cast<char>(pValue->value));
      ASSERT_TRUE(pStream->next());
      ASSERT_EQ("he", irs::ref_cast<char>(pValue->value));
      ASSERT_TRUE(pStream->next());
      ASSERT_EQ("her", irs::ref_cast<char>(pValue->value));
      ASSERT_TRUE(pStream->next());
      ASSERT_EQ("herd", irs::ref_cast<char>(pValue->value));
      ASSERT_TRUE(pStream->next());
      ASSERT_EQ("o", irs::ref_cast<char>(pValue->value));
      ASSERT_TRUE(pStream->next());
      ASSERT_EQ("of", irs::ref_cast<char>(pValue->value));
      ASSERT_FALSE(pStream->next());
    };

    {
      auto stream = irs::analysis::analyzers::get("text", irs::type<irs::text_format::json>::get(), "{\"locale\":\"en_US.UTF-8\", \"stopwords\":[\"a\"], \"edgeNgram\" : {\"preserveOriginal\":true}}");
      ASSERT_NE(nullptr, stream);
      testFunc(data, stream.get());
    }
  }

  // wide symbols
  {
    std::wstring sDataUCS2 = L"\u041F\u043E \u0432\u0435\u0447\u0435\u0440\u0430\u043C \u043A \u041C\u0435\u0434\u0432\u0435\u0436\u043E\u043D\u043A\u0443";
    auto locale = irs::locale_utils::locale(irs::string_ref::NIL, "utf8", true); // utf8 internal and external
    std::string data;
    ASSERT_TRUE(irs::locale_utils::append_external<wchar_t>(data, sDataUCS2, locale));

    auto testFunc = [](const irs::string_ref& data, analyzer* pStream) {
      ASSERT_TRUE(pStream->reset(data));

      auto* pValue = irs::get<irs::term_attribute>(*pStream);

      ASSERT_TRUE(pStream->next());
      ASSERT_EQ(L"\u043F", utf_to_utf(pValue->value));
      ASSERT_TRUE(pStream->next());
      ASSERT_EQ(L"\u043F\u043E", utf_to_utf(pValue->value));
      ASSERT_TRUE(pStream->next());
      ASSERT_EQ(L"\u0432", utf_to_utf(pValue->value));
      ASSERT_TRUE(pStream->next());
      ASSERT_EQ(L"\u0432\u0435", utf_to_utf(pValue->value));
      ASSERT_TRUE(pStream->next());
      ASSERT_EQ(L"\u043C", utf_to_utf(pValue->value));
      ASSERT_TRUE(pStream->next());
      ASSERT_EQ(L"\u043C\u0435", utf_to_utf(pValue->value));
      ASSERT_FALSE(pStream->next());
    };

    {
      auto stream = irs::analysis::analyzers::get("text", irs::type<irs::text_format::json>::get(), "{\"locale\":\"ru_RU.UTF-8\", \"stopwords\":[\"\\u043A\"], \"edgeNgram\" : {\"min\":1, \"max\":2, \"preserveOriginal\":false}}");
      ASSERT_NE(nullptr, stream);
      testFunc(data, stream.get());
    }
  }
}
