////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2017 ArangoDB GmbH, Cologne, Germany
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
#include "Agency/AgencyComm.h"
#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/Thread.h"
#include "Cluster/ClusterFeature.h"
#include "Logger/Logger.h"
#include "ProgramOptions/ProgramOptions.h"
#include "ProgramOptions/Section.h"
#include "Replication/DatabaseReplicationApplier.h"
#include "Replication/GlobalReplicationApplier.h"
#include "Replication/ReplicationApplierConfiguration.h"
#include "Rest/GeneralResponse.h"
#include "VocBase/vocbase.h"

using namespace arangodb::application_features;
using namespace arangodb::options;

namespace arangodb {

ReplicationFeature* ReplicationFeature::INSTANCE = nullptr;

ReplicationFeature::ReplicationFeature(ApplicationServer& server)
    : ApplicationFeature(server, "Replication"),
      _replicationApplierAutoStart(true),
      _enableActiveFailover(false) {

  setOptional(true);
  startsAfter("BasicsPhase");
  startsAfter("Database");
  startsAfter("StorageEngine");
  startsAfter("SystemDatabase");
}

void ReplicationFeature::collectOptions(std::shared_ptr<ProgramOptions> options) {
  options->addSection("replication", "Configure the replication");
  options->addOption("--replication.auto-start",
                     "switch to enable or disable the automatic start "
                     "of replication appliers",
                     new BooleanParameter(&_replicationApplierAutoStart),
                     arangodb::options::makeFlags(arangodb::options::Flags::Hidden));

  options->addSection("database", "Configure the database");
  options->addOldOption("server.disable-replication-applier",
                        "replication.auto-start");
  options->addOldOption("database.replication-applier",
                        "replication.auto-start");
  options->addOption("--replication.automatic-failover",
                     "Please use `--replication.active-failover` instead",
                     new BooleanParameter(&_enableActiveFailover),
                     arangodb::options::makeFlags(arangodb::options::Flags::Hidden));
  options->addOption("--replication.active-failover",
                      "Enable active-failover during asynchronous replication",
                      new BooleanParameter(&_enableActiveFailover));
}

void ReplicationFeature::validateOptions(std::shared_ptr<options::ProgramOptions> options) {
  auto feature = ApplicationServer::getFeature<ClusterFeature>("Cluster");
  if (_enableActiveFailover && feature->agencyEndpoints().empty()) {
    LOG_TOPIC(FATAL, arangodb::Logger::REPLICATION)
    << "automatic failover needs to be started with agency endpoint configured";
    FATAL_ERROR_EXIT();
  }
}

void ReplicationFeature::prepare() {
  if (ServerState::instance()->isCoordinator()) {
    setEnabled(false);
    return;
  }

  INSTANCE = this;
}

void ReplicationFeature::start() {
  _globalReplicationApplier.reset(new GlobalReplicationApplier(GlobalReplicationApplier::loadConfiguration()));

  try {
    _globalReplicationApplier->loadState();
  } catch (...) {
    // :snake:
  }

  LOG_TOPIC(DEBUG, Logger::REPLICATION) << "checking global applier startup. autoStart: " << _globalReplicationApplier->autoStart() << ", hasState: " << _globalReplicationApplier->hasState();

  if (_globalReplicationApplier->autoStart() &&
      _globalReplicationApplier->hasState() &&
      _replicationApplierAutoStart) {
    _globalReplicationApplier->startTailing(0, false, 0);
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
    _globalReplicationApplier->stop();
  }
  _globalReplicationApplier.reset();
}

// start the replication applier for a single database
void ReplicationFeature::startApplier(TRI_vocbase_t* vocbase) {
  TRI_ASSERT(vocbase->type() == TRI_VOCBASE_TYPE_NORMAL);
  TRI_ASSERT(vocbase->replicationApplier() != nullptr);

  if (vocbase->replicationApplier()->autoStart()) {
    if (!_replicationApplierAutoStart) {
      LOG_TOPIC(INFO, arangodb::Logger::REPLICATION)
          << "replication applier explicitly deactivated for database '"
          << vocbase->name() << "'";
    } else {
      try {
        vocbase->replicationApplier()->startTailing(0, false, 0);
      } catch (std::exception const& ex) {
        LOG_TOPIC(WARN, arangodb::Logger::REPLICATION)
            << "unable to start replication applier for database '"
            << vocbase->name() << "': " << ex.what();
      } catch (...) {
        LOG_TOPIC(WARN, arangodb::Logger::REPLICATION)
            << "unable to start replication applier for database '"
            << vocbase->name() << "'";
      }
    }
  }
}

// stop the replication applier for a single database
void ReplicationFeature::stopApplier(TRI_vocbase_t* vocbase) {
  TRI_ASSERT(vocbase->type() == TRI_VOCBASE_TYPE_NORMAL);

  if (vocbase->replicationApplier() != nullptr) {
    vocbase->replicationApplier()->stopAndJoin();
  }
}

// replace tcp:// with http://, and ssl:// with https://
static std::string FixEndpointProto(std::string const& endpoint) {
  if (endpoint.compare(0, 6, "tcp://") == 0) { //  find("tcp://", 0, 6)
    return "http://" + endpoint.substr(6); // strlen("tcp://")
  }
  if (endpoint.compare(0, 6, "ssl://") == 0) { // find("ssl://", 0, 6) == 0
    return "https://" + endpoint.substr(6); // strlen("ssl://")
  }
  return endpoint;
}

static void writeError(int code, GeneralResponse* response) {
  response->setResponseCode(GeneralResponse::responseCode(code));

  VPackBuffer<uint8_t> buffer;
  VPackBuilder builder(buffer);
  builder.add(VPackValue(VPackValueType::Object));
  builder.add(StaticStrings::Error, VPackValue(true));
  builder.add(StaticStrings::ErrorNum, VPackValue(code));
  builder.add(StaticStrings::ErrorMessage, VPackValue(TRI_errno_string(code)));
  builder.add(StaticStrings::Code, VPackValue((int)response->responseCode()));
  builder.close();

  VPackOptions options(VPackOptions::Defaults);
  options.escapeUnicode = true;
  response->setPayload(std::move(buffer), true, VPackOptions::Defaults);
}

/// @brief set the x-arango-endpoint header
void ReplicationFeature::setEndpointHeader(GeneralResponse* res,
                                           arangodb::ServerState::Mode mode) {
  std::string endpoint;
  ReplicationFeature* replication = ReplicationFeature::INSTANCE;
  if (replication != nullptr && replication->isActiveFailoverEnabled()) {
    GlobalReplicationApplier* applier = replication->globalReplicationApplier();
    if (applier != nullptr) {
      endpoint = applier->endpoint();
      // replace tcp:// with http://, and ssl:// with https://
      endpoint = FixEndpointProto(endpoint);
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
      writeError(TRI_ERROR_CLUSTER_NOT_LEADER, response);
      // return the endpoint of the actual leader
    }
    break;

    case ServerState::Mode::TRYAGAIN:
      // intentionally do not set "Location" header, but use a custom header that
      // clients can inspect. if they find an empty endpoint, it means that there
      // is an ongoing leadership challenge
      response->setHeaderNC(StaticStrings::LeaderEndpoint, "");
      writeError(TRI_ERROR_CLUSTER_LEADERSHIP_CHALLENGE_ONGOING, response);
      break;

    case ServerState::Mode::INVALID:
      writeError(TRI_ERROR_SHUTTING_DOWN, response);
      break;
    case ServerState::Mode::MAINTENANCE:
    default: {
      response->setResponseCode(rest::ResponseCode::SERVICE_UNAVAILABLE);
      break;
    }
  }
}

} // arangodb
