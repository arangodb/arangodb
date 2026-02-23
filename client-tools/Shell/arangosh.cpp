////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
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
///
/// @author Dr. Frank Celler
////////////////////////////////////////////////////////////////////////////////

#include "Basics/signals.h"
#include "Basics/directories.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "ApplicationFeatures/CommunicationFeaturePhase.h"
#include "ApplicationFeatures/ConfigFeature.h"
#include "ApplicationFeatures/FileSystemFeature.h"
#include "ApplicationFeatures/GreetingsFeaturePhase.h"
#include "ApplicationFeatures/ProcessEnvironmentFeature.h"
#include "ApplicationFeatures/LanguageFeature.h"
#include "ApplicationFeatures/OptionsCheckFeature.h"
#include "ApplicationFeatures/ShellColorsFeature.h"
#include "ApplicationFeatures/ShutdownFeature.h"
#include "ApplicationFeatures/TempFeature.h"
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
#include "Shell/ShellConsoleFeature.h"
#include "Shell/ShellFeature.h"
#include "Shell/V8ShellFeature.h"
#include "Shell/ProcessMonitoringFeature.h"
#include "Ssl/SslFeature.h"
#include "V8/V8PlatformFeature.h"
#include "V8/V8SecurityFeature.h"

#ifdef USE_ENTERPRISE
#include "Enterprise/Encryption/EncryptionFeature.h"
#endif

using namespace arangodb;
using namespace arangodb::application_features;

int main(int argc, char* argv[]) {
  TRI_GET_ARGV(argc, argv);
  return ClientFeature::runMain(argc, argv, [&](int argc, char* argv[]) -> int {
    int ret{EXIT_SUCCESS};

    ArangoGlobalContext context(argc, argv, BIN_DIRECTORY);
    arangodb::signals::maskAllSignalsClient();
    context.installHup();

    std::shared_ptr<options::ProgramOptions> options(
        new options::ProgramOptions(
            argv[0], "Usage: " + context.binaryName() + " [<options>]",
            "For more information use:", BIN_DIRECTORY,
            arangodb::options::ParseJsOps::parseJS));
    application_features::ApplicationServer server(options, BIN_DIRECTORY);

    // Add features in order (based on ArangoshFeaturesList)
    // Phases first
    server.addFeature<BasicFeaturePhaseClient>();
    server.addFeature<CommunicationFeaturePhase>();
    server.addFeature<GreetingsFeaturePhase>(std::true_type{});
    // Features
    server.addFeature<VersionFeature>();
#ifdef USE_ENTERPRISE
    server.addFeature<EncryptionFeature>();
#endif
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
    server.addFeature<ProcessEnvironmentFeature>(context.binaryName());
#endif
    server.addFeature<ShellConsoleFeature>();
    server.addFeature<HttpEndpointProvider, ClientFeature>(true);
    server.addFeature<ConfigFeature>(context.binaryName());
    server.addFeature<LoggerFeature>(false);
    server.addFeature<OptionsCheckFeature>();
    server.addFeature<FileSystemFeature>();
    server.addFeature<RandomFeature>();
    server.addFeature<ShellColorsFeature>();
    server.addFeature<ShutdownFeature>(
        std::array{std::type_index(typeid(ShellFeature))});
    server.addFeature<SslFeature>();
    server.addFeature<V8ShellFeaturePhase>();
    server.addFeature<ShellFeature>(&ret);
    server.addFeature<V8PlatformFeature>();

    auto& v8ShellFeature =
        server.addFeature<V8ShellFeature>(context.binaryName());
    server.addFeature<LanguageFeature>();
    server.addFeature<V8SecurityFeature>();
    server.addFeature<ProcessMonitoringFeature>(v8ShellFeature);
    server.addFeature<TempFeature>(context.binaryName());

    try {
      server.run(argc, argv);
      if (server.helpShown()) {
        // --help was displayed
        ret = EXIT_SUCCESS;
      }
    } catch (std::exception const& ex) {
      LOG_TOPIC("da777", ERR, arangodb::Logger::FIXME)
          << "arangosh terminated because of an unhandled exception: "
          << ex.what();
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
