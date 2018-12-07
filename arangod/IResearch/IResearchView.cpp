//////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2017 EMC Corporation
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
/// Copyright holder is EMC Corporation
///
/// @author Andrey Abramov
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#include "IResearchCommon.h"
#include "IResearchFeature.h"
#include "IResearchLink.h"
#include "IResearchLinkHelper.h"
#include "VelocyPackHelper.h"

#include "Aql/AstNode.h"
#include "Aql/QueryCache.h"
#include "Basics/StaticStrings.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "StorageEngine/TransactionState.h"
#include "StorageEngine/StorageEngine.h"
#include "RestServer/DatabaseFeature.h"
#include "RestServer/FlushFeature.h"
#include "RestServer/ViewTypesFeature.h"
#include "Transaction/Methods.h"
#include "Transaction/StandaloneContext.h"
#include "Utils/ExecContext.h"
#include "VocBase/LogicalCollection.h"

#include "IResearchView.h"

namespace {

////////////////////////////////////////////////////////////////////////////////
/// @brief surrogate root for all queries without a filter
////////////////////////////////////////////////////////////////////////////////
arangodb::aql::AstNode ALL(arangodb::aql::AstNodeValue(true));

typedef irs::async_utils::read_write_mutex::read_mutex ReadMutex;
typedef irs::async_utils::read_write_mutex::write_mutex WriteMutex;

////////////////////////////////////////////////////////////////////////////////
/// @brief index reader implementation over multiple irs::index_reader
///        the container storing the view state for a given TransactionState
/// @note it is assumed that DBServer ViewState resides in the same
///       TransactionState as the IResearchView ViewState, therefore a separate
///       lock is not required to be held by the DBServer CompoundReader
////////////////////////////////////////////////////////////////////////////////
class ViewTrxState final
  : public arangodb::TransactionState::Cookie, public irs::index_reader {
 public:
  irs::sub_reader const& operator[](
      size_t subReaderId
  ) const noexcept override {
    TRI_ASSERT(subReaderId < _subReaders.size());
    return *(_subReaders[subReaderId]);
  }

  void add(arangodb::iresearch::IResearchLink::Snapshot&& snapshot);
  void clear() noexcept { _subReaders.clear(); _snapshots.clear(); }
  virtual uint64_t docs_count() const override;
  virtual uint64_t live_docs_count() const override;
  virtual size_t size() const noexcept override { return _subReaders.size(); }

 private:
  std::vector<arangodb::iresearch::IResearchLink::Snapshot> _snapshots; // prevent data-store deallocation (lock @ AsyncSelf)
  std::vector<irs::sub_reader const*> _subReaders;
};

void ViewTrxState::add(
    arangodb::iresearch::IResearchLink::Snapshot&& snapshot
) {
  for(auto& entry: static_cast<irs::index_reader const&>(snapshot)) {
    _subReaders.emplace_back(&entry);
  }

  _snapshots.emplace_back(std::move(snapshot));
}

uint64_t ViewTrxState::docs_count() const {
  uint64_t count = 0;

  for (auto& entry: _subReaders) {
    count += entry->docs_count();
  }

  return count;
}

uint64_t ViewTrxState::live_docs_count() const {
  uint64_t count = 0;

  for (auto& entry: _subReaders) {
    count += entry->live_docs_count();
  }

  return count;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generates user-friendly description of the specified view
////////////////////////////////////////////////////////////////////////////////
std::string toString(arangodb::iresearch::IResearchView const& view) {
  std::string str(arangodb::iresearch::DATA_SOURCE_TYPE.name());

  str += ":";
  str += std::to_string(view.id());

  return str;
}

////////////////////////////////////////////////////////////////////////////////
/// @returns 'Flush' feature from AppicationServer
////////////////////////////////////////////////////////////////////////////////
inline arangodb::FlushFeature* getFlushFeature() noexcept {
  return arangodb::application_features::ApplicationServer::lookupFeature<
    arangodb::FlushFeature
  >("Flush");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief persist view definition to the storage engine
///        if in-recovery then register a post-recovery lambda for persistence
/// @return success
////////////////////////////////////////////////////////////////////////////////
arangodb::Result persistProperties(
    arangodb::LogicalView const& view,
    arangodb::iresearch::IResearchView::AsyncViewPtr const& asyncSelf
) {
  auto* engine = arangodb::EngineSelectorFeature::ENGINE;

  if (!engine) {
    return arangodb::Result(
      TRI_ERROR_INTERNAL,
      std::string("failure to get storage engine while persisting definition for LogicalView '") + view.name() + "'"
    );
  }

  if (!engine->inRecovery()) {
    // change view throws exception on error
    try {
      engine->changeView(view.vocbase(), view, true);
    } catch (arangodb::basics::Exception& e) {
      IR_LOG_EXCEPTION();

      return arangodb::Result(
        e.code(),
        std::string("caught exception during persistance of properties for arangosearch View '") + view.name() + "': " + e.what()
      );
    } catch (std::exception const& e) {
      IR_LOG_EXCEPTION();

      return arangodb::Result(
        TRI_ERROR_INTERNAL,
        std::string("caught exception during persistance of properties for arangosearch View '") + view.name() + "': " + e.what()
      );
    } catch (...) {
      IR_LOG_EXCEPTION();

      return arangodb::Result(
        TRI_ERROR_INTERNAL,
        std::string("caught exception during persistance of properties for arangosearch View '") + view.name() + "'"
      );
    }

    return arangodb::Result();
  }

  auto* feature = arangodb::application_features::ApplicationServer::lookupFeature<
    arangodb::DatabaseFeature
  >("Database");

  if (!feature) {
    return arangodb::Result(
      TRI_ERROR_INTERNAL,
      std::string("failure to get 'Database' feature while persisting definition for LogicalView '") + view.name() + "'"
    );
  }

  return feature->registerPostRecoveryCallback(
    [&view, asyncSelf]()->arangodb::Result {
      auto* engine = arangodb::EngineSelectorFeature::ENGINE;

      if (!engine) {
        return arangodb::Result(
          TRI_ERROR_INTERNAL,
          std::string("failure to get storage engine while persisting definition for LogicalView")
        );
      }

      if (!asyncSelf) {
        return arangodb::Result(
          TRI_ERROR_INTERNAL,
          std::string("invalid view instance passed while persisting definition for LogicalView")
        );
      }

      SCOPED_LOCK(asyncSelf->mutex());

      if (!asyncSelf->get()) {
        LOG_TOPIC(INFO, arangodb::iresearch::TOPIC)
          << "no view instance available while persisting definition for LogicalView";

        return arangodb::Result(); // nothing to persist, view allready deallocated
      }

      // change view throws exception on error
      try {
        engine->changeView(view.vocbase(), view, true);
      } catch (arangodb::basics::Exception& e) {
        IR_LOG_EXCEPTION();

        return arangodb::Result(
          e.code(),
          std::string("caught exception during persistance of properties for arangosearch View '") + view.name() + "': " + e.what()
        );
      } catch (std::exception const& e) {
        IR_LOG_EXCEPTION();

        return arangodb::Result(
          TRI_ERROR_INTERNAL,
          std::string("caught exception during persistance of properties for arangosearch View '") + view.name() + "': " + e.what()
        );
      } catch (...) {
        IR_LOG_EXCEPTION();

        return arangodb::Result(
          TRI_ERROR_INTERNAL,
          std::string("caught exception during persistance of properties for arangosearch View '") + view.name() + "'"
        );
      }

      return arangodb::Result();
    }
  );
}

}

namespace arangodb {
namespace iresearch {

////////////////////////////////////////////////////////////////////////////////
/// @brief IResearchView-specific implementation of a ViewFactory
////////////////////////////////////////////////////////////////////////////////
struct IResearchView::ViewFactory: public arangodb::ViewFactory {
  virtual arangodb::Result create(
      arangodb::LogicalView::ptr& view,
      TRI_vocbase_t& vocbase,
      arangodb::velocypack::Slice const& definition
  ) const override {
    auto* engine = arangodb::EngineSelectorFeature::ENGINE;
    auto& properties = definition.isObject()
                     ? definition
                     : arangodb::velocypack::Slice::emptyObjectSlice(); // if no 'info' then assume defaults
    auto links = properties.hasKey(StaticStrings::LinksField)
               ? properties.get(StaticStrings::LinksField)
               : arangodb::velocypack::Slice::emptyObjectSlice();
    auto res = engine && engine->inRecovery()
             ? arangodb::Result() // do not validate if in recovery
             : IResearchLinkHelper::validateLinks(vocbase, links);

    if (!res.ok()) {
      return res;
    }

    TRI_set_errno(TRI_ERROR_NO_ERROR); // reset before calling createView(...)
    auto impl = vocbase.createView(definition);

    if (!impl) {
      return arangodb::Result(
        TRI_ERROR_NO_ERROR == TRI_errno() ? TRI_ERROR_INTERNAL : TRI_errno(),
        std::string("failure during instantiation while creating arangosearch View in database '") + vocbase.name() + "'"
      );
    }

    // create links on a best-effor basis
    // link creation failure does not cause view creation failure
    try {
      std::unordered_set<TRI_voc_cid_t> collections;

      res = IResearchLinkHelper::updateLinks(
        collections, vocbase, *impl, links
      );

      if (!res.ok()) {
        LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
          << "failed to create links while creating arangosearch view '" << impl->name() <<  "': " << res.errorNumber() << " " <<  res.errorMessage();
      }
    } catch (arangodb::basics::Exception const& e) {
      IR_LOG_EXCEPTION();
      LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
        << "caught exception while creating links while creating arangosearch view '" << impl->name() << "': " << e.code() << " " << e.what();
    } catch (std::exception const& e) {
      IR_LOG_EXCEPTION();
      LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
        << "caught exception while creating links while creating arangosearch view '" << impl->name() << "': " << e.what();
    } catch (...) {
      IR_LOG_EXCEPTION();
      LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
        << "caught exception while creating links while creating arangosearch view '" << impl->name() << "'";
    }

    view = impl;

    return arangodb::Result();
  }

  virtual arangodb::Result instantiate(
      arangodb::LogicalView::ptr& view,
      TRI_vocbase_t& vocbase,
      arangodb::velocypack::Slice const& definition,
      uint64_t planVersion
  ) const override {
    std::string error;
    auto impl = std::shared_ptr<IResearchView>(
      new IResearchView(vocbase, definition, planVersion)
    );
    auto meta = std::make_shared<AsyncMeta>();
    IResearchViewMetaState metaState;

    if (!meta->init(definition, error)
        || meta->_version == 0 // version 0 must be upgraded to split data-store on a per-link basis
        || meta->_version > LATEST_VERSION
        || !metaState.init(definition, error)) {
      return arangodb::Result(
        TRI_ERROR_BAD_PARAMETER,
        error.empty()
        ? (std::string("failed to initialize arangosearch View '") + impl->name() + "' from definition: " + definition.toString())
        : (std::string("failed to initialize arangosearch View '") + impl->name() + "' from definition, error in attribute '" + error + "': " + definition.toString())
      );
    }

    // NOTE: for single-server must have full list of collections to lock
    //       for cluster the shards to lock come from coordinator, so ignore pre-loading
    if (ServerState::instance()->isSingleServer()) {
      for (auto cid: metaState._collections) {
        auto collection = vocbase.lookupCollection(cid); // always look up in vocbase (single server or cluster per-shard collection)
        auto link = collection
                  ? IResearchLinkHelper::find(*collection, *impl)
                  : nullptr
                  ; // add placeholders to links, when the collection comes up it'll bring up the link

        impl->_links.emplace(cid, link ? link->self() : nullptr); // add placeholders to links, when the link comes up it'll call link(...)
      }
    }

    auto res = impl->updateProperties(meta); // update separately since per-instance async jobs already started

    if (!res.ok()) {
      return res;
    }

    view = impl;

    return arangodb::Result();
  }
};

IResearchView::IResearchView(
    TRI_vocbase_t& vocbase,
    arangodb::velocypack::Slice const& info,
    uint64_t planVersion
): LogicalViewStorageEngine(vocbase, info, planVersion),
   FlushTransaction(toString(*this)),
   _asyncFeature(nullptr),
   _asyncSelf(irs::memory::make_unique<AsyncViewPtr::element_type>(this)),
   _meta(std::make_shared<AsyncMeta>()),
   _asyncTerminate(false),
   _inRecovery(false) {
  _asyncFeature = arangodb::application_features::ApplicationServer::lookupFeature<
    arangodb::iresearch::IResearchFeature
  >();

  // add asynchronous commit tasks
  if (_asyncFeature) {
    struct State: public IResearchViewMeta {
      size_t _cleanupIntervalCount{ 0 };
      std::chrono::system_clock::time_point _last{ std::chrono::system_clock::now() };
      decltype(IResearchView::_links) _links;
      irs::merge_writer::flush_progress_t _progress;
    } state;

    state._progress = [this]()->bool { return !_asyncTerminate.load(); };
    _asyncFeature->async(
      _asyncSelf,
      [this, state](size_t& timeoutMsec, bool) mutable ->bool {
        if (_asyncTerminate.load()) {
          return false; // termination requested
        }

        // reload meta
        {
          auto meta = std::atomic_load(&_meta);
          SCOPED_LOCK(meta->read());
          auto& stateMeta = static_cast<IResearchViewMeta&>(state);

          if (stateMeta != *meta) {
            stateMeta = *meta;
          }
        }

        // reload metaState
        {
          ReadMutex mutex(_mutex); // '_links' can be asynchronously modified
          SCOPED_LOCK(mutex);
          state._links = _links;
        }

        if (!state._consolidationIntervalMsec) {
          timeoutMsec = 0; // task not enabled

          return true; // reschedule
        }

        size_t usedMsec = std::chrono::duration_cast<std::chrono::milliseconds>(
          std::chrono::system_clock::now() - state._last
        ).count();

        if (usedMsec < state._consolidationIntervalMsec) {
          timeoutMsec = state._consolidationIntervalMsec - usedMsec; // still need to sleep

          return true; // reschedule (with possibly updated '_consolidationIntervalMsec')
        }

        state._last = std::chrono::system_clock::now(); // remember last task start time
        timeoutMsec = state._consolidationIntervalMsec;

        auto const runCleanupAfterConsolidation =
          state._cleanupIntervalCount > state._cleanupIntervalStep;

        // invoke consolidation on all known links
        for (auto& entry: state._links) {
          if (!entry.second) {
            continue; // skip link placeholders
          }

          SCOPED_LOCK(entry.second->mutex()); // ensure link is not deallocated for the duration of the operation
          auto* link = entry.second->get();

          if (!link) {
            continue; // skip missing links
          }

          auto res = link->consolidate(
            state._consolidationPolicy,
            state._progress,
            runCleanupAfterConsolidation
          );

          if (res.ok()
              && state._cleanupIntervalStep
              && state._cleanupIntervalCount++ > state._cleanupIntervalStep) {
            state._cleanupIntervalCount = 0;
          }
        }

        return true; // reschedule
    });
  }

  auto self = _asyncSelf;

  // initialize transaction read callback
  _trxCallback = [self](
      arangodb::transaction::Methods& trx,
      arangodb::transaction::Status status
  )->void {
    if (arangodb::transaction::Status::RUNNING != status) {
      return; // NOOP
    }

    SCOPED_LOCK(self->mutex());
    auto* view = self->get();

    if (view) {
      view->snapshot(trx, IResearchView::Snapshot::FindOrCreate);
    }
  };
}

IResearchView::~IResearchView() {
  _asyncTerminate.store(true); // mark long-running async jobs for terminatation
  updateProperties(_meta); // trigger reload of settings for async jobs
  _asyncSelf->reset(); // the view is being deallocated, its use is no longer valid (wait for all the view users to finish)
  _flushCallback.reset(); // unregister flush callback from flush thread
}

arangodb::Result IResearchView::appendVelocyPackDetailed(
    arangodb::velocypack::Builder& builder,
    bool forPersistence
) const {
  if (!builder.isOpenObject()) {
    return arangodb::Result(TRI_ERROR_BAD_PARAMETER);
  }

  std::vector<std::string> collections;

  {
    auto meta = std::atomic_load(&_meta);
    SCOPED_LOCK(meta->read()); // '_meta' can be asynchronously updated

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

    if (!meta->json(sanitizedBuilder)
        || !mergeSliceSkipKeys(builder, sanitizedBuilder.close().slice(), forPersistence ? persistenceAcceptor : acceptor)) {
      return arangodb::Result(
        TRI_ERROR_INTERNAL,
        std::string("failure to generate definition while generating properties jSON for arangosearch View in database '") + vocbase().name() + "'"
      );
    }
  }

  {
    ReadMutex mutex(_mutex); // '_links' can be asynchronously updated
    SCOPED_LOCK(mutex);

    if (forPersistence) {
      IResearchViewMetaState metaState;

      for (auto& entry: _links) {
        metaState._collections.emplace(entry.first);
      }

      metaState.json(builder);

      return arangodb::Result(); // nothing more to output (persistent configuration does not need links)
    }

    // add CIDs of known collections to list
    for (auto& entry: _links) {
      // skip collections missing from vocbase or UserTransaction constructor will throw an exception
      if (vocbase().lookupCollection(entry.first)) {
        collections.emplace_back(std::to_string(entry.first));
      }
    }
  }

  // open up a read transaction and add all linked collections to verify that
  // the current user has access

  arangodb::velocypack::Builder linksBuilder;
  static std::vector<std::string> const EMPTY;

  // use default lock timeout
  arangodb::transaction::Options options;

  options.waitForSync = false;
  options.allowImplicitCollections = false;

  try {
    arangodb::transaction::Methods trx(
      transaction::StandaloneContext::Create(vocbase()),
      collections, // readCollections
      EMPTY, // writeCollections
      EMPTY, // exclusiveCollections
      options
    );
    auto res = trx.begin();

    if (!res.ok()) {
      return res; // nothing more to output
    }

    auto* state = trx.state();

    if (!state) {
      return arangodb::Result(
        TRI_ERROR_INTERNAL,
        std::string("failed to get transaction state while generating json for arangosearch view '") + name() + "'"
      );
    }

    arangodb::velocypack::ObjectBuilder linksBuilderWrapper(&linksBuilder);

    for (auto& collectionName: state->collectionNames()) {
      for (auto& index: trx.indexesForCollection(collectionName)) {
        if (index && arangodb::Index::IndexType::TRI_IDX_TYPE_IRESEARCH_LINK == index->type()) {
          // TODO FIXME find a better way to retrieve an IResearch Link
          // cannot use static_cast/reinterpret_cast since Index is not related to IResearchLink
          auto* ptr = dynamic_cast<IResearchLink*>(index.get());

          if (!ptr || *ptr != *this) {
            continue; // the index is not a link for the current view
          }

          arangodb::velocypack::Builder linkBuilder;

          linkBuilder.openObject();

          if (!ptr->json(linkBuilder)) {
            LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
              << "failed to generate json for arangosearch link '" << ptr->id()
              << "' while generating json for arangosearch view '" << id() << "'";
            continue; // skip invalid link definitions
          }

          linkBuilder.close();

          // need to mask out some fields
          static const std::function<bool(irs::string_ref const& key)> acceptor = [](
              irs::string_ref const& key
          )->bool {
            return key != arangodb::StaticStrings::IndexId
                && key != arangodb::StaticStrings::IndexType
                && key != StaticStrings::ViewIdField; // ignored fields
          };

          arangodb::velocypack::Builder sanitizedBuilder;

          sanitizedBuilder.openObject();

          if (!mergeSliceSkipKeys(sanitizedBuilder, linkBuilder.slice(), acceptor)) {
            Result result(TRI_ERROR_INTERNAL,
                std::string("failed to generate externally visible link ")
                .append("definition while emplacing link definition into ")
                .append("arangosearch view '").append(name()).append("'"));

            LOG_TOPIC(WARN, iresearch::TOPIC) << result.errorMessage();

            return result;
          }

          sanitizedBuilder.close();
          linksBuilderWrapper->add(collectionName, sanitizedBuilder.slice());
        }
      }
    }

    trx.commit();
  } catch (arangodb::basics::Exception& e) {
    IR_LOG_EXCEPTION();

    return arangodb::Result(
      e.code(),
      std::string("caught exception while generating json for arangosearch view '") + name() + "': " + e.what()
    );
  } catch (std::exception const& e) {
    IR_LOG_EXCEPTION();

    return arangodb::Result(
      TRI_ERROR_INTERNAL,
      std::string("caught exception while generating json for arangosearch view '") + name() + "': " + e.what()
    );
  } catch (...) {
    IR_LOG_EXCEPTION();

    return arangodb::Result(
      TRI_ERROR_INTERNAL,
      std::string("caught exception while generating json for arangosearch view '") + name() + "'"
    );
  }

  builder.add(StaticStrings::LinksField, linksBuilder.slice());

  return arangodb::Result();
}

bool IResearchView::apply(arangodb::transaction::Methods& trx) {
  // called from IResearchView when this view is added to a transaction
  return trx.addStatusChangeCallback(&_trxCallback); // add shapshot
}

arangodb::Result IResearchView::dropImpl() {
  std::unordered_set<TRI_voc_cid_t> collections;
  std::unordered_set<TRI_voc_cid_t> stale;

  // drop all known links
  {
    ReadMutex mutex(_mutex); // '_metaState' can be asynchronously updated
    SCOPED_LOCK(mutex);

    for (auto& entry: _links) {
      stale.emplace(entry.first);
    }
  }

  if (!stale.empty()) {
    // check link auth as per https://github.com/arangodb/backlog/issues/459
    if (arangodb::ExecContext::CURRENT) {
      for (auto& entry: stale) {
        auto collection = vocbase().lookupCollection(entry);

        if (collection
            && !arangodb::ExecContext::CURRENT->canUseCollection(vocbase().name(), collection->name(), arangodb::auth::Level::RO)) {
          return arangodb::Result(TRI_ERROR_FORBIDDEN);
        }
      }
    }

    arangodb::Result res;

    {
      if (!_updateLinksLock.try_lock()) {
        return arangodb::Result(
          TRI_ERROR_FAILED, // FIXME use specific error code
          std::string("failed to remove arangosearch view '") + name()
        );
      }

      ADOPT_SCOPED_LOCK_NAMED(_updateLinksLock, lock);

      res = IResearchLinkHelper::updateLinks(
        collections,
        vocbase(),
        *this,
        arangodb::velocypack::Slice::emptyObjectSlice(),
        stale
      );
    }

    if (!res.ok()) {
      return arangodb::Result(
        res.errorNumber(),
        std::string("failed to remove links while removing arangosearch view '") + name() + "': " + res.errorMessage()
      );
    }
  }

  _asyncTerminate.store(true); // mark long-running async jobs for terminatation
  updateProperties(_meta); // trigger reload of settings for async jobs
  _asyncSelf->reset(); // the view data-stores are being deallocated, view use is no longer valid (wait for all the view users to finish)
  _flushCallback.reset(); // unregister flush callback from flush thread

  WriteMutex mutex(_mutex); // members can be asynchronously updated
  SCOPED_LOCK(mutex);

  for (auto& entry: _links) {
    collections.emplace(entry.first);
  }

  auto collectionsCount = collections.size();

  for (auto& entry: collections) {
    auto collection = vocbase().lookupCollection(entry);

    if (!collection || !IResearchLinkHelper::find(*collection, *this)) {
      --collectionsCount;
    }
  }

  // ArangoDB global consistency check, no known dangling links
  if (collectionsCount) {
    return arangodb::Result(
      TRI_ERROR_INTERNAL,
      std::string("links still present while removing arangosearch view '") + std::to_string(id()) + "'"
    );
  }

  return arangodb::Result();
}

/*static*/ arangodb::ViewFactory const& IResearchView::factory() {
  static const ViewFactory factory;

  return factory;
}

bool IResearchView::link(AsyncLinkPtr const& link) {
  if (!link || !link->get()) {
    return false; // invalid link
  }

  auto cid = link->get()->collection().id();
  WriteMutex mutex(_mutex); // '_links' can be asynchronously updated
  SCOPED_LOCK(mutex);
  auto itr = _links.find(cid);

  if (itr == _links.end()) {
    _links.emplace(cid, link);
  } else if (ServerState::instance()->isSingleServer() && !itr->second) {
    _links[cid] = link;

    return true; // single-server persisted cid placeholder substituted with actual link
  } else if (itr->second && !itr->second->get()) {
    _links[cid] = link;

    return true; // a previous link instance was unload()ed and a new instance is linking
  } else {
    return false; // link already present
  }

  try {
    auto res = persistProperties(*this, _asyncSelf);

    if (!res.ok()) {
      _links.erase(cid); // undo meta modification
      LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
        << "failed to persist logical view while emplacing collection '" << cid
        << "' into arangosearch View '" << name() << "': " << res.errorMessage();

      return false;
    }
  } catch (arangodb::basics::Exception& e) {
    _links.erase(cid); // undo meta modification
    LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
      << "caught exception during persisting of logical view while emplacing collection ' " << cid
      << "' into arangosearch View '" << name() << "': " << e.code() << " " << e.what();
    IR_LOG_EXCEPTION();
    throw;
  } catch (std::exception const& e) {
    _links.erase(cid); // undo meta modification
    LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
      << "caught exception during persisting of logical view while emplacing collection ' " << cid
      << "' into arangosearch View '" << name() << "': " << e.what();
    IR_LOG_EXCEPTION();
    throw;
  } catch (...) {
    _links.erase(cid); // undo meta modification
    LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
      << "caught exception during persisting of logical view while emplacing collection ' " << cid
      << "' into arangosearch View '" << name() << "'";
    IR_LOG_EXCEPTION();
    throw;
  }

  return true;
}

std::shared_ptr<const AsyncMeta> IResearchView::meta() const {
  return std::atomic_load(&_meta);
}

arangodb::Result IResearchView::commit() {
  ReadMutex mutex(_mutex); // '_links' can be asynchronously updated
  SCOPED_LOCK(mutex);

  for (auto& entry: _links) {
    auto cid = entry.first;

    if (!entry.second) {
      return arangodb::Result(
        TRI_ERROR_ARANGO_INDEX_HANDLE_BAD,
        std::string("failed to find an arangosearch link in collection '") + std::to_string(cid) + "' while syncing arangosearch view '" + name() + "'"
      );
    }

    SCOPED_LOCK(entry.second->mutex()); // ensure link is not deallocated for the duration of the operation
    auto* link = entry.second->get();

    if (!link) {
      return arangodb::Result(
        TRI_ERROR_ARANGO_INDEX_HANDLE_BAD,
        std::string("failed to find a loaded arangosearch link in collection '") + std::to_string(cid) + "' while syncing arangosearch view '" + name() + "'"
      );
    }

    auto res = link->commit();

    if (!res.ok()) {
      return res;
    }
  }

  // invalidate query cache if there were some data changes
  arangodb::aql::QueryCache::instance()->invalidate(&vocbase(), name());

  return arangodb::Result();
}

size_t IResearchView::memory() const {
  size_t size = sizeof(IResearchView);

  {
    auto meta = std::atomic_load(&_meta);
    SCOPED_LOCK(meta->read()); // '_meta' can be asynchronously updated

    size += meta->memory();
  }

  ReadMutex mutex(_mutex); // '_links' can be asynchronously updated
  SCOPED_LOCK(mutex);

  size += sizeof(decltype(_links)::value_type) * _links.size();

  for (auto& entry: _links) {
    if (!entry.second) {
      continue; // skip link placeholders
    }

    SCOPED_LOCK(entry.second->mutex()); // ensure link is not deallocated for the duration of the operation
    auto* link = entry.second->get();

    if (!link) {
      continue; // skip missing links
    }

    size += link->memory();
  }

  return size;
}

void IResearchView::open() {
  auto* engine = arangodb::EngineSelectorFeature::ENGINE;

  if (engine) {
    _inRecovery = engine->inRecovery();
  } else {
    LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
      << "failure to get storage engine while opening arangosearch view: " << name();
    // assume not inRecovery()
  }

  WriteMutex mutex(_mutex); // '_meta' can be asynchronously updated
  SCOPED_LOCK(mutex);

  if (!_flushCallback) {
    registerFlushCallback();
  }
}

irs::index_reader const* IResearchView::snapshot(
    transaction::Methods& trx,
    IResearchView::Snapshot mode /*= IResearchView::Snapshot::Find*/
) const {
  if (!trx.state()) {
    LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
      << "failed to get transaction state while creating arangosearch view snapshot";

    return nullptr;
  }

  auto& state = *(trx.state());
  auto* key = this;

  // TODO FIXME find a better way to look up a ViewState
  #ifdef ARANGODB_ENABLE_MAINTAINER_MODE
    auto* ctx = dynamic_cast<ViewTrxState*>(state.cookie(key));
  #else
    auto* ctx = static_cast<ViewTrxState*>(state.cookie(key));
  #endif

  switch (mode) {
   case Snapshot::Find:
    return ctx;
   case Snapshot::FindOrCreate:
    if (ctx) {
      return ctx;
    }
    break;
   case Snapshot::SyncAndReplace: {
    if (ctx) {
      ctx->clear(); // ignore existing cookie, recreate snapshot
    }

    auto res = const_cast<IResearchView*>(this)->commit();

    if (!res.ok()) {
      LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
        << "failed to sync while creating snapshot for arangosearch view '" << name() << "', previous snapshot will be used instead, error: '" << res.errorMessage() << "'";
    }

    break;
   }
   default:
    TRI_ASSERT(false); // all values of the enum should be covered
  }

  if (!ctx) {
    auto ptr = irs::memory::make_unique<ViewTrxState>();

    ctx = ptr.get();
    state.cookie(key, std::move(ptr));

    if (!ctx) {
      LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
        << "failed to store state into a TransactionState for snapshot of arangosearch view '" << name() << "', tid '" << state.id() << "'";

      return nullptr;
    }
  }

  ReadMutex mutex(_mutex); // '_metaState' can be asynchronously modified
  SCOPED_LOCK(mutex);

  try {
    // collect snapshots from all known links
    for (auto& entry: _links) {
      auto cid = entry.first;
      auto* link = entry.second ? entry.second->get() : nullptr; // do not need to lock link since collection is part of the transaction

      if (!link) {
        LOG_TOPIC(ERR, arangodb::iresearch::TOPIC)
          << "failed to find an arangosearch link in collection '" << cid << "' for arangosearch view '" << name() << "', skipping it";
        state.cookie(key, nullptr); // unset cookie

        return nullptr; // skip missing links
      }

      auto snapshot = link->snapshot();

      if (!static_cast<irs::directory_reader const&>(snapshot)) {
        LOG_TOPIC(ERR, arangodb::iresearch::TOPIC)
          << "failed to get snaphot of arangosearch link in collection '" << cid << "' for arangosearch view '" << name() << "', skipping it";
        state.cookie(key, nullptr); // unset cookie

        return nullptr; // skip failed readers
      }

      ctx->add(std::move(snapshot));
    }
  } catch (arangodb::basics::Exception& e) {
    LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
      << "caught exception while collecting readers for snapshot of arangosearch view '" << name() << "', tid '" << state.id() << "': " << e.code() << " " << e.what();
    IR_LOG_EXCEPTION();

    return nullptr;
  } catch (std::exception const& e) {
    LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
      << "caught exception while collecting readers for snapshot of arangosearch view '" << name() << "', tid '" << state.id() << "': " << e.what();
    IR_LOG_EXCEPTION();

    return nullptr;
  } catch (...) {
    LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
      << "caught exception while collecting readers for snapshot of arangosearch view '" << name() << "', tid '" << state.id() << "'";
    IR_LOG_EXCEPTION();

    return nullptr;
  }

  return ctx;
}

arangodb::Result IResearchView::unlink(TRI_voc_cid_t cid) noexcept {
  try {
    WriteMutex mutex(_mutex); // '_links' can be asynchronously read
    SCOPED_LOCK(mutex);
    auto itr = _links.find(cid);

    if (itr == _links.end()) {
      return arangodb::Result(); // already unlinked
    }

    auto links = _links;

    _links.erase(itr);

    try {
      auto res = persistProperties(*this, _asyncSelf);

      if (!res.ok()) {
        _links.swap(links); // restore original collections

        return res;
      }
    } catch (...) {
      _links.swap(links); // restore original collections
      throw;
    }
  } catch (arangodb::basics::Exception const& e) {
    return arangodb::Result(
      e.code(),
      std::string("caught exception while collection '") + std::to_string(cid) + "' from arangosearch view '" + name() + "': " + e.what()
    );
  } catch (std::exception const& e) {
    return arangodb::Result(
      TRI_ERROR_INTERNAL,
      std::string("caught exception while collection '") + std::to_string(cid) + "' from arangosearch view '" + name() + "': " + e.what()
    );
  } catch (...) {
    return arangodb::Result(
      TRI_ERROR_INTERNAL,
      std::string("caught exception while collection '") + std::to_string(cid) + "' from arangosearch view '" + name() + "'"
    );
  }

  return arangodb::Result();
}

arangodb::Result IResearchView::updateProperties(
    arangodb::velocypack::Slice const& slice,
    bool partialUpdate
) {
  try {
    auto links = slice.hasKey(StaticStrings::LinksField)
               ? slice.get(StaticStrings::LinksField)
               : arangodb::velocypack::Slice::emptyObjectSlice();
    auto res = _inRecovery
             ? arangodb::Result() // do not validate if in recovery
             : IResearchLinkHelper::validateLinks(vocbase(), links);

    if (!res.ok()) {
      return res;
    }

    WriteMutex mutex(_mutex); // '_metaState' can be asynchronously read
    SCOPED_LOCK_NAMED(mutex, mtx);

    // check link auth as per https://github.com/arangodb/backlog/issues/459
    if (arangodb::ExecContext::CURRENT) {
      // check existing links
      for (auto& entry: _links) {
        auto collection = vocbase().lookupCollection(entry.first);

        if (collection
            && !arangodb::ExecContext::CURRENT->canUseCollection(vocbase().name(), collection->name(), arangodb::auth::Level::RO)) {
          return arangodb::Result(
            TRI_ERROR_FORBIDDEN,
            std::string("while updating arangosearch definition, error: collection '") + collection->name() + "' not authorised for read access"
          );
        }
      }
    }

    std::string error;
    IResearchViewMeta meta;

    {
      auto viewMeta = std::atomic_load(&_meta);
      SCOPED_LOCK(viewMeta->write());
      IResearchViewMeta* metaPtr = viewMeta.get();
      auto& initialMeta = partialUpdate ? *metaPtr : IResearchViewMeta::DEFAULT();

      if (!meta.init(slice, error, initialMeta)) {
        return arangodb::Result(
          TRI_ERROR_BAD_PARAMETER,
          error.empty()
          ? (std::string("failed to update arangosearch view '") + name() + "' from definition: " + slice.toString())
          : (std::string("failed to update arangosearch view '") + name() + "' from definition, error in attribute '" + error + "': " + slice.toString())
        );
      }

      // reset non-updatable values to match current meta
      meta._locale = viewMeta->_locale;
      meta._version = viewMeta->_version;
      meta._writebufferActive = viewMeta->_writebufferActive;
      meta._writebufferIdle = viewMeta->_writebufferIdle;
      meta._writebufferSizeMax = viewMeta->_writebufferSizeMax;

      if (arangodb::ServerState::instance()->isDBServer()) {
        viewMeta = std::make_shared<AsyncMeta>(); // create an instance not shared with cluster-view
      }

      static_cast<IResearchViewMeta&>(*viewMeta) = std::move(meta);
      updateProperties(viewMeta); // trigger reload of settings for async jobs
    }

    mutex.unlock(true); // downgrade to a read-lock

    if (links.isEmptyObject() && (partialUpdate || _inRecovery.load())) { // ignore missing links coming from WAL (inRecovery)
      return res;
    }

    // ...........................................................................
    // update links if requested (on a best-effort basis)
    // indexing of collections is done in different threads so no locks can be held and rollback is not possible
    // as a result it's also possible for links to be simultaneously modified via a different callflow (e.g. from collections)
    // ...........................................................................

    std::unordered_set<TRI_voc_cid_t> collections;

    if (partialUpdate) {
      mtx.unlock(); // release lock

      SCOPED_LOCK(_updateLinksLock);

      return IResearchLinkHelper::updateLinks(
        collections, vocbase(), *this, links
      );
    }

    std::unordered_set<TRI_voc_cid_t> stale;

    for (auto& entry: _links) {
      stale.emplace(entry.first);
    }

    mtx.unlock(); // release lock

    SCOPED_LOCK(_updateLinksLock);

    return IResearchLinkHelper::updateLinks(
      collections, vocbase(), *this, links, stale
    );
  } catch (arangodb::basics::Exception& e) {
    LOG_TOPIC(WARN, iresearch::TOPIC)
      << "caught exception while updating properties for arangosearch view '" << name() << "': " << e.code() << " " << e.what();
    IR_LOG_EXCEPTION();

    return arangodb::Result(
      e.code(),
      std::string("error updating properties for arangosearch view '") + name() + "'"
    );
  } catch (std::exception const& e) {
    LOG_TOPIC(WARN, iresearch::TOPIC)
      << "caught exception while updating properties for arangosearch view '" << name() << "': " << e.what();
    IR_LOG_EXCEPTION();

    return arangodb::Result(
      TRI_ERROR_BAD_PARAMETER,
      std::string("error updating properties for arangosearch view '") + name() + "'"
    );
  } catch (...) {
    LOG_TOPIC(WARN, iresearch::TOPIC)
      << "caught exception while updating properties for arangosearch view '" << name() << "'";
    IR_LOG_EXCEPTION();

    return arangodb::Result(
      TRI_ERROR_BAD_PARAMETER,
      std::string("error updating properties for arangosearch view '") + name() + "'"
    );
  }
}

arangodb::Result IResearchView::updateProperties(
  std::shared_ptr<AsyncMeta> const& meta
) {
  if (!meta) {
    return arangodb::Result(TRI_ERROR_BAD_PARAMETER);
  }

  std::atomic_store(&_meta, meta);

  if (_asyncFeature) {
    _asyncFeature->asyncNotify();
  }

  return arangodb::Result();
}

void IResearchView::registerFlushCallback() {
  auto* flush = getFlushFeature();

  if (!flush) {
    // feature not registered
    return;
  }

  auto viewSelf = _asyncSelf;

  flush->registerCallback(this, [viewSelf]() {
    static struct NoopFlushTransaction: arangodb::FlushTransaction {
      NoopFlushTransaction(): FlushTransaction("ArangoSearchNoop") {}
      virtual arangodb::Result commit() override {
        return arangodb::Result(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND);
      }
    } noopFlushTransaction;
    SCOPED_LOCK_NAMED(viewSelf->mutex(), lock);

    if (!*viewSelf) {
      return arangodb::FlushFeature::FlushTransactionPtr(
        &noopFlushTransaction, [](arangodb::FlushTransaction*)->void {}
      );
    }

    auto trx = arangodb::FlushFeature::FlushTransactionPtr(
      viewSelf->get(),
      [](arangodb::FlushTransaction* trx)->void {
        ADOPT_SCOPED_LOCK_NAMED(static_cast<IResearchView*>(trx)->_asyncSelf->mutex(), lock);
      }
    );

    lock.release(); // unlocked in distructor above

    return trx;
  });

  // noexcept
  _flushCallback.reset(this); // mark for future unregistration
}

bool IResearchView::visitCollections(
    LogicalView::CollectionVisitor const& visitor
) const {
  ReadMutex mutex(_mutex); // '_links' can be asynchronously modified
  SCOPED_LOCK(mutex);

  for (auto& entry: _links) {
    if (!visitor(entry.first)) {
      return false;
    }
  }

  return true;
}

void IResearchView::FlushCallbackUnregisterer::operator()(IResearchView* view) const noexcept {
  arangodb::FlushFeature* flush = nullptr;

  if (!view || !(flush = getFlushFeature())) {
    return;
  }

  try {
    flush->unregisterCallback(view);
  } catch (...) {
    // suppress all errors
  }
}

void IResearchView::verifyKnownCollections() {
  bool modified = false;
  WriteMutex mutex(_mutex); // '_links' can be asynchronously read
  SCOPED_LOCK(mutex);

  // verify existence of all known links
  for (auto itr = _links.begin(); itr != _links.end();) {
    auto cid = itr->first;
    auto collection = vocbase().lookupCollection(cid); // always look up in vocbase (single server or cluster per-shard collection)

    if (!collection) {
      LOG_TOPIC(TRACE, arangodb::iresearch::TOPIC)
        << "collection '" << cid << "' no longer exists! removing from arangosearch view '" << name() << "'";
      itr = _links.erase(itr);
      modified = true;

      continue;
    }

    auto link = IResearchLinkHelper::find(*collection, *this);

    if (!link) {
      LOG_TOPIC(TRACE, arangodb::iresearch::TOPIC)
        << "collection '" << collection->name() << "' no longer linked! removing from arangosearch view '" << name() << "'";
      itr = _links.erase(itr);
      modified = true;

      continue;
    }

    TRI_ASSERT(itr->second); // all links must be valid even on single-server
    ++itr;
  }

  if (modified) {
    persistProperties(*this, _asyncSelf);
  }
}

} // iresearch
} // arangodb

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------