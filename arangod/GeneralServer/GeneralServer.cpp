////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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
/// @author Achim Brandt
////////////////////////////////////////////////////////////////////////////////

#include "GeneralServer.h"

#include "Agency/AgencyFeature.h"
#include "Agency/Agent.h"
#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/NumberOfCores.h"
#include "Basics/PhysicalMemory.h"
#include "Basics/StaticStrings.h"
#include "Basics/StringBuffer.h"
#include "Basics/VPackStringBufferAdapter.h"
#include "Basics/application-exit.h"
#include "Basics/exitcodes.h"
#include "Basics/hashes.h"
#include "Cluster/ServerState.h"
#include "Endpoint/Endpoint.h"
#include "Endpoint/EndpointList.h"
#include "GeneralServer/Acceptor.h"
#include "GeneralServer/CommTask.h"
#include "GeneralServer/GeneralServerFeature.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"
#include "Rest/Version.h"
#include "RestServer/ServerIdFeature.h"
#include "Scheduler/Scheduler.h"
#include "Scheduler/SchedulerFeature.h"
#include "VocBase/Identifiers/ServerId.h"
#include "VocBase/vocbase.h"

#include <chrono>
#include <thread>
#include <algorithm>

#include <velocypack/Collection.h>
#include <velocypack/Dumper.h>
#include <velocypack/Iterator.h>
#include <velocypack/StringRef.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::rest;

namespace {
class InfoThread : virtual public Thread {
 public:
  explicit InfoThread(application_features::ApplicationServer& server)
    : Thread(server, "Info"), _count(0), _licenseHash(0),
      _agencyEnabled(false), _agencySize(0), _coordinators(0), _dbservers(0) {}
  ~InfoThread() { shutdown(); }

  void run() override {
    char const* licenseKey = getenv("ARANGO_LICENSE_KEY");

    if (licenseKey != nullptr) {
      _licenseHash = TRI_FnvHashString(licenseKey);
    }

    LOG_TOPIC("29daa", ERR, Logger::FIXME)
      << "INFO STARTED";

    while (!_server.isStopping()) {
      if (_count % agencyPeriod == 0) {
        readAgency();
      }

      if (_count % logPeriod == 1) {
	logInfo();
      }
      std::this_thread::sleep_for(std::chrono::seconds(10));

      ++_count;
    }
  }

private:
  static constexpr uint64_t logPeriod = 6;
  static constexpr uint64_t agencyPeriod = 5;

  static constexpr char const* CORES = "cores";
  static constexpr char const* DEPLOYMENT = "deployment";
  static constexpr char const* EDITION = "edition";
  static constexpr char const* MEMORY = "memory";
  static constexpr char const* LICENSE = "license";
  static constexpr char const* OS = "os";
  static constexpr char const* SERVER_ID = "serverId";
  static constexpr char const* VERSION = "version";

  static constexpr char const* AGENCY_SIZE = "agency";
  static constexpr char const* NUM_COORDINATORS = "coordinators";
  static constexpr char const* NUM_DBSERVERS = "dbservers";

  static constexpr char const* CLUSTER = "cluster";
  static constexpr char const* COMMUNITY = "community";
  static constexpr char const* ENTERPRISE = "enterprise";
  static constexpr char const* SINGLE = "single";

  void readAgency() {
    static std::string const PLAN_COORDINATORS = "/arango/Plan/Coordinators";
    static std::string const PLAN_DBSERVERS = "/arango/Plan/DBservers";

    if (!ServerState::instance()->isAgent()) {
      return;
    }

    AgencyFeature& agency = _server.getFeature<AgencyFeature>();

    if (agency.activated()) {
      consensus::Agent* agent = agency.agent();

      if (agent != nullptr) {
	_agencyEnabled = true;
        _agencySize = agent->size();

	agent->executeLockedRead([&]() {
				   LOG_TOPIC("29daa", ERR, Logger::FIXME) << "TEST 1";

				   if (agent->readDB().has("/")) {
				     LOG_TOPIC("29daa", ERR, Logger::FIXME) << "TEST 6";
				   }

				   if (agent->readDB().has("/arango")) {
				     LOG_TOPIC("29daa", ERR, Logger::FIXME) << "TEST 5";
				   }

				   if (agent->readDB().has("/agency")) {
				     LOG_TOPIC("29daa", ERR, Logger::FIXME) << "TEST 7";
				   }

				   if (agent->readDB().has("/arango/Plan")) {
				     LOG_TOPIC("29daa", ERR, Logger::FIXME) << "TEST 4";
				   }

				   if (agent->readDB().has(PLAN_COORDINATORS)) {
				     LOG_TOPIC("29daa", ERR, Logger::FIXME) << "TEST 2";
				     auto const pn = agent->readDB().get(PLAN_COORDINATORS);
				     _coordinators = pn.children().size();
				   } 

				   if (agent->readDB().has(PLAN_DBSERVERS)) {
				     LOG_TOPIC("29daa", ERR, Logger::FIXME) << "TEST 3";
				     auto const pn = agent->readDB().get(PLAN_DBSERVERS);
				     _dbservers = pn.children().size();
				   } 
				 });
      }
    }
  }

  void logInfo() {
    if (ServerState::instance()->isSingleServer()) {
      logInfoSingleServer();
    } else if (ServerState::instance()->isAgent()) {
      logInfoAgency();
    } else {
      LOG_TOPIC("29daa", ERR, Logger::FIXME)
	<< "TEST";
    }
  }

  void logInfoSingleServer() {
    VPackBuilder builder;
    {
      VPackObjectBuilder ob(&builder);

      builder.add(VERSION, VPackValue(Version::getNumericServerVersion()));
#ifdef USE_ENTERPRISE
      builder.add(EDITION, VPackValue(ENTERPRISE));
#else
      builder.add(EDITION, VPackValue(COMMUNITY));
#endif

      builder.add(DEPLOYMENT, VPackValue(SINGLE));

      auto& feature = server().getFeature<ServerIdFeature>();
      builder.add(SERVER_ID, VPackValue(feature.getId().id()));

      builder.add(MEMORY, VPackValue(PhysicalMemory::getValue())); 
      builder.add(CORES, VPackValue(NumberOfCores::getValue())); 
      builder.add(OS, VPackValue(TRI_PLATFORM));

      if (_licenseHash != 0) {
        builder.add(LICENSE, VPackValue(_licenseHash));
      }
    }

    output(builder);
  }

  void logInfoAgency() {
    VPackBuilder builder;
    {
      VPackObjectBuilder ob(&builder);

      builder.add(VERSION, VPackValue(Version::getNumericServerVersion()));
#ifdef USE_ENTERPRISE
      builder.add(EDITION, VPackValue(ENTERPRISE));
#else
      builder.add(EDITION, VPackValue(COMMUNITY));
#endif

      builder.add(DEPLOYMENT, VPackValue(CLUSTER));

      auto& feature = server().getFeature<ServerIdFeature>();
      builder.add(SERVER_ID, VPackValue(feature.getId().id()));

      builder.add(MEMORY, VPackValue(PhysicalMemory::getValue())); 
      builder.add(CORES, VPackValue(NumberOfCores::getValue())); 
      builder.add(OS, VPackValue(TRI_PLATFORM));

      if (_licenseHash != 0) {
        builder.add(LICENSE, VPackValue(_licenseHash)); 
      }

      if (_agencyEnabled) {
	builder.add(AGENCY_SIZE, VPackValue(_agencySize));
	builder.add(NUM_COORDINATORS, VPackValue(_coordinators));
	builder.add(NUM_DBSERVERS, VPackValue(_dbservers));
      }
    }

    output(builder);
  }

  void output(VPackBuilder const& builder) {
    StringBuffer output;
    VPackStringBufferAdapter buffer(output.stringBuffer());
    VPackOptions opts;
    VPackDumper dumper(&buffer, &opts);
    dumper.dump(builder.slice());
    std::string s(output.c_str(), output.length());

    LOG_TOPIC("29daa", ERR, Logger::FIXME)
      << "TEST " << s;
  }

private:
  uint64_t _count;
  uint64_t _licenseHash;
  bool _agencyEnabled;
  uint64_t _agencySize;
  std::atomic<uint64_t> _coordinators;
  std::atomic<uint64_t> _dbservers;
};
}

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------
GeneralServer::GeneralServer(GeneralServerFeature& feature, uint64_t numIoThreads)
    : _feature(feature), _endpointList(nullptr), _contexts(), _infoThread() {
  auto& server = feature.server();
  for (size_t i = 0; i < numIoThreads; ++i) {
    _contexts.emplace_back(server);
  }
}

GeneralServer::~GeneralServer() = default;

void GeneralServer::registerTask(std::shared_ptr<CommTask> task) {
  if (_feature.server().isStopping()) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_SHUTTING_DOWN);
  }
  auto* t = task.get();
  LOG_TOPIC("29da9", TRACE, Logger::REQUESTS)
      << "registering CommTask with ptr " << t;
  {
    auto* ptr = task.get();
    std::lock_guard<std::recursive_mutex> guard(_tasksLock);
    _commTasks.try_emplace(ptr, std::move(task));
  }
  t->start();
}

void GeneralServer::unregisterTask(CommTask* task) {
  LOG_TOPIC("090d8", TRACE, Logger::REQUESTS)
      << "unregistering CommTask with ptr " << task;
  std::shared_ptr<CommTask> old;
  {
    std::lock_guard<std::recursive_mutex> guard(_tasksLock);
    auto it = _commTasks.find(task);
    if (it != _commTasks.end()) {
      old = std::move(it->second);
      _commTasks.erase(it);
    }
  }
  old.reset();
}

void GeneralServer::setEndpointList(EndpointList const* list) {
  _endpointList = list;
}

void GeneralServer::startListening() {
  unsigned int i = 0;

  for (auto& it : _endpointList->allEndpoints()) {
    LOG_TOPIC("e62e0", TRACE, arangodb::Logger::FIXME)
        << "trying to bind to endpoint '" << it.first << "' for requests";

    // distribute endpoints across all io contexts
    IoContext& ioContext = _contexts[i++ % _contexts.size()];
    bool ok = openEndpoint(ioContext, it.second);

    if (ok) {
      LOG_TOPIC("dc45a", DEBUG, arangodb::Logger::FIXME)
          << "bound to endpoint '" << it.first << "'";
    } else {
      LOG_TOPIC("c81f6", FATAL, arangodb::Logger::FIXME)
          << "failed to bind to endpoint '" << it.first
          << "'. Please check whether another instance is already "
             "running using this endpoint and review your endpoints "
             "configuration.";
      FATAL_ERROR_EXIT_CODE(TRI_EXIT_COULD_NOT_BIND_PORT);
    }
  }

  _infoThread.reset(new InfoThread(server()));
  _infoThread->start();
}

/// stop accepting new connections
void GeneralServer::stopListening() {
  for (std::unique_ptr<Acceptor>& acceptor : _acceptors) {
    acceptor->close();
  }

  _infoThread.reset();
}

/// stop connections
void GeneralServer::stopConnections() {
  // close connections of all socket tasks so the tasks will
  // eventually shut themselves down
  std::lock_guard<std::recursive_mutex> guard(_tasksLock);
  for (auto const& pair : _commTasks) {
    pair.second->stop();
  }
}

void GeneralServer::stopWorking() {
  auto const started {std::chrono::system_clock::now()};
  constexpr auto timeout {std::chrono::seconds(5)};
  do {
    {
      std::unique_lock<std::recursive_mutex> guard(_tasksLock);
      if (_commTasks.empty())
        break;
    }
    std::this_thread::yield();
  } while((std::chrono::system_clock::now() - started) < timeout);
  {
    std::lock_guard<std::recursive_mutex> guard(_tasksLock);
    _commTasks.clear();
  }
  _acceptors.clear();
  _contexts.clear();  // stops threads
}

// -----------------------------------------------------------------------------
// --SECTION--                                                 protected methods
// -----------------------------------------------------------------------------

bool GeneralServer::openEndpoint(IoContext& ioContext, Endpoint* endpoint) {
  auto acceptor = rest::Acceptor::factory(*this, ioContext, endpoint);
  try {
    acceptor->open();
  } catch (...) {
    return false;
  }
  _acceptors.emplace_back(std::move(acceptor));
  return true;
}

IoContext& GeneralServer::selectIoContext() {
  return *std::min_element(_contexts.begin(), _contexts.end(), 
    [](auto const &a, auto const &b) { return a.clients() < b.clients(); });
}

#ifdef USE_ENTERPRISE
extern int clientHelloCallback(SSL* ssl, int* al, void* arg);
#endif

SslServerFeature::SslContextList GeneralServer::sslContexts() {
  std::lock_guard<std::mutex> guard(_sslContextMutex);
  if (!_sslContexts) {
    _sslContexts = server().getFeature<SslServerFeature>().createSslContexts();
#ifdef USE_ENTERPRISE
    if (_sslContexts->size() > 0) {
      // Set a client hello callback such that we have a chance to change the SSL context:
      SSL_CTX_set_client_hello_cb((*_sslContexts)[0].native_handle(), &clientHelloCallback, (void*) this);
    }
#endif
  }
  return _sslContexts;
}

SSL_CTX* GeneralServer::getSSL_CTX(size_t index) {
  std::lock_guard<std::mutex> guard(_sslContextMutex);
  return (*_sslContexts)[index].native_handle();
}

Result GeneralServer::reloadTLS() {
  try {
    {
      std::lock_guard<std::mutex> guard(_sslContextMutex);
      _sslContexts = server().getFeature<SslServerFeature>().createSslContexts();
#ifdef USE_ENTERPRISE
      if (_sslContexts->size() > 0) {
        // Set a client hello callback such that we have a chance to change the SSL context:
        SSL_CTX_set_client_hello_cb((*_sslContexts)[0].native_handle(), &clientHelloCallback, (void*) this);
      }
#endif
    }
    // Now cancel every acceptor once, such that a new AsioSocket is generated which will
    // use the new context. Otherwise, the first connection will still use the old certs:
    for (auto& a : _acceptors) {
      a->cancel();
    }
    return TRI_ERROR_NO_ERROR;
  } catch(std::exception& e) {
    LOG_TOPIC("feffe", ERR, Logger::SSL) << "Could not reload TLS context from files, got exception with this error: " << e.what();
    return Result(TRI_ERROR_CANNOT_READ_FILE, "Could not reload TLS context from files.");
  }
}

application_features::ApplicationServer& GeneralServer::server() const {
  return _feature.server();
}
