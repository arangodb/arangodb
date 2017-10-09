////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2017 ArangoDB GmbH, Cologne, Germany
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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#include "GlobalTailingSyncer.h"
#include "Logger/Logger.h"
#include "Replication/GlobalInitialSyncer.h"
#include "Replication/ReplicationFeature.h"

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::httpclient;
using namespace arangodb::rest;

GlobalTailingSyncer::GlobalTailingSyncer(
    ReplicationApplierConfiguration const& configuration,
    TRI_voc_tick_t initialTick, bool useTick, TRI_voc_tick_t barrierId)
    : TailingSyncer(ReplicationFeature::INSTANCE->globalReplicationApplier(),
                    configuration, initialTick, useTick, barrierId) {
      _ignoreDatabaseMarkers = false;
      _databaseName = TRI_VOC_SYSTEM_DATABASE;
}

std::string GlobalTailingSyncer::tailingBaseUrl(std::string const& command) {
  TRI_ASSERT(!_masterInfo._endpoint.empty());
  TRI_ASSERT(_masterInfo._serverId != 0);
  TRI_ASSERT(_masterInfo._majorVersion != 0);

  if (_masterInfo._majorVersion < 3 ||
      (_masterInfo._majorVersion == 3 && _masterInfo._minorVersion <= 2)) {
    std::string err = "You need >= 3.3 to perform replication of entire server";
    LOG_TOPIC(ERR, Logger::REPLICATION) << err;
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_NOT_IMPLEMENTED, err);
  }
  return TailingSyncer::WalAccessUrl + "/" + command + "?global=true&";
}


/// @brief save the current applier state
Result GlobalTailingSyncer::saveApplierState() {
  LOG_TOPIC(TRACE, Logger::REPLICATION)
  << "saving replication applier state. last applied continuous tick: "
  << applier()->_state._lastAppliedContinuousTick
  << ", safe resume tick: " << applier()->_state._safeResumeTick;
  
  try {
    _applier->persistState(false);
    return Result();
  } catch (basics::Exception const& ex) {
    LOG_TOPIC(WARN, Logger::REPLICATION)
    << "unable to save replication applier state: " << ex.what();
    return Result(ex.code(), ex.what());
  } catch (std::exception const& ex) {
    LOG_TOPIC(WARN, Logger::REPLICATION)
    << "unable to save replication applier state: " << ex.what();
    return Result(TRI_ERROR_INTERNAL, ex.what());
  } catch (...) {
    return Result(TRI_ERROR_INTERNAL, "unknown exception");
  }
  return TRI_ERROR_INTERNAL;
}

std::unique_ptr<InitialSyncer> GlobalTailingSyncer::initialSyncer() {
  return std::make_unique<GlobalInitialSyncer>(_configuration);
}
