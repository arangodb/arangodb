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

#include "VPack/ArangoVPackServer.h"

#include "ApplicationFeatures/ConfigFeature.h"
#include "ApplicationFeatures/FileSystemFeature.h"
#include "ApplicationFeatures/GreetingsFeaturePhase.h"
#include "ApplicationFeatures/OptionsCheckFeature.h"
#include "ApplicationFeatures/ProcessEnvironmentFeature.h"
#include "ApplicationFeatures/ShellColorsFeature.h"
#include "ApplicationFeatures/ShutdownFeature.h"
#include "ApplicationFeatures/VersionFeature.h"
#include "FeaturePhases/BasicFeaturePhaseClient.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerFeature.h"
#include "Logger/LoggerStream.h"
#include "ProgramOptions/ProgramOptions.h"
#include "Random/RandomFeature.h"
#include "VPack/VPackFeature.h"

#include <array>
#include <typeindex>
#include <utility>

namespace arangodb {

using namespace arangodb::application_features;

ArangoVPackServer::ArangoVPackServer(
    std::shared_ptr<options::ProgramOptions> options, char const* binaryPath,
    std::string binaryName, int* ret)
    : ApplicationServer(options, binaryPath),
      _programOptions(std::move(options)),
      _binaryName(std::move(binaryName)),
      _ret(ret) {}

void ArangoVPackServer::collectOptions() {
  LOG_TOPIC("c0a21", TRACE, Logger::STARTUP)
      << "ArangoVPackServer::collectOptions";
  ApplicationServer::collectOptions();
  _optionProviders.declareOptions(_programOptions);
}

void ArangoVPackServer::validateOptions() {
  LOG_TOPIC("c0a22", TRACE, Logger::STARTUP)
      << "ArangoVPackServer::validateOptions";
  ApplicationServer::validateOptions();
  _optionProviders.validateOptions(_programOptions);
}

void ArangoVPackServer::addFeatures() {
  addFeature<BasicFeaturePhaseClient>();
  addFeature<GreetingsFeaturePhase>(std::true_type{});
  addFeature<VersionFeature>();
  addFeature<ConfigFeature>(_binaryName, "none");
  addFeature<OptionsCheckFeature>();
  addFeature<ShellColorsFeature>();
  addFeature<ShutdownFeature>(
      std::array{std::type_index(typeid(VPackFeature))});
  addFeature<VPackFeature>(_ret);
}

void ArangoVPackServer::addFeaturesWithOptionProvider() {
  addFeature<LoggerFeature>(
      false, _optionProviders.getOptions<LoggerOptionsProvider>());
  addFeature<FileSystemFeature>(
      _optionProviders.getOptions<FileSystemOptionsProvider>());
  addFeature<RandomFeature>(
      _optionProviders.getOptions<RandomOptionsProvider>());
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  addFeature<ProcessEnvironmentFeature>(
      _binaryName,
      _optionProviders.getOptions<ProcessEnvironmentOptionsProvider>());
#endif
}

}  // namespace arangodb
