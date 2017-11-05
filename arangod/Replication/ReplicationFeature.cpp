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
#include "Cluster/ClusterFeature.h"
#include "Logger/Logger.h"
#include "ProgramOptions/ProgramOptions.h"
#include "ProgramOptions/Section.h"
#include "Replication/DatabaseReplicationApplier.h"
#include "Replication/GlobalReplicationApplier.h"
#include "Replication/ReplicationApplierConfiguration.h"
#include "VocBase/vocbase.h"

using namespace arangodb;
using namespace arangodb::application_features;
using namespace arangodb::options;

ReplicationFeature* ReplicationFeature::INSTANCE = nullptr;

ReplicationFeature::ReplicationFeature(ApplicationServer* server)
    : ApplicationFeature(server, "Replication"),
      _replicationApplierAutoStart(true),
      _enableReplicationFailover(false) {

  setOptional(false);
  requiresElevatedPrivileges(false);
  startsAfter("Database");
  startsAfter("StorageEngine");
}

void ReplicationFeature::collectOptions(std::shared_ptr<ProgramOptions> options) {
  options->addSection("replication", "Configure the replication");
  options->addHiddenOption("--replication.auto-start",
                           "switch to enable or disable the automatic start "
                           "of replication appliers",
                           new BooleanParameter(&_replicationApplierAutoStart));
  
  options->addSection("database", "Configure the database");
  options->addOldOption("server.disable-replication-applier",
                        "replication.auto-start");
  options->addOldOption("database.replication-applier",
                        "replication.auto-start");
  
  options->addHiddenOption("--replication.automatic-failover",
                           "Enable auto-failover during asynchronous replication",
                           new BooleanParameter(&_enableReplicationFailover));
}

void ReplicationFeature::validateOptions(std::shared_ptr<options::ProgramOptions> options) {
  auto feature = ApplicationServer::getFeature<ClusterFeature>("Cluster");
  if (_enableReplicationFailover && feature->agencyEndpoints().empty()) {
    LOG_TOPIC(FATAL, arangodb::Logger::FIXME)
    << "automatic failover needs to be started with agency endpoint configured";
    FATAL_ERROR_EXIT();
  }
}

void ReplicationFeature::prepare() { 
  INSTANCE = this;
}

void ReplicationFeature::start() { 
  _globalReplicationApplier.reset(new GlobalReplicationApplier(GlobalReplicationApplier::loadConfiguration()));
  
  if (_globalReplicationApplier->autoStart() &&
      _globalReplicationApplier->hasState() &&
      _replicationApplierAutoStart) {
    _globalReplicationApplier->start(0, false, 0);
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
      LOG_TOPIC(INFO, arangodb::Logger::FIXME) << "replication applier explicitly deactivated for database '"
                << vocbase->name() << "'";
    } else {
      try {
        vocbase->replicationApplier()->start(0, false, 0);
      } catch (std::exception const& ex) {
        LOG_TOPIC(WARN, arangodb::Logger::FIXME) << "unable to start replication applier for database '"
                  << vocbase->name() << "': " << ex.what();
      } catch (...) {
        LOG_TOPIC(WARN, arangodb::Logger::FIXME) << "unable to start replication applier for database '"
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
