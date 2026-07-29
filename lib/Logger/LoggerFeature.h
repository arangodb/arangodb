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
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <memory>

#include "ApplicationFeatures/ApplicationFeature.h"
#include "ApplicationFeatures/ShellColorsFeature.h"
#include "ApplicationFeatures/VersionFeature.h"
#include "Logger/LogApiOptions.h"
#include "Logger/LoggerOptions.h"
#include <velocypack/Builder.h>

namespace arangodb {
namespace application_features {
class ApplicationServer;
}
namespace options {
class ProgramOptions;
}

class ShellColorsFeature;
class VersionFeature;

class LoggerFeature final : public application_features::ApplicationFeature {
 public:
  static constexpr std::string_view name() { return "Logger"; }

  LoggerFeature(application_features::ApplicationServer& server, bool threaded,
                LoggerOptions options, LogApiOptions apiOptions = {});
  LoggerFeature(application_features::ApplicationServer& server, bool threaded);

  ~LoggerFeature();

  void prepare() override final;
  void unprepare() override final;

  void disableThreaded() noexcept { _threaded = false; }
  void setSupervisor(bool supervisor) noexcept { _supervisor = supervisor; }

  // TODO(COR-793): Move apiEnabled and apiSwitch to appropriate place
  bool isAPIEnabled() const noexcept { return _apiOptions.apiEnabled; }
  bool onlySuperUser() const noexcept { return _apiOptions.apiSwitch == "jwt"; }

 private:
  LoggerFeature(application_features::ApplicationServer& server,
                std::type_index registration, bool threaded,
                LoggerOptions options);

  LoggerOptions _options;
  LogApiOptions _apiOptions;
  bool _supervisor = false;
  bool _threaded = false;
};

}  // namespace arangodb
