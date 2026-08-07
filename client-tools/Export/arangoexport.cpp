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
////////////////////////////////////////////////////////////////////////////////

#include "Basics/ArangoGlobalContext.h"
#include "Basics/directories.h"
#include "Basics/signals.h"
#include "Export/ArangoExportServer.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"
#include "ProgramOptions/ProgramOptions.h"
#include "Shell/ClientFeature.h"

using namespace arangodb;

int main(int argc, char* argv[]) {
  TRI_GET_ARGV(argc, argv);
  return ClientFeature::runMain(argc, argv, [&](int argc, char* argv[]) -> int {
    ArangoGlobalContext context(argc, argv, BIN_DIRECTORY);
    arangodb::signals::maskAllSignalsClient();
    context.installHup();

    auto options = std::make_shared<options::ProgramOptions>(
        argv[0], "Usage: arangoexport [<options>]",
        "For more information use:", BIN_DIRECTORY);

    int ret = EXIT_SUCCESS;
    ArangoExportServer server(options, BIN_DIRECTORY, context.binaryName(),
                              &ret);
    server.addFeatures();

    try {
      server.run(argc, argv);
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
