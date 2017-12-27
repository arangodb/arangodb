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
#include "store/fs_directory.hpp"
#include "utils/directory_utils.hpp"
#include "utils/memory.hpp"
#include "utils/misc.hpp"
#include "utils/utf8_path.hpp"

#include "ApplicationServerHelper.h"
#include "IResearchAttributes.h"
#include "IResearchDocument.h"
#include "IResearchOrderFactory.h"
#include "IResearchFilterFactory.h"
#include "IResearchFeature.h"
#include "IResearchLink.h"
#include "ExpressionFilter.h"
#include "AqlHelper.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Aql/AstNode.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/SortCondition.h"
#include "Basics/Result.h"
#include "Basics/files.h"
#include "Logger/Logger.h"
#include "Logger/LogMacros.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "StorageEngine/StorageEngine.h"
#include "RestServer/DatabaseFeature.h"
#include "RestServer/DatabasePathFeature.h"
#include "RestServer/FlushFeature.h"
#include "Transaction/StandaloneContext.h"
#include "Utils/CollectionNameResolver.h"
#include "Transaction/UserTransaction.h"
#include "velocypack/Builder.h"
#include "velocypack/Iterator.h"
#include "VocBase/LocalDocumentId.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/LogicalView.h"
#include "VocBase/PhysicalView.h"

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

////////////////////////////////////////////////////////////////////////////////
/// @brief the name of the field in the iResearch View definition denoting the
///        corresponding link definitions
////////////////////////////////////////////////////////////////////////////////
const std::string LINKS_FIELD("links");

typedef irs::async_utils::read_write_mutex::write_mutex WriteMutex;

////////////////////////////////////////////////////////////////////////////////
/// @brief generates user-friendly description of the specified view
////////////////////////////////////////////////////////////////////////////////
std::string toString(arangodb::iresearch::IResearchView const& view) {
  std::string str(arangodb::iresearch::IResearchView::type());
  str += ":";
  str += std::to_string(view.id());
  return str;
}

////////////////////////////////////////////////////////////////////////////////
/// @returns 'Flush' feature from AppicationServer
////////////////////////////////////////////////////////////////////////////////
inline arangodb::FlushFeature* getFlushFeature() noexcept {
  return arangodb::iresearch::getFeature<arangodb::FlushFeature>("Flush");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief construct an absolute path from the IResearchViewMeta configuration
////////////////////////////////////////////////////////////////////////////////
bool appendAbsolutePersistedDataPath(
    std::string& buf,
    arangodb::iresearch::IResearchView const& view,
    arangodb::iresearch::IResearchViewMeta const& meta
) {
  auto& path = meta._dataPath;

  if (path.empty()) {
    return false; // empty data path should have been set to a default value
  }

  if (TRI_PathIsAbsolute(path)) {
    buf = path;

    return true;
  }

  auto* feature = arangodb::iresearch::getFeature<arangodb::DatabasePathFeature>("DatabasePath");

  if (!feature) {
    return false;
  }

  // get base path from DatabaseServerFeature (similar to MMFilesEngine)
  irs::utf8_path absPath(feature->directory());
  static std::string subPath("databases");

  absPath/=subPath;
  absPath/=path;
  buf = absPath.utf8();

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief append link removal directives to the builder and return success
////////////////////////////////////////////////////////////////////////////////
bool appendLinkRemoval(
    arangodb::velocypack::Builder& builder,
    arangodb::iresearch::IResearchViewMeta const& meta
) {
  if (!builder.isOpenObject()) {
    return false;
  }

  for (auto& entry: meta._collections) {
    builder.add(
      std::to_string(entry),
      arangodb::velocypack::Value(arangodb::velocypack::ValueType::Null)
    );
  }

  return true;
}

arangodb::Result createPersistedDataDirectory(
    irs::directory::ptr& dstDirectory, // out param
    irs::index_writer::ptr& dstWriter, // out param
    std::string const& dstDataPath,
    irs::directory_reader srcReader, // !srcReader ==  do not import
    TRI_voc_cid_t viewId
) noexcept {
  try {
    auto directory = irs::directory::make<irs::fs_directory>(dstDataPath);

    if (!directory) {
      return arangodb::Result(
        TRI_ERROR_BAD_PARAMETER,
        std::string("error creating persistent directry for iResearch view '") + std::to_string(viewId) + "' at path '" + dstDataPath + "'"
      );
    }

    // for the case where the data directory is moved into the same physical location (possibly different path)
    // 1. open the writer on the last index meta (will prevent cleaning files of the last index meta)
    // 2. clear the index (new index meta)
    // 3. populate index from the reader
    auto format = irs::formats::get(IRESEARCH_STORE_FORMAT);
    auto writer = irs::index_writer::make(*directory, format, irs::OM_CREATE_APPEND);

    if (!writer) {
      return arangodb::Result(
        TRI_ERROR_BAD_PARAMETER,
        std::string("error creating persistent writer for iResearch view '") + std::to_string(viewId) + "' at path '" + dstDataPath + "'"
      );
    }

    irs::all filter;

    writer->remove(filter);
    writer->commit(); // clear destination directory

    if (srcReader) {
      srcReader = srcReader.reopen();
      writer->import(srcReader);
      writer->commit(); // initialize 'store' as a copy of the existing store
    }

    dstDirectory = std::move(directory);
    dstWriter = std::move(writer);

    return arangodb::Result(/*TRI_ERROR_NO_ERROR*/);
  } catch (std::exception const& e) {
    LOG_TOPIC(WARN, arangodb::iresearch::IResearchFeature::IRESEARCH)
      << "caught exception while creating iResearch view '" << viewId
      << "' data path '" + dstDataPath + "': " << e.what();
    IR_EXCEPTION();
    return arangodb::Result(
      TRI_ERROR_BAD_PARAMETER,
      std::string("error creating iResearch view '") + std::to_string(viewId) + "' data path '" + dstDataPath + "'"
    );
  } catch (...) {
    LOG_TOPIC(WARN, arangodb::iresearch::IResearchFeature::IRESEARCH)
      << "caught exception while creating iResearch view '" << viewId
      << "' data path '" + dstDataPath + "'";
    IR_EXCEPTION();
    return arangodb::Result(
      TRI_ERROR_BAD_PARAMETER,
      std::string("error creating iResearch view '") + std::to_string(viewId) + "' data path '" + dstDataPath + "'"
    );
  }
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
    LOG_TOPIC(WARN, arangodb::iresearch::IResearchFeature::IRESEARCH)
      << "caught error while calculating size of iResearch view '" << viewId << "': " << e.what();
    IR_EXCEPTION();
  } catch (...) {
    LOG_TOPIC(WARN, arangodb::iresearch::IResearchFeature::IRESEARCH)
      << "caught error while calculating size of iResearch view '" << viewId << "'";
    IR_EXCEPTION();
  }

  return size;
}

arangodb::iresearch::IResearchLink* findFirstMatchingLink(
    arangodb::LogicalCollection const& collection,
    arangodb::iresearch::IResearchView const& view
) {
  for (auto& index: collection.getIndexes()) {
    if (!index || arangodb::Index::TRI_IDX_TYPE_IRESEARCH_LINK != index->type()) {
      continue; // not an iresearch Link
    }

    // TODO FIXME find a better way to retrieve an iResearch Link
    // cannot use static_cast/reinterpret_cast since Index is not related to IResearchLink
    auto* link = dynamic_cast<arangodb::iresearch::IResearchLink*>(index.get());

    if (link && *link == view) {
      return link; // found required link
    }
  }

  return nullptr;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief register a callback to be called after recovery to persist properties
////////////////////////////////////////////////////////////////////////////////
arangodb::Result persistProperties(
    arangodb::PhysicalView& view,
    std::shared_ptr<arangodb::iresearch::AsyncValid> const& valid
) {
  auto* feature = arangodb::iresearch::getFeature<arangodb::DatabaseFeature>("Database");

  if (!feature) {
    return view.persistProperties(); // database cannot be in recovery if there is no Database feature
  }

  return feature->registerPostRecoveryCallback([&view, valid]()->arangodb::Result {
    arangodb::iresearch::ReadMutex mutex(valid->_mutex);
    SCOPED_LOCK(mutex); // ensure view does not get deallocated before call back finishes

    if (!valid->_valid) {
      LOG_TOPIC(WARN, arangodb::iresearch::IResearchFeature::IRESEARCH)
        << "Invalid call to post-recovery callback of iResearch view";

      return arangodb::Result(); // view no longer in recovery state
    }

    return view.persistProperties();
  });
}

arangodb::Result updateLinks(
    TRI_vocbase_t& vocbase,
    arangodb::iresearch::IResearchView& view,
    arangodb::velocypack::Slice const& links
) noexcept {
  try {
    if (!links.isObject()) {
      return arangodb::Result(
        TRI_ERROR_BAD_PARAMETER,
        std::string("error parsing link parameters from json for iResearch view '") + std::to_string(view.id()) + "'"
      );
    }

    struct State {
      arangodb::LogicalCollection* _collection = nullptr;
      size_t _collectionsToLockOffset; // std::numeric_limits<size_t>::max() == removal only
      arangodb::iresearch::IResearchLink const* _link = nullptr;
      size_t _linkDefinitionsOffset;
      bool _valid = true;
      State(size_t collectionsToLockOffset)
        : State(collectionsToLockOffset, std::numeric_limits<size_t>::max()) {}
      State(size_t collectionsToLockOffset, size_t linkDefinitionsOffset)
        : _collectionsToLockOffset(collectionsToLockOffset), _linkDefinitionsOffset(linkDefinitionsOffset) {}
    };
    std::vector<std::string> collectionsToLock;
    std::vector<std::pair<arangodb::velocypack::Builder, arangodb::iresearch::IResearchLinkMeta>> linkDefinitions;
    std::vector<State> linkModifications;

    for (arangodb::velocypack::ObjectIterator linksItr(links); linksItr.valid(); ++linksItr) {
      auto collection = linksItr.key();

      if (!collection.isString()) {
        return arangodb::Result(
          TRI_ERROR_BAD_PARAMETER,
          std::string("error parsing link parameters from json for iResearch view '") + std::to_string(view.id()) + "' offset '" + arangodb::basics::StringUtils::itoa(linksItr.index()) + '"'
        );
      }

      auto link = linksItr.value();
      auto collectionName = collection.copyString();

      if (link.isNull()) {
        linkModifications.emplace_back(collectionsToLock.size());
        collectionsToLock.emplace_back(collectionName);

        continue; // only removal requested
      }

      arangodb::velocypack::Builder namedJson;

      namedJson.openObject();

      if (!arangodb::iresearch::mergeSlice(namedJson, link)
          || !arangodb::iresearch::IResearchLink::setType(namedJson)
          || !arangodb::iresearch::IResearchLink::setView(namedJson, view.id())) {
        return arangodb::Result(
          TRI_ERROR_INTERNAL,
          std::string("failed to update link definition with the view name while updating iResearch view '") + std::to_string(view.id()) + "' collection '" + collectionName + "'"
        );
      }

      namedJson.close();

      std::string error;
      arangodb::iresearch::IResearchLinkMeta linkMeta;

      if (!linkMeta.init(namedJson.slice(), error)) {
        return arangodb::Result(
          TRI_ERROR_BAD_PARAMETER,
          std::string("error parsing link parameters from json for iResearch view '") + std::to_string(view.id()) + "' collection '" + collectionName + "' error '" + error + "'"
        );
      }

      linkModifications.emplace_back(collectionsToLock.size(), linkDefinitions.size());
      collectionsToLock.emplace_back(collectionName);
      linkDefinitions.emplace_back(std::move(namedJson), std::move(linkMeta));
    }

    if (collectionsToLock.empty()) {
      return arangodb::Result(/*TRI_ERROR_NO_ERROR*/); // nothing to update
    }

    static std::vector<std::string> const EMPTY;
    arangodb::transaction::UserTransaction trx(
      arangodb::transaction::StandaloneContext::Create(&vocbase),
      EMPTY, // readCollections
      EMPTY, // writeCollections
      collectionsToLock, // exclusiveCollections
      arangodb::transaction::Options() // use default lock timeout
    );
    auto res = trx.begin();

    if (res.fail()) {
      return res;
//      return arangodb::Result(
//        res.error,
//        std::string("failed to start collection updating transaction for iResearch view '") + view.name() + "'"
//      );
    }

    auto* resolver = trx.resolver();

    if (!resolver) {
      return arangodb::Result(
        TRI_ERROR_INTERNAL,
        std::string("failed to get resolver from transaction while updating while iResearch view '") + std::to_string(view.id()) + "'"
      );
    }

    {
      std::unordered_set<TRI_voc_cid_t> collectionsToRemove; // track removal for potential reindex

      // resolve corresponding collection and link
      for (auto itr = linkModifications.begin(); itr != linkModifications.end();) {
        auto& state = *itr;
        auto& collectionName = collectionsToLock[state._collectionsToLockOffset];

        state._collection = const_cast<arangodb::LogicalCollection*>(resolver->getCollectionStruct(collectionName));

        if (!state._collection) {
          return arangodb::Result(
            TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND,
            std::string("failed to get collection while updating iResearch view '") + std::to_string(view.id()) + "' collection '" + collectionName + "'"
          );
        }

        state._link = findFirstMatchingLink(*(state._collection), view);

        // remove modification state if removal of non-existant link
        if (!state._link // links currently does not exist
            && state._linkDefinitionsOffset >= linkDefinitions.size()) { // link removal request
          view.drop(state._collection->cid()); // drop any stale data for the specified collection
          itr = linkModifications.erase(itr);
          continue;
        }

        if (state._link
            && state._linkDefinitionsOffset >= linkDefinitions.size()) { // link removal request
          collectionsToRemove.emplace(state._collection->cid());
        }

        ++itr;
      }

      // remove modification state if no change on existing link and reindex not requested
      for (auto itr = linkModifications.begin(); itr != linkModifications.end();) {
        auto& state = *itr;

        // remove modification state if no change on existing link or
        if (state._link // links currently exists
            && state._linkDefinitionsOffset < linkDefinitions.size() // link creation request
            && collectionsToRemove.find(state._collection->cid()) == collectionsToRemove.end() // not a reindex request
            && *(state._link) == linkDefinitions[state._linkDefinitionsOffset].second) { // link meta not modified
          itr = linkModifications.erase(itr);
          continue;
        }

        ++itr;
      }
    }

    // execute removals
    for (auto& state: linkModifications) {
      if (state._link
          && state._linkDefinitionsOffset >= linkDefinitions.size()) { // link removal request
        state._valid = state._collection->dropIndex(state._link->id());
      }
    }

    // execute additions
    for (auto& state: linkModifications) {
      if (state._valid && state._linkDefinitionsOffset < linkDefinitions.size()) {
        bool isNew = false;
        auto linkPtr = state._collection->createIndex(&trx, linkDefinitions[state._linkDefinitionsOffset].first.slice(), isNew);

        // TODO FIXME find a better way to retrieve an iResearch Link
        // cannot use static_cast/reinterpret_cast since Index is not related to IResearchLink
        auto* ptr = dynamic_cast<arangodb::iresearch::IResearchLink*>(linkPtr.get());

        state._valid = ptr && isNew;
      }
    }

    std::string error;

    // validate success
    for (auto& state: linkModifications) {
      if (!state._valid) {
        error.append(error.empty() ? "" : ", ").append(collectionsToLock[state._collectionsToLockOffset]);
      }
    }

    if (error.empty()) {
      return arangodb::Result(trx.commit());
    }

    return arangodb::Result(
      TRI_ERROR_ARANGO_ILLEGAL_STATE,
      std::string("failed to update links while updating iResearch view '") + std::to_string(view.id()) + "', retry same request or examine errors for collections: " + error
    );
  } catch (std::exception const& e) {
    LOG_TOPIC(WARN, arangodb::iresearch::IResearchFeature::IRESEARCH)
      << "caught exception while updating links for iResearch view '" << view.id() << "': " << e.what();
    IR_EXCEPTION();
    return arangodb::Result(
      TRI_ERROR_BAD_PARAMETER,
      std::string("error updating links for iResearch view '") + std::to_string(view.id()) + "'"
    );
  } catch (...) {
    LOG_TOPIC(WARN, arangodb::iresearch::IResearchFeature::IRESEARCH)
      << "caught exception while updating links for iResearch view '" << view.id() << "'";
    IR_EXCEPTION();
    return arangodb::Result(
      TRI_ERROR_BAD_PARAMETER,
      std::string("error updating links for iResearch view '") + std::to_string(view.id()) + "'"
    );
  }
}

// inserts ArangoDB document into IResearch index
inline void insertDocument(
    irs::index_writer::document& doc,
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

NS_END

NS_BEGIN(arangodb)
NS_BEGIN(iresearch)

///////////////////////////////////////////////////////////////////////////////
/// --SECTION--                                   CompoundReader implementation
///////////////////////////////////////////////////////////////////////////////

CompoundReader::CompoundReader(irs::async_utils::read_write_mutex& mutex)
  : _mutex(mutex), _lock(_mutex) {
}

CompoundReader::CompoundReader(CompoundReader&& other) noexcept
  : _mutex(other._mutex),
    _lock(_mutex, std::adopt_lock),
    _readers(std::move(other._readers)),
    _subReaders(std::move(other._subReaders)) {
  other._lock.release();
}

void CompoundReader::add(irs::directory_reader const& reader) {
  _readers.emplace_back(reader);

  for(auto& entry: _readers.back()) {
    const auto* pkColumn = entry.column_reader(arangodb::iresearch::DocumentPrimaryKey::PK());

    if (!pkColumn) {
      LOG_TOPIC(WARN, arangodb::iresearch::IResearchFeature::IRESEARCH)
        << "encountered a sub-reader without a primary key column while creating a reader for iResearch view, ignoring";

      continue;
    }

    _subReaders.emplace_back(&entry, pkColumn->values());
  }
}

irs::index_reader::reader_iterator CompoundReader::begin() const {
  return reader_iterator(new IteratorImpl(_subReaders.begin()));
}

uint64_t CompoundReader::docs_count(const irs::string_ref& field) const {
  uint64_t count = 0;

  for (auto& entry: _subReaders) {
    count += entry.first->docs_count(field);
  }

  return count;
}

uint64_t CompoundReader::docs_count() const {
  uint64_t count = 0;

  for (auto& entry: _subReaders) {
    count += entry.first->docs_count();
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
  _writer->commit();
  _reader = _reader.reopen(); // update reader
}

IResearchView::MemoryStore::MemoryStore() {
  auto format = irs::formats::get(IRESEARCH_STORE_FORMAT);

  _directory = irs::directory::make<irs::memory_directory>();

  // create writer before reader to ensure data directory is present
  _writer = irs::index_writer::make(*_directory, format, irs::OM_CREATE_APPEND);
  _writer->commit(); // initialize 'store'
  _reader = irs::directory_reader::open(*_directory); // open after 'commit' for valid 'store'
}

IResearchView::SyncState::PolicyState::PolicyState(
    size_t intervalStep,
    std::shared_ptr<irs::index_writer::consolidation_policy_t> policy
): _intervalCount(0), _intervalStep(intervalStep), _policy(policy) {
}

IResearchView::SyncState::SyncState() noexcept
  : _cleanupIntervalCount(0),
    _cleanupIntervalStep(0) {
}

IResearchView::SyncState::SyncState(
    IResearchViewMeta::CommitMeta const& meta
): SyncState() {
  _cleanupIntervalStep = meta._cleanupIntervalStep;

  for (auto& entry: meta._consolidationPolicies) {
    if (entry.policy()) {
      _consolidationPolicies.emplace_back(
        entry.intervalStep(),
        irs::memory::make_unique<irs::index_writer::consolidation_policy_t>(entry.policy())
      );
    }
  }
}

IResearchView::TidStore::TidStore(
    transaction::Methods& trx,
    std::function<void(transaction::Methods*)> const& trxCallback
) {
  // FIXME trx copies the provided callback inside,
  // probably it's better to store just references instead

  // register callback for newly created transactions
  trx.registerCallback(trxCallback);
}

IResearchView::IResearchView(
  arangodb::LogicalView* view,
  arangodb::velocypack::Slice const& info
) : ViewImplementation(view, info),
    FlushTransaction(toString(*this)),
   _asyncMetaRevision(1),
   _asyncTerminate(false),
   _memoryNode(&_memoryNodes[0]), // set current memory node (arbitrarily 0)
   _toFlush(&_memoryNodes[1]), // set flush-pending memory node (not same as _memoryNode)
   _threadPool(0, 0), // 0 == create pool with no threads, i.e. not running anything
   _inRecovery(false),
   _valid(irs::memory::make_unique<AsyncValid>(true)) {

  // set up in-recovery insertion hooks
  auto* feature = arangodb::iresearch::getFeature<arangodb::DatabaseFeature>("Database");

  if (feature) {
    auto valid = _valid; // create copy for lambda

    feature->registerPostRecoveryCallback([this, valid]()->arangodb::Result {
      arangodb::iresearch::ReadMutex mutex(valid->_mutex);
      SCOPED_LOCK(mutex); // ensure view does not get deallocated before call back finishes

      if (!valid->_valid) {
        LOG_TOPIC(WARN, arangodb::iresearch::IResearchFeature::IRESEARCH)
          << "Invalid call to post-recovery callback of iResearch view";

        return arangodb::Result(); // view no longer in recovery state
      }

      verifyKnownCollections();

      if (_storePersisted) {
        LOG_TOPIC(DEBUG, iresearch::IResearchFeature::IRESEARCH)
          << "starting persisted-sync sync for iResearch view '" << id() << "'";

        try {
          _storePersisted.sync();
        } catch (std::exception const& e) {
          LOG_TOPIC(ERR, iresearch::IResearchFeature::IRESEARCH)
            << "caught exception while committing persisted store for iResearch view '" << id()
            << "': " << e.what();

          return arangodb::Result(TRI_ERROR_INTERNAL, e.what());
        } catch (...) {
          LOG_TOPIC(ERR, iresearch::IResearchFeature::IRESEARCH)
            << "caught exception while committing persisted store for iResearch view '" << id() << "'";

          return arangodb::Result(TRI_ERROR_INTERNAL);
        }

        LOG_TOPIC(DEBUG, iresearch::IResearchFeature::IRESEARCH)
          << "finished persisted-sync sync for iResearch view '" << id() << "'";
      }

      _threadPool.max_threads(_meta._threadsMaxTotal); // start pool
      _inRecovery = false;

      return arangodb::Result();
    });
  }

  // initialize round-robin memory store chain
  auto it = _memoryNodes;
  for (auto next = it + 1, end = std::end(_memoryNodes); next < end; ++it, ++next) {
    it->_next = next;
  }
  it->_next = _memoryNodes;

  // initialize transaction callback
  _transactionCallback = [this](transaction::Methods* trx)->void {
    if (!trx || !trx->state()) {
      LOG_TOPIC(WARN, iresearch::IResearchFeature::IRESEARCH)
        << "failed to find transaction id while processing transaction callback for iResearch view '" << id() << "'";
      return; // 'trx' and transaction state required
    }

    switch (trx->status()) {
     case transaction::Status::ABORTED: {
      auto res = finish(trx->state()->id(), false);

      if (TRI_ERROR_NO_ERROR != res) {
        LOG_TOPIC(WARN, iresearch::IResearchFeature::IRESEARCH)
          << "failed to finish abort while processing transaction callback for iResearch view '" << id() << "'";
      }

      return;
     }
     case transaction::Status::COMMITTED: {
      auto res = finish(trx->state()->id(), true);

      if (TRI_ERROR_NO_ERROR != res) {
        LOG_TOPIC(WARN, iresearch::IResearchFeature::IRESEARCH)
          << "failed to finish commit while processing transaction callback for iResearch view '" << id() << "'";
      } else if (trx->state()->options().waitForSync && !sync()) {
        LOG_TOPIC(WARN, iresearch::IResearchFeature::IRESEARCH)
          << "failed to sync while processing transaction callback for iResearch view '" << id() << "'";
      }

      return;
     }
     default:
      {} // NOOP
    }
  };

  // add asynchronous commit job
  _threadPool.run(
    [this]()->void {
    struct State: public SyncState {
      size_t _asyncMetaRevision;
      size_t _commitIntervalMsecRemainder;
      size_t _commitTimeoutMsec;

      State():
        SyncState(),
        _asyncMetaRevision(0), // '0' differs from IResearchView constructor above
        _commitIntervalMsecRemainder(std::numeric_limits<size_t>::max()),
        _commitTimeoutMsec(0) {
      }
      State(IResearchViewMeta::CommitMeta const& meta)
        : SyncState(meta),
          _asyncMetaRevision(0), // '0' differs from IResearchView constructor above
          _commitIntervalMsecRemainder(std::numeric_limits<size_t>::max()),
          _commitTimeoutMsec(meta._commitTimeoutMsec) {
      }
    };

    State state;
    arangodb::iresearch::ReadMutex mutex(_mutex); // '_meta' can be asynchronously modified

    for(;;) {
      // sleep until timeout
      {
        SCOPED_LOCK_NAMED(mutex, lock); // for '_meta._commit._commitIntervalMsec'
        SCOPED_LOCK_NAMED(_asyncMutex, asyncLock); // aquire before '_asyncTerminate' check

        if (_asyncTerminate.load()) {
          break;
        }

        if (!_meta._commit._commitIntervalMsec) {
          lock.unlock(); // do not hold read lock while waiting on condition
          _asyncCondition.wait(asyncLock); // wait forever
          continue;
        }

        auto startTime = std::chrono::system_clock::now();
        auto endTime = startTime
          + std::chrono::milliseconds(std::min(state._commitIntervalMsecRemainder, _meta._commit._commitIntervalMsec))
          ;

        lock.unlock(); // do not hold read lock while waiting on condition
        state._commitIntervalMsecRemainder = std::numeric_limits<size_t>::max(); // longest possible time assuming an uninterrupted sleep

        if (std::cv_status::timeout != _asyncCondition.wait_until(asyncLock, endTime)) {
          auto nowTime = std::chrono::system_clock::now();

          // if still need to sleep more then must relock '_meta' and sleep for min (remainder, interval)
          if (nowTime < endTime) {
            state._commitIntervalMsecRemainder = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - nowTime).count();

            continue; // need to reaquire lock to chech for change in '_meta'
          }
        }

        if (_asyncTerminate.load()) {
          break;
        }
      }

      // reload state if required
      if (_asyncMetaRevision.load() != state._asyncMetaRevision) {
        SCOPED_LOCK(mutex);
        state = State(_meta._commit);
        state._asyncMetaRevision = _asyncMetaRevision.load();
      }

      // perform sync
      sync(state, state._commitTimeoutMsec);
    }
  });
}

IResearchView::~IResearchView() {
  {
    WriteMutex mutex(_valid->_mutex);
    SCOPED_LOCK(mutex);
    _valid->_valid = false; // the view is being deallocated, its use is not longer valid
  }

  _asyncTerminate.store(true); // mark long-running async jobs for terminatation

  {
    SCOPED_LOCK(_asyncMutex);
    _asyncCondition.notify_all(); // trigger reload of settings for async jobs
  }

  _threadPool.max_threads_delta(int(std::max(size_t(std::numeric_limits<int>::max()), _threadPool.tasks_pending()))); // finish ASAP
  _threadPool.stop();

  _flushCallback.reset(); // unregister flush callback from flush thread

  std::vector<sptr> viewPointers;

  {
    WriteMutex mutex(_mutex); // '_meta' can be asynchronously read
    SCOPED_LOCK(mutex);

    // unregister all registred links from view and retain a copy so that can call destructor outside of lock
    for (auto& entry: _registeredLinks) {
      if (entry.second) {
        viewPointers.emplace_back(entry.second->updateView(nullptr));
      }
    }

    if (_storePersisted) {
      _storePersisted._writer->commit();
      _storePersisted._writer->close();
      _storePersisted._writer.reset();
      _storePersisted._directory->close();
      _storePersisted._directory.reset();
    }
  }
}

bool IResearchView::appendKnownCollections(
    std::unordered_set<TRI_voc_cid_t>& set
) const {
  CompoundReader reader(_mutex); // will aquire read-lock since members can be asynchronously updated

  reader.add(_memoryNode->_store._reader);
  SCOPED_LOCK(_toFlush->_readMutex);
  reader.add(_toFlush->_store._reader);

  if (_storePersisted) {
    reader.add(_storePersisted._reader);
  }

  set.insert(_meta._collections.begin(), _meta._collections.end());

  if (arangodb::iresearch::appendKnownCollections(set, reader)) {
    return true;
  }

  LOG_TOPIC(ERR, iresearch::IResearchFeature::IRESEARCH)
    << "failed to collect CIDs for IResearch view '" << id() << "'";

  return false;
}

bool IResearchView::cleanup(size_t maxMsec /*= 0*/) {
  arangodb::iresearch::ReadMutex mutex(_mutex);
  auto thresholdSec = TRI_microtime() + maxMsec/1000.0;

  try {
    SCOPED_LOCK(mutex);

    LOG_TOPIC(DEBUG, iresearch::IResearchFeature::IRESEARCH)
      << "starting active memory-store cleanup for iResearch view '" << id() << "'";
    irs::directory_utils::remove_all_unreferenced(*(_memoryNode->_store._directory));
    LOG_TOPIC(DEBUG, iresearch::IResearchFeature::IRESEARCH)
      << "finished active memory-store cleanup for iResearch view '" << id() << "'";

    if (maxMsec && TRI_microtime() >= thresholdSec) {
      return true; // skip if timout exceeded
    }

    LOG_TOPIC(DEBUG, iresearch::IResearchFeature::IRESEARCH)
      << "starting flushing memory-store cleanup for iResearch view '" << id() << "'";
    irs::directory_utils::remove_all_unreferenced(*(_toFlush->_store._directory));
    LOG_TOPIC(DEBUG, iresearch::IResearchFeature::IRESEARCH)
      << "finished flushing memory-store cleanup for iResearch view '" << id() << "'";

    if (maxMsec && TRI_microtime() >= thresholdSec) {
      return true; // skip if timout exceeded
    }

    if (_storePersisted) {
      LOG_TOPIC(DEBUG, iresearch::IResearchFeature::IRESEARCH)
        << "starting persisted-store cleanup for iResearch view '" << id() << "'";
      irs::directory_utils::remove_all_unreferenced(*(_storePersisted._directory));
      LOG_TOPIC(DEBUG, iresearch::IResearchFeature::IRESEARCH)
        << "finished persisted-store cleanup for iResearch view '" << id() << "'";
    }

    return true;
  } catch (std::exception const& e) {
    LOG_TOPIC(WARN, iresearch::IResearchFeature::IRESEARCH)
      << "caught exception during cleanup of iResearch view '" << id() << "': " << e.what();
    IR_EXCEPTION();
  } catch (...) {
    LOG_TOPIC(WARN, iresearch::IResearchFeature::IRESEARCH)
      << "caught exception during cleanup of iResearch view '" << id() << "'";
    IR_EXCEPTION();
  }

  return false;
}

void IResearchView::drop() {
  // drop all known links
  if (_logicalView && _logicalView->vocbase()) {
    arangodb::velocypack::Builder builder;

    {
      ReadMutex mutex(_mutex);
      SCOPED_LOCK(mutex); // '_meta' can be asynchronously updated

      builder.openObject();

      if (!appendLinkRemoval(builder, _meta)) {
        throw std::runtime_error(std::string("failed to construct link removal directive while removing iResearch view '") + std::to_string(id()) + "'");
      }

      builder.close();
    }

    if (!updateLinks(*(_logicalView->vocbase()), *this, builder.slice()).ok()) {
      throw std::runtime_error(std::string("failed to remove links while removing iResearch view '") + std::to_string(id()) + "'");
    }
  }

  _asyncTerminate.store(true); // mark long-running async jobs for terminatation

  {
    SCOPED_LOCK(_asyncMutex);
    _asyncCondition.notify_all(); // trigger reload of settings for async jobs
  }

  _threadPool.stop();

  WriteMutex mutex(_mutex); // members can be asynchronously updated
  SCOPED_LOCK(mutex);

  // ArangoDB global consistency check, no known dangling links
  if (!_meta._collections.empty() || !_registeredLinks.empty()) {
    throw std::runtime_error(std::string("links still present while removing iResearch view '") + std::to_string(id()) + "'");
  }

  // ...........................................................................
  // if an exception occurs below than a drop retry would most likely happen
  // ...........................................................................
  try {
    _storeByTid.clear();

    auto& memoryStore = activeMemoryStore();
    memoryStore._writer->close();
    memoryStore._writer.reset();
    memoryStore._directory->close();
    memoryStore._directory.reset();

    if (_storePersisted) {
      _storePersisted._writer->close();
      _storePersisted._writer.reset();
      _storePersisted._directory->close();
      _storePersisted._directory.reset();
    }

    if (!TRI_IsDirectory(_meta._dataPath.c_str()) || TRI_ERROR_NO_ERROR == TRI_RemoveDirectory(_meta._dataPath.c_str())) {
      return; // success
    }
  } catch (std::exception const& e) {
    LOG_TOPIC(WARN, iresearch::IResearchFeature::IRESEARCH)
      << "caught exception while removing iResearch view '" << id() << "': " << e.what();
    IR_EXCEPTION();
    throw;
  } catch (...) {
    LOG_TOPIC(WARN, iresearch::IResearchFeature::IRESEARCH)
      << "caught exception while removing iResearch view '" << id() << "'";
    IR_EXCEPTION();
    throw;
  }

  throw std::runtime_error(std::string("failed to remove iResearch view '") + arangodb::basics::StringUtils::itoa(id()) + "'");
}

int IResearchView::drop(TRI_voc_cid_t cid) {
  if (!_logicalView || !_logicalView->getPhysical()) {
    LOG_TOPIC(WARN, iresearch::IResearchFeature::IRESEARCH)
      << "failed to find meta-store while dropping collection from iResearch view '" << id()
      << "' cid '" << cid << "'";

    return TRI_ERROR_INTERNAL;
  }

  auto& metaStore = *(_logicalView->getPhysical());
  std::shared_ptr<irs::filter> shared_filter(iresearch::FilterFactory::filter(cid));
  WriteMutex mutex(_mutex); // '_storeByTid' can be asynchronously updated
  SCOPED_LOCK(mutex);
  bool meta_updated = _meta._collections.erase(cid); // will no longer be fully indexed
  mutex.unlock(true); // downgrade to a read-lock

  // no need to persist properties if thy were not modified (optimization)
  if (meta_updated) {
    auto res = persistProperties(metaStore, _valid); // persist '_meta' definition

    if (!res.ok()) {
      LOG_TOPIC(WARN, iresearch::IResearchFeature::IRESEARCH)
        << "failed to persist view definition while dropping collection from iResearch view '" << id()
        << "' cid '" << cid << "'";

      return res.errorNumber();
    }
  }

  // ...........................................................................
  // if an exception occurs below than a drop retry would most likely happen
  // ...........................................................................
  try {
    for (auto& tidStore: _storeByTid) {
      tidStore.second._store._writer->remove(shared_filter);
    }

    auto& memoryStore = activeMemoryStore();
    memoryStore._writer->remove(shared_filter);

    if (_storePersisted) {
      _storePersisted._writer->remove(shared_filter);
    }

    return TRI_ERROR_NO_ERROR;
  } catch (std::exception const& e) {
    LOG_TOPIC(WARN, iresearch::IResearchFeature::IRESEARCH)
      << "caught exception while removing from iResearch view '" << id()
      << "', collection '" << cid << "': " << e.what();
    IR_EXCEPTION();
  } catch (...) {
    LOG_TOPIC(WARN, iresearch::IResearchFeature::IRESEARCH)
      << "caught exception while removing from iResearch view '" << id()
      << "', collection '" << cid << "'";
    IR_EXCEPTION();
  }

  return TRI_ERROR_INTERNAL;
}

int IResearchView::finish(TRI_voc_tid_t tid, bool commit) {
  std::vector<std::shared_ptr<irs::filter>> removals;
  DataStore trxStore;

  {
    SCOPED_LOCK(_trxStoreMutex); // '_storeByTid' can be asynchronously updated

    auto tidStoreItr = _storeByTid.find(tid);

    if (tidStoreItr == _storeByTid.end()) {
      return TRI_ERROR_NO_ERROR; // nothing to finish
    }

    if (!commit) {
      _storeByTid.erase(tidStoreItr);

      return TRI_ERROR_NO_ERROR; // nothing more to do
    }

    // no need to lock TidStore::_mutex since have write-lock on IResearchView::_mutex
    removals = std::move(tidStoreItr->second._removals);
    trxStore = std::move(tidStoreItr->second._store);

    _storeByTid.erase(tidStoreItr);
  }

  ReadMutex mutex(_mutex); // '_memoryStore'/'_storePersisted' can be asynchronously modified
  SCOPED_LOCK(mutex);

  try {
    // transfer filters first since they only apply to pre-merge data
    for (auto& filter: removals) {
      // FIXME TODO potential problem of loss of 'remove' if:
      // 'insert' in '_toFlush' and 'remove' comes during IResearchView::commit()
      // after '_toFlush' is commit()ed but before '_toFlush' in import()ed
      _memoryNode->_store._writer->remove(filter);
      _toFlush->_store._writer->remove(filter);
    }

    // transfer filters to persisted store as well otherwise query resuts will be incorrect
    // on recovery the same removals will be replayed from the WAL
    if (_storePersisted) {
      for (auto& filter: removals) {
        _storePersisted._writer->remove(filter);
      }
    }

    auto& memoryStore = activeMemoryStore();

    trxStore._writer->commit(); // ensure have latest view in reader
    memoryStore._writer->import(trxStore._reader.reopen());

    return TRI_ERROR_NO_ERROR;
  } catch (std::exception const& e) {
    LOG_TOPIC(ERR, iresearch::IResearchFeature::IRESEARCH)
      << "caught exception while committing transaction for iResearch view '" << id()
      << "', tid '" << tid << "': " << e.what();
    IR_EXCEPTION();
  } catch (...) {
    LOG_TOPIC(ERR, iresearch::IResearchFeature::IRESEARCH)
      << "caught exception while committing transaction for iResearch view '" << id()
      << "', tid '" << tid << "'";
    IR_EXCEPTION();
  }

  return TRI_ERROR_INTERNAL;
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
    _storePersisted._writer->commit(); // finishing flush transaction
    memoryStore._writer->clear(); // prepare the store for reuse

    SCOPED_LOCK(_toFlush->_readMutex); // do not allow concurrent read since _storePersisted/_toFlush need to be updated atomically
    _storePersisted._reader = _storePersisted._reader.reopen(); // update reader
    memoryStore._reader = memoryStore._reader.reopen(); // update reader

    return TRI_ERROR_NO_ERROR;
  } catch (std::exception const& e) {
    LOG_TOPIC(ERR, iresearch::IResearchFeature::IRESEARCH)
      << "caught exception while committing memory store for iResearch view '" << id() << "': " << e.what();
    IR_EXCEPTION();
  } catch (...) {
    LOG_TOPIC(ERR, iresearch::IResearchFeature::IRESEARCH)
      << "caught exception while committing memory store for iResearch view '" << id();
    IR_EXCEPTION();
  }

  return {TRI_ERROR_INTERNAL};
}

void IResearchView::getPropertiesVPack(
  arangodb::velocypack::Builder& builder, bool forPersistence
) const {
  ReadMutex mutex(_mutex);
  SCOPED_LOCK(mutex); // '_meta'/'_links' can be asynchronously updated

  _meta.json(builder);

  if (!_logicalView || forPersistence) {
    return; // nothing more to output (persistent configuration does not need links)
  }

  std::vector<std::string> collections;

  // add CIDs of known collections to list
  for (auto& entry: _meta._collections) {
    collections.emplace_back(arangodb::basics::StringUtils::itoa(entry));
  }

  // add CIDs of registered collections to list
  for (auto& entry: _registeredLinks) {
    collections.emplace_back(std::to_string(entry.first));
  }

  arangodb::velocypack::Builder linksBuilder;

  static std::vector<std::string> const EMPTY;

  // use default lock timeout
  arangodb::transaction::Options options;
  options.waitForSync = false;
  options.allowImplicitCollections = false;

  try {
    arangodb::transaction::UserTransaction trx(
      transaction::StandaloneContext::Create(_logicalView->vocbase()),
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
            LOG_TOPIC(WARN, arangodb::iresearch::IResearchFeature::IRESEARCH)
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
    LOG_TOPIC(WARN, arangodb::iresearch::IResearchFeature::IRESEARCH)
      << "caught exception while generating json for IResearch view '" << id() << "': " << e.what();
    IR_EXCEPTION();
    return; // do not add 'links' section
  } catch (...) {
    LOG_TOPIC(WARN, arangodb::iresearch::IResearchFeature::IRESEARCH)
      << "caught exception while generating json for IResearch view '" << id() << "'";
    IR_EXCEPTION();
    return; // do not add 'links' section
  }

  builder.add(LINKS_FIELD, linksBuilder.slice());
}

TRI_voc_cid_t IResearchView::id() const noexcept {
  return _logicalView ? _logicalView->id() : 0; // 0 is an invalid view id
}

int IResearchView::insert(
    transaction::Methods& trx,
    TRI_voc_cid_t cid,
    arangodb::LocalDocumentId const& documentId,
    arangodb::velocypack::Slice const& doc,
    IResearchLinkMeta const& meta
) {
  if (!trx.state()) {
    return TRI_ERROR_BAD_PARAMETER; // 'trx' and transaction id required
  }

  DataStore* store;

  if (_inRecovery) {
    _storePersisted._writer->remove(FilterFactory::filter(cid, documentId.id()));

    store = &_storePersisted;
  } else {
    // '_storeByTid' can be asynchronously updated
    SCOPED_LOCK(_trxStoreMutex);

    auto storeItr = irs::map_utils::try_emplace(
      _storeByTid, trx.state()->id(), trx, _transactionCallback
    );

    store = &(storeItr.first->second._store);
  }

  TRI_ASSERT(store);

  FieldIterator body(doc, meta);

  if (!body.valid()) {
    return TRI_ERROR_NO_ERROR; // nothing to index
  }

  auto insert = [&body, cid, documentId](irs::index_writer::document& doc)->bool {
    insertDocument(doc, body, cid, documentId.id());

    return false; // break the loop
  };

  try {
    if (store->_writer->insert(insert)) {
      return TRI_ERROR_NO_ERROR;
    }

    LOG_TOPIC(WARN, iresearch::IResearchFeature::IRESEARCH)
      << "failed inserting into iResearch view '" << id()
      << "', collection '" << cid << "', revision '" << documentId.id() << "'";
  } catch (std::exception const& e) {
    LOG_TOPIC(WARN, iresearch::IResearchFeature::IRESEARCH)
      << "caught exception while inserting into iResearch view '" << id()
      << "', collection '" << cid << "', revision '" << documentId.id() << "': " << e.what();
    IR_EXCEPTION();
  } catch (...) {
    LOG_TOPIC(WARN, iresearch::IResearchFeature::IRESEARCH)
      << "caught exception while inserting into iResearch view '" << id()
      << "', collection '" << cid << "', revision '" << documentId.id() << "'";
    IR_EXCEPTION();
  }

  return TRI_ERROR_INTERNAL;
}

int IResearchView::insert(
    transaction::Methods& trx,
    TRI_voc_cid_t cid,
    std::vector<std::pair<arangodb::LocalDocumentId, arangodb::velocypack::Slice>> const& batch,
    IResearchLinkMeta const& meta
) {
  if (!trx.state()) {
    return TRI_ERROR_BAD_PARAMETER; // 'trx' and transaction id required
  }

  DataStore* store;

  if (_inRecovery) {
    for (auto& doc : batch) {
      _storePersisted._writer->remove(FilterFactory::filter(cid, doc.first.id()));
    }

    store = &_storePersisted;
  } else {
    // '_storeByTid' can be asynchronously updated
    SCOPED_LOCK(_trxStoreMutex);

    auto storeItr = irs::map_utils::try_emplace(
      _storeByTid, trx.state()->id(), trx, _transactionCallback
    );

    store = &(storeItr.first->second._store);
  }

  TRI_ASSERT(store);

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

  auto insert = [&meta, &begin, &end, &body, cid, &rid](irs::index_writer::document& doc)->bool {
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
      LOG_TOPIC(WARN, iresearch::IResearchFeature::IRESEARCH)
        << "failed inserting batch into iResearch view '" << id() << "', collection '" << cid;
      return TRI_ERROR_INTERNAL;
    }

    store->_writer->commit(); // no need to consolidate if batch size is set correctly
  } catch (std::exception const& e) {
    LOG_TOPIC(WARN, iresearch::IResearchFeature::IRESEARCH)
      << "caught exception while inserting batch into iResearch view '" << id() << "', collection '" << cid << e.what();
    IR_EXCEPTION();
  } catch (...) {
    LOG_TOPIC(WARN, iresearch::IResearchFeature::IRESEARCH)
      << "caught exception while inserting batch into iResearch view '" << id() << "', collection '" << cid;
    IR_EXCEPTION();
  }

  return TRI_ERROR_NO_ERROR;
}

iresearch::CompoundReader IResearchView::snapshot(transaction::Methods &trx) {
  iresearch::CompoundReader compoundReader(_mutex); // will aquire read-lock since members can be asynchronously updated

  try {
    compoundReader.add(_memoryNode->_store._reader);
    SCOPED_LOCK(_toFlush->_readMutex);
    compoundReader.add(_toFlush->_store._reader);

    if (_storePersisted) {
      compoundReader.add(_storePersisted._reader);
    }
  } catch (std::exception const& e) {
    LOG_TOPIC(WARN, iresearch::IResearchFeature::IRESEARCH)
      << "caught exception while collecting readers for querying iResearch view '" << id()
      << "': " << e.what();
    IR_EXCEPTION();
    throw;
  } catch (...) {
    LOG_TOPIC(WARN, iresearch::IResearchFeature::IRESEARCH)
      << "caught exception while collecting readers for querying iResearch view '" << id() << "'";
    IR_EXCEPTION();
    throw;
  }

  // add CIDs of known collections to transaction
  for (auto& entry: _meta._collections) {
    trx.addCollectionAtRuntime(arangodb::basics::StringUtils::itoa(entry));
  }

  // add CIDs of registered collections to transaction
  for (auto& entry: _registeredLinks) {
    trx.addCollectionAtRuntime(std::to_string(entry.first));
  }

  return compoundReader;
}

size_t IResearchView::linkCount() const noexcept {
  ReadMutex mutex(_mutex); // '_links' can be asynchronously updated
  SCOPED_LOCK(mutex);

  return _meta._collections.size();
}

bool IResearchView::linkRegister(IResearchLink& link) {
  if (!link.collection()) {
    return false; // do not register empty pointers
  }

  if (!_logicalView || !_logicalView->getPhysical()) {
    return false; // cannot persist view
  }

  auto cid = link.collection()->cid();

  WriteMutex mutex(_mutex); // '_links' can be asynchronously updated
  SCOPED_LOCK(mutex);

  // do not allow duplicate registration
  if (!_registeredLinks.emplace(cid, &link).second) {
    LOG_TOPIC(WARN, iresearch::IResearchFeature::IRESEARCH)
      << "duplicate iResearch link registration detected for iResearch view '" << id()
      <<"' cid '" << link.collection()->cid() << "' iid '" << link.id() << "'";

    return false;
  }

  auto unregistrar = [cid](IResearchView* ptr)->void {
    if (ptr) {
      WriteMutex mutex(ptr->_mutex); // '_registredLinks' can be asynchronously updated
      SCOPED_LOCK(mutex);
      ptr->_registeredLinks.erase(cid);
    }
  };

  if (!_meta._collections.emplace(cid).second) {
    // first time an IResearchLink object was seen for the specified cid
    // e.g. this happends during startup when links initially register with the view
    link.updateView(sptr(this, std::move(unregistrar))); // will not deadlock since checked for duplicate cid above

    return true;
  }

  auto& metaStore = *(_logicalView->getPhysical());
  auto res = persistProperties(metaStore, _valid); // persist '_meta' definition

  if (res.ok()) {
    link.updateView(sptr(this, std::move(unregistrar)), true); // will not deadlock since checked for duplicate cid above

    return true;
  }

  LOG_TOPIC(WARN, iresearch::IResearchFeature::IRESEARCH)
    << "failed to persist iResearch view definition during new iResearch link registration for iResearch view '" << id()
    << "' cid '" << link.collection()->cid() << "' iid '" << link.id() << "'";
  _meta._collections.erase(cid); // revert state
  _registeredLinks.erase(cid); // revert state

  return false;
}

/*static*/ IResearchView::ptr IResearchView::make(
  arangodb::LogicalView* view,
  arangodb::velocypack::Slice const& info,
  bool isNew
) {
  PTR_NAMED(IResearchView, ptr, view, info);
  auto& impl = reinterpret_cast<IResearchView&>(*ptr);
  auto& json = info.isNone() ? emptyObjectSlice() : info; // if no 'info' then assume defaults
  std::string error;

  if (!view || !impl._meta.init(json, error, *view)) {
    LOG_TOPIC(WARN, iresearch::IResearchFeature::IRESEARCH)
      << "failed to initialize iResearch view from definition, error: " << error;

    return nullptr;
  }

  return std::move(ptr);
}

size_t IResearchView::memory() const {
  ReadMutex mutex(_mutex); // view members can be asynchronously updated
  SCOPED_LOCK(mutex);
  size_t size = sizeof(IResearchView);

  size += sizeof(decltype(_registeredLinks)::value_type) * _registeredLinks.size();
  size += _meta.memory();

  for (auto& tidEntry: _storeByTid) {
    size += sizeof(tidEntry.first) + sizeof(tidEntry.second);
    size += directoryMemory(*tidEntry.second._store._directory, id());

    // no way to determine size of actual filter
    SCOPED_LOCK(tidEntry.second._mutex);
    size += tidEntry.second._removals.size() * (sizeof(decltype(tidEntry.second._removals)::pointer) + sizeof(decltype(tidEntry.second._removals)::value_type));
  }

  size += sizeof(_memoryNode) + sizeof(_toFlush) + sizeof(_memoryNodes);
  size += directoryMemory(*(_memoryNode->_store._directory), id());
  size += directoryMemory(*(_toFlush->_store._directory), id());

  if (_storePersisted) {
    size += directoryMemory(*(_storePersisted._directory), id());
  }

  return size;
}

void IResearchView::open() {
  auto* engine = arangodb::EngineSelectorFeature::ENGINE;

  if (engine) {
    _inRecovery = engine->inRecovery();
  } else {
    LOG_TOPIC(WARN, iresearch::IResearchFeature::IRESEARCH)
      << "failure to get storage engine while starting feature 'IResearchAnalyzer'";
    // assume not inRecovery()
  }

  WriteMutex mutex(_mutex); // '_meta' can be asynchronously updated
  SCOPED_LOCK(mutex);

  if (_storePersisted) {
    return; // view already open
  }

  std::string absoluteDataPath;

  try {
    auto format = irs::formats::get(IRESEARCH_STORE_FORMAT);

    if (format && appendAbsolutePersistedDataPath(absoluteDataPath, *this, _meta)) {
      _storePersisted._directory =
        irs::directory::make<irs::fs_directory>(absoluteDataPath);

      if (_storePersisted._directory) {
        // create writer before reader to ensure data directory is present
        _storePersisted._writer =
          irs::index_writer::make(*(_storePersisted._directory), format, irs::OM_CREATE_APPEND);

        if (_storePersisted._writer) {
          _storePersisted._writer->commit(); // initialize 'store'
          _threadPool.max_idle(_meta._threadsMaxIdle);

          if (!_inRecovery) {
            // start pool only if we're not in recovery now,
            // otherwise 'PostRecoveryCallback' will take care of that
            _threadPool.max_threads(_meta._threadsMaxTotal);
          }

          _storePersisted._reader
            = irs::directory_reader::open(*(_storePersisted._directory));

          if (_storePersisted._reader) {
            registerFlushCallback();
            return; // success
          }

          _storePersisted._writer.reset(); // unlock the directory
        }
      }
    }
  } catch (std::exception const& e) {
    LOG_TOPIC(WARN, iresearch::IResearchFeature::IRESEARCH)
      << "caught exception while opening iResearch view '" << id() << "': " << e.what();
    IR_EXCEPTION();
    throw;
  } catch (...) {
    LOG_TOPIC(WARN, iresearch::IResearchFeature::IRESEARCH)
      << "caught exception while opening iResearch view '" << id() << "'";
    IR_EXCEPTION();
    throw;
  }

  LOG_TOPIC(WARN, iresearch::IResearchFeature::IRESEARCH)
    << "failed to open iResearch view '" << id() << "' at: " << absoluteDataPath;

  throw std::runtime_error(
    std::string("failed to open iResearch view '") + std::to_string(id()) + "' at: " + absoluteDataPath
  );
}

int IResearchView::remove(
  transaction::Methods& trx,
  TRI_voc_cid_t cid,
  arangodb::LocalDocumentId const& documentId
) {
  if (!trx.state()) {
    return TRI_ERROR_BAD_PARAMETER; // 'trx' and transaction id required
  }

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

  TidStore* store;

  {
    SCOPED_LOCK(_trxStoreMutex); // '_storeByTid' can be asynchronously updated

    auto storeItr = irs::map_utils::try_emplace(
      _storeByTid, trx.state()->id(), trx, _transactionCallback
    );

    store = &(storeItr.first->second);
  }

  TRI_ASSERT(store);

  // ...........................................................................
  // if an exception occurs below than the transaction is droped including all
  // all of its fid stores, no impact to iResearch View data integrity
  // ...........................................................................
  try {
    store->_store._writer->remove(shared_filter);

    SCOPED_LOCK(store->_mutex); // '_removals' can be asynchronously updated
    store->_removals.emplace_back(shared_filter);

    return TRI_ERROR_NO_ERROR;
  } catch (std::exception const& e) {
    LOG_TOPIC(WARN, iresearch::IResearchFeature::IRESEARCH)
      << "caught exception while removing from iResearch view '" << id()
      << "', collection '" << cid << "', revision '" << documentId.id() << "': " << e.what();
    IR_EXCEPTION();
  } catch (...) {
    LOG_TOPIC(WARN, iresearch::IResearchFeature::IRESEARCH)
      << "caught exception while removing from iResearch view '" << id()
      << "', collection '" << cid << "', revision '" << documentId.id() << "'";
    IR_EXCEPTION();
  }

  return TRI_ERROR_INTERNAL;
}

IResearchView::MemoryStore& IResearchView::activeMemoryStore() const {
  return _memoryNode->_store;
}

bool IResearchView::sync(size_t maxMsec /*= 0*/) {
  ReadMutex mutex(_mutex);
  auto thresholdSec = TRI_microtime() + maxMsec/1000.0;

  try {
    SCOPED_LOCK(mutex);

    LOG_TOPIC(DEBUG, iresearch::IResearchFeature::IRESEARCH)
      << "starting active memory-store sync for iResearch view '" << id() << "'";
    _memoryNode->_store.sync();
    LOG_TOPIC(DEBUG, iresearch::IResearchFeature::IRESEARCH)
      << "finished memory-store sync for iResearch view '" << id() << "'";

    if (maxMsec && TRI_microtime() >= thresholdSec) {
      return true; // skip if timout exceeded
    }

    LOG_TOPIC(DEBUG, iresearch::IResearchFeature::IRESEARCH)
      << "starting pending memory-store sync for iResearch view '" << id() << "'";
    _toFlush->_store._writer->commit();

    {
      SCOPED_LOCK(_toFlush->_reopenMutex);
      _toFlush->_store._reader = _toFlush->_store._reader.reopen(); // update reader
    }

    LOG_TOPIC(DEBUG, iresearch::IResearchFeature::IRESEARCH)
      << "finished pending memory-store sync for iResearch view '" << id() << "'";

    if (maxMsec && TRI_microtime() >= thresholdSec) {
      return true; // skip if timout exceeded
    }

    // must sync persisted store as well to ensure removals are applied
    if (_storePersisted) {
      LOG_TOPIC(DEBUG, iresearch::IResearchFeature::IRESEARCH)
        << "starting persisted-sync sync for iResearch view '" << id() << "'";
      _storePersisted._writer->commit();

      {
        SCOPED_LOCK(_toFlush->_reopenMutex);
        _storePersisted._reader = _storePersisted._reader.reopen(); // update reader
      }

      LOG_TOPIC(DEBUG, iresearch::IResearchFeature::IRESEARCH)
        << "finished persisted-sync sync for iResearch view '" << id() << "'";
    }

    return true;
  } catch (std::exception const& e) {
    LOG_TOPIC(WARN, iresearch::IResearchFeature::IRESEARCH)
      << "caught exception during sync of iResearch view '" << id() << "': " << e.what();
    IR_EXCEPTION();
  } catch (...) {
    LOG_TOPIC(WARN, iresearch::IResearchFeature::IRESEARCH)
      << "caught exception during sync of iResearch view '" << id() << "'";
    IR_EXCEPTION();
  }

  return false;
}

bool IResearchView::sync(SyncState& state, size_t maxMsec /*= 0*/) {
  char runId = 0; // value not used
  auto thresholdMsec = TRI_microtime() * 1000 + maxMsec;
  ReadMutex mutex(_mutex); // '_memoryStore'/'_storePersisted' can be asynchronously modified

  LOG_TOPIC(DEBUG, iresearch::IResearchFeature::IRESEARCH)
    << "starting flush for iResearch view '" << id() << "' run id '" << size_t(&runId) << "'";

  // .............................................................................
  // apply consolidation policies
  // .............................................................................
  for (auto& entry: state._consolidationPolicies) {
    if (!entry._intervalStep || ++entry._intervalCount < entry._intervalStep) {
      continue; // skip if interval not reached or no valid policy to execute
    }

    entry._intervalCount = 0;
    LOG_TOPIC(DEBUG, iresearch::IResearchFeature::IRESEARCH)
      << "starting consolidation for iResearch view '" << id() << "' run id '" << size_t(&runId) << "'";

    try {
      SCOPED_LOCK(mutex);

      auto& memoryStore = activeMemoryStore();
      memoryStore._writer->consolidate(entry._policy, false);

      if (_storePersisted) {
        _storePersisted._writer->consolidate(entry._policy, false);
      }
    } catch (std::exception const& e) {
      LOG_TOPIC(WARN, iresearch::IResearchFeature::IRESEARCH)
        << "caught exception while consolidating iResearch view '" << id() << "': " << e.what();
      IR_EXCEPTION();
    } catch (...) {
      LOG_TOPIC(WARN, iresearch::IResearchFeature::IRESEARCH)
        << "caught exception while consolidating iResearch view '" << id() << "'";
      IR_EXCEPTION();
    }

    LOG_TOPIC(DEBUG, iresearch::IResearchFeature::IRESEARCH)
      << "finished consolidation for iResearch view '" << id() << "' run id '" << size_t(&runId) << "'";
  }

  // .............................................................................
  // apply data store commit
  // .............................................................................

  LOG_TOPIC(DEBUG, iresearch::IResearchFeature::IRESEARCH)
    << "starting commit for iResearch view '" << id() << "' run id '" << size_t(&runId) << "'";

  auto res = sync(std::max(size_t(1), size_t(thresholdMsec - TRI_microtime() * 1000))); // set min 1 msec to enable early termination

  LOG_TOPIC(DEBUG, iresearch::IResearchFeature::IRESEARCH)
    << "finished commit for iResearch view '" << id() << "' run id '" << size_t(&runId) << "'";

  // .............................................................................
  // apply cleanup
  // .............................................................................
  if (state._cleanupIntervalStep && ++state._cleanupIntervalCount >= state._cleanupIntervalStep) {
    state._cleanupIntervalCount = 0;
    LOG_TOPIC(DEBUG, iresearch::IResearchFeature::IRESEARCH)
      << "starting cleanup for iResearch view '" << id() << "' run id '" << size_t(&runId) << "'";

    cleanup(std::max(size_t(1), size_t(thresholdMsec - TRI_microtime() * 1000))); // set min 1 msec to enable early termination

    LOG_TOPIC(DEBUG, iresearch::IResearchFeature::IRESEARCH)
      << "finished cleanup for iResearch view '" << id() << "' run id '" << size_t(&runId) << "'";
  }

  LOG_TOPIC(DEBUG, iresearch::IResearchFeature::IRESEARCH)
    << "finished flush for iResearch view '" << id() << "' run id '" << size_t(&runId) << "'";

  return res;
}

/*static*/ std::string const& IResearchView::type() noexcept {
  static const std::string  type = "iresearch";

  return type;
}

arangodb::Result IResearchView::updateProperties(
  arangodb::velocypack::Slice const& slice,
  bool partialUpdate,
  bool doSync
) {
  if (!_logicalView || !_logicalView->getPhysical()) {
    return arangodb::Result(
      TRI_ERROR_INTERNAL,
      std::string("failed to find meta-store while updating iResearch view '") + std::to_string(id()) + "'"
    );
  }

  auto* vocbase = _logicalView->vocbase();

  if (!vocbase) {
    return arangodb::Result(
      TRI_ERROR_INTERNAL,
      std::string("failed to find vocbase while updating links for iResearch view '") + std::to_string(id()) + "'"
    );
  }

  auto& metaStore = *(_logicalView->getPhysical());
  std::string error;
  IResearchViewMeta meta;
  IResearchViewMeta::Mask mask;
  WriteMutex mutex(_mutex); // '_meta' can be asynchronously read
  arangodb::Result res = arangodb::Result(/*TRI_ERROR_NO_ERROR*/);

  {
    SCOPED_LOCK(mutex);

    arangodb::velocypack::Builder originalMetaJson; // required for reverting links on failure

    if (!_meta.json(arangodb::velocypack::ObjectBuilder(&originalMetaJson))) {
      return arangodb::Result(
        TRI_ERROR_INTERNAL,
        std::string("failed to generate json definition while updating iResearch view '") + std::to_string(id()) + "'"
      );
    }

    auto& initialMeta = partialUpdate ? _meta : IResearchViewMeta::DEFAULT();

    if (!meta.init(slice, error, *_logicalView, initialMeta, &mask)) {
      return arangodb::Result(TRI_ERROR_BAD_PARAMETER, std::move(error));
    }

    // reset non-updatable values to match current meta
    meta._collections = _meta._collections;

    // do not modify data path if not changes since will cause lock obtain failure
    mask._dataPath = mask._dataPath && _meta._dataPath != meta._dataPath;

    DataStore storePersisted; // renamed persisted data store
    std::string srcDataPath = _meta._dataPath;
    char const* dropDataPath = nullptr;

    // copy directory to new location
    if (mask._dataPath) {
      std::string absoluteDataPath;

      if (!appendAbsolutePersistedDataPath(absoluteDataPath, *this, meta)) {
        return arangodb::Result(
          TRI_ERROR_BAD_PARAMETER,
          std::string("error generating absolute path for iResearch view '") + std::to_string(id()) + "' data path '" + meta._dataPath + "'"
        );
      }

      auto res = createPersistedDataDirectory(
        storePersisted._directory,
        storePersisted._writer,
        absoluteDataPath,
        _storePersisted._reader, // reader from the original persisted data store
        id()
      );

      if (!res.ok()) {
        return res;
      }

      try {
        storePersisted._reader = irs::directory_reader::open(*(storePersisted._directory));
        dropDataPath = _storePersisted ? srcDataPath.c_str() : nullptr;
      } catch (std::exception const& e) {
        LOG_TOPIC(WARN, iresearch::IResearchFeature::IRESEARCH)
          << "caught exception while opening iResearch view '" << id()
          << "' data path '" + meta._dataPath + "': " << e.what();
        IR_EXCEPTION();
        return arangodb::Result(
          TRI_ERROR_BAD_PARAMETER,
          std::string("error opening iResearch view '") + std::to_string(id()) + "' data path '" + meta._dataPath + "'"
        );
      } catch (...) {
        LOG_TOPIC(WARN, iresearch::IResearchFeature::IRESEARCH)
          << "caught exception while opening iResearch view '" << id() << "' data path '" + meta._dataPath + "'";
        IR_EXCEPTION();
        return arangodb::Result(
          TRI_ERROR_BAD_PARAMETER,
          std::string("error opening iResearch view '") + std::to_string(id()) + "' data path '" + meta._dataPath + "'"
        );
      }

      if (!storePersisted._reader) {
        LOG_TOPIC(WARN, iresearch::IResearchFeature::IRESEARCH)
          << "error while opening reader for iResearch view '" << id() << "' data path '" + meta._dataPath + "'";
        return arangodb::Result(
          TRI_ERROR_BAD_PARAMETER,
          std::string("error opening reader for iResearch view '") + std::to_string(id()) + "' data path '" + meta._dataPath + "'"
        );
      }
    }

    IResearchViewMeta metaBackup;

    metaBackup = std::move(_meta);
    _meta = std::move(meta);

    res = persistProperties(metaStore, _valid); // persist '_meta' definition (so that on failure can revert meta)

    if (!res.ok()) {
      _meta = std::move(metaBackup); // revert to original meta
      LOG_TOPIC(WARN, iresearch::IResearchFeature::IRESEARCH)
        << "failed to persist view definition while updating iResearch view '" << id() << "'";

      return res;
    }

    if (mask._dataPath) {
      _storePersisted = std::move(storePersisted);
    }

    if (mask._threadsMaxIdle) {
      _threadPool.max_idle(meta._threadsMaxIdle);
    }

    if (mask._threadsMaxTotal) {
      _threadPool.max_threads(meta._threadsMaxTotal);
    }

    {
      SCOPED_LOCK(_asyncMutex);
      _asyncCondition.notify_all(); // trigger reload of timeout settings for async jobs
    }

    if (dropDataPath) {
      res = TRI_RemoveDirectory(dropDataPath); // ignore error (only done to tidy-up filesystem)
    }
  }

  // update links if requested (on a best-effort basis)
  // indexing of collections is done in different threads so no locks can be held and rollback is not possible
  // as a result it's also possible for links to be simultaneously modified via a different callflow (e.g. from collections)
  if (slice.hasKey(LINKS_FIELD)) {
    if (partialUpdate) {
      res = updateLinks(*vocbase, *this, slice.get(LINKS_FIELD));
    } else {
      arangodb::velocypack::Builder builder;

      builder.openObject();

      if (!appendLinkRemoval(builder, _meta)
          || !mergeSlice(builder, slice.get(LINKS_FIELD))) {
        return arangodb::Result(
          TRI_ERROR_INTERNAL,
          std::string("failed to construct link update directive while updating iResearch view '") + std::to_string(id()) + "'"
        );
      }

      builder.close();
      res = updateLinks(*vocbase, *this, builder.slice());
    }
  }

  return res;
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
  std::unordered_set<TRI_voc_cid_t> ids;
  bool success = appendKnownCollections(ids);
  if (success) {
    for (auto cid : ids) {
      auto collection = _logicalView->vocbase()->lookupCollection(cid);
      if (!collection) {
        // collection no longer exists, drop it and move on
        LOG_TOPIC(TRACE, arangodb::iresearch::IResearchFeature::IRESEARCH)
          << "collection " << cid << " no longer exists! removing from view "
          << _logicalView->id();
        drop(cid);
      } else {
        // see if the link still exists, otherwise drop and move on
        auto link = findFirstMatchingLink(*collection, *this);
        if (!link) {
          LOG_TOPIC(TRACE, arangodb::iresearch::IResearchFeature::IRESEARCH)
            << "collection " << cid << " no longer linked! removing from view "
            << _logicalView->id();
          drop(cid);
        }
      }
    }
  }
}

NS_END // iresearch
NS_END // arangodb

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------