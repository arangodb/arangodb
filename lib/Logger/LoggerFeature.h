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
/// @author Dr. Frank Celler
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <memory>

#include "ApplicationFeatures/ApplicationFeature.h"
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

  template<typename Server>
  LoggerFeature(Server& server, bool threaded)
      : LoggerFeature(server, Server::template id<LoggerFeature>(), threaded) {
    startsAfter<ShellColorsFeature, Server>();
    startsAfter<VersionFeature, Server>();
  }

  ~LoggerFeature();

  void collectOptions(std::shared_ptr<options::ProgramOptions>) override final;
  void loadOptions(std::shared_ptr<options::ProgramOptions>,
                   char const* binaryPath) override final;
  void validateOptions(std::shared_ptr<options::ProgramOptions>) override final;
  void prepare() override final;
  void unprepare() override final;

  void disableThreaded() noexcept { _threaded = false; }
  void setSupervisor(bool supervisor) noexcept { _supervisor = supervisor; }

  bool isAPIEnabled() const noexcept { return _options.apiEnabled; }
  bool onlySuperUser() const noexcept { return _options.apiSwitch == "jwt"; }

 private:
  LoggerFeature(application_features::ApplicationServer& server,
                size_t registration, bool threaded);

  LoggerOptions _options;
  bool _supervisor = false;
  bool _threaded = false;
};

}  // namespace arangodb
