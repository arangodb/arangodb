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

#include "arangovpack.h"

#include "Basics/signals.h"
#include "Basics/directories.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "ApplicationFeatures/ConfigFeature.h"
#include "ApplicationFeatures/FileSystemFeature.h"
#include "ApplicationFeatures/GreetingsFeaturePhase.h"
#include "ApplicationFeatures/OptionsCheckFeature.h"
#include "ApplicationFeatures/ShellColorsFeature.h"
#include "ApplicationFeatures/ShutdownFeature.h"
#include "ApplicationFeatures/VersionFeature.h"
#include "Basics/ArangoGlobalContext.h"
#include "FeaturePhases/BasicFeaturePhaseClient.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerFeature.h"
#include "Logger/LoggerStream.h"
#include "ProgramOptions/ProgramOptions.h"
#include "Random/RandomFeature.h"
#include "Shell/ClientFeature.h"
#include "VPack/VPackFeature.h"

using namespace arangodb;
using namespace arangodb::application_features;

int main(int argc, char* argv[]) {
  TRI_GET_ARGV(argc, argv);
  return ClientFeature::runMain(argc, argv, [&](int argc, char* argv[]) -> int {
    ArangoGlobalContext context(argc, argv, BIN_DIRECTORY);
    arangodb::signals::maskAllSignalsClient();
    context.installHup();

    std::shared_ptr<options::ProgramOptions> options(
        new options::ProgramOptions(
            argv[0], "Usage: arangovpack [<options>]",
            "For more information use:", BIN_DIRECTORY));

    int ret = EXIT_SUCCESS;
    ArangoVPackServer server(options, BIN_DIRECTORY);

    server.addFeatures(Visitor{
        []<typename T>(auto& server, TypeTag<T>) {
          return std::make_unique<T>(server);
        },
        [&](ArangoVPackServer& server, TypeTag<VPackFeature>) {
          return std::make_unique<VPackFeature>(server, &ret);
        },
        [&](ArangoVPackServer& server, TypeTag<ConfigFeature>) {
          // default is to use no config file
          return std::make_unique<ConfigFeature>(server, context.binaryName(),
                                                 "none");
        },
        [](ArangoVPackServer& server, TypeTag<ShutdownFeature>) {
          return std::make_unique<ShutdownFeature>(
              server, std::array{ArangoVPackServer::id<VPackFeature>()});
        },
        [](ArangoVPackServer& server, TypeTag<GreetingsFeaturePhase>) {
          return std::make_unique<GreetingsFeaturePhase>(server,
                                                         std::true_type{});
        },
        [](ArangoVPackServer& server, TypeTag<LoggerFeature>) {
          return std::make_unique<LoggerFeature>(server, false);
        }});

    try {
      server.run(argc, argv);
      if (server.helpShown()) {
        // --help was displayed
        ret = EXIT_SUCCESS;
      }
    } catch (std::exception const& ex) {
      LOG_TOPIC("f8d39", ERR, arangodb::Logger::FIXME)
          << "arangovpack terminated because of an unhandled exception: "
          << ex.what();
      ret = EXIT_FAILURE;
    } catch (...) {
      LOG_TOPIC("785f7", ERR, arangodb::Logger::FIXME)
          << "arangovpack terminated because of an unhandled exception of "
             "unknown type";
      ret = EXIT_FAILURE;
    }

    return context.exit(ret);
  });
}
