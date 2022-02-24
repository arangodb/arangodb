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

namespace  {
  void checkCollatorSettings(bool isDefaultLang) {
    UErrorCode status = U_ZERO_ERROR;
    const auto* collator = arangodb::basics::Utf8Helper::DefaultUtf8Helper.getCollator();

    auto actualCaseFirst = collator->getAttribute(UCOL_CASE_FIRST, status); // UCOL_UPPER_FIRST
    auto actualNorm = collator->getAttribute(UCOL_NORMALIZATION_MODE, status); // UCOL_OFF
    auto actualStrength = collator->getAttribute(UCOL_STRENGTH, status); // UCOL_IDENTICAL

//    ASSERT_EQ(actualCaseFirst == UCOL_UPPER_FIRST, isDefaultLang);
//    ASSERT_EQ(actualNorm == UCOL_OFF, isDefaultLang);
//    ASSERT_EQ(actualStrength == UCOL_IDENTICAL, isDefaultLang);

  }

  void checkLanguageFile(const arangodb::ArangodServer& server, const std::string expectedLang,
                         const std::string& expectedParameter) {

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

      VPackSlice actualSlice = content.get(expectedParameter);

      ASSERT_TRUE(actualSlice.isString());

      auto actualLang = actualSlice.copyString();

      ASSERT_EQ(actualLang, expectedLang);

    } catch (...) {
      ASSERT_TRUE(false);
    }
  }

  void modifyLanguageFile(const arangodb::ArangodServer& server, const std::string newLang,
                          const std::string& parameter) {
  }
}

using namespace arangodb::options;

class ArangoLanguageFeatureTest
    : public ::testing::Test,
      public arangodb::tests::LogSuppressor<arangodb::Logger::AUTHENTICATION,
                                            arangodb::LogLevel::ERR>,
      public arangodb::tests::LogSuppressor<arangodb::Logger::CLUSTER,
                                            arangodb::LogLevel::FATAL> {
 protected:
  arangodb::tests::mocks::MockAqlServer server;

 private:
  TRI_vocbase_t* _vocbase;
  icu::Collator* collator;

 public:
  ArangoLanguageFeatureTest() : server(false) {

    collator = arangodb::basics::Utf8Helper::DefaultUtf8Helper.getCollator();
    arangodb::basics::Utf8Helper::DefaultUtf8Helper.setCollator(nullptr);

    arangodb::tests::init();

    server.startFeatures();

    auto& dbFeature = server.getFeature<arangodb::DatabaseFeature>();
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

TEST_F(ArangoLanguageFeatureTest, test_1) {


  auto& langFeature = server.addFeatureUntracked<arangodb::LanguageFeature>();
  auto& langCheckFeature = server.addFeatureUntracked<arangodb::LanguageCheckFeature>();

  langFeature.collectOptions(server.server().options());


  std::string lang = "ru";
  std::string parameter = "icu-language";
  server.server()
      .options()
      ->get<StringParameter>(
          parameter)
      ->set(lang);


  langFeature.validateOptions(server.server().options());

  langFeature.prepare();
  langCheckFeature.start();

  checkLanguageFile(server.server(), lang, parameter);

  server.server()
      .options()
      ->get<StringParameter>(
          parameter)
      ->set("");

  langFeature.validateOptions(server.server().options());

  langFeature.prepare();
  langCheckFeature.start();

  checkLanguageFile(server.server(), lang, parameter);


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
