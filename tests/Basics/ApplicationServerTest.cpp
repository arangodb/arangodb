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

#include "catch.hpp"

#include "ApplicationFeatures/ApplicationFeature.h"
#include "ApplicationFeatures/ApplicationServer.h"
#include "ProgramOptions/ProgramOptions.h"
#include "Basics/Exceptions.h"

using namespace arangodb;

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief setup
////////////////////////////////////////////////////////////////////////////////

class TestFeature : public application_features::ApplicationFeature {
 public:
  TestFeature(application_features::ApplicationServer* server,
              std::string const& name,
              std::vector<std::string> const& startsAfter,
              std::vector<std::string> const& startsBefore) 
    : ApplicationFeature(server, name) {
    for (auto const& it : startsAfter) {
      this->startsAfter(it);
    }
    for (auto const& it : startsBefore) {
      this->startsBefore(it);
    }
  }
};

TEST_CASE("ApplicationServerTest", "[applicationservertest]") {

SECTION("test_startsAfterValid") {
  bool failed = false;
  std::function<void(std::string const&)> callback = [&failed](std::string const&) {
    failed = true;
  };

  auto options = std::make_shared<options::ProgramOptions>("arangod", "something", "", "path");
  application_features::ApplicationServer server(options, "path");

  auto feature1 = std::make_unique<TestFeature>(&server, "feature1", std::vector<std::string>{ }, std::vector<std::string>{ });
  auto feature2 = std::make_unique<TestFeature>(&server, "feature2", std::vector<std::string>{ "feature1" }, std::vector<std::string>{ });

  server.registerFailCallback(callback);
  server.addFeature(feature1.get());
  server.addFeature(feature2.get());
  server.setupDependencies(true);

  CHECK(!failed);
  CHECK(feature1->doesStartBefore("feature2"));
  CHECK(!feature1->doesStartAfter("feature2"));
  CHECK(!feature1->doesStartBefore("feature1"));
  CHECK(feature1->doesStartAfter("feature1"));
  CHECK(!feature2->doesStartBefore("feature1"));
  CHECK(feature2->doesStartAfter("feature1"));
  CHECK(!feature2->doesStartBefore("feature2"));
  CHECK(feature2->doesStartAfter("feature2"));

  feature1.release();
  feature2.release();
}

SECTION("test_startsAfterCyclic") {
  bool failed = false;
  std::function<void(std::string const&)> callback = [&failed](std::string const&) {
    failed = true;
  };

  auto options = std::make_shared<options::ProgramOptions>("arangod", "something", "", "path");
  application_features::ApplicationServer server(options, "path");

  auto feature1 = std::make_unique<TestFeature>(&server, "feature1", std::vector<std::string>{ "feature2" }, std::vector<std::string>{ });
  auto feature2 = std::make_unique<TestFeature>(&server, "feature2", std::vector<std::string>{ "feature1" }, std::vector<std::string>{ });

  server.registerFailCallback(callback);
  server.addFeature(feature1.get());
  server.addFeature(feature2.get());

  try {
    server.setupDependencies(true);
  } catch (basics::Exception const& ex) {
    CHECK(ex.code() == TRI_ERROR_INTERNAL);
    failed = true;
  }
  CHECK(failed);
  
  feature1.release();
  feature2.release();
}  

SECTION("test_startsBeforeCyclic") {
  bool failed = false;
  std::function<void(std::string const&)> callback = [&failed](std::string const&) {
    failed = true;
  };

  auto options = std::make_shared<options::ProgramOptions>("arangod", "something", "", "path");
  application_features::ApplicationServer server(options, "path");

  auto feature1 = std::make_unique<TestFeature>(&server, "feature1", std::vector<std::string>{ }, std::vector<std::string>{ "feature2" });
  auto feature2 = std::make_unique<TestFeature>(&server, "feature2", std::vector<std::string>{ }, std::vector<std::string>{ "feature1" });

  server.registerFailCallback(callback);
  server.addFeature(feature1.get());
  server.addFeature(feature2.get());

  try {
    server.setupDependencies(true);
  } catch (basics::Exception const& ex) {
    CHECK(ex.code() == TRI_ERROR_INTERNAL);
    failed = true;
  }
  CHECK(failed);
  
  feature1.release();
  feature2.release();
}  
  
}

