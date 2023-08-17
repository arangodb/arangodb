////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
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
////////////////////////////////////////////////////////////////////////////////

#include "ApplicationFeatures/OptionsCheckFeature.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "ProgramOptions/Option.h"
#include "ProgramOptions/Parameters.h"
#include "ProgramOptions/ProgramOptions.h"
#include "Logger/LogMacros.h"

using namespace arangodb::options;

namespace arangodb {

void OptionsCheckFeature::prepare() {
  auto const& options = server().options();

  auto const& modernizedOptions = options->modernizedOptions();
  if (!modernizedOptions.empty()) {
    for (auto const& it : modernizedOptions) {
      LOG_TOPIC("3e342", WARN, Logger::STARTUP)
          << "please note that the specified option '--" << it.first
          << " has been renamed to '--" << it.second << "'";
    }

    LOG_TOPIC("27c9c", INFO, Logger::STARTUP)
        << "please read the release notes about changed options";
  }

  options->walk(
      [](Section const&, Option const& option) {
        if (option.hasDeprecatedIn()) {
          LOG_TOPIC("78b1e", WARN, Logger::STARTUP)
              << "option '" << option.displayName() << "' is deprecated since "
              << option.deprecatedInString()
              << " and may be removed or unsupported in a future version";
        }
      },
      true, true);

  // inform about obsolete options
  options->walk(
      [](Section const&, Option const& option) {
        if (option.hasFlag(arangodb::options::Flags::Obsolete)) {
          LOG_TOPIC("6843e", WARN, Logger::STARTUP)
              << "obsolete option '" << option.displayName()
              << "' used in configuration. "
              << "Setting this option does not have any effect.";
        }
      },
      true, true);

  // inform about experimental options
  options->walk(
      [](Section const&, Option const& option) {
        if (option.hasFlag(arangodb::options::Flags::Experimental)) {
          LOG_TOPIC("de8f3", WARN, Logger::STARTUP)
              << "experimental option '" << option.displayName()
              << "' used in configuration.";
        }
      },
      true, true);
}

}  // namespace arangodb
