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
/// @author Lars Maier
////////////////////////////////////////////////////////////////////////////////

#include "StorageEngine.h"

#include <velocypack/Builder.h>
#include <velocypack/Slice.h>

#include "Replication2/ReplicatedLog/LogCommon.h"

#include <utility>

using namespace arangodb;

StorageEngine::StorageEngine(application_features::ApplicationServer& server,
                             std::string engineName, const std::string& featureName,
                             std::unique_ptr<IndexFactory>&& indexFactory)
    : application_features::ApplicationFeature(server, featureName),
      _indexFactory(std::move(indexFactory)),
      _typeName(std::move(engineName)) {
  // each specific storage engine feature is optional. the storage engine
  // selection feature will make sure that exactly one engine is selected at
  // startup
  setOptional(true);
  // storage engines must not use elevated privileges for files etc
  startsAfter<application_features::BasicFeaturePhaseServer>();

  startsAfter<CacheManagerFeature>();
  startsBefore<StorageEngineFeature>();
  startsAfter<transaction::ManagerFeature>();
  startsAfter<ViewTypesFeature>();
}

void StorageEngine::addParametersForNewCollection(velocypack::Builder&, VPackSlice) {}

Result StorageEngine::writeCreateDatabaseMarker(TRI_voc_tick_t id, const VPackSlice& slice) { return {}; }
Result StorageEngine::prepareDropDatabase(TRI_vocbase_t& vocbase) { return {}; }
bool StorageEngine::inRecovery() { return recoveryState() < RecoveryState::DONE; }
IndexFactory const& StorageEngine::indexFactory() const {
  // The factory has to be created by the implementation
  // and shall never be deleted
  TRI_ASSERT(_indexFactory.get() != nullptr);
  return *_indexFactory;
}

void StorageEngine::getCapabilities(velocypack::Builder& builder) const {
  builder.openObject();
  builder.add("name", velocypack::Value(typeName()));
  builder.add("supports", velocypack::Value(VPackValueType::Object));
  builder.add("dfdb", velocypack::Value(false));

  builder.add("indexes", velocypack::Value(VPackValueType::Array));
  for (auto const& it : indexFactory().supportedIndexes()) {
    builder.add(velocypack::Value(it));
  }
  builder.close();  // indexes

  builder.add("aliases", velocypack::Value(VPackValueType::Object));
  builder.add("indexes", velocypack::Value(VPackValueType::Object));
  for (auto const& it : indexFactory().indexAliases()) {
    builder.add(it.first, velocypack::Value(it.second));
  }
  builder.close();  // indexes
  builder.close();  // aliases

  builder.close();  // supports
  builder.close();  // object
}

void StorageEngine::getStatistics(velocypack::Builder& builder, bool v2) const {
  builder.openObject();
  builder.close();
}

void StorageEngine::getStatistics(std::string& result, bool v2) const {}

void StorageEngine::registerCollection(TRI_vocbase_t& vocbase,
                                       const std::shared_ptr<arangodb::LogicalCollection>& collection) {
  vocbase.registerCollection(true, collection);
}

void StorageEngine::registerView(TRI_vocbase_t& vocbase,
                                 const std::shared_ptr<arangodb::LogicalView>& view) {
  vocbase.registerView(true, view);
}

void StorageEngine::registerReplicatedLog(
    TRI_vocbase_t& vocbase, arangodb::replication2::LogId id,
    std::shared_ptr<arangodb::replication2::replicated_log::PersistedLog> log) {
  vocbase.registerReplicatedLog(id, std::move(log));
}

std::string const& StorageEngine::typeName() const { return _typeName; }

void StorageEngine::addOptimizerRules(aql::OptimizerRulesFeature&) {}

void StorageEngine::addV8Functions() {}

void StorageEngine::addRestHandlers(rest::RestHandlerFactory& handlerFactory) {}
