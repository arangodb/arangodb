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
#include "RestServer/ViewTypesFeature.h"
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
/// @brief index reader implementation over multiple irs::index_reader
////////////////////////////////////////////////////////////////////////////////
class CompoundReader final: public irs::index_reader {
 public:
  irs::sub_reader const& operator[](
      size_t subReaderId
  ) const noexcept override {
    TRI_ASSERT(subReaderId < _subReaders.size());
    return *(_subReaders[subReaderId]);
  }

  void add(irs::index_reader const& reader) {
    for(auto& entry: reader) {
      _subReaders.emplace_back(&entry);
    }
  }

  void clear() noexcept { _subReaders.clear(); }
  virtual uint64_t docs_count() const override;
  virtual uint64_t live_docs_count() const override;
  virtual size_t size() const noexcept override { return _subReaders.size(); }

 private:
  std::vector<irs::sub_reader const*> _subReaders;
};

uint64_t CompoundReader::docs_count() const {
  uint64_t count = 0;

  for (auto& entry: _subReaders) {
    count += entry->docs_count();
  }

  return count;
}

uint64_t CompoundReader::live_docs_count() const {
  uint64_t count = 0;

  for (auto& entry: _subReaders) {
    count += entry->live_docs_count();
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

////////////////////////////////////////////////////////////////////////////////
/// @brief IResearchView-specific implementation of a ViewFactory
////////////////////////////////////////////////////////////////////////////////
struct IResearchViewDBServer::ViewFactory: public arangodb::ViewFactory {
  virtual arangodb::Result create(
      arangodb::LogicalView::ptr& view,
      TRI_vocbase_t& vocbase,
      arangodb::velocypack::Slice const& definition
  ) const override {
    auto* ci = ClusterInfo::instance();

    if (!ci) {
      return arangodb::Result(
        TRI_ERROR_INTERNAL,
        std::string("failure to find 'ClusterInfo' instance while creating arangosearch View in database '") + vocbase.name() + "'"
      );
    }

    arangodb::LogicalView::ptr impl;
    auto res = instantiate(impl, vocbase, definition, 0);

    if (!res.ok()) {
      return res;
    }

    if (!impl) {
      return arangodb::Result(
        TRI_ERROR_INTERNAL,
        std::string("failure during instantiation while creating arangosearch View in database '") + vocbase.name() + "'"
      );
    }

    arangodb::velocypack::Builder builder;

    builder.openObject();
    res = impl->properties(builder, true, true); // include links so that Agency will always have a full definition

    if (!res.ok()) {
      return res;
    }

    builder.close();

    std::string error;
    auto resNum = ci->createViewCoordinator(
      vocbase.name(), std::to_string(impl->id()), builder.slice(), error
    );

    if (TRI_ERROR_NO_ERROR != resNum) {
      if (error.empty()) {
        error = TRI_errno_string(resNum);
      }

      return arangodb::Result(
        resNum,
        std::string("failure during ClusterInfo persistance of created view while creating arangosearch View in database '") + vocbase.name() + "', error: " + error
      );
    }

    // NOTE: link creation is ignored since on the db-server links are created
    //       by their LogicalCollections themselves

    view = ci->getView(vocbase.name(), std::to_string(impl->id())); // refresh view from Agency

    if (view) {
      view->open(); // open view to match the behaviour in StorageEngine::openExistingDatabase(...) and original behaviour of TRI_vocbase_t::createView(...)
    }

    return arangodb::Result();
  }

  virtual arangodb::Result instantiate(
      arangodb::LogicalView::ptr& view,
      TRI_vocbase_t& vocbase,
      arangodb::velocypack::Slice const& definition,
      uint64_t planVersion
  ) const override {
    irs::string_ref name;
    bool seen;

    if (!getString(name, definition, arangodb::StaticStrings::DataSourceName, seen, irs::string_ref::EMPTY)
        || !seen) {
      return arangodb::Result(
        TRI_ERROR_BAD_PARAMETER,
        std::string("definition supplied without a 'name' while instantiating arangosearch View in database '") + vocbase.name() + "'"
      );
    }

    // not a per-cid view instance (get here from ClusterInfo)
    if (!irs::starts_with(name, VIEW_NAME_PREFIX)) {
      auto* feature = arangodb::application_features::ApplicationServer::lookupFeature<
        arangodb::DatabasePathFeature
      >("DatabasePath");

      if (!feature) {
        return arangodb::Result(
          TRI_ERROR_INTERNAL,
          std::string("failure to find feature 'DatabasePath' while constructing arangosearch View in database '") + vocbase.name() + "'"
        );
      }

      auto* ci = ClusterInfo::instance();

      if (!ci) {
        return arangodb::Result(
          TRI_ERROR_INTERNAL,
          std::string("failure to find 'ClusterInfo' instance while constructing arangosearch View in database '") + vocbase.name() + "'"
        );
      }

      std::string error;
      auto meta = std::make_shared<AsyncMeta>();

      if (!meta->init(definition, error)) {
        return arangodb::Result(
          TRI_ERROR_BAD_PARAMETER,
          error.empty()
          ? (std::string("failed to initialize arangosearch View '") + static_cast<std::string>(name) + "' from definition: " + definition.toString())
          : (std::string("failed to initialize arangosearch View '") + static_cast<std::string>(name) + "' from definition, error in attribute '" + error + "': " + definition.toString())
        );
      }

      view = std::shared_ptr<IResearchViewDBServer>(
        new IResearchViewDBServer(vocbase, definition, *feature, planVersion, std::move(meta))
      );

      return arangodb::Result();
    }

    // .........................................................................
    // a per-cid view instance
    // get here only from StorageEngine startup or WAL recovery
    // .........................................................................

    view = vocbase.lookupView(name);

    if (view) {
      return arangodb::Result(); // resuse view from vocbase
    }

    // no view for shard
    arangodb::LogicalView::ptr impl;
    auto res = IResearchView::factory().instantiate(
      impl, vocbase, definition, planVersion
    );

    if (!res.ok()) {
      return res;
    }

    if (!impl) {
      return arangodb::Result(
        TRI_ERROR_INTERNAL,
        std::string("failure during instantiation while creating an arangosearch View '") + std::string(name) + "' in database '" + vocbase.name() + "'"
      );
    }

    // a wrapper to remove the view from vocbase if it no longer has any links
    // hold a reference to the original view in the deleter so that the view is
    // still valid for the duration of the pointer wrapper
    view = std::shared_ptr<arangodb::LogicalView>(
      impl.get(),
      [impl] (arangodb::LogicalView*) noexcept -> void {
        auto& vocbase = impl->vocbase();

        // suppress any errors in destructor

        try {
          // same view in vocbase and with no collections
          if (impl == vocbase.lookupView(impl->id()) // avoid double dropView(...)
              && impl->visitCollections([](TRI_voc_cid_t){ return false; })
              && !impl->drop().ok()) { // per-cid collections always system
            LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
              << "failure to drop stale arangosearch View '" << impl->name() << "' while from database '" << vocbase.name() << "'";
          }
        } catch (basics::Exception const& e) {
          LOG_TOPIC(ERR, arangodb::iresearch::TOPIC)
            << "caught exception while dropping stale arangosearch View '" << impl->name()
            << "' while from database '" << vocbase.name()
            << "', errorCode: '" << e.code()
            << "', error: '" << e.message();
        } catch (std::exception const& e) {
          LOG_TOPIC(ERR, arangodb::iresearch::TOPIC)
            << "caught exception while dropping stale arangosearch View '" << impl->name()
            << "' while from database '" << vocbase.name()
            << "', error: '" << e.what() << "'";
        } catch (...) {
          LOG_TOPIC(ERR, arangodb::iresearch::TOPIC)
            << "caught an unspecified exception while dropping stale arangosearch View '" << impl->name()
            << "' while from database '" << vocbase.name() << "'";
        }
      }
    );

    return arangodb::Result();
  }
};

IResearchViewDBServer::IResearchViewDBServer(
    TRI_vocbase_t& vocbase,
    arangodb::velocypack::Slice const& info,
    arangodb::DatabasePathFeature const& /*dbPathFeature*/,
    uint64_t planVersion,
    std::shared_ptr<AsyncMeta> meta /*=nullptr*/
) : LogicalViewClusterInfo(vocbase, info, planVersion),
    _meta(std::move(meta)) {
}

IResearchViewDBServer::~IResearchViewDBServer() noexcept {
  _collections.clear(); // ensure view distructors called before mutex is deallocated
}

arangodb::Result IResearchViewDBServer::appendVelocyPackDetailed(
    arangodb::velocypack::Builder& builder,
    bool forPersistence
) const {
  if (!builder.isOpenObject()) {
    return arangodb::Result(
      TRI_ERROR_BAD_PARAMETER,
      std::string("invalid builder provided for IResearchViewDBServer definition")
    );
  }

  {
    SCOPED_LOCK(_meta->read()); // '_meta' can be asynchronously updated

    static const std::function<bool(irs::string_ref const& key)> acceptor = [](
        irs::string_ref const& key
    )->bool {
      return key != StaticStrings::VersionField; // ignored fields
    };
    static const std::function<bool(irs::string_ref const& key)> persistenceAcceptor = [](
        irs::string_ref const&
    )->bool {
      return true;
    };
    arangodb::velocypack::Builder sanitizedBuilder;

    sanitizedBuilder.openObject();

    if (!_meta->json(sanitizedBuilder)
        || !mergeSliceSkipKeys(builder, sanitizedBuilder.close().slice(), forPersistence ? persistenceAcceptor : acceptor)) {
      return arangodb::Result(
        TRI_ERROR_INTERNAL,
        std::string("failure to generate definition while generating properties jSON for arangosearch view in database '") + vocbase().name() + "'"
      );
    }
  }

  return arangodb::Result();
}


arangodb::Result IResearchViewDBServer::dropImpl() {
  WriteMutex mutex(_mutex);
  SCOPED_LOCK(mutex); // 'collections_' can be asynchronously read

  for (auto itr = _collections.begin(); itr != _collections.end();) {
    auto res = itr->second->drop();

    if (!res.ok()) {
      return res; // fail on first failure
    }

    itr = _collections.erase(itr);
  }

  return arangodb::Result();
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
        << "failure to generate properties definition while constructing arangosearch view in database '" << vocbase().name() << "'";

      return nullptr;
    }
  }

  builder.close();
  view = vocbase().createView(builder.slice());
  impl = LogicalView::cast<IResearchView>(view.get());

  if (!impl) {
    LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
      << "failure while creating an arangosearch view for collection '" << cid << "' in database '" << vocbase().name() << "'";

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
        unlink(cid);
      }
    }
  );
}

/*static*/ arangodb::ViewFactory const& IResearchViewDBServer::factory() {
  static const ViewFactory factory;

  return factory;
}

void IResearchViewDBServer::open() {
  ReadMutex mutex(_mutex);
  SCOPED_LOCK(mutex); // 'collections_' can be asynchronously modified

  for (auto& entry: _collections) {
    entry.second->open();
  }
}

irs::index_reader const* IResearchViewDBServer::snapshot(
    transaction::Methods& trx,
    std::vector<std::string> const& shards,
    IResearchView::Snapshot mode /*= IResearchView::Snapshot::Find*/
) const {
  auto* state = trx.state();

  if (!state) {
    LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
      << "failed to get transaction state while creating arangosearch view snapshot";

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
      << "caught exception while collecting readers for snapshot of DBServer arangosearch view '" << id()
      << "': " << e.code() << " " << e.what();
    IR_LOG_EXCEPTION();

    return nullptr;
  } catch (std::exception const& e) {
    LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
      << "caught exception while collecting readers for snapshot of DBServer arangosearch view '" << id()
      << "': " << e.what();
    IR_LOG_EXCEPTION();

    return nullptr;
  } catch (...) {
    LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
      << "caught exception while collecting readers for snapshot of DBServer arangosearch view '" << id() << "'";
    IR_LOG_EXCEPTION();

    return nullptr;
  }

  if (cookiePtr) {
    state->cookie(this, std::move(cookiePtr));
  }

  return reader;
}

arangodb::Result IResearchViewDBServer::unlink(TRI_voc_cid_t cid) noexcept {
  try {
    WriteMutex mutex(_mutex);
    SCOPED_LOCK(mutex); // 'collections_' can be asynchronously read
    auto itr = _collections.find(cid);

    if (itr == _collections.end()) {
      return arangodb::Result(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND);
    }

    auto res = itr->second->drop();

    if (!res.ok()) {
      return res;
    }

    _collections.erase(itr);
  } catch (arangodb::basics::Exception const& e) {
    return arangodb::Result(
      e.code(),
      std::string("caught exception while unlinking collection '") + std::to_string(cid) + "' from arangosearch view '" + name() + "': " + e.what()
    );
  } catch (std::exception const& e) {
    return arangodb::Result(
      TRI_ERROR_INTERNAL,
      std::string("caught exception while unlinking collection '") + std::to_string(cid) + "' from arangosearch view '" + name() + "': " + e.what()
    );
  } catch (...) {
    return arangodb::Result(
      TRI_ERROR_INTERNAL,
      std::string("caught exception while unlinking collection '") + std::to_string(cid) + "' from arangosearch view '" + name() + "'"
    );
  }

  return arangodb::Result();
}

arangodb::Result IResearchViewDBServer::properties(
  arangodb::velocypack::Slice const& slice,
  bool partialUpdate
) {
  if (!slice.isObject()) {
    return arangodb::Result(
      TRI_ERROR_BAD_PARAMETER,
      std::string("invalid properties supplied while updating arangosearch view in database '") + vocbase().name() + "'"
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
      std::string("failure to generate definition while updating arangosearch view in database '") + vocbase().name() + "'"
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
        std::string("failure parsing properties while updating arangosearch view in database '") + vocbase().name() + "'"
      );
    }
  } else if (!meta.init(props.slice(), error)) {
    return arangodb::Result(
      TRI_ERROR_BAD_PARAMETER,
      std::string("failure parsing properties while updating arangosearch view in database '") + vocbase().name() + "'"
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