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
#include <string>
#include <vector>

#include "ApplicationFeatures/ApplicationFeature.h"

namespace arangodb {
namespace options {
class ProgramOptions;
}

class LoggerFeature;
class ShellColorsFeature;
class VersionFeature;

class ConfigFeature final : public application_features::ApplicationFeature {
 public:
  static constexpr std::string_view name() noexcept { return "Config"; }

  template<typename Server>
  ConfigFeature(Server& server, std::string const& progname,
                std::string const& configFilename = "")
      : application_features::ApplicationFeature{server, *this},
        _version{[&server]() {
          return server.template hasFeature<VersionFeature>()
                     ? &server.template getFeature<VersionFeature>()
                     : nullptr;
        }()},
        _file(configFilename),
        _progname(progname),
        _checkConfiguration(false),
        _honorNsswitch(false) {
    static_assert(
        Server::template isCreatedAfter<ConfigFeature, VersionFeature>());

    setOptional(false);
    startsAfter<LoggerFeature, Server>();
    startsAfter<ShellColorsFeature, Server>();
  }

  void collectOptions(std::shared_ptr<options::ProgramOptions>) override final;
  void loadOptions(std::shared_ptr<options::ProgramOptions>,
                   char const* binaryPath) override final;

  void prepare() override final;

 private:
  void loadConfigFile(std::shared_ptr<options::ProgramOptions>,
                      std::string const& progname, char const* binaryPath);

  VersionFeature* _version;
  std::string _file;
  std::string _progname;
  std::vector<std::string> _defines;
  bool _checkConfiguration;
  bool _honorNsswitch;  // If this is set to true, the internal override is
                        // deactivated.
};

}  // namespace arangodb
