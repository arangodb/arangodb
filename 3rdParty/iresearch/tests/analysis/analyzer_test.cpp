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

#if defined(_MSC_VER)
  #pragma warning(disable: 4229)
#endif

  #include <unicode/uclean.h> // for u_cleanup

#if defined(_MSC_VER)
  #pragma warning(default: 4229)
#endif

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
    irs::attribute_view attrs_;
    DECLARE_ANALYZER_TYPE() { static irs::analysis::analyzer::type_id type("dummy_analyzer"); return type; }
    static ptr make(const irs::string_ref&) { return ptr(new dummy_analyzer()); }
    dummy_analyzer(): irs::analysis::analyzer(dummy_analyzer::type()) { }
    virtual const irs::attribute_view& attributes() const NOEXCEPT override { return attrs_; }
    virtual bool next() override { return false; }
    virtual bool reset(const irs::string_ref&) override { return false; }
  };

  static bool initial_expected = true;

  // check required for tests with repeat (static maps are not cleared between runs)
  if (initial_expected) {
    ASSERT_FALSE(irs::analysis::analyzers::exists("dummy_analyzer"));
    ASSERT_EQ(nullptr, irs::analysis::analyzers::get("dummy_analyzer", irs::string_ref::nil));

    irs::analysis::analyzer_registrar initial0(dummy_analyzer::type(), irs::text_format::csv, &dummy_analyzer::make);
    irs::analysis::analyzer_registrar initial1(dummy_analyzer::type(), irs::text_format::json, &dummy_analyzer::make);
    irs::analysis::analyzer_registrar initial2(dummy_analyzer::type(), irs::text_format::text, &dummy_analyzer::make);
    irs::analysis::analyzer_registrar initial3(dummy_analyzer::type(), irs::text_format::xml, &dummy_analyzer::make);
    ASSERT_EQ(!initial_expected, !initial0);
    /* FIXME TODO enable once type diferentiation is supported
    ASSERT_EQ(!initial_expected, !initial1);
    ASSERT_EQ(!initial_expected, !initial2);
    ASSERT_EQ(!initial_expected, !initial3);
    */
  }

  initial_expected = false; // next test iteration will not be able to register the same analyzer
  irs::analysis::analyzer_registrar duplicate0(dummy_analyzer::type(), irs::text_format::csv, &dummy_analyzer::make);
  irs::analysis::analyzer_registrar duplicate1(dummy_analyzer::type(), irs::text_format::json, &dummy_analyzer::make);
  irs::analysis::analyzer_registrar duplicate2(dummy_analyzer::type(), irs::text_format::text, &dummy_analyzer::make);
  irs::analysis::analyzer_registrar duplicate3(dummy_analyzer::type(), irs::text_format::xml, &dummy_analyzer::make);
  ASSERT_TRUE(!duplicate0);
  ASSERT_TRUE(!duplicate1);
  ASSERT_TRUE(!duplicate2);
  ASSERT_TRUE(!duplicate3);

  ASSERT_TRUE(irs::analysis::analyzers::exists("dummy_analyzer"));
  ASSERT_NE(nullptr, irs::analysis::analyzers::get("dummy_analyzer", irs::string_ref::nil));
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

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------