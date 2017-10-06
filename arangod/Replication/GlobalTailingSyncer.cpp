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

#include "GlobalTailingSyncer.h"
#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/Exceptions.h"
#include "Basics/ReadLocker.h"
#include "Basics/Result.h"
#include "Basics/StaticStrings.h"
#include "Basics/StringBuffer.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/WriteLocker.h"
#include "Logger/Logger.h"
#include "Replication/GlobalInitialSyncer.h"
#include "Replication/GlobalReplicationApplier.h"
#include "Replication/ReplicationFeature.h"
#include "Rest/HttpRequest.h"
#include "RestServer/DatabaseFeature.h"
#include "SimpleHttpClient/SimpleHttpClient.h"
#include "SimpleHttpClient/SimpleHttpResult.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "StorageEngine/StorageEngine.h"
#include "Transaction/Hints.h"
#include "Utils/CollectionGuard.h"
#include "Utils/SingleCollectionTransaction.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/voc-types.h"
#include "VocBase/vocbase.h"

#include <velocypack/Builder.h>
#include <velocypack/Iterator.h>
#include <velocypack/Parser.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

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
