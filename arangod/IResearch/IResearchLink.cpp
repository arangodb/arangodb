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
/// @author Andrey Abramov
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#include <index/column_info.hpp>
#include <store/mmap_directory.hpp>
#include <utils/encryption.hpp>
#include <utils/file_utils.hpp>
#include <utils/singleton.hpp>

#include "IResearchDocument.h"
#include "IResearchLink.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Aql/QueryCache.h"
#include "Basics/StaticStrings.h"
#include "Cluster/ClusterFeature.h"
#include "Cluster/ClusterInfo.h"
#ifdef USE_ENTERPRISE
#include "Cluster/ClusterMethods.h"
#endif
#include "IResearch/IResearchCommon.h"
#include "IResearch/IResearchCompression.h"
#include "IResearch/IResearchFeature.h"
#include "IResearch/IResearchLinkHelper.h"
#include "IResearch/IResearchPrimaryKeyFilter.h"
#include "IResearch/IResearchView.h"
#include "IResearch/IResearchViewCoordinator.h"
#include "IResearch/VelocyPackHelper.h"
#include "Metrics/BatchBuilder.h"
#include "Metrics/GaugeBuilder.h"
#include "Metrics/MetricsFeature.h"
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

namespace {


using namespace arangodb;
using namespace arangodb::iresearch;

DECLARE_GAUGE(arangosearch_num_buffered_docs, uint64_t,
              "Number of buffered documents");
DECLARE_GAUGE(arangosearch_num_docs, uint64_t, "Number of documents");
DECLARE_GAUGE(arangosearch_num_live_docs, uint64_t, "Number of live documents");
DECLARE_GAUGE(arangosearch_num_segments, uint64_t, "Number of segments");
DECLARE_GAUGE(arangosearch_num_files, uint64_t, "Number of files");
DECLARE_GAUGE(arangosearch_index_size, uint64_t, "Size of the index in bytes");
DECLARE_GAUGE(arangosearch_num_failed_commits, uint64_t,
              "Number of failed commits");
DECLARE_GAUGE(arangosearch_num_failed_cleanups, uint64_t,
              "Number of failed cleanups");
DECLARE_GAUGE(arangosearch_num_failed_consolidations, uint64_t,
              "Number of failed consolidations");
DECLARE_GAUGE(arangosearch_commit_time, uint64_t,
              "Average time of few last commits");
DECLARE_GAUGE(arangosearch_cleanup_time, uint64_t,
              "Average time of few last cleanups");
DECLARE_GAUGE(arangosearch_consolidation_time, uint64_t,
              "Average time of few last consolidations");


template <typename T>
T getMetric(const IResearchLink& link) {
  T metric;
  metric.addLabel("view", link.getViewId());
  metric.addLabel("collection", link.getCollectionName());
  metric.addLabel("shard", link.getShardName());
  metric.addLabel("db", link.getDbName());
  return metric;
}

} // namespace

namespace arangodb::iresearch {

// -----------------------------------------------------------------------------
// --SECTION--                                                     IResearchLink
// -----------------------------------------------------------------------------

IResearchLink::IResearchLink(IndexId iid, LogicalCollection& collection)
  : IResearchDataStore(iid, collection) { }


IResearchLink::~IResearchLink() {
  Result res;
  try {
    res = unload();  // disassociate from view if it has not been done yet
  } catch (...) {
  }

  if (!res.ok()) {
    LOG_TOPIC("2b41f", ERR, iresearch::TOPIC)
        << "failed to unload arangosearch link in link destructor: "
        << res.errorNumber() << " " << res.errorMessage();
  }
}

bool IResearchLink::operator==(LogicalView const& view) const noexcept {
  return _viewGuid == view.guid();
}

bool IResearchLink::operator==(IResearchLinkMeta const& meta) const noexcept {
  return _meta == meta;
}

Result IResearchLink::drop() {
  // the lookup and unlink is valid for single-server only (that is the only scenario where links are persisted)
  // on coordinator and db-server the IResearchView is immutable and lives in ClusterInfo
  // therefore on coordinator and db-server a new plan will already have an IResearchView without the link
  // this avoids deadlocks with ClusterInfo::loadPlan() during lookup in ClusterInfo
  if (ServerState::instance()->isSingleServer()) {
    auto logicalView = collection().vocbase().lookupView(_viewGuid);
    auto* view = LogicalView::cast<IResearchView>(logicalView.get());

    // may occur if the link was already unlinked from the view via another instance
    // this behavior was seen user-access-right-drop-view-arangosearch-spec.js
    // where the collection drop was called through REST,
    // the link was dropped as a result of the collection drop call
    // then the view was dropped via a separate REST call
    // then the vocbase was destroyed calling
    // collection close()-> link unload() -> link drop() due to collection marked as dropped
    // thus returning an error here will cause ~TRI_vocbase_t() on RocksDB to
    // receive an exception which is not handled in the destructor
    // the reverse happens during drop of a collection with MMFiles
    // i.e. collection drop() -> collection close()-> link unload(), then link drop()
    if (!view) {
      LOG_TOPIC("f4e2c", WARN, iresearch::TOPIC)
          << "unable to find arangosearch view '" << _viewGuid
          << "' while dropping arangosearch link '" << id().id() << "'";
    } else {
      view->unlink(collection().id());  // unlink before reset() to release lock in view (if any)
    }
  }

  return deleteDataStore();
}

Result IResearchLink::init(velocypack::Slice const& definition,
                           InitCallback const& initCallback) {
  // disassociate from view if it has not been done yet
  if (!unload().ok()) {
    return {TRI_ERROR_INTERNAL, "failed to unload link"};
  }

  std::string error;
  IResearchLinkMeta meta;

  // definition should already be normalized and analyzers created if required
  if (!meta.init(_collection.vocbase().server(), definition, true, error,
                 _collection.vocbase().name())) {
    return {TRI_ERROR_BAD_PARAMETER,
            "error parsing view link parameters from json: " + error};
  }

  if (!definition.isObject()  // not object
      || !definition.get(StaticStrings::ViewIdField).isString()) {
    return {TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND,
            "error finding view for link '" + std::to_string(_id.id()) + "'"};
  }

  auto viewId = definition.get(StaticStrings::ViewIdField).copyString();
  auto& vocbase = _collection.vocbase();
  bool const sorted = !meta._sort.empty();
  auto const& storedValuesColumns = meta._storedValues.columns();
  TRI_ASSERT(meta._sortCompression);
  auto const primarySortCompression =
      meta._sortCompression ? meta._sortCompression : getDefaultCompression();
  bool clusterWideLink{true};
  if (ServerState::instance()->isCoordinator()) {  // coordinator link
    if (!vocbase.server().hasFeature<ClusterFeature>()) {
      return {
          TRI_ERROR_INTERNAL,
          "failure to get cluster info while initializing arangosearch link '" +
              std::to_string(_id.id()) + "'"};
    }
    auto& ci = vocbase.server().getFeature<ClusterFeature>().clusterInfo();

    auto logicalView = ci.getView(vocbase.name(), viewId);

    // if there is no logicalView present yet then skip this step
    if (logicalView) {
      if (iresearch::DATA_SOURCE_TYPE != logicalView->type()) {
        return {TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND,  // code
                "error finding view: '" + viewId + "' for link '" +
                    std::to_string(_id.id()) + "' : no such view"};
      }

      auto* view = LogicalView::cast<IResearchViewCoordinator>(logicalView.get());

      if (!view) {
        return {TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND,
                "error finding view: '" + viewId + "' for link '" +
                    std::to_string(_id.id()) + "'"};
      }

      viewId = view->guid();  // ensue that this is a GUID (required by operator==(IResearchView))

      // required for IResearchViewCoordinator which calls IResearchLink::properties(...)
      std::swap(const_cast<IResearchLinkMeta&>(_meta), meta);
      auto revert = irs::make_finally([this, &meta] {
        std::swap(const_cast<IResearchLinkMeta&>(_meta), meta);
      });

      auto res = view->link(*this);

      if (!res.ok()) {
        return res;
      }
    }
  } else if (ServerState::instance()->isDBServer()) {  // db-server link
    if (!vocbase.server().hasFeature<ClusterFeature>()) {
      return {
          TRI_ERROR_INTERNAL,
          "failure to get cluster info while initializing arangosearch link '" +
              std::to_string(_id.id()) + "'"};
    }
    if (vocbase.server().getFeature<ClusterFeature>().isEnabled()) {
      auto& ci = vocbase.server().getFeature<ClusterFeature>().clusterInfo();

      clusterWideLink = _collection.id() == _collection.planId() && _collection.isAStub();

      // upgrade step for old link definition without collection name
      // this could be received from  agency while shard of the collection was moved (or added)
      // to the server.
      // New links already has collection name set, but here we must get this name on our own
      if (meta._collectionName.empty()) {
        if (clusterWideLink) {  // could set directly
          LOG_TOPIC("86ecd", TRACE, iresearch::TOPIC)
              << "Setting collection name '" << _collection.name()
              << "' for new link '" << this->id().id() << "'";
          meta._collectionName = _collection.name();
        } else {
          meta._collectionName = ci.getCollectionNameForShard(_collection.name());
          LOG_TOPIC("86ece", TRACE, iresearch::TOPIC)
              << "Setting collection name '" << meta._collectionName
              << "' for new link '" << this->id().id() << "'";
        }
        if (ADB_UNLIKELY(meta._collectionName.empty())) {
          LOG_TOPIC("67da6", WARN, iresearch::TOPIC)
              << "Failed to init collection name for the link '"
              << this->id().id() << "'. Link will not index '_id' attribute. Please recreate the link if this is necessary!";
        }

#ifdef USE_ENTERPRISE
        // enterprise name is not used in _id so should not be here!
        if (ADB_LIKELY(!meta._collectionName.empty())) {
          arangodb::ClusterMethods::realNameFromSmartName(meta._collectionName);
        }
#endif
      }

      if (!clusterWideLink) {
        // prepare data-store which can then update options
        // via the IResearchView::link(...) call
        auto res = initDataStore(initCallback, meta._version, sorted,
                                 storedValuesColumns, primarySortCompression);

        if (!res.ok()) {
          return res;
        }
      }

      // valid to call ClusterInfo (initialized in ClusterFeature::prepare()) even from DatabaseFeature::start()
      auto logicalView = ci.getView(vocbase.name(), viewId);

      // if there is no logicalView present yet then skip this step
      if (logicalView) {
        if (iresearch::DATA_SOURCE_TYPE != logicalView->type()) {
          unload();  // unlock the data store directory
          return {TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND,
                  "error finding view: '" + viewId + "' for link '" +
                      std::to_string(_id.id()) + "' : no such view"};
        }

        auto* view = LogicalView::cast<IResearchView>(logicalView.get());

        if (!view) {
          unload();  // unlock the data store directory
          return {TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND,
                  "error finding view: '" + viewId + "' for link '" +
                      std::to_string(_id.id()) + "'"};
        }

        viewId = view->guid();  // ensue that this is a GUID (required by operator==(IResearchView))

        if (clusterWideLink) {  // cluster cluster-wide link
          auto shardIds = _collection.shardIds();

          // go through all shard IDs of the collection and try to link any
          // links missing links will be populated when they are created in the
          // per-shard collection
          if (shardIds) {
            for (auto& entry : *shardIds) {
              auto collection = vocbase.lookupCollection(
                  entry.first);  // per-shard collections are always in 'vocbase'

              if (!collection) {
                continue;  // missing collection should be created after Plan becomes Current
              }

              auto link = IResearchLinkHelper::find(*collection, *view);

              if (link) {
                auto res = view->link(link->self());

                if (!res.ok()) {
                  return res;
                }
              }
            }
          }
        } else {  // cluster per-shard link
          auto res = view->link(_asyncSelf);

          if (!res.ok()) {
            unload();  // unlock the data store directory

            return res;
          }
        }
      }
    } else {
      LOG_TOPIC("67dd6", DEBUG, iresearch::TOPIC)
          << "Skipped link '" << this->id().id() << "' due to disabled cluster features.";
    }
  } else if (ServerState::instance()->isSingleServer()) {  // single-server link
    // prepare data-store which can then update options
    // via the IResearchView::link(...) call
    auto res = initDataStore(initCallback, meta._version, sorted,
                             storedValuesColumns, primarySortCompression);

    if (!res.ok()) {
      return res;
    }

    auto logicalView = vocbase.lookupView(viewId);

    // if there is no logicalView present yet then skip this step
    if (logicalView) {
      if (iresearch::DATA_SOURCE_TYPE != logicalView->type()) {
        unload();  // unlock the data store directory

        return {TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND,
                "error finding view: '" + viewId + "' for link '" +
                    std::to_string(_id.id()) + "' : no such view"};
      }

      auto* view = LogicalView::cast<IResearchView>(logicalView.get());

      if (!view) {
        unload();  // unlock the data store directory

        return {TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND,
                "error finding view: '" + viewId + "' for link '" +
                    std::to_string(_id.id()) + "'"};
      }

      viewId = view->guid();  // ensue that this is a GUID (required by operator==(IResearchView))

      auto linkRes = view->link(_asyncSelf);

      if (!linkRes.ok()) {
        unload();  // unlock the directory

        return linkRes;
      }
    }
  }

  const_cast<std::string&>(_viewGuid) = std::move(viewId);
  const_cast<IResearchLinkMeta&>(_meta) = std::move(meta);
  _comparer.reset(_meta._sort);
  return {};
}

Result IResearchLink::insert(transaction::Methods& trx,
                             LocalDocumentId const documentId, velocypack::Slice const doc) {
  return IResearchDataStore::insert<FieldIterator, IResearchLinkMeta>(trx, documentId, doc, _meta);
}

bool IResearchLink::isHidden() {
  return !ServerState::instance()->isDBServer();  // hide links unless we are on a DBServer
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
    return false;  // IResearch View identifiers of current object and slice do not match
  }

  IResearchLinkMeta other;
  std::string errorField;

  return other.init(_collection.vocbase().server(), slice, true, errorField,
                    _collection.vocbase().name())  // for db-server analyzer validation should have already passed on coordinator (missing analyzer == no match)
         && _meta == other;
}

Result IResearchLink::properties(velocypack::Builder& builder, bool forPersistence) const {
  if (!builder.isOpenObject()  // not an open object
      || !_meta.json(_collection.vocbase().server(), builder, forPersistence,
                     nullptr, &(_collection.vocbase()))) {
    return {TRI_ERROR_BAD_PARAMETER};
  }

  builder.add(arangodb::StaticStrings::IndexId,
              velocypack::Value(std::to_string(_id.id())));
  builder.add(arangodb::StaticStrings::IndexType,
              velocypack::Value(IResearchLinkHelper::type()));
  builder.add(StaticStrings::ViewIdField, velocypack::Value(_viewGuid));

  return {};
}

Result IResearchLink::properties(IResearchViewMeta const& meta) {
  // '_dataStore' can be asynchronously modified
  auto lock = _asyncSelf->lock();

  if (!_asyncSelf.get()) {
    // the current link is no longer valid (checked after ReadLock acquisition)
    return {TRI_ERROR_ARANGO_INDEX_HANDLE_BAD,
            "failed to lock arangosearch link while modifying properties "
            "of arangosearch link '" +
                std::to_string(id().id()) + "'"};
  }

  TRI_ASSERT(_dataStore);  // must be valid if _asyncSelf->get() is valid

  {
    WRITE_LOCKER(writeLock, _dataStore._mutex);  // '_meta' can be asynchronously modified
    _dataStore._meta = meta;
  }

  if (_engine->recoveryState() == RecoveryState::DONE) {
    if (meta._commitIntervalMsec) {
      scheduleCommit(std::chrono::milliseconds(meta._commitIntervalMsec));
    }

    if (meta._consolidationIntervalMsec && meta._consolidationPolicy.policy()) {
      scheduleConsolidation(std::chrono::milliseconds(meta._consolidationIntervalMsec));
    }
  }

  irs::index_writer::segment_options properties;
  properties.segment_count_max = meta._writebufferActive;
  properties.segment_memory_max = meta._writebufferSizeMax;

  static_assert(noexcept(_dataStore._writer->options(properties)));
  _dataStore._writer->options(properties);

  return {};
}

Index::IndexType IResearchLink::type() {
  // TODO: don't use enum
  return Index::TRI_IDX_TYPE_IRESEARCH_LINK;
}

char const* IResearchLink::typeName() {
  return IResearchLinkHelper::type().c_str();
}

bool IResearchLink::setCollectionName(irs::string_ref name) noexcept {
  TRI_ASSERT(!name.empty());
  if (_meta._collectionName.empty()) {
    auto& nonConstMeta = const_cast<IResearchLinkMeta&>(_meta);
    nonConstMeta._collectionName = name;
    return true;
  }
  LOG_TOPIC_IF("5573c", ERR, iresearch::TOPIC, name != _meta._collectionName)
      << "Collection name mismatch for arangosearch link '" << id() << "'."
      << " Meta name '" << _meta._collectionName << "' setting name '" << name << "'";
  TRI_ASSERT(name == _meta._collectionName);
  return false;
}

Result IResearchLink::unload() {
  // this code is used by the MMFilesEngine
  // if the collection is in the process of being removed then drop it from the view
  // FIXME TODO remove once LogicalCollection::drop(...) will drop its indexes explicitly
  if (_collection.deleted()  // collection deleted
      || TRI_vocbase_col_status_e::TRI_VOC_COL_STATUS_DELETED == _collection.status()) {
    return drop();
  }

  std::atomic_store(&_flushSubscription, {});  // reset together with '_asyncSelf'
  _asyncSelf->reset();  // the data-store is being deallocated, link use is no longer valid (wait for all the view users to finish)

  if (!_dataStore) {
    return {};
  }
  removeStats();

  try {
    _dataStore.resetDataStore();
  } catch (basics::Exception const& e) {
    return {e.code(), "caught exception while unloading arangosearch link '" +
                          std::to_string(id().id()) + "': " + e.what()};
  } catch (std::exception const& e) {
    return {TRI_ERROR_INTERNAL,
            "caught exception while removing arangosearch link '" +
                std::to_string(id().id()) + "': " + e.what()};
  } catch (...) {
    return {TRI_ERROR_INTERNAL,
            "caught exception while removing arangosearch link '" +
                std::to_string(id().id()) + "'"};
  }

  return {};
}

////////////////////////////////////////////////////////////////////////////////
/// @brief lookup referenced analyzer
////////////////////////////////////////////////////////////////////////////////
AnalyzerPool::ptr IResearchLink::findAnalyzer(AnalyzerPool const& analyzer) const {
  auto const it = _meta._analyzerDefinitions.find(irs::string_ref(analyzer.name()));

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

const std::string& IResearchLink::getViewId() const noexcept {
  return _viewGuid;
}

std::string IResearchLink::getDbName() const {
  return std::to_string(_collection.vocbase().id());
}

const std::string& IResearchLink::getShardName() const noexcept {
  if (ServerState::instance()->isDBServer()) {
    return _collection.name();
  }
  return arangodb::StaticStrings::Empty;
}

std::string IResearchLink::getCollectionName() const {
  if (ServerState::instance()->isDBServer()) {
    return _meta._collectionName;
  }
  if (ServerState::instance()->isSingleServer()) {
    return std::to_string(_collection.id().id());
  }
  TRI_ASSERT(false);
  return {};
}

IResearchLink::LinkStats IResearchLink::stats() const {
  return LinkStats(statsSynced());
}

void IResearchLink::updateStats(IResearchDataStore::Stats const& stats) {
  _linkStats->store(IResearchLink::LinkStats(stats));
}

void IResearchLink::insertStats() {
  auto& metric = _collection.vocbase().server().getFeature<metrics::MetricsFeature>();
  auto builder = getMetric<metrics::BatchBuilder<LinkStats>>(*this);
  builder.setName("arangosearch_link_stats");
  _linkStats = &metric.add(std::move(builder));
  _numFailedCommits = &metric.add(getMetric<arangosearch_num_failed_commits>(*this));
  _numFailedCleanups = &metric.add(getMetric<arangosearch_num_failed_cleanups>(*this));
  _numFailedConsolidations =
    &metric.add(getMetric<arangosearch_num_failed_consolidations>(*this));
  _avgCommitTimeMs = &metric.add(getMetric<arangosearch_commit_time>(*this));
  _avgCleanupTimeMs = &metric.add(getMetric<arangosearch_cleanup_time>(*this));
  _avgConsolidationTimeMs =
    &metric.add(getMetric<arangosearch_consolidation_time>(*this));
}

void IResearchLink::removeStats() {
  auto& metricFeature =
      _collection.vocbase().server().getFeature<metrics::MetricsFeature>();
  if (_linkStats) {
    _linkStats = nullptr;
    auto builder = getMetric<metrics::BatchBuilder<LinkStats>>(*this);
    builder.setName("arangosearch_link_stats");
    metricFeature.remove(std::move(builder));
  }
  if (_numFailedCommits) {
    _numFailedCommits = nullptr;
    metricFeature.remove(getMetric<arangosearch_num_failed_commits>(*this));
  }
  if (_numFailedCleanups) {
    _numFailedCleanups = nullptr;
    metricFeature.remove(getMetric<arangosearch_num_failed_cleanups>(*this));
  }
  if (_numFailedConsolidations) {
    _numFailedConsolidations = nullptr;
    metricFeature.remove(getMetric<arangosearch_num_failed_consolidations>(*this));
  }
  if (_avgCommitTimeMs) {
    _avgCommitTimeMs = nullptr;
    metricFeature.remove(getMetric<arangosearch_commit_time>(*this));
  }
  if (_avgCleanupTimeMs) {
    _avgCleanupTimeMs = nullptr;
    metricFeature.remove(getMetric<arangosearch_cleanup_time>(*this));
  }
  if (_avgConsolidationTimeMs) {
    _avgConsolidationTimeMs = nullptr;
    metricFeature.remove(getMetric<arangosearch_consolidation_time>(*this));
  }
}

void IResearchLink::invalidateQueryCache(TRI_vocbase_t* vocbase) {
  aql::QueryCache::instance()->invalidate(vocbase , _viewGuid);
}

void IResearchLink::LinkStats::needName() const { _needName = true; }

void IResearchLink::LinkStats::toPrometheus(std::string& result,       //
                                            std::string_view globals,  //
                                            std::string_view labels) const {
  auto writeAnnotation = [&] {
    result.push_back('{');
    result.append(globals);
    if (!labels.empty()) {
      if (!globals.empty()) {
        result.push_back(',');
      }
      result.append(labels);
    }
    result.push_back('}');
  };
  auto writeMetric = [&](std::string_view name, std::string_view help, size_t value) {
    if (_needName) {
      result.append("# HELP ");
      result.append(name);
      result.push_back(' ');
      result.append(help);
      result.push_back('\n');
      result.append("# TYPE ");
      result.append(name);
      result.append(" gauge\n");
    }
    result.append(name);
    writeAnnotation();
    result.append(std::to_string(value));
    result.push_back('\n');
  };
  writeMetric(arangosearch_num_buffered_docs::kName,
              "Number of buffered documents", numBufferedDocs);
  writeMetric(arangosearch_num_docs::kName, "Number of documents", numDocs);
  writeMetric(arangosearch_num_live_docs::kName, "Number of live documents", numLiveDocs);
  writeMetric(arangosearch_num_segments::kName, "Number of segments", numSegments);
  writeMetric(arangosearch_num_files::kName, "Number of files", numFiles);
  writeMetric(arangosearch_index_size::kName, "Size of the index in bytes", indexSize);
  _needName = false;
}

}  // namespace arangodb::iresearch
