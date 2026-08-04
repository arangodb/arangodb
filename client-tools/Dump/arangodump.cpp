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
#include "Dump/ArangoDumpServer.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"
#include "Maskings/AttributeMasking.h"
#include "ProgramOptions/ProgramOptions.h"
#include "Shell/ClientFeature.h"

#ifdef USE_ENTERPRISE
#include "Enterprise/Maskings/AttributeMaskingEE.h"
#endif

using namespace arangodb;

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

    auto options = std::make_shared<options::ProgramOptions>(
        argv[0], "Usage: arangodump [<options>]",
        "For more information use:", BIN_DIRECTORY);

    ArangoDumpServer server(options, BIN_DIRECTORY, context.binaryName(), &ret);
    server.addFeatures();

    try {
      server.run(argc, argv);
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
