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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "arangodump.h"

#include "Basics/Common.h"
#include "Basics/signals.h"
#include "Basics/directories.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "ApplicationFeatures/CommunicationFeaturePhase.h"
#include "ApplicationFeatures/ConfigFeature.h"
#include "ApplicationFeatures/GreetingsFeaturePhase.h"
#include "ApplicationFeatures/ShellColorsFeature.h"
#include "ApplicationFeatures/ShutdownFeature.h"
#include "ApplicationFeatures/VersionFeature.h"
#include "Basics/ArangoGlobalContext.h"
#include "Dump/DumpFeature.h"
#include "FeaturePhases/BasicFeaturePhaseClient.h"
#include "Logger/Logger.h"
#include "Logger/LoggerFeature.h"
#include "Maskings/AttributeMasking.h"
#include "ProgramOptions/ProgramOptions.h"
#include "Random/RandomFeature.h"
#include "Shell/ClientFeature.h"
#include "Ssl/SslFeature.h"

#ifdef USE_ENTERPRISE
#include "Enterprise/Encryption/EncryptionFeature.h"
#include "Enterprise/Maskings/AttributeMaskingEE.h"
#endif

using namespace arangodb;
using namespace arangodb::application_features;

struct ArangoDumpInitializer
    : public ArangoClientInitializer<ArangoDumpServer> {
 public:
  ArangoDumpInitializer(int* ret, char const* binaryName,
                        ArangoDumpServer& client)
      : ArangoClientInitializer{binaryName, client}, _ret{ret} {}

  using ArangoClientInitializer::operator();

  void operator()(TypeTag<HttpEndpointProvider>) {
    _client.addFeature<HttpEndpointProvider, ClientFeature>(
        true, std::numeric_limits<size_t>::max());
  }

  void operator()(TypeTag<DumpFeature>) {
    _client.addFeature<DumpFeature>(*_ret);
  }

  void operator()(TypeTag<ShutdownFeature>) {
    _client.addFeature<ShutdownFeature>(
        std::vector<size_t>{ArangoDumpServer::id<DumpFeature>()});
  }

 private:
  int* _ret;
};

int main(int argc, char* argv[]) {
  TRI_GET_ARGV(argc, argv);
  return ClientFeature::runMain(argc, argv, [&](int argc, char* argv[]) -> int {
    int ret = EXIT_SUCCESS;

    ArangoGlobalContext context(argc, argv, BIN_DIRECTORY);
    arangodb::signals::maskAllSignalsClient();
    context.installHup();

    maskings::InstallMaskings();
#ifdef USE_ENTERPRISE
    maskings::InstallMaskingsEE();
#endif

    std::shared_ptr<options::ProgramOptions> options(
        new options::ProgramOptions(
            argv[0], "Usage: arangodump [<options>]",
            "For more information use:", BIN_DIRECTORY));
    ArangoDumpServer server(options, BIN_DIRECTORY);
    ArangoDumpInitializer init{&ret, context.binaryName().c_str(), server};
    ArangoDumpServer::Features::visit(init);

    try {
      server.run(argc, argv);
      if (server.helpShown()) {
        // --help was displayed
        ret = EXIT_SUCCESS;
      }
    } catch (std::exception const& ex) {
      LOG_TOPIC("8363a", ERR, arangodb::Logger::FIXME)
          << "arangodump terminated because of an unhandled exception: "
          << ex.what();
      ret = EXIT_FAILURE;
    } catch (...) {
      LOG_TOPIC("5ddce", ERR, arangodb::Logger::FIXME)
          << "arangodump terminated because of an unhandled exception of "
             "unknown type";
      ret = EXIT_FAILURE;
    }

    return context.exit(ret);
  });
}
