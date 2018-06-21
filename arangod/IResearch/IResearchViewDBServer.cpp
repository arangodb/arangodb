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
#include "IResearchLinkHelper.h"
#include "IResearchView.h"
#include "IResearchViewDBServer.h"
#include "VelocyPackHelper.h"
#include "Basics/StaticStrings.h"
#include "Logger/Logger.h"
#include "Logger/LogMacros.h"
#include "RestServer/DatabasePathFeature.h"
#include "RestServer/ViewTypesFeature.h"
#include "StorageEngine/TransactionState.h"
#include "Transaction/Methods.h"
#include "VocBase/vocbase.h"

NS_LOCAL

typedef irs::async_utils::read_write_mutex::read_mutex ReadMutex;
typedef irs::async_utils::read_write_mutex::write_mutex WriteMutex;

////////////////////////////////////////////////////////////////////////////////
/// @brief the view name prefix of per-cid view instances
////////////////////////////////////////////////////////////////////////////////
std::string const VIEW_NAME_PREFIX("_iresearch_");

////////////////////////////////////////////////////////////////////////////////
/// @brief a key in the jSON definition that differentiates a view-cid container
///        from individual per-cid view implementation
///        (view types are identical)
////////////////////////////////////////////////////////////////////////////////
std::string const VIEW_CONTAINER_MARKER("master");

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
std::string generateName(
    std::string const& viewName,
    TRI_voc_cid_t viewId,
    TRI_voc_cid_t collectionId
) {
  return VIEW_NAME_PREFIX
   + std::to_string(collectionId)
   + "_" + std::to_string(viewId)
   + "_" + viewName
   ;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief compute the data path to user for iresearch persisted-store
///        get base path from DatabaseServerFeature (similar to MMFilesEngine)
///        the path is hardcoded to reside under:
///        <DatabasePath>/<IResearchView::type()>-<view id>
///        similar to the data path calculation for collections
////////////////////////////////////////////////////////////////////////////////
irs::utf8_path getPersistedPath(
    arangodb::DatabasePathFeature const& dbPathFeature, TRI_voc_cid_t id
) {
  irs::utf8_path dataPath(dbPathFeature.directory());
  static const std::string subPath("databases");

  dataPath /= subPath;
  dataPath /= arangodb::iresearch::DATA_SOURCE_TYPE.name();
  dataPath += "-";
  dataPath += std::to_string(id);

  return dataPath;
}

NS_END

NS_BEGIN(arangodb)
NS_BEGIN(iresearch)

IResearchViewDBServer::IResearchViewDBServer(
    TRI_vocbase_t& vocbase,
    arangodb::velocypack::Slice const& info,
    arangodb::DatabasePathFeature const& dbPathFeature,
    uint64_t planVersion
): LogicalView(vocbase, info, planVersion),
   _meta(
     info.isObject() && info.get(StaticStrings::PropertiesField).isObject()
     ? info.get(StaticStrings::PropertiesField) : emptyObjectSlice()
   ),
   _persistedPath(getPersistedPath(dbPathFeature, id())) {

  auto* viewPtr = this;

  // initialize transaction read callback
  _trxReadCallback = [viewPtr](
      arangodb::transaction::Methods& trx,
      arangodb::transaction::Status status
  )->void {
    auto* state = trx.state();

    if (!state || arangodb::transaction::Status::RUNNING != status) {
      return; // NOOP
    }

    viewPtr->snapshot(*state, true);
  };
}

IResearchViewDBServer::~IResearchViewDBServer() {
  _collections.clear(); // ensure view distructors called before mutex is deallocated
}

arangodb::Result IResearchViewDBServer::appendVelocyPack(
  arangodb::velocypack::Builder& builder,
  bool detailed,
  bool //forPersistence
) const {
  if (!builder.isOpenObject()) {
    return arangodb::Result(
      TRI_ERROR_BAD_PARAMETER,
      std::string("invalid builder provided for IResearchViewDBServer definition")
    );
  }

  builder.add(
    arangodb::StaticStrings::DataSourceType,
    arangodb::velocypack::Value(type().name())
  );

  if (!detailed) {
    return arangodb::Result();
  }

  static const std::function<bool(irs::string_ref const& key)> acceptor = [](
      irs::string_ref const& key
  )->bool {
    // ignored fields
    return key != StaticStrings::CollectionsField
      && key != StaticStrings::LinksField;
  };
  ReadMutex mutex(_mutex);
  SCOPED_LOCK(mutex); // '_collections'/'_meta' can be asynchronously modified

  builder.add(
    StaticStrings::PropertiesField,
    arangodb::velocypack::Value(arangodb::velocypack::ValueType::Object)
  );

  {
    builder.add(
      StaticStrings::CollectionsField,
      arangodb::velocypack::Value(arangodb::velocypack::ValueType::Array)
    );

    for (auto& entry: _collections) {
      builder.add(arangodb::velocypack::Value(entry.first));
    }

    builder.close(); // close COLLECTIONS_FIELD
  }

  if (!mergeSliceSkipKeys(builder, _meta.slice(), acceptor)) {
    LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
      << "failure to generate definition while generating properties jSON IResearch View in database '" << vocbase().name() << "'";
  }

  builder.close(); // close PROPERTIES_FIELD

  return arangodb::Result();
}

bool IResearchViewDBServer::apply(arangodb::transaction::Methods& trx) {
  // called from IResearchView when this view is added to a transaction
  return trx.addStatusChangeCallback(&_trxReadCallback); // add shapshot
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

arangodb::Result IResearchViewDBServer::drop(TRI_voc_cid_t cid) {
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
}

std::shared_ptr<arangodb::LogicalView> IResearchViewDBServer::ensure(
    TRI_voc_cid_t cid
) {
  WriteMutex mutex(_mutex);
  SCOPED_LOCK(mutex); // 'collections_' can be asynchronously read
  auto itr = _collections.find(cid);

  if (itr != _collections.end()) {
    return itr->second;
  }

  static const std::function<bool(irs::string_ref const& key)> acceptor = [](
      irs::string_ref const& key
  )->bool {
    return key != StaticStrings::CollectionsField
      && key != StaticStrings::LinksField; // ignored fields
  };
  arangodb::velocypack::Builder builder;

  builder.openObject();
  builder.add(
    arangodb::StaticStrings::DataSourceSystem,
    arangodb::velocypack::Value(true)
  ); // required to for use of VIEW_NAME_PREFIX
  builder.add(
    arangodb::StaticStrings::DataSourceName,
    toValuePair(generateName(name(), id(), cid))
  ); // mark the view definition as an internal per-cid instance
  builder.add(
    arangodb::StaticStrings::DataSourceType,
    toValuePair(DATA_SOURCE_TYPE.name())
  ); // type required for proper factory selection

  {
    builder.add(
      StaticStrings::PropertiesField,
      arangodb::velocypack::Value(arangodb::velocypack::ValueType::Object)
    );

    if (!mergeSliceSkipKeys(builder, _meta.slice(), acceptor)) {
      LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
        << "failure to generate properties definition while constructing IResearch View in database '" << vocbase().id() << "'";

        return nullptr;
    }

    builder.close(); // close StaticStrings::PropertiesField
  }

  builder.close();

  auto view = vocbase().createView(builder.slice());

  if (!view) {
    LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
      << "failure while creating an IResearch View for collection '" << cid << "' in database '" << vocbase().name() << "'";

    return nullptr;
  }

  // hold a reference to the original view in the deleter so that the view is still valid for the duration of the pointer wrapper
  return std::shared_ptr<arangodb::LogicalView>(
    view.get(),
    [this, view, cid](arangodb::LogicalView*)->void {
      static const auto visitor = [](TRI_voc_cid_t)->bool { return false; };

      // same view in vocbase and with no collections
      if (view.get() == vocbase().lookupView(view->id()).get() // avoid double dropView(...)
          && view->visitCollections(visitor)) {
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
    auto wiew = vocbase.lookupView(name);

    // DBServer view already exists, treat as an update
    if (wiew) {
      return wiew->updateProperties(info.get(StaticStrings::PropertiesField), false, true).ok() // 'false' because full view definition
        ? wiew : nullptr;
    }

    // if creation request not coming from the vocbase
    if (!info.hasKey(VIEW_CONTAINER_MARKER)) {
      arangodb::velocypack::Builder builder;

      builder.openObject();

      if (!mergeSlice(builder, info)) {
        LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
          << "failure to generate definition while constructing IResearch View in database '" << vocbase.id() << "'";

          return nullptr;
      }

      builder.add(
        VIEW_CONTAINER_MARKER,
        arangodb::velocypack::Value(arangodb::velocypack::ValueType::Null)
      );
      builder.close();

      return vocbase.createView(builder.slice());
    }

    auto* feature = arangodb::application_features::ApplicationServer::lookupFeature<
      arangodb::DatabasePathFeature
    >("DatabasePath");

    if (!feature) {
      LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
        << "failure to find feature 'DatabasePath' while constructing IResearch View in database '" << vocbase.id() << "'";

      return nullptr;
    }

    static const std::function<bool(irs::string_ref const& key)> acceptor = [](
        irs::string_ref const& key
    )->bool {
      return key != VIEW_CONTAINER_MARKER; // ignored internally injected filed
    };
    arangodb::velocypack::Builder builder;

    builder.openObject();

    if (!mergeSliceSkipKeys(builder, info, acceptor)) {
      LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
        << "failure to generate definition while constructing IResearch View in database '" << vocbase.name() << "'";

      return nullptr;
    }

    builder.close();

    PTR_NAMED(IResearchViewDBServer, view, vocbase, builder.slice(), *feature, planVersion);

    if (preCommit && !preCommit(view)) {
      LOG_TOPIC(ERR, arangodb::iresearch::TOPIC)
        << "failure during pre-commit while constructing IResearch View in database '" << vocbase.id() << "'";

      return nullptr;
    }

    return view;
  }

  // ...........................................................................
  // a per-cid view instance (get here only from StorageEngine startup or WAL recovery)
  // ...........................................................................

  // parse view names created by generateName(...)
  auto* begin = name.c_str() + VIEW_NAME_PREFIX.size();
  auto size = name.size() - VIEW_NAME_PREFIX.size();
  auto* end = (char*)::memchr(begin, '_', size);

  if (!end) {
    LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
      << "failed to determine collection ID while constructing IResearch View in database '" << vocbase.name() << "'";

    return nullptr;
  }

  irs::string_ref collectionId(begin, end - begin);

  begin = end + 1; // +1 for '_'
  size -= collectionId.size() + 1; // +1 for '_'
  end = (char*)::memchr(begin, '_', size);

  if (!end) {
    LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
      << "failed to determine view ID while constructing IResearch View in database '" << vocbase.name() << "'";

    return nullptr;
  }

  irs::string_ref viewId(begin, end - begin);

  begin = end + 1; // +1 for '_'
  size -= viewId.size() + 1; // +1 for '_'

  irs::string_ref viewName(begin, size);
  char* suffix;
  auto cid = std::strtoull(collectionId.c_str(), &suffix, 10); // 10 for base-10

  if (suffix != collectionId.c_str() + collectionId.size()) {
    LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
      << "failed to parse collection ID while constructing IResearch View in database '" << vocbase.name() << "'";

    return nullptr;
  }

  auto wiew = vocbase.lookupView(viewId); // always look up by view ID since it cannot change

  // create DBServer view
  if (!wiew) {
    static const std::function<bool(irs::string_ref const& key)> acceptor = [](
        irs::string_ref const& key
    )->bool {
      return key != arangodb::StaticStrings::DataSourceId
        && key != arangodb::StaticStrings::DataSourceSystem
        && key != arangodb::StaticStrings::DataSourceName; // ignored fields
    };
    arangodb::velocypack::Builder builder;

    builder.openObject();
    builder.add(
      arangodb::StaticStrings::DataSourceId,
      arangodb::velocypack::Value(viewId)
    );
    builder.add(
      arangodb::StaticStrings::DataSourceName,
      arangodb::velocypack::Value(viewName)
    );
    builder.add(// mark the view definition as a DBServer instance
      VIEW_CONTAINER_MARKER,
      arangodb::velocypack::Value(arangodb::velocypack::ValueType::Null)
    );

    if (!mergeSliceSkipKeys(builder, info, acceptor)) {
      LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
        << "failure to generate definition while constructing IResearch View in database '" << vocbase.name() << "'";

        return nullptr;
    }

    builder.close();
    wiew = vocbase.createView(builder.slice());
  }

  // TODO FIXME find a better way to look up an iResearch View
  auto* view = LogicalView::cast<IResearchViewDBServer>(wiew.get());

  if (!view) {
    LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
      << "failure while creating an IResearch View '" << std::string(name) << "' in database '" << vocbase.name() << "'";

    return nullptr;
  }

  auto impl = IResearchView::make(vocbase, info, isNew, planVersion, preCommit);

  if (!impl) {
    LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
      << "failure while creating an IResearch View for collection '" << cid << "' in database '" << vocbase.name() << "'";

    return nullptr;
  }

  WriteMutex mutex(view->_mutex);
  SCOPED_LOCK(mutex); // 'collections_' can be asynchronously read

  return view->_collections.emplace(cid, impl).first->second;
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
  ReadMutex mutex(_mutex);
  SCOPED_LOCK(mutex); // 'collections_' can be asynchronously modified

  for (auto& entry: _collections) {
    auto res = vocbase().renameView(
      entry.second, generateName(newName, id(), entry.first)
    );

    if (TRI_ERROR_NO_ERROR != res) {
      return res; // fail on first failure
    }
  }

  name(std::move(newName));

  return arangodb::Result();
}

PrimaryKeyIndexReader* IResearchViewDBServer::snapshot(
    TransactionState& state,
    bool force /*= false*/
) const {
  // TODO FIXME find a better way to look up a ViewState
  #ifdef ARANGODB_ENABLE_MAINTAINER_MODE
    auto* cookie = dynamic_cast<ViewState*>(state.cookie(this));
  #else
    auto* cookie = static_cast<ViewState*>(state.cookie(this));
  #endif

  if (cookie) {
    return &(cookie->_snapshot);
  }

  if (!force) {
    return nullptr;
  }

  auto cookiePtr = irs::memory::make_unique<ViewState>();
  auto& reader = cookiePtr->_snapshot;
  ReadMutex mutex(_mutex);
  SCOPED_LOCK(mutex); // 'collections_' can be asynchronously modified

  try {
    for (auto& entry: _collections) {
      auto* rdr =
        static_cast<IResearchView*>(entry.second.get())->snapshot(state, force);

      if (rdr) {
        reader.add(*rdr);
      }
    }
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

  state.cookie(this, std::move(cookiePtr));

  return &reader;
}

arangodb::Result IResearchViewDBServer::updateProperties(
  arangodb::velocypack::Slice const& properties,
  bool partialUpdate,
  bool doSync
) {
  if (!properties.isObject()) {
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
    return key != StaticStrings::CollectionsField && key != StaticStrings::LinksField; // ignored fields
  };
  arangodb::velocypack::Builder props;

  props.openObject();

  if (!mergeSliceSkipKeys(props, properties, propsAcceptor)) {
    return arangodb::Result(
      TRI_ERROR_INTERNAL,
      std::string("failure to generate definition while updating IResearch View in database '") + vocbase().name() + "'"
    );
  }

  props.close();

  IResearchViewMeta meta;
  std::string error;

  if (partialUpdate) {
    IResearchViewMeta oldMeta;

    if (!oldMeta.init(_meta.slice(), error)
        || !meta.init(props.slice(), error, oldMeta)) {
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
  SCOPED_LOCK(mutex); // '_meta' can be asynchronously read

  meta._collections.clear();

  for (auto& entry: _collections) {
    meta._collections.emplace(entry.first);
  }

  // ...........................................................................
  // prepare replacement '_meta'
  // ...........................................................................

  arangodb::velocypack::Builder builder;

  builder.openObject();

  if (!meta.json(builder)) {
    return arangodb::Result(
      TRI_ERROR_INTERNAL,
      std::string("failure to generate 'properties' definition while updating IResearch View in database '") + vocbase().name() + "'"
    );
  }

  builder.close();

  // ...........................................................................
  // update per-cid views
  // ...........................................................................

  for (auto& entry: _collections) {
    auto res =
      entry.second->updateProperties(props.slice(), partialUpdate, doSync);

    if (!res.ok()) {
      return res; // fail on first failure
    }
  }

  _meta = std::move(builder);

  if (!properties.hasKey(StaticStrings::LinksField)) {
    return arangodb::Result();
  }

  // ...........................................................................
  // update links if requested (on a best-effort basis)
  // ...........................................................................

  std::unordered_set<TRI_voc_cid_t> collections;
  auto links = properties.get(StaticStrings::LinksField);

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

NS_END // iresearch
NS_END // arangodb

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------