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

#include "IResearchViewCoordinator.h"
#include "IResearchCommon.h"
#include "IResearchLinkCoordinator.h"
#include "IResearchLinkHelper.h"
#include "Basics/StringUtils.h"
#include "Cluster/ClusterInfo.h"
#include "Cluster/ClusterFeature.h"
#include "Cluster/ServerState.h"
#include "IResearch/IResearchFeature.h"
#include "IResearch/VelocyPackHelper.h"
#include "Utils/CollectionNameResolver.h"
#include "VocBase/Methods/Indexes.h"
#include "VocBase/LogicalCollection.h"
#include <velocypack/Builder.h>
#include <velocypack/Collection.h>
#include <velocypack/Parser.h>

namespace {

typedef irs::async_utils::read_write_mutex::read_mutex ReadMutex;
typedef irs::async_utils::read_write_mutex::write_mutex WriteMutex;

using namespace arangodb::iresearch;

arangodb::Result dropLink(
    IResearchLinkCoordinator const& link,
    arangodb::LogicalCollection& collection,
    VPackBuilder& builder
) {
  TRI_ASSERT(builder.isEmpty());

  builder.openObject();
  builder.add("id", VPackValue(link.id()));
  builder.close();

  return arangodb::methods::Indexes::drop(&collection, builder.slice());
}

arangodb::Result createLink(
    arangodb::LogicalCollection& collection,
    IResearchViewCoordinator const& view,
    VPackSlice link,
    VPackBuilder& builder
) {
  TRI_ASSERT(builder.isEmpty());

  static const std::function<bool(irs::string_ref const& key)> acceptor = [](
      irs::string_ref const& key
  )->bool {
    // ignored fields
    return key != arangodb::StaticStrings::IndexType
      && key != StaticStrings::ViewIdField;
  };

  builder.openObject();
  builder.add(
    arangodb::StaticStrings::IndexType,
    arangodb::velocypack::Value(IResearchLinkHelper::type())
  );
  builder.add(
    StaticStrings::ViewIdField, arangodb::velocypack::Value(view.id())
  );

  if (!mergeSliceSkipKeys(builder, link, acceptor)) {
    return arangodb::Result(
      TRI_ERROR_INTERNAL,
      std::string("failed to update link definition with the view name while updating IResearch view '")
      + std::to_string(view.id()) + "' collection '" + collection.name() + "'"
    );
  }

  builder.close();

  VPackBuilder tmp;

  return arangodb::methods::Indexes::ensureIndex(
    &collection, builder.slice(), true, tmp
  );
}

arangodb::Result updateLinks(
    VPackSlice newLinks,
    VPackSlice currentLinks,
    IResearchViewCoordinator const& view,
    bool partialUpdate,
    std::unordered_set<TRI_voc_cid_t> currentCids, // intentional copy
    bool& modified,
    VPackBuilder& viewNewProperties,
    std::unordered_set<TRI_voc_cid_t>& newCids
) {
  if (!newLinks.isObject()) {
    newLinks = VPackSlice::emptyObjectSlice();
  }

  arangodb::CollectionNameResolver resolver(view.vocbase());
  arangodb::Result res;
  std::string error;
  VPackBuilder builder;
  IResearchLinkMeta linkMeta;

  // process new links
  for (VPackObjectIterator linksItr(newLinks); linksItr.valid(); ++linksItr) {
    auto const collectionNameOrIdSlice = linksItr.key();

    if (!collectionNameOrIdSlice.isString()) {
      return {
        TRI_ERROR_BAD_PARAMETER,
        std::string("error parsing link parameters from json for IResearch view '")
          + view.name()
          + "' offset '"
          + std::to_string(linksItr.index())
          + '"'
      };
    }

    auto const collectionNameOrId = collectionNameOrIdSlice.copyString();
    auto const collection = resolver.getCollection(collectionNameOrId);

    if (!collection) {
      return { TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND };
    }

    auto const link = linksItr.value();

    builder.clear();
    res.reset();

    auto const existingLink = IResearchLinkCoordinator::find(*collection, view);

    if (link.isNull()) {
      if (existingLink) {
        res = dropLink(*existingLink, *collection, builder);

        // do not need to drop link afterwards
        currentCids.erase(collection->id());
        modified = true;
      }
    } else {
      if (!linkMeta.init(link, error)) {
        return { TRI_ERROR_BAD_PARAMETER, error };
      }

      // append link definition
      {
        VPackObjectBuilder const guard(&viewNewProperties, collectionNameOrId);
        linkMeta.json(viewNewProperties);
      }

      currentCids.erase(collection->id()); // already processed
      newCids.emplace(collection->id()); // new collection

      if (existingLink) {
        if (*existingLink == linkMeta) {
          // nothing to do
          continue;
        }

        res = dropLink(*existingLink, *collection, builder);

        if (!res.ok()) {
          return res;
        }

        builder.clear();
      }

      res = createLink(*collection, view, link, builder);
      modified = true;
    }

    if (!res.ok()) {
      return res;
    }
  }

  // process the rest of the nonprocessed collections
  for (auto const cid : currentCids) {
    auto const collection = resolver.getCollection(cid);

    if (!collection) {
      return { TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND };
    }

    if (!partialUpdate) {
      auto const link = IResearchLinkCoordinator::find(*collection, view);

      if (!link) {
        return {
          TRI_ERROR_ARANGO_INDEX_NOT_FOUND,
          std::string("no link between view '")
            + view.name()
            + "' and collection '"
            + std::to_string(cid)
            + "' found"
        };
      }

      builder.clear();
      res = dropLink(*link, *collection, builder);
      modified = true;
    } else {
      auto link = currentLinks.get(collection->name());

      if (!link.isObject()) {
        auto const collectiondId = std::to_string(collection->id());
        link = currentLinks.get(collectiondId);

        if (!link.isObject()) {
          // inconsistent links
          return { TRI_ERROR_BAD_PARAMETER };
        }

        viewNewProperties.add(collectiondId, link);
      } else {
        viewNewProperties.add(collection->name(), link);
      }

      newCids.emplace(collection->id());
    }

    if (!res.ok()) {
      return res;
    }
  }

  return res;
}

}

namespace arangodb {

using namespace basics;

namespace iresearch {

arangodb::Result IResearchViewCoordinator::appendVelocyPack(
  arangodb::velocypack::Builder& builder,
  bool detailed,
  bool forPersistence
) const {
  if (!builder.isOpenObject()) {
    return arangodb::Result(
      TRI_ERROR_BAD_PARAMETER,
      std::string("invalid builder provided for IResearchViewCoordinator definition")
    );
  }

  builder.add(
    arangodb::StaticStrings::DataSourceType,
    arangodb::velocypack::Value(type().name())
  );

  if (!detailed) {
    return arangodb::Result();
  }

  builder.add(
    StaticStrings::PropertiesField,
    arangodb::velocypack::Value(arangodb::velocypack::ValueType::Object)
  );
  _meta.json(builder); // regular properites

  arangodb::velocypack::Builder links;
  IResearchViewMetaState metaState;

  {
    ReadMutex mutex(_mutex);
    SCOPED_LOCK(mutex); // '_collections' can be asynchronously modified

    links.openObject();

    for (auto& entry: _collections) {
      links.add(entry.second.first, entry.second.second.slice());
      metaState._collections.emplace(entry.first);
    }

    links.close();
  }

  metaState.json(builder); // FIXME TODO remove and fix JavaScript tests (no longer required)

  // links are not persisted, their definitions are part of the corresponding collections
  if (!forPersistence) {
    builder.add(StaticStrings::LinksField, links.slice());
  }

  builder.close(); // close PROPERTIES_FIELD

  return arangodb::Result();
}

bool IResearchViewCoordinator::emplace(
    TRI_voc_cid_t cid,
    std::string const& key,
    arangodb::velocypack::Slice const& value
) {
  static const std::function<bool(irs::string_ref const& key)> acceptor = [](
      irs::string_ref const& key
  )->bool {
    return key != arangodb::StaticStrings::IndexType
      && key != StaticStrings::ViewIdField; // ignored fields
  };
  arangodb::velocypack::Builder builder;

  builder.openObject();

  // strip internal keys (added in createLink(...)) from externally visible link definition
  if (!mergeSliceSkipKeys(builder, value, acceptor)) {
    LOG_TOPIC(WARN, iresearch::TOPIC)
      << "failed to generate externally visible link definition while emplacing link definition into IResearch view '"
      << name() << "' collection '" << cid << "'";

    return false;
  }

  builder.close();

  WriteMutex mutex(_mutex);
  SCOPED_LOCK(mutex); // '_collections' can be asynchronously read

  return _collections.emplace(
    std::piecewise_construct,
    std::forward_as_tuple(cid),
    std::forward_as_tuple(key, std::move(builder))
  ).second;
}

/*static*/ std::shared_ptr<LogicalView> IResearchViewCoordinator::make(
    TRI_vocbase_t& vocbase,
    velocypack::Slice const& info,
    bool isNew,
    uint64_t planVersion,
    LogicalView::PreCommitCallback const& preCommit
) {
  auto view = std::shared_ptr<IResearchViewCoordinator>(
    new IResearchViewCoordinator(vocbase, info, planVersion)
  );
  auto& json = info.isObject() ? info : emptyObjectSlice(); // if no 'info' then assume defaults
  auto props = json.get(StaticStrings::PropertiesField);
  auto& properties = props.isObject() ? props : emptyObjectSlice(); // if no 'props' then assume defaults
  std::string error;

  if (!view->_meta.init(properties, error)) {
    LOG_TOPIC(WARN, iresearch::TOPIC)
        << "failed to initialize IResearch view from definition, error: " << error;

    return nullptr;
  }

  if (preCommit && !preCommit(view)) {
    LOG_TOPIC(ERR, arangodb::iresearch::TOPIC)
      << "Failure during pre-commit while constructing IResearch View in database '" << vocbase.id() << "'";

    return nullptr;
  }

  if (isNew) {
    arangodb::velocypack::Builder builder;
    auto* ci = ClusterInfo::instance();

    if (!ci) {
      LOG_TOPIC(ERR, arangodb::iresearch::TOPIC)
        << "Failure to find ClusterInfo instance while constructing IResearch View in database '" << vocbase.id() << "'";

      return nullptr;
    }

    builder.openObject();

    auto res = view->toVelocyPack(builder, true, true); // include links so that Agency will always have a full definition

    if (!res.ok()) {
      LOG_TOPIC(ERR, arangodb::iresearch::TOPIC)
        << "Failure to generate definitionf created view while constructing IResearch View in database '" << vocbase.id() << "', error: " << res.errorMessage();

      return nullptr;
    }

    builder.close();

    auto resNum = ci->createViewCoordinator(
      vocbase.name(), std::to_string(view->id()), builder.slice(), error
    );

    if (TRI_ERROR_NO_ERROR != resNum) {
      LOG_TOPIC(ERR, arangodb::iresearch::TOPIC)
        << "Failure during commit of created view while constructing IResearch View in database '" << vocbase.id() << "', error: " << error;

      return nullptr;
    }
  }

  return view;
}

IResearchViewCoordinator::IResearchViewCoordinator(
    TRI_vocbase_t& vocbase,
    velocypack::Slice info,
    uint64_t planVersion
) : LogicalView(vocbase, info, planVersion) {
  TRI_ASSERT(ServerState::instance()->isCoordinator());
}

bool IResearchViewCoordinator::visitCollections(
    CollectionVisitor const& visitor
) const {
  ReadMutex mutex(_mutex);
  SCOPED_LOCK(mutex); // '_collections' can be asynchronously modified

  for (auto& entry: _collections) {
    if (!visitor(entry.first)) {
      return false;
    }
  }

  return true;
}

arangodb::Result IResearchViewCoordinator::updateProperties(
    velocypack::Slice const& properties,
    bool partialUpdate,
    bool /*doSync*/
) {
  try {
    IResearchViewMeta meta;
    std::string error;

    auto const& defaults = partialUpdate
      ? _meta
      : IResearchViewMeta::DEFAULT();

    if (!meta.init(properties, error, defaults)) {
      return { TRI_ERROR_BAD_PARAMETER, error };
    }

    // only trigger persisting of properties if they have changed
    if (_meta != meta) {
      auto* engine = arangodb::ClusterInfo::instance();

      if (!engine) {
        return arangodb::Result(
          TRI_ERROR_INTERNAL,
          std::string("failure to get storage engine while updating IResearch View '") + name() + "'"
        );
      }

      arangodb::velocypack::Builder builder;

      builder.openObject();
        builder.add(
          StaticStrings::PropertiesField,
          arangodb::velocypack::Value(arangodb::velocypack::ValueType::Object)
        );
          meta.json(builder);
        builder.close(); // close PROPERTIES_FIELD

        auto result = toVelocyPack(builder, false, true);

        if (!result.ok()) {
          return result;
        }

      builder.close();
      result = engine->setViewPropertiesCoordinator(
        vocbase().name(), std::to_string(id()), builder.slice()
      );

      if (!result.ok()) {
        return result;
      }
    }

    if (!properties.hasKey(StaticStrings::LinksField)) {
      return arangodb::Result(); // nothing more to do
    }

    // ...........................................................................
    // update links if requested (on a best-effort basis)
    // indexing of collections is done in different threads so no locks can be held and rollback is not possible
    // as a result it's also possible for links to be simultaneously modified via a different callflow (e.g. from collections)
    // ...........................................................................

    arangodb::velocypack::Builder currentLinks;
    std::unordered_set<TRI_voc_cid_t> currentCids;

    {
      ReadMutex mutex(_mutex);
      SCOPED_LOCK(mutex); // '_collections' can be asynchronously modified

      currentLinks.openObject();

      for (auto& entry: _collections) {
        currentCids.emplace(entry.first);
        currentLinks.add(entry.second.first, entry.second.second.slice());
      }

      currentLinks.close();
    }

    arangodb::velocypack::Builder viewNewProperties;
    bool modified = false;
    std::unordered_set<TRI_voc_cid_t> newCids;

    viewNewProperties.openObject();

    return updateLinks(
      properties.get(StaticStrings::LinksField),
      currentLinks.slice(),
      *this,
      partialUpdate,
      currentCids,
      modified,
      viewNewProperties,
      newCids
    );
  } catch (std::exception const& e) {
    LOG_TOPIC(WARN, iresearch::TOPIC)
      << "caught exception while updating properties for IResearch view '" << id() << "': " << e.what();
    IR_LOG_EXCEPTION();
    return arangodb::Result(
      TRI_ERROR_BAD_PARAMETER,
      std::string("error updating properties for IResearch view '") + StringUtils::itoa(id()) + "'"
    );
  } catch (...) {
    LOG_TOPIC(WARN, iresearch::TOPIC)
      << "caught exception while updating properties for IResearch view '" << id() << "'";
    IR_LOG_EXCEPTION();
    return arangodb::Result(
      TRI_ERROR_BAD_PARAMETER,
      std::string("error updating properties for IResearch view '") + StringUtils::itoa(id()) + "'"
    );
  }

  return {};
}

Result IResearchViewCoordinator::drop() {
  // drop links first
  {
    std::unordered_set<TRI_voc_cid_t> currentCids;

    visitCollections([&currentCids](TRI_voc_cid_t cid)->bool { currentCids.emplace(cid); return true; });

    arangodb::velocypack::Builder builder;
    bool modified;
    std::unordered_set<TRI_voc_cid_t> newCids;
    auto res = updateLinks(
      arangodb::velocypack::Slice::emptyObjectSlice(),
      arangodb::velocypack::Slice::emptyObjectSlice(),
      *this,
      false,
      currentCids,
      modified,
      builder,
      newCids
    );

    if (!res.ok()) {
      return res;
    }
  }

  // drop view then
  std::string errorMsg;

  int const res = ClusterInfo::instance()->dropViewCoordinator(
    vocbase().name(), // database name
    StringUtils::itoa(id()), // cluster-wide view id
    errorMsg
  );

  if (res != TRI_ERROR_NO_ERROR) {
    LOG_TOPIC(ERR, arangodb::Logger::CLUSTER)
      << "Could not drop view in agency, error: " << errorMsg
      << ", errorCode: " << res;

    return { res, errorMsg };
  }

  return {};
}

} // iresearch
} // arangodb

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------