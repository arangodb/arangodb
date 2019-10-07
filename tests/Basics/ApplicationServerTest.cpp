////////////////////////////////////////////////////////////////////////////////
/// @brief test suite for application server
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2004-2012 triagens GmbH, Cologne, Germany
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
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Jan Steemann
/// @author Copyright 2017, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "Basics/Common.h"

#include "gtest/gtest.h"

#include "ApplicationFeatures/ApplicationFeature.h"
#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/Exceptions.h"
#include "ProgramOptions/ProgramOptions.h"

using namespace arangodb;

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief setup
////////////////////////////////////////////////////////////////////////////////

class TestFeatureA : public application_features::ApplicationFeature {
 public:
  TestFeatureA(application_features::ApplicationServer& server,
               std::string const& name, std::vector<std::type_index> const& startsAfter,
               std::vector<std::type_index> const& startsBefore)
      : ApplicationFeature(server, name) {
    for (auto const& it : startsAfter) {
      this->startsAfter(it);
    }
    for (auto const& it : startsBefore) {
      this->startsBefore(it);
    }
  }
};

class TestFeatureB : public application_features::ApplicationFeature {
 public:
  TestFeatureB(application_features::ApplicationServer& server,
               std::string const& name, std::vector<std::type_index> const& startsAfter,
               std::vector<std::type_index> const& startsBefore)
      : ApplicationFeature(server, name) {
    for (auto const& it : startsAfter) {
      this->startsAfter(it);
    }
    for (auto const& it : startsBefore) {
      this->startsBefore(it);
    }
  }
};

TEST(ApplicationServerTest, test_startsAfterValid) {
  bool failed = false;
  std::function<void(std::string const&)> callback = [&failed](std::string const&) {
    failed = true;
  };

  auto options =
      std::make_shared<options::ProgramOptions>("arangod", "something", "",
                                                "path");
  application_features::ApplicationServer server(options, "path");
  server.registerFailCallback(callback);

  auto& feature1 =
      server.addFeature<TestFeatureA>("feature1", std::vector<std::type_index>{},
                                      std::vector<std::type_index>{});

  auto& feature2 =
      server.addFeature<TestFeatureB>("feature2",
                                      std::vector<std::type_index>{
                                          std::type_index(typeid(TestFeatureA))},
                                      std::vector<std::type_index>{});

  server.setupDependencies(true);

  EXPECT_FALSE(failed);
  EXPECT_TRUE(feature1.doesStartBefore<TestFeatureB>());
  EXPECT_FALSE(feature1.doesStartAfter<TestFeatureB>());
  EXPECT_FALSE(feature1.doesStartBefore<TestFeatureA>());
  EXPECT_TRUE(feature1.doesStartAfter<TestFeatureA>());
  EXPECT_FALSE(feature2.doesStartBefore<TestFeatureA>());
  EXPECT_TRUE(feature2.doesStartAfter<TestFeatureA>());
  EXPECT_FALSE(feature2.doesStartBefore<TestFeatureB>());
  EXPECT_TRUE(feature2.doesStartAfter<TestFeatureB>());
}

TEST(ApplicationServerTest, test_startsAfterCyclic) {
  bool failed = false;
  std::function<void(std::string const&)> callback = [&failed](std::string const&) {
    failed = true;
  };

  auto options =
      std::make_shared<options::ProgramOptions>("arangod", "something", "",
                                                "path");
  application_features::ApplicationServer server(options, "path");
  server.registerFailCallback(callback);

  server.addFeature<TestFeatureA>("feature1",
                                  std::vector<std::type_index>{
                                      std::type_index(typeid(TestFeatureB))},
                                  std::vector<std::type_index>{});
  server.addFeature<TestFeatureB>("feature2",
                                  std::vector<std::type_index>{
                                      std::type_index(typeid(TestFeatureA))},
                                  std::vector<std::type_index>{});

  try {
    server.setupDependencies(true);
  } catch (basics::Exception const& ex) {
    EXPECT_EQ(ex.code(), TRI_ERROR_INTERNAL);
    failed = true;
  }
  EXPECT_TRUE(failed);
}

TEST(ApplicationServerTest, test_startsBeforeCyclic) {
  bool failed = false;
  std::function<void(std::string const&)> callback = [&failed](std::string const&) {
    failed = true;
  };

  auto options =
      std::make_shared<options::ProgramOptions>("arangod", "something", "",
                                                "path");
  application_features::ApplicationServer server(options, "path");
  server.registerFailCallback(callback);

  server.addFeature<TestFeatureA>("feature1", std::vector<std::type_index>{},
                                  std::vector<std::type_index>{
                                      std::type_index(typeid(TestFeatureB))});
  server.addFeature<TestFeatureB>("feature2", std::vector<std::type_index>{},
                                  std::vector<std::type_index>{
                                      std::type_index(typeid(TestFeatureA))});

  try {
    server.setupDependencies(true);
  } catch (basics::Exception const& ex) {
    EXPECT_EQ(ex.code(), TRI_ERROR_INTERNAL);
    failed = true;
  }
  EXPECT_TRUE(failed);
}
