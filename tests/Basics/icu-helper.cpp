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
/// @author Andreas Streichardt
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "icu-helper.h"
#include "ApplicationFeatures/LanguageFeature.h"
#include "Basics/Utf8Helper.h"
#include "Basics/directories.h"
#include "Basics/files.h"
#include "Basics/memory.h"

#include <iostream>

static IcuInitializer theInstance; // must be here to call the dtor

bool IcuInitializer::initialized = false;
void* IcuInitializer::icuDataPtr = nullptr;
  
IcuInitializer::IcuInitializer() {}

IcuInitializer::~IcuInitializer() {
  if (icuDataPtr != nullptr) {
    TRI_Free(icuDataPtr);
  }
  icuDataPtr = nullptr;
}
  
void IcuInitializer::setup(char const* path) {
  if (initialized) {
    return;
  }
  initialized = true;
  std::string p;
  std::string binaryPath = TRI_LocateBinaryPath(path);
  icuDataPtr = arangodb::LanguageFeature::prepareIcu(TEST_DIRECTORY, binaryPath, p, "basics_suite");
  if (icuDataPtr == nullptr ||
      !arangodb::basics::Utf8Helper::DefaultUtf8Helper.setCollatorLanguage("", icuDataPtr)) {
    std::string msg =
      "failed to initialize ICU library. The environment variable ICU_DATA";
    if (getenv("ICU_DATA") != nullptr) {
      msg += "='" + std::string(getenv("ICU_DATA")) + "'";
    }
    msg += " should point to the directory containing the icudtl.dat file. We searched here: " + p;
    std::cerr << msg << std::endl;
  }
}
