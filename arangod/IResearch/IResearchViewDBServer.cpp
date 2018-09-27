////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2018 ArangoDB GmbH, Cologne, Germany
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

#include "IResearchCommon.h"
#include "IResearchDocument.h"
#include "IResearchFeature.h"
#include "IResearchLinkHelper.h"
#include "IResearchView.h"
#include "IResearchViewDBServer.h"
#include "VelocyPackHelper.h"
#include "Basics/StaticStrings.h"
#include "Cluster/ClusterInfo.h"
#include "Logger/LogMacros.h"
#include "RestServer/DatabasePathFeature.h"
#include "StorageEngine/TransactionState.h"
#include "Transaction/Methods.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/vocbase.h"

namespace {

typedef irs::async_utils::read_write_mutex::read_mutex ReadMutex;
typedef irs::async_utils::read_write_mutex::write_mutex WriteMutex;

////////////////////////////////////////////////////////////////////////////////
/// @brief the view name prefix of per-cid view instances
////////////////////////////////////////////////////////////////////////////////
std::string const VIEW_NAME_PREFIX("_iresearch_");

////////////////////////////////////////////////////////////////////////////////
/// @brief index reader implementation over multiple PrimaryKeyIndexReaders
////////////////////////////////////////////////////////////////////////////////
class CompoundReader final: public arangodb::iresearch::PrimaryKeyIndexReader {
 public:
  irs::sub_reader const& operator[](
      size_t subReaderId
  ) const noexcept override {
    return *(_subReaders[subReaderId].first);
  }

  void add(arangodb::iresearch::PrimaryKeyIndexReader const& reader);
  virtual reader_iterator begin() const override;
  void clear() noexcept { _subReaders.clear(); }
  virtual uint64_t docs_count() const override;
  virtual uint64_t docs_count(const irs::string_ref& field) const override;
  virtual reader_iterator end() const override;
  virtual uint64_t live_docs_count() const override;

  irs::columnstore_reader::values_reader_f const& pkColumn(
      size_t subReaderId
  ) const noexcept override {
    return _subReaders[subReaderId].second;
  }

  virtual size_t size() const noexcept override { return _subReaders.size(); }

 private:
  typedef std::vector<
    std::pair<irs::sub_reader*, irs::columnstore_reader::values_reader_f>
  > SubReadersType;

  class IteratorImpl final: public irs::index_reader::reader_iterator_impl {
   public:
    explicit IteratorImpl(SubReadersType::const_iterator const& itr)
      : _itr(itr) {
    }

    virtual void operator++() noexcept override { ++_itr; }
    virtual reference operator*() noexcept override { return *(_itr->first); }

    virtual const_reference operator*() const noexcept override {
      return *(_itr->first);
    }

    virtual bool operator==(
        const reader_iterator_impl& other
    ) noexcept override {
      return static_cast<IteratorImpl const&>(other)._itr == _itr;
    }

   private:
    SubReadersType::const_iterator _itr;
  };

  SubReadersType _subReaders;
};

void CompoundReader::add(
    arangodb::iresearch::PrimaryKeyIndexReader const& reader
) {
  for(auto& entry: reader) {
    const auto* pkColumn =
      entry.column_reader(arangodb::iresearch::DocumentPrimaryKey::PK());

    if (!pkColumn) {
      LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
        << "encountered a sub-reader without a primary key column while creating a reader for IResearch view, ignoring";

      continue;
    }

    _subReaders.emplace_back(&entry, pkColumn->values());
  }
}

irs::index_reader::reader_iterator CompoundReader::begin() const {
  return reader_iterator(new IteratorImpl(_subReaders.begin()));
}

uint64_t CompoundReader::docs_count() const {
  uint64_t count = 0;

  for (auto& entry: _subReaders) {
    count += entry.first->docs_count();
  }

  return count;
}

uint64_t CompoundReader::docs_count(const irs::string_ref& field) const {
  uint64_t count = 0;

  for (auto& entry: _subReaders) {
    count += entry.first->docs_count(field);
  }

  return count;
}

irs::index_reader::reader_iterator CompoundReader::end() const {
  return reader_iterator(new IteratorImpl(_subReaders.end()));
}

uint64_t CompoundReader::live_docs_count() const {
  uint64_t count = 0;

  for (auto& entry: _subReaders) {
    count += entry.first->live_docs_count();
  }

  return count;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief the container storing the view state for a given TransactionState
/// @note it is assumed that DBServer ViewState resides in the same
///       TransactionState as the IResearchView ViewState, therefore a separate
///       lock is not required to be held by the DBServer CompoundReader
////////////////////////////////////////////////////////////////////////////////
struct ViewState: public arangodb::TransactionState::Cookie {
  CompoundReader _snapshot;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief generate the name used for the per-cid views
///        must be unique to avoid view collisions in vocbase
////////////////////////////////////////////////////////////////////////////////
std::string generateName(TRI_voc_cid_t viewId, TRI_voc_cid_t collectionId) {
  return VIEW_NAME_PREFIX
   + std::to_string(collectionId)
   + "_" + std::to_string(viewId)
   ;
}

}

namespace arangodb {
namespace iresearch {

IResearchViewDBServer::IResearchViewDBServer(
    TRI_vocbase_t& vocbase,
    arangodb::velocypack::Slice const& info,
    arangodb::DatabasePathFeature const& dbPathFeature,
    uint64_t planVersion
): LogicalViewClusterInfo(vocbase, info, planVersion) {
}

IResearchViewDBServer::~IResearchViewDBServer() {
  _collections.clear(); // ensure view distructors called before mutex is deallocated
}

arangodb::Result IResearchViewDBServer::appendVelocyPackDetailed(
  arangodb::velocypack::Builder& builder,
  bool //forPersistence
) const {
  if (!builder.isOpenObject()) {
    return arangodb::Result(
      TRI_ERROR_BAD_PARAMETER,
      std::string("invalid builder provided for IResearchViewDBServer definition")
    );
  }

  {
    SCOPED_LOCK(_meta->read()); // '_meta' can be asynchronously updated

    if (!_meta->json(builder)) {
      return arangodb::Result(
        TRI_ERROR_INTERNAL,
        std::string("failure to generate definition while generating properties jSON for IResearch View in database '") + vocbase().name() + "'"
      );
    }
  }

  return arangodb::Result();
}


arangodb::Result IResearchViewDBServer::drop() {
  WriteMutex mutex(_mutex);
  SCOPED_LOCK(mutex); // 'collections_' can be asynchronously read

  for (auto itr = _collections.begin(); itr != _collections.end();) {
    auto res = vocbase().dropView(itr->second->id(), true); // per-cid collections always system

    if (!res.ok()) {
      return res; // fail on first failure
    }

    itr = _collections.erase(itr);
  }

  return arangodb::Result();
}

arangodb::Result IResearchViewDBServer::drop(TRI_voc_cid_t cid) noexcept {
  try {
    WriteMutex mutex(_mutex);
    SCOPED_LOCK(mutex); // 'collections_' can be asynchronously read
    auto itr = _collections.find(cid);

    if (itr == _collections.end()) {
      return arangodb::Result(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND);
    }

    auto res = vocbase().dropView(itr->second->id(), true); // per-cid collections always system

    if (res.ok()) {
      _collections.erase(itr);
    }

    return res;
  } catch (arangodb::basics::Exception& e) {
    LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
      << "caught exception while dropping collection '" << cid << "' from IResearchView '" << name() << "': " << e.code() << " " << e.what();
    IR_LOG_EXCEPTION();

    return arangodb::Result(
      e.code(),
      std::string("error dropping collection '") + std::to_string(cid) + "' from IResearchView '" + name() + "'"
    );
  } catch (std::exception const& e) {
    LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
      << "caught exception while dropping collection '" << cid << "' from IResearchView '" << name() << "': " << e.what();
    IR_LOG_EXCEPTION();

    return arangodb::Result(
      TRI_ERROR_INTERNAL,
      std::string("error dropping collection '") + std::to_string(cid) + "' from IResearchView '" + name() + "'"
    );
  } catch (...) {
    LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
      << "caught exception while dropping collection '" << cid << "' from IResearchView '" << name() << "'";
    IR_LOG_EXCEPTION();

    return arangodb::Result(
      TRI_ERROR_INTERNAL,
      std::string("error dropping collection '") + std::to_string(cid) + "' from IResearchView '" + name() + "'"
    );
  }

  return arangodb::Result(TRI_ERROR_INTERNAL);
}

std::shared_ptr<arangodb::LogicalView> IResearchViewDBServer::ensure(
    TRI_voc_cid_t cid,
    bool create /*= true*/
) {
  WriteMutex mutex(_mutex);
  SCOPED_LOCK(mutex); // 'collections_' can be asynchronously read
  auto itr = _collections.find(cid);

  if (itr != _collections.end()) {
    return itr->second;
  }

  auto viewName = generateName(id(), cid);
  auto view = vocbase().lookupView(viewName); // on startup a IResearchView might only be in vocbase but not in a brand new IResearchViewDBServer
  auto* impl = LogicalView::cast<IResearchView>(view.get());

  if (impl) {
    _collections.emplace(cid, view); // track the IResearchView instance from vocbase
    impl->updateProperties(_meta);

    return view; // do not wrap in deleter since view already present in vocbase (as if already present in '_collections')
  }

  if (!create) {
    return nullptr;
  }

  arangodb::velocypack::Builder builder;

  builder.openObject();
  builder.add(
    arangodb::StaticStrings::DataSourceSystem,
    arangodb::velocypack::Value(true)
  ); // required to for use of VIEW_NAME_PREFIX
  builder.add(
    arangodb::StaticStrings::DataSourceName,
    toValuePair(viewName)
  ); // mark the view definition as an internal per-cid instance
  builder.add(
    arangodb::StaticStrings::DataSourcePlanId,
    arangodb::velocypack::Value(id())
  ); // planId required for cluster-wide view lookup from per-cid view
  builder.add(
    arangodb::StaticStrings::DataSourceType,
    toValuePair(DATA_SOURCE_TYPE.name())
  ); // type required for proper factory selection

  {
    SCOPED_LOCK(_meta->read()); // '_meta' can be asynchronously updated

    if (!_meta->json(builder)) {
      builder.close(); // close StaticStrings::PropertiesField
      LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
        << "failure to generate properties definition while constructing IResearch View in database '" << vocbase().name() << "'";

      return nullptr;
    }
  }

  builder.close();
  view = vocbase().createView(builder.slice());
  impl = LogicalView::cast<IResearchView>(view.get());

  if (!impl) {
    LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
      << "failure while creating an IResearch View for collection '" << cid << "' in database '" << vocbase().name() << "'";

    return nullptr;
  }

  // FIXME should we register?
  _collections.emplace(cid, view);
  impl->updateProperties(_meta);

  // hold a reference to the original view in the deleter so that the view is still valid for the duration of the pointer wrapper
  // this shared_ptr should not be stored in TRI_vocbase_t since the deleter depends on 'this'
  return std::shared_ptr<arangodb::LogicalView>(
    view.get(),
    [this, view, cid](arangodb::LogicalView*)->void {
      // FIXME destructor has to be noexcept
      static const auto visitor = [](TRI_voc_cid_t)->bool { return false; };
      auto& vocbase = view->vocbase();

      // same view in vocbase and with no collections
      if (view.get() == vocbase.lookupView(view->id()).get() // avoid double dropView(...)
          && view->visitCollections(visitor)) {
        // FIXME TODO ensure somehow that 'this' is still valid
        drop(cid);
      }
    }
  );
}

/*static*/ std::shared_ptr<LogicalView> IResearchViewDBServer::make(
    TRI_vocbase_t& vocbase,
    arangodb::velocypack::Slice const& info,
    bool isNew,
    uint64_t planVersion,
    LogicalView::PreCommitCallback const& preCommit /*= {}*/
) {
  if (!info.isObject()) {
    LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
      << "non-object definition supplied while instantiating IResearch View in database '" << vocbase.name() << "'";

    return nullptr;
  }

  irs::string_ref name;
  bool seen;

  if (!getString(name, info, arangodb::StaticStrings::DataSourceName, seen, std::string())
      || !seen) {
    LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
      << "definition supplied without a 'name' while instantiating IResearch View in database '" << vocbase.name() << "'";

    return nullptr;
  }

  // not a per-cid view instance (get here from ClusterInfo)
  if (!irs::starts_with(name, VIEW_NAME_PREFIX)) {
    auto* feature = arangodb::application_features::ApplicationServer::lookupFeature<
      arangodb::DatabasePathFeature
    >("DatabasePath");

    if (!feature) {
      LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
        << "failure to find feature 'DatabasePath' while constructing IResearch View in database '" << vocbase.id() << "'";

      return nullptr;
    }

    auto* ci = ClusterInfo::instance();

    if (!ci) {
      LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
        << "failure to find ClusterInfo instance while constructing IResearch View in database '" << vocbase.id() << "'";
      TRI_set_errno(TRI_ERROR_INTERNAL);

      return nullptr;
    }

    auto wiew = std::shared_ptr<IResearchViewDBServer>(
      new IResearchViewDBServer(vocbase, info, *feature, planVersion)
    );

    auto& properties = info.isObject() ? info : emptyObjectSlice(); // if no 'info' then assume defaults
    std::string error;
    IResearchViewMeta meta;

    if (!meta.init(properties, error)) {
      LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
        << "failed to initialize IResearch view from definition, error: " << error;

      return nullptr;
    }

    // search for the previous view instance and check if it's meta is the same
    {
      auto oldLogicalWiew =
        ci->getViewCurrent(vocbase.name(), std::to_string(wiew->id()));
      auto* oldWiew =
        LogicalView::cast<IResearchViewDBServer>(oldLogicalWiew.get());

      if (oldWiew && *(oldWiew->_meta) == meta) {
        wiew->_meta = oldWiew->_meta;
      }
    }

    if (!(wiew->_meta)) {
      wiew->_meta = std::make_shared<AsyncMeta>();
      static_cast<IResearchViewMeta&>(*(wiew->_meta)) = std::move(meta);
    }

    if (preCommit && !preCommit(wiew)) {
      LOG_TOPIC(ERR, arangodb::iresearch::TOPIC)
        << "failure during pre-commit while constructing IResearch View in database '" << vocbase.id() << "'";

      return nullptr;
    }

    return wiew;
  }

  // ...........................................................................
  // a per-cid view instance (get here only from StorageEngine startup or WAL recovery)
  // ...........................................................................

  auto view = vocbase.lookupView(name);

  if (view) {
    return view;
  }

  auto* ci = ClusterInfo::instance();
  std::shared_ptr<AsyncMeta> meta;

  // reference meta from cluster-wide view if available to
  // avoid memory and thread allocation
  // if not availble then the meta will be reassigned when
  // the per-cid instance is associated with the cluster-wide view
  if (ci) {
    auto planId = arangodb::basics::VelocyPackHelper::stringUInt64(
      info.get(arangodb::StaticStrings::DataSourcePlanId)
    ); // planId set in ensure(...)
    auto wiewId = std::to_string(planId);
    auto logicalWiew = ci->getView(vocbase.name(), wiewId); // here if creating per-cid view during loadPlan()
    auto* wiew = LogicalView::cast<IResearchViewDBServer>(logicalWiew.get());

    // if not found in 'Plan' then search in 'Current'
    if (!wiew) {
      logicalWiew = ci->getViewCurrent(vocbase.name(), wiewId); // here if creating per-cid view outisde of loadPlan()
      wiew = LogicalView::cast<IResearchViewDBServer>(logicalWiew.get());
    }

    if (wiew) {
      meta = wiew->_meta;
    }
  }

  // no view for shard
  view = IResearchView::make(vocbase, info, isNew, planVersion, preCommit);

  if (!view
      || (meta && !LogicalView::cast<IResearchView>(*view).updateProperties(meta).ok())) {
    LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
      << "failure while creating an IResearch View '" << name << "' in database '" << vocbase.name() << "'";

    return nullptr;
  }

  // a wrapper to remove the view from vocbase if it no longer has any links
  // hold a reference to the original view in the deleter so that the view is
  // still valid for the duration of the pointer wrapper
  return std::shared_ptr<arangodb::LogicalView>(
    view.get(),
    [view](arangodb::LogicalView*)->void {
      static const auto visitor = [](TRI_voc_cid_t)->bool { return false; };
      auto& vocbase = view->vocbase();

      // same view in vocbase and with no collections
      if (view.get() == vocbase.lookupView(view->id()).get() // avoid double dropView(...)
          && view->visitCollections(visitor)
          && !vocbase.dropView(view->id(), true).ok()) { // per-cid collections always system
        LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
          << "failure to drop stale IResearchView '" << view->name() << "' while from database '" << vocbase.name() << "'";
      }
    }
  );
}

void IResearchViewDBServer::open() {
  ReadMutex mutex(_mutex);
  SCOPED_LOCK(mutex); // 'collections_' can be asynchronously modified

  for (auto& entry: _collections) {
    entry.second->open();
  }
}

arangodb::Result IResearchViewDBServer::rename(
    std::string&& newName,
    bool /*doSync*/
) {
  name(std::move(newName));

  return arangodb::Result();
}

PrimaryKeyIndexReader* IResearchViewDBServer::snapshot(
    transaction::Methods& trx,
    std::vector<std::string> const& shards,
    IResearchView::Snapshot mode /*= IResearchView::Snapshot::Find*/
) const {
  auto* state = trx.state();

  if (!state) {
    LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
      << "failed to get transaction state while creating IResearchView snapshot";

    return nullptr;
  }

  // TODO FIXME find a better way to look up a ViewState
  #ifdef ARANGODB_ENABLE_MAINTAINER_MODE
    auto* cookie = dynamic_cast<ViewState*>(state->cookie(this));
  #else
    auto* cookie = static_cast<ViewState*>(state->cookie(this));
  #endif

  switch (mode) {
    case IResearchView::Snapshot::Find:
      return cookie ? &cookie->_snapshot : nullptr;
    case IResearchView::Snapshot::FindOrCreate:
      if (cookie) {
        return &cookie->_snapshot;
      }
      break;
    default:
      break;
  }

  auto* resolver = trx.resolver();

  if (!resolver) {
    LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
      << "failed to retrieve CollectionNameResolver from the transaction";

    return nullptr;
  }

  std::unique_ptr<ViewState> cookiePtr;
  CompoundReader* reader = nullptr;

  if (!cookie) {
    cookiePtr = irs::memory::make_unique<ViewState>();
    reader = &cookiePtr->_snapshot;
  } else {
    reader = &cookie->_snapshot;
    reader->clear();
  }

  TRI_ASSERT(reader);

  ReadMutex mutex(_mutex);
  SCOPED_LOCK(mutex); // 'collections_' can be asynchronously modified

  try {
    for (auto& shardId : shards) {
      auto shard = resolver->getCollection(shardId);

      if (!shard) {
        LOG_TOPIC(ERR, arangodb::iresearch::TOPIC)
          << "failed to find shard by id '" << shardId << "', skipping it";
        continue;
      }

      auto cid = shard->id();
      auto const shardView = _collections.find(cid);

      if (shardView == _collections.end()) {
        LOG_TOPIC(ERR, arangodb::iresearch::TOPIC)
          << "failed to find shard view for shard id '" << cid << "', skipping it";
        continue;
      }

      auto* rdr = LogicalView::cast<IResearchView>(*shardView->second).snapshot(trx, mode);

      if (rdr) {
        reader->add(*rdr);
      }
    }
  } catch (arangodb::basics::Exception& e) {
    LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
      << "caught exception while collecting readers for snapshot of DBServer IResearch view '" << id()
      << "': " << e.code() << " " << e.what();
    IR_LOG_EXCEPTION();

    return nullptr;
  } catch (std::exception const& e) {
    LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
      << "caught exception while collecting readers for snapshot of DBServer IResearch view '" << id()
      << "': " << e.what();
    IR_LOG_EXCEPTION();

    return nullptr;
  } catch (...) {
    LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
      << "caught exception while collecting readers for snapshot of DBServer IResearch view '" << id() << "'";
    IR_LOG_EXCEPTION();

    return nullptr;
  }

  if (cookiePtr) {
    state->cookie(this, std::move(cookiePtr));
  }

  return reader;
}

arangodb::Result IResearchViewDBServer::updateProperties(
  arangodb::velocypack::Slice const& slice,
  bool partialUpdate,
  bool doSync
) {
  if (!slice.isObject()) {
    return arangodb::Result(
      TRI_ERROR_BAD_PARAMETER,
      std::string("invalid properties supplied while updating IResearch View in database '") + vocbase().name() + "'"
    );
  }

  // ...........................................................................
  // sanitize update slice
  // ...........................................................................

  static const std::function<bool(irs::string_ref const& key)> propsAcceptor = [](
      irs::string_ref const& key
  )->bool {
    return key != StaticStrings::LinksField; // ignored fields
  };
  arangodb::velocypack::Builder props;

  props.openObject();

  if (!mergeSliceSkipKeys(props, slice, propsAcceptor)) {
    return arangodb::Result(
      TRI_ERROR_INTERNAL,
      std::string("failure to generate definition while updating IResearch View in database '") + vocbase().name() + "'"
    );
  }

  props.close();

  IResearchViewMeta meta;
  std::string error;

  if (partialUpdate) {
    SCOPED_LOCK(_meta->read());

    if (!meta.init(props.slice(), error, *_meta)) {
      return arangodb::Result(
        TRI_ERROR_BAD_PARAMETER,
        std::string("failure parsing properties while updating IResearch View in database '") + vocbase().name() + "'"
      );
    }
  } else if (!meta.init(props.slice(), error)) {
    return arangodb::Result(
      TRI_ERROR_BAD_PARAMETER,
      std::string("failure parsing properties while updating IResearch View in database '") + vocbase().name() + "'"
    );
  }

  WriteMutex mutex(_mutex);
  SCOPED_LOCK(mutex); // '_collections' can be asynchronously read

  {
    SCOPED_LOCK(_meta->write());

    // reset non-updatable values to match current meta
    meta._locale = _meta->_locale;

    static_cast<IResearchViewMeta&>(*_meta) = std::move(meta);
  }

  auto* feature = arangodb::application_features::ApplicationServer::lookupFeature<
    arangodb::iresearch::IResearchFeature
  >();

  if (feature) {
    feature->asyncNotify();
  }

  if (!slice.hasKey(StaticStrings::LinksField) && partialUpdate) {
    return arangodb::Result();
  }

  // ...........................................................................
  // update links if requested (on a best-effort basis)
  // ...........................................................................

  std::unordered_set<TRI_voc_cid_t> collections;
  auto links = slice.hasKey(StaticStrings::LinksField)
             ? slice.get(StaticStrings::LinksField)
             : arangodb::velocypack::Slice::emptyObjectSlice(); // used for !partialUpdate


  if (partialUpdate) {
    return IResearchLinkHelper::updateLinks(
      collections, vocbase(), *this, links
    );
  }

  std::unordered_set<TRI_voc_cid_t> stale;

  for (auto& entry: _collections) {
    stale.emplace(entry.first);
  }

  return IResearchLinkHelper::updateLinks(
    collections, vocbase(), *this, links, stale
  );
}

bool IResearchViewDBServer::visitCollections(
    CollectionVisitor const& visitor
) const {
  ReadMutex mutex(_mutex);
  SCOPED_LOCK(mutex); // 'collections_' can be asynchronously modified

  for (auto& entry: _collections) {
    if (!visitor(entry.first)) {
      return false;
    }
  }

  return true;
}

} // iresearch
} // arangodb

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------
