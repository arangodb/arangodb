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

////////////////////////////////////////////////////////////////////////////////
/// @brief the string representing the view type
////////////////////////////////////////////////////////////////////////////////
static const std::string& VIEW_TYPE = arangodb::iresearch::IResearchFeature::type();

typedef irs::async_utils::read_write_mutex::read_mutex ReadMutex;
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
    auto directory = irs::directory::make<irs::mmap_directory>(dstDataPath);

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

    LOG_TOPIC(DEBUG, arangodb::iresearch::IResearchFeature::IRESEARCH)
      << "registering consolidation policy '" << size_t(entry.type()) << "' with IResearch view '" << viewName << "' run id '" << size_t(&runId) << " segment threshold '" << entry.segmentThreshold() << "' segment count '" << segmentCount.load() << "'";

    try {
      writer.consolidate(entry.policy(), false);
      forceCommit = true; // a consolidation policy was found requiring commit
    } catch (std::exception const& e) {
      LOG_TOPIC(WARN, arangodb::iresearch::IResearchFeature::IRESEARCH)
        << "caught exception during registeration of consolidation policy '" << size_t(entry.type()) << "' with IResearch view '" << viewName << "': " << e.what();
      IR_EXCEPTION();
    } catch (...) {
      LOG_TOPIC(WARN, arangodb::iresearch::IResearchFeature::IRESEARCH)
        << "caught exception during registeration of consolidation policy '" << size_t(entry.type()) << "' with IResearch view '" << viewName << "'";
      IR_EXCEPTION();
    }

    LOG_TOPIC(DEBUG, arangodb::iresearch::IResearchFeature::IRESEARCH)
      << "finished registering consolidation policy '" << size_t(entry.type()) << "' with IResearch view '" << viewName << "' run id '" << size_t(&runId) << "'";
  }

  if (!forceCommit) {
    LOG_TOPIC(DEBUG, arangodb::iresearch::IResearchFeature::IRESEARCH)
      << "skipping store sync since no consolidation policies matched and sync not forced for IResearch view '" << viewName << "' run id '" << size_t(&runId) << "'";

    return false; // commit not done
  }

  // ...........................................................................
  // apply data store commit
  // ...........................................................................

  LOG_TOPIC(DEBUG, arangodb::iresearch::IResearchFeature::IRESEARCH)
    << "starting store sync for IResearch view '" << viewName << "' run id '" << size_t(&runId) << "' segment count before '" << segmentCount.load() << "'";

  try {
    segmentCount.store(0); // reset to zero to get count of new segments that appear during commit
    writer.commit();
    reader = reader.reopen(); // update reader
    segmentCount += reader.size(); // add commited segments
  } catch (std::exception const& e) {
    LOG_TOPIC(WARN, arangodb::iresearch::IResearchFeature::IRESEARCH)
      << "caught exception during sync of IResearch view '" << viewName << "': " << e.what();
    IR_EXCEPTION();
  } catch (...) {
    LOG_TOPIC(WARN, arangodb::iresearch::IResearchFeature::IRESEARCH)
      << "caught exception during sync of IResearch view '" << viewName << "'";
    IR_EXCEPTION();
  }

  LOG_TOPIC(DEBUG, arangodb::iresearch::IResearchFeature::IRESEARCH)
    << "finished store sync for IResearch view '" << viewName << "' run id '" << size_t(&runId) << "' segment count after '" << segmentCount.load() << "'";

  if (!runCleanupAfterCommit) {
    return true; // commit done
  }

  // ...........................................................................
  // apply cleanup
  // ...........................................................................

  LOG_TOPIC(DEBUG, arangodb::iresearch::IResearchFeature::IRESEARCH)
    << "starting cleanup for IResearch view '" << viewName << "' run id '" << size_t(&runId) << "'";

  try {
    irs::directory_utils::remove_all_unreferenced(directory);
  } catch (std::exception const& e) {
    LOG_TOPIC(WARN, arangodb::iresearch::IResearchFeature::IRESEARCH)
      << "caught exception during cleanup of IResearch view '" << viewName << "': " << e.what();
    IR_EXCEPTION();
  } catch (...) {
    LOG_TOPIC(WARN, arangodb::iresearch::IResearchFeature::IRESEARCH)
      << "caught exception during cleanup of IResearch view '" << viewName << "'";
    IR_EXCEPTION();
  }

  LOG_TOPIC(DEBUG, arangodb::iresearch::IResearchFeature::IRESEARCH)
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
      arangodb::LogicalCollection* _collection = nullptr;
      size_t _collectionsToLockOffset; // std::numeric_limits<size_t>::max() == removal only
      arangodb::iresearch::IResearchLink const* _link = nullptr;
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
      std::unordered_set<TRI_voc_cid_t> collectionsToUpdate; // track reindex requests

      // resolve corresponding collection and link
      for (auto itr = linkModifications.begin(); itr != linkModifications.end();) {
        auto& state = *itr;
        auto& collectionName = collectionsToLock[state._collectionsToLockOffset];

        state._collection = const_cast<arangodb::LogicalCollection*>(resolver->getCollectionStruct(collectionName));

        if (!state._collection) {
          // remove modification state if removal of non-existant link on non-existant collection
          if (state._linkDefinitionsOffset >= linkDefinitions.size()) { // link removal request
            itr = linkModifications.erase(itr);
            continue;
          }

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

        if (state._link // links currently exists
            && state._linkDefinitionsOffset >= linkDefinitions.size()) { // link removal request
          auto cid = state._collection->cid();

          // remove duplicate removal requests (e.g. by name + by CID)
          if (collectionsToRemove.find(cid) != collectionsToRemove.end()) { // removal previously requested
            itr = linkModifications.erase(itr);
            continue;
          }

          collectionsToRemove.emplace(cid);
        }

        if (state._link // links currently exists
            && state._linkDefinitionsOffset < linkDefinitions.size()) { // link update request
          collectionsToUpdate.emplace(state._collection->cid());
        }

        ++itr;
      }

      // remove modification state if no change on existing link and reindex not requested
      for (auto itr = linkModifications.begin(); itr != linkModifications.end();) {
        auto& state = *itr;

        // remove modification if removal request with an update request also present
        if (state._link // links currently exists
            && state._linkDefinitionsOffset >= linkDefinitions.size() // link removal request
            && collectionsToUpdate.find(state._collection->cid()) != collectionsToUpdate.end()) { // also has a reindex request
          itr = linkModifications.erase(itr);
          continue;
        }

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
      if (state._link) { // link removal or recreate request
        modified.emplace(state._collection->cid());
        state._valid = state._collection->dropIndex(state._link->id());
      }
    }

    // execute additions
    for (auto& state: linkModifications) {
      if (state._valid && state._linkDefinitionsOffset < linkDefinitions.size()) {
        bool isNew = false;
        auto linkPtr = state._collection->createIndex(&trx, linkDefinitions[state._linkDefinitionsOffset].first.slice(), isNew);

        modified.emplace(state._collection->cid());
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
    auto* collection = vocbase.lookupCollection(*itr);

    if (!collection || !findFirstMatchingLink(*collection, view)) {
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
   _asyncSelf(irs::memory::make_unique<AsyncSelf>(this)),
   _asyncTerminate(false),
   _memoryNode(&_memoryNodes[0]), // set current memory node (arbitrarily 0)
   _toFlush(&_memoryNodes[1]), // set flush-pending memory node (not same as _memoryNode)
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
        LOG_TOPIC(WARN, arangodb::iresearch::IResearchFeature::IRESEARCH)
          << "Invalid call to post-recovery callback of iResearch view";

        return arangodb::Result(); // view no longer in recovery state
      }

      viewPtr->verifyKnownCollections();

      if (viewPtr->_storePersisted) {
        LOG_TOPIC(DEBUG, iresearch::IResearchFeature::IRESEARCH)
          << "starting persisted-sync sync for iResearch view '" << viewPtr->id() << "'";

        try {
          viewPtr->_storePersisted.sync();
        } catch (std::exception const& e) {
          LOG_TOPIC(ERR, iresearch::IResearchFeature::IRESEARCH)
            << "caught exception while committing persisted store for iResearch view '" << viewPtr->id()
            << "': " << e.what();

          return arangodb::Result(TRI_ERROR_INTERNAL, e.what());
        } catch (...) {
          LOG_TOPIC(ERR, iresearch::IResearchFeature::IRESEARCH)
            << "caught exception while committing persisted store for iResearch view '" << viewPtr->id() << "'";

          return arangodb::Result(TRI_ERROR_INTERNAL);
        }

        LOG_TOPIC(DEBUG, iresearch::IResearchFeature::IRESEARCH)
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
          << "failed to sync while processing transaction callback for IResearch view '" << id() << "'";
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
    size_t cleanupIntervalStep; // will be initialized when states are updated below
    auto commitIntervalMsecRemainder = std::numeric_limits<size_t>::max(); // longest possible time for std::min(...)
    size_t commitTimeoutMsec; // will be initialized when states are updated below
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
    _asyncSelf->_value.store(nullptr); // the view is being deallocated, its use is not longer valid
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
      _storePersisted._writer->commit();
      _storePersisted._writer->close();
      _storePersisted._writer.reset();
      _storePersisted._directory->close();
      _storePersisted._directory.reset();
    }
  }
}

IResearchView::MemoryStore& IResearchView::activeMemoryStore() const {
  return _memoryNode->_store;
}

void IResearchView::drop() {
  std::unordered_set<TRI_voc_cid_t> collections;

  // drop all known links
  if (_logicalView && _logicalView->vocbase()) {
    arangodb::velocypack::Builder builder;

    {
      ReadMutex mutex(_mutex);
      SCOPED_LOCK(mutex); // '_meta' and '_trackedCids' can be asynchronously updated

      builder.openObject();

      if (!appendLinkRemoval(builder, _meta)) {
        throw std::runtime_error(std::string("failed to construct link removal directive while removing iResearch view '") + std::to_string(id()) + "'");
      }

      builder.close();
    }

    if (!updateLinks(collections, *(_logicalView->vocbase()), *this, builder.slice()).ok()) {
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

  collections.insert(_meta._collections.begin(), _meta._collections.end());

  if (_logicalView->vocbase()) {
    validateLinks(collections, *(_logicalView->vocbase()), *this);
  }

  // ArangoDB global consistency check, no known dangling links
  if (!collections.empty()) {
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
  std::shared_ptr<irs::filter> shared_filter(iresearch::FilterFactory::filter(cid));
  ReadMutex mutex(_mutex); // '_storeByTid' can be asynchronously updated
  SCOPED_LOCK(mutex);

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
    ++memoryStore._segmentCount; // a new segment was imported
    _asyncCondition.notify_all(); // trigger recheck of sync

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

  if (!_logicalView || !_logicalView->vocbase() || forPersistence) {
    return; // nothing more to output (persistent configuration does not need links)
  }

  std::vector<std::string> collections;

  // add CIDs of known collections to list
  for (auto& entry: _meta._collections) {
    // skip collections missing from vocbase or UserTransaction constructor will throw an exception
    if (nullptr != _logicalView->vocbase()->lookupCollection(entry)) {
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

  auto insert = [&body, cid, documentId](irs::segment_writer::document& doc)->bool {
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

arangodb::Result IResearchView::link(
    TRI_voc_cid_t cid,
    arangodb::velocypack::Slice const link
) {
  if (!_logicalView) {
    return arangodb::Result(
      TRI_ERROR_INTERNAL,
      std::string("failed to find logical view while linking IResearch view '") + std::to_string(id()) + "'"
    );
  }

  auto* vocbase = _logicalView->vocbase();

  if (!vocbase) {
    return arangodb::Result(
      TRI_ERROR_INTERNAL,
      std::string("failed to find vocbase while linking IResearch view '") + std::to_string(id()) + "'"
    );
  }

  arangodb::velocypack::Builder builder;

  builder.openObject();
  builder.add(
    std::to_string(cid),
    arangodb::velocypack::Value(arangodb::velocypack::ValueType::Null)
  );

  if (link.isObject()) {
    builder.add(std::to_string(cid), link);
  }

  builder.close();

  std::unordered_set<TRI_voc_cid_t> collections;
  auto result = updateLinks(collections, *vocbase, *this, builder.slice());

  if (result.ok()) {
    WriteMutex mutex(_mutex); // '_meta' can be asynchronously read
    SCOPED_LOCK(mutex);

    collections.insert(_meta._collections.begin(), _meta._collections.end());
    validateLinks(collections, *vocbase, *this); // remove invalid cids (no such collection or no such link)
    _meta._collections = std::move(collections);
  }

  return result;
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
        irs::directory::make<irs::mmap_directory>(absoluteDataPath);

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

iresearch::CompoundReader IResearchView::snapshot() {
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

  return compoundReader;
}

IResearchView::AsyncSelf::ptr IResearchView::self() const {
  return _asyncSelf;
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
    _toFlush->_store._segmentCount.store(0); // reset to zero to get count of new segments that appear during commit
    _toFlush->_store._writer->commit();

    {
      SCOPED_LOCK(_toFlush->_reopenMutex);
      _toFlush->_store._reader = _toFlush->_store._reader.reopen(); // update reader
      _toFlush->_store._segmentCount += _toFlush->_store._reader.size(); // add commited segments
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
      _storePersisted._segmentCount.store(0); // reset to zero to get count of new segments that appear during commit
      _storePersisted._writer->commit();

      {
        SCOPED_LOCK(_toFlush->_reopenMutex);
        _storePersisted._reader = _storePersisted._reader.reopen(); // update reader
        _storePersisted._segmentCount += _storePersisted._reader.size(); // add commited segments
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

/*static*/ std::string const& IResearchView::type() noexcept {
  return VIEW_TYPE;
}

arangodb::Result IResearchView::updateLogicalProperties(
  arangodb::velocypack::Slice const& slice,
  bool partialUpdate,
  bool doSync
) {
  return _logicalView
    ? _logicalView->updateProperties(slice, partialUpdate, doSync)
    : arangodb::Result(TRI_ERROR_ARANGO_VIEW_NOT_FOUND)
    ;
}

arangodb::Result IResearchView::updateProperties(
  arangodb::velocypack::Slice const& slice,
  bool partialUpdate,
  bool doSync
) {
  if (!_logicalView) {
    return arangodb::Result(
      TRI_ERROR_INTERNAL,
      std::string("failed to find logical view while updating IResearch view '") + std::to_string(id()) + "'"
    );
  }

  auto* vocbase = _logicalView->vocbase();

  if (!vocbase) {
    return arangodb::Result(
      TRI_ERROR_INTERNAL,
      std::string("failed to find vocbase while updating links for iResearch view '") + std::to_string(id()) + "'"
    );
  }

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

    // FIXME TODO remove once View::updateProperties(...) will be fixed to write
    // the update delta into the WAL marker instead of the full persisted state
    // below is a very dangerous hack as it allows multiple links from the same
    // collection to point to the same view, thus breaking view data consistency
    {
      auto* engine = arangodb::EngineSelectorFeature::ENGINE;

      if (engine && engine->inRecovery()) {
        arangodb::velocypack::Builder linksBuilder;

        linksBuilder.openObject();

        // remove links no longer present in incming update
        for (auto& cid: _meta._collections) {
          if (meta._collections.find(cid) == meta._collections.end()) {
            linksBuilder.add(
              std::to_string(cid),
              arangodb::velocypack::Value(arangodb::velocypack::ValueType::Null)
            );
          }
        }

        for (auto& cid: meta._collections) {
          auto* collection = vocbase->lookupCollection(cid);

          if (collection) {
            _meta._collections.emplace(cid);

            for (auto& index: collection->getIndexes()) {
              if (index && arangodb::Index::TRI_IDX_TYPE_IRESEARCH_LINK == index->type()) {
                auto* link = dynamic_cast<arangodb::iresearch::IResearchLink*>(index.get());

                if (link && link->_defaultId == id() && !link->view()) {
                  arangodb::velocypack::Builder linkBuilder;
                  bool valid;

                  linkBuilder.openObject();
                  valid = link->json(linkBuilder, true);
                  linkBuilder.close();

                  linksBuilder.add(
                    std::to_string(cid),
                    arangodb::velocypack::Value(arangodb::velocypack::ValueType::Null)
                  );

                  if (valid) {
                    linksBuilder.add(std::to_string(cid), linkBuilder.slice());
                  }
                }
              }
            }
          }
        }

        std::unordered_set<TRI_voc_cid_t> collections;

        linksBuilder.close();
        updateLinks(collections, *vocbase, *this, linksBuilder.slice());
        collections.insert(_meta._collections.begin(), _meta._collections.end());
        validateLinks(collections, *vocbase, *this); // remove invalid cids (no such collection or no such link)
        _meta._collections = std::move(collections);
      }
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
        storePersisted._segmentCount += storePersisted._reader.size(); // add commited segments (previously had 0)
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

    _meta = std::move(meta);

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

  std::unordered_set<TRI_voc_cid_t> collections;

  // update links if requested (on a best-effort basis)
  // indexing of collections is done in different threads so no locks can be held and rollback is not possible
  // as a result it's also possible for links to be simultaneously modified via a different callflow (e.g. from collections)
  if (slice.hasKey(LINKS_FIELD)) {
    if (partialUpdate) {
      res = updateLinks(collections, *vocbase, *this, slice.get(LINKS_FIELD));
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
      res = updateLinks(collections, *vocbase, *this, builder.slice());
    }
  }

  // ...........................................................................
  // if an exception occurs below then it would only affect collection linking
  // consistency and an update retry would most likely happen
  // always re-validate '_collections' because may have had externally triggered
  // collection/link drops
  // ...........................................................................
  {
    SCOPED_LOCK(mutex); // '_meta' can be asynchronously read
    collections.insert(_meta._collections.begin(), _meta._collections.end());
    validateLinks(collections, *vocbase, *this); // remove invalid cids (no such collection or no such link)
    _meta._collections = std::move(collections);
  }

  // FIXME TODO to ensure valid recovery remove the original datapath only if the entire, but under lock to prevent double rename

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

bool IResearchView::visitCollections(
    std::function<bool(TRI_voc_cid_t)> const& visitor
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
  std::unordered_set<TRI_voc_cid_t> cids;

  if (!appendKnownCollections(cids, snapshot())) {
    LOG_TOPIC(ERR, iresearch::IResearchFeature::IRESEARCH)
      << "failed to collect collection IDs for IResearch view '" << id() << "'";

    return;
  }

  for (auto cid : cids) {
    auto collection = _logicalView->vocbase()->lookupCollection(cid);

    if (!collection) {
      // collection no longer exists, drop it and move on
      LOG_TOPIC(TRACE, arangodb::iresearch::IResearchFeature::IRESEARCH)
        << "collection '" << cid
        << "' no longer exists! removing from IResearch view '"
        << _logicalView->id() << "'";
      drop(cid);
    } else {
      // see if the link still exists, otherwise drop and move on
      auto link = findFirstMatchingLink(*collection, *this);
      if (!link) {
        LOG_TOPIC(TRACE, arangodb::iresearch::IResearchFeature::IRESEARCH)
          << "collection '" << cid
          << "' no longer linked! removing from IResearch view '"
          << _logicalView->id() << "'";
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