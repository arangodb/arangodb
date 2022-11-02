////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
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
/// @author Andrey Abramov
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////
#include "IResearchLink.h"

#include <index/column_info.hpp>
#include <utils/singleton.hpp>

#include "ApplicationFeatures/ApplicationServer.h"
#include "Aql/QueryCache.h"
#include "Basics/DownCast.h"
#include "Basics/StaticStrings.h"
#include "Cluster/ClusterFeature.h"
#include "Cluster/ClusterInfo.h"
#include "IResearchDocument.h"
#ifdef USE_ENTERPRISE
#include "Cluster/ClusterMethods.h"
#endif
#include "IResearch/IResearchCommon.h"
#include "IResearch/IResearchCompression.h"
#include "IResearch/IResearchFeature.h"
#include "IResearch/IResearchLinkHelper.h"
#include "IResearch/IResearchMetricStats.h"
#include "IResearch/IResearchPrimaryKeyFilter.h"
#include "IResearch/IResearchView.h"
#include "IResearch/IResearchViewCoordinator.h"
#include "IResearch/VelocyPackHelper.h"
#include "RestServer/DatabaseFeature.h"
#include "RestServer/DatabasePathFeature.h"
#include "RestServer/FlushFeature.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "StorageEngine/StorageEngine.h"
#include "StorageEngine/TransactionState.h"
#include "Transaction/Helpers.h"
#include "Transaction/Methods.h"
#include "VocBase/LogicalCollection.h"

using namespace std::literals;

namespace arangodb::iresearch {
namespace {

// Ensures that all referenced analyzer features are consistent.
[[maybe_unused]] void checkAnalyzerFeatures(IResearchLinkMeta const& meta) {
  auto assertAnalyzerFeatures =
      [version = LinkVersion{meta._version}](auto const& analyzers) {
        for (auto& analyzer : analyzers) {
          irs::type_info::type_id invalidNorm;
          if (version < LinkVersion::MAX) {
            invalidNorm = irs::type<irs::Norm2>::id();
          } else {
            invalidNorm = irs::type<irs::Norm>::id();
          }

          const auto features = analyzer->fieldFeatures();

          TRI_ASSERT(std::end(features) == std::find(std::begin(features),
                                                     std::end(features),
                                                     invalidNorm));
        }
      };

  auto checkFieldFeatures = [&assertAnalyzerFeatures](auto const& fieldMeta,
                                                      auto&& self) -> void {
    assertAnalyzerFeatures(fieldMeta._analyzers);
    for (auto const& entry : fieldMeta._fields) {
      self(*entry.value(), self);
    }
  };
  assertAnalyzerFeatures(meta._analyzerDefinitions);
  checkFieldFeatures(meta, checkFieldFeatures);
}

template<typename T>
T getMetric(IResearchLink const& link) {
  T metric;
  metric.addLabel("db", link.getDbName());
  metric.addLabel("view", link.getViewId());
  metric.addLabel("collection", link.getCollectionName());
  metric.addLabel("shard", link.getShardName());
  return metric;
}

std::string getLabels(IResearchLink const& link) {
  return "db=\"" + link.getDbName() +                     //
         "\",view=\"" + link.getViewId() +                //
         "\",collection=\"" + link.getCollectionName() +  //
         "\",shard=\"" + link.getShardName() + "\"";
}

Result linkWideCluster(LogicalCollection const& logical, IResearchView* view) {
  if (!view) {
    return {};
  }
  auto shardIds = logical.shardIds();
  // go through all shard IDs of the collection and
  // try to link any links missing links will be populated
  // when they are created in the per-shard collection
  if (!shardIds) {
    return {};
  }
  for (auto& entry : *shardIds) {  // per-shard collection is always in vocbase
    auto collection = logical.vocbase().lookupCollection(entry.first);
    if (!collection) {
      // missing collection should be created after Plan becomes Current
      continue;
    }
    if (auto link = IResearchLinkHelper::find(*collection, *view); link) {
      if (auto r = view->link(link->self()); !r.ok()) {
        return r;
      }
    }
  }
  return {};
}

}  // namespace

template<typename T>
Result IResearchLink::toView(std::shared_ptr<LogicalView> const& logical,
                             std::shared_ptr<T>& view) {
  if (!logical) {
    return {};
  }
  if (logical->type() != ViewType::kArangoSearch) {
    return {TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND,
            "error finding view: '" + _viewGuid + "' for link '" +
                std::to_string(_id.id()) + "' : no such view"};
  }
  view = basics::downCast<T>(logical);
  // TODO(MBkkt) Now its workaround for unit tests that expected this behavior
  _viewGuid = view->guid();  // TRI_ASSERT(_viewGuid == view->guid());
  return {};
}

Result IResearchLink::initAndLink(bool& pathExists, InitCallback const& init,
                                  IResearchView* view) {
  auto r = initDataStore(pathExists, init, _meta._version, !_meta._sort.empty(),
#ifdef USE_ENTERPRISE
                         _meta._hasNested,
#else
                         false,
#endif
                         _meta._storedValues.columns(), _meta._sortCompression);
  if (r.ok() && view) {
    r = view->link(_asyncSelf);
  }
  return r;
}

Result IResearchLink::initSingleServer(bool& pathExists,
                                       InitCallback const& init) {
  std::shared_ptr<IResearchView> view;
  auto r = toView(_collection.vocbase().lookupView(_viewGuid), view);
  if (!r.ok()) {
    return r;
  }
  return initAndLink(pathExists, init, view.get());
}

Result IResearchLink::initCoordinator(InitCallback const& init) {
  auto& vocbase = _collection.vocbase();
  auto& ci = vocbase.server().getFeature<ClusterFeature>().clusterInfo();
  std::shared_ptr<IResearchViewCoordinator> view;
  if (auto r = toView(ci.getView(vocbase.name(), _viewGuid), view); !view) {
    return r;
  }
  return view->link(*this);
}

Result IResearchLink::initDBServer(bool& pathExists, InitCallback const& init) {
  auto& vocbase = _collection.vocbase();
  auto& server = vocbase.server();
  bool const clusterEnabled = server.getFeature<ClusterFeature>().isEnabled();
  bool wide = _collection.id() == _collection.planId() && _collection.isAStub();
  std::shared_ptr<IResearchView> view;
  if (clusterEnabled) {
    auto& ci = server.getFeature<ClusterFeature>().clusterInfo();
    clusterCollectionName(_collection, wide ? nullptr : &ci, id().id(),
                          _meta.willIndexIdAttribute(), _meta._collectionName);
    if (auto r = toView(ci.getView(vocbase.name(), _viewGuid), view); !r.ok()) {
      return r;
    }
  } else {
    LOG_TOPIC("67dd6", DEBUG, TOPIC)
        << "Skipped link '" << id().id()
        << "' maybe due to disabled cluster features.";
  }
  if (wide) {
    return linkWideCluster(_collection, view.get());
  }
  if (_meta._collectionName.empty() && !clusterEnabled &&
      server.getFeature<EngineSelectorFeature>().engine().inRecovery() &&
      _meta.willIndexIdAttribute()) {
    LOG_TOPIC("f25ce", FATAL, TOPIC)
        << "Upgrade conflicts with recovering ArangoSearch link '" << id().id()
        << "' Please rollback the updated arangodb binary and"
           " finish the recovery first.";
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_INTERNAL,
        "Upgrade conflicts with recovering ArangoSearch link."
        " Please rollback the updated arangodb binary and"
        " finish the recovery first.");
  }
  return initAndLink(pathExists, init, view.get());
}

IResearchLink::IResearchLink(IndexId iid, LogicalCollection& collection)
    : IResearchDataStore(iid, collection) {}

IResearchLink::~IResearchLink() {
  // disassociate from view if it has not been done yet
  auto r = unload();
  if (!r.ok()) {
    try {
      LOG_TOPIC("2b41f", ERR, TOPIC)
          << "failed to unload arangodb_search link in link destructor: "
          << r.errorNumber() << " " << r.errorMessage();
    } catch (...) {
    }
  }
}

bool IResearchLink::operator==(LogicalView const& view) const noexcept {
  return _viewGuid == view.guid();
}

bool IResearchLink::operator==(IResearchLinkMeta const& meta) const noexcept {
  return _meta == meta;
}

Result IResearchLink::drop() {
  // the lookup and unlink is valid for single-server only (that is the only
  // scenario where links are persisted) on coordinator and db-server the
  // IResearchView is immutable and lives in ClusterInfo therefore on
  // coordinator and db-server a new plan will already have an IResearchView
  // without the link this avoids deadlocks with ClusterInfo::loadPlan() during
  // lookup in ClusterInfo
  if (ServerState::instance()->isSingleServer()) {
    auto view = basics::downCast<IResearchView>(
        collection().vocbase().lookupView(_viewGuid));

    // may occur if the link was already unlinked from the view via another
    // instance this behavior was seen
    // user-access-right-drop-view-arangosearch-spec.js where the collection
    // drop was called through REST, the link was dropped as a result of the
    // collection drop call then the view was dropped via a separate REST call
    // then the vocbase was destroyed calling
    // collection close()-> link unload() -> link drop() due to collection
    // marked as dropped thus returning an error here will cause
    // ~TRI_vocbase_t() on RocksDB to receive an exception which is not handled
    // in the destructor the reverse happens during drop of a collection with
    // MMFiles i.e. collection drop() -> collection close()-> link unload(),
    // then link drop()
    if (!view) {
      LOG_TOPIC("f4e2c", WARN, iresearch::TOPIC)
          << "unable to find arangosearch view '" << _viewGuid
          << "' while dropping arangosearch link '" << _id.id() << "'";
    } else {
      // unlink before reset() to release lock in view (if any)
      view->unlink(collection().id());
    }
  }

  return deleteDataStore();
}

Result IResearchLink::init(velocypack::Slice definition, bool& pathExists,
                           InitCallback const& init) {
  auto& vocbase = _collection.vocbase();
  auto& server = vocbase.server();
  bool const isSingleServer = ServerState::instance()->isSingleServer();
  if (!isSingleServer && !server.hasFeature<ClusterFeature>()) {
    return {
        TRI_ERROR_INTERNAL,
        "failure to get cluster info while initializing arangosearch link '" +
            std::to_string(_id.id()) + "'"};
  }
  std::string error;
  // definition should already be normalized and analyzers created if required
  if (!_meta.init(server, definition, error, vocbase.name())) {
    return {TRI_ERROR_BAD_PARAMETER,
            "error parsing view link parameters from json: " + error};
  }
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  checkAnalyzerFeatures(_meta);
#endif
  if (!definition.isObject() ||
      !definition.get(StaticStrings::ViewIdField).isString()) {
    return {TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND,
            "error finding view for link '" + std::to_string(_id.id()) + "'"};
  }
  TRI_ASSERT(_meta._sortCompression);
  _viewGuid = definition.get(StaticStrings::ViewIdField).stringView();

  if (auto s = definition.get(StaticStrings::LinkError); s.isString()) {
    if (s.stringView() == StaticStrings::LinkErrorOutOfSync) {
      // mark index as out of sync
      setOutOfSync();
    } else if (s.stringView() == StaticStrings::LinkErrorFailed) {
      // TODO: not implemented yet
    }
  }

  Result r;
  if (isSingleServer) {
    r = initSingleServer(pathExists, init);
  } else if (ServerState::instance()->isCoordinator()) {
    r = initCoordinator(init);
  } else if (ServerState::instance()->isDBServer()) {
    r = initDBServer(pathExists, init);
  } else {
    TRI_ASSERT(false);
    return r;
  }
  if (r.ok()) {  // TODO(MBkkt) Do we really need this check?
    _comparer.reset(_meta._sort);
  }
  return r;
}

Result IResearchLink::insert(transaction::Methods& trx,
                             LocalDocumentId const documentId,
                             velocypack::Slice const doc) {
  return IResearchDataStore::insert<FieldIterator<FieldMeta>,
                                    IResearchLinkMeta>(trx, documentId, doc,
                                                       _meta);
}

bool IResearchLink::isHidden() {
  // hide links unless we are on a DBServer
  return !ServerState::instance()->isDBServer();
}

bool IResearchLink::isSorted() {
  return false;  // IResearch does not provide a fixed default sort order
}

void IResearchLink::load() {
  // Note: this function is only used by RocksDB
}

bool IResearchLink::matchesDefinition(velocypack::Slice slice) const {
  if (!slice.isObject() || !slice.hasKey(StaticStrings::ViewIdField)) {
    return false;  // slice has no view identifier field
  }
  auto viewId = slice.get(StaticStrings::ViewIdField);
  // NOTE: below will not match if 'viewId' is 'id' or 'name',
  //       but ViewIdField should always contain GUID
  if (!viewId.isString() || !viewId.isEqualString(_viewGuid)) {
    // IResearch View identifiers of current object and slice do not match
    return false;
  }
  IResearchLinkMeta other;
  std::string errorField;
  // for db-server analyzer validation should have already passed on coordinator
  // (missing analyzer == no match)
  auto& vocbase = _collection.vocbase();
  return other.init(vocbase.server(), slice, errorField, vocbase.name()) &&
         _meta == other;
}

Result IResearchLink::properties(velocypack::Builder& builder,
                                 bool forPersistence) const {
  if (!builder.isOpenObject()  // not an open object
      || !_meta.json(_collection.vocbase().server(), builder, forPersistence,
                     nullptr, &(_collection.vocbase()))) {
    return {TRI_ERROR_BAD_PARAMETER};
  }

  builder.add(arangodb::StaticStrings::IndexId,
              velocypack::Value(std::to_string(_id.id())));
  builder.add(arangodb::StaticStrings::IndexType,
              velocypack::Value(
                  arangodb::iresearch::StaticStrings::ViewArangoSearchType));
  builder.add(StaticStrings::ViewIdField, velocypack::Value(_viewGuid));

  if (isOutOfSync()) {
    // link is out of sync - we need to report that
    builder.add(StaticStrings::LinkError,
                VPackValue(StaticStrings::LinkErrorOutOfSync));
  }

  return {};
}

Index::IndexType IResearchLink::type() {
  // TODO: don't use enum
  return Index::TRI_IDX_TYPE_IRESEARCH_LINK;
}

char const* IResearchLink::typeName() {
  return StaticStrings::ViewArangoSearchType.data();
}

bool IResearchLink::setCollectionName(irs::string_ref name) noexcept {
  TRI_ASSERT(!name.empty());
  if (_meta._collectionName.empty()) {
    _meta._collectionName = name;
    return true;
  }
  LOG_TOPIC_IF("5573c", ERR, TOPIC, name != _meta._collectionName)
      << "Collection name mismatch for arangosearch link '" << id() << "'."
      << " Meta name '" << _meta._collectionName << "' setting name '" << name
      << "'";
  TRI_ASSERT(name == _meta._collectionName);
  return false;
}

Result IResearchLink::unload() noexcept {
  // TODO(MBkkt) It's true? Now it's just a guess.
  // This is necessary because the removal of the collection can be parallel
  // with the removal of the link: when we decided whether to drop the link,
  // the collection was still there. But when we did an unload link,
  // the collection was already lazily deleted.
  if (_collection.deleted()  // collection deleted
      || _collection.status() == TRI_VOC_COL_STATUS_DELETED) {
    return basics::catchToResult([&] { return drop(); });
  }
  shutdownDataStore();
  return {};
}

////////////////////////////////////////////////////////////////////////////////
/// @brief lookup referenced analyzer
////////////////////////////////////////////////////////////////////////////////
AnalyzerPool::ptr IResearchLink::findAnalyzer(
    AnalyzerPool const& analyzer) const {
  auto const it =
      _meta._analyzerDefinitions.find(irs::string_ref(analyzer.name()));

  if (it == _meta._analyzerDefinitions.end()) {
    return nullptr;
  }

  auto pool = *it;

  if (pool && analyzer == *pool) {
    return pool;
  }

  return nullptr;
}

std::string_view IResearchLink::format() const noexcept {
  return getFormat(LinkVersion{_meta._version});
}

IResearchViewStoredValues const& IResearchLink::storedValues() const noexcept {
  return _meta._storedValues;
}

std::string const& IResearchLink::getDbName() const noexcept {
  return _collection.vocbase().name();
}

std::string const& IResearchLink::getViewId() const noexcept {
  return _viewGuid;
}

std::string IResearchLink::getCollectionName() const {
  if (ServerState::instance()->isSingleServer()) {
    return std::to_string(_collection.id().id());
  }
  if (ServerState::instance()->isDBServer()) {
    return _meta._collectionName;
  }
  return _collection.name();
}

std::string const& IResearchLink::getShardName() const noexcept {
  if (ServerState::instance()->isDBServer()) {
    return _collection.name();
  }
  return arangodb::StaticStrings::Empty;
}

bool IResearchLink::hasNested() const noexcept {
#ifdef USE_ENTERPRISE
  return _meta._hasNested;
#else
  return false;
#endif
}

void IResearchLink::insertMetrics() {
  auto& metric =
      _collection.vocbase().server().getFeature<metrics::MetricsFeature>();
  _numFailedCommits =
      &metric.add(getMetric<arangodb_search_num_failed_commits>(*this));
  _numFailedCleanups =
      &metric.add(getMetric<arangodb_search_num_failed_cleanups>(*this));
  _numFailedConsolidations =
      &metric.add(getMetric<arangodb_search_num_failed_consolidations>(*this));
  _avgCommitTimeMs = &metric.add(getMetric<arangodb_search_commit_time>(*this));
  _avgCleanupTimeMs =
      &metric.add(getMetric<arangodb_search_cleanup_time>(*this));
  _avgConsolidationTimeMs =
      &metric.add(getMetric<arangodb_search_consolidation_time>(*this));
  _metricStats = &metric.batchAdd<MetricStats>(kSearchStats, getLabels(*this));
}

void IResearchLink::removeMetrics() {
  auto& metric =
      _collection.vocbase().server().getFeature<metrics::MetricsFeature>();
  if (_numFailedCommits) {
    _numFailedCommits = nullptr;
    metric.remove(getMetric<arangodb_search_num_failed_commits>(*this));
  }
  if (_numFailedCleanups) {
    _numFailedCleanups = nullptr;
    metric.remove(getMetric<arangodb_search_num_failed_cleanups>(*this));
  }
  if (_numFailedConsolidations) {
    _numFailedConsolidations = nullptr;
    metric.remove(getMetric<arangodb_search_num_failed_consolidations>(*this));
  }
  if (_avgCommitTimeMs) {
    _avgCommitTimeMs = nullptr;
    metric.remove(getMetric<arangodb_search_commit_time>(*this));
  }
  if (_avgCleanupTimeMs) {
    _avgCleanupTimeMs = nullptr;
    metric.remove(getMetric<arangodb_search_cleanup_time>(*this));
  }
  if (_avgConsolidationTimeMs) {
    _avgConsolidationTimeMs = nullptr;
    metric.remove(getMetric<arangodb_search_consolidation_time>(*this));
  }
  if (_metricStats) {
    _metricStats = nullptr;
    metric.batchRemove(kSearchStats, getLabels(*this));
  }
}

void IResearchLink::invalidateQueryCache(TRI_vocbase_t* vocbase) {
  aql::QueryCache::instance()->invalidate(vocbase, _viewGuid);
}

}  // namespace arangodb::iresearch
