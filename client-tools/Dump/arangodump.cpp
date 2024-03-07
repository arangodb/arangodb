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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "arangodump.h"

#include "Basics/Common.h"
#include "Basics/signals.h"
#include "Basics/directories.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "ApplicationFeatures/BumpFileDescriptorsFeature.h"
#include "ApplicationFeatures/CommunicationFeaturePhase.h"
#include "ApplicationFeatures/ConfigFeature.h"
#include "ApplicationFeatures/FileSystemFeature.h"
#include "ApplicationFeatures/GreetingsFeaturePhase.h"
#include "ApplicationFeatures/OptionsCheckFeature.h"
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

    server.addFeatures(Visitor{
        []<typename T>(auto& server, TypeTag<T>) {
          return std::make_unique<T>(server);
        },
#ifdef TRI_HAVE_GETRLIMIT
        [](ArangoDumpServer& server, TypeTag<BumpFileDescriptorsFeature>) {
          return std::make_unique<BumpFileDescriptorsFeature>(
              server, "--descriptors-minimum");
        },
#endif
        [](ArangoDumpServer& server, TypeTag<GreetingsFeaturePhase>) {
          return std::make_unique<GreetingsFeaturePhase>(server,
                                                         std::true_type{});
        },
        [&context](ArangoDumpServer& server, TypeTag<ConfigFeature>) {
          return std::make_unique<ConfigFeature>(server, context.binaryName());
        },
        [](ArangoDumpServer& server, TypeTag<LoggerFeature>) {
          return std::make_unique<LoggerFeature>(server, false);
        },
        [](ArangoDumpServer& server, TypeTag<HttpEndpointProvider>) {
          return std::make_unique<ClientFeature>(
              server, true, std::numeric_limits<size_t>::max());
        },
        [&ret](ArangoDumpServer& server, TypeTag<DumpFeature>) {
          return std::make_unique<DumpFeature>(server, ret);
        },
        [](ArangoDumpServer& server, TypeTag<ShutdownFeature>) {
          return std::make_unique<ShutdownFeature>(
              server, std::array{ArangoDumpServer::id<DumpFeature>()});
        }});

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
