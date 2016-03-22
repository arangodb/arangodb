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

#include "ApplicationFeatures/ClientFeature.h"
#include "ApplicationFeatures/ConfigFeature.h"
#include "ApplicationFeatures/LoggerFeature.h"
#include "ApplicationFeatures/RandomFeature.h"
#include "ApplicationFeatures/ShutdownFeature.h"
#include "ApplicationFeatures/TempFeature.h"
#include "Basics/ArangoGlobalContext.h"
#include "Restore/RestoreFeature.h"
#include "ProgramOptions2/ProgramOptions.h"

using namespace arangodb;
using namespace arangodb::application_features;

int main(int argc, char* argv[]) {
  ArangoGlobalContext context(argc, argv);
  context.installHup();

  std::shared_ptr<options::ProgramOptions> options(new options::ProgramOptions(
      argv[0], "Usage: arangorestore [<options>]", "For more information use:"));

  ApplicationServer server(options);

  int ret;

  server.addFeature(new ClientFeature(&server));
  server.addFeature(new ConfigFeature(&server, "arangorestore"));
  server.addFeature(new LoggerFeature(&server));
  server.addFeature(new RandomFeature(&server));
  server.addFeature(new RestoreFeature(&server, &ret));
  server.addFeature(new ShutdownFeature(&server, "Restore"));
  server.addFeature(new TempFeature(&server, "arangorestore"));

  server.run(argc, argv);

  return context.exit(ret);
}
