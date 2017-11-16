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

#include <unicode/uclean.h> // for u_cleanup

#include "tests_config.hpp"
#include "tests_shared.hpp"
#include "analysis/analyzers.hpp"
#include "utils/runtime_utils.hpp"

NS_BEGIN(tests)

class analyzer_test: public ::testing::Test {

  virtual void SetUp() {
    // Code here will be called immediately after the constructor (right before each test).

    // ensure stopwords are loaded/cached for the 'en' locale used for text analysis below
    {
      // same env variable name as iresearch::analysis::text_token_stream::STOPWORD_PATH_ENV_VARIABLE
      const auto text_stopword_path_var = "IRESEARCH_TEXT_STOPWORD_PATH";
      const char* czOldStopwordPath = iresearch::getenv(text_stopword_path_var);
      std::string sOldStopwordPath = czOldStopwordPath == nullptr ? "" : czOldStopwordPath;

      iresearch::setenv(text_stopword_path_var, IResearch_test_resource_dir, true);
      iresearch::analysis::analyzers::get("text", "en"); // stream needed only to load stopwords

      if (czOldStopwordPath) {
        iresearch::setenv(text_stopword_path_var, sOldStopwordPath.c_str(), true);
      }
    }
  }

  virtual void TearDown() {
    // Code here will be called immediately after each test (right before the destructor).

    u_cleanup(); // release/free all memory used by ICU
  }
};

NS_END

using namespace tests;

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

TEST_F(analyzer_test, duplicate_register) {
  struct dummy_analyzer: public irs::analysis::analyzer {
    DECLARE_ANALYZER_TYPE() { static irs::analysis::analyzer::type_id type("dummy_analyzer"); return type; }
    static ptr make(const irs::string_ref&) { return nullptr; }
    dummy_analyzer(): irs::analysis::analyzer(dummy_analyzer::type()) { }
  };

  static bool initial_expected = true;
  irs::analysis::analyzer_registrar initial(dummy_analyzer::type(), &dummy_analyzer::make);
  ASSERT_EQ(!initial_expected, !initial);
  initial_expected = false; // next test iteration will not be able to register the same analyzer
  irs::analysis::analyzer_registrar duplicate(dummy_analyzer::type(), &dummy_analyzer::make);
  ASSERT_TRUE(!duplicate);
}

TEST_F(analyzer_test, test_load) {
  {
    auto analyzer = iresearch::analysis::analyzers::get("text", "en");

    ASSERT_NE(nullptr, analyzer);
    ASSERT_TRUE(analyzer->reset("abc"));
  }

  // locale with default ingnored_words
  {
    auto analyzer = iresearch::analysis::analyzers::get("text", "{\"locale\":\"en\"}");

    ASSERT_NE(nullptr, analyzer);
    ASSERT_TRUE(analyzer->reset("abc"));
  }

  // locale with provided ignored_words
  {
    auto analyzer = iresearch::analysis::analyzers::get("text", "{\"locale\":\"en\", \"ignored_words\":[\"abc\", \"def\", \"ghi\"]}");

    ASSERT_NE(nullptr, analyzer);
    ASSERT_TRUE(analyzer->reset("abc"));
  }

  // ...........................................................................
  // invalid
  // ...........................................................................

  // missing required locale
  ASSERT_EQ(nullptr, iresearch::analysis::analyzers::get("text", "{}"));

  // invalid ignored_words
  ASSERT_EQ(nullptr, iresearch::analysis::analyzers::get("text", "{{\"locale\":\"en\", \"ignored_words\":\"abc\"}}"));
  ASSERT_EQ(nullptr, iresearch::analysis::analyzers::get("text", "{{\"locale\":\"en\", \"ignored_words\":[1, 2, 3]}}"));
}
