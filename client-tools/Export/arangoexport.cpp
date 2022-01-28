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

#include "arangoexport.h"

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
#include "Export/ExportFeature.h"
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

using namespace arangodb;
using namespace arangodb::application_features;

class ArangoExportInitializer {
 public:
  ArangoExportInitializer(int* ret, char const* binaryName,
                          ArangoExportServer& client)
      : _ret{ret}, _binaryName{binaryName}, _client{client} {}

  template<typename T>
  void operator()(TypeTag<T>) {
    _client.addFeature<T>();
  }

  void operator()(TypeTag<GreetingsFeaturePhase>) {
    _client.addFeature<GreetingsFeaturePhase>(std::true_type{});
  }

  void operator()(TypeTag<ConfigFeature>) {
    _client.addFeature<ConfigFeature>(_binaryName);
  }

  void operator()(TypeTag<LoggerFeature>) {
    _client.addFeature<LoggerFeature>(false);
  }

  void operator()(TypeTag<HttpEndpointProvider>) {
    _client.addFeature<HttpEndpointProvider, ClientFeature>(false);
  }

  void operator()(TypeTag<ExportFeature>) {
    _client.addFeature<ExportFeature>(_ret);
  }

  void operator()(TypeTag<ShutdownFeature>) {
    constexpr size_t kFeatures[]{ArangoExportServer::id<ExportFeature>()};
    _client.addFeature<ShutdownFeature>(kFeatures);
  }

  void operator()(TypeTag<TempFeature>) {
    _client.addFeature<TempFeature>(_binaryName);
  }

 private:
  int* _ret;
  char const* _binaryName;
  ArangoExportServer& _client;
};

int main(int argc, char* argv[]) {
  TRI_GET_ARGV(argc, argv);
  return ClientFeature::runMain(argc, argv, [&](int argc, char* argv[]) -> int {
    ArangoGlobalContext context(argc, argv, BIN_DIRECTORY);
    arangodb::signals::maskAllSignalsClient();
    context.installHup();

    std::shared_ptr<options::ProgramOptions> options(
        new options::ProgramOptions(
            argv[0], "Usage: arangoexport [<options>]",
            "For more information use:", BIN_DIRECTORY));

    int ret = EXIT_SUCCESS;
    ArangoExportServer server(options, BIN_DIRECTORY);
    ArangoExportInitializer init{&ret, context.binaryName().c_str(), server};
    ArangoExportServer::Features::visit(init);

    try {
      server.run(argc, argv);
      if (server.helpShown()) {
        // --help was displayed
        ret = EXIT_SUCCESS;
      }
    } catch (std::exception const& ex) {
      LOG_TOPIC("c2ae7", ERR, Logger::STARTUP)
          << "arangoexport terminated because of an unhandled exception: "
          << ex.what();
      ret = EXIT_FAILURE;
    } catch (...) {
      LOG_TOPIC("dce1f", ERR, Logger::STARTUP)
          << "arangoexport terminated because of an unhandled exception of "
             "unknown type";
      ret = EXIT_FAILURE;
    }

    return context.exit(ret);
  });
}
