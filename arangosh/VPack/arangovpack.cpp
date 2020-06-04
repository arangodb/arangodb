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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "Basics/Common.h"
#include "Basics/directories.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "ApplicationFeatures/ConfigFeature.h"
#include "ApplicationFeatures/GreetingsFeaturePhase.h"
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
    context.installHup();

    std::shared_ptr<options::ProgramOptions> options(
        new options::ProgramOptions(argv[0], "Usage: arangovpack [<options>]",
                                    "For more information use:", BIN_DIRECTORY));
    ApplicationServer server(options, BIN_DIRECTORY);
    int ret;

    server.addFeature<BasicFeaturePhaseClient>();
    server.addFeature<GreetingsFeaturePhase>(true);

    // default is to use no config file
    server.addFeature<ConfigFeature>("arangovpack", "none");
    server.addFeature<LoggerFeature>(false);
    server.addFeature<RandomFeature>();
    server.addFeature<ShellColorsFeature>();
    server.addFeature<ShutdownFeature>(
        std::vector<std::type_index>{std::type_index(typeid(VPackFeature))});
    server.addFeature<VPackFeature>(&ret);
    server.addFeature<VersionFeature>();

    try {
      server.run(argc, argv);
      if (server.helpShown()) {
        // --help was displayed
        ret = EXIT_SUCCESS;
      }
    } catch (std::exception const& ex) {
      LOG_TOPIC("f8d39", ERR, arangodb::Logger::FIXME)
          << "arangovpack terminated because of an unhandled exception: " << ex.what();
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
