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

#include "Basics/Common.h"

#include "IResearchLinkHelper.h"

#include <velocypack/Iterator.h>

#include "IResearchCommon.h"
#include "IResearchFeature.h"
#include "IResearchLink.h"
#include "IResearchLinkMeta.h"
#include "IResearchView.h"
#include "IResearchViewCoordinator.h"
#include "VelocyPackHelper.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/StaticStrings.h"
#include "Basics/StringUtils.h"
#include "Cluster/ClusterMethods.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "RestServer/DatabaseFeature.h"
#include "RestServer/SystemDatabaseFeature.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "StorageEngine/StorageEngine.h"
#include "Transaction/Methods.h"
#include "Transaction/StandaloneContext.h"
#include "Utils/CollectionNameResolver.h"
#include "Utils/ExecContext.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/Methods/Indexes.h"

namespace {

////////////////////////////////////////////////////////////////////////////////
/// @brief the string representing the link type
////////////////////////////////////////////////////////////////////////////////
std::string const& LINK_TYPE = arangodb::iresearch::DATA_SOURCE_TYPE.name();

arangodb::Result canUseAnalyzers( // validate
  arangodb::iresearch::IResearchLinkMeta const& meta, // metadata
  TRI_vocbase_t const& defaultVocbase // default vocbase
) {
  for (auto& pool: meta._analyzerDefinitions) {
    if (!pool) {
      continue; // skip invalid entries
    }

    auto result = arangodb::iresearch::IResearchAnalyzerFeature::canUse(
        arangodb::iresearch::IResearchAnalyzerFeature::normalize(pool->name(), defaultVocbase.name()),
        arangodb::auth::Level::RO);

    if (!result) {
      return {
        TRI_ERROR_FORBIDDEN,
        std::string("read access is forbidden to arangosearch analyzer '") + pool->name() + "'"
      };
    }
  }

//  for (auto& field: meta._fields) {
//    TRI_ASSERT(field.value().get()); // ensured by UniqueHeapInstance constructor
//    auto& entry = field.value();
//    auto res = canUseAnalyzers(*entry, defaultVocbase);
//
//    if (!res.ok()) {
//      return res;
//    }
//  }

  return {};
}

arangodb::Result createLink( // create link
    arangodb::LogicalCollection& collection, // link collection
    arangodb::LogicalView const& view, // link view
    arangodb::velocypack::Slice definition // link definition
) {
  try {
    bool isNew = false;
    auto link = collection.createIndex(definition, isNew);

    if (!(link && isNew)) {
      return arangodb::Result( // result
        TRI_ERROR_INTERNAL, // code
        std::string("failed to create link between arangosearch view '") + view.name() + "' and collection '" + collection.name() + "'"
      );
    }

    // ensure link is synchronized after upgrade in single-server
    if (arangodb::ServerState::instance()->isSingleServer()) {
      auto* db = arangodb::DatabaseFeature::DATABASE;

      if (db && (db->checkVersion() || db->upgrade())) {
        // FIXME find a better way to retrieve an IResearch Link
        // cannot use static_cast/reinterpret_cast since Index is not related to
        // IResearchLink
        auto impl = std::dynamic_pointer_cast<arangodb::iresearch::IResearchLink>(link);

        if (impl) {
          return impl->commit();
        }
      }
    }
  } catch (arangodb::basics::Exception const& e) {
    return arangodb::Result(e.code(), e.what());
  }

  return arangodb::Result();
}

arangodb::Result createLink( // create link
    arangodb::LogicalCollection& collection, // link collection
    arangodb::iresearch::IResearchViewCoordinator const& view, // link view
    arangodb::velocypack::Slice definition // link definition
) {
  if (arangodb::ClusterMethods::filterHiddenCollections(collection)) {
    // Enterprise variant, we only need to create links on non-hidden
    // collections (e.g. in SmartGraph Case)
    // The hidden collections are managed by the logic around the SmartEdgeCollection
    // and do not allow to have their own modifications.
    return TRI_ERROR_NO_ERROR;
  }
  static const std::function<bool(irs::string_ref const& key)> acceptor = [](
      irs::string_ref const& key // json key
  )->bool {

    // ignored fields
    return key != arangodb::StaticStrings::IndexType // type field
        && key != arangodb::iresearch::StaticStrings::ViewIdField; // view id field
  };
  arangodb::velocypack::Builder builder;

  builder.openObject();
  builder.add( // add
    arangodb::StaticStrings::IndexType, // key
    arangodb::velocypack::Value(LINK_TYPE) // value
  );
  builder.add( // add
    arangodb::iresearch::StaticStrings::ViewIdField, // key
    arangodb::velocypack::Value(view.guid()) // value
  );

  if (!arangodb::iresearch::mergeSliceSkipKeys(builder, definition, acceptor)) {
    return arangodb::Result( // result
      TRI_ERROR_INTERNAL, // code
      std::string("failed to generate definition while creating link between arangosearch view '") + view.name() + "' and collection '" + collection.name() + "'"
    );
  }

  builder.close();

  arangodb::velocypack::Builder tmp;

  return arangodb::methods::Indexes::ensureIndex( // ensure index
    &collection, builder.slice(), true, tmp // args
  );
}

template<typename ViewType>
arangodb::Result dropLink( // drop link
    arangodb::LogicalCollection& collection, // link collection
    arangodb::iresearch::IResearchLink const& link // link to drop
) {

  // don't need to create an extra transaction inside arangodb::methods::Indexes::drop(...)
  if (!collection.dropIndex(link.id())) {
    return arangodb::Result(  // result
        TRI_ERROR_INTERNAL,   // code
        std::string("failed to drop link '") + std::to_string(link.id().id()) +
            "' from collection '" + collection.name() + "'");
  }

  return arangodb::Result();
}

template <>
arangodb::Result dropLink<arangodb::iresearch::IResearchViewCoordinator>(  // drop link
    arangodb::LogicalCollection& collection,        // link collection
    arangodb::iresearch::IResearchLink const& link  // link to drop
) {

  if (arangodb::ClusterMethods::filterHiddenCollections(collection)) {
    // Enterprise variant, we only need to drop links on non-hidden
    // collections (e.g. in SmartGraph Case)
    // The hidden collections are managed by the logic around the SmartEdgeCollection
    // and do not allow to have their own modifications.
    return arangodb::Result();
  }

  arangodb::velocypack::Builder builder;
  builder.openObject();
  builder.add(                                     // add
      arangodb::StaticStrings::IndexId,            // key
      arangodb::velocypack::Value(link.id().id())  // value
  );
  builder.close();

  return arangodb::methods::Indexes::drop(&collection, builder.slice());
}

template <typename ViewType>
arangodb::Result modifyLinks( // modify links
    std::unordered_set<TRI_voc_cid_t>& modified, // modified collection ids
    ViewType& view, // modified view
    arangodb::velocypack::Slice const& links, // modified link definitions
    std::unordered_set<TRI_voc_cid_t> const& stale = {} // stale links
) {
  LOG_TOPIC("4bdd2", DEBUG, arangodb::iresearch::TOPIC)
      << "link modification request for view '" << view.name() << "', original definition:" << links.toString();

  if (!links.isObject()) {
    return arangodb::Result( // result
      TRI_ERROR_BAD_PARAMETER, // code
      std::string("error parsing link parameters from json for arangosearch view '") + view.name() + "'"
    );
  }

  struct State {
    std::shared_ptr<arangodb::LogicalCollection> _collection;
    size_t _collectionsToLockOffset; // std::numeric_limits<size_t>::max() == removal only
    std::shared_ptr<arangodb::iresearch::IResearchLink> _link;
    size_t _linkDefinitionsOffset;
    arangodb::Result _result; // operation result
    bool _stale = false; // request came from the stale list
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
      return arangodb::Result( // result
        TRI_ERROR_BAD_PARAMETER, // code
        std::string("error parsing link parameters from json for arangosearch view '") + view.name() + "' offset '" + arangodb::basics::StringUtils::itoa(linksItr.index()) + '"'
      );
    }

    auto link = linksItr.value();
    auto collectionName = collection.copyString();

    if (link.isNull()) {
      linkModifications.emplace_back(collectionsToLock.size());
      collectionsToLock.emplace_back(collectionName);

      continue; // only removal requested
    }

    arangodb::velocypack::Builder normalized;

    normalized.openObject();

    // @note: DBServerAgencySync::getLocalCollections(...) generates
    //        'forPersistence' definitions that are then compared in
    //        Maintenance.cpp:compareIndexes(...) via
    //        arangodb::Index::Compare(...)
    //        hence must use 'isCreation=true' for normalize(...) to match
    auto res = arangodb::iresearch::IResearchLinkHelper::normalize( // normalize to validate analyzer definitions
        normalized, link, true, view.vocbase(), &view.primarySort(),
        &view.primarySortCompression(), &view.storedValues(),
        link.get(arangodb::StaticStrings::IndexId)
    );

    if (!res.ok()) {
      return res;
    }

    normalized.close();
    link = normalized.slice(); // use normalized definition for index creation

    LOG_TOPIC("4bdd1", DEBUG, arangodb::iresearch::TOPIC)
        << "link modification request for view '" << view.name() << "', normalized definition:" << link.toString();

    static const std::function<bool(irs::string_ref const& key)> acceptor = [](
        irs::string_ref const& key // json key
    )->bool {
      // ignored fields
      return key != arangodb::StaticStrings::IndexType // type field
          && key != arangodb::iresearch::StaticStrings::ViewIdField; // view id field
    };
    arangodb::velocypack::Builder namedJson;

    namedJson.openObject();
    namedJson.add( // add
      arangodb::StaticStrings::IndexType, // key
      arangodb::velocypack::Value(LINK_TYPE) // value
    );
    namedJson.add( // add
      arangodb::iresearch::StaticStrings::ViewIdField, // key
      arangodb::velocypack::Value(view.guid()) // value
    );

    if (!arangodb::iresearch::mergeSliceSkipKeys(namedJson, link, acceptor)) {
      return arangodb::Result(
        TRI_ERROR_INTERNAL,
        std::string("failed to update link definition with the view name while updating arangosearch view '") + view.name() + "' collection '" + collectionName + "'"
      );
    }

    namedJson.close();

    std::string error;
    arangodb::iresearch::IResearchLinkMeta linkMeta;

    if (!linkMeta.init(view.vocbase().server(),
                       namedJson.slice(), true, error, view.vocbase().name())) {  // validated and normalized with 'isCreation=true' above via normalize(...)
      return arangodb::Result(
        TRI_ERROR_BAD_PARAMETER,
        std::string("error parsing link parameters from json for arangosearch view '") + view.name() + "' collection '" + collectionName + "' error '" + error + "'"
      );
    }

    linkModifications.emplace_back(collectionsToLock.size(), linkDefinitions.size());
    collectionsToLock.emplace_back(collectionName);
    linkDefinitions.emplace_back(std::move(namedJson), std::move(linkMeta));
  }

  auto trxCtx = // transaction context
    arangodb::transaction::StandaloneContext::Create(view.vocbase());

  // add removals for any 'stale' links not found in the 'links' definition
  for (auto& id: stale) {
    if (!trxCtx->resolver().getCollection(id)) {
      LOG_TOPIC("4bdd7", WARN, arangodb::iresearch::TOPIC)
        << "request for removal of a stale link to a missing collection '" << id << "', ignoring";

      continue; // skip adding removal requests to stale links to non-existent collections (already dropped)
    }

    linkModifications.emplace_back(collectionsToLock.size());
    linkModifications.back()._stale = true;
    collectionsToLock.emplace_back(std::to_string(id));
  }

  if (collectionsToLock.empty()) {
    return arangodb::Result(); // nothing to update
  }

  arangodb::ExecContextSuperuserScope scope; // required to remove links from non-RW collections

  {
    std::unordered_set<TRI_voc_cid_t> collectionsToRemove; // track removal for potential reindex
    std::unordered_set<TRI_voc_cid_t> collectionsToUpdate; // track reindex requests

    // resolve corresponding collection and link
    for (auto itr = linkModifications.begin(); itr != linkModifications.end();) {
      auto& state = *itr;
      auto& collectionName = collectionsToLock[state._collectionsToLockOffset];

      state._collection = trxCtx->resolver().getCollection(collectionName);

      if (!state._collection) {
        // remove modification state if removal of non-existant link on
        // non-existant collection
        if (state._linkDefinitionsOffset >= linkDefinitions.size()) {  // link removal request
          itr = linkModifications.erase(itr);

          continue;
        }

        return arangodb::Result(
            TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND,
            std::string(
                "failed to get collection while updating arangosearch view '") +
                view.name() + "' collection '" + collectionName + "'");
      }

      state._link =
          arangodb::iresearch::IResearchLinkHelper::find(*(state._collection), view);

      // remove modification state if removal of non-existant link
      if (!state._link  // links currently does not exist
          && state._linkDefinitionsOffset >= linkDefinitions.size()) {  // link removal request
        LOG_TOPIC("c7111", TRACE, arangodb::iresearch::TOPIC)
            << "found link for collection '" << state._collection->name()
            << "' - slated for removal";

        view.unlink(state._collection->id());  // drop any stale data for the specified collection
        itr = linkModifications.erase(itr);

        continue;
      }

      if (state._link       // links currently exists
          && !state._stale  // did not originate from the stale list (remove
                            // stale links lower)
          && state._linkDefinitionsOffset >= linkDefinitions.size()) {  // link removal request
        LOG_TOPIC("a58da", TRACE, arangodb::iresearch::TOPIC)
            << "found link '" << state._link->id() << "' for collection '"
            << state._collection->name() << "' - slated for removal";
        auto cid = state._collection->id();

        // remove duplicate removal requests (e.g. by name + by CID)
        if (collectionsToRemove.find(cid) != collectionsToRemove.end()) {  // removal previously requested
          itr = linkModifications.erase(itr);

          continue;
        }

        collectionsToRemove.emplace(cid);
      }

      if (state._link  // links currently exists
          && state._linkDefinitionsOffset < linkDefinitions.size()) {  // link update request
        LOG_TOPIC("8419d", TRACE, arangodb::iresearch::TOPIC)
            << "found link '" << state._link->id() << "' for collection '"
            << state._collection->name() << "' - slated for update";
        collectionsToUpdate.emplace(state._collection->id());
      }

      LOG_TOPIC_IF("e9a8c", TRACE, arangodb::iresearch::TOPIC, state._link)
          << "found link '" << state._link->id() << "' for collection '"
          << state._collection->name() << "' - unsure what to do";

      LOG_TOPIC_IF("b01be", TRACE, arangodb::iresearch::TOPIC, !state._link)
          << "no link found for collection '" << state._collection->name() << "'";

      ++itr;
    }

    // remove modification state if it came from the stale list and a separate
    // removal or reindex also present
    for (auto itr = linkModifications.begin(); itr != linkModifications.end();) {
      auto& state = *itr;
      auto cid = state._collection->id();

      // remove modification if came from the stale list and a separate reindex
      // or removal request also present otherwise consider 'stale list
      // requests' as valid removal requests
      if (state._stale  // originated from the stale list
          && (collectionsToRemove.find(cid) !=
                  collectionsToRemove.end()  // also has a removal request
                                             // (duplicate removal request)
              || collectionsToUpdate.find(cid) != collectionsToUpdate.end())) {  // also has a reindex request
        itr = linkModifications.erase(itr);
        LOG_TOPIC("5c99e", TRACE, arangodb::iresearch::TOPIC)
            << "modification unnecessary, came from stale list, for link '"
            << state._link->id() << "'";

        continue;
      }

      ++itr;
    }

    // remove modification state if no change on existing link and reindex not
    // requested
    for (auto itr = linkModifications.begin(); itr != linkModifications.end();) {
      auto& state = *itr;

      // remove modification if removal request with an update request also
      // present
      if (state._link  // links currently exists
          && state._linkDefinitionsOffset >= linkDefinitions.size()  // link removal request
          && collectionsToUpdate.find(state._collection->id()) !=
                 collectionsToUpdate.end()) {  // also has a reindex request
        itr = linkModifications.erase(itr);
        LOG_TOPIC("1d095", TRACE, arangodb::iresearch::TOPIC)
            << "modification unnecessary, remove+update, for link '"
            << state._link->id() << "'";

        continue;
      }

      // remove modification state if no change on existing link or
      if (state._link  // links currently exists
          && state._linkDefinitionsOffset < linkDefinitions.size()  // link creation request
          && collectionsToRemove.find(state._collection->id()) ==
                 collectionsToRemove.end()  // not a reindex request
          && *(state._link) == linkDefinitions[state._linkDefinitionsOffset].second) {  // link meta not modified
        itr = linkModifications.erase(itr);
        LOG_TOPIC("4c196", TRACE, arangodb::iresearch::TOPIC)
            << "modification unnecessary, no change, for link '"
            << state._link->id() << "'";

        continue;
      }

      ++itr;
    }
  }

  // execute removals
  for (auto& state : linkModifications) {
    if (state._link) {  // link removal or recreate request
      state._result = dropLink<ViewType>(*(state._collection), *(state._link));
      modified.emplace(state._collection->id());
    }
  }

  // execute additions
  for (auto& state: linkModifications) {
    if (state._result.ok() // valid state (unmodified or after removal)
        && state._linkDefinitionsOffset < linkDefinitions.size()) {
      state._result = createLink( // create link
        *(state._collection), // collection
        view, // view
        linkDefinitions[state._linkDefinitionsOffset].first.slice() // definition
      );
      modified.emplace(state._collection->id());
    }
  }

  std::string error;

  // validate success
  for (auto& state: linkModifications) {
    if (!state._result.ok()) {
      error.append(error.empty() ? "" : ", ") // separator
        .append(collectionsToLock[state._collectionsToLockOffset]) // collection name
        .append(": ").append(std::to_string(state._result.errorNumber())) // error code
        .append(" ").append(state._result.errorMessage()); // error message
    }
  }

  if (error.empty()) {
    return arangodb::Result();
  }

  return arangodb::Result( // result
    TRI_ERROR_ARANGO_ILLEGAL_STATE, // code
    std::string("failed to update links while updating arangosearch view '") + view.name() + "', retry same request or examine errors for collections: " + error
  );
}

}  // namespace

namespace arangodb {
namespace iresearch {

/*static*/ VPackSlice const& IResearchLinkHelper::emptyIndexSlice() {
  static const struct EmptySlice {
    VPackBuilder _builder;
    VPackSlice _slice;
    EmptySlice() {
      VPackBuilder fieldsBuilder;

      fieldsBuilder.openArray();
      fieldsBuilder.close();  // empty array
      _builder.openObject();
      _builder.add(arangodb::StaticStrings::IndexFields,
                   fieldsBuilder.slice());  // empty array
      _builder.add(arangodb::StaticStrings::IndexType,
                   arangodb::velocypack::Value(LINK_TYPE));  // the index type required by Index
      _builder.close();  // object with just one field required by the Index
                         // constructor
      _slice = _builder.slice();
    }
  } emptySlice;

  // cppcheck-suppress returnReference
  return emptySlice._slice;
}

/*static*/ bool IResearchLinkHelper::equal(  // are link definitions equal
    arangodb::application_features::ApplicationServer& server,
    arangodb::velocypack::Slice const& lhs,  // left hand side
    arangodb::velocypack::Slice const& rhs,   // right hand side
    irs::string_ref const& dbname
) {
  if (!lhs.isObject() || !rhs.isObject()) {
    return false;
  }

  auto lhsViewSlice = lhs.get(StaticStrings::ViewIdField);
  auto rhsViewSlice = rhs.get(StaticStrings::ViewIdField);

  if (!lhsViewSlice.binaryEquals(rhsViewSlice)) {
    if (!lhsViewSlice.isString() || !rhsViewSlice.isString()) {
      return false;
    }

    auto ls = lhsViewSlice.copyString();
    auto rs = rhsViewSlice.copyString();

    if (ls.size() > rs.size()) {
      std::swap(ls, rs);
    }

    // in the cluster, we may have identifiers of the form
    // `cxxx/` and `cxxx/yyy` which should be considered equal if the
    // one is a prefix of the other up to the `/`
    if (ls.empty() || ls.back() != '/' || ls.compare(rs.substr(0, ls.size()))) {
      return false;
    }
  }

  std::string errorField;
  IResearchLinkMeta lhsMeta;
  IResearchLinkMeta rhsMeta;

  return lhsMeta.init(server, lhs, true, errorField, dbname)  // left side meta valid (for db-server analyzer validation should have already passed on coordinator)
         && rhsMeta.init(server, rhs, true, errorField, dbname)  // right side meta valid (for db-server analyzer validation should have already passed on coordinator)
         && lhsMeta == rhsMeta;  // left meta equal right meta
}

/*static*/ std::shared_ptr<IResearchLink> IResearchLinkHelper::find(  // find link
    LogicalCollection const& collection,  // collection to search
    IndexId id                            // index id to find
) {
  auto index = collection.lookupIndex(id);

  if (!index || arangodb::Index::TRI_IDX_TYPE_IRESEARCH_LINK != index->type()) {
    return nullptr;  // not an IResearchLink
  }

  // TODO FIXME find a better way to retrieve an IResearch Link
  // cannot use static_cast/reinterpret_cast since Index is not related to
  // IResearchLink
  auto link = std::dynamic_pointer_cast<IResearchLink>(index);

  if (link && link->id() == id) {
    return link;  // found required link
  }

  return nullptr;
}

/*static*/ std::shared_ptr<IResearchLink> IResearchLinkHelper::find(
    LogicalCollection const& collection, LogicalView const& view) {
  for (auto& index : collection.getIndexes()) {
    if (!index || arangodb::Index::TRI_IDX_TYPE_IRESEARCH_LINK != index->type()) {
      continue;  // not an IResearchLink
    }

    // TODO FIXME find a better way to retrieve an iResearch Link
    // cannot use static_cast/reinterpret_cast since Index is not related to
    // IResearchLink
    auto link = std::dynamic_pointer_cast<IResearchLink>(index);

    if (link && *link == view) {
      return link;  // found required link
    }
  }

  return nullptr;
}

/*static*/ arangodb::Result IResearchLinkHelper::normalize( // normalize definition
    arangodb::velocypack::Builder& normalized, // normalized definition (out-param)
    arangodb::velocypack::Slice definition, // source definition
    bool isCreation, // definition for index creation
    TRI_vocbase_t const& vocbase, // index vocbase
    IResearchViewSort const* primarySort, /* = nullptr */
    irs::type_info::type_id const* primarySortCompression /*= nullptr*/,
    IResearchViewStoredValues const* storedValues, /* = nullptr */
    arangodb::velocypack::Slice idSlice /* = arangodb::velocypack::Slice()*/ // id for normalized
) {
  if (!normalized.isOpenObject()) {
    return arangodb::Result(
      TRI_ERROR_BAD_PARAMETER,
      std::string("invalid output buffer provided for arangosearch link normalized definition generation")
    );
  }

  std::string error;
  IResearchLinkMeta meta;

  // @note: implicit analyzer validation via IResearchLinkMeta done in 2 places:
  //        IResearchLinkHelper::normalize(...) if creating via collection API
  //        ::modifyLinks(...) (via call to normalize(...) prior to getting
  //        superuser) if creating via IResearchLinkHelper API
  if (!meta.init(vocbase.server(), definition, true, error, vocbase.name())) {
    return arangodb::Result(
      TRI_ERROR_BAD_PARAMETER,
      std::string("error parsing arangosearch link parameters from json: ") + error
    );
  }

  auto res = canUseAnalyzers(meta, vocbase); // same validation as in modifyLinks(...) for Views API

  if (!res.ok()) {
    return res;
  }

  normalized.add(
    arangodb::StaticStrings::IndexType, arangodb::velocypack::Value(LINK_TYPE)
  );

  // copy over IResearch Link identifier
  if (!idSlice.isNone()) {
    if (idSlice.isNumber()) {
      normalized.add(
        arangodb::StaticStrings::IndexId,
        arangodb::velocypack::Value(std::to_string(idSlice.getNumericValue<uint64_t>()))
      );
    } else {
      normalized.add(
        arangodb::StaticStrings::IndexId,
        idSlice
      );
    }
  }

  // copy over IResearch View identifier
  if (definition.hasKey(StaticStrings::ViewIdField)) {
    normalized.add(
      StaticStrings::ViewIdField, definition.get(StaticStrings::ViewIdField)
    );
  }

  if (definition.hasKey(arangodb::StaticStrings::IndexInBackground)) {
    normalized.add( // preserve field
      arangodb::StaticStrings::IndexInBackground, // key
      definition.get(arangodb::StaticStrings::IndexInBackground) // value
    );
  }

  if (primarySort) {
    // normalize sort if specified
    meta._sort = *primarySort;
  }

  if (primarySortCompression) {
    meta._sortCompression = *primarySortCompression;
  }

  if (storedValues) {
    // normalize stored values if specified
    meta._storedValues = *storedValues;
  }

  if (!meta.json(vocbase.server(), normalized, isCreation, nullptr, &vocbase)) { // 'isCreation' is set when forPersistence
    return arangodb::Result( // result
      TRI_ERROR_BAD_PARAMETER, // code
      "error generating arangosearch link normalized definition" // message
    );
  }

  return arangodb::Result();
}

/*static*/ std::string const& IResearchLinkHelper::type() noexcept {
  return LINK_TYPE;
}

/*static*/ arangodb::Result IResearchLinkHelper::validateLinks( // validate
    TRI_vocbase_t& vocbase, // vocbase
    arangodb::velocypack::Slice const& links // link definitions
) {
  if (!links.isObject()) {
    return arangodb::Result( // result
      TRI_ERROR_BAD_PARAMETER, // code
      std::string("while validating arangosearch link definition, error: definition is not an object")
    );
  }

  size_t offset = 0;
  arangodb::CollectionNameResolver resolver(vocbase);

  for (arangodb::velocypack::ObjectIterator itr(links); // setup
       itr.valid(); // condition
       ++itr, ++offset // step
  ) {
    auto collectionName = itr.key();
    auto linkDefinition = itr.value();

    if (!collectionName.isString()) {
      return arangodb::Result( // result
        TRI_ERROR_BAD_PARAMETER, // code
        std::string("while validating arangosearch link definition, error: collection at offset ") + std::to_string(offset) + " is not a string"
      );
    }

    auto collection = resolver.getCollection(collectionName.copyString());

    if (!collection) {
      return arangodb::Result( // result
        TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND, // code
        std::string("while validating arangosearch link definition, error: collection '") + collectionName.copyString() + "' not found"
      );
    }

    // check link auth as per https://github.com/arangodb/backlog/issues/459
    if (!arangodb::ExecContext::current().canUseCollection(vocbase.name(), collection->name(), arangodb::auth::Level::RO)) {
      return arangodb::Result( // result
        TRI_ERROR_FORBIDDEN, // code
        std::string("while validating arangosearch link definition, error: collection '") + collectionName.copyString() + "' not authorized for read access"
      );
    }

    IResearchLinkMeta meta;
    std::string errorField;

    if (!linkDefinition.isNull()) { // have link definition
      if (!meta.init(vocbase.server(), linkDefinition, true, errorField, vocbase.name())) { // for db-server analyzer validation should have already applied on coordinator
        return arangodb::Result( // result
          TRI_ERROR_BAD_PARAMETER, // code
          errorField.empty()
          ? (std::string("while validating arangosearch link definition, error: invalid link definition for collection '") + collectionName.copyString() + "': " + linkDefinition.toString())
          : (std::string("while validating arangosearch link definition, error: invalid link definition for collection '") + collectionName.copyString() + "' error in attribute: " + errorField)
        );
      }
      // validate analyzers origin
      // analyzer should be either from same database as view (and collection) or from system database
      {
        const auto& currentVocbase = vocbase.name();
        for (const auto& analyzer : meta._analyzerDefinitions) {
          TRI_ASSERT(analyzer); // should be checked in meta init
          if (ADB_UNLIKELY(!analyzer)) {
            continue;
          }
          auto* pool = analyzer.get();
          auto analyzerVocbase = IResearchAnalyzerFeature::extractVocbaseName(pool->name());

          if (!IResearchAnalyzerFeature::analyzerReachableFromDb(analyzerVocbase, currentVocbase, true)) {
            return {
              TRI_ERROR_BAD_PARAMETER,
              std::string("Analyzer '")
                  .append(pool->name())
                  .append("' is not accessible from database '")
                  .append(currentVocbase).append("'")
            };
          }
        }
      }
    }
  }

  return arangodb::Result();
}

/*static*/ bool IResearchLinkHelper::visit( // visit
    arangodb::LogicalCollection const& collection, // collection to visit
    std::function<bool(IResearchLink& link)> const& visitor // visitor to apply
) {
  for (auto& index: collection.getIndexes()) {
    if (!index // not a valid index
        || arangodb::Index::TRI_IDX_TYPE_IRESEARCH_LINK != index->type()) {
      continue; // not an IResearchLink
    }

    // TODO FIXME find a better way to retrieve an iResearch Link
    // cannot use static_cast/reinterpret_cast since Index is not related to IResearchLink
    auto link = std::dynamic_pointer_cast<IResearchLink>(index);

    if (link && !visitor(*link)) {
      return false; // abort requested
    }
  }

  return true;
}

/*static*/ arangodb::Result IResearchLinkHelper::updateLinks(
    std::unordered_set<TRI_voc_cid_t>& modified,
    arangodb::LogicalView& view,
    arangodb::velocypack::Slice const& links,
    std::unordered_set<TRI_voc_cid_t> const& stale /*= {}*/
) {
  LOG_TOPIC("00bf9", TRACE, arangodb::iresearch::TOPIC)
      << "beginning IResearchLinkHelper::updateLinks";
  try {
    if (arangodb::ServerState::instance()->isCoordinator()) {
      return modifyLinks<IResearchViewCoordinator>( // modify
        modified, // modified cids
        LogicalView::cast<IResearchViewCoordinator>(view), // modified view
        links, // link modifications
        stale // stale links
      );
    }

    return modifyLinks<IResearchView>( // modify
      modified, // modified cids
      LogicalView::cast<IResearchView>(view), // modified view
      links, // link modifications
      stale // stale links
    );
  } catch (arangodb::basics::Exception& e) {
    LOG_TOPIC("72dde", WARN, arangodb::iresearch::TOPIC)
      << "caught exception while updating links for arangosearch view '" << view.name() << "': " << e.code() << " " << e.what();

    return arangodb::Result(
      e.code(),
      std::string("error updating links for arangosearch view '") + view.name() + "'"
    );
  } catch (std::exception const& e) {
    LOG_TOPIC("9d5f8", WARN, arangodb::iresearch::TOPIC)
      << "caught exception while updating links for arangosearch view '" << view.name() << "': " << e.what();

    return arangodb::Result(
      TRI_ERROR_BAD_PARAMETER,
      std::string("error updating links for arangosearch view '") + view.name() + "'"
    );
  } catch (...) {
    LOG_TOPIC("ff0b6", WARN, arangodb::iresearch::TOPIC)
      << "caught exception while updating links for arangosearch view '" << view.name() << "'";

    return arangodb::Result(
      TRI_ERROR_BAD_PARAMETER,
      std::string("error updating links for arangosearch view '") + view.name() + "'"
    );
  }
}

} // iresearch
} // arangodb

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------
