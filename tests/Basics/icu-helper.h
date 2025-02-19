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
/// @author Andreas Streichardt
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#pragma once

// This file is a hack. In normal server operations, we load the libicu data
// once and for all at server start in the LanguageFeature::prepare method.
// In unit tests, we want to test the initialization of the LanguageFeature
// from scratch (in the ArangoLanguageFeatureTest), so we need to be able to
// reinitialize the libicu data. This is what this file is for. The unit
// test main() function calls the static method IcuInitializer::setup() to
// initialize the libicu data (since we do not have a LanguageFeature in the
// unit tests). In the actual ArangoLanguageFeatureTest we shut down
// everything before each test to test initialization and reinit it later
// such that other tests can use Utf8Helper functions. This can happen in
// lots of places, for example in the agency store tests, where VPack
// objects are compared!

#include <string>

struct IcuInitializer {
  static void setup(char const* path);
  static std::string loadIcuData();
  static void reinit();

  static std::string exePath;  // initialized in setup, used in reinit
  static std::string icuData;
};
