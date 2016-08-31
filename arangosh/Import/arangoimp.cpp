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

#include "ApplicationFeatures/ConfigFeature.h"
#include "ApplicationFeatures/GreetingsFeature.h"
#include "ApplicationFeatures/ShutdownFeature.h"
#include "ApplicationFeatures/TempFeature.h"
#include "ApplicationFeatures/VersionFeature.h"
#include "Basics/ArangoGlobalContext.h"
#include "Import/ImportFeature.h"
#include "Logger/LoggerFeature.h"
#include "ProgramOptions/ProgramOptions.h"
#include "Random/RandomFeature.h"
#include "Shell/ClientFeature.h"
#include "Ssl/SslFeature.h"

using namespace arangodb;
using namespace arangodb::application_features;

int main(int argc, char* argv[]) {
  ArangoGlobalContext context(argc, argv, BIN_DIRECTORY);
  context.installHup();

  std::shared_ptr<options::ProgramOptions> options(new options::ProgramOptions(
      argv[0], "Usage: arangoimp [<options>]", "For more information use:"));

  ApplicationServer server(options);

  int ret;

  server.addFeature(new ClientFeature(&server));
  server.addFeature(new ConfigFeature(&server, "arangoimp"));
  server.addFeature(new GreetingsFeature(&server, "arangoimp"));
  server.addFeature(new ImportFeature(&server, &ret));
  server.addFeature(new LoggerFeature(&server, false));
  server.addFeature(new RandomFeature(&server));
  server.addFeature(new ShutdownFeature(&server, {"Import"}));
  server.addFeature(new SslFeature(&server));
  server.addFeature(new TempFeature(&server, "arangoimp"));
  server.addFeature(new VersionFeature(&server));

  try {
    server.run(argc, argv);
    if (server.helpShown()) {
      // --help was displayed
      ret = EXIT_SUCCESS;
    }
  } catch (std::exception const& ex) {
    LOG(ERR) << "arangoimp terminated because of an unhandled exception: "
             << ex.what();
    ret = EXIT_FAILURE;
  } catch (...) {
    LOG(ERR) << "arangoimp terminated because of an unhandled exception of "
                "unknown type";
    ret = EXIT_FAILURE;
  }

  return context.exit(ret);
}
