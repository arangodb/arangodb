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

#include "icu-helper.h"
#include <unicode/udata.h>

#include "ApplicationFeatures/LanguageFeature.h"
#include "Basics/Utf8Helper.h"
#include "Basics/directories.h"
#include "Basics/files.h"

std::string IcuInitializer::icuData;
std::string IcuInitializer::exePath;

void IcuInitializer::reinit() {
  loadIcuData();
  arangodb::basics::Utf8Helper::DefaultUtf8Helper.setCollatorLanguage(
      "", arangodb::basics::LanguageType::DEFAULT);
}

void IcuInitializer::setup(char const* path) {
  if (!icuData.empty()) {
    return;
  }
  exePath = path;
  std::string p = loadIcuData();
  if (icuData.empty() ||
      !arangodb::basics::Utf8Helper::DefaultUtf8Helper.setCollatorLanguage(
          "", arangodb::basics::LanguageType::DEFAULT)) {
    std::string msg =
        "failed to initialize ICU library. The environment variable "
        "ICU_DATA_LEGACY";
    if (getenv("ICU_DATA_LEGACY") != nullptr) {
      msg += "='" + std::string(getenv("ICU_DATA_LEGACY")) + "'";
    }
    msg +=
        " should point to the directory containing the icudtl_legacy.dat file. "
        "We "
        "searched here: " +
        p;
    std::cerr << msg << std::endl;
  }
}

std::string IcuInitializer::loadIcuData() {
  std::string p;
  std::string binaryPath = TRI_LocateBinaryPath(exePath.c_str());
  icuData = arangodb::LanguageFeature::prepareIcu(TEST_DIRECTORY, binaryPath, p,
                                                  "basics_suite");
  UErrorCode status = U_ZERO_ERROR;
  udata_setCommonData(reinterpret_cast<void const*>(icuData.c_str()), &status);
  return p;
}
