////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2026 ArangoDB GmbH, Cologne, Germany
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
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <memory>
#include <string>

#include "ApplicationFeatures/ConfigOptionsProvider.h"
#include "ApplicationFeatures/FeatureOptionProviderContainer.h"
#include "ApplicationFeatures/FileSystemOptionsProvider.h"
#include "ApplicationFeatures/ProcessEnvironmentOptionsProvider.h"
#include "ApplicationFeatures/VersionOptionsProvider.h"
#include "Logger/Logger.h"
#include "Logger/LoggerOptionsProvider.h"
#include "Random/RandomOptionsProvider.h"

namespace arangodb {
namespace options {
class ProgramOptions;
}

// OptionProvider set shared by all client-tool binaries. Individual
// binaries can extend it with additional providers via `Extras...`.
template<class... Extras>
using CoreOptionProviders =
    application_features::FeatureOptionProviderContainer<
        ConfigOptionsProvider, FileSystemOptionsProvider, LoggerOptionsProvider,
        VersionOptionsProvider,
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
        ProcessEnvironmentOptionsProvider,
#endif
        RandomOptionsProvider, Extras...>;

inline void loadConfigAndEarlyLoggerOptions(
    LoggerOptionsProvider& loggerProvider,
    ConfigOptionsProvider& configProvider, bool versionRequested,
    std::shared_ptr<options::ProgramOptions> const& programOptions,
    char const* binaryPath, std::string const& binaryName) {
  Logger::setLogLevel(loggerProvider.options().levels);
}

}  // namespace arangodb
