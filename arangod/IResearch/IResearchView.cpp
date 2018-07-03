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

#include "formats/formats.hpp"
#include "search/all_filter.hpp"
#include "search/boolean_filter.hpp"
#include "search/scorers.hpp"
#include "search/score.hpp"
#include "store/memory_directory.hpp"
#include "store/mmap_directory.hpp"
#include "utils/directory_utils.hpp"
#include "utils/memory.hpp"
#include "utils/misc.hpp"

#include "IResearchAttributes.h"
#include "IResearchCommon.h"
#include "IResearchDocument.h"
#include "IResearchOrderFactory.h"
#include "IResearchFilterFactory.h"
#include "IResearchLink.h"
#include "IResearchLinkHelper.h"
#include "IResearchViewDBServer.h"
#include "ExpressionFilter.h"
#include "AqlHelper.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Aql/AstNode.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/SortCondition.h"
#include "Basics/ConditionLocker.h"
#include "Basics/Result.h"
#include "Basics/files.h"
#include "Logger/LogMacros.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "StorageEngine/TransactionState.h"
#include "StorageEngine/StorageEngine.h"
#include "RestServer/DatabaseFeature.h"
#include "RestServer/DatabasePathFeature.h"
#include "RestServer/FlushFeature.h"
#include "Transaction/Methods.h"
#include "Transaction/StandaloneContext.h"
#include "Utils/CollectionNameResolver.h"
#include "velocypack/Builder.h"
#include "velocypack/Iterator.h"
#include "VocBase/LocalDocumentId.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/LogicalView.h"

#include "IResearchView.h"

NS_LOCAL

////////////////////////////////////////////////////////////////////////////////
/// @brief surrogate root for all queries without a filter
////////////////////////////////////////////////////////////////////////////////
arangodb::aql::AstNode ALL(true, arangodb::aql::VALUE_TYPE_BOOL);

////////////////////////////////////////////////////////////////////////////////
/// @brief the storage format used with iResearch writers
////////////////////////////////////////////////////////////////////////////////
const irs::string_ref IRESEARCH_STORE_FORMAT("1_0");

typedef irs::async_utils::read_write_mutex::read_mutex ReadMutex;
typedef irs::async_utils::read_write_mutex::write_mutex WriteMutex;

////////////////////////////////////////////////////////////////////////////////
/// --SECTION--                                               utility constructs
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief index reader implementation over multiple directory readers
////////////////////////////////////////////////////////////////////////////////
class CompoundReader final: public arangodb::iresearch::PrimaryKeyIndexReader {
 public:
  CompoundReader(ReadMutex& viewMutex);
  irs::sub_reader const& operator[](
      size_t subReaderId
  ) const noexcept override {
    return *(_subReaders[subReaderId].first);
  }

  void add(irs::directory_reader const& reader);
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

  std::vector<irs::directory_reader> _readers;
  SubReadersType _subReaders;
  std::lock_guard<ReadMutex> _viewLock; // prevent data-store deallocation (lock @ AsyncSelf)
};

CompoundReader::CompoundReader(ReadMutex& viewMutex): _viewLock(viewMutex) {
}

void CompoundReader::add(irs::directory_reader const& reader) {
  _readers.emplace_back(reader);

  for(auto& entry: _readers.back()) {
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

// @brief approximate iResearch directory instance size
size_t directoryMemory(irs::directory const& directory, TRI_voc_cid_t viewId) noexcept {
  size_t size = 0;

  try {
    directory.visit([&directory, &size](std::string& file)->bool {
      uint64_t length;

      size += directory.length(length, file) ? length : 0;

      return true;
    });
  } catch (std::exception const& e) {
    LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
      << "caught error while calculating size of iResearch view '" << viewId << "': " << e.what();
    IR_LOG_EXCEPTION();
  } catch (...) {
    LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
      << "caught error while calculating size of iResearch view '" << viewId << "'";
    IR_LOG_EXCEPTION();
  }

  return size;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief compute the data path to user for iresearch persisted-store
///        get base path from DatabaseServerFeature (similar to MMFilesEngine)
///        the path is hardcoded to reside under:
///        <DatabasePath>/<IResearchView::type()>-<view id>
///        similar to the data path calculation for collections
////////////////////////////////////////////////////////////////////////////////
irs::utf8_path getPersistedPath(
    arangodb::DatabasePathFeature const& dbPathFeature,
    TRI_vocbase_t const& vocbase,
    TRI_voc_cid_t id
) {
  irs::utf8_path dataPath(dbPathFeature.directory());
  static const std::string subPath("databases");
  static const std::string dbPath("database-");

  dataPath /= subPath;
  dataPath /= dbPath;
  dataPath += std::to_string(vocbase.id());
  dataPath /= arangodb::iresearch::DATA_SOURCE_TYPE.name();
  dataPath += "-";
  dataPath += std::to_string(id);

  return dataPath;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief inserts ArangoDB document into an IResearch data store
////////////////////////////////////////////////////////////////////////////////
inline void insertDocument(
    irs::segment_writer::document& doc,
    arangodb::iresearch::FieldIterator& body,
    TRI_voc_cid_t cid,
    TRI_voc_rid_t rid) {
  using namespace arangodb::iresearch;

  // reuse the 'Field' instance stored
  // inside the 'FieldIterator' after
  auto& field = const_cast<Field&>(*body);

  // User fields
  while (body.valid()) {
    doc.insert(irs::action::index_store, field);
    ++body;
  }

  // System fields
  // Indexed: CID
  Field::setCidValue(field, cid, Field::init_stream_t());
  doc.insert(irs::action::index, field);

  // Indexed: RID
  Field::setRidValue(field, rid);
  doc.insert(irs::action::index, field);

  // Stored: CID + RID
  DocumentPrimaryKey const primaryKey(cid, rid);
  doc.insert(irs::action::store, primaryKey);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief persist view definition to the storage engine
///        if in-recovery then register a post-recovery lambda for persistence
/// @return success
////////////////////////////////////////////////////////////////////////////////
arangodb::Result persistProperties(
    arangodb::LogicalView const& view,
    arangodb::iresearch::IResearchView::AsyncSelf::ptr asyncSelf
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
      engine->changeView(view.vocbase(), view.id(), view, true);
    } catch (std::exception const& e) {
      return arangodb::Result(
        TRI_ERROR_INTERNAL,
        std::string("caught exception during persistance of properties for IResearch View '") + view.name() + "': " + e.what()
      );
    } catch (...) {
      return arangodb::Result(
        TRI_ERROR_INTERNAL,
        std::string("caught exception during persistance of properties for IResearch View '") + view.name() + "'"
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
        engine->changeView(view.vocbase(), view.id(), view, true);
      } catch (std::exception const& e) {
        return arangodb::Result(
          TRI_ERROR_INTERNAL,
          std::string("caught exception during persistance of properties for IResearch View '") + view.name() + "': " + e.what()
        );
      } catch (...) {
        return arangodb::Result(
          TRI_ERROR_INTERNAL,
          std::string("caught exception during persistance of properties for IResearch View '") + view.name() + "'"
        );
      }

      return arangodb::Result();
    }
  );
}

////////////////////////////////////////////////////////////////////////////////
/// @brief syncs an IResearch DataStore if required
/// @return a sync was executed
////////////////////////////////////////////////////////////////////////////////
bool syncStore(
    irs::directory& directory,
    irs::directory_reader& reader,
    irs::index_writer& writer,
    std::atomic<size_t>& segmentCount,
    arangodb::iresearch::IResearchViewMeta::CommitMeta::ConsolidationPolicies const& policies,
    bool forceCommit,
    bool runCleanupAfterCommit,
    std::string const& viewName
) {
  char runId = 0; // value not used

  // ...........................................................................
  // apply consolidation policies
  // ...........................................................................

  for (auto& entry: policies) {
    if (!entry.segmentThreshold()
        || entry.segmentThreshold() > segmentCount.load()) {
      continue; // skip if interval not reached or no valid policy to execute
    }

    LOG_TOPIC(DEBUG, arangodb::iresearch::TOPIC)
      << "registering consolidation policy '" << size_t(entry.type()) << "' with IResearch view '" << viewName << "' run id '" << size_t(&runId) << " segment threshold '" << entry.segmentThreshold() << "' segment count '" << segmentCount.load() << "'";

    try {
      writer.consolidate(entry.policy(), false);
      forceCommit = true; // a consolidation policy was found requiring commit
    } catch (std::exception const& e) {
      LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
        << "caught exception during registeration of consolidation policy '" << size_t(entry.type()) << "' with IResearch view '" << viewName << "': " << e.what();
      IR_LOG_EXCEPTION();
    } catch (...) {
      LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
        << "caught exception during registeration of consolidation policy '" << size_t(entry.type()) << "' with IResearch view '" << viewName << "'";
      IR_LOG_EXCEPTION();
    }

    LOG_TOPIC(DEBUG, arangodb::iresearch::TOPIC)
      << "finished registering consolidation policy '" << size_t(entry.type()) << "' with IResearch view '" << viewName << "' run id '" << size_t(&runId) << "'";
  }

  if (!forceCommit) {
    LOG_TOPIC(DEBUG, arangodb::iresearch::TOPIC)
      << "skipping store sync since no consolidation policies matched and sync not forced for IResearch view '" << viewName << "' run id '" << size_t(&runId) << "'";

    return false; // commit not done
  }

  // ...........................................................................
  // apply data store commit
  // ...........................................................................

  LOG_TOPIC(DEBUG, arangodb::iresearch::TOPIC)
    << "starting store sync for IResearch view '" << viewName << "' run id '" << size_t(&runId) << "' segment count before '" << segmentCount.load() << "'";

  try {
    segmentCount.store(0); // reset to zero to get count of new segments that appear during commit
    writer.commit();
    reader = reader.reopen(); // update reader
    segmentCount += reader.size(); // add commited segments
  } catch (std::exception const& e) {
    LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
      << "caught exception during sync of IResearch view '" << viewName << "': " << e.what();
    IR_LOG_EXCEPTION();
  } catch (...) {
    LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
      << "caught exception during sync of IResearch view '" << viewName << "'";
    IR_LOG_EXCEPTION();
  }

  LOG_TOPIC(DEBUG, arangodb::iresearch::TOPIC)
    << "finished store sync for IResearch view '" << viewName << "' run id '" << size_t(&runId) << "' segment count after '" << segmentCount.load() << "'";

  if (!runCleanupAfterCommit) {
    return true; // commit done
  }

  // ...........................................................................
  // apply cleanup
  // ...........................................................................

  LOG_TOPIC(DEBUG, arangodb::iresearch::TOPIC)
    << "starting cleanup for IResearch view '" << viewName << "' run id '" << size_t(&runId) << "'";

  try {
    irs::directory_utils::remove_all_unreferenced(directory);
  } catch (std::exception const& e) {
    LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
      << "caught exception during cleanup of IResearch view '" << viewName << "': " << e.what();
    IR_LOG_EXCEPTION();
  } catch (...) {
    LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
      << "caught exception during cleanup of IResearch view '" << viewName << "'";
    IR_LOG_EXCEPTION();
  }

  LOG_TOPIC(DEBUG, arangodb::iresearch::TOPIC)
    << "finished cleanup for IResearch view '" << viewName << "' run id '" << size_t(&runId) << "'";

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief remove all cids from 'collections' that do not actually exist in
///        'vocbase' for the specified 'view'
////////////////////////////////////////////////////////////////////////////////
void validateLinks(
    std::unordered_set<TRI_voc_cid_t>& collections,
    TRI_vocbase_t& vocbase,
    arangodb::iresearch::IResearchView const& view
) {
  for (auto itr = collections.begin(), end = collections.end(); itr != end;) {
    auto collection = vocbase.lookupCollection(*itr);

    if (!collection || !arangodb::iresearch::IResearchLink::find(*collection, view)) {
      itr = collections.erase(itr);
    } else {
      ++itr;
    }
  }
}

NS_END

NS_BEGIN(arangodb)
NS_BEGIN(iresearch)

///////////////////////////////////////////////////////////////////////////////
/// --SECTION--                                    IResearchView implementation
///////////////////////////////////////////////////////////////////////////////

IResearchView::DataStore::DataStore(DataStore&& other) noexcept {
  *this = std::move(other);
}

IResearchView::DataStore& IResearchView::DataStore::operator=(
    IResearchView::DataStore&& other
) noexcept {
  if (this != &other) {
    _directory = std::move(other._directory);
    _reader = std::move(other._reader);
    _writer = std::move(other._writer);
  }

  return *this;
}

void IResearchView::DataStore::sync() {
  TRI_ASSERT(_writer && _reader);
  _segmentCount.store(0); // reset to zero to get count of new segments that appear during commit
  _writer->commit();
  _reader = _reader.reopen(); // update reader
  _segmentCount += _reader.size(); // add commited segments
}

IResearchView::MemoryStore::MemoryStore() {
  auto format = irs::formats::get(IRESEARCH_STORE_FORMAT);

  _directory = irs::directory::make<irs::memory_directory>();

  // create writer before reader to ensure data directory is present
  _writer = irs::index_writer::make(*_directory, format, irs::OM_CREATE_APPEND);
  _writer->commit(); // initialize 'store'
  _reader = irs::directory_reader::open(*_directory); // open after 'commit' for valid 'store'
}

IResearchView::PersistedStore::PersistedStore(irs::utf8_path&& path)
  : _path(std::move(path)) {
}

////////////////////////////////////////////////////////////////////////////////
/// @brief container storing the view 'read' state for a given TransactionState
////////////////////////////////////////////////////////////////////////////////
struct IResearchView::ViewStateRead: public arangodb::TransactionState::Cookie {
  CompoundReader _snapshot;
  ViewStateRead(ReadMutex& mutex): _snapshot(mutex) {}
};

////////////////////////////////////////////////////////////////////////////////
/// @brief container storing the view 'write' state for a given TransactionState
////////////////////////////////////////////////////////////////////////////////
struct IResearchView::ViewStateWrite
  : public arangodb::TransactionState::Cookie,
    public IResearchView::MemoryStore {
  // removal filters to be applied to during merge
  // transactions are single-threaded so no mutex is required
  std::vector<std::shared_ptr<irs::filter>> _removals;
  std::lock_guard<ReadMutex> _viewLock; // prevent data-store deallocation (lock @ AsyncSelf)
  ViewStateWrite(ReadMutex& viewMutex): _viewLock(viewMutex) {}
};

////////////////////////////////////////////////////////////////////////////////
/// @brief helper class for retrieving/setting view transaction states
////////////////////////////////////////////////////////////////////////////////
class IResearchView::ViewStateHelper {
 public:
  static IResearchView::ViewStateRead* read(
      arangodb::TransactionState& state,
      IResearchView const& view
  ) {
    static_assert(sizeof(IResearchView) > Reader, "'Reader' offset >= sizeof(IResearchView)");
    auto* key = &view + Reader;

    // TODO FIXME find a better way to look up a ViewState
    #ifdef ARANGODB_ENABLE_MAINTAINER_MODE
      return dynamic_cast<IResearchView::ViewStateRead*>(state.cookie(key));
    #else
      return static_cast<IResearchView::ViewStateRead*>(state.cookie(key));
    #endif
  }

  static bool read(
      arangodb::TransactionState& state,
      IResearchView const& view,
      std::unique_ptr<IResearchView::ViewStateRead>&& value
  ) {
    static_assert(sizeof(IResearchView) > Reader, "'Reader' offset >= sizeof(IResearchView)");
    auto* key = &view + Reader;
    auto prev = state.cookie(key, std::move(value));

    if (!prev) {
      return true;
    }

    state.cookie(key, std::move(prev)); // put back original value

    return false;
  }

  static IResearchView::ViewStateWrite* write(
      arangodb::TransactionState& state,
      IResearchView const& view
  ) {
    static_assert(sizeof(IResearchView) > Writer, "'Writer' offset >= sizeof(IResearchView)");
    auto* key = &view + Writer;

    // TODO FIXME find a better way to look up a ViewState
    #ifdef ARANGODB_ENABLE_MAINTAINER_MODE
      return dynamic_cast<IResearchView::ViewStateWrite*>(state.cookie(key));
    #else
      return static_cast<IResearchView::ViewStateWrite*>(state.cookie(key));
    #endif
  }

  static bool write(
      arangodb::TransactionState& state,
      IResearchView const& view,
      std::unique_ptr<IResearchView::ViewStateWrite>&& value
  ) {
    static_assert(sizeof(IResearchView) > Writer, "'Writer' offset >= sizeof(IResearchView)");
    auto* key = &view + Writer;
    auto prev = state.cookie(key, std::move(value));

    if (!prev) {
      return true;
    }

    state.cookie(key, std::move(prev)); // put back original value

    return false;
  }

 private:
  enum offsets { Reader, Writer }; // offsets from key
};

IResearchView::IResearchView(
    TRI_vocbase_t& vocbase,
    arangodb::velocypack::Slice const& info,
    arangodb::DatabasePathFeature const& dbPathFeature,
    uint64_t planVersion
): DBServerLogicalView(vocbase, info, planVersion),
    FlushTransaction(toString(*this)),
   _asyncSelf(irs::memory::make_unique<AsyncSelf>(this)),
   _asyncTerminate(false),
   _memoryNode(&_memoryNodes[0]), // set current memory node (arbitrarily 0)
   _toFlush(&_memoryNodes[1]), // set flush-pending memory node (not same as _memoryNode)
   _storePersisted(getPersistedPath(dbPathFeature, vocbase, id())),
   _inRecovery(false) {
  // set up in-recovery insertion hooks
  auto* feature = arangodb::application_features::ApplicationServer::lookupFeature<
    arangodb::DatabaseFeature
  >("Database");

  if (feature) {
    auto view = _asyncSelf; // create copy for lambda

    feature->registerPostRecoveryCallback([view]()->arangodb::Result {
      auto viewMutex = view->mutex();
      SCOPED_LOCK(viewMutex); // ensure view does not get deallocated before call back finishes
      auto* viewPtr = view->get();

      if (!viewPtr) {
        LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
          << "Invalid call to post-recovery callback of iResearch view";

        return arangodb::Result(); // view no longer in recovery state
      }

      viewPtr->verifyKnownCollections();

      if (viewPtr->_storePersisted) {
        LOG_TOPIC(DEBUG, arangodb::iresearch::TOPIC)
          << "starting persisted-sync sync for iResearch view '" << viewPtr->id() << "'";

        try {
          viewPtr->_storePersisted.sync();
        } catch (std::exception const& e) {
          LOG_TOPIC(ERR, arangodb::iresearch::TOPIC)
            << "caught exception while committing persisted store for iResearch view '" << viewPtr->id()
            << "': " << e.what();

          return arangodb::Result(TRI_ERROR_INTERNAL, e.what());
        } catch (...) {
          LOG_TOPIC(ERR, arangodb::iresearch::TOPIC)
            << "caught exception while committing persisted store for iResearch view '" << viewPtr->id() << "'";

          return arangodb::Result(TRI_ERROR_INTERNAL);
        }

        LOG_TOPIC(DEBUG, arangodb::iresearch::TOPIC)
          << "finished persisted-sync sync for iResearch view '" << viewPtr->id() << "'";
      }

      viewPtr->_inRecovery = false;

      return arangodb::Result();
    });
  }

  // initialize round-robin memory store chain
  auto it = _memoryNodes;
  for (auto next = it + 1, end = std::end(_memoryNodes); next < end; ++it, ++next) {
    it->_next = next;
  }
  it->_next = _memoryNodes;

  auto* viewPtr = this;

  // initialize transaction read callback
  _trxReadCallback = [viewPtr](
      arangodb::transaction::Methods& trx,
      arangodb::transaction::Status status
  )->void {
    if (arangodb::transaction::Status::RUNNING != status) {
      return; // NOOP
    }

    viewPtr->snapshot(trx, true);
  };

  // initialize transaction write callback
  _trxWriteCallback = [viewPtr](
      arangodb::transaction::Methods& trx,
      arangodb::transaction::Status status
  )->void {
    auto* state = trx.state();

    // check state of the top-most transaction only
    if (!state || arangodb::transaction::Status::COMMITTED != state->status()) {
      return; // NOOP
    }

    auto* cookie = ViewStateHelper::write(*state, *viewPtr);
    TRI_ASSERT(cookie); // must have been added together with this callback
    ReadMutex mutex(viewPtr->_mutex); // '_memoryStore'/'_storePersisted' can be asynchronously modified

    try {
      {
        SCOPED_LOCK(mutex);

        // transfer filters first since they only apply to pre-merge data
        // transactions are single-threaded so no mutex is required for '_removals'
        for (auto& filter: cookie->_removals) {
          // FIXME TODO potential problem of loss of 'remove' if:
          // 'insert' in '_toFlush' and 'remove' comes during IResearchView::commit()
          // after '_toFlush' is commit()ed but before '_toFlush' in import()ed
          viewPtr->_memoryNode->_store._writer->remove(filter);
          viewPtr->_toFlush->_store._writer->remove(filter);
        }

        // transfer filters to persisted store as well otherwise query resuts will be incorrect
        // on recovery the same removals will be replayed from the WAL
        if (viewPtr->_storePersisted) {
          // transactions are single-threaded so no mutex is required for '_removals'
          for (auto& filter: cookie->_removals) {
            viewPtr->_storePersisted._writer->remove(filter);
          }
        }

        auto& memoryStore = viewPtr->activeMemoryStore();

        cookie->_writer->commit(); // ensure have latest view in reader
        memoryStore._writer->import(cookie->_reader.reopen());
        ++memoryStore._segmentCount; // a new segment was imported
      }

      if (state->waitForSync() && !viewPtr->sync()) {
        LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
          << "failed to sync while committing transaction for IResearch view '" << viewPtr->name()
          << "', tid '" << state->id() << "'";
      }
    } catch (std::exception const& e) {
      LOG_TOPIC(ERR, arangodb::iresearch::TOPIC)
        << "caught exception while committing transaction for IResearch view '" << viewPtr->name()
        << "', tid '" << state->id() << "': " << e.what();
      IR_LOG_EXCEPTION();
    } catch (...) {
      LOG_TOPIC(ERR, arangodb::iresearch::TOPIC)
        << "caught exception while committing transaction for iResearch view '" << viewPtr->name()
        << "', tid '" << state->id() << "'";
      IR_LOG_EXCEPTION();
    }
  };
}

IResearchView::~IResearchView() {
  _asyncTerminate.store(true); // mark long-running async jobs for terminatation
  _syncWorker->refresh(); // trigger reload of settings for async jobs
  _syncWorker.reset(); // ensure destructor called if required
  _asyncSelf->reset(); // the view is being deallocated, its use is no longer valid (wait for all the view users to finish)
  _flushCallback.reset(); // unregister flush callback from flush thread

  {
    WriteMutex mutex(_mutex); // '_meta' can be asynchronously read
    SCOPED_LOCK(mutex);

    if (_storePersisted) {
      try {
        _storePersisted._writer->commit();
        _storePersisted._writer->close();
        _storePersisted._writer.reset();
        _storePersisted._directory->close();
        _storePersisted._directory.reset();
      } catch (...) {
        // FIXME add logging
        // must not propagate exception out of destructor
      }
    }
  }

  // noexcept below
  if (deleted()) {
    StorageEngine* engine = EngineSelectorFeature::ENGINE;
    TRI_ASSERT(engine);
    engine->destroyView(vocbase(), *this);
  }
}

IResearchView::MemoryStore& IResearchView::activeMemoryStore() const {
  return _memoryNode->_store;
}

bool IResearchView::apply(arangodb::transaction::Methods& trx) {
  // called from IResearchView when this view is added to a transaction
  return trx.addStatusChangeCallback(&_trxReadCallback); // add shapshot
}

int IResearchView::drop(TRI_voc_cid_t cid) {
  std::shared_ptr<irs::filter> shared_filter(iresearch::FilterFactory::filter(cid));
  WriteMutex mutex(_mutex); // '_meta' and '_storeByTid' can be asynchronously updated
  SCOPED_LOCK(mutex);
  auto cid_itr = _metaState._collections.find(cid);

  if (cid_itr != _metaState._collections.end()) {
    auto result = persistProperties(*this, _asyncSelf);

    if (!result.ok()) {
      LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
        << "failed to persist logical view while dropping collection ' " << cid
        << "' from IResearch View '" << name() << "': " << result.errorMessage();

      return result.errorNumber();
    }

    _metaState._collections.erase(cid_itr);
  }

  mutex.unlock(true); // downgrade to a read-lock

  // ...........................................................................
  // if an exception occurs below than a drop retry would most likely happen
  // ...........................................................................
  try {
    // FIXME TODO remove from in-progress transactions, i.e. ViewStateWrite ???
    // FIXME TODO remove from '_toFlush' memory-store ???
    auto& memoryStore = activeMemoryStore();
    memoryStore._writer->remove(shared_filter);

    if (_storePersisted) {
      _storePersisted._writer->remove(shared_filter);
    }

    return TRI_ERROR_NO_ERROR;
  } catch (std::exception const& e) {
    LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
      << "caught exception while removing from iResearch view '" << id()
      << "', collection '" << cid << "': " << e.what();
    IR_LOG_EXCEPTION();
  } catch (...) {
    LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
      << "caught exception while removing from iResearch view '" << id()
      << "', collection '" << cid << "'";
    IR_LOG_EXCEPTION();
  }

  return TRI_ERROR_INTERNAL;
}

arangodb::Result IResearchView::dropImpl() {
  std::unordered_set<TRI_voc_cid_t> stale;

  // drop all known links
  {
    ReadMutex mutex(_mutex);
    SCOPED_LOCK(mutex); // '_meta' can be asynchronously updated
    stale = _metaState._collections;
  }

  std::unordered_set<TRI_voc_cid_t> collections;
  auto res = IResearchLinkHelper::updateLinks(
    collections, vocbase(), *this, emptyObjectSlice(), stale
  );

  if (!res.ok()) {
    return arangodb::Result(
      TRI_ERROR_INTERNAL,
      std::string("failed to remove links while removing IResearch view '") + std::to_string(id()) + "'"
    );
  }

  _asyncTerminate.store(true); // mark long-running async jobs for terminatation
  _syncWorker->refresh(); // trigger reload of settings for async jobs
  _asyncSelf->reset(); // the view data-stores are being deallocated, view use is no longer valid (wait for all the view users to finish)
  WriteMutex mutex(_mutex); // members can be asynchronously updated
  SCOPED_LOCK(mutex);

  collections.insert(
    _metaState._collections.begin(), _metaState._collections.end()
  );
  validateLinks(collections, vocbase(), *this);

  // ArangoDB global consistency check, no known dangling links
  if (!collections.empty()) {
    return arangodb::Result(
      TRI_ERROR_INTERNAL,
      std::string("links still present while removing iResearch view '") + std::to_string(id()) + "'"
    );
  }

  // ...........................................................................
  // if an exception occurs below than a drop retry would most likely happen
  // ...........................................................................
  try {
    for (size_t i = 0, count = IRESEARCH_COUNTOF(_memoryNodes); i < count; ++i) {
      auto& memoryStore = _memoryNodes[i]._store;

      // ensure no error on double-drop
      if (memoryStore) {
        memoryStore._writer->close();
        memoryStore._writer.reset();
        memoryStore._directory->close();
        memoryStore._directory.reset();
      }
    }

    if (_storePersisted) {
      _storePersisted._writer->close();
      _storePersisted._writer.reset();
      _storePersisted._directory->close();
      _storePersisted._directory.reset();
    }

    bool exists;

    // remove persisted data store directory if present
    if (_storePersisted._path.exists_directory(exists)
        && (!exists || _storePersisted._path.remove())) {
      return arangodb::Result(); // success
    }
  } catch (std::exception const& e) {
    LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
      << "caught exception while removing IResearch view '" << name() << "': " << e.what();
    IR_LOG_EXCEPTION();

    return arangodb::Result(
      TRI_ERROR_INTERNAL,
      std::string("caught exception while removing IResearch view '") + name() + "': " + e.what()
    );
  } catch (...) {
    LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
      << "caught exception while removing IResearch view '" << name() << "'";
    IR_LOG_EXCEPTION();

    return arangodb::Result(
      TRI_ERROR_INTERNAL,
      std::string("caught exception while removing IResearch view '") + name() + "'"
    );
  }

  return arangodb::Result(
    TRI_ERROR_INTERNAL,
    std::string("failed to remove IResearch view '") + name() + "'"
  );
}

bool IResearchView::emplace(TRI_voc_cid_t cid) {
  WriteMutex mutex(_mutex); // '_meta' can be asynchronously updated
  SCOPED_LOCK(mutex);
  arangodb::Result result;

  if (!_metaState._collections.emplace(cid).second) {
    return false;
  }

  try {
    result = persistProperties(*this, _asyncSelf);

    if (result.ok()) {
      return true;
    }
  } catch (std::exception const& e) {
    _metaState._collections.erase(cid); // undo meta modification
    LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
      << "caught exception during persisting of logical view while emplacing collection ' " << cid
      << "' into IResearch View '" << name() << "': " << e.what();
    IR_LOG_EXCEPTION();
    throw;
  } catch (...) {
    _metaState._collections.erase(cid); // undo meta modification
    LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
      << "caught exception during persisting of logical view while emplacing collection ' " << cid
      << "' into IResearch View '" << name() << "'";
    IR_LOG_EXCEPTION();
    throw;
  }

  _metaState._collections.erase(cid); // undo meta modification
  LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
    << "failed to persist logical view while emplacing collection ' " << cid
    << "' into IResearch View '" << name() << "': " << result.errorMessage();

  return false;
}

arangodb::Result IResearchView::commit() {
  ReadMutex mutex(_mutex); // '_storePersisted' can be asynchronously updated
  SCOPED_LOCK(mutex);

  if (!_storePersisted) {
    return {}; // nothing more to do
  }

  auto& memoryStore = _toFlush->_store;

  try {
    memoryStore._writer->commit(); // ensure have latest view in reader

    // intentional copy since `memoryStore._reader` may be updated
    const auto reader = (
      memoryStore._reader = memoryStore._reader.reopen() // update reader
    );

    // merge memory store into persisted
    if (!_storePersisted._writer->import(reader)) {
      return {TRI_ERROR_INTERNAL};
    }

    SCOPED_LOCK(_toFlush->_reopenMutex); // do not allow concurrent reopen
    _storePersisted._segmentCount.store(0); // reset to zero to get count of new segments that appear during commit
    _storePersisted._writer->commit(); // finishing flush transaction
    memoryStore._segmentCount.store(0); // reset to zero to get count of new segments that appear during commit
    memoryStore._writer->clear(); // prepare the store for reuse

    SCOPED_LOCK(_toFlush->_readMutex); // do not allow concurrent read since _storePersisted/_toFlush need to be updated atomically
    _storePersisted._reader = _storePersisted._reader.reopen(); // update reader
    _storePersisted._segmentCount += _storePersisted._reader.size(); // add commited segments
    memoryStore._reader = memoryStore._reader.reopen(); // update reader
    memoryStore._segmentCount += memoryStore._reader.size(); // add commited segments

    return TRI_ERROR_NO_ERROR;
  } catch (std::exception const& e) {
    LOG_TOPIC(ERR, arangodb::iresearch::TOPIC)
      << "caught exception while committing memory store for iResearch view '" << id() << "': " << e.what();
    IR_LOG_EXCEPTION();
  } catch (...) {
    LOG_TOPIC(ERR, arangodb::iresearch::TOPIC)
      << "caught exception while committing memory store for iResearch view '" << id();
    IR_LOG_EXCEPTION();
  }

  return {TRI_ERROR_INTERNAL};
}

void IResearchView::getPropertiesVPack(
  arangodb::velocypack::Builder& builder, bool forPersistence
) const {
  ReadMutex mutex(_mutex);
  SCOPED_LOCK(mutex); // '_metaState'/'_links' can be asynchronously updated

  {
    SCOPED_LOCK(_meta->read()); // '_meta' can be asynchronously updated

    _meta->json(builder);
  }

  _metaState.json(builder);

  if (forPersistence) {
    return; // nothing more to output (persistent configuration does not need links)
  }

  TRI_ASSERT(builder.isOpenObject());
  std::vector<std::string> collections;

  // add CIDs of known collections to list
  for (auto& entry: _metaState._collections) {
    // skip collections missing from vocbase or UserTransaction constructor will throw an exception
    if (vocbase().lookupCollection(entry)) {
      collections.emplace_back(std::to_string(entry));
    }
  }

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

    if (trx.begin().fail()) {
      return; // nothing more to output
    }

    auto* state = trx.state();

    if (!state) {
      return; // nothing more to output
    }

    arangodb::velocypack::ObjectBuilder linksBuilderWrapper(&linksBuilder);

    for (auto& collectionName: state->collectionNames()) {
      for (auto& index: trx.indexesForCollection(collectionName)) {
        if (index && arangodb::Index::IndexType::TRI_IDX_TYPE_IRESEARCH_LINK == index->type()) {
          // TODO FIXME find a better way to retrieve an iResearch Link
          // cannot use static_cast/reinterpret_cast since Index is not related to IResearchLink
          auto* ptr = dynamic_cast<IResearchLink*>(index.get());

          if (!ptr || *ptr != *this) {
            continue; // the index is not a link for the current view
          }

          arangodb::velocypack::Builder linkBuilder;

          linkBuilder.openObject();

          if (!ptr->json(linkBuilder, false)) {
            LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
              << "failed to generate json for IResearch link '" << ptr->id()
              << "' while generating json for IResearch view '" << id() << "'";
            continue; // skip invalid link definitions
          }

          linkBuilder.close();
          linksBuilderWrapper->add(collectionName, linkBuilder.slice());
        }
      }
    }

    trx.commit();
  } catch (std::exception const& e) {
    LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
      << "caught exception while generating json for IResearch view '" << id() << "': " << e.what();
    IR_LOG_EXCEPTION();
    return; // do not add 'links' section
  } catch (...) {
    LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
      << "caught exception while generating json for IResearch view '" << id() << "'";
    IR_LOG_EXCEPTION();
    return; // do not add 'links' section
  }

  builder.add(StaticStrings::LinksField, linksBuilder.slice());
}

int IResearchView::insert(
    transaction::Methods& trx,
    TRI_voc_cid_t cid,
    arangodb::LocalDocumentId const& documentId,
    arangodb::velocypack::Slice const& doc,
    IResearchLinkMeta const& meta
) {
  DataStore* store;

  if (_inRecovery) {
    _storePersisted._writer->remove(FilterFactory::filter(cid, documentId.id()));

    store = &_storePersisted;
  } else if (!trx.state()) {
    return TRI_ERROR_BAD_PARAMETER; // 'trx' and transaction state required
  } else {
    auto& state = *(trx.state());

    store = ViewStateHelper::write(state, *this);

    if (!store) {
      auto ptr = irs::memory::make_unique<ViewStateWrite>(_asyncSelf->mutex()); // will aquire read-lock to prevent data-store deallocation

      if (!_asyncSelf->get()) {
        return TRI_ERROR_INTERNAL; // the current view is no longer valid (checked after ReadLock aquisition)
      }

      store = ptr.get();

      if (!ViewStateHelper::write(state, *this, std::move(ptr))
          || !trx.addStatusChangeCallback(&_trxWriteCallback)) {
        LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
          << "failed to store state into a TransactionState for insert into IResearch view '" << name() << "'"
          << "', tid '" << state.id() << "', collection '" << cid << "', revision '" << documentId.id() << "'";

        return TRI_ERROR_INTERNAL;
      }
    }
  }

  TRI_ASSERT(store && false == !*store);

  FieldIterator body(doc, meta);

  if (!body.valid()) {
    return TRI_ERROR_NO_ERROR; // nothing to index
  }

  auto insert = [&body, cid, documentId](irs::segment_writer::document& doc)->bool {
    insertDocument(doc, body, cid, documentId.id());

    return false; // break the loop
  };

  try {
    if (store->_writer->insert(insert)) {
      return TRI_ERROR_NO_ERROR;
    }

    LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
      << "failed inserting into iResearch view '" << id()
      << "', collection '" << cid << "', revision '" << documentId.id() << "'";
  } catch (std::exception const& e) {
    LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
      << "caught exception while inserting into iResearch view '" << id()
      << "', collection '" << cid << "', revision '" << documentId.id() << "': " << e.what();
    IR_LOG_EXCEPTION();
  } catch (...) {
    LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
      << "caught exception while inserting into iResearch view '" << id()
      << "', collection '" << cid << "', revision '" << documentId.id() << "'";
    IR_LOG_EXCEPTION();
  }

  return TRI_ERROR_INTERNAL;
}

int IResearchView::insert(
    transaction::Methods& trx,
    TRI_voc_cid_t cid,
    std::vector<std::pair<arangodb::LocalDocumentId, arangodb::velocypack::Slice>> const& batch,
    IResearchLinkMeta const& meta
) {
  DataStore* store;

  if (_inRecovery) {
    for (auto& doc : batch) {
      _storePersisted._writer->remove(FilterFactory::filter(cid, doc.first.id()));
    }

    store = &_storePersisted;
  } else if (!trx.state()) {
    return TRI_ERROR_BAD_PARAMETER; // 'trx' and transaction state required
  } else {
    auto& state = *(trx.state());

    store = ViewStateHelper::write(state, *this);

    if (!store) {
      auto ptr = irs::memory::make_unique<ViewStateWrite>(_asyncSelf->mutex()); // will aquire read-lock to prevent data-store deallocation

      if (!_asyncSelf->get()) {
        return TRI_ERROR_INTERNAL; // the current view is no longer valid (checked after ReadLock aquisition)
      }

      store = ptr.get();

      if (!ViewStateHelper::write(state, *this, std::move(ptr))
          || !trx.addStatusChangeCallback(&_trxWriteCallback)) {
        LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
          << "failed to store state into a TransactionState for insert into IResearch view '" << name() << "'"
          << "', tid '" << state.id() << "', collection '" << cid << "'";

        return TRI_ERROR_INTERNAL;
      }
    }
  }

  TRI_ASSERT(store && false == !*store);

  auto begin = batch.begin();
  auto const end = batch.end();
  FieldIterator body;
  TRI_voc_rid_t rid = 0; // initialize to an arbitary value to avoid compile warning

  // find first valid document
  while (!body.valid() && begin != end) {
    body.reset(begin->second, meta);
    rid = begin->first.id();
    ++begin;
  }

  if (!body.valid()) {
    return TRI_ERROR_NO_ERROR; // nothing to index
  }

  auto insert = [&meta, &begin, &end, &body, cid, &rid](irs::segment_writer::document& doc)->bool {
    insertDocument(doc, body, cid, rid);

    // find next valid document
    while (begin != end) {
      body.reset(begin->second, meta);
      rid = begin->first.id();
      ++begin;

      if (body.valid()) {
        return true; // next document available
      }
    }

    return false; // break the loop
  };

  try {
    if (!store->_writer->insert(insert)) {
      LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
        << "failed inserting batch into iResearch view '" << id() << "', collection '" << cid;
      return TRI_ERROR_INTERNAL;
    }

    store->_writer->commit(); // no need to consolidate if batch size is set correctly
  } catch (std::exception const& e) {
    LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
      << "caught exception while inserting batch into iResearch view '" << id() << "', collection '" << cid << e.what();
    IR_LOG_EXCEPTION();
  } catch (...) {
    LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
      << "caught exception while inserting batch into iResearch view '" << id() << "', collection '" << cid;
    IR_LOG_EXCEPTION();
  }

  return TRI_ERROR_NO_ERROR;
}

/*static*/ std::shared_ptr<LogicalView> IResearchView::make(
    TRI_vocbase_t& vocbase,
    arangodb::velocypack::Slice const& info,
    bool isNew,
    uint64_t planVersion,
    LogicalView::PreCommitCallback const& preCommit /*= {}*/
) {
  return makeWithMeta(
    vocbase, info, isNew, planVersion, nullptr, nullptr, preCommit
  );
}

/*static*/ std::shared_ptr<LogicalView> IResearchView::makeWithMeta(
    TRI_vocbase_t& vocbase,
    arangodb::velocypack::Slice const& info,
    bool isNew,
    uint64_t planVersion,
    std::shared_ptr<AsyncMeta> const& meta,
    std::shared_ptr<IResearchViewSyncWorker> const& syncWorker,
    LogicalView::PreCommitCallback const& preCommit /*= {}*/
) {
  auto* feature = arangodb::application_features::ApplicationServer::lookupFeature<
    arangodb::DatabasePathFeature
  >("DatabasePath");

  if (!feature) {
    LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
      << "failure to find feature 'DatabasePath' while constructing IResearch View in database '" << vocbase.id() << "'";

    return nullptr;
  }

  auto view = std::shared_ptr<IResearchView>(
    new IResearchView(vocbase, info, *feature, planVersion)
  );
  auto& impl = reinterpret_cast<IResearchView&>(*view);
  auto& json = info.isObject() ? info : emptyObjectSlice(); // if no 'info' then assume defaults
  auto props = json.get(StaticStrings::PropertiesField);
  auto& properties = props.isObject() ? props : emptyObjectSlice(); // if no 'info' then assume defaults
  std::string error;

  impl._meta = meta ? meta : std::make_shared<AsyncMeta>();

  if (!(meta || impl._meta->init(properties, error)) // do not reinit external meta
      || !impl._metaState.init(properties, error)) {
    LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
      << "failed to initialize iResearch view from definition, error: " << error;

    return nullptr;
  }

  impl._syncWorker = syncWorker
    ? syncWorker
    : std::make_shared<IResearchViewSyncWorker>(impl._meta)
    ;

  if (preCommit && !preCommit(view)) {
    LOG_TOPIC(ERR, arangodb::iresearch::TOPIC)
      << "Failure during pre-commit while constructing IResearch View in database '" << vocbase.id() << "'";

    return nullptr;
  }

  if (isNew) {
    auto const res = create(static_cast<arangodb::DBServerLogicalView&>(*view));

    if (!res.ok()) {
      LOG_TOPIC(ERR, arangodb::iresearch::TOPIC)
        << "Failure during commit of created view while constructing IResearch View in database '" << vocbase.id() << "', error: " << res.errorMessage();

      return nullptr;
    }
  }

  return view;
}

size_t IResearchView::memory() const {
  ReadMutex mutex(_mutex); // view members can be asynchronously updated
  SCOPED_LOCK(mutex);
  size_t size = sizeof(IResearchView);

  {
    SCOPED_LOCK(_meta->read()); // '_meta' can be asynchronously updated

    size += _meta->memory();
  }

  size += _metaState.memory();
  // FIXME TODO somehow compute the size of TransactionState cookies for this view
  size += sizeof(_memoryNode) + sizeof(_toFlush) + sizeof(_memoryNodes);
  size += directoryMemory(*(_memoryNode->_store._directory), id());
  size += directoryMemory(*(_toFlush->_store._directory), id());

  if (_storePersisted) {
    size += directoryMemory(*(_storePersisted._directory), id());
    size += _storePersisted._path.native().size() * sizeof(irs::utf8_path::native_char_t);
  }

  return size;
}

void IResearchView::open() {
  auto* engine = arangodb::EngineSelectorFeature::ENGINE;

  if (engine) {
    _inRecovery = engine->inRecovery();
  } else {
    LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
      << "failure to get storage engine while opening IResearch View: " << name();
    // assume not inRecovery()
  }

  WriteMutex mutex(_mutex); // '_meta' can be asynchronously updated
  SCOPED_LOCK(mutex);

  if (_storePersisted) {
    return; // view already open
  }

  try {
    auto format = irs::formats::get(IRESEARCH_STORE_FORMAT);

    if (format) {
      _storePersisted._directory =
        irs::directory::make<irs::mmap_directory>(_storePersisted._path.utf8());

      if (_storePersisted._directory) {
        // create writer before reader to ensure data directory is present
        _storePersisted._writer =
          irs::index_writer::make(*(_storePersisted._directory), format, irs::OM_CREATE_APPEND);

        if (_storePersisted._writer) {
          _storePersisted._writer->commit(); // initialize 'store'
          _storePersisted._reader
            = irs::directory_reader::open(*(_storePersisted._directory));

          if (_storePersisted._reader) {
            registerFlushCallback();
            updateProperties(_meta, _syncWorker); // register store commit tasks

            return; // success
          }

          _storePersisted._writer.reset(); // unlock the directory
        }
      }
    }
  } catch (std::exception const& e) {
    LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
      << "caught exception while opening iResearch view '" << id() << "': " << e.what();
    IR_LOG_EXCEPTION();
    throw;
  } catch (...) {
    LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
      << "caught exception while opening iResearch view '" << id() << "'";
    IR_LOG_EXCEPTION();
    throw;
  }

  LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
    << "failed to open IResearch view '" << name() << "' at: " << _storePersisted._path.utf8();

  throw std::runtime_error(
    std::string("failed to open iResearch view '") + name() + "' at: " + _storePersisted._path.utf8()
  );
}

int IResearchView::remove(
  transaction::Methods& trx,
  TRI_voc_cid_t cid,
  arangodb::LocalDocumentId const& documentId
) {
  std::shared_ptr<irs::filter> shared_filter(FilterFactory::filter(cid, documentId.id()));

  if (_inRecovery) {
    // FIXME TODO potential problem of loss of 'remove' if:
    // 'insert' in '_toFlush' and 'remove' comes during IResearchView::commit()
    // after '_toFlush' is commit()ed but before '_toFlush' in import()ed
    _memoryNode->_store._writer->remove(shared_filter);
    _toFlush->_store._writer->remove(shared_filter);
    _storePersisted._writer->remove(shared_filter);

    return TRI_ERROR_NO_ERROR;
  }

  if (!trx.state()) {
    return TRI_ERROR_BAD_PARAMETER; // 'trx' and transaction state required
  }

  auto& state = *(trx.state());
  auto* store = ViewStateHelper::write(state, *this);

  if (!store) {
    auto ptr = irs::memory::make_unique<ViewStateWrite>(_asyncSelf->mutex()); // will aquire read-lock to prevent data-store deallocation

    if (!_asyncSelf->get()) {
      return TRI_ERROR_INTERNAL; // the current view is no longer valid (checked after ReadLock aquisition)
    }

    store = ptr.get();

    if (!ViewStateHelper::write(state, *this, std::move(ptr))
        || !trx.addStatusChangeCallback(&_trxWriteCallback)) {
      LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
        << "failed to store state into a TransactionState for insert into IResearch view '" << name() << "'"
        << "', tid '" << state.id() << "', collection '" << cid << "', revision '" << documentId.id() << "'";

      return TRI_ERROR_INTERNAL;
    }
  }

  TRI_ASSERT(store && false == !*store);

  // ...........................................................................
  // if an exception occurs below than the transaction is droped including all
  // all of its fid stores, no impact to iResearch View data integrity
  // ...........................................................................
  try {
    store->_writer->remove(shared_filter);
    store->_removals.emplace_back(shared_filter); // transactions are single-threaded so no mutex is required for '_removals'

    return TRI_ERROR_NO_ERROR;
  } catch (std::exception const& e) {
    LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
      << "caught exception while removing from iResearch view '" << id()
      << "', tid '" << state.id() << "', collection '" << cid << "', revision '" << documentId.id() << "': " << e.what();
    IR_LOG_EXCEPTION();
  } catch (...) {
    LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
      << "caught exception while removing from iResearch view '" << id()
      << "', tid '" << state.id() << "', collection '" << cid << "', revision '" << documentId.id() << "'";
    IR_LOG_EXCEPTION();
  }

  return TRI_ERROR_INTERNAL;
}

PrimaryKeyIndexReader* IResearchView::snapshot(
    transaction::Methods& trx,
    bool force /*= false*/
) const {
  auto* state = trx.state();

  if (!state) {
    LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
      << "failed to get transaction state while creating IResearchView snapshot";

    return nullptr;
  }

  auto* cookie = ViewStateHelper::read(*state, *this);

  if (cookie) {
    return &(cookie->_snapshot);
  }

  if (!force) {
    return nullptr;
  }

  if (state->waitForSync() && !const_cast<IResearchView*>(this)->sync()) {
    LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
      << "failed to sync while creating snapshot for IResearch view '" << name() << "', previous snapshot will be used instead";
  }

  auto cookiePtr = irs::memory::make_unique<ViewStateRead>(_asyncSelf->mutex()); // will aquire read-lock to prevent data-store deallocation
  auto& reader = cookiePtr->_snapshot;

  if (!_asyncSelf->get()) {
    return nullptr; // the current view is no longer valid (checked after ReadLock aquisition)
  }

  try {
    ReadMutex mutex(_mutex); // _memoryNodes/_storePersisted can be asynchronously updated
    SCOPED_LOCK(mutex);

    reader.add(_memoryNode->_store._reader);
    SCOPED_LOCK(_toFlush->_readMutex);
    reader.add(_toFlush->_store._reader);

    if (_storePersisted) {
      reader.add(_storePersisted._reader);
    }
  } catch (std::exception const& e) {
    LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
      << "caught exception while collecting readers for snapshot of IResearch view '" << name()
      << "', tid '" << state->id() << "': " << e.what();
    IR_LOG_EXCEPTION();

    return nullptr;
  } catch (...) {
    LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
      << "caught exception while collecting readers for snapshot of IResearch view '" << name()
      << "', tid '" << state->id() << "'";
    IR_LOG_EXCEPTION();

    return nullptr;
  }

  if (!ViewStateHelper::read(*state, *this, std::move(cookiePtr))) {
    LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
      << "failed to store state into a TransactionState for snapshot of IResearch view '" << name()
      << "', tid '" << state->id() << "'";

    return nullptr;
  }

  return &reader;
}

IResearchView::AsyncSelf::ptr IResearchView::self() const {
  return _asyncSelf;
}

bool IResearchView::sync(size_t maxMsec /*= 0*/) {
  ReadMutex mutex(_mutex);
  auto thresholdSec = TRI_microtime() + maxMsec/1000.0;

  try {
    SCOPED_LOCK(mutex);

    LOG_TOPIC(DEBUG, arangodb::iresearch::TOPIC)
      << "starting active memory-store sync for iResearch view '" << id() << "'";
    _memoryNode->_store.sync();
    LOG_TOPIC(DEBUG, arangodb::iresearch::TOPIC)
      << "finished memory-store sync for iResearch view '" << id() << "'";

    if (maxMsec && TRI_microtime() >= thresholdSec) {
      return true; // skip if timout exceeded
    }

    LOG_TOPIC(DEBUG, arangodb::iresearch::TOPIC)
      << "starting pending memory-store sync for iResearch view '" << id() << "'";
    _toFlush->_store._segmentCount.store(0); // reset to zero to get count of new segments that appear during commit
    _toFlush->_store._writer->commit();

    {
      SCOPED_LOCK(_toFlush->_reopenMutex);
      _toFlush->_store._reader = _toFlush->_store._reader.reopen(); // update reader
      _toFlush->_store._segmentCount += _toFlush->_store._reader.size(); // add commited segments
    }

    LOG_TOPIC(DEBUG, arangodb::iresearch::TOPIC)
      << "finished pending memory-store sync for iResearch view '" << id() << "'";

    if (maxMsec && TRI_microtime() >= thresholdSec) {
      return true; // skip if timout exceeded
    }

    // must sync persisted store as well to ensure removals are applied
    if (_storePersisted) {
      LOG_TOPIC(DEBUG, arangodb::iresearch::TOPIC)
        << "starting persisted-sync sync for iResearch view '" << id() << "'";
      _storePersisted._segmentCount.store(0); // reset to zero to get count of new segments that appear during commit
      _storePersisted._writer->commit();

      {
        SCOPED_LOCK(_toFlush->_reopenMutex);
        _storePersisted._reader = _storePersisted._reader.reopen(); // update reader
        _storePersisted._segmentCount += _storePersisted._reader.size(); // add commited segments
      }

      LOG_TOPIC(DEBUG, arangodb::iresearch::TOPIC)
        << "finished persisted-sync sync for iResearch view '" << id() << "'";
    }

    return true;
  } catch (std::exception const& e) {
    LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
      << "caught exception during sync of iResearch view '" << id() << "': " << e.what();
    IR_LOG_EXCEPTION();
  } catch (...) {
    LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
      << "caught exception during sync of iResearch view '" << id() << "'";
    IR_LOG_EXCEPTION();
  }

  return false;
}

arangodb::Result IResearchView::updateProperties(
    arangodb::velocypack::Slice const& slice,
    bool partialUpdate
) {
  std::string error;
  IResearchViewMeta meta;
  WriteMutex mutex(_mutex); // '_metaState' can be asynchronously read
  arangodb::Result res;
  SCOPED_LOCK_NAMED(mutex, mtx);

  {
    SCOPED_LOCK(_meta->write());
    IResearchViewMeta* metaPtr = _meta.get();
    auto& initialMeta = partialUpdate ? *metaPtr : IResearchViewMeta::DEFAULT();

    if (!meta.init(slice, error, initialMeta)) {
      return arangodb::Result(TRI_ERROR_BAD_PARAMETER, std::move(error));
    }

    *metaPtr = std::move(meta);
  }

  _syncWorker->refresh();
  mutex.unlock(true); // downgrade to a read-lock

  if (!slice.hasKey(StaticStrings::LinksField)) {
    return res;
  }

  // ...........................................................................
  // update links if requested (on a best-effort basis)
  // indexing of collections is done in different threads so no locks can be held and rollback is not possible
  // as a result it's also possible for links to be simultaneously modified via a different callflow (e.g. from collections)
  // ...........................................................................

  std::unordered_set<TRI_voc_cid_t> collections;
  auto links = slice.get(StaticStrings::LinksField);

  if (partialUpdate) {
    mtx.unlock(); // release lock

    return IResearchLinkHelper::updateLinks(
      collections, vocbase(), *this, links
    );
  }

  auto stale = _metaState._collections;

  mtx.unlock(); // release lock

  return IResearchLinkHelper::updateLinks(
    collections, vocbase(), *this, links, stale
  );
}

arangodb::Result IResearchView::updateProperties(
  std::shared_ptr<AsyncMeta> const& meta,
  std::shared_ptr<IResearchViewSyncWorker> const& syncWorker /*= nullptr*/
) {
  if (!meta) {
    return arangodb::Result(TRI_ERROR_BAD_PARAMETER);
  }

  std::atomic_store(&_meta, meta);

  if (!syncWorker) {
    return arangodb::Result(); // NOOP
  }

  std::atomic_store(&_syncWorker, syncWorker);

  std::vector<DataStore*> dataStores = {
    &(_memoryNode[0]._store), &(_memoryNode[1]._store), &_storePersisted
  };

  for (auto* store: dataStores) {
    syncWorker->emplace(_asyncSelf, name(), _asyncTerminate, *store, _mutex);
  }

  return arangodb::Result();
}

void IResearchView::registerFlushCallback() {
  auto* flush = getFlushFeature();

  if (!flush) {
    // feature not registered
    return;
  }

  flush->registerCallback(this, [this]() noexcept {
    ////////////////////////////////////////////////////////////////////////////////
    /// @brief opens a flush transaction and returns a control object to be used
    ///        by FlushThread spawned by FlushFeature
    /// @returns empty object if something's gone wrong
    ////////////////////////////////////////////////////////////////////////////////
    WriteMutex mutex(_mutex); // ensure that _memoryNode->_store is not in use
    SCOPED_LOCK(mutex);

    _toFlush = _memoryNode; // memory store to be flushed into the persisted store
    _memoryNode = _memoryNode->_next; // switch to the next node

    mutex.unlock(true); // downgrade to a read-lock

    return IResearchView::FlushTransactionPtr(
      this,
      [](arangodb::FlushTransaction*){} // empty deleter
    );
  });

  // noexcept
  _flushCallback.reset(this); // mark for future unregistration
}

bool IResearchView::visitCollections(
    LogicalView::CollectionVisitor const& visitor
) const {
  ReadMutex mutex(_mutex);
  SCOPED_LOCK(mutex);

  for (auto& cid: _metaState._collections) {
    if (!visitor(cid)) {
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
  auto cids = _metaState._collections;

  {
    struct DummyTransaction : transaction::Methods {
      explicit DummyTransaction(std::shared_ptr<transaction::Context> const& ctx)
        : transaction::Methods(ctx) {
      }
    };

    transaction::StandaloneContext context(vocbase());
    std::shared_ptr<transaction::Context> dummy;  // intentionally empty
    DummyTransaction trx(std::shared_ptr<transaction::Context>(dummy, &context)); // use aliasing constructor

    if (!appendKnownCollections(cids, *snapshot(trx, true))) {
      LOG_TOPIC(ERR, arangodb::iresearch::TOPIC)
        << "failed to collect collection IDs for IResearch view '" << id() << "'";

      return;
    }
  }

  for (auto cid : cids) {
    auto collection = vocbase().lookupCollection(cid);

    if (!collection) {
      // collection no longer exists, drop it and move on
      LOG_TOPIC(TRACE, arangodb::iresearch::TOPIC)
        << "collection '" << cid
        << "' no longer exists! removing from IResearch view '"
        << id() << "'";
      drop(cid);
    } else {
      // see if the link still exists, otherwise drop and move on
      auto link = IResearchLink::find(*collection, *this);
      if (!link) {
        LOG_TOPIC(TRACE, arangodb::iresearch::TOPIC)
          << "collection '" << cid
          << "' no longer linked! removing from IResearch view '"
          << id() << "'";
        drop(cid);
      }
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// --SECTION--                           IResearchViewSyncWorker implementation
////////////////////////////////////////////////////////////////////////////////

IResearchViewSyncWorker::IResearchViewSyncWorker(
    std::shared_ptr<AsyncMeta> const& meta
  ): _meta(meta),
     _metaRefresh(true), // ensure initial load of meta
     _terminate(false),
     _thread("ArangoSearch Sync") {
  TRI_ASSERT(meta); // FIXME TODO use make(..)

  _thread._fn = [this]()->void {
    IResearchViewMeta::CommitMeta meta;
    auto commitIntervalMsecRemainder = std::numeric_limits<size_t>::max(); // longest possible time for std::min(...)

    for(;;) {
      if (_metaRefresh.load()) {
        SCOPED_LOCK(_meta->read()); // '_meta' may be modified asynchronously (do not aquire inside '_mutex')
        meta = _meta->_commit; // local copy
        _metaRefresh.store(false);
      }

      // remove any stale jobs before goin back to sleep (could have appeared during execution)
      for (size_t i = 0, count = _tasks.size(); i < count;) {
        if (!_tasks[i]._terminate->load()) {
          ++i;

          continue;
        }

        if (i < count - 1) {
          std::swap(_tasks[i], _tasks[count - 1]); // swap 'i' with tail
        }

        _tasks.pop_back(); // remove stale tail
        count = _tasks.size();
      }

      bool commitTimeoutReached = false;

      {
        SCOPED_LOCK_NAMED(_mutex, lock); // aquire before '_terminate' check so that don't miss notify()

        if (_terminate.load()) {
          return; // termination requested
        }

        // transfer any new pending tasks into active tasks
        for (auto& pending: _pending) {
          _tasks.emplace_back(std::move(pending)); // will aquire resource lock

          auto& task = _tasks.back();

          // view not valid or task terminated
          if (task._resourceMutex->get() || task._terminate->load()) {
            _tasks.pop_back();
          }
        }

        _pending.clear();

        // sleep until timeout
        if (!meta._commitIntervalMsec) {
          _cond.wait(lock); // wait forever
        } else {
          auto msecRemainder =
            std::min(commitIntervalMsecRemainder, meta._commitIntervalMsec);
          auto startTime = std::chrono::system_clock::now();
          auto endTime = startTime + std::chrono::milliseconds(msecRemainder);

          commitIntervalMsecRemainder = std::numeric_limits<size_t>::max(); // longest possible time assuming an uninterrupted sleep
          commitTimeoutReached = true;

          if (std::cv_status::timeout != _cond.wait_until(lock, endTime)) {
            auto nowTime = std::chrono::system_clock::now();

            // if still need to sleep more then must relock '_meta' and sleep for min (remainder, interval)
            if (nowTime < endTime) {
              commitIntervalMsecRemainder =
                std::chrono::duration_cast<std::chrono::milliseconds>(endTime - nowTime).count();
              commitTimeoutReached = false;
            }
          }
        }

        if (_terminate.load()) { // check again after sleep
          return; // termination requested
        }
      }

      auto thresholdSec = TRI_microtime() + meta._commitTimeoutMsec/1000.0;

      for (size_t i = 0, count = _tasks.size();
           i < count && TRI_microtime() <= thresholdSec;) {
        auto& task = _tasks[i];

        // task removal requested
        if (task._terminate->load()) {
          if (i < count - 1) {
            std::swap(task, _tasks[count - 1]); // swap 'i' with tail
          }

          _tasks.pop_back(); // remove stale tail
          --count;

          continue;
        }

        ++i;

        auto& store = *(task._store);
        ReadMutex storeMutex(*(task._storeMutex));
        auto runCleanupAfterCommit =
          task._cleanupIntervalCount > meta._cleanupIntervalStep;
        SCOPED_LOCK(storeMutex); // 'store' can be asynchronously modified/reset (do not aquire inside '_mutex')

        if (store._directory
            && store._writer
            && syncStore(*(store._directory),
                         store._reader,
                         *(store._writer),
                         store._segmentCount,
                         meta._consolidationPolicies,
                         commitTimeoutReached,
                         runCleanupAfterCommit,
                         *(task._name)
                        )
           ) {
          commitIntervalMsecRemainder = std::numeric_limits<size_t>::max(); // longest possible time for std::min(...)

          if (runCleanupAfterCommit
              && ++(task._cleanupIntervalCount) >= meta._cleanupIntervalStep) {
            task._cleanupIntervalCount = 0; // use offset since task may have changed its location im memory due to addition/resize
          }
        }
      }
    }
  };

  _thread.start(&_join); // std::thread thread(...); _thread = std::move(thread);
}

IResearchViewSyncWorker::~IResearchViewSyncWorker() {
  // stop asynchronous jobs
  {
    _terminate.store(true);
    SCOPED_LOCK(_mutex);
    _cond.notify_all();
  }

  CONDITION_LOCKER(lock, _join);

  // _thread.join();
  while(_thread.isRunning()) {
    _join.wait();
  }
}

void IResearchViewSyncWorker::emplace(
    std::shared_ptr<IResearchView::AsyncSelf> resourceMutex,
    std::string const& name,
    std::atomic<bool> const& terminate,
    IResearchView::DataStore& store,
    irs::async_utils::read_write_mutex& storeMutex
) {
  SCOPED_LOCK(_mutex);
  _pending.emplace_back(resourceMutex, terminate, name, store, storeMutex);
}

void IResearchViewSyncWorker::refresh() {
  _metaRefresh.store(true);
  SCOPED_LOCK(_mutex);
  _cond.notify_all(); // wake up threads
}

NS_END // iresearch
NS_END // arangodb

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------