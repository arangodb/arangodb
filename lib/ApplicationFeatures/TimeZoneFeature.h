////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
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
/// @author Andreas Dominik Jung
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <memory>
#include <string>

#include "ApplicationFeatures/ApplicationFeature.h"

namespace arangodb {
namespace application_features {
class ApplicationServer;
}
namespace options {
class ProgramOptions;
}

class TimeZoneFeature final : public application_features::ApplicationFeature {
 public:
  explicit TimeZoneFeature(application_features::ApplicationServer& server);
  ~TimeZoneFeature();

  void prepare() override final;
  void start() override final;

  static void prepareTimeZoneData(std::string const& binaryPath,
                                  std::string const& binaryExecutionPath,
                                  std::string const& binaryName);

 private:
  char const* _binaryPath;
};

}  // namespace arangodb
