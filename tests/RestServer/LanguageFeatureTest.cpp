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
#include "Basics/files.h"

#include "IResearch/common.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                 setup / tear-down
// -----------------------------------------------------------------------------

namespace {
void checkCollatorSettings(const std::string& language, bool isOldLanguage) {
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

  if (isOldLanguage) {
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

 private:
  //  TRI_vocbase_t* _vocbase;
  icu::Collator* collator;

 public:
  ArangoLanguageFeatureTest() : server(false) {
    collator = arangodb::basics::Utf8Helper::DefaultUtf8Helper.getCollator();

    arangodb::basics::Utf8Helper::DefaultUtf8Helper.setCollator(nullptr);

    arangodb::tests::init();

    server.startFeatures();

    auto& dbPathFeature = server.getFeature<arangodb::DatabasePathFeature>();

    server.server().setBinaryPath(dbPathFeature.directory().c_str());
  }

  ~ArangoLanguageFeatureTest() {
    arangodb::basics::Utf8Helper::DefaultUtf8Helper.setCollator(collator);
  }
};

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

TEST_F(ArangoLanguageFeatureTest, test_both_arguments_specified) {
  // Specify both language arguments and get server failure

  auto& langFeature = server.addFeatureUntracked<arangodb::LanguageFeature>();
  langFeature.collectOptions(server.server().options());

  std::string lang = "ru";
  server.server().options()->get<StringParameter>("icu-language")->set(lang);
  server.server()
      .options()
      ->get<StringParameter>("default-language")
      ->set(lang);

  langFeature.validateOptions(server.server().options());

  // Simulate server launch
  EXPECT_DEATH(langFeature.prepare(), "");
}

TEST_F(ArangoLanguageFeatureTest, test_default_lang_check_true) {
  // default-language-check=true
  // test behaviour of --default-language parameter

  server.addFeatureUntracked<arangodb::LanguageFeature>().collectOptions(
      server.server().options());
  server.addFeatureUntracked<arangodb::LanguageCheckFeature>();

  std::string firstLang = "sv";
  std::string secondLang = "de";
  std::string defaultParameter = "default-language";
  std::string icuParameter = "icu-language";

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
        ->get<StringParameter>(defaultParameter)
        ->set(firstLang);

    langFeature.validateOptions(server.server().options());

    // Simulate server launch
    langFeature.prepare();
    langCheckFeature.start();

    bool shouldBeLangEqual = true;
    checkLanguageFile(server.server(), firstLang, defaultParameter,
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

    server.server()
        .options()
        ->get<StringParameter>(defaultParameter)
        ->set(firstLang);

    langFeature.validateOptions(server.server().options());

    // Simulate server launch
    langFeature.prepare();
    langCheckFeature.start();

    bool shouldBeLangEqual = true;
    checkLanguageFile(server.server(), firstLang, defaultParameter,
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
        ->get<StringParameter>(defaultParameter)
        ->set(secondLang);

    langFeature.validateOptions(server.server().options());

    // Simulate server launch
    langFeature.prepare();
    bool shouldBeLangEqual = false;
    checkLanguageFile(server.server(), secondLang, defaultParameter,
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
        ->get<StringParameter>(defaultParameter)
        ->set("");  // clear value for defaultParameter
    server.server()
        .options()
        ->get<StringParameter>(icuParameter)
        ->set(secondLang);

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
        ->get<StringParameter>(defaultParameter)
        ->set("");  // clear value for parameter
    server.server()
        .options()
        ->get<StringParameter>(icuParameter)
        ->set("");  // clear value for parameter

    langFeature.validateOptions(server.server().options());

    // Simulate server launch
    langFeature.prepare();
    langCheckFeature.start();

    bool shouldBeLangEqual = true;
    checkLanguageFile(server.server(), firstLang, defaultParameter,
                      shouldBeLangEqual);
    bool isDefaultLanguage = true;
    checkCollatorSettings(firstLang, isDefaultLanguage);
  }
}

TEST_F(ArangoLanguageFeatureTest, test_empty_lang_check_true) {
  // default-language-check=true
  // test behaviour of parameters

  server.addFeatureUntracked<arangodb::LanguageFeature>().collectOptions(
      server.server().options());
  server.addFeatureUntracked<arangodb::LanguageCheckFeature>();

  // firstLang in this case will be initialized after langFeature.prepare()
  // because collator will be ready after prepare()
  std::string firstLang = {};
  std::string secondLang = "de";
  std::string defaultParameter = "default-language";
  std::string icuParameter = "icu-language";

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

    server.server().options()->get<StringParameter>(defaultParameter)->set("");
    server.server().options()->get<StringParameter>(icuParameter)->set("");

    langFeature.validateOptions(server.server().options());

    // Simulate server launch
    langFeature.prepare();
    firstLang = langFeature.getCollatorLanguage();
    langCheckFeature.start();

    bool shouldBeLangEqual = true;
    checkLanguageFile(server.server(), firstLang, defaultParameter,
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

    server.server()
        .options()
        ->get<StringParameter>(defaultParameter)
        ->set(firstLang);

    langFeature.validateOptions(server.server().options());

    // Simulate server launch
    langFeature.prepare();
    langCheckFeature.start();

    bool shouldBeLangEqual = true;
    checkLanguageFile(server.server(), firstLang, defaultParameter,
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
        ->get<StringParameter>(defaultParameter)
        ->set(secondLang);

    langFeature.validateOptions(server.server().options());

    // Simulate server launch
    langFeature.prepare();
    bool shouldBeLangEqual = false;
    checkLanguageFile(server.server(), secondLang, defaultParameter,
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
        ->get<StringParameter>(defaultParameter)
        ->set("");  // clear value for defaultParameter
    server.server()
        .options()
        ->get<StringParameter>(icuParameter)
        ->set(secondLang);

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
        ->get<StringParameter>(defaultParameter)
        ->set("");  // clear value for parameter
    server.server()
        .options()
        ->get<StringParameter>(icuParameter)
        ->set("");  // clear value for parameter

    langFeature.validateOptions(server.server().options());

    // Simulate server launch
    langFeature.prepare();
    langCheckFeature.start();

    bool shouldBeLangEqual = true;
    checkLanguageFile(server.server(), firstLang, defaultParameter,
                      shouldBeLangEqual);
    bool isDefaultLanguage = true;
    checkCollatorSettings(firstLang, isDefaultLanguage);
  }
}

TEST_F(ArangoLanguageFeatureTest, test_icu_lang_check_true) {
  // default-language-check=true
  // test behaviour of --icu-language parameter

  server.addFeatureUntracked<arangodb::LanguageFeature>().collectOptions(
      server.server().options());
  server.addFeatureUntracked<arangodb::LanguageCheckFeature>();

  std::string firstLang = "sv";
  std::string secondLang = "de";
  std::string defaultParameter = "default-language";
  std::string icuParameter = "icu-language";

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
        ->get<StringParameter>(icuParameter)
        ->set(firstLang);

    langFeature.validateOptions(server.server().options());

    // Simulate server launch
    langFeature.prepare();
    langCheckFeature.start();

    bool shouldBeLangEqual = true;
    checkLanguageFile(server.server(), firstLang, icuParameter,
                      shouldBeLangEqual);
    bool isDefaultLanguage = false;
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
        ->get<StringParameter>(icuParameter)
        ->set(firstLang);

    langFeature.validateOptions(server.server().options());

    // Simulate server launch
    langFeature.prepare();
    langCheckFeature.start();

    bool shouldBeLangEqual = true;
    checkLanguageFile(server.server(), firstLang, icuParameter,
                      shouldBeLangEqual);
    bool isDefaultLanguage = false;
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
        ->get<StringParameter>(icuParameter)
        ->set(secondLang);

    langFeature.validateOptions(server.server().options());

    // Simulate server launch
    langFeature.prepare();
    bool shouldBeLangEqual = false;
    checkLanguageFile(server.server(), secondLang, icuParameter,
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
        ->get<StringParameter>(icuParameter)
        ->set("");  // clear value for defaultParameter
    server.server()
        .options()
        ->get<StringParameter>(defaultParameter)
        ->set(secondLang);

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
        ->get<StringParameter>(defaultParameter)
        ->set("");  // clear value for parameter
    server.server()
        .options()
        ->get<StringParameter>(icuParameter)
        ->set("");  // clear value for parameter

    langFeature.validateOptions(server.server().options());

    // Simulate server launch
    langFeature.prepare();
    langCheckFeature.start();

    bool shouldBeLangEqual = true;
    checkLanguageFile(server.server(), firstLang, icuParameter,
                      shouldBeLangEqual);
    bool isDefaultLanguage = false;
    checkCollatorSettings(firstLang, isDefaultLanguage);
  }
}

/*
  {
    auto result = arangodb::tests::executeQuery(
        vocbase(),
        "FOR d IN testView SEARCH PHRASE(d.X, 'abc', 'test_A') OPTIONS { "
        "waitForSync: true } SORT d.seq RETURN d",
        nullptr);
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    EXPECT_TRUE(slice.isArray());
    std::vector<size_t> expected{0, 1};
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      auto key = resolved.get("seq");
      EXPECT_TRUE(i < expected.size());
      EXPECT_EQ(expected[i++], key.getNumber<size_t>());
    }

    EXPECT_EQ(i, expected.size());
  }
*/
