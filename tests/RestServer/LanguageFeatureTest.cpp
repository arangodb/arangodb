////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2022 ArangoDB GmbH, Cologne, Germany
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
/// @author Alexey Bakharew
////////////////////////////////////////////////////////////////////////////////

#include "gtest/gtest.h"

#include "ApplicationFeatures/LanguageFeature.h"
#include "RestServer/LanguageCheckFeature.h"
#include "RestServer/AqlFeature.h"
#include "RestServer/InitDatabaseFeature.h"
#include "RestServer/DatabaseFeature.h"
#include "Basics/directories.h"
#include "Servers.h"
#include "Basics/Utf8Helper.h"
#include <unicode/coll.h>
#include <unicode/uclean.h>
#include <unicode/urename.h>
#include "Basics/files.h"
#include "Basics/icu-helper.h"
#include "IResearch/common.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                 setup / tear-down
// -----------------------------------------------------------------------------

namespace {
void checkCollatorSettings(const std::string& language,
                           bool isDefaultLanguage) {
  // Create collator with expected language
  UErrorCode status = U_ZERO_ERROR;
  icu::Collator* expectedColl = nullptr;
  if (language == "") {
    // get default collator for empty language
    expectedColl = icu::Collator::createInstance(status);
  } else {
    icu::Locale locale(language.c_str());
    expectedColl = icu::Collator::createInstance(locale, status);
  }

  if (isDefaultLanguage) {
    // set the default attributes for sorting:
    expectedColl->setAttribute(UCOL_CASE_FIRST, UCOL_UPPER_FIRST,
                               status);  // A < a
    expectedColl->setAttribute(UCOL_NORMALIZATION_MODE, UCOL_OFF,
                               status);  // no normalization
    expectedColl->setAttribute(
        UCOL_STRENGTH, UCOL_IDENTICAL,
        status);  // UCOL_IDENTICAL, UCOL_PRIMARY, UCOL_SECONDARY, UCOL_TERTIARY
  }

  ASSERT_FALSE(U_FAILURE(status));

  // Get actual collator
  const auto* actualColl =
      arangodb::basics::Utf8Helper::DefaultUtf8Helper.getCollator();
  status = U_ZERO_ERROR;

  ASSERT_EQ(expectedColl->getAttribute(UCOL_CASE_FIRST, status),
            actualColl->getAttribute(UCOL_CASE_FIRST, status));
  ASSERT_FALSE(U_FAILURE(status));

  ASSERT_EQ(expectedColl->getAttribute(UCOL_NORMALIZATION_MODE, status),
            actualColl->getAttribute(UCOL_NORMALIZATION_MODE, status));
  ASSERT_FALSE(U_FAILURE(status));

  ASSERT_EQ(expectedColl->getAttribute(UCOL_STRENGTH, status),
            actualColl->getAttribute(UCOL_STRENGTH, status));
  ASSERT_FALSE(U_FAILURE(status));

  delete expectedColl;
}

void checkLanguageFile(const arangodb::ArangodServer& server,
                       const std::string expectedLang,
                       const std::string& expectedParameter,
                       bool shouldBeEqual) {
  std::string key = expectedParameter;
  if (key == "default-language") {
    // Because value for 'default-language' parameter store
    // under 'default' key in LANGUAGE json file.
    // For 'icu-language' parameter and key are equal
    key = "default";
  }

  auto& databasePath = server.getFeature<arangodb::DatabasePathFeature>();
  std::string filename = databasePath.subdirectoryName("LANGUAGE");

  EXPECT_TRUE(TRI_ExistsFile(filename.c_str()));

  try {
    VPackBuilder builder =
        arangodb::basics::VelocyPackHelper::velocyPackFromFile(filename);
    VPackSlice content = builder.slice();

    ASSERT_TRUE(content.isObject());

    // We expect only one key in this file
    ASSERT_TRUE(content.length() == 1);

    VPackSlice actualSlice = content.get(key);

    ASSERT_TRUE(actualSlice.isString());

    auto actualLang = actualSlice.copyString();

    ASSERT_EQ(actualLang == expectedLang, shouldBeEqual);

  } catch (...) {
    ASSERT_TRUE(false);
  }
}
}  // namespace

using namespace arangodb::options;

class ArangoLanguageFeatureTest
    : public ::testing::Test,
      public arangodb::tests::LogSuppressor<arangodb::Logger::CONFIG,
                                            arangodb::LogLevel::FATAL> {
 protected:
  arangodb::tests::mocks::MockAqlServer server;

  // Per-test-suite set-up.
  // Called before the first test in this test suite.
  // Can be omitted if not needed.
  static void SetUpTestCase() {
    auto collator =
        arangodb::basics::Utf8Helper::DefaultUtf8Helper.getCollator();
    delete collator;
    arangodb::basics::Utf8Helper::DefaultUtf8Helper.setCollator(nullptr);
    u_cleanup();
  }

  // Per-test-suite tear-down.
  // Called after the last test in this test suite.
  // Can be omitted if not needed.
  static void TearDownTestCase() { IcuInitializer::reinit(); }

  void TearDown() override { SetUpTestCase(); }

 public:
  ArangoLanguageFeatureTest() : server(false) {
    arangodb::tests::init();

    server.startFeatures();

    auto& dbPathFeature = server.getFeature<arangodb::DatabasePathFeature>();

    server.server().setBinaryPath(dbPathFeature.directory().c_str());
  }

  ~ArangoLanguageFeatureTest() = default;
};

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

TEST_F(ArangoLanguageFeatureTest, testResetLanguageDefault) {
  auto& langFeature = server.addFeatureUntracked<arangodb::LanguageFeature>();
  langFeature.collectOptions(server.server().options());
  constexpr std::string_view language1 = "ru";
  constexpr std::string_view language2 = "sv";

  server.server()
      .options()
      ->get<StringParameter>("default-language")
      ->set(language1.data());
  langFeature.validateOptions(server.server().options());

  langFeature.prepare();
  {
    auto [lang, type] = langFeature.getLanguage();
    ASSERT_EQ(lang, language1);
    ASSERT_EQ(type, arangodb::basics::LanguageType::DEFAULT);
  }

  langFeature.resetLanguage(language2, arangodb::basics::LanguageType::ICU);
  {
    auto [lang, type] = langFeature.getLanguage();
    ASSERT_EQ(lang, language2);
    ASSERT_EQ(type, arangodb::basics::LanguageType::ICU);
  }

  langFeature.resetLanguage(language1, arangodb::basics::LanguageType::DEFAULT);
  {
    auto [lang, type] = langFeature.getLanguage();
    ASSERT_EQ(lang, language1);
    ASSERT_EQ(type, arangodb::basics::LanguageType::DEFAULT);
  }
}

TEST_F(ArangoLanguageFeatureTest, testResetLanguageIcu) {
  auto& langFeature = server.addFeatureUntracked<arangodb::LanguageFeature>();
  langFeature.collectOptions(server.server().options());
  constexpr std::string_view language1 = "ru";
  constexpr std::string_view language2 = "sv";

  server.server()
      .options()
      ->get<StringParameter>("icu-language")
      ->set(language1.data());
  langFeature.validateOptions(server.server().options());

  langFeature.prepare();
  {
    auto [lang, type] = langFeature.getLanguage();
    ASSERT_EQ(lang, language1);
    ASSERT_EQ(type, arangodb::basics::LanguageType::ICU);
  }

  langFeature.resetLanguage(language2, arangodb::basics::LanguageType::DEFAULT);
  {
    auto [lang, type] = langFeature.getLanguage();
    ASSERT_EQ(lang, language2);
    ASSERT_EQ(type, arangodb::basics::LanguageType::DEFAULT);
  }

  langFeature.resetLanguage(language1, arangodb::basics::LanguageType::ICU);
  {
    auto [lang, type] = langFeature.getLanguage();
    ASSERT_EQ(lang, language1);
    ASSERT_EQ(type, arangodb::basics::LanguageType::ICU);
  }
}

TEST_F(ArangoLanguageFeatureTest, testBothArgumentsSpecifiedLangCheckTrue) {
  // Specify both language arguments and get server failure

  auto& langFeature = server.addFeatureUntracked<arangodb::LanguageFeature>();
  langFeature.collectOptions(server.server().options());

  // Enable force check for languages
  server.server()
      .options()
      ->get<BooleanParameter>("default-language-check")
      ->set("true");

  constexpr std::string_view lang = "ru";
  server.server()
      .options()
      ->get<StringParameter>("icu-language")
      ->set(lang.data());
  server.server()
      .options()
      ->get<StringParameter>("default-language")
      ->set(lang.data());

  langFeature.validateOptions(server.server().options());

  // Simulate server launch
  EXPECT_DEATH(langFeature.prepare(), "");
}

TEST_F(ArangoLanguageFeatureTest, testBothArgumentsSpecifiedLangCheckFalse) {
  // Specify both language arguments and get server failure

  auto& langFeature = server.addFeatureUntracked<arangodb::LanguageFeature>();
  langFeature.collectOptions(server.server().options());

  // Disable force check for languages
  server.server()
      .options()
      ->get<BooleanParameter>("default-language-check")
      ->set("false");

  constexpr std::string_view lang = "ru";
  server.server()
      .options()
      ->get<StringParameter>("icu-language")
      ->set(lang.data());
  server.server()
      .options()
      ->get<StringParameter>("default-language")
      ->set(lang.data());

  langFeature.validateOptions(server.server().options());

  // Simulate server launch
  EXPECT_DEATH(langFeature.prepare(), "");
}

TEST_F(ArangoLanguageFeatureTest, testDefaultLangCheckTrue) {
  // default-language-check=true
  // test behaviour of --default-language parameter

  server.addFeatureUntracked<arangodb::LanguageFeature>().collectOptions(
      server.server().options());
  server.addFeatureUntracked<arangodb::LanguageCheckFeature>();

  constexpr std::string_view firstLang = "sv";
  constexpr std::string_view secondLang = "de";
  constexpr std::string_view defaultParameter = "default-language";
  constexpr std::string_view icuParameter = "icu-language";

  // Enable force check for languages
  server.server()
      .options()
      ->get<BooleanParameter>("default-language-check")
      ->set("true");

  // Assume that it is first launch of server
  {
    auto& langFeature = server.getFeature<arangodb::LanguageFeature>();
    auto& langCheckFeature =
        server.getFeature<arangodb::LanguageCheckFeature>();

    server.server()
        .options()
        ->get<StringParameter>(defaultParameter.data())
        ->set(firstLang.data());

    langFeature.validateOptions(server.server().options());

    // Simulate server launch
    langFeature.prepare();
    langCheckFeature.start();

    bool shouldBeLangEqual = true;
    checkLanguageFile(server.server(), firstLang.data(),
                      defaultParameter.data(), shouldBeLangEqual);
    bool isDefaultLanguage = true;
    checkCollatorSettings(firstLang.data(), isDefaultLanguage);
  }

  // Assume that server is stoped
  // We launch it again with parameters
  {
    auto& langFeature = server.getFeature<arangodb::LanguageFeature>();
    auto& langCheckFeature =
        server.getFeature<arangodb::LanguageCheckFeature>();

    server.server()
        .options()
        ->get<StringParameter>(defaultParameter.data())
        ->set(firstLang.data());

    langFeature.validateOptions(server.server().options());

    // Simulate server launch
    langFeature.prepare();
    langCheckFeature.start();

    bool shouldBeLangEqual = true;
    checkLanguageFile(server.server(), firstLang.data(),
                      defaultParameter.data(), shouldBeLangEqual);
    bool isDefaultLanguage = true;
    checkCollatorSettings(firstLang.data(), isDefaultLanguage);
  }

  // Assume that server is stoped
  // We launch it again with parameters
  {
    auto& langFeature = server.getFeature<arangodb::LanguageFeature>();
    auto& langCheckFeature =
        server.getFeature<arangodb::LanguageCheckFeature>();

    // Now we try to launch server with same parameter but with another lang
    server.server()
        .options()
        ->get<StringParameter>(defaultParameter.data())
        ->set(secondLang.data());

    langFeature.validateOptions(server.server().options());

    // Simulate server launch
    langFeature.prepare();
    bool shouldBeLangEqual = false;
    checkLanguageFile(server.server(), secondLang.data(),
                      defaultParameter.data(), shouldBeLangEqual);
    EXPECT_DEATH(langCheckFeature.start(), "");
  }

  // Assume that server is stoped
  // We launch it again with parameters
  {
    auto& langFeature = server.getFeature<arangodb::LanguageFeature>();
    auto& langCheckFeature =
        server.getFeature<arangodb::LanguageCheckFeature>();

    // Now we try to launch server with different parameter
    server.server()
        .options()
        ->get<StringParameter>(defaultParameter.data())
        ->set("");  // clear value for defaultParameter
    server.server()
        .options()
        ->get<StringParameter>(icuParameter.data())
        ->set(secondLang.data());

    langFeature.validateOptions(server.server().options());

    // Simulate server launch
    langFeature.prepare();
    EXPECT_DEATH(langCheckFeature.start(), "");
  }

  // Assume that server is stoped
  // We launch it again with parameters
  {
    auto& langFeature = server.getFeature<arangodb::LanguageFeature>();
    auto& langCheckFeature =
        server.getFeature<arangodb::LanguageCheckFeature>();

    server.server()
        .options()
        ->get<StringParameter>(defaultParameter.data())
        ->set("");  // clear value for parameter
    server.server()
        .options()
        ->get<StringParameter>(icuParameter.data())
        ->set("");  // clear value for parameter

    langFeature.validateOptions(server.server().options());

    // Simulate server launch
    langFeature.prepare();
    langCheckFeature.start();

    bool shouldBeLangEqual = true;
    checkLanguageFile(server.server(), firstLang.data(),
                      defaultParameter.data(), shouldBeLangEqual);
    bool isDefaultLanguage = true;
    checkCollatorSettings(firstLang.data(), isDefaultLanguage);
  }
}

TEST_F(ArangoLanguageFeatureTest, testDefaultLangCheckFalse) {
  // default-language-check=true
  // test behaviour of --default-language parameter

  server.addFeatureUntracked<arangodb::LanguageFeature>().collectOptions(
      server.server().options());
  server.addFeatureUntracked<arangodb::LanguageCheckFeature>();

  constexpr std::string_view firstLang = "sv";
  constexpr std::string_view secondLang = "de";
  constexpr std::string_view defaultParameter = "default-language";
  constexpr std::string_view icuParameter = "icu-language";

  // Disable force check for languages
  server.server()
      .options()
      ->get<BooleanParameter>("default-language-check")
      ->set("false");

  // Assume that it is first launch of server
  {
    auto& langFeature = server.getFeature<arangodb::LanguageFeature>();
    auto& langCheckFeature =
        server.getFeature<arangodb::LanguageCheckFeature>();

    server.server()
        .options()
        ->get<StringParameter>(defaultParameter.data())
        ->set(firstLang.data());

    langFeature.validateOptions(server.server().options());

    // Simulate server launch
    langFeature.prepare();
    langCheckFeature.start();

    bool shouldBeLangEqual = true;
    checkLanguageFile(server.server(), firstLang.data(),
                      defaultParameter.data(), shouldBeLangEqual);
    bool isDefaultLanguage = true;
    checkCollatorSettings(firstLang.data(), isDefaultLanguage);
  }

  // Assume that server is stoped
  // We launch it again with parameters
  {
    auto& langFeature = server.getFeature<arangodb::LanguageFeature>();
    auto& langCheckFeature =
        server.getFeature<arangodb::LanguageCheckFeature>();

    server.server()
        .options()
        ->get<StringParameter>(defaultParameter.data())
        ->set(firstLang.data());

    langFeature.validateOptions(server.server().options());

    // Simulate server launch
    langFeature.prepare();
    langCheckFeature.start();

    bool shouldBeLangEqual = true;
    checkLanguageFile(server.server(), firstLang.data(),
                      defaultParameter.data(), shouldBeLangEqual);
    bool isDefaultLanguage = true;
    checkCollatorSettings(firstLang.data(), isDefaultLanguage);
  }

  // Assume that server is stoped
  // We launch it again with parameters
  {
    auto& langFeature = server.getFeature<arangodb::LanguageFeature>();
    auto& langCheckFeature =
        server.getFeature<arangodb::LanguageCheckFeature>();

    // Now we try to launch server with same parameter but with another lang
    server.server()
        .options()
        ->get<StringParameter>(defaultParameter.data())
        ->set(secondLang.data());

    langFeature.validateOptions(server.server().options());

    // Simulate server launch
    langFeature.prepare();
    bool shouldBeLangEqual = false;
    checkLanguageFile(server.server(), secondLang.data(),
                      defaultParameter.data(), shouldBeLangEqual);
    langCheckFeature.start();
  }

  // Assume that server is stoped
  // We launch it again with parameters
  {
    auto& langFeature = server.getFeature<arangodb::LanguageFeature>();
    auto& langCheckFeature =
        server.getFeature<arangodb::LanguageCheckFeature>();

    // Now we try to launch server with different parameter
    server.server()
        .options()
        ->get<StringParameter>(defaultParameter.data())
        ->set("");  // clear value for defaultParameter
    server.server()
        .options()
        ->get<StringParameter>(icuParameter.data())
        ->set(secondLang.data());

    langFeature.validateOptions(server.server().options());

    // Simulate server launch
    langFeature.prepare();
    langCheckFeature.start();
  }

  // Assume that server is stoped
  // We launch it again with parameters
  {
    auto& langFeature = server.getFeature<arangodb::LanguageFeature>();
    auto& langCheckFeature =
        server.getFeature<arangodb::LanguageCheckFeature>();

    server.server()
        .options()
        ->get<StringParameter>(defaultParameter.data())
        ->set("");  // clear value for parameter
    server.server()
        .options()
        ->get<StringParameter>(icuParameter.data())
        ->set("");  // clear value for parameter

    langFeature.validateOptions(server.server().options());

    // Simulate server launch
    langFeature.prepare();
    langCheckFeature.start();

    bool shouldBeLangEqual = true;
    checkLanguageFile(server.server(), firstLang.data(),
                      defaultParameter.data(), shouldBeLangEqual);
    bool isDefaultLanguage = true;
    checkCollatorSettings(firstLang.data(), isDefaultLanguage);
  }
}

TEST_F(ArangoLanguageFeatureTest, testEmptyLangCheckTrue) {
  // default-language-check=true
  // test behaviour of parameters

  server.addFeatureUntracked<arangodb::LanguageFeature>().collectOptions(
      server.server().options());
  server.addFeatureUntracked<arangodb::LanguageCheckFeature>();

  // firstLang in this case will be initialized after langFeature.prepare()
  // because collator will be ready after prepare()
  std::string firstLang = {};
  constexpr std::string_view secondLang = "de";
  constexpr std::string_view defaultParameter = "default-language";
  constexpr std::string_view icuParameter = "icu-language";

  // Enable force check for languages
  server.server()
      .options()
      ->get<BooleanParameter>("default-language-check")
      ->set("true");

  // Assume that it is first launch of server
  {
    auto& langFeature = server.getFeature<arangodb::LanguageFeature>();
    auto& langCheckFeature =
        server.getFeature<arangodb::LanguageCheckFeature>();

    server.server()
        .options()
        ->get<StringParameter>(defaultParameter.data())
        ->set("");
    server.server()
        .options()
        ->get<StringParameter>(icuParameter.data())
        ->set("");

    langFeature.validateOptions(server.server().options());

    // Simulate server launch
    langFeature.prepare();
    firstLang = langFeature.getCollatorLanguage();
    langCheckFeature.start();

    bool shouldBeLangEqual = true;
    checkLanguageFile(server.server(), firstLang.data(),
                      defaultParameter.data(), shouldBeLangEqual);
    bool isDefaultLanguage = true;
    checkCollatorSettings(firstLang, isDefaultLanguage);
  }

  // Assume that server is stoped
  // We launch it again with parameters
  {
    auto& langFeature = server.getFeature<arangodb::LanguageFeature>();
    auto& langCheckFeature =
        server.getFeature<arangodb::LanguageCheckFeature>();

    server.server()
        .options()
        ->get<StringParameter>(defaultParameter.data())
        ->set(firstLang.data());

    langFeature.validateOptions(server.server().options());

    // Simulate server launch
    langFeature.prepare();
    langCheckFeature.start();

    bool shouldBeLangEqual = true;
    checkLanguageFile(server.server(), firstLang, defaultParameter.data(),
                      shouldBeLangEqual);
    bool isDefaultLanguage = true;
    checkCollatorSettings(firstLang, isDefaultLanguage);
  }

  // Assume that server is stoped
  // We launch it again with parameters
  {
    auto& langFeature = server.getFeature<arangodb::LanguageFeature>();
    auto& langCheckFeature =
        server.getFeature<arangodb::LanguageCheckFeature>();

    // Now we try to launch server with same parameter but with another lang
    server.server()
        .options()
        ->get<StringParameter>(defaultParameter.data())
        ->set(secondLang.data());

    langFeature.validateOptions(server.server().options());

    // Simulate server launch
    langFeature.prepare();
    bool shouldBeLangEqual = false;
    checkLanguageFile(server.server(), secondLang.data(),
                      defaultParameter.data(), shouldBeLangEqual);
    EXPECT_DEATH(langCheckFeature.start(), "");
  }

  // Assume that server is stoped
  // We launch it again with parameters
  {
    auto& langFeature = server.getFeature<arangodb::LanguageFeature>();
    auto& langCheckFeature =
        server.getFeature<arangodb::LanguageCheckFeature>();

    // Now we try to launch server with different parameter
    server.server()
        .options()
        ->get<StringParameter>(defaultParameter.data())
        ->set("");  // clear value for defaultParameter
    server.server()
        .options()
        ->get<StringParameter>(icuParameter.data())
        ->set(secondLang.data());

    langFeature.validateOptions(server.server().options());

    // Simulate server launch
    langFeature.prepare();
    EXPECT_DEATH(langCheckFeature.start(), "");
  }

  // Assume that server is stoped
  // We launch it again with parameters
  {
    auto& langFeature = server.getFeature<arangodb::LanguageFeature>();
    auto& langCheckFeature =
        server.getFeature<arangodb::LanguageCheckFeature>();

    server.server()
        .options()
        ->get<StringParameter>(defaultParameter.data())
        ->set("");  // clear value for parameter
    server.server()
        .options()
        ->get<StringParameter>(icuParameter.data())
        ->set("");  // clear value for parameter

    langFeature.validateOptions(server.server().options());

    // Simulate server launch
    langFeature.prepare();
    langCheckFeature.start();

    bool shouldBeLangEqual = true;
    checkLanguageFile(server.server(), firstLang, defaultParameter.data(),
                      shouldBeLangEqual);
    bool isDefaultLanguage = true;
    checkCollatorSettings(firstLang, isDefaultLanguage);
  }
}

TEST_F(ArangoLanguageFeatureTest, testEmptyLangCheckFalse) {
  // default-language-check=true
  // test behaviour of parameters

  server.addFeatureUntracked<arangodb::LanguageFeature>().collectOptions(
      server.server().options());
  server.addFeatureUntracked<arangodb::LanguageCheckFeature>();

  // firstLang in this case will be initialized after langFeature.prepare()
  // because collator will be ready after prepare()
  std::string firstLang = {};
  constexpr std::string_view secondLang = "de";
  constexpr std::string_view defaultParameter = "default-language";
  constexpr std::string_view icuParameter = "icu-language";

  // Disable force check for languages
  server.server()
      .options()
      ->get<BooleanParameter>("default-language-check")
      ->set("false");

  // Assume that it is first launch of server
  {
    auto& langFeature = server.getFeature<arangodb::LanguageFeature>();
    auto& langCheckFeature =
        server.getFeature<arangodb::LanguageCheckFeature>();

    server.server()
        .options()
        ->get<StringParameter>(defaultParameter.data())
        ->set("");
    server.server()
        .options()
        ->get<StringParameter>(icuParameter.data())
        ->set("");

    langFeature.validateOptions(server.server().options());

    // Simulate server launch
    langFeature.prepare();
    firstLang = langFeature.getCollatorLanguage();
    langCheckFeature.start();

    bool shouldBeLangEqual = true;
    checkLanguageFile(server.server(), firstLang.data(),
                      defaultParameter.data(), shouldBeLangEqual);
    bool isDefaultLanguage = true;
    checkCollatorSettings(firstLang, isDefaultLanguage);
  }

  // Assume that server is stoped
  // We launch it again with parameters
  {
    auto& langFeature = server.getFeature<arangodb::LanguageFeature>();
    auto& langCheckFeature =
        server.getFeature<arangodb::LanguageCheckFeature>();

    server.server()
        .options()
        ->get<StringParameter>(defaultParameter.data())
        ->set(firstLang.data());

    langFeature.validateOptions(server.server().options());

    // Simulate server launch
    langFeature.prepare();
    langCheckFeature.start();

    bool shouldBeLangEqual = true;
    checkLanguageFile(server.server(), firstLang, defaultParameter.data(),
                      shouldBeLangEqual);
    bool isDefaultLanguage = true;
    checkCollatorSettings(firstLang, isDefaultLanguage);
  }

  // Assume that server is stoped
  // We launch it again with parameters
  {
    auto& langFeature = server.getFeature<arangodb::LanguageFeature>();
    auto& langCheckFeature =
        server.getFeature<arangodb::LanguageCheckFeature>();

    // Now we try to launch server with same parameter but with another lang
    server.server()
        .options()
        ->get<StringParameter>(defaultParameter.data())
        ->set(secondLang.data());

    langFeature.validateOptions(server.server().options());

    // Simulate server launch
    langFeature.prepare();
    bool shouldBeLangEqual = false;
    checkLanguageFile(server.server(), secondLang.data(),
                      defaultParameter.data(), shouldBeLangEqual);
    langCheckFeature.start();
  }

  // Assume that server is stoped
  // We launch it again with parameters
  {
    auto& langFeature = server.getFeature<arangodb::LanguageFeature>();
    auto& langCheckFeature =
        server.getFeature<arangodb::LanguageCheckFeature>();

    // Now we try to launch server with different parameter
    server.server()
        .options()
        ->get<StringParameter>(defaultParameter.data())
        ->set("");  // clear value for defaultParameter
    server.server()
        .options()
        ->get<StringParameter>(icuParameter.data())
        ->set(secondLang.data());

    langFeature.validateOptions(server.server().options());

    // Simulate server launch
    langFeature.prepare();
    langCheckFeature.start();
  }

  // Assume that server is stoped
  // We launch it again with parameters
  {
    auto& langFeature = server.getFeature<arangodb::LanguageFeature>();
    auto& langCheckFeature =
        server.getFeature<arangodb::LanguageCheckFeature>();

    server.server()
        .options()
        ->get<StringParameter>(defaultParameter.data())
        ->set("");  // clear value for parameter
    server.server()
        .options()
        ->get<StringParameter>(icuParameter.data())
        ->set("");  // clear value for parameter

    langFeature.validateOptions(server.server().options());

    // Simulate server launch
    langFeature.prepare();
    langCheckFeature.start();

    bool shouldBeLangEqual = true;
    checkLanguageFile(server.server(), firstLang, defaultParameter.data(),
                      shouldBeLangEqual);
    bool isDefaultLanguage = true;
    checkCollatorSettings(firstLang, isDefaultLanguage);
  }
}

TEST_F(ArangoLanguageFeatureTest, testIcuLangCheckTrue) {
  // default-language-check=true
  // test behaviour of --icu-language parameter

  server.addFeatureUntracked<arangodb::LanguageFeature>().collectOptions(
      server.server().options());
  server.addFeatureUntracked<arangodb::LanguageCheckFeature>();

  constexpr std::string_view firstLang = "sv";
  constexpr std::string_view secondLang = "de";
  constexpr std::string_view defaultParameter = "default-language";
  constexpr std::string_view icuParameter = "icu-language";

  // Enable force check for languages
  server.server()
      .options()
      ->get<BooleanParameter>("default-language-check")
      ->set("true");

  // Assume that it is first launch of server
  {
    auto& langFeature = server.getFeature<arangodb::LanguageFeature>();
    auto& langCheckFeature =
        server.getFeature<arangodb::LanguageCheckFeature>();

    server.server()
        .options()
        ->get<StringParameter>(icuParameter.data())
        ->set(firstLang.data());

    langFeature.validateOptions(server.server().options());

    // Simulate server launch
    langFeature.prepare();
    langCheckFeature.start();

    bool shouldBeLangEqual = true;
    checkLanguageFile(server.server(), firstLang.data(), icuParameter.data(),
                      shouldBeLangEqual);
    bool isDefaultLanguage = false;
    checkCollatorSettings(firstLang.data(), isDefaultLanguage);
  }

  // Assume that server is stoped
  // We launch it again with parameters
  {
    auto& langFeature = server.getFeature<arangodb::LanguageFeature>();
    auto& langCheckFeature =
        server.getFeature<arangodb::LanguageCheckFeature>();

    server.server()
        .options()
        ->get<StringParameter>(icuParameter.data())
        ->set(firstLang.data());

    langFeature.validateOptions(server.server().options());

    // Simulate server launch
    langFeature.prepare();
    langCheckFeature.start();

    bool shouldBeLangEqual = true;
    checkLanguageFile(server.server(), firstLang.data(), icuParameter.data(),
                      shouldBeLangEqual);
    bool isDefaultLanguage = false;
    checkCollatorSettings(firstLang.data(), isDefaultLanguage);
  }

  // Assume that server is stoped
  // We launch it again with parameters
  {
    auto& langFeature = server.getFeature<arangodb::LanguageFeature>();
    auto& langCheckFeature =
        server.getFeature<arangodb::LanguageCheckFeature>();

    // Now we try to launch server with same parameter but with another lang
    server.server()
        .options()
        ->get<StringParameter>(icuParameter.data())
        ->set(secondLang.data());

    langFeature.validateOptions(server.server().options());

    // Simulate server launch
    langFeature.prepare();
    bool shouldBeLangEqual = false;
    checkLanguageFile(server.server(), secondLang.data(), icuParameter.data(),
                      shouldBeLangEqual);
    EXPECT_DEATH(langCheckFeature.start(), "");
  }

  // Assume that server is stoped
  // We launch it again with parameters
  {
    auto& langFeature = server.getFeature<arangodb::LanguageFeature>();
    auto& langCheckFeature =
        server.getFeature<arangodb::LanguageCheckFeature>();

    // Now we try to launch server with different parameter
    server.server()
        .options()
        ->get<StringParameter>(icuParameter.data())
        ->set("");  // clear value for defaultParameter
    server.server()
        .options()
        ->get<StringParameter>(defaultParameter.data())
        ->set(secondLang.data());

    langFeature.validateOptions(server.server().options());

    // Simulate server launch
    langFeature.prepare();
    EXPECT_DEATH(langCheckFeature.start(), "");
  }

  // Assume that server is stoped
  // We launch it again with parameters
  {
    auto& langFeature = server.getFeature<arangodb::LanguageFeature>();
    auto& langCheckFeature =
        server.getFeature<arangodb::LanguageCheckFeature>();

    server.server()
        .options()
        ->get<StringParameter>(defaultParameter.data())
        ->set("");  // clear value for parameter
    server.server()
        .options()
        ->get<StringParameter>(icuParameter.data())
        ->set("");  // clear value for parameter

    langFeature.validateOptions(server.server().options());

    // Simulate server launch
    langFeature.prepare();
    langCheckFeature.start();

    bool shouldBeLangEqual = true;
    checkLanguageFile(server.server(), firstLang.data(), icuParameter.data(),
                      shouldBeLangEqual);
    bool isDefaultLanguage = false;
    checkCollatorSettings(firstLang.data(), isDefaultLanguage);
  }
}

TEST_F(ArangoLanguageFeatureTest, testIcuLangCheckFalse) {
  // default-language-check=true
  // test behaviour of --icu-language parameter

  server.addFeatureUntracked<arangodb::LanguageFeature>().collectOptions(
      server.server().options());
  server.addFeatureUntracked<arangodb::LanguageCheckFeature>();

  constexpr std::string_view firstLang = "sv";
  constexpr std::string_view secondLang = "de";
  constexpr std::string_view defaultParameter = "default-language";
  constexpr std::string_view icuParameter = "icu-language";

  // Disable force check for languages
  server.server()
      .options()
      ->get<BooleanParameter>("default-language-check")
      ->set("false");

  // Assume that it is first launch of server
  {
    auto& langFeature = server.getFeature<arangodb::LanguageFeature>();
    auto& langCheckFeature =
        server.getFeature<arangodb::LanguageCheckFeature>();

    server.server()
        .options()
        ->get<StringParameter>(icuParameter.data())
        ->set(firstLang.data());

    langFeature.validateOptions(server.server().options());

    // Simulate server launch
    langFeature.prepare();
    langCheckFeature.start();

    bool shouldBeLangEqual = true;
    checkLanguageFile(server.server(), firstLang.data(), icuParameter.data(),
                      shouldBeLangEqual);
    bool isDefaultLanguage = false;
    checkCollatorSettings(firstLang.data(), isDefaultLanguage);
  }

  // Assume that server is stoped
  // We launch it again with parameters
  {
    auto& langFeature = server.getFeature<arangodb::LanguageFeature>();
    auto& langCheckFeature =
        server.getFeature<arangodb::LanguageCheckFeature>();

    server.server()
        .options()
        ->get<StringParameter>(icuParameter.data())
        ->set(firstLang.data());

    langFeature.validateOptions(server.server().options());

    // Simulate server launch
    langFeature.prepare();
    langCheckFeature.start();

    bool shouldBeLangEqual = true;
    checkLanguageFile(server.server(), firstLang.data(), icuParameter.data(),
                      shouldBeLangEqual);
    bool isDefaultLanguage = false;
    checkCollatorSettings(firstLang.data(), isDefaultLanguage);
  }

  // Assume that server is stoped
  // We launch it again with parameters
  {
    auto& langFeature = server.getFeature<arangodb::LanguageFeature>();
    auto& langCheckFeature =
        server.getFeature<arangodb::LanguageCheckFeature>();

    // Now we try to launch server with same parameter but with another lang
    server.server()
        .options()
        ->get<StringParameter>(icuParameter.data())
        ->set(secondLang.data());

    langFeature.validateOptions(server.server().options());

    // Simulate server launch
    langFeature.prepare();
    bool shouldBeLangEqual = false;
    checkLanguageFile(server.server(), secondLang.data(), icuParameter.data(),
                      shouldBeLangEqual);
    langCheckFeature.start();
  }

  // Assume that server is stoped
  // We launch it again with parameters
  {
    auto& langFeature = server.getFeature<arangodb::LanguageFeature>();
    auto& langCheckFeature =
        server.getFeature<arangodb::LanguageCheckFeature>();

    // Now we try to launch server with different parameter
    server.server()
        .options()
        ->get<StringParameter>(icuParameter.data())
        ->set("");  // clear value for defaultParameter
    server.server()
        .options()
        ->get<StringParameter>(defaultParameter.data())
        ->set(secondLang.data());

    langFeature.validateOptions(server.server().options());

    // Simulate server launch
    langFeature.prepare();
    langCheckFeature.start();
  }

  // Assume that server is stoped
  // We launch it again with parameters
  {
    auto& langFeature = server.getFeature<arangodb::LanguageFeature>();
    auto& langCheckFeature =
        server.getFeature<arangodb::LanguageCheckFeature>();

    server.server()
        .options()
        ->get<StringParameter>(defaultParameter.data())
        ->set("");  // clear value for parameter
    server.server()
        .options()
        ->get<StringParameter>(icuParameter.data())
        ->set("");  // clear value for parameter

    langFeature.validateOptions(server.server().options());

    // Simulate server launch
    langFeature.prepare();
    langCheckFeature.start();

    bool shouldBeLangEqual = true;
    checkLanguageFile(server.server(), firstLang.data(), icuParameter.data(),
                      shouldBeLangEqual);
    bool isDefaultLanguage = false;
    checkCollatorSettings(firstLang.data(), isDefaultLanguage);
  }
}

TEST_F(ArangoLanguageFeatureTest, testIcuWithVariantLangCheckTrue) {
  // default-language-check=true
  // test behaviour of --icu-language parameter

  server.addFeatureUntracked<arangodb::LanguageFeature>().collectOptions(
      server.server().options());
  server.addFeatureUntracked<arangodb::LanguageCheckFeature>();
  constexpr std::string_view inputFirstLang = "de@PhOneBoOk";
  constexpr std::string_view actualFirstLang = "de__PHONEBOOK";
  constexpr std::string_view secondLang = "de";
  constexpr std::string_view defaultParameter = "default-language";
  constexpr std::string_view icuParameter = "icu-language";
  // Enable force check for languages
  server.server()
      .options()
      ->get<BooleanParameter>("default-language-check")
      ->set("true");
  // Assume that it is first launch of server
  {
    auto& langFeature = server.getFeature<arangodb::LanguageFeature>();
    auto& langCheckFeature =
        server.getFeature<arangodb::LanguageCheckFeature>();
    server.server()
        .options()
        ->get<StringParameter>(icuParameter.data())
        ->set(inputFirstLang.data());
    langFeature.validateOptions(server.server().options());
    // Simulate server launch
    langFeature.prepare();
    langCheckFeature.start();
    bool shouldBeLangEqual = true;
    checkLanguageFile(server.server(), actualFirstLang.data(),
                      icuParameter.data(), shouldBeLangEqual);
    bool isDefaultLanguage = false;
    checkCollatorSettings(actualFirstLang.data(), isDefaultLanguage);
  }
  // Assume that server is stoped
  // We launch it again with parameters
  {
    auto& langFeature = server.getFeature<arangodb::LanguageFeature>();
    auto& langCheckFeature =
        server.getFeature<arangodb::LanguageCheckFeature>();
    server.server()
        .options()
        ->get<StringParameter>(icuParameter.data())
        ->set(inputFirstLang.data());
    langFeature.validateOptions(server.server().options());
    // Simulate server launch
    langFeature.prepare();
    langCheckFeature.start();
    bool shouldBeLangEqual = true;
    checkLanguageFile(server.server(), actualFirstLang.data(),
                      icuParameter.data(), shouldBeLangEqual);
    bool isDefaultLanguage = false;
    checkCollatorSettings(actualFirstLang.data(), isDefaultLanguage);
  }
  // Assume that server is stoped
  // We launch it again with parameters
  {
    auto& langFeature = server.getFeature<arangodb::LanguageFeature>();
    auto& langCheckFeature =
        server.getFeature<arangodb::LanguageCheckFeature>();
    // Now we try to launch server with same parameter but with another lang
    server.server()
        .options()
        ->get<StringParameter>(icuParameter.data())
        ->set(secondLang.data());
    langFeature.validateOptions(server.server().options());
    // Simulate server launch
    langFeature.prepare();
    bool shouldBeLangEqual = false;
    checkLanguageFile(server.server(), secondLang.data(), icuParameter.data(),
                      shouldBeLangEqual);
    EXPECT_DEATH(langCheckFeature.start(), "");
  }
  // Assume that server is stoped
  // We launch it again with parameters
  {
    auto& langFeature = server.getFeature<arangodb::LanguageFeature>();
    auto& langCheckFeature =
        server.getFeature<arangodb::LanguageCheckFeature>();
    // Now we try to launch server with different parameter
    server.server()
        .options()
        ->get<StringParameter>(icuParameter.data())
        ->set("");  // clear value for defaultParameter
    server.server()
        .options()
        ->get<StringParameter>(defaultParameter.data())
        ->set(secondLang.data());
    langFeature.validateOptions(server.server().options());
    // Simulate server launch
    langFeature.prepare();
    EXPECT_DEATH(langCheckFeature.start(), "");
  }
  // Assume that server is stoped
  // We launch it again with parameters
  {
    auto& langFeature = server.getFeature<arangodb::LanguageFeature>();
    auto& langCheckFeature =
        server.getFeature<arangodb::LanguageCheckFeature>();
    server.server()
        .options()
        ->get<StringParameter>(defaultParameter.data())
        ->set("");  // clear value for parameter
    server.server()
        .options()
        ->get<StringParameter>(icuParameter.data())
        ->set("");  // clear value for parameter
    langFeature.validateOptions(server.server().options());
    // Simulate server launch
    langFeature.prepare();
    langCheckFeature.start();
    bool shouldBeLangEqual = true;
    checkLanguageFile(server.server(), actualFirstLang.data(),
                      icuParameter.data(), shouldBeLangEqual);
    bool isDefaultLanguage = false;
    checkCollatorSettings(actualFirstLang.data(), isDefaultLanguage);
  }
}

TEST_F(ArangoLanguageFeatureTest, testIcuWithCollationLangCheckTrue) {
  // default-language-check=true
  // test behaviour of --icu-language parameter

  server.addFeatureUntracked<arangodb::LanguageFeature>().collectOptions(
      server.server().options());
  server.addFeatureUntracked<arangodb::LanguageCheckFeature>();

  constexpr std::string_view firstLang = "de@collation=phonebook";
  constexpr std::string_view secondLang = "de";
  constexpr std::string_view defaultParameter = "default-language";
  constexpr std::string_view icuParameter = "icu-language";

  // Enable force check for languages
  server.server()
      .options()
      ->get<BooleanParameter>("default-language-check")
      ->set("true");

  // Assume that it is first launch of server
  {
    auto& langFeature = server.getFeature<arangodb::LanguageFeature>();
    auto& langCheckFeature =
        server.getFeature<arangodb::LanguageCheckFeature>();

    server.server()
        .options()
        ->get<StringParameter>(icuParameter.data())
        ->set(firstLang.data());

    langFeature.validateOptions(server.server().options());

    // Simulate server launch
    langFeature.prepare();
    langCheckFeature.start();

    bool shouldBeLangEqual = true;
    checkLanguageFile(server.server(), firstLang.data(), icuParameter.data(),
                      shouldBeLangEqual);
    bool isDefaultLanguage = false;
    checkCollatorSettings(firstLang.data(), isDefaultLanguage);
  }

  // Assume that server is stoped
  // We launch it again with parameters
  {
    auto& langFeature = server.getFeature<arangodb::LanguageFeature>();
    auto& langCheckFeature =
        server.getFeature<arangodb::LanguageCheckFeature>();

    server.server()
        .options()
        ->get<StringParameter>(icuParameter.data())
        ->set(firstLang.data());

    langFeature.validateOptions(server.server().options());

    // Simulate server launch
    langFeature.prepare();
    langCheckFeature.start();

    bool shouldBeLangEqual = true;
    checkLanguageFile(server.server(), firstLang.data(), icuParameter.data(),
                      shouldBeLangEqual);
    bool isDefaultLanguage = false;
    checkCollatorSettings(firstLang.data(), isDefaultLanguage);
  }

  // Assume that server is stoped
  // We launch it again with parameters
  {
    auto& langFeature = server.getFeature<arangodb::LanguageFeature>();
    auto& langCheckFeature =
        server.getFeature<arangodb::LanguageCheckFeature>();

    // Now we try to launch server with same parameter but with another lang
    server.server()
        .options()
        ->get<StringParameter>(icuParameter.data())
        ->set(secondLang.data());

    langFeature.validateOptions(server.server().options());

    // Simulate server launch
    langFeature.prepare();
    bool shouldBeLangEqual = false;
    checkLanguageFile(server.server(), secondLang.data(), icuParameter.data(),
                      shouldBeLangEqual);
    EXPECT_DEATH(langCheckFeature.start(), "");
  }

  // Assume that server is stoped
  // We launch it again with parameters
  {
    auto& langFeature = server.getFeature<arangodb::LanguageFeature>();
    auto& langCheckFeature =
        server.getFeature<arangodb::LanguageCheckFeature>();

    // Now we try to launch server with different parameter
    server.server()
        .options()
        ->get<StringParameter>(icuParameter.data())
        ->set("");  // clear value for defaultParameter
    server.server()
        .options()
        ->get<StringParameter>(defaultParameter.data())
        ->set(secondLang.data());

    langFeature.validateOptions(server.server().options());

    // Simulate server launch
    langFeature.prepare();
    EXPECT_DEATH(langCheckFeature.start(), "");
  }

  // Assume that server is stoped
  // We launch it again with parameters
  {
    auto& langFeature = server.getFeature<arangodb::LanguageFeature>();
    auto& langCheckFeature =
        server.getFeature<arangodb::LanguageCheckFeature>();

    server.server()
        .options()
        ->get<StringParameter>(defaultParameter.data())
        ->set("");  // clear value for parameter
    server.server()
        .options()
        ->get<StringParameter>(icuParameter.data())
        ->set("");  // clear value for parameter

    langFeature.validateOptions(server.server().options());

    // Simulate server launch
    langFeature.prepare();
    langCheckFeature.start();

    bool shouldBeLangEqual = true;
    checkLanguageFile(server.server(), firstLang.data(), icuParameter.data(),
                      shouldBeLangEqual);
    bool isDefaultLanguage = false;
    checkCollatorSettings(firstLang.data(), isDefaultLanguage);
  }
}

TEST_F(ArangoLanguageFeatureTest, testIcuCountry1WithCollationLangCheckTrue) {
  // default-language-check=true
  // test behaviour of --icu-language parameter

  server.addFeatureUntracked<arangodb::LanguageFeature>().collectOptions(
      server.server().options());
  server.addFeatureUntracked<arangodb::LanguageCheckFeature>();

  constexpr std::string_view inputFirstLang = "en_US@collation=phonebook";
  constexpr std::string_view actualFirstLang = "en_US";
  constexpr std::string_view secondLang = "en";
  constexpr std::string_view defaultParameter = "default-language";
  constexpr std::string_view icuParameter = "icu-language";

  // Enable force check for languages
  server.server()
      .options()
      ->get<BooleanParameter>("default-language-check")
      ->set("true");

  // Assume that it is first launch of server
  {
    auto& langFeature = server.getFeature<arangodb::LanguageFeature>();
    auto& langCheckFeature =
        server.getFeature<arangodb::LanguageCheckFeature>();

    server.server()
        .options()
        ->get<StringParameter>(icuParameter.data())
        ->set(inputFirstLang.data());

    langFeature.validateOptions(server.server().options());

    // Simulate server launch
    langFeature.prepare();
    langCheckFeature.start();

    bool shouldBeLangEqual = true;
    checkLanguageFile(server.server(), actualFirstLang.data(),
                      icuParameter.data(), shouldBeLangEqual);
    bool isDefaultLanguage = false;
    checkCollatorSettings(actualFirstLang.data(), isDefaultLanguage);
  }

  // Assume that server is stoped
  // We launch it again with parameters
  {
    auto& langFeature = server.getFeature<arangodb::LanguageFeature>();
    auto& langCheckFeature =
        server.getFeature<arangodb::LanguageCheckFeature>();

    server.server()
        .options()
        ->get<StringParameter>(icuParameter.data())
        ->set(inputFirstLang.data());

    langFeature.validateOptions(server.server().options());

    // Simulate server launch
    langFeature.prepare();
    langCheckFeature.start();

    bool shouldBeLangEqual = true;
    checkLanguageFile(server.server(), actualFirstLang.data(),
                      icuParameter.data(), shouldBeLangEqual);
    bool isDefaultLanguage = false;
    checkCollatorSettings(actualFirstLang.data(), isDefaultLanguage);
  }

  // Assume that server is stoped
  // We launch it again with parameters
  {
    auto& langFeature = server.getFeature<arangodb::LanguageFeature>();
    auto& langCheckFeature =
        server.getFeature<arangodb::LanguageCheckFeature>();

    // Now we try to launch server with same parameter but with another lang
    server.server()
        .options()
        ->get<StringParameter>(icuParameter.data())
        ->set(secondLang.data());

    langFeature.validateOptions(server.server().options());

    // Simulate server launch
    langFeature.prepare();
    bool shouldBeLangEqual = false;
    checkLanguageFile(server.server(), secondLang.data(), icuParameter.data(),
                      shouldBeLangEqual);
    EXPECT_DEATH(langCheckFeature.start(), "");
  }

  // Assume that server is stoped
  // We launch it again with parameters
  {
    auto& langFeature = server.getFeature<arangodb::LanguageFeature>();
    auto& langCheckFeature =
        server.getFeature<arangodb::LanguageCheckFeature>();

    // Now we try to launch server with different parameter
    server.server()
        .options()
        ->get<StringParameter>(icuParameter.data())
        ->set("");  // clear value for defaultParameter
    server.server()
        .options()
        ->get<StringParameter>(defaultParameter.data())
        ->set(secondLang.data());

    langFeature.validateOptions(server.server().options());

    // Simulate server launch
    langFeature.prepare();
    EXPECT_DEATH(langCheckFeature.start(), "");
  }

  // Assume that server is stoped
  // We launch it again with parameters
  {
    auto& langFeature = server.getFeature<arangodb::LanguageFeature>();
    auto& langCheckFeature =
        server.getFeature<arangodb::LanguageCheckFeature>();

    server.server()
        .options()
        ->get<StringParameter>(defaultParameter.data())
        ->set("");  // clear value for parameter
    server.server()
        .options()
        ->get<StringParameter>(icuParameter.data())
        ->set("");  // clear value for parameter

    langFeature.validateOptions(server.server().options());

    // Simulate server launch
    langFeature.prepare();
    langCheckFeature.start();

    bool shouldBeLangEqual = true;
    checkLanguageFile(server.server(), actualFirstLang.data(),
                      icuParameter.data(), shouldBeLangEqual);
    bool isDefaultLanguage = false;
    checkCollatorSettings(actualFirstLang.data(), isDefaultLanguage);
  }
}

TEST_F(ArangoLanguageFeatureTest, testIcuCountry2WithCollationLangCheckTrue) {
  // default-language-check=true
  // test behaviour of --icu-language parameter

  server.addFeatureUntracked<arangodb::LanguageFeature>().collectOptions(
      server.server().options());
  server.addFeatureUntracked<arangodb::LanguageCheckFeature>();

  constexpr std::string_view inputFirstLang = "de_DE@collation=phonebook";
  constexpr std::string_view actualFirstLang = "de@collation=phonebook";
  constexpr std::string_view secondLang = "de";
  constexpr std::string_view defaultParameter = "default-language";
  constexpr std::string_view icuParameter = "icu-language";

  // Enable force check for languages
  server.server()
      .options()
      ->get<BooleanParameter>("default-language-check")
      ->set("true");

  // Assume that it is first launch of server
  {
    auto& langFeature = server.getFeature<arangodb::LanguageFeature>();
    auto& langCheckFeature =
        server.getFeature<arangodb::LanguageCheckFeature>();

    server.server()
        .options()
        ->get<StringParameter>(icuParameter.data())
        ->set(inputFirstLang.data());

    langFeature.validateOptions(server.server().options());

    // Simulate server launch
    langFeature.prepare();
    langCheckFeature.start();

    bool shouldBeLangEqual = true;
    checkLanguageFile(server.server(), actualFirstLang.data(),
                      icuParameter.data(), shouldBeLangEqual);
    bool isDefaultLanguage = false;
    checkCollatorSettings(actualFirstLang.data(), isDefaultLanguage);
  }

  // Assume that server is stoped
  // We launch it again with parameters
  {
    auto& langFeature = server.getFeature<arangodb::LanguageFeature>();
    auto& langCheckFeature =
        server.getFeature<arangodb::LanguageCheckFeature>();

    server.server()
        .options()
        ->get<StringParameter>(icuParameter.data())
        ->set(inputFirstLang.data());

    langFeature.validateOptions(server.server().options());

    // Simulate server launch
    langFeature.prepare();
    langCheckFeature.start();

    bool shouldBeLangEqual = true;
    checkLanguageFile(server.server(), actualFirstLang.data(),
                      icuParameter.data(), shouldBeLangEqual);
    bool isDefaultLanguage = false;
    checkCollatorSettings(actualFirstLang.data(), isDefaultLanguage);
  }

  // Assume that server is stoped
  // We launch it again with parameters
  {
    auto& langFeature = server.getFeature<arangodb::LanguageFeature>();
    auto& langCheckFeature =
        server.getFeature<arangodb::LanguageCheckFeature>();

    // Now we try to launch server with same parameter but with another lang
    server.server()
        .options()
        ->get<StringParameter>(icuParameter.data())
        ->set(secondLang.data());

    langFeature.validateOptions(server.server().options());

    // Simulate server launch
    langFeature.prepare();
    bool shouldBeLangEqual = false;
    checkLanguageFile(server.server(), secondLang.data(), icuParameter.data(),
                      shouldBeLangEqual);
    EXPECT_DEATH(langCheckFeature.start(), "");
  }

  // Assume that server is stoped
  // We launch it again with parameters
  {
    auto& langFeature = server.getFeature<arangodb::LanguageFeature>();
    auto& langCheckFeature =
        server.getFeature<arangodb::LanguageCheckFeature>();

    // Now we try to launch server with different parameter
    server.server()
        .options()
        ->get<StringParameter>(icuParameter.data())
        ->set("");  // clear value for defaultParameter
    server.server()
        .options()
        ->get<StringParameter>(defaultParameter.data())
        ->set(secondLang.data());

    langFeature.validateOptions(server.server().options());

    // Simulate server launch
    langFeature.prepare();
    EXPECT_DEATH(langCheckFeature.start(), "");
  }

  // Assume that server is stoped
  // We launch it again with parameters
  {
    auto& langFeature = server.getFeature<arangodb::LanguageFeature>();
    auto& langCheckFeature =
        server.getFeature<arangodb::LanguageCheckFeature>();

    server.server()
        .options()
        ->get<StringParameter>(defaultParameter.data())
        ->set("");  // clear value for parameter
    server.server()
        .options()
        ->get<StringParameter>(icuParameter.data())
        ->set("");  // clear value for parameter

    langFeature.validateOptions(server.server().options());

    // Simulate server launch
    langFeature.prepare();
    langCheckFeature.start();

    bool shouldBeLangEqual = true;
    checkLanguageFile(server.server(), actualFirstLang.data(),
                      icuParameter.data(), shouldBeLangEqual);
    bool isDefaultLanguage = false;
    checkCollatorSettings(actualFirstLang.data(), isDefaultLanguage);
  }
}

TEST_F(ArangoLanguageFeatureTest, testIcuCountry3WithCollationLangCheckTrue) {
  // default-language-check=true
  // test behaviour of --icu-language parameter

  server.addFeatureUntracked<arangodb::LanguageFeature>().collectOptions(
      server.server().options());
  server.addFeatureUntracked<arangodb::LanguageCheckFeature>();

  constexpr std::string_view firstLang = "de_AT@collation=phonebook";
  constexpr std::string_view secondLang = "de_AT";
  constexpr std::string_view defaultParameter = "default-language";
  constexpr std::string_view icuParameter = "icu-language";

  // Enable force check for languages
  server.server()
      .options()
      ->get<BooleanParameter>("default-language-check")
      ->set("true");

  // Assume that it is first launch of server
  {
    auto& langFeature = server.getFeature<arangodb::LanguageFeature>();
    auto& langCheckFeature =
        server.getFeature<arangodb::LanguageCheckFeature>();

    server.server()
        .options()
        ->get<StringParameter>(icuParameter.data())
        ->set(firstLang.data());

    langFeature.validateOptions(server.server().options());

    // Simulate server launch
    langFeature.prepare();
    langCheckFeature.start();

    bool shouldBeLangEqual = true;
    checkLanguageFile(server.server(), firstLang.data(), icuParameter.data(),
                      shouldBeLangEqual);
    bool isDefaultLanguage = false;
    checkCollatorSettings(firstLang.data(), isDefaultLanguage);
  }

  // Assume that server is stoped
  // We launch it again with parameters
  {
    auto& langFeature = server.getFeature<arangodb::LanguageFeature>();
    auto& langCheckFeature =
        server.getFeature<arangodb::LanguageCheckFeature>();

    server.server()
        .options()
        ->get<StringParameter>(icuParameter.data())
        ->set(firstLang.data());

    langFeature.validateOptions(server.server().options());

    // Simulate server launch
    langFeature.prepare();
    langCheckFeature.start();

    bool shouldBeLangEqual = true;
    checkLanguageFile(server.server(), firstLang.data(), icuParameter.data(),
                      shouldBeLangEqual);
    bool isDefaultLanguage = false;
    checkCollatorSettings(firstLang.data(), isDefaultLanguage);
  }

  // Assume that server is stoped
  // We launch it again with parameters
  {
    auto& langFeature = server.getFeature<arangodb::LanguageFeature>();
    auto& langCheckFeature =
        server.getFeature<arangodb::LanguageCheckFeature>();

    // Now we try to launch server with same parameter but with another lang
    server.server()
        .options()
        ->get<StringParameter>(icuParameter.data())
        ->set(secondLang.data());

    langFeature.validateOptions(server.server().options());

    // Simulate server launch
    langFeature.prepare();
    bool shouldBeLangEqual = false;
    checkLanguageFile(server.server(), secondLang.data(), icuParameter.data(),
                      shouldBeLangEqual);
    EXPECT_DEATH(langCheckFeature.start(), "");
  }

  // Assume that server is stoped
  // We launch it again with parameters
  {
    auto& langFeature = server.getFeature<arangodb::LanguageFeature>();
    auto& langCheckFeature =
        server.getFeature<arangodb::LanguageCheckFeature>();

    // Now we try to launch server with different parameter
    server.server()
        .options()
        ->get<StringParameter>(icuParameter.data())
        ->set("");  // clear value for defaultParameter
    server.server()
        .options()
        ->get<StringParameter>(defaultParameter.data())
        ->set(secondLang.data());

    langFeature.validateOptions(server.server().options());

    // Simulate server launch
    langFeature.prepare();
    EXPECT_DEATH(langCheckFeature.start(), "");
  }

  // Assume that server is stoped
  // We launch it again with parameters
  {
    auto& langFeature = server.getFeature<arangodb::LanguageFeature>();
    auto& langCheckFeature =
        server.getFeature<arangodb::LanguageCheckFeature>();

    server.server()
        .options()
        ->get<StringParameter>(defaultParameter.data())
        ->set("");  // clear value for parameter
    server.server()
        .options()
        ->get<StringParameter>(icuParameter.data())
        ->set("");  // clear value for parameter

    langFeature.validateOptions(server.server().options());

    // Simulate server launch
    langFeature.prepare();
    langCheckFeature.start();

    bool shouldBeLangEqual = true;
    checkLanguageFile(server.server(), firstLang.data(), icuParameter.data(),
                      shouldBeLangEqual);
    bool isDefaultLanguage = false;
    checkCollatorSettings(firstLang.data(), isDefaultLanguage);
  }
}

TEST_F(ArangoLanguageFeatureTest, testDefaultWithCollationLangCheckTrue) {
  // default-language-check=true
  // test behaviour of --icu-language parameter

  server.addFeatureUntracked<arangodb::LanguageFeature>().collectOptions(
      server.server().options());
  server.addFeatureUntracked<arangodb::LanguageCheckFeature>();

  constexpr std::string_view inputFirstLang = "de_DE@collation=phonebook";
  constexpr std::string_view actualFirstLang = "de";
  constexpr std::string_view secondLang = "de";
  constexpr std::string_view defaultParameter = "default-language";
  constexpr std::string_view icuParameter = "icu-language";

  // Enable force check for languages
  server.server()
      .options()
      ->get<BooleanParameter>("default-language-check")
      ->set("true");

  // Assume that it is first launch of server
  {
    auto& langFeature = server.getFeature<arangodb::LanguageFeature>();
    auto& langCheckFeature =
        server.getFeature<arangodb::LanguageCheckFeature>();

    server.server()
        .options()
        ->get<StringParameter>(defaultParameter.data())
        ->set(inputFirstLang.data());
    server.server()
        .options()
        ->get<StringParameter>(icuParameter.data())
        ->set("");

    langFeature.validateOptions(server.server().options());

    // Simulate server launch
    langFeature.prepare();
    langCheckFeature.start();

    bool shouldBeLangEqual = true;
    checkLanguageFile(server.server(), actualFirstLang.data(),
                      defaultParameter.data(), shouldBeLangEqual);
    bool isDefaultLanguage = true;
    checkCollatorSettings(actualFirstLang.data(), isDefaultLanguage);
  }

  // Assume that server is stoped
  // We launch it again with parameters
  {
    auto& langFeature = server.getFeature<arangodb::LanguageFeature>();
    auto& langCheckFeature =
        server.getFeature<arangodb::LanguageCheckFeature>();

    server.server()
        .options()
        ->get<StringParameter>(defaultParameter.data())
        ->set(inputFirstLang.data());
    server.server()
        .options()
        ->get<StringParameter>(icuParameter.data())
        ->set("");

    langFeature.validateOptions(server.server().options());

    // Simulate server launch
    langFeature.prepare();
    langCheckFeature.start();

    bool shouldBeLangEqual = true;
    checkLanguageFile(server.server(), actualFirstLang.data(),
                      defaultParameter.data(), shouldBeLangEqual);
    bool isDefaultLanguage = true;
    checkCollatorSettings(actualFirstLang.data(), isDefaultLanguage);
  }

  // Assume that server is stoped
  // We launch it again with parameters
  {
    auto& langFeature = server.getFeature<arangodb::LanguageFeature>();
    auto& langCheckFeature =
        server.getFeature<arangodb::LanguageCheckFeature>();

    // Now we try to launch server with same parameter but with another lang
    server.server()
        .options()
        ->get<StringParameter>(defaultParameter.data())
        ->set(secondLang.data());
    server.server()
        .options()
        ->get<StringParameter>(icuParameter.data())
        ->set("");

    langFeature.validateOptions(server.server().options());

    // Simulate server launch
    langFeature.prepare();
    langCheckFeature.start();

    bool shouldBeLangEqual = true;
    checkLanguageFile(server.server(), secondLang.data(),
                      defaultParameter.data(), shouldBeLangEqual);
    bool isDefaultLanguage = true;
    checkCollatorSettings(secondLang.data(), isDefaultLanguage);
  }

  // Assume that server is stoped
  // We launch it again with parameters
  {
    auto& langFeature = server.getFeature<arangodb::LanguageFeature>();
    auto& langCheckFeature =
        server.getFeature<arangodb::LanguageCheckFeature>();

    // Now we try to launch server with different parameter
    server.server()
        .options()
        ->get<StringParameter>(defaultParameter.data())
        ->set("");  // clear value for defaultParameter
    server.server()
        .options()
        ->get<StringParameter>(icuParameter.data())
        ->set(secondLang.data());

    langFeature.validateOptions(server.server().options());

    // Simulate server launch
    langFeature.prepare();
    EXPECT_DEATH(langCheckFeature.start(), "");
  }

  // Assume that server is stoped
  // We launch it again with parameters
  {
    auto& langFeature = server.getFeature<arangodb::LanguageFeature>();
    auto& langCheckFeature =
        server.getFeature<arangodb::LanguageCheckFeature>();

    server.server()
        .options()
        ->get<StringParameter>(defaultParameter.data())
        ->set("");  // clear value for parameter
    server.server()
        .options()
        ->get<StringParameter>(icuParameter.data())
        ->set("");  // clear value for parameter

    langFeature.validateOptions(server.server().options());

    // Simulate server launch
    langFeature.prepare();
    langCheckFeature.start();

    bool shouldBeLangEqual = true;
    checkLanguageFile(server.server(), actualFirstLang.data(),
                      defaultParameter.data(), shouldBeLangEqual);
    bool isDefaultLanguage = true;
    checkCollatorSettings(actualFirstLang.data(), isDefaultLanguage);
  }
}

TEST_F(ArangoLanguageFeatureTest,
       testDefaultCountryWithCollationLangCheckTrue) {
  // default-language-check=true
  // test behaviour of --icu-language parameter

  server.addFeatureUntracked<arangodb::LanguageFeature>().collectOptions(
      server.server().options());
  server.addFeatureUntracked<arangodb::LanguageCheckFeature>();

  constexpr std::string_view inputFirstLang = "en_US@collation=phonebook";
  constexpr std::string_view actualFirstLang = "en_US";
  constexpr std::string_view secondLang = "en_US";
  constexpr std::string_view defaultParameter = "default-language";
  constexpr std::string_view icuParameter = "icu-language";

  // Enable force check for languages
  server.server()
      .options()
      ->get<BooleanParameter>("default-language-check")
      ->set("true");

  // Assume that it is first launch of server
  {
    auto& langFeature = server.getFeature<arangodb::LanguageFeature>();
    auto& langCheckFeature =
        server.getFeature<arangodb::LanguageCheckFeature>();

    server.server()
        .options()
        ->get<StringParameter>(defaultParameter.data())
        ->set(inputFirstLang.data());
    server.server()
        .options()
        ->get<StringParameter>(icuParameter.data())
        ->set("");

    langFeature.validateOptions(server.server().options());

    // Simulate server launch
    langFeature.prepare();
    langCheckFeature.start();

    bool shouldBeLangEqual = true;
    checkLanguageFile(server.server(), actualFirstLang.data(),
                      defaultParameter.data(), shouldBeLangEqual);
    bool isDefaultLanguage = true;
    checkCollatorSettings(actualFirstLang.data(), isDefaultLanguage);
  }

  // Assume that server is stoped
  // We launch it again with parameters
  {
    auto& langFeature = server.getFeature<arangodb::LanguageFeature>();
    auto& langCheckFeature =
        server.getFeature<arangodb::LanguageCheckFeature>();

    server.server()
        .options()
        ->get<StringParameter>(defaultParameter.data())
        ->set(inputFirstLang.data());
    server.server()
        .options()
        ->get<StringParameter>(icuParameter.data())
        ->set("");

    langFeature.validateOptions(server.server().options());

    // Simulate server launch
    langFeature.prepare();
    langCheckFeature.start();

    bool shouldBeLangEqual = true;
    checkLanguageFile(server.server(), actualFirstLang.data(),
                      defaultParameter.data(), shouldBeLangEqual);
    bool isDefaultLanguage = true;
    checkCollatorSettings(actualFirstLang.data(), isDefaultLanguage);
  }

  // Assume that server is stoped
  // We launch it again with parameters
  {
    auto& langFeature = server.getFeature<arangodb::LanguageFeature>();
    auto& langCheckFeature =
        server.getFeature<arangodb::LanguageCheckFeature>();

    // Now we try to launch server with same parameter but with another lang
    server.server()
        .options()
        ->get<StringParameter>(defaultParameter.data())
        ->set(secondLang.data());
    server.server()
        .options()
        ->get<StringParameter>(icuParameter.data())
        ->set("");

    langFeature.validateOptions(server.server().options());

    // Simulate server launch
    langFeature.prepare();
    langCheckFeature.start();

    bool shouldBeLangEqual = true;
    checkLanguageFile(server.server(), secondLang.data(),
                      defaultParameter.data(), shouldBeLangEqual);
    bool isDefaultLanguage = true;
    checkCollatorSettings(secondLang.data(), isDefaultLanguage);
  }

  // Assume that server is stoped
  // We launch it again with parameters
  {
    auto& langFeature = server.getFeature<arangodb::LanguageFeature>();
    auto& langCheckFeature =
        server.getFeature<arangodb::LanguageCheckFeature>();

    // Now we try to launch server with different parameter
    server.server()
        .options()
        ->get<StringParameter>(defaultParameter.data())
        ->set("");  // clear value for defaultParameter
    server.server()
        .options()
        ->get<StringParameter>(icuParameter.data())
        ->set(secondLang.data());

    langFeature.validateOptions(server.server().options());

    // Simulate server launch
    langFeature.prepare();
    EXPECT_DEATH(langCheckFeature.start(), "");
  }

  // Assume that server is stoped
  // We launch it again with parameters
  {
    auto& langFeature = server.getFeature<arangodb::LanguageFeature>();
    auto& langCheckFeature =
        server.getFeature<arangodb::LanguageCheckFeature>();

    server.server()
        .options()
        ->get<StringParameter>(defaultParameter.data())
        ->set("");  // clear value for parameter
    server.server()
        .options()
        ->get<StringParameter>(icuParameter.data())
        ->set("");  // clear value for parameter

    langFeature.validateOptions(server.server().options());

    // Simulate server launch
    langFeature.prepare();
    langCheckFeature.start();

    bool shouldBeLangEqual = true;
    checkLanguageFile(server.server(), actualFirstLang.data(),
                      defaultParameter.data(), shouldBeLangEqual);
    bool isDefaultLanguage = true;
    checkCollatorSettings(actualFirstLang.data(), isDefaultLanguage);
  }
}

TEST_F(ArangoLanguageFeatureTest, testIcuWithWrongCollationLangCheckTrue) {
  // default-language-check=true
  // test behaviour of --icu-language parameter

  server.addFeatureUntracked<arangodb::LanguageFeature>().collectOptions(
      server.server().options());
  server.addFeatureUntracked<arangodb::LanguageCheckFeature>();

  constexpr std::string_view inputFirstLang = "de@collation=AbCxYz";
  constexpr std::string_view actualFirstLang = "de";
  constexpr std::string_view secondLang = "de";
  constexpr std::string_view defaultParameter = "default-language";
  constexpr std::string_view icuParameter = "icu-language";

  // Enable force check for languages
  server.server()
      .options()
      ->get<BooleanParameter>("default-language-check")
      ->set("true");

  // Assume that it is first launch of server
  {
    auto& langFeature = server.getFeature<arangodb::LanguageFeature>();
    auto& langCheckFeature =
        server.getFeature<arangodb::LanguageCheckFeature>();

    server.server()
        .options()
        ->get<StringParameter>(defaultParameter.data())
        ->set("");  // clear value for parameter
    server.server()
        .options()
        ->get<StringParameter>(icuParameter.data())
        ->set(inputFirstLang.data());

    langFeature.validateOptions(server.server().options());

    // Simulate server launch
    langFeature.prepare();
    langCheckFeature.start();

    bool shouldBeLangEqual = true;
    checkLanguageFile(server.server(), actualFirstLang.data(),
                      icuParameter.data(), shouldBeLangEqual);
    bool isDefaultLanguage = false;
    checkCollatorSettings(actualFirstLang.data(), isDefaultLanguage);
  }

  // Assume that server is stoped
  // We launch it again with parameters
  {
    auto& langFeature = server.getFeature<arangodb::LanguageFeature>();
    auto& langCheckFeature =
        server.getFeature<arangodb::LanguageCheckFeature>();

    server.server()
        .options()
        ->get<StringParameter>(defaultParameter.data())
        ->set("");  // clear value for parameter
    server.server()
        .options()
        ->get<StringParameter>(icuParameter.data())
        ->set(inputFirstLang.data());

    langFeature.validateOptions(server.server().options());

    // Simulate server launch
    langFeature.prepare();
    langCheckFeature.start();

    bool shouldBeLangEqual = true;
    checkLanguageFile(server.server(), actualFirstLang.data(),
                      icuParameter.data(), shouldBeLangEqual);
    bool isDefaultLanguage = false;
    checkCollatorSettings(actualFirstLang.data(), isDefaultLanguage);
  }

  // Assume that server is stoped
  // We launch it again with parameters
  {
    auto& langFeature = server.getFeature<arangodb::LanguageFeature>();
    auto& langCheckFeature =
        server.getFeature<arangodb::LanguageCheckFeature>();

    // Now we try to launch server with same parameter but with another lang
    server.server()
        .options()
        ->get<StringParameter>(defaultParameter.data())
        ->set("");  // clear value for parameter
    server.server()
        .options()
        ->get<StringParameter>(icuParameter.data())
        ->set(secondLang.data());

    langFeature.validateOptions(server.server().options());

    // Simulate server launch
    langFeature.prepare();
    langCheckFeature.start();

    bool shouldBeLangEqual = true;
    checkLanguageFile(server.server(), secondLang.data(), icuParameter.data(),
                      shouldBeLangEqual);
    bool isDefaultLanguage = false;
    checkCollatorSettings(secondLang.data(), isDefaultLanguage);
  }

  // Assume that server is stoped
  // We launch it again with parameters
  {
    auto& langFeature = server.getFeature<arangodb::LanguageFeature>();
    auto& langCheckFeature =
        server.getFeature<arangodb::LanguageCheckFeature>();

    // Now we try to launch server with another parameter and with another lang
    server.server()
        .options()
        ->get<StringParameter>(defaultParameter.data())
        ->set(secondLang.data());
    server.server()
        .options()
        ->get<StringParameter>(icuParameter.data())
        ->set("");

    langFeature.validateOptions(server.server().options());

    // Simulate server launch
    langFeature.prepare();
    EXPECT_DEATH(langCheckFeature.start(), "");
  }

  // Assume that server is stoped
  // We launch it again with parameters
  {
    auto& langFeature = server.getFeature<arangodb::LanguageFeature>();
    auto& langCheckFeature =
        server.getFeature<arangodb::LanguageCheckFeature>();

    server.server()
        .options()
        ->get<StringParameter>(defaultParameter.data())
        ->set("");  // clear value for parameter
    server.server()
        .options()
        ->get<StringParameter>(icuParameter.data())
        ->set("");  // clear value for parameter

    langFeature.validateOptions(server.server().options());

    // Simulate server launch
    langFeature.prepare();
    langCheckFeature.start();

    bool shouldBeLangEqual = true;
    checkLanguageFile(server.server(), actualFirstLang.data(),
                      icuParameter.data(), shouldBeLangEqual);
    bool isDefaultLanguage = false;
    checkCollatorSettings(actualFirstLang.data(), isDefaultLanguage);
  }
}

TEST_F(ArangoLanguageFeatureTest, testDefaultWithWrongCollationLangCheckTrue) {
  // default-language-check=true
  // test behaviour of --icu-language parameter

  server.addFeatureUntracked<arangodb::LanguageFeature>().collectOptions(
      server.server().options());
  server.addFeatureUntracked<arangodb::LanguageCheckFeature>();

  constexpr std::string_view inputFirstLang = "de@collation=AbCxYz";
  constexpr std::string_view actualFirstLang = "de";
  constexpr std::string_view secondLang = "de";
  constexpr std::string_view defaultParameter = "default-language";
  constexpr std::string_view icuParameter = "icu-language";

  // Enable force check for languages
  server.server()
      .options()
      ->get<BooleanParameter>("default-language-check")
      ->set("true");

  // Assume that it is first launch of server
  {
    auto& langFeature = server.getFeature<arangodb::LanguageFeature>();
    auto& langCheckFeature =
        server.getFeature<arangodb::LanguageCheckFeature>();

    server.server()
        .options()
        ->get<StringParameter>(defaultParameter.data())
        ->set(inputFirstLang.data());
    server.server()
        .options()
        ->get<StringParameter>(icuParameter.data())
        ->set("");  // clear value for parameter

    langFeature.validateOptions(server.server().options());

    // Simulate server launch
    langFeature.prepare();
    langCheckFeature.start();

    bool shouldBeLangEqual = true;
    checkLanguageFile(server.server(), actualFirstLang.data(),
                      defaultParameter.data(), shouldBeLangEqual);
    bool isDefaultLanguage = true;
    checkCollatorSettings(actualFirstLang.data(), isDefaultLanguage);
  }

  // Assume that server is stoped
  // We launch it again with parameters
  {
    auto& langFeature = server.getFeature<arangodb::LanguageFeature>();
    auto& langCheckFeature =
        server.getFeature<arangodb::LanguageCheckFeature>();

    server.server()
        .options()
        ->get<StringParameter>(defaultParameter.data())
        ->set(inputFirstLang.data());
    server.server()
        .options()
        ->get<StringParameter>(icuParameter.data())
        ->set("");  // clear value for parameter

    langFeature.validateOptions(server.server().options());

    // Simulate server launch
    langFeature.prepare();
    langCheckFeature.start();

    bool shouldBeLangEqual = true;
    checkLanguageFile(server.server(), actualFirstLang.data(),
                      defaultParameter.data(), shouldBeLangEqual);
    bool isDefaultLanguage = true;
    checkCollatorSettings(actualFirstLang.data(), isDefaultLanguage);
  }

  // Assume that server is stoped
  // We launch it again with parameters
  {
    auto& langFeature = server.getFeature<arangodb::LanguageFeature>();
    auto& langCheckFeature =
        server.getFeature<arangodb::LanguageCheckFeature>();

    // Now we try to launch server with another parameter and with another lang
    server.server()
        .options()
        ->get<StringParameter>(defaultParameter.data())
        ->set(secondLang.data());
    server.server()
        .options()
        ->get<StringParameter>(icuParameter.data())
        ->set("");  // clear value for parameter

    langFeature.validateOptions(server.server().options());

    // Simulate server launch
    langFeature.prepare();
    langCheckFeature.start();

    bool shouldBeLangEqual = true;
    checkLanguageFile(server.server(), secondLang.data(),
                      defaultParameter.data(), shouldBeLangEqual);
    bool isDefaultLanguage = true;
    checkCollatorSettings(secondLang.data(), isDefaultLanguage);
  }

  // Assume that server is stoped
  // We launch it again with parameters
  {
    auto& langFeature = server.getFeature<arangodb::LanguageFeature>();
    auto& langCheckFeature =
        server.getFeature<arangodb::LanguageCheckFeature>();

    // Now we try to launch server with same parameter but with normalized lang
    server.server()
        .options()
        ->get<StringParameter>(defaultParameter.data())
        ->set(secondLang.data());
    server.server()
        .options()
        ->get<StringParameter>(icuParameter.data())
        ->set("");

    langFeature.validateOptions(server.server().options());

    // Simulate server launch
    langFeature.prepare();
    langCheckFeature.start();

    bool shouldBeLangEqual = true;
    checkLanguageFile(server.server(), secondLang.data(),
                      defaultParameter.data(), shouldBeLangEqual);
    bool isDefaultLanguage = true;
    checkCollatorSettings(secondLang.data(), isDefaultLanguage);
  }

  // Assume that server is stoped
  // We launch it again with parameters
  {
    auto& langFeature = server.getFeature<arangodb::LanguageFeature>();
    auto& langCheckFeature =
        server.getFeature<arangodb::LanguageCheckFeature>();

    server.server()
        .options()
        ->get<StringParameter>(defaultParameter.data())
        ->set("");  // clear value for parameter
    server.server()
        .options()
        ->get<StringParameter>(icuParameter.data())
        ->set("");  // clear value for parameter

    langFeature.validateOptions(server.server().options());

    // Simulate server launch
    langFeature.prepare();
    langCheckFeature.start();

    bool shouldBeLangEqual = true;
    checkLanguageFile(server.server(), actualFirstLang.data(),
                      defaultParameter.data(), shouldBeLangEqual);
    bool isDefaultLanguage = true;
    checkCollatorSettings(actualFirstLang.data(), isDefaultLanguage);
  }
}
