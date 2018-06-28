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

using namespace arangodb::basics;
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
          + StringUtils::itoa(view.id())
          + "' offset '"
          + StringUtils::itoa(linksItr.index()) + '"'
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
            + StringUtils::itoa(view.id())
            + "' and collection '" + StringUtils::itoa(cid)
            + "' found"
        };
      }

      builder.clear();
      res = dropLink(*link, *collection, builder);
      modified = true;
    } else {
      auto link = currentLinks.get(collection->name());

      if (!link.isObject()) {
        auto const collectiondId = StringUtils::itoa(collection->id());
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
  bool //forPersistence
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
  _metaState.json(builder); // regular properites

  auto links = _links.slice();

  if (links.isObject() && links.length()) {
    builder.add(StaticStrings::LinksField, links);
  }

  builder.close(); // close PROPERTIES_FIELD

  return arangodb::Result();
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

  if (!view->_meta.init(properties, error)
      || !view->_metaState.init(properties, error)) {
    LOG_TOPIC(WARN, iresearch::TOPIC)
        << "failed to initialize IResearch view from definition, error: " << error;

    return nullptr;
  }

  auto const links = properties.get(StaticStrings::LinksField);
  IResearchLinkMeta linkMeta;

  if (links.isObject()) {
    auto& builder = view->_links;
    VPackObjectBuilder guard(&builder);

    size_t idx = 0;
    for (auto link : velocypack::ObjectIterator(links)) {
      auto const nameSlice = link.key;

      if (!nameSlice.isString()) {
        LOG_TOPIC(WARN, iresearch::TOPIC)
            << "failed to initialize IResearch view link from definition at index " << idx
            << ", link name is not string";

        return nullptr;
      }

      if (!linkMeta.init(link.value, error)) {
        LOG_TOPIC(WARN, iresearch::TOPIC)
            << "failed to initialize IResearch view link from definition at index " << idx
            << ", error: " << error;

        return nullptr;
      }

      // append normalized link definition
      {
        VPackValueLength length;
        char const* name = nameSlice.getString(length);

        builder.add(name, length, VPackValue(VPackValueType::Object));
        linkMeta.json(builder);
        builder.close();
      }
    }
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
  for (auto& cid: _metaState._collections) {
    if (!visitor(cid)) {
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
    IResearchViewMetaState metaState;
    std::string error;

    auto const& defaults = partialUpdate
      ? _meta
      : IResearchViewMeta::DEFAULT();

    if (!meta.init(properties, error, defaults)) {
      return { TRI_ERROR_BAD_PARAMETER, error };
    }

    VPackBuilder builder;
    {
      VPackObjectBuilder const guard(&builder);

      // system properties
      toVelocyPack(builder, false, true);

      // properties
      {
        VPackObjectBuilder const guard(&builder, StaticStrings::PropertiesField);

        // links
        {
          VPackObjectBuilder const guard(&builder, StaticStrings::LinksField);

          bool modified = false;
          auto const res = updateLinks(
            properties.get(StaticStrings::LinksField),
            _links.slice(),
            *this,
            partialUpdate,
            _metaState._collections,
            modified,
            builder,
            metaState._collections
          );

          if (!res.ok()) {
            return res;
          }

          if (!modified && _meta == meta && _metaState == metaState) {
            // nothing to do
            return {};
          }
        }

        // meta
        meta.json(builder);
        metaState.json(builder);
      }
    }

    return ClusterInfo::instance()->setViewPropertiesCoordinator(
      vocbase().name(), // database name,
      StringUtils::itoa(id()), // cluster-wide view id
      builder.slice()
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
    auto const res = updateProperties(
      VPackSlice::emptyObjectSlice(), false, true
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

Result IResearchViewCoordinator::drop(TRI_voc_cid_t cid) {
  auto cid_itr = _metaState._collections.find(cid);

  if (cid_itr == _metaState._collections.end()) {
    // no such cid
    return { TRI_ERROR_BAD_PARAMETER };
  }

  VPackBuilder builder;
  {
    VPackObjectBuilder const guard(&builder);
    {
      VPackObjectBuilder const guard(&builder, StaticStrings::LinksField);
      builder.add(StringUtils::itoa(cid), VPackSlice::nullSlice());
    }
  }

  return updateProperties(builder.slice(), true, true);
}

} // iresearch
} // arangodb

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------