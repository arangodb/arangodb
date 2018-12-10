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
    arangodb::LogicalCollection& collection,
    arangodb::LogicalView const& view,
    arangodb::velocypack::Slice definition
) {
  bool isNew = false;
  auto link = collection.createIndex(definition, isNew);
  LOG_TOPIC_IF(DEBUG, arangodb::iresearch::TOPIC, link)
      << "added link '" << link->id() << "'";

  return link && isNew;
}

bool createLink(
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

template<typename ViewType>
bool dropLink(
    arangodb::LogicalCollection& collection,
    arangodb::iresearch::IResearchLink const& link
) {
  // don't need to create an extra transaction inside arangodb::methods::Indexes::drop(...)
  return collection.dropIndex(link.id());
}

template<>
bool dropLink<arangodb::iresearch::IResearchViewCoordinator>(
    arangodb::LogicalCollection& collection,
    arangodb::iresearch::IResearchLink const& link
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

template<typename ViewType>
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
      std::string("error parsing link parameters from json for arangosearch view '") + view.name() + "'"
    );
  }

  struct State {
    std::shared_ptr<arangodb::LogicalCollection> _collection;
    size_t _collectionsToLockOffset; // std::numeric_limits<size_t>::max() == removal only
    std::shared_ptr<arangodb::iresearch::IResearchLink> _link;
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
        std::string("failed to update link definition with the view name while updating arangosearch view '") + view.name() + "' collection '" + collectionName + "'"
      );
    }

    namedJson.close();

    std::string error;
    arangodb::iresearch::IResearchLinkMeta linkMeta;

    if (!linkMeta.init(namedJson.slice(), error)) {
      return arangodb::Result(
        TRI_ERROR_BAD_PARAMETER,
        std::string("error parsing link parameters from json for arangosearch view '") + view.name() + "' collection '" + collectionName + "' error '" + error + "'"
      );
    }

    linkModifications.emplace_back(collectionsToLock.size(), linkDefinitions.size());
    collectionsToLock.emplace_back(collectionName);
    linkDefinitions.emplace_back(std::move(namedJson), std::move(linkMeta));
  }

  auto trxCtx = arangodb::transaction::StandaloneContext::Create(vocbase);

  // add removals for any 'stale' links not found in the 'links' definition
  for (auto& id: stale) {
    if (!trxCtx->resolver().getCollection(id)) {
      LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
        << "request for removal of a stale link to a missing collection '" << id << "', ignoring";

      continue; // skip adding removal requests to stale links to non-existant collections (already dropped)
    }

    linkModifications.emplace_back(collectionsToLock.size());
    linkModifications.back()._stale = true;
    collectionsToLock.emplace_back(std::to_string(id));
  }

  if (collectionsToLock.empty()) {
    return arangodb::Result(); // nothing to update
  }

  static std::vector<std::string> const EMPTY;
  arangodb::ExecContextScope scope(arangodb::ExecContext::superuser()); // required to remove links from non-RW collections

  {
    std::unordered_set<TRI_voc_cid_t> collectionsToRemove; // track removal for potential reindex
    std::unordered_set<TRI_voc_cid_t> collectionsToUpdate; // track reindex requests

    // resolve corresponding collection and link
    for (auto itr = linkModifications.begin(); itr != linkModifications.end();) {
      auto& state = *itr;
      auto& collectionName = collectionsToLock[state._collectionsToLockOffset];

      state._collection = trxCtx->resolver().getCollection(collectionName);

      if (!state._collection) {
        // remove modification state if removal of non-existant link on non-existant collection
        if (state._linkDefinitionsOffset >= linkDefinitions.size()) { // link removal request
          itr = linkModifications.erase(itr);

          continue;
        }

        return arangodb::Result(
          TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND,
          std::string("failed to get collection while updating arangosearch view '") + view.name() + "' collection '" + collectionName + "'"
        );
      }

      state._link = arangodb::iresearch::IResearchLinkHelper::find(
        *(state._collection),
        view
      );

      // remove modification state if removal of non-existant link
      if (!state._link // links currently does not exist
          && state._linkDefinitionsOffset >= linkDefinitions.size()) { // link removal request

        LOG_TOPIC(TRACE, arangodb::iresearch::TOPIC)
            << "found link for collection '"
            << state._collection->name() << "' - slated for removal";

        view.unlink(state._collection->id()); // drop any stale data for the specified collection
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
      state._valid = dropLink<ViewType>(*(state._collection), *(state._link));
      modified.emplace(state._collection->id());
    }
  }

  // execute additions
  for (auto& state: linkModifications) {
    if (state._valid && state._linkDefinitionsOffset < linkDefinitions.size()) {
      state._valid = createLink(
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
    return arangodb::Result();
  }

  return arangodb::Result(
    TRI_ERROR_ARANGO_ILLEGAL_STATE,
    std::string("failed to update links while updating arangosearch view '") + view.name() + "', retry same request or examine errors for collections: " + error
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

/*static*/ bool IResearchLinkHelper::equal(
    arangodb::velocypack::Slice const& lhs,
    arangodb::velocypack::Slice const& rhs
) {
  if (!lhs.isObject() || !rhs.isObject()) {
    return false;
  }

  auto lhsViewSlice = lhs.get(StaticStrings::ViewIdField);
  auto rhsViewSlice = rhs.get(StaticStrings::ViewIdField);

  if (!lhsViewSlice.equals(rhsViewSlice)) {
    if (!lhsViewSlice.isString() || !rhsViewSlice.isString()) {
      return false;
    }

    auto ls = lhsViewSlice.copyString();
    auto rs = lhsViewSlice.copyString();

    if (ls.size() > rs.size()) {
      std::swap(ls, rs);
    }

    // in the cluster, we may have identifiers of the form
    // `cxxx/` and `cxxx/yyy` which should be considered equal if the
    // one is a prefix of the other up to the `/`
    if (ls.empty() ||
        ls.back() != '/' ||
        ls.compare(rs.substr(0, ls.size()))) {
      return false;
    }
  }

  std::string errorField;
  IResearchLinkMeta lhsMeta;
  IResearchLinkMeta rhsMeta;

  return lhsMeta.init(lhs, errorField)
         && rhsMeta.init(rhs, errorField)
         && lhsMeta == rhsMeta;
}

/*static*/ std::shared_ptr<IResearchLink> IResearchLinkHelper::find(
    LogicalCollection const& collection,
    TRI_idx_iid_t id
) {
  auto index = collection.lookupIndex(id);

  if (!index || arangodb::Index::TRI_IDX_TYPE_IRESEARCH_LINK != index->type()) {
    return nullptr; // not an IResearchLink
  }

  // TODO FIXME find a better way to retrieve an IResearch Link
  // cannot use static_cast/reinterpret_cast since Index is not related to IResearchLink
  auto link = std::dynamic_pointer_cast<IResearchLink>(index);

  if (link && link->id() == id) {
    return link; // found required link
  }

  return nullptr;
}

/*static*/ std::shared_ptr<IResearchLink> IResearchLinkHelper::find(
    LogicalCollection const& collection,
    LogicalView const& view
) {
  for (auto& index: collection.getIndexes()) {
    if (!index
        || arangodb::Index::TRI_IDX_TYPE_IRESEARCH_LINK != index->type()) {
      continue; // not an IResearchLink
    }

    // TODO FIXME find a better way to retrieve an iResearch Link
    // cannot use static_cast/reinterpret_cast since Index is not related to IResearchLink
    auto link = std::dynamic_pointer_cast<IResearchLink>(index);

    if (link && *link == view) {
      return link; // found required link
    }
  }

  return nullptr;
}

/*static*/ arangodb::Result IResearchLinkHelper::normalize(
  arangodb::velocypack::Builder& normalized,
  velocypack::Slice definition,
  bool // isCreation
) {
  if (!normalized.isOpenObject()) {
    return arangodb::Result(
      TRI_ERROR_BAD_PARAMETER,
      std::string("invalid output buffer provided for arangosearch link normalized definition generation")
    );
  }

  std::string error;
  IResearchLinkMeta meta;

  if (!meta.init(definition, error)) {
    return arangodb::Result(
      TRI_ERROR_BAD_PARAMETER,
      std::string("error parsing arangosearch link parameters from json: ") + error
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
        std::string("error generating arangosearch link normalized definition")
      )
    ;
}

/*static*/ std::string const& IResearchLinkHelper::type() noexcept {
  return LINK_TYPE;
}

/*static*/ arangodb::Result IResearchLinkHelper::validateLinks(
    TRI_vocbase_t& vocbase,
    arangodb::velocypack::Slice const& links
) {
  if (!links.isObject()) {
    return arangodb::Result(
      TRI_ERROR_BAD_PARAMETER,
      std::string("while validating arangosearch link definition, error: definition is not an object")
    );
  }

  size_t offset = 0;
  arangodb::CollectionNameResolver resolver(vocbase);

  for (arangodb::velocypack::ObjectIterator itr(links);
       itr.valid();
       ++itr, ++offset
  ) {
    auto collectionName = itr.key();
    auto linkDefinition = itr.value();

    if (!collectionName.isString()) {
      return arangodb::Result(
        TRI_ERROR_BAD_PARAMETER,
        std::string("while validating arangosearch link definition, error: collection at offset ") + std::to_string(offset) + " is not a string"
      );
    }

    auto collection = resolver.getCollection(collectionName.copyString());

    if (!collection) {
      return arangodb::Result(
        TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND,
        std::string("while validating arangosearch link definition, error: collection '") + collectionName.copyString() + "' not found"
      );
    }

    // check link auth as per https://github.com/arangodb/backlog/issues/459
    if (arangodb::ExecContext::CURRENT
        && !arangodb::ExecContext::CURRENT->canUseCollection(vocbase.name(), collection->name(), arangodb::auth::Level::RO)) {
      return arangodb::Result(
        TRI_ERROR_FORBIDDEN,
        std::string("while validating arangosearch link definition, error: collection '") + collectionName.copyString() + "' not authorized for read access"
      );
    }

    IResearchLinkMeta meta;
    std::string errorField;

    if (!linkDefinition.isNull() && !meta.init(linkDefinition, errorField)) {
      return arangodb::Result(
        TRI_ERROR_BAD_PARAMETER,
        errorField.empty()
         ? (std::string("while validating arangosearch link definition, error: invalid link definition for collection '") + collectionName.copyString() + "': " + linkDefinition.toString())
         : (std::string("while validating arangosearch link definition, error: invalid link definition for collection '") + collectionName.copyString() + "' error in attribute: " + errorField)
      );
    }
  }

  return arangodb::Result();
}

/*static*/ bool IResearchLinkHelper::visit(
  arangodb::LogicalCollection const& collection,
  std::function<bool(IResearchLink& link)> const& visitor
) {
  for (auto& index: collection.getIndexes()) {
    if (!index
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
    TRI_vocbase_t& vocbase,
    arangodb::LogicalView& view,
    arangodb::velocypack::Slice const& links,
    std::unordered_set<TRI_voc_cid_t> const& stale /*= {}*/
) {
  LOG_TOPIC(TRACE, arangodb::iresearch::TOPIC)
      << "beginning IResearchLinkHelper::updateLinks";
  try {
    if (arangodb::ServerState::instance()->isCoordinator()) {
      return modifyLinks<IResearchViewCoordinator>(
        modified,
        vocbase,
        LogicalView::cast<IResearchViewCoordinator>(view),
        links,
        stale
      );
    }

    auto* dbServerView = dynamic_cast<IResearchViewDBServer*>(&view);

    // dbserver has both IResearchViewDBServer and IResearchView instances
    if (dbServerView) {
      return modifyLinks<IResearchViewDBServer>(
        modified,
        vocbase,
        *dbServerView,
        links,
        stale
      );
    }

    return modifyLinks<IResearchView>(
      modified,
      vocbase,
      LogicalView::cast<IResearchView>(view),
      links,
      stale
    );
  } catch (arangodb::basics::Exception& e) {
    LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
      << "caught exception while updating links for arangosearch view '" << view.name() << "': " << e.code() << " " << e.what();
    IR_LOG_EXCEPTION();

    return arangodb::Result(
      e.code(),
      std::string("error updating links for arangosearch view '") + view.name() + "'"
    );
  } catch (std::exception const& e) {
    LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
      << "caught exception while updating links for arangosearch view '" << view.name() << "': " << e.what();
    IR_LOG_EXCEPTION();

    return arangodb::Result(
      TRI_ERROR_BAD_PARAMETER,
      std::string("error updating links for arangosearch view '") + view.name() + "'"
    );
  } catch (...) {
    LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
      << "caught exception while updating links for arangosearch view '" << view.name() << "'";
    IR_LOG_EXCEPTION();

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