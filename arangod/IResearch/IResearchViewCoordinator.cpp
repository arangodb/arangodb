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

  builder.openObject();
  if (!mergeSlice(builder, link)
      || !IResearchLinkHelper::setType(builder)
      || !IResearchLinkHelper::setView(builder, view.id())) {
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
    VPackSlice properties,
    bool partialUpdate,
    IResearchViewCoordinator const& view,
    VPackBuilder& viewProperties,
    std::unordered_set<TRI_voc_cid_t>& cids
) {
  auto links = properties.get(StaticStrings::LinksField);

  if (links.isNone()) {
    return {};
  }

  arangodb::CollectionNameResolver resolver(&view.vocbase());

  auto collections = partialUpdate
    ? cids
    : std::unordered_set<TRI_voc_cid_t>{};
  cids.clear();

  arangodb::Result res;
  std::string error;
  VPackBuilder builder;
  IResearchLinkMeta linkMeta;

  for (VPackObjectIterator linksItr(links); linksItr.valid(); ++linksItr) {
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
      if (existingLink && partialUpdate) {
        res = dropLink(*existingLink, *collection, builder);

        // do not need to drop link afterwards
        collections.erase(collection->id());
      }
    } else {
      if (!linkMeta.init(link, error)) {
        return { TRI_ERROR_BAD_PARAMETER, error } ;
      }

      // append link definition
      {
        VPackObjectBuilder const guard(&viewProperties, collectionNameOrId);
        linkMeta.json(viewProperties);
        cids.emplace(collection->id());
      }

      if (existingLink) {
        // do not need to drop link afterwards
        collections.erase(collection->id());

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
    }

    if (!res.ok()) {
      return res;
    }
  }

  // in case of full update drop unprocessed links
  for (auto const cid : collections) {
    auto const collection = resolver.getCollection(cid);

    if (!collection) {
      return { TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND };
    }

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

/*static*/ std::shared_ptr<LogicalView> IResearchViewCoordinator::make(
    TRI_vocbase_t& vocbase,
    velocypack::Slice const& info,
    uint64_t planVersion,
    LogicalView::PreCommitCallback const& /*preCommit*/
) {
  auto view = std::unique_ptr<IResearchViewCoordinator>(
    new IResearchViewCoordinator(vocbase, info, planVersion)
  );

  std::string error;

  auto properties = info.get(StaticStrings::PropertiesField);

  if (!properties.isObject()) {
    // set to defaults
    properties = VPackSlice::emptyObjectSlice();
  }

  auto& meta = view->_meta;

  if (!meta.init(properties, error)) {
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
  for (auto& cid: _meta._collections) {
    if (!visitor(cid)) {
      return false;
    }
  }

  return true;
}

void IResearchViewCoordinator::toVelocyPack(
    velocypack::Builder& result,
    bool includeProperties,
    bool includeSystem
) const {
  // We write into an open object
  TRI_ASSERT(result.isOpenObject());

  // Meta Information
  result.add("id", VPackValue(StringUtils::itoa(id())));
  result.add("name", VPackValue(name()));
  result.add("type", VPackValue(type().name()));

  if (includeSystem) {
    result.add("deleted", VPackValue(deleted()));
    result.add("planId", VPackValue(std::to_string(planId())));
  }

  if (includeProperties) {
    VPackObjectBuilder guard(&result, StaticStrings::PropertiesField);

    // regular properites
    _meta.json(result);

    // view links
    auto const links = _links.slice();

    if (links.isObject() && links.length()) {
      VPackObjectBuilder guard(&result, StaticStrings::LinksField);
      mergeSlice(result, links);
    }
  }

  TRI_ASSERT(result.isOpenObject()); // We leave the object open
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

          auto const res = updateLinks(
            properties,
            partialUpdate,
            *this,
            builder,
            meta._collections
          );

          if (!res.ok()) {
            return res;
          }
        }

        // meta
        meta.json(builder);
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
  auto cid_itr = _meta._collections.find(cid);

  if (cid_itr == _meta._collections.end()) {
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

