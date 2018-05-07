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

#include "ApplicationServerHelper.h"
#include "IResearchAttributes.h"
#include "IResearchCommon.h"
#include "IResearchDocument.h"
#include "IResearchOrderFactory.h"
#include "IResearchFilterFactory.h"
#include "IResearchLink.h"
#include "IResearchLinkHelper.h"
#include "ExpressionFilter.h"
#include "AqlHelper.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Aql/AstNode.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/SortCondition.h"
#include "Basics/Result.h"
#include "Basics/files.h"
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
/// @brief the container storing the view state for a given TransactionState
////////////////////////////////////////////////////////////////////////////////
struct ViewState: public arangodb::TransactionState::Cookie {
  CompoundReader _snapshot;
  ViewState(ReadMutex& mutex): _snapshot(mutex) {}
};

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
  return arangodb::iresearch::getFeature<arangodb::FlushFeature>("Flush");
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

  auto* feature =
    arangodb::iresearch::getFeature<arangodb::DatabaseFeature>("Database");

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
/// @brief updates the collections in 'vocbase' to match the specified
///        IResearchLink definitions
////////////////////////////////////////////////////////////////////////////////
arangodb::Result updateLinks(
    std::unordered_set<TRI_voc_cid_t>& modified,
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
      std::shared_ptr<arangodb::LogicalCollection> _collection;
      size_t _collectionsToLockOffset; // std::numeric_limits<size_t>::max() == removal only
      std::shared_ptr<arangodb::iresearch::IResearchLink> _link;
      size_t _linkDefinitionsOffset;
      bool _valid = true;
      explicit State(size_t collectionsToLockOffset)
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
          || !arangodb::iresearch::IResearchLinkHelper::setType(namedJson)
          || !arangodb::iresearch::IResearchLinkHelper::setView(namedJson, view.id())) {
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

    auto* vocbase = trx.vocbase();

    if (!vocbase) {
      return arangodb::Result(
        TRI_ERROR_INTERNAL,
        std::string("failed to get vocbase from transaction while updating while IResearch view '") + std::to_string(view.id()) + "'"
      );
    }

    {
      std::unordered_set<TRI_voc_cid_t> collectionsToRemove; // track removal for potential reindex
      std::unordered_set<TRI_voc_cid_t> collectionsToUpdate; // track reindex requests

      // resolve corresponding collection and link
      for (auto itr = linkModifications.begin(); itr != linkModifications.end();) {
        auto& state = *itr;
        auto& collectionName = collectionsToLock[state._collectionsToLockOffset];

        state._collection = vocbase->lookupCollection(collectionName);

        if (!state._collection) {
          // remove modification state if removal of non-existant link on non-existant collection
          if (state._linkDefinitionsOffset >= linkDefinitions.size()) { // link removal request
            itr = linkModifications.erase(itr);
            continue;
          }

          return arangodb::Result(
            TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND,
            std::string("failed to get collection while updating iResearch view '") + std::to_string(view.id()) + "' collection '" + collectionName + "'"
          );
        }

        state._link = arangodb::iresearch::IResearchLink::find(*(state._collection), view);

        // remove modification state if removal of non-existant link
        if (!state._link // links currently does not exist
            && state._linkDefinitionsOffset >= linkDefinitions.size()) { // link removal request
          view.drop(state._collection->id()); // drop any stale data for the specified collection
          itr = linkModifications.erase(itr);
          continue;
        }

        if (state._link // links currently exists
            && state._linkDefinitionsOffset >= linkDefinitions.size()) { // link removal request
          auto cid = state._collection->id();

          // remove duplicate removal requests (e.g. by name + by CID)
          if (collectionsToRemove.find(cid) != collectionsToRemove.end()) { // removal previously requested
            itr = linkModifications.erase(itr);
            continue;
          }

          collectionsToRemove.emplace(cid);
        }

        if (state._link // links currently exists
            && state._linkDefinitionsOffset < linkDefinitions.size()) { // link update request
          collectionsToUpdate.emplace(state._collection->id());
        }

        ++itr;
      }

      // remove modification state if no change on existing link and reindex not requested
      for (auto itr = linkModifications.begin(); itr != linkModifications.end();) {
        auto& state = *itr;

        // remove modification if removal request with an update request also present
        if (state._link // links currently exists
            && state._linkDefinitionsOffset >= linkDefinitions.size() // link removal request
            && collectionsToUpdate.find(state._collection->id()) != collectionsToUpdate.end()) { // also has a reindex request
          itr = linkModifications.erase(itr);
          continue;
        }

        // remove modification state if no change on existing link or
        if (state._link // links currently exists
            && state._linkDefinitionsOffset < linkDefinitions.size() // link creation request
            && collectionsToRemove.find(state._collection->id()) == collectionsToRemove.end() // not a reindex request
            && *(state._link) == linkDefinitions[state._linkDefinitionsOffset].second) { // link meta not modified
          itr = linkModifications.erase(itr);
          continue;
        }

        ++itr;
      }
    }

    // execute removals
    for (auto& state: linkModifications) {
      if (state._link) { // link removal or recreate request
        modified.emplace(state._collection->id());
        state._valid = state._collection->dropIndex(state._link->id());
      }
    }

    // execute additions
    for (auto& state: linkModifications) {
      if (state._valid && state._linkDefinitionsOffset < linkDefinitions.size()) {
        bool isNew = false;
        auto linkPtr = state._collection->createIndex(&trx, linkDefinitions[state._linkDefinitionsOffset].first.slice(), isNew);

        modified.emplace(state._collection->id());
        state._valid = linkPtr && isNew;
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
    LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
      << "caught exception while updating links for iResearch view '" << view.id() << "': " << e.what();
    IR_LOG_EXCEPTION();
    return arangodb::Result(
      TRI_ERROR_BAD_PARAMETER,
      std::string("error updating links for iResearch view '") + std::to_string(view.id()) + "'"
    );
  } catch (...) {
    LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
      << "caught exception while updating links for iResearch view '" << view.id() << "'";
    IR_LOG_EXCEPTION();
    return arangodb::Result(
      TRI_ERROR_BAD_PARAMETER,
      std::string("error updating links for iResearch view '") + std::to_string(view.id()) + "'"
    );
  }
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

IResearchView::IResearchView(
    TRI_vocbase_t& vocbase,
    arangodb::velocypack::Slice const& info,
    arangodb::DatabasePathFeature const& dbPathFeature,
    uint64_t planVersion
): DBServerLogicalView(vocbase, info, planVersion),
    FlushTransaction(toString(*this)),
   _asyncMetaRevision(1),
   _asyncSelf(irs::memory::make_unique<AsyncSelf>(this)),
   _asyncTerminate(false),
   _memoryNode(&_memoryNodes[0]), // set current memory node (arbitrarily 0)
   _toFlush(&_memoryNodes[1]), // set flush-pending memory node (not same as _memoryNode)
   _storePersisted(getPersistedPath(dbPathFeature, id())),
   _threadPool(0, 0), // 0 == create pool with no threads, i.e. not running anything
   _inRecovery(false) {
  // set up in-recovery insertion hooks
  auto* feature = arangodb::iresearch::getFeature<arangodb::DatabaseFeature>("Database");

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

      viewPtr->_threadPool.max_threads(viewPtr->_meta._threadsMaxTotal); // start pool
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
  _trxReadCallback = [viewPtr](arangodb::TransactionState& state)->void {
    switch(state.status()) {
     case transaction::Status::RUNNING:
      viewPtr->snapshot(state, true);
      return;
     default:
      {} // NOOP
    }
  };

  // initialize transaction write callback
  _trxWriteCallback = [viewPtr](arangodb::TransactionState& state)->void {
    switch (state.status()) {
     case transaction::Status::ABORTED: {
      auto res = viewPtr->finish(state.id(), false);

      if (TRI_ERROR_NO_ERROR != res) {
        LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
          << "failed to finish abort while processing write-transaction callback for IResearch view '" << viewPtr->name() << "'";
      }

      return;
     }
     case transaction::Status::COMMITTED: {
      auto res = viewPtr->finish(state.id(), true);

      if (TRI_ERROR_NO_ERROR != res) {
        LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
          << "failed to finish commit while processing write-transaction callback for IResearch view '" << viewPtr->name() << "'";
      } else if (state.waitForSync() && !viewPtr->sync()) {
        LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
          << "failed to sync while processing write-transaction callback for IResearch view '" << viewPtr->name() << "'";
      }

      return;
     }
     default:
      {} // NOOP
    }
  };

  // add asynchronous commit job
  _threadPool.run([this]()->void {
    struct DataStoreState {
      size_t _cleanupIntervalCount;
      DataStore& _dataStore;
      DataStoreState(DataStore& store)
        : _cleanupIntervalCount(0), _dataStore(store) {}
    };

    size_t asyncMetaRevision = 0; // '0' differs from IResearchView constructor above
    size_t cleanupIntervalStep = std::numeric_limits<size_t>::max(); // will be initialized when states are updated below
    auto commitIntervalMsecRemainder = std::numeric_limits<size_t>::max(); // longest possible time for std::min(...)
    size_t commitTimeoutMsec = 0; // will be initialized when states are updated below
    IResearchViewMeta::CommitMeta::ConsolidationPolicies consolidationPolicies;
    DataStoreState states[] = {
      DataStoreState(_memoryNodes[0]._store),
      DataStoreState(_memoryNodes[1]._store),
      DataStoreState(_storePersisted)
    };
    ReadMutex mutex(_mutex); // '_meta' can be asynchronously modified

    for(;;) {
      bool commitTimeoutReached = false;

      // sleep until timeout
      {
        SCOPED_LOCK_NAMED(mutex, lock); // for '_meta._commit._commitIntervalMsec'
        SCOPED_LOCK_NAMED(_asyncMutex, asyncLock); // aquire before '_asyncTerminate' check

        if (_asyncTerminate.load()) {
          return; // termination requested
        }

        if (!_meta._commit._commitIntervalMsec) {
          lock.unlock(); // do not hold read lock while waiting on condition
          _asyncCondition.wait(asyncLock); // wait forever
        } else {
          auto msecRemainder =
            std::min(commitIntervalMsecRemainder, _meta._commit._commitIntervalMsec);
          auto startTime = std::chrono::system_clock::now();
          auto endTime = startTime + std::chrono::milliseconds(msecRemainder);

          lock.unlock(); // do not hold read lock while waiting on condition
          commitIntervalMsecRemainder = std::numeric_limits<size_t>::max(); // longest possible time assuming an uninterrupted sleep
          commitTimeoutReached = true;

          if (std::cv_status::timeout != _asyncCondition.wait_until(asyncLock, endTime)) {
            auto nowTime = std::chrono::system_clock::now();

            // if still need to sleep more then must relock '_meta' and sleep for min (remainder, interval)
            if (nowTime < endTime) {
              commitIntervalMsecRemainder = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - nowTime).count();
              commitTimeoutReached = false;
            }
          }
        }

        if (_asyncTerminate.load()) {
          return; // termination requested
        }
      }

      SCOPED_LOCK(mutex); // '_meta'/'_memoryStore'/'_storePersisted' can be asynchronously modified

      // reload states if required
      if (_asyncMetaRevision.load() != asyncMetaRevision) {
        asyncMetaRevision = _asyncMetaRevision.load();
        cleanupIntervalStep = _meta._commit._cleanupIntervalStep;
        commitTimeoutMsec = _meta._commit._commitTimeoutMsec;
        consolidationPolicies = _meta._commit._consolidationPolicies; // local copy
      }

      auto thresholdSec = TRI_microtime() + commitTimeoutMsec/1000.0;

      // perform sync
      for (size_t i = 0, count = IRESEARCH_COUNTOF(states);
           i < count && TRI_microtime() <= thresholdSec;
           ++i) {
        auto& state = states[i];
        auto runCleanupAfterCommit =
          state._cleanupIntervalCount > cleanupIntervalStep;

        if (state._dataStore._directory
            && state._dataStore._writer
            && syncStore(*(state._dataStore._directory),
                         state._dataStore._reader,
                         *(state._dataStore._writer),
                         state._dataStore._segmentCount,
                         consolidationPolicies,
                         commitTimeoutReached,
                         runCleanupAfterCommit,
                         name()
                        )) {
          commitIntervalMsecRemainder = std::numeric_limits<size_t>::max(); // longest possible time for std::min(...)

          if (runCleanupAfterCommit
              && ++state._cleanupIntervalCount >= cleanupIntervalStep) {
            state._cleanupIntervalCount = 0;
          }
        }
      }
    }
  });
}

IResearchView::~IResearchView() {
  {
    WriteMutex mutex(_asyncSelf->_mutex);
    SCOPED_LOCK(mutex); // wait for all the view users to finish
    _asyncSelf->_value.store(nullptr); // the view is being deallocated, its use is no longer valid
  }

  _asyncTerminate.store(true); // mark long-running async jobs for terminatation

  {
    SCOPED_LOCK(_asyncMutex);
    _asyncCondition.notify_all(); // trigger reload of settings for async jobs
  }

  _threadPool.max_threads_delta(int(std::max(size_t(std::numeric_limits<int>::max()), _threadPool.tasks_pending()))); // finish ASAP
  _threadPool.stop();

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
    engine->destroyView(vocbase(), this);
  }
}

IResearchView::MemoryStore& IResearchView::activeMemoryStore() const {
  return _memoryNode->_store;
}

void IResearchView::apply(arangodb::TransactionState& state) {
  state.addStatusChangeCallback(_trxReadCallback);
}

int IResearchView::drop(TRI_voc_cid_t cid) {
  std::shared_ptr<irs::filter> shared_filter(iresearch::FilterFactory::filter(cid));
  WriteMutex mutex(_mutex); // '_meta' and '_storeByTid' can be asynchronously updated
  SCOPED_LOCK(mutex);
  auto cid_itr = _meta._collections.find(cid);

  if (cid_itr != _meta._collections.end()) {
    auto result = persistProperties(*this, _asyncSelf);

    if (!result.ok()) {
      LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
        << "failed to persist logical view while dropping collection ' " << cid
        << "' from IResearch View '" << name() << "': " << result.errorMessage();

      return result.errorNumber();
    }

    _meta._collections.erase(cid_itr);
  }

  mutex.unlock(true); // downgrade to a read-lock

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
  arangodb::velocypack::Builder builder;

  // drop all known links
  {
    ReadMutex mutex(_mutex);
    SCOPED_LOCK(mutex); // '_meta' and '_trackedCids' can be asynchronously updated

    builder.openObject();

    if (!appendLinkRemoval(builder, _meta)) {
      return arangodb::Result(
        TRI_ERROR_INTERNAL,
        std::string("failed to construct link removal directive while removing IResearch view '") + std::to_string(id()) + "'"
      );
    }

    builder.close();
  }

  std::unordered_set<TRI_voc_cid_t> collections;

  if (!updateLinks(collections, vocbase(), *this, builder.slice()).ok()) {
    return arangodb::Result(
      TRI_ERROR_INTERNAL,
      std::string("failed to remove links while removing IResearch view '") + std::to_string(id()) + "'"
    );
  }

  {
    WriteMutex mutex(_asyncSelf->_mutex);
    SCOPED_LOCK(mutex); // wait for all the view users to finish
    _asyncSelf->_value.store(nullptr); // the view data-stores are being deallocated, view use is no longer valid
  }

  _asyncTerminate.store(true); // mark long-running async jobs for terminatation

  {
    SCOPED_LOCK(_asyncMutex);
    _asyncCondition.notify_all(); // trigger reload of settings for async jobs
  }

  _threadPool.stop();

  WriteMutex mutex(_mutex); // members can be asynchronously updated
  SCOPED_LOCK(mutex);

  collections.insert(_meta._collections.begin(), _meta._collections.end());
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
    _storeByTid.clear();

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

  if (!_meta._collections.emplace(cid).second) {
    return false;
  }

  try {
    result = persistProperties(*this, _asyncSelf);

    if (result.ok()) {
      return true;
    }
  } catch (std::exception const& e) {
    _meta._collections.erase(cid); // undo meta modification
    LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
      << "caught exception during persisting of logical view while emplacing collection ' " << cid
      << "' into IResearch View '" << name() << "': " << e.what();
    IR_LOG_EXCEPTION();
    throw;
  } catch (...) {
    _meta._collections.erase(cid); // undo meta modification
    LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
      << "caught exception during persisting of logical view while emplacing collection ' " << cid
      << "' into IResearch View '" << name() << "'";
    IR_LOG_EXCEPTION();
    throw;
  }

  _meta._collections.erase(cid); // undo meta modification
  LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
    << "failed to persist logical view while emplacing collection ' " << cid
    << "' into IResearch View '" << name() << "': " << result.errorMessage();

  return false;
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
    ++memoryStore._segmentCount; // a new segment was imported
    _asyncCondition.notify_all(); // trigger recheck of sync

    return TRI_ERROR_NO_ERROR;
  } catch (std::exception const& e) {
    LOG_TOPIC(ERR, arangodb::iresearch::TOPIC)
      << "caught exception while committing transaction for iResearch view '" << id()
      << "', tid '" << tid << "': " << e.what();
    IR_LOG_EXCEPTION();
  } catch (...) {
    LOG_TOPIC(ERR, arangodb::iresearch::TOPIC)
      << "caught exception while committing transaction for iResearch view '" << id()
      << "', tid '" << tid << "'";
    IR_LOG_EXCEPTION();
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
    _storePersisted._segmentCount.store(0); // reset to zero to get count of new segments that appear during commit
    _storePersisted._writer->commit(); // finishing flush transaction
    memoryStore._segmentCount.store(0); // reset to zero to get count of new segments that appear during commit
    memoryStore._writer->clear(); // prepare the store for reuse

    SCOPED_LOCK(_toFlush->_readMutex); // do not allow concurrent read since _storePersisted/_toFlush need to be updated atomically
    _storePersisted._reader = _storePersisted._reader.reopen(); // update reader
    _storePersisted._segmentCount += _storePersisted._reader.size(); // add commited segments
    memoryStore._reader = memoryStore._reader.reopen(); // update reader
    memoryStore._segmentCount += memoryStore._reader.size(); // add commited segments

    _asyncCondition.notify_all(); // trigger recheck of sync

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
  SCOPED_LOCK(mutex); // '_meta'/'_links' can be asynchronously updated

  _meta.json(builder);

  if (forPersistence) {
    return; // nothing more to output (persistent configuration does not need links)
  }

  TRI_ASSERT(builder.isOpenObject());
  std::vector<std::string> collections;

  // add CIDs of known collections to list
  for (auto& entry: _meta._collections) {
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
    arangodb::transaction::UserTransaction trx(
      transaction::StandaloneContext::Create(&vocbase()),
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

    auto storeItr = irs::map_utils::try_emplace(_storeByTid, trx.state()->id());

    if (storeItr.second) {
      trx.state()->addStatusChangeCallback(_trxWriteCallback);
    }

    store = &(storeItr.first->second._store);
  }

  TRI_ASSERT(store);

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

    auto storeItr = irs::map_utils::try_emplace(_storeByTid, trx.state()->id());

    if (storeItr.second) {
      trx.state()->addStatusChangeCallback(_trxWriteCallback);
    }

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
  auto* feature =
    arangodb::iresearch::getFeature<arangodb::DatabasePathFeature>("DatabasePath");

  if (!feature) {
    LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
      << "failure to find feature 'DatabasePath' while constructing IResearch View in database '" << vocbase.id() << "'";

    return nullptr;
  }

  PTR_NAMED(IResearchView, view, vocbase, info, *feature, planVersion);
  auto& impl = reinterpret_cast<IResearchView&>(*view);
  auto& json = info.isObject() ? info : emptyObjectSlice(); // if no 'info' then assume defaults
  auto props = json.get("properties");
  auto& properties = props.isObject() ? props : emptyObjectSlice(); // if no 'info' then assume defaults
  std::string error;

  if (!impl._meta.init(properties, error)) {
    LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
      << "failed to initialize iResearch view from definition, error: " << error;

    return nullptr;
  }

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

    auto storeItr = irs::map_utils::try_emplace(_storeByTid, trx.state()->id());

    if (storeItr.second) {
      trx.state()->addStatusChangeCallback(_trxWriteCallback);
    }

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
    LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
      << "caught exception while removing from iResearch view '" << id()
      << "', collection '" << cid << "', revision '" << documentId.id() << "': " << e.what();
    IR_LOG_EXCEPTION();
  } catch (...) {
    LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
      << "caught exception while removing from iResearch view '" << id()
      << "', collection '" << cid << "', revision '" << documentId.id() << "'";
    IR_LOG_EXCEPTION();
  }

  return TRI_ERROR_INTERNAL;
}

PrimaryKeyIndexReader* IResearchView::snapshot(
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

  if (state.waitForSync() && !const_cast<IResearchView*>(this)->sync()) {
    LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
      << "failed to sync while creating snapshot for IResearch view '" << name() << "', previous snapshot will be used instead";
  }

  auto cookiePtr = irs::memory::make_unique<ViewState>(_asyncSelf->mutex()); // will aquire read-lock to prevent data-store deallocation
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
      << "caught exception while collecting readers for snapshot of IResearch view '" << id()
      << "': " << e.what();
    IR_LOG_EXCEPTION();

    return nullptr;
  } catch (...) {
    LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
      << "caught exception while collecting readers for snapshot of IResearch view '" << id() << "'";
    IR_LOG_EXCEPTION();

    return nullptr;
  }

  state.cookie(this, std::move(cookiePtr));

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
  IResearchViewMeta::Mask mask;
  WriteMutex mutex(_mutex); // '_meta' can be asynchronously read
  arangodb::Result res;

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

    if (!meta.init(slice, error, initialMeta, &mask)) {
      return arangodb::Result(TRI_ERROR_BAD_PARAMETER, std::move(error));
    }

    // reset non-updatable values to match current meta
    meta._collections = _meta._collections;

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

    _meta = std::move(meta);
  }

  if (!slice.hasKey(StaticStrings::LinksField)) {
    return res;
  }

  // ...........................................................................
  // update links if requested (on a best-effort basis)
  // indexing of collections is done in different threads so no locks can be held and rollback is not possible
  // as a result it's also possible for links to be simultaneously modified via a different callflow (e.g. from collections)
  // ...........................................................................

  std::unordered_set<TRI_voc_cid_t> collections;

  if (partialUpdate) {
    return updateLinks(collections, vocbase(), *this, slice.get(StaticStrings::LinksField));
  }

  arangodb::velocypack::Builder builder;

  builder.openObject();

  // FIXME do not blindly drop links in case if
  // current and expected metas are same
  if (!appendLinkRemoval(builder, _meta)
      || !mergeSlice(builder, slice.get(StaticStrings::LinksField))) {
    return arangodb::Result(
      TRI_ERROR_INTERNAL,
      std::string("failed to construct link update directive while updating IResearch View '") + name() + "'"
    );
  }

  builder.close();

  return updateLinks(collections, vocbase(), *this, builder.slice());
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

  for (auto& cid: _meta._collections) {
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
  auto cids = _meta._collections;

  {
    static const arangodb::transaction::Options defaults;
    struct State final: public arangodb::TransactionState {
      State(TRI_vocbase_t& vocbase)
        : arangodb::TransactionState(&vocbase, defaults) {}
      virtual arangodb::Result abortTransaction(
          arangodb::transaction::Methods*
      ) override { return TRI_ERROR_NOT_IMPLEMENTED; }
      virtual arangodb::Result beginTransaction(
          arangodb::transaction::Hints
      ) override { return TRI_ERROR_NOT_IMPLEMENTED; }
      virtual arangodb::Result commitTransaction(
          arangodb::transaction::Methods*
      ) override { return TRI_ERROR_NOT_IMPLEMENTED; }
      virtual bool hasFailedOperations() const override { return false; }
    };

    State state(vocbase());

    if (!appendKnownCollections(cids, *snapshot(state, true))) {
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

NS_END // iresearch
NS_END // arangodb

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------
