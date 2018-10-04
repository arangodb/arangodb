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

#include "IResearchLinkHelper.h"
#include "IResearchCommon.h"
#include "IResearchFeature.h"
#include "IResearchLink.h"
#include "IResearchLinkCoordinator.h"
#include "IResearchLinkMeta.h"
#include "IResearchView.h"
#include "IResearchViewCoordinator.h"
#include "IResearchViewDBServer.h"
#include "VelocyPackHelper.h"
#include "Basics/StaticStrings.h"
#include "Logger/Logger.h"
#include "Logger/LogMacros.h"
#include "Transaction/Methods.h"
#include "Transaction/StandaloneContext.h"
#include "Utils/CollectionNameResolver.h"
#include "Utils/ExecContext.h"
#include "velocypack/Iterator.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/Methods/Indexes.h"

namespace {

////////////////////////////////////////////////////////////////////////////////
/// @brief the string representing the link type
////////////////////////////////////////////////////////////////////////////////
std::string const& LINK_TYPE = arangodb::iresearch::DATA_SOURCE_TYPE.name();

bool createLink(
    arangodb::transaction::Methods& trx,
    arangodb::LogicalCollection& collection,
    arangodb::LogicalView const& view,
    arangodb::velocypack::Slice definition
) {
  bool isNew = false;
  auto link = collection.createIndex(&trx, definition, isNew);
  LOG_TOPIC_IF(DEBUG, arangodb::iresearch::TOPIC, link)
      << "added link '" << link->id() << "'";

  return link && isNew;
}

bool createLink(
    arangodb::transaction::Methods& trx,
    arangodb::LogicalCollection& collection,
    arangodb::iresearch::IResearchViewCoordinator const& view,
    arangodb::velocypack::Slice definition
) {
  static const std::function<bool(irs::string_ref const& key)> acceptor = [](
      irs::string_ref const& key
  )->bool {
    // ignored fields
    return key != arangodb::StaticStrings::IndexType
      && key != arangodb::iresearch::StaticStrings::ViewIdField;
  };
  arangodb::velocypack::Builder builder;

  builder.openObject();
  builder.add(
    arangodb::StaticStrings::IndexType,
    arangodb::velocypack::Value(LINK_TYPE)
  );
  builder.add(
    arangodb::iresearch::StaticStrings::ViewIdField,
    arangodb::velocypack::Value(view.guid())
  );

  if (!arangodb::iresearch::mergeSliceSkipKeys(builder, definition, acceptor)) {
    return false;
  }

  builder.close();

  arangodb::velocypack::Builder tmp;

  return arangodb::methods::Indexes::ensureIndex(
    &collection, builder.slice(), true, tmp
  ).ok();
}

bool dropLink(
    arangodb::LogicalCollection& collection,
    arangodb::iresearch::IResearchLink const& link
) {
  // don't need to create an extra transaction inside arangodb::methods::Indexes::drop(...)
  return collection.dropIndex(link.id());
}

bool dropLink(
    arangodb::LogicalCollection& collection,
    arangodb::iresearch::IResearchLinkCoordinator const& link
) {
  arangodb::velocypack::Builder builder;

  builder.openObject();
  builder.add(
    arangodb::StaticStrings::IndexId,
    arangodb::velocypack::Value(link.id())
  );
  builder.close();

  return arangodb::methods::Indexes::drop(&collection, builder.slice()).ok();
}

template<typename ViewType, typename LinkType>
arangodb::Result modifyLinks(
  std::unordered_set<TRI_voc_cid_t>& modified,
  TRI_vocbase_t& vocbase,
  ViewType& view,
  arangodb::velocypack::Slice const& links,
  std::unordered_set<TRI_voc_cid_t> const& stale = {}
) {
  if (!links.isObject()) {
    return arangodb::Result(
      TRI_ERROR_BAD_PARAMETER,
      std::string("error parsing link parameters from json for IResearch view '") + view.name() + "'"
    );
  }

  struct State {
    std::shared_ptr<arangodb::LogicalCollection> _collection;
    size_t _collectionsToLockOffset; // std::numeric_limits<size_t>::max() == removal only
    std::shared_ptr<LinkType> _link;
    size_t _linkDefinitionsOffset;
    bool _stale = false; // request came from the stale list
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
        std::string("error parsing link parameters from json for IResearch view '") + view.name() + "' offset '" + arangodb::basics::StringUtils::itoa(linksItr.index()) + '"'
      );
    }

    auto link = linksItr.value();
    auto collectionName = collection.copyString();

    if (link.isNull()) {
      linkModifications.emplace_back(collectionsToLock.size());
      collectionsToLock.emplace_back(collectionName);

      continue; // only removal requested
    }

    static const std::function<bool(irs::string_ref const& key)> acceptor = [](
        irs::string_ref const& key
    )->bool {
      // ignored fields
      return key != arangodb::StaticStrings::IndexType
        && key != arangodb::iresearch::StaticStrings::ViewIdField;
    };
    arangodb::velocypack::Builder namedJson;

    namedJson.openObject();
    namedJson.add(
      arangodb::StaticStrings::IndexType,
      arangodb::velocypack::Value(LINK_TYPE)
    );
    namedJson.add(
      arangodb::iresearch::StaticStrings::ViewIdField,
      arangodb::velocypack::Value(view.guid())
    );

    if (!arangodb::iresearch::mergeSliceSkipKeys(namedJson, link, acceptor)) {
      return arangodb::Result(
        TRI_ERROR_INTERNAL,
        std::string("failed to update link definition with the view name while updating IResearch view '") + view.name() + "' collection '" + collectionName + "'"
      );
    }

    namedJson.close();

    std::string error;
    arangodb::iresearch::IResearchLinkMeta linkMeta;

    if (!linkMeta.init(namedJson.slice(), error)) {
      return arangodb::Result(
        TRI_ERROR_BAD_PARAMETER,
        std::string("error parsing link parameters from json for IResearch view '") + view.name() + "' collection '" + collectionName + "' error '" + error + "'"
      );
    }

    linkModifications.emplace_back(collectionsToLock.size(), linkDefinitions.size());
    collectionsToLock.emplace_back(collectionName);
    linkDefinitions.emplace_back(std::move(namedJson), std::move(linkMeta));
  }

  // add removals for any 'stale' links not found in the 'links' definition
  for (auto& id: stale) {
    linkModifications.emplace_back(collectionsToLock.size());
    linkModifications.back()._stale = true;
    collectionsToLock.emplace_back(std::to_string(id));
  }

  if (collectionsToLock.empty()) {
    return arangodb::Result(); // nothing to update
  }

  static std::vector<std::string> const EMPTY;
  arangodb::ExecContextScope scope(arangodb::ExecContext::superuser()); // required to remove links from non-RW collections
  arangodb::transaction::Methods trx(
    arangodb::transaction::StandaloneContext::Create(vocbase),
    EMPTY, // readCollections
    EMPTY, // writeCollections
    collectionsToLock, // exclusiveCollections
    arangodb::transaction::Options() // use default lock timeout
  );
  auto* trxResolver = trx.resolver();

  if (!trxResolver) {
    return arangodb::Result(
      TRI_ERROR_ARANGO_ILLEGAL_STATE,
      std::string("failed to find collection name resolver while updating IResearch view '") + view.name() + "'"
    );
  }

  auto res = trx.begin();

  if (!res.ok()) {
    return res;
  }

  {
    std::unordered_set<TRI_voc_cid_t> collectionsToRemove; // track removal for potential reindex
    std::unordered_set<TRI_voc_cid_t> collectionsToUpdate; // track reindex requests

    // resolve corresponding collection and link
    for (auto itr = linkModifications.begin(); itr != linkModifications.end();) {
      auto& state = *itr;
      auto& collectionName = collectionsToLock[state._collectionsToLockOffset];

      state._collection = trxResolver->getCollection(collectionName);

      if (!state._collection) {
        // remove modification state if removal of non-existant link on non-existant collection
        if (state._linkDefinitionsOffset >= linkDefinitions.size()) { // link removal request
          itr = linkModifications.erase(itr);

          continue;
        }

        return arangodb::Result(
          TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND,
          std::string("failed to get collection while updating IResearch view '") + view.name() + "' collection '" + collectionName + "'"
        );
      }

      state._link = LinkType::find(*(state._collection), view);

      // remove modification state if removal of non-existant link
      if (!state._link // links currently does not exist
          && state._linkDefinitionsOffset >= linkDefinitions.size()) { // link removal request
        LOG_TOPIC(TRACE, arangodb::iresearch::TOPIC)
            << "found link '" << state._link->id() << "' for collection '"
            << state._collection->name() << "' - slated for removal";
        view.drop(state._collection->id()); // drop any stale data for the specified collection
        itr = linkModifications.erase(itr);

        continue;
      }

      if (state._link // links currently exists
          && !state._stale // did not originate from the stale list (remove stale links lower)
          && state._linkDefinitionsOffset >= linkDefinitions.size()) { // link removal request
        LOG_TOPIC(TRACE, arangodb::iresearch::TOPIC)
            << "found link '" << state._link->id() << "' for collection '"
            << state._collection->name() << "' - slated for removal";
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
        LOG_TOPIC(TRACE, arangodb::iresearch::TOPIC)
            << "found link '" << state._link->id() << "' for collection '"
            << state._collection->name() << "' - slated for update";
        collectionsToUpdate.emplace(state._collection->id());
      }

      LOG_TOPIC_IF(TRACE, arangodb::iresearch::TOPIC, state._link)
          << "found link '" << state._link->id() << "' for collection '"
          << state._collection->name() << "' - unsure what to do";

      LOG_TOPIC_IF(TRACE, arangodb::iresearch::TOPIC, !state._link)
          << "no link found for collection '"
          << state._collection->name() << "'";

      ++itr;
    }

    // remove modification state if it came from the stale list and a separate removal or reindex also present
    for (auto itr = linkModifications.begin(); itr != linkModifications.end();) {
      auto& state = *itr;
      auto cid = state._collection->id();

      // remove modification if came from the stale list and a separate reindex or removal request also present
      // otherwise consider 'stale list requests' as valid removal requests
      if (state._stale // originated from the stale list
          && (collectionsToRemove.find(cid) != collectionsToRemove.end() // also has a removal request (duplicate removal request)
              || collectionsToUpdate.find(cid) != collectionsToUpdate.end())) { // also has a reindex request
        itr = linkModifications.erase(itr);
        LOG_TOPIC(TRACE, arangodb::iresearch::TOPIC)
            << "modification unnecessary, came from stale list, for link '"
            << state._link->id() << "'";

        continue;
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
        LOG_TOPIC(TRACE, arangodb::iresearch::TOPIC)
            << "modification unnecessary, remove+update, for link '"
            << state._link->id() << "'";

        continue;
      }

      // remove modification state if no change on existing link or
      if (state._link // links currently exists
          && state._linkDefinitionsOffset < linkDefinitions.size() // link creation request
          && collectionsToRemove.find(state._collection->id()) == collectionsToRemove.end() // not a reindex request
          && *(state._link) == linkDefinitions[state._linkDefinitionsOffset].second) { // link meta not modified
        itr = linkModifications.erase(itr);
        LOG_TOPIC(TRACE, arangodb::iresearch::TOPIC)
            << "modification unnecessary, no change, for link '"
            << state._link->id() << "'";

        continue;
      }

      ++itr;
    }
  }

  // execute removals
  for (auto& state: linkModifications) {
    if (state._link) { // link removal or recreate request
      LOG_TOPIC(DEBUG, arangodb::iresearch::TOPIC)
          << "removed link '" << state._link->id() << "'";
      state._valid = dropLink(*(state._collection), *(state._link));
      modified.emplace(state._collection->id());
    }
  }

  // execute additions
  for (auto& state: linkModifications) {
    if (state._valid && state._linkDefinitionsOffset < linkDefinitions.size()) {
      state._valid = createLink(
        trx,
        *(state._collection),
        view,
        linkDefinitions[state._linkDefinitionsOffset].first.slice()
      );
      modified.emplace(state._collection->id());
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
    std::string("failed to update links while updating IResearch view '") + view.name() + "', retry same request or examine errors for collections: " + error
  );
}

}

namespace arangodb {
namespace iresearch {

/*static*/ VPackSlice const& IResearchLinkHelper::emptyIndexSlice() {
  static const struct EmptySlice {
    VPackBuilder _builder;
    VPackSlice _slice;
    EmptySlice() {
      VPackBuilder fieldsBuilder;

      fieldsBuilder.openArray();
      fieldsBuilder.close(); // empty array
      _builder.openObject();
      _builder.add(arangodb::StaticStrings::IndexFields, fieldsBuilder.slice()); // empty array
      _builder.add(
        arangodb::StaticStrings::IndexType,
        arangodb::velocypack::Value(LINK_TYPE)
      ); // the index type required by Index
      _builder.close(); // object with just one field required by the Index constructor
      _slice = _builder.slice();
    }
  } emptySlice;

  return emptySlice._slice;
}

/*static*/ arangodb::Result IResearchLinkHelper::normalize(
  arangodb::velocypack::Builder& normalized,
  velocypack::Slice definition,
  bool // isCreation
) {
  if (!normalized.isOpenObject()) {
    return arangodb::Result(
      TRI_ERROR_BAD_PARAMETER,
      std::string("invalid output buffer provided for IResearch link normalized definition generation")
    );
  }

  std::string error;
  IResearchLinkMeta meta;

  if (!meta.init(definition, error)) {
    return arangodb::Result(
      TRI_ERROR_BAD_PARAMETER,
      std::string("error parsing IResearch link parameters from json: ") + error
    );
  }

  normalized.add(
    arangodb::StaticStrings::IndexType, arangodb::velocypack::Value(LINK_TYPE)
  );

  // copy over IResearch View identifier
  if (definition.hasKey(StaticStrings::ViewIdField)) {
    normalized.add(
      StaticStrings::ViewIdField, definition.get(StaticStrings::ViewIdField)
    );
  }

  return meta.json(normalized)
    ? arangodb::Result()
    : arangodb::Result(
        TRI_ERROR_BAD_PARAMETER,
        std::string("error generating IResearch link normalized definition")
      )
    ;
}

/*static*/ std::string const& IResearchLinkHelper::type() noexcept {
  return LINK_TYPE;
}

/*static*/ arangodb::Result IResearchLinkHelper::updateLinks(
    std::unordered_set<TRI_voc_cid_t>& modified,
    TRI_vocbase_t& vocbase,
    arangodb::LogicalView& view,
    arangodb::velocypack::Slice const& links,
    std::unordered_set<TRI_voc_cid_t> const& stale /*= {}*/
) {
  LOG_TOPIC(TRACE, arangodb::iresearch::TOPIC)
      << "beginning IResearchLinkHelper::updateLinks";
  try {
    if (arangodb::ServerState::instance()->isCoordinator()) {
      return modifyLinks<IResearchViewCoordinator, IResearchLinkCoordinator>(
        modified,
        vocbase,
        LogicalView::cast<IResearchViewCoordinator>(view),
        links,
        stale
      );
    }

    auto* dbServerView = LogicalView::cast<IResearchViewDBServer>(&view);

    // dbserver has both IResearchViewDBServer and IResearchView instances
    if (dbServerView) {
      return modifyLinks<IResearchViewDBServer, IResearchLink>(
        modified,
        vocbase,
        *dbServerView,
        links,
        stale
      );
    }

    return modifyLinks<IResearchView, IResearchLink>(
      modified,
      vocbase,
      LogicalView::cast<IResearchView>(view),
      links,
      stale
    );
  } catch (arangodb::basics::Exception& e) {
    LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
      << "caught exception while updating links for IResearch view '" << view.name() << "': " << e.code() << " " << e.what();
    IR_LOG_EXCEPTION();

    return arangodb::Result(
      e.code(),
      std::string("error updating links for IResearch view '") + view.name() + "'"
    );
  } catch (std::exception const& e) {
    LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
      << "caught exception while updating links for IResearch view '" << view.name() << "': " << e.what();
    IR_LOG_EXCEPTION();

    return arangodb::Result(
      TRI_ERROR_BAD_PARAMETER,
      std::string("error updating links for IResearch view '") + view.name() + "'"
    );
  } catch (...) {
    LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
      << "caught exception while updating links for IResearch view '" << view.name() << "'";
    IR_LOG_EXCEPTION();

    return arangodb::Result(
      TRI_ERROR_BAD_PARAMETER,
      std::string("error updating links for IResearch view '") + view.name() + "'"
    );
  }
}

} // iresearch
} // arangodb

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------
