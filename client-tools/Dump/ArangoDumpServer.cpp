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

#include "Dump/ArangoDumpServer.h"

#include "ApplicationFeatures/BumpFileDescriptorsFeature.h"
#include "ApplicationFeatures/CommunicationFeaturePhase.h"
#include "ApplicationFeatures/ConfigFeature.h"
#include "ApplicationFeatures/FileSystemFeature.h"
#include "ApplicationFeatures/GreetingsFeaturePhase.h"
#include "ApplicationFeatures/OptionsCheckFeature.h"
#include "ApplicationFeatures/ProcessEnvironmentFeature.h"
#include "ApplicationFeatures/ShellColorsFeature.h"
#include "ApplicationFeatures/ShutdownFeature.h"
#include "ApplicationFeatures/VersionFeature.h"
#include "Dump/DumpFeature.h"
#include "FeaturePhases/BasicFeaturePhaseClient.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerFeature.h"
#include "Logger/LoggerStream.h"
#include "ProgramOptions/ProgramOptions.h"
#include "Random/RandomFeature.h"
#include "Shell/ClientFeature.h"
#include "Ssl/SslFeature.h"

#ifdef USE_ENTERPRISE
#include "Enterprise/Encryption/EncryptionFeature.h"
#endif

#include <array>
#include <limits>
#include <typeindex>
#include <utility>

namespace arangodb {

using namespace arangodb::application_features;

ArangoDumpServer::ArangoDumpServer(
    std::shared_ptr<options::ProgramOptions> options, char const* binaryPath,
    std::string binaryName, int* ret)
    : OptionProvidingServer<ArangoDumpOptionProviders>(
          options, binaryPath, std::move(binaryName), ret) {}

void ArangoDumpServer::addFeatures() {
  addFeature<BasicFeaturePhaseClient>();
  addFeature<CommunicationFeaturePhase>();
  addFeature<GreetingsFeaturePhase>(std::true_type{});
  auto& client = addFeature<HttpEndpointProvider, ClientFeature>(
      true, std::numeric_limits<size_t>::max());
  // TODO: COR-788: Migrate ConfigFeature
  addFeature<ConfigFeature>(_binaryName);
  addFeature<OptionsCheckFeature>();
  addFeature<ShellColorsFeature>();
  addFeature<ShutdownFeature>(std::array{std::type_index(typeid(DumpFeature))});
  addFeature<SslFeature>();
#ifdef TRI_HAVE_GETRLIMIT
  addFeature<BumpFileDescriptorsFeature>("--descriptors-minimum");
#endif
  addFeature<DumpFeature>(client, *_ret);
}

void ArangoDumpServer::addFeaturesWithOptionProvider() {
  addFeature<VersionFeature>(getOptions<VersionOptionsProvider>());
  addFeature<LoggerFeature>(false, getOptions<LoggerOptionsProvider>());
  addFeature<FileSystemFeature>(getOptions<FileSystemOptionsProvider>());
  addFeature<RandomFeature>(getOptions<RandomOptionsProvider>());
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  addFeature<ProcessEnvironmentFeature>(
      _binaryName, getOptions<ProcessEnvironmentOptionsProvider>());
#endif
#ifdef USE_ENTERPRISE
  addFeature<EncryptionFeature>(getOptions<EncryptionOptionsProvider>());
#endif
}

}  // namespace arangodb
