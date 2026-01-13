////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Jan Steemann
/// @author Copyright 2017, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "gtest/gtest.h"

#include <typeindex>
#include <vector>

#include "ApplicationFeatures/ApplicationServer.h"
#include "ApplicationFeatures/ApplicationFeature.h"
#include "Basics/Exceptions.h"
#include "ProgramOptions/ProgramOptions.h"
#include "RestServer/arangod.h"

using namespace arangodb;
using namespace arangodb::application_features;

class TestFeatureA;
class TestFeatureB;

using TestApplicationServer = ApplicationServer;
using TestApplicationFeature = ApplicationFeatureT<TestApplicationServer>;

class TestFeatureA : public TestApplicationFeature {
 public:
  static constexpr std::string_view name() { return "TestFeatureA"; }

  using ApplicationFeature::startsAfter;
  using ApplicationFeature::startsBefore;

  TestFeatureA(TestApplicationServer& server,
               std::vector<std::type_index> const& startsAfter,
               std::vector<std::type_index> const& startsBefore)
      : TestApplicationFeature(server, *this) {
    for (auto const& type : startsAfter) {
      this->startsAfter(type);
    }
    for (auto const& type : startsBefore) {
      this->startsBefore(type);
    }
  }
};

class TestFeatureB : public TestApplicationFeature {
 public:
  static constexpr std::string_view name() { return "TestFeatureB"; }

  using ApplicationFeature::startsAfter;
  using ApplicationFeature::startsBefore;

  TestFeatureB(TestApplicationServer& server,
               std::vector<std::type_index> const& startsAfter,
               std::vector<std::type_index> const& startsBefore)
      : TestApplicationFeature(server, *this) {
    for (auto const& type : startsAfter) {
      this->startsAfter(type);
    }
    for (auto const& type : startsBefore) {
      this->startsBefore(type);
    }
  }
};

TEST(ApplicationServerTest, test_startsAfterValid) {
  bool failed = false;
  std::function<void(std::string const&)> callback =
      [&failed](std::string const&) { failed = true; };

  auto options = std::make_shared<options::ProgramOptions>(
      "arangod", "something", "", "path");
  TestApplicationServer server(options, "path");
  server.registerFailCallback(callback);

  auto& feature1 = server.addFeature<TestFeatureA>(std::vector<std::type_index>{},
                                                   std::vector<std::type_index>{});

  auto& feature2 = server.addFeature<TestFeatureB>(
      std::vector<std::type_index>{std::type_index(typeid(TestFeatureA))},
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
  std::function<void(std::string const&)> callback =
      [&failed](std::string const&) { failed = true; };

  auto options = std::make_shared<options::ProgramOptions>(
      "arangod", "something", "", "path");
  TestApplicationServer server(options, "path");
  server.registerFailCallback(callback);

  server.addFeature<TestFeatureA>(
      std::vector<std::type_index>{std::type_index(typeid(TestFeatureB))},
      std::vector<std::type_index>{});
  server.addFeature<TestFeatureB>(
      std::vector<std::type_index>{std::type_index(typeid(TestFeatureA))},
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
  std::function<void(std::string const&)> callback =
      [&failed](std::string const&) { failed = true; };

  auto options = std::make_shared<options::ProgramOptions>(
      "arangod", "something", "", "path");
  TestApplicationServer server(options, "path");
  server.registerFailCallback(callback);

  server.addFeature<TestFeatureA>(
      std::vector<std::type_index>{},
      std::vector<std::type_index>{std::type_index(typeid(TestFeatureB))});
  server.addFeature<TestFeatureB>(
      std::vector<std::type_index>{},
      std::vector<std::type_index>{std::type_index(typeid(TestFeatureA))});

  try {
    server.setupDependencies(true);
  } catch (basics::Exception const& ex) {
    EXPECT_EQ(ex.code(), TRI_ERROR_INTERNAL);
    failed = true;
  }
  EXPECT_TRUE(failed);
}
