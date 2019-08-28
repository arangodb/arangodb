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
#include "ApplicationFeatures/LanguageFeature.h"
#include "ApplicationFeatures/ShellColorsFeature.h"
#include "ApplicationFeatures/ShutdownFeature.h"
#include "ApplicationFeatures/TempFeature.h"
#include "ApplicationFeatures/V8PlatformFeature.h"
#include "ApplicationFeatures/V8SecurityFeature.h"
#include "ApplicationFeatures/VersionFeature.h"
#include "Basics/ArangoGlobalContext.h"
#include "FeaturePhases/BasicFeaturePhaseClient.h"
#include "FeaturePhases/V8ShellFeaturePhase.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerFeature.h"
#include "Logger/LoggerStream.h"
#include "ProgramOptions/ProgramOptions.h"
#include "Random/RandomFeature.h"
#include "Shell/ClientFeature.h"
#include "Shell/ConsoleFeature.h"
#include "Shell/ShellFeature.h"
#include "Shell/V8ShellFeature.h"
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

    std::string name = context.binaryName();
    std::shared_ptr<options::ProgramOptions> options(
        new options::ProgramOptions(argv[0], "Usage: " + name + " [<options>]",
                                    "For more information use:", BIN_DIRECTORY));
    ApplicationServer server(options, BIN_DIRECTORY);
    int ret = EXIT_SUCCESS;

    try {
      server.addFeature<BasicFeaturePhaseClient>(
          std::make_unique<BasicFeaturePhaseClient>(server));
      server.addFeature<CommunicationFeaturePhase>(
          std::make_unique<CommunicationFeaturePhase>(server));
      server.addFeature<GreetingsFeaturePhase>(
          std::make_unique<GreetingsFeaturePhase>(server, true));
      server.addFeature<V8ShellFeaturePhase>(std::make_unique<V8ShellFeaturePhase>(server));

      server.addFeature<ClientFeature>(std::make_unique<ClientFeature>(server, true));
      server.addFeature<ConfigFeature>(std::make_unique<ConfigFeature>(server, name));
      server.addFeature<ConsoleFeature>(std::make_unique<ConsoleFeature>(server));
      server.addFeature<LanguageFeature>(std::make_unique<LanguageFeature>(server));
      server.addFeature<LoggerFeature>(std::make_unique<LoggerFeature>(server, false));
      server.addFeature<RandomFeature>(std::make_unique<RandomFeature>(server));
      server.addFeature<ShellColorsFeature>(std::make_unique<ShellColorsFeature>(server));
      server.addFeature<ShellFeature>(std::make_unique<ShellFeature>(server, &ret));
      server.addFeature<ShutdownFeature>(std::make_unique<ShutdownFeature>(
          server, std::vector<std::type_index>{typeid(ShellFeature)}));
      server.addFeature<SslFeature>(std::make_unique<SslFeature>(server));
      server.addFeature<TempFeature>(std::make_unique<TempFeature>(server, name));
      server.addFeature<V8PlatformFeature>(std::make_unique<V8PlatformFeature>(server));
      server.addFeature<V8SecurityFeature>(std::make_unique<V8SecurityFeature>(server));
      server.addFeature<V8ShellFeature>(std::make_unique<V8ShellFeature>(server, name));
      server.addFeature<VersionFeature>(std::make_unique<VersionFeature>(server));

#ifdef USE_ENTERPRISE
      server.addFeature<EncryptionFeature>(std::make_unique<EncryptionFeature>(server));
#endif

      server.run(argc, argv);
      if (server.helpShown()) {
        // --help was displayed
        ret = EXIT_SUCCESS;
      }
    } catch (std::exception const& ex) {
      LOG_TOPIC("da777", ERR, arangodb::Logger::FIXME)
          << "arangosh terminated because of an unhandled exception: " << ex.what();
      ret = EXIT_FAILURE;
    } catch (...) {
      LOG_TOPIC("ed049", ERR, arangodb::Logger::FIXME)
          << "arangosh terminated because of an unhandled exception of "
             "unknown type";
      ret = EXIT_FAILURE;
    }

    return context.exit(ret);
  });
}
