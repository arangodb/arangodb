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

#include "Actions/ActionFeature.h"
#include "Agency/AgencyFeature.h"
#include "ApplicationFeatures/ConfigFeature.h"
#include "ApplicationFeatures/DaemonFeature.h"
#include "ApplicationFeatures/LanguageFeature.h"
#include "ApplicationFeatures/NonceFeature.h"
#include "ApplicationFeatures/ShutdownFeature.h"
#include "ApplicationFeatures/SslFeature.h"
#include "ApplicationFeatures/SupervisorFeature.h"
#include "ApplicationFeatures/TempFeature.h"
#include "ApplicationFeatures/V8PlatformFeature.h"
#include "ApplicationFeatures/WorkMonitorFeature.h"
#include "Basics/ArangoGlobalContext.h"
#include "Cluster/ClusterFeature.h"
#include "Dispatcher/DispatcherFeature.h"
#include "Logger/LoggerBufferFeature.h"
#include "Logger/LoggerFeature.h"
#include "ProgramOptions/ProgramOptions.h"
#include "Random/RandomFeature.h"
#include "RestServer/AffinityFeature.h"
#include "RestServer/CheckVersionFeature.h"
#include "RestServer/ConsoleFeature.h"
#include "RestServer/DatabaseFeature.h"
#include "RestServer/EndpointFeature.h"
#include "RestServer/FileDescriptorsFeature.h"
#include "RestServer/FrontendFeature.h"
#include "RestServer/RestServerFeature.h"
#include "RestServer/ServerFeature.h"
#include "RestServer/UpgradeFeature.h"
#include "Scheduler/SchedulerFeature.h"
#include "Statistics/StatisticsFeature.h"
#include "V8Server/V8DealerFeature.h"

using namespace arangodb;

////////////////////////////////////////////////////////////////////////////////
/// @brief Hooks for OS-Specific functions
////////////////////////////////////////////////////////////////////////////////

#warning TODO
#if 0
#ifdef _WIN32
extern bool TRI_ParseMoreArgs(int argc, char* argv[]);
extern void TRI_StartService(int argc, char* argv[]);
#else
bool TRI_ParseMoreArgs(int argc, char* argv[]) { return false; }
void TRI_StartService(int argc, char* argv[]) {}
#endif
#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief creates an application server
////////////////////////////////////////////////////////////////////////////////

int main(int argc, char* argv[]) {
  ArangoGlobalContext context(argc, argv);
  context.installSegv();
  context.maskAllSignals();
  context.runStartupChecks();

  std::string name = context.binaryName();

  std::shared_ptr<options::ProgramOptions> options(new options::ProgramOptions(
      argv[0], "Usage: " + name + " [<options>]", "For more information use:"));

  application_features::ApplicationServer server(options);

  std::vector<std::string> nonServerFeatures = {
      "Action", "Agency", "Cluster", "Daemon", "Dispatcher", "Endpoint",
      "LoggerBufferFeature", "Server", "Scheduler", "Ssl", "Supervisor"};

  int ret = EXIT_FAILURE;

  server.addFeature(new ActionFeature(&server));
  server.addFeature(new AffinityFeature(&server));
  server.addFeature(new AgencyFeature(&server));
  server.addFeature(new CheckVersionFeature(&server, &ret, nonServerFeatures));
  server.addFeature(new ClusterFeature(&server));
  server.addFeature(new ConfigFeature(&server, name));
  server.addFeature(new ConsoleFeature(&server));
  server.addFeature(new DatabaseFeature(&server));
  server.addFeature(new DispatcherFeature(&server));
  server.addFeature(new EndpointFeature(&server));
  server.addFeature(new FileDescriptorsFeature(&server));
  server.addFeature(new FrontendFeature(&server));
  server.addFeature(new LanguageFeature(&server));
  server.addFeature(new LoggerBufferFeature(&server));
  server.addFeature(new LoggerFeature(&server, true));
  server.addFeature(new NonceFeature(&server));
  server.addFeature(new RandomFeature(&server));
  server.addFeature(new RestServerFeature(&server, "arangodb"));
  server.addFeature(new SchedulerFeature(&server));
  server.addFeature(new ServerFeature(&server, &ret));
  server.addFeature(new ShutdownFeature(&server, "Server"));
  server.addFeature(new SslFeature(&server));
  server.addFeature(new StatisticsFeature(&server));
  server.addFeature(new TempFeature(&server, name));
  server.addFeature(new UpgradeFeature(&server, &ret, nonServerFeatures));
  server.addFeature(new V8DealerFeature(&server));
  server.addFeature(new V8PlatformFeature(&server));
  server.addFeature(new WorkMonitorFeature(&server));

#ifdef ARANGODB_HAVE_FORK
  server.addFeature(new DaemonFeature(&server));

  std::unique_ptr<SupervisorFeature> supervisor =
      std::make_unique<SupervisorFeature>(&server);
  supervisor->supervisorStart({"Logger"});
  server.addFeature(supervisor.release());
#endif

  server.run(argc, argv);

  return context.exit(ret);
}

#warning TODO
#if 0

  // windows only
  bool const startAsService = TRI_ParseMoreArgs(argc, argv);

  // initialize sub-systems
  if (startAsService) {
    TRI_StartService(argc, argv);
  } else {
    ArangoInstance = new ArangoServer(argc, argv);
    res = ArangoInstance->start();
  }

  if (ArangoInstance != nullptr) {
    try {
      delete ArangoInstance;
    } catch (...) {
      // caught an error during shutdown
      res = EXIT_FAILURE;

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
      std::cerr << "Caught an exception during shutdown" << std::endl;
#endif
    }

    ArangoInstance = nullptr;
  }

  // shutdown sub-systems
  return context.exit(ret);
#endif
