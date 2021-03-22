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

#include "tests_config.hpp"
#include "tests_shared.hpp"
#include "analysis/analyzers.hpp"
#include "utils/runtime_utils.hpp"

namespace tests {

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
      irs::analysis::analyzers::get("text", irs::type<irs::text_format::text>::get(), "en"); // stream needed only to load stopwords

      if (czOldStopwordPath) {
        iresearch::setenv(text_stopword_path_var, sOldStopwordPath.c_str(), true);
      }
    }
  }

  virtual void TearDown() {
    // Code here will be called immediately after each test (right before the destructor).
  }
};

}

using namespace tests;

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

TEST_F(analyzer_test, duplicate_register) {
  struct dummy_analyzer: public irs::analysis::analyzer {
    static ptr make(const irs::string_ref&) { return ptr(new dummy_analyzer()); }
    static bool normalize(const irs::string_ref&, std::string&) { return true; }
    dummy_analyzer(): irs::analysis::analyzer(irs::type<dummy_analyzer>::get()) { }
    virtual irs::attribute* get_mutable(irs::type_info::type_id) { return nullptr;}
    virtual bool next() override { return false; }
    virtual bool reset(const irs::string_ref&) override { return false; }
  };

  static bool initial_expected = true;

  // check required for tests with repeat (static maps are not cleared between runs)
  if (initial_expected) {
    ASSERT_FALSE(irs::analysis::analyzers::exists(irs::type<dummy_analyzer>::name(), irs::type<irs::text_format::vpack>::get()));
    ASSERT_FALSE(irs::analysis::analyzers::exists(irs::type<dummy_analyzer>::name(), irs::type<irs::text_format::json>::get()));
    ASSERT_FALSE(irs::analysis::analyzers::exists(irs::type<dummy_analyzer>::name(), irs::type<irs::text_format::text>::get()));
    ASSERT_EQ(nullptr, irs::analysis::analyzers::get(irs::type<dummy_analyzer>::name(), irs::type<irs::text_format::json>::get(), irs::string_ref::NIL));
    ASSERT_EQ(nullptr, irs::analysis::analyzers::get(irs::type<dummy_analyzer>::name(), irs::type<irs::text_format::text>::get(), irs::string_ref::NIL));
    ASSERT_EQ(nullptr, irs::analysis::analyzers::get(irs::type<dummy_analyzer>::name(), irs::type<irs::text_format::vpack>::get(), irs::string_ref::NIL));

    irs::analysis::analyzer_registrar initial0(irs::type<dummy_analyzer>::get(),
                                               irs::type<irs::text_format::vpack>::get(),
                                               &dummy_analyzer::make,
                                               &dummy_analyzer::normalize);
    irs::analysis::analyzer_registrar initial1(irs::type<dummy_analyzer>::get(),
                                               irs::type<irs::text_format::json>::get(), &dummy_analyzer::make,
                                               &dummy_analyzer::normalize);
    irs::analysis::analyzer_registrar initial2(irs::type<dummy_analyzer>::get(),
                                               irs::type<irs::text_format::text>::get(), &dummy_analyzer::make,
                                               &dummy_analyzer::normalize);
    ASSERT_EQ(!initial_expected, !initial0);
    ASSERT_EQ(!initial_expected, !initial1);
    ASSERT_EQ(!initial_expected, !initial2);
  }

  initial_expected = false; // next test iteration will not be able to register the same analyzer
  irs::analysis::analyzer_registrar duplicate0(irs::type<dummy_analyzer>::get(),
                                               irs::type<irs::text_format::vpack>::get(),
                                               &dummy_analyzer::make,
                                               &dummy_analyzer::normalize);
  irs::analysis::analyzer_registrar duplicate1(irs::type<dummy_analyzer>::get(),
                                               irs::type<irs::text_format::json>::get(),
                                               &dummy_analyzer::make,
                                               &dummy_analyzer::normalize);
  irs::analysis::analyzer_registrar duplicate2(irs::type<dummy_analyzer>::get(),
                                               irs::type<irs::text_format::text>::get(),
                                               &dummy_analyzer::make,
                                               &dummy_analyzer::normalize);
  ASSERT_TRUE(!duplicate0);
  ASSERT_TRUE(!duplicate1);
  ASSERT_TRUE(!duplicate2);

  ASSERT_TRUE(irs::analysis::analyzers::exists(irs::type<dummy_analyzer>::name(), irs::type<irs::text_format::vpack>::get()));
  ASSERT_TRUE(irs::analysis::analyzers::exists(irs::type<dummy_analyzer>::name(), irs::type<irs::text_format::json>::get()));
  ASSERT_TRUE(irs::analysis::analyzers::exists(irs::type<dummy_analyzer>::name(), irs::type<irs::text_format::text>::get()));
  ASSERT_NE(nullptr, irs::analysis::analyzers::get(irs::type<dummy_analyzer>::name(), irs::type<irs::text_format::vpack>::get(), irs::string_ref::NIL));
  ASSERT_NE(nullptr, irs::analysis::analyzers::get(irs::type<dummy_analyzer>::name(), irs::type<irs::text_format::json>::get(), irs::string_ref::NIL));
  ASSERT_NE(nullptr, irs::analysis::analyzers::get(irs::type<dummy_analyzer>::name(), irs::type<irs::text_format::text>::get(), irs::string_ref::NIL));
}

TEST_F(analyzer_test, test_load) {
  {
    auto analyzer = irs::analysis::analyzers::get("text", irs::type<irs::text_format::text>::get(), "en");

    ASSERT_NE(nullptr, analyzer);
    ASSERT_TRUE(analyzer->reset("abc"));
  }

  // locale with default ingnored_words
  {
    auto analyzer = irs::analysis::analyzers::get("text", irs::type<irs::text_format::json>::get(), "{\"locale\":\"en\"}");

    ASSERT_NE(nullptr, analyzer);
    ASSERT_TRUE(analyzer->reset("abc"));
  }

  // locale with provided ignored_words
  {
    auto analyzer = irs::analysis::analyzers::get("text", irs::type<irs::text_format::json>::get(), "{\"locale\":\"en\", \"stopwords\":[\"abc\", \"def\", \"ghi\"]}");

    ASSERT_NE(nullptr, analyzer);
    ASSERT_TRUE(analyzer->reset("abc"));
  }

  // ...........................................................................
  // invalid
  // ...........................................................................

  // missing required locale
  ASSERT_EQ(nullptr, irs::analysis::analyzers::get("text", irs::type<irs::text_format::json>::get(), "{}"));

  // invalid ignored_words
  ASSERT_EQ(nullptr, irs::analysis::analyzers::get("text", irs::type<irs::text_format::json>::get(), "{{\"locale\":\"en\", \"stopwords\":\"abc\"}}"));
  ASSERT_EQ(nullptr, irs::analysis::analyzers::get("text", irs::type<irs::text_format::json>::get(), "{{\"locale\":\"en\", \"stopwords\":[1, 2, 3]}}"));
}
