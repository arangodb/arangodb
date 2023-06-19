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
/// @author Dan Larkin-York
////////////////////////////////////////////////////////////////////////////////

#include "LogLevels.h"

#include "utils/log.hpp"

#include <iostream>

namespace arangodb {
namespace tests {

static void LogCallback(irs::SourceLocation&& source,
                        std::string_view message) {
  std::cerr << source.file << ":" << source.line << ": " << source.func << ": "
            << message << std::endl;
}

IResearchLogSuppressor::IResearchLogSuppressor() {
  irs::log::SetCallback(irs::log::Level::kFatal, &LogCallback);
  irs::log::SetCallback(irs::log::Level::kError, nullptr);
  irs::log::SetCallback(irs::log::Level::kWarn, nullptr);
  irs::log::SetCallback(irs::log::Level::kInfo, nullptr);
  irs::log::SetCallback(irs::log::Level::kDebug, nullptr);
  irs::log::SetCallback(irs::log::Level::kTrace, nullptr);
}

}  // namespace tests
}  // namespace arangodb
