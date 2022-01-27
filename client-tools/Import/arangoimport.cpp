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
/// @author Dr. Frank Celler
////////////////////////////////////////////////////////////////////////////////

#include "arangoimport.h"

#include "Basics/Common.h"
#include "Basics/signals.h"
#include "Basics/directories.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "ApplicationFeatures/CommunicationFeaturePhase.h"
#include "ApplicationFeatures/ConfigFeature.h"
#include "ApplicationFeatures/GreetingsFeaturePhase.h"
#include "ApplicationFeatures/ShellColorsFeature.h"
#include "ApplicationFeatures/ShutdownFeature.h"
#include "ApplicationFeatures/TempFeature.h"
#include "ApplicationFeatures/VersionFeature.h"
#include "Basics/ArangoGlobalContext.h"
#include "FeaturePhases/BasicFeaturePhaseClient.h"
#include "Import/ImportFeature.h"
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

using namespace arangodb;
using namespace arangodb::application_features;

struct ArangoImportInitializer
    : public ArangoClientInitializer<ArangoImportServer> {
 public:
  ArangoImportInitializer(int* ret, char const* binaryName,
                          ArangoImportServer& client)
      : ArangoClientInitializer{binaryName, client}, _ret{ret} {}

  using ArangoClientInitializer::operator();

  void operator()(TypeTag<HttpEndpointProvider>) {
    _client.addFeature<HttpEndpointProvider, ClientFeature>(false);
  }

  void operator()(TypeTag<ImportFeature>) {
    _client.addFeature<ImportFeature>(*_ret);
  }

  void operator()(TypeTag<ShutdownFeature>) {
    _client.addFeature<ShutdownFeature>(
        std::vector<size_t>{ArangoImportServer::id<ImportFeature>()});
  }

  void operator()(TypeTag<TempFeature>) {
    _client.template addFeature<TempFeature>(_binaryName);
  }

 private:
  int* _ret;
};

int main(int argc, char* argv[]) {
  TRI_GET_ARGV(argc, argv);
  return ClientFeature::runMain(argc, argv, [&](int argc, char* argv[]) -> int {
    ArangoGlobalContext context(argc, argv, BIN_DIRECTORY);
    arangodb::signals::maskAllSignalsClient();
    context.installHup();

    std::shared_ptr<options::ProgramOptions> options(
        new options::ProgramOptions(
            argv[0], "Usage: arangoimport [<options>]",
            "For more information use:", BIN_DIRECTORY));

    int ret = EXIT_SUCCESS;
    ArangoImportServer server(options, BIN_DIRECTORY);
    ArangoImportInitializer init{&ret, context.binaryName().c_str(), server};
    ArangoImportServer::Features::visit(init);

    try {
      server.run(argc, argv);
      if (server.helpShown()) {
        // --help was displayed
        ret = EXIT_SUCCESS;
      }
    } catch (std::exception const& ex) {
      LOG_TOPIC("65114", ERR, arangodb::Logger::FIXME)
          << "arangoimport terminated because of an unhandled exception: "
          << ex.what();
      ret = EXIT_FAILURE;
    } catch (...) {
      LOG_TOPIC("63aa2", ERR, arangodb::Logger::FIXME)
          << "arangoimport terminated because of an unhandled exception of "
             "unknown type";
      ret = EXIT_FAILURE;
    }

    return context.exit(ret);
  });
}
