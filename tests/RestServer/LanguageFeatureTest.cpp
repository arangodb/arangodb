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

#include "Servers.h"

#include "IResearch/common.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                 setup / tear-down
// -----------------------------------------------------------------------------

namespace  {

}

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

 public:
  ArangoLanguageFeatureTest() : server(false) {

    arangodb::tests::init();

    server.startFeatures();
//    server.addFeature<arangodb::DatabaseFeature>(true);
//    server.addFeature<arangodb::DatabasePathFeature>(true);
    auto& dbFeature = server.getFeature<arangodb::DatabaseFeature>();
    auto& dbPathFeature = server.getFeature<arangodb::DatabasePathFeature>();

    auto vocbase =
        dbFeature.useDatabase(arangodb::StaticStrings::SystemDatabase);
//    dbFeature.createDatabase(
//        testDBInfo(server.server()),
//        _vocbase);

    arangodb::tests::setDatabasePath(
        dbPathFeature);  // ensure test data is stored in a unique directory

//      DatabasePathFeature


  }


//  ~ArangoLanguageFeatureTest() {

//  }


};

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

TEST_F(ArangoLanguageFeatureTest, test_1) {

  using namespace arangodb::options;

  auto& langFeature = server.addFeatureUntracked<arangodb::LanguageFeature>();
  auto& langCheckFeature = server.addFeatureUntracked<arangodb::LanguageCheckFeature>();

//  arangodb::AqlFeature aqlFeature(server.server());

  langFeature.collectOptions(server.server().options());
  langFeature.validateOptions(server.server().options());

//  server.server()
//      .options()
//      ->get<StringParameter>(
//          "icu-locale")
//      ->set("ru");

//  feature.validateOptions(server.server().options());
//  feature.prepare();
//  feature.start();  // start thread pool


  langFeature.prepare();
//  langCheckFeature.start();
//  langCheckFeature.stop();
//  langFeature.stop();
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
