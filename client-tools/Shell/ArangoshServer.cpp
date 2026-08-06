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

#include "Shell/ArangoshServer.h"

#include "ApplicationFeatures/CommunicationFeaturePhase.h"
#include "ApplicationFeatures/ConfigFeature.h"
#include "ApplicationFeatures/FileSystemFeature.h"
#include "ApplicationFeatures/GreetingsFeaturePhase.h"
#include "ApplicationFeatures/LanguageFeature.h"
#include "ApplicationFeatures/OptionsCheckFeature.h"
#include "ApplicationFeatures/ProcessEnvironmentFeature.h"
#include "ApplicationFeatures/ShellColorsFeature.h"
#include "ApplicationFeatures/ShutdownFeature.h"
#include "ApplicationFeatures/TempFeature.h"
#include "FeaturePhases/BasicFeaturePhaseClient.h"
#include "FeaturePhases/V8ShellFeaturePhase.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerFeature.h"
#include "Logger/LoggerStream.h"
#include "ProgramOptions/ProgramOptions.h"
#include "Random/RandomFeature.h"
#include "Shell/ClientFeature.h"
#include "Shell/ProcessMonitoringFeature.h"
#include "Shell/ShellConsoleFeature.h"
#include "Shell/ShellFeature.h"
#include "Shell/V8ShellFeature.h"
#include "Ssl/SslFeature.h"
#include "V8/V8PlatformFeature.h"
#include "V8/V8SecurityFeature.h"

#ifdef USE_ENTERPRISE
#include "Enterprise/Encryption/EncryptionFeature.h"
#endif

#include <array>
#include <typeindex>
#include <utility>

namespace arangodb {

using namespace arangodb::application_features;

ArangoshServer::ArangoshServer(std::shared_ptr<options::ProgramOptions> options,
                               char const* binaryPath, std::string binaryName,
                               int* ret)
    : OptionProvidingServer<ArangoshOptionProviders>(
          options, binaryPath, std::move(binaryName), ret) {
  // Set a different default for the ClientFeature
  mutableOptions<ClientOptionsProvider>().allowJwtSecret = true;
}

void ArangoshServer::addFeatures() {
  // Phases first
  addFeature<BasicFeaturePhaseClient>();
  addFeature<CommunicationFeaturePhase>();
  addFeature<GreetingsFeaturePhase>(std::true_type{});

  addFeature<OptionsCheckFeature>();
  addFeature<ShellColorsFeature>();
  addFeature<ShutdownFeature>(
      std::array{std::type_index(typeid(ShellFeature))});
  addFeature<SslFeature>();
  addFeature<V8ShellFeaturePhase>();
}

void ArangoshServer::addFeaturesWithOptionProvider() {
  addFeature<LoggerFeature>(false, getOptions<LoggerOptionsProvider>());
  addFeature<ConfigFeature>(getOptions<ConfigOptionsProvider>());
  addFeature<TempFeature>(_binaryName, getOptions<TempOptionsProvider>());
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  addFeature<ProcessEnvironmentFeature>(
      _binaryName, getOptions<ProcessEnvironmentOptionsProvider>());
#endif

  addFeature<V8PlatformFeature>(getOptions<V8PlatformOptionsProvider>());
  auto& v8SecurityFeature = addFeature<V8SecurityFeature>(
      AllowListStrictness::NONSTRICT, getOptions<V8SecurityOptionsProvider>());
  auto& v8ShellFeature = addFeature<V8ShellFeature>(
      _binaryName, getOptions<V8ShellOptionsProvider>());
  addFeature<ProcessMonitoringFeature>(v8ShellFeature, v8SecurityFeature);

  addFeature<FileSystemFeature>(getOptions<FileSystemOptionsProvider>());
  addFeature<RandomFeature>(getOptions<RandomOptionsProvider>());
  addFeature<LanguageFeature>(getOptions<LanguageOptionsProvider>());
#ifdef USE_ENTERPRISE
  addFeature<EncryptionFeature>(getOptions<EncryptionOptionsProvider>());
#endif
  auto& client = addFeature<HttpEndpointProvider, ClientFeature>(
      getOptions<ClientOptionsProvider>());
  auto& console = addFeature<ShellConsoleFeature>(
      getOptions<ShellConsoleOptionsProvider>());
  addFeature<ShellFeature>(_ret, client, console,
                           getOptions<ShellOptionsProvider>());
}

}  // namespace arangodb
