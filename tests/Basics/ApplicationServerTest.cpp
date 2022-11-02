////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
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
/// @author Jan Steemann
/// @author Copyright 2017, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "Basics/Common.h"

#include "gtest/gtest.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "ApplicationFeatures/ApplicationFeature.h"
#include "Basics/Exceptions.h"
#include "ProgramOptions/ProgramOptions.h"
#include "RestServer/arangod.h"

using namespace arangodb;

class TestFeatureA;
class TestFeatureB;

using TestFeatures = TypeList<TestFeatureA, TestFeatureB>;
using TestApplicationServer = ApplicationServerT<TestFeatures>;
using TestApplicationFeature = ApplicationFeatureT<TestApplicationServer>;

class TestFeatureA : public TestApplicationFeature {
 public:
  static constexpr std::string_view name() { return "TestFeatureA"; }

  using ApplicationFeature::startsAfter;
  using ApplicationFeature::startsBefore;

  TestFeatureA(TestApplicationServer& server,
               std::vector<size_t> const& startsAfter,
               std::vector<size_t> const& startsBefore)
      : TestApplicationFeature(server, *this) {
    for (auto const type : startsAfter) {
      this->startsAfter(type);
    }
    for (auto const type : startsBefore) {
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
               std::vector<size_t> const& startsAfter,
               std::vector<size_t> const& startsBefore)
      : TestApplicationFeature(server, *this) {
    for (auto const type : startsAfter) {
      this->startsAfter(type);
    }
    for (auto const type : startsBefore) {
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

  auto& feature1 = server.addFeature<TestFeatureA>(std::vector<size_t>{},
                                                   std::vector<size_t>{});

  auto& feature2 = server.addFeature<TestFeatureB>(
      std::vector<size_t>{TestApplicationServer::id<TestFeatureA>()},
      std::vector<size_t>{});

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
      std::vector<size_t>{TestApplicationServer::id<TestFeatureB>()},
      std::vector<size_t>{});
  server.addFeature<TestFeatureB>(
      std::vector<size_t>{TestApplicationServer::id<TestFeatureA>()},
      std::vector<size_t>{});

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
      std::vector<size_t>{},
      std::vector<size_t>{TestApplicationServer::id<TestFeatureB>()});
  server.addFeature<TestFeatureB>(
      std::vector<size_t>{},
      std::vector<size_t>{TestApplicationServer::id<TestFeatureA>()});

  try {
    server.setupDependencies(true);
  } catch (basics::Exception const& ex) {
    EXPECT_EQ(ex.code(), TRI_ERROR_INTERNAL);
    failed = true;
  }
  EXPECT_TRUE(failed);
}
