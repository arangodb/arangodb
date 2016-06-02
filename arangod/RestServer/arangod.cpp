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
#ifdef _WIN32
#include "ApplicationFeatures/WindowsServiceFeature.h"
#endif
#include "ApplicationFeatures/ConfigFeature.h"
#include "ApplicationFeatures/DaemonFeature.h"
#include "ApplicationFeatures/LanguageFeature.h"
#include "ApplicationFeatures/NonceFeature.h"
#include "ApplicationFeatures/PrivilegeFeature.h"
#include "ApplicationFeatures/ShutdownFeature.h"
#include "ApplicationFeatures/SupervisorFeature.h"
#include "ApplicationFeatures/TempFeature.h"
#include "ApplicationFeatures/V8PlatformFeature.h"
#include "ApplicationFeatures/VersionFeature.h"
#include "ApplicationFeatures/WorkMonitorFeature.h"
#include "Basics/ArangoGlobalContext.h"
#include "Cluster/ClusterFeature.h"
#include "Dispatcher/DispatcherFeature.h"
#include "Logger/LoggerBufferFeature.h"
#include "Logger/LoggerFeature.h"
#include "ProgramOptions/ProgramOptions.h"
#include "Random/RandomFeature.h"
#include "RestServer/AffinityFeature.h"
#include "RestServer/BootstrapFeature.h"
#include "RestServer/CheckVersionFeature.h"
#include "RestServer/ConsoleFeature.h"
#include "RestServer/DatabaseFeature.h"
#include "RestServer/DatabaseServerFeature.h"
#include "RestServer/EndpointFeature.h"
#include "RestServer/FileDescriptorsFeature.h"
#include "RestServer/FrontendFeature.h"
#include "RestServer/QueryRegistryFeature.h"
#include "RestServer/RestServerFeature.h"
#include "RestServer/ScriptFeature.h"
#include "RestServer/ServerFeature.h"
#include "RestServer/UnitTestsFeature.h"
#include "RestServer/UpgradeFeature.h"
#include "Scheduler/SchedulerFeature.h"
#include "Ssl/SslFeature.h"
#include "Ssl/SslServerFeature.h"
#include "Statistics/StatisticsFeature.h"
#include "V8Server/FoxxQueuesFeature.h"
#include "V8Server/V8DealerFeature.h"
#include "Wal/LogfileManager.h"
#include "Wal/RecoveryFeature.h"

#ifdef ARANGODB_ENABLE_ROCKSDB
#include "Indexes/RocksDBFeature.h"
#endif

using namespace arangodb;
using namespace arangodb::wal;

static int runServer(int argc, char** argv) {
  ArangoGlobalContext context(argc, argv);
  context.installSegv();
  context.maskAllSignals();
  context.runStartupChecks();

  std::string name = context.binaryName();

  auto options = std::make_shared<options::ProgramOptions>(
      argv[0], "Usage: " + name + " [<options>]", "For more information use:");

  application_features::ApplicationServer server(options);

  std::vector<std::string> nonServerFeatures = {
      "Action",     "Affinity",   "Agency",
      "Cluster",    "Daemon",     "Dispatcher",
      "Endpoint",   "FoxxQueues", "LoggerBufferFeature",
      "RestServer", "Server",     "Scheduler",
      "SslServer",  "Statistics", "Supervisor"};

  int ret = EXIT_FAILURE;
  
#ifdef _WIN32
  server.addFeature(new WindowsServiceFeature(&server));
#endif
  
  server.addFeature(new ActionFeature(&server));
  server.addFeature(new AffinityFeature(&server));
  server.addFeature(new AgencyFeature(&server));
  server.addFeature(new BootstrapFeature(&server));
  server.addFeature(new CheckVersionFeature(&server, &ret, nonServerFeatures));
  server.addFeature(new ClusterFeature(&server));
  server.addFeature(new ConfigFeature(&server, name));
  server.addFeature(new ConsoleFeature(&server));
  server.addFeature(new DatabaseFeature(&server));
  server.addFeature(new DatabaseServerFeature(&server));
  server.addFeature(new DispatcherFeature(&server));
  server.addFeature(new EndpointFeature(&server));
  server.addFeature(new FileDescriptorsFeature(&server));
  server.addFeature(new FoxxQueuesFeature(&server));
  server.addFeature(new FrontendFeature(&server));
  server.addFeature(new LanguageFeature(&server));
  server.addFeature(new LogfileManager(&server));
  server.addFeature(new LoggerBufferFeature(&server));
  server.addFeature(new LoggerFeature(&server, true));
  server.addFeature(new NonceFeature(&server));
  server.addFeature(new PrivilegeFeature(&server));
  server.addFeature(new QueryRegistryFeature(&server));
  server.addFeature(new RandomFeature(&server));
  server.addFeature(new RecoveryFeature(&server));
  server.addFeature(new RestServerFeature(&server));
  server.addFeature(new SchedulerFeature(&server));
  server.addFeature(new ScriptFeature(&server, &ret));
  server.addFeature(new ServerFeature(&server, &ret));
  server.addFeature(new ShutdownFeature(&server, {"UnitTests", "Script"}));
  server.addFeature(new SslFeature(&server));
  server.addFeature(new SslServerFeature(&server));
  server.addFeature(new StatisticsFeature(&server));
  server.addFeature(new TempFeature(&server, name));
  server.addFeature(new UnitTestsFeature(&server, &ret));
  server.addFeature(new UpgradeFeature(&server, &ret, nonServerFeatures));
  server.addFeature(new V8DealerFeature(&server));
  server.addFeature(new V8PlatformFeature(&server));
  server.addFeature(new VersionFeature(&server));
  server.addFeature(new WorkMonitorFeature(&server));

#ifdef ARANGODB_ENABLE_ROCKSDB
  server.addFeature(new RocksDBFeature(&server));
#endif

#ifdef ARANGODB_HAVE_FORK
  server.addFeature(new DaemonFeature(&server));

  std::unique_ptr<SupervisorFeature> supervisor =
      std::make_unique<SupervisorFeature>(&server);
  supervisor->supervisorStart({"Logger"});
  server.addFeature(supervisor.release());
#endif

  try {
    server.run(argc, argv);
  } catch (std::exception const& ex) {
    LOG(ERR) << "arangod terminated because of an unhandled exception: "
             << ex.what();
    ret = EXIT_FAILURE;
  } catch (...) {
    LOG(ERR) << "arangod terminated because of an unhandled exception of "
                "unknown type";
    ret = EXIT_FAILURE;
  }

  return context.exit(ret);
}

int main(int argc, char* argv[]) {

  return runServer(argc, argv);
}
