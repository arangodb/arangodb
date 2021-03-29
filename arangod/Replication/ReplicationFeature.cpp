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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "ReplicationFeature.h"
#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/StaticStrings.h"
#include "Basics/Thread.h"
#include "Basics/application-exit.h"
#include "Cluster/ClusterFeature.h"
#include "FeaturePhases/BasicFeaturePhaseServer.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"
#include "ProgramOptions/ProgramOptions.h"
#include "ProgramOptions/Section.h"
#include "Replication/DatabaseReplicationApplier.h"
#include "Replication/GlobalReplicationApplier.h"
#include "Replication/ReplicationApplierConfiguration.h"
#include "Rest/GeneralResponse.h"
#include "RestServer/DatabaseFeature.h"
#include "RestServer/MetricsFeature.h"
#include "RestServer/SystemDatabaseFeature.h"
#include "StorageEngine/StorageEngineFeature.h"
#include "VocBase/vocbase.h"

using namespace arangodb::application_features;
using namespace arangodb::options;

namespace {
// replace tcp:// with http://, and ssl:// with https://
std::string fixEndpointProto(std::string const& endpoint) {
  if (endpoint.compare(0, 6, "tcp://") == 0) {  //  find("tcp://", 0, 6)
    return "http://" + endpoint.substr(6);      // strlen("tcp://")
  }
  if (endpoint.compare(0, 6, "ssl://") == 0) {  // find("ssl://", 0, 6) == 0
    return "https://" + endpoint.substr(6);     // strlen("ssl://")
  }
  return endpoint;
}

void writeError(ErrorCode code, arangodb::GeneralResponse* response) {
  response->setResponseCode(arangodb::GeneralResponse::responseCode(code));

  VPackBuffer<uint8_t> buffer;
  VPackBuilder builder(buffer);
  builder.add(VPackValue(VPackValueType::Object));
  builder.add(arangodb::StaticStrings::Error, VPackValue(true));
  builder.add(arangodb::StaticStrings::ErrorNum, VPackValue(code));
  builder.add(arangodb::StaticStrings::ErrorMessage, VPackValue(TRI_errno_string(code)));
  builder.add(arangodb::StaticStrings::Code, VPackValue(static_cast<int>(response->responseCode())));
  builder.close();

  response->setPayload(std::move(buffer), VPackOptions::Defaults);
}
} // namespace


DECLARE_COUNTER(arangodb_replication_cluster_inventory_requests_total, "(DC-2-DC only) Number of times the database and collection overviews have been requested.");

namespace arangodb {

ReplicationFeature::ReplicationFeature(ApplicationServer& server)
    : ApplicationFeature(server, "Replication"),
      _connectTimeout(10.0),
      _requestTimeout(600.0),
      _forceConnectTimeout(false),
      _forceRequestTimeout(false),
      _replicationApplierAutoStart(true),
      _enableActiveFailover(false),
      _syncByRevision(true),
      _parallelTailingInvocations(0),
      _maxParallelTailingInvocations(0),
      _quickKeysLimit(1000000),
      _inventoryRequests(
        server.getFeature<arangodb::MetricsFeature>().add(arangodb_replication_cluster_inventory_requests_total{})) {
  setOptional(true);
  startsAfter<BasicFeaturePhaseServer>();

  startsAfter<DatabaseFeature>();
  startsAfter<StorageEngineFeature>();
  startsAfter<SystemDatabaseFeature>();
}

ReplicationFeature::~ReplicationFeature() = default;

void ReplicationFeature::collectOptions(std::shared_ptr<ProgramOptions> options) {
  options->addSection("replication", "Configure the replication");
  options->addOption("--replication.auto-start",
                     "switch to enable or disable the automatic start "
                     "of replication appliers",
                     new BooleanParameter(&_replicationApplierAutoStart),
                     arangodb::options::makeDefaultFlags(arangodb::options::Flags::Hidden));

  options->addSection("database", "Configure the database");
  options->addOldOption("server.disable-replication-applier",
                        "replication.auto-start");
  options->addOldOption("database.replication-applier",
                        "replication.auto-start");
  options->addOption("--replication.automatic-failover",
                     "Please use `--replication.active-failover` instead",
                     new BooleanParameter(&_enableActiveFailover),
                     arangodb::options::makeDefaultFlags(arangodb::options::Flags::Hidden));
  options->addOption("--replication.active-failover",
                     "Enable active-failover during asynchronous replication",
                     new BooleanParameter(&_enableActiveFailover));
  
  options->addOption("--replication.max-parallel-tailing-invocations",
                     "Maximum number of concurrently allowed WAL tailing invocations (0 = unlimited)",
                     new UInt64Parameter(&_maxParallelTailingInvocations),
                     arangodb::options::makeDefaultFlags(arangodb::options::Flags::Hidden))
                     .setIntroducedIn(30500);
  
  options->addOption("--replication.connect-timeout",
                     "Default timeout value for replication connection attempts (in seconds)",
                     new DoubleParameter(&_connectTimeout))
                     .setIntroducedIn(30409).setIntroducedIn(30504);
  options->addOption("--replication.request-timeout",
                     "Default timeout value for replication requests (in seconds)",
                     new DoubleParameter(&_requestTimeout))
                     .setIntroducedIn(30409).setIntroducedIn(30504);

  options->addOption("--replication.quick-keys-limit",
                     "Limit at which 'quick' calls to the replication keys API return only the document count for second run",
                     new UInt64Parameter(&_quickKeysLimit),
                     arangodb::options::makeDefaultFlags(arangodb::options::Flags::Hidden))
                     .setIntroducedIn(30709);

  options
      ->addOption(
          "--replication.sync-by-revision",
          "Whether to use the newer revision-based replication protocol",
          new BooleanParameter(&_syncByRevision))
      .setIntroducedIn(30700);
}

void ReplicationFeature::validateOptions(std::shared_ptr<options::ProgramOptions> options) {
  auto& feature = server().getFeature<ClusterFeature>();
  if (_enableActiveFailover && feature.agencyEndpoints().empty()) {
    LOG_TOPIC("68fcb", FATAL, arangodb::Logger::REPLICATION)
        << "automatic failover needs to be started with agency endpoint "
           "configured";
    FATAL_ERROR_EXIT();
  }

  if (_connectTimeout < 1.0) {
    _connectTimeout = 1.0;
  }
  if (options->processingResult().touched("--replication.connect-timeout")) {
    _forceConnectTimeout = true;
  }

  if (_requestTimeout < 3.0) {
    _requestTimeout = 3.0;
  }
  if (options->processingResult().touched("--replication.request-timeout")) {
    _forceRequestTimeout = true;
  }
}

void ReplicationFeature::prepare() {
  if (ServerState::instance()->isCoordinator()) {
    setEnabled(false);
    return;
  }
}

void ReplicationFeature::start() {
  _globalReplicationApplier.reset(new GlobalReplicationApplier(
      GlobalReplicationApplier::loadConfiguration(server())));

  try {
    _globalReplicationApplier->loadState();
  } catch (...) {
    // :snake:
  }

  LOG_TOPIC("1214b", DEBUG, Logger::REPLICATION)
      << "checking global applier startup. autoStart: "
      << _globalReplicationApplier->autoStart()
      << ", hasState: " << _globalReplicationApplier->hasState();

  if (_globalReplicationApplier->autoStart() &&
      _globalReplicationApplier->hasState() && _replicationApplierAutoStart) {
    _globalReplicationApplier->startTailing(/*initialTick*/0, /*useTick*/false);
  }
}

void ReplicationFeature::beginShutdown() {
  try {
    if (_globalReplicationApplier != nullptr) {
      _globalReplicationApplier->stop();
    }
  } catch (...) {
    // ignore any error
  }
}

void ReplicationFeature::stop() {
  try {
    if (_globalReplicationApplier != nullptr) {
      _globalReplicationApplier->stop();
    }
  } catch (...) {
    // ignore any error
  }
}

void ReplicationFeature::unprepare() {
  if (_globalReplicationApplier != nullptr) {
    _globalReplicationApplier->stopAndJoin();
  }
  _globalReplicationApplier.reset();
}
  
/// @brief track the number of (parallel) tailing operations
/// will throw an exception if the number of concurrently running operations
/// would exceed the configured maximum
void ReplicationFeature::trackTailingStart() {
  if (++_parallelTailingInvocations > _maxParallelTailingInvocations &&
      _maxParallelTailingInvocations > 0) {
    // we are above the configured maximum
    --_parallelTailingInvocations;
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_RESOURCE_LIMIT, "too many parallel invocations of WAL tailing operations");
  }
}

/// @brief count down the number of parallel tailing operations
/// must only be called after a successful call to trackTailingstart
void ReplicationFeature::trackTailingEnd() noexcept {
  --_parallelTailingInvocations;
}
  
double ReplicationFeature::checkConnectTimeout(double value) const {
  if (_forceConnectTimeout) {
    return _connectTimeout;
  }
  return value;
}

double ReplicationFeature::checkRequestTimeout(double value) const {
  if (_forceRequestTimeout) {
    return _requestTimeout;
  }
  return value;
}

bool ReplicationFeature::isActiveFailoverEnabled() const {
  return _enableActiveFailover;
}

bool ReplicationFeature::syncByRevision() const { return _syncByRevision; }

// start the replication applier for a single database
void ReplicationFeature::startApplier(TRI_vocbase_t* vocbase) {
  TRI_ASSERT(vocbase->type() == TRI_VOCBASE_TYPE_NORMAL);
  TRI_ASSERT(vocbase->replicationApplier() != nullptr);

  if (!ServerState::instance()->isClusterRole() &&
      vocbase->replicationApplier()->autoStart()) {
    if (!_replicationApplierAutoStart) {
      LOG_TOPIC("c5378", INFO, arangodb::Logger::REPLICATION)
          << "replication applier explicitly deactivated for database '"
          << vocbase->name() << "'";
    } else {
      try {
        vocbase->replicationApplier()->startTailing(/*initialTick*/0, /*useTick*/false);
      } catch (std::exception const& ex) {
        LOG_TOPIC("2038f", WARN, arangodb::Logger::REPLICATION)
            << "unable to start replication applier for database '"
            << vocbase->name() << "': " << ex.what();
      } catch (...) {
        LOG_TOPIC("76ad6", WARN, arangodb::Logger::REPLICATION)
            << "unable to start replication applier for database '"
            << vocbase->name() << "'";
      }
    }
  }
}

GlobalReplicationApplier* ReplicationFeature::globalReplicationApplier() const {
  TRI_ASSERT(_globalReplicationApplier != nullptr);
  return _globalReplicationApplier.get();
}

void ReplicationFeature::disableReplicationApplier() {
  _replicationApplierAutoStart = false;
}

// stop the replication applier for a single database
void ReplicationFeature::stopApplier(TRI_vocbase_t* vocbase) {
  TRI_ASSERT(vocbase->type() == TRI_VOCBASE_TYPE_NORMAL);

  if (!ServerState::instance()->isClusterRole() &&
      vocbase->replicationApplier() != nullptr) {
    vocbase->replicationApplier()->stopAndJoin();
  }
}

/// @brief returns the connect timeout for replication requests
double ReplicationFeature::connectTimeout() const { return _connectTimeout; }

/// @brief returns the request timeout for replication requests
double ReplicationFeature::requestTimeout() const { return _requestTimeout; }

/// @brief set the x-arango-endpoint header
void ReplicationFeature::setEndpointHeader(GeneralResponse* res,
                                           arangodb::ServerState::Mode mode) {
  std::string endpoint;
  if (isActiveFailoverEnabled()) {
    GlobalReplicationApplier* applier = globalReplicationApplier();
    if (applier != nullptr) {
      endpoint = applier->endpoint();
      // replace tcp:// with http://, and ssl:// with https://
      endpoint = ::fixEndpointProto(endpoint);
    }
  }
  res->setHeaderNC(StaticStrings::LeaderEndpoint, endpoint);
}

/// @brief fill a response object with correct response for a follower
void ReplicationFeature::prepareFollowerResponse(GeneralResponse* response,
                                                 arangodb::ServerState::Mode mode) {
  switch (mode) {
    case ServerState::Mode::REDIRECT: {
      setEndpointHeader(response, mode);
      ::writeError(TRI_ERROR_CLUSTER_NOT_LEADER, response);
      // return the endpoint of the actual leader
    } break;

    case ServerState::Mode::TRYAGAIN:
      // intentionally do not set "Location" header, but use a custom header
      // that clients can inspect. if they find an empty endpoint, it means that
      // there is an ongoing leadership challenge
      response->setHeaderNC(StaticStrings::LeaderEndpoint, "");
      ::writeError(TRI_ERROR_CLUSTER_LEADERSHIP_CHALLENGE_ONGOING, response);
      break;

    case ServerState::Mode::INVALID:
      ::writeError(TRI_ERROR_SHUTTING_DOWN, response);
      break;
    case ServerState::Mode::MAINTENANCE:
    default: {
      response->setResponseCode(rest::ResponseCode::SERVICE_UNAVAILABLE);
      break;
    }
  }
}

}  // namespace arangodb
