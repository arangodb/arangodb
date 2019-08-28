////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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

#include "Basics/Common.h"
#include "Basics/directories.h"

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

int main(int argc, char* argv[]) {
  TRI_GET_ARGV(argc, argv);
  return ClientFeature::runMain(argc, argv, [&](int argc, char* argv[]) -> int {
    ArangoGlobalContext context(argc, argv, BIN_DIRECTORY);
    context.installHup();

    std::shared_ptr<options::ProgramOptions> options(
        new options::ProgramOptions(argv[0], "Usage: arangoimport [<options>]",
                                    "For more information use:", BIN_DIRECTORY));
    ApplicationServer server(options, BIN_DIRECTORY);
    int ret;

    server.addFeature<BasicFeaturePhaseClient>(
        std::make_unique<BasicFeaturePhaseClient>(server));
    server.addFeature<CommunicationFeaturePhase>(
        std::make_unique<CommunicationFeaturePhase>(server));
    server.addFeature<GreetingsFeaturePhase>(
        std::make_unique<GreetingsFeaturePhase>(server, true));

    server.addFeature<ClientFeature>(std::make_unique<ClientFeature>(server, false));
    server.addFeature<ConfigFeature>(
        std::make_unique<ConfigFeature>(server, "arangoimport"));
    server.addFeature<ImportFeature>(std::make_unique<ImportFeature>(server, &ret));
    server.addFeature<LoggerFeature>(std::make_unique<LoggerFeature>(server, false));
    server.addFeature<RandomFeature>(std::make_unique<RandomFeature>(server));
    server.addFeature<ShellColorsFeature>(std::make_unique<ShellColorsFeature>(server));
    server.addFeature<ShutdownFeature>(std::make_unique<ShutdownFeature>(
        server, std::vector<std::type_index>{typeid(ImportFeature)}));
    server.addFeature<SslFeature>(std::make_unique<SslFeature>(server));
    server.addFeature<TempFeature>(
        std::make_unique<TempFeature>(server, "arangoimport"));
    server.addFeature<VersionFeature>(std::make_unique<VersionFeature>(server));

#ifdef USE_ENTERPRISE
    server.addFeature(new EncryptionFeature(server));
#endif

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
