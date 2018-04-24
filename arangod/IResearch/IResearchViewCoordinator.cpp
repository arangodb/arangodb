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

arangodb::Result dropLink(
    arangodb::iresearch::IResearchLinkCoordinator const& link,
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
    arangodb::iresearch::IResearchViewCoordinator const& view,
    VPackSlice link,
    VPackBuilder& builder
) {
  TRI_ASSERT(builder.isEmpty());

  builder.openObject();
  if (!arangodb::iresearch::mergeSlice(builder, link)
      || !arangodb::iresearch::IResearchLinkHelper::setType(builder)
      || !arangodb::iresearch::IResearchLinkHelper::setView(builder, view.id())) {
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

  auto const links = info.get(StaticStrings::LinksField);

  if (links.isObject()) {
    auto& builder = view->_links;

    size_t idx = 0;
    for (auto link : velocypack::ObjectIterator(links)) {
      auto name = link.key;

      if (!name.isString()) {
        LOG_TOPIC(WARN, iresearch::TOPIC)
            << "failed to initialize IResearch view link from definition at index " << idx
            << ", link name is not string";

        return nullptr;
      }

      builder.add(name);
      builder.openObject();
      auto const res = IResearchLinkHelper::normalize(builder, link.value, false);

      if (!res.ok()) {
        LOG_TOPIC(WARN, iresearch::TOPIC)
            << "failed to initialize IResearch view link from definition at index " << idx
            << ", error: " << error;

        return nullptr;
      }
      builder.close();
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
    result.add(StaticStrings::PropertiesField, VPackValue(VPackValueType::Object)); // properties: {

    // regular properites
    _meta.json(result);

    // view links
    auto const links = _links.slice();
    if (links.isObject()) {
      mergeSlice(result, links);
    }

    result.close(); // }
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

    if (meta != _meta) {
      VPackBuilder builder;
      builder.openObject(); // {
      toVelocyPack(builder, false, true); // only system properties
      builder.add(StaticStrings::PropertiesField, VPackValue(VPackValueType::Object)); // "properties" : {
      meta.json(builder);
      builder.close(); // }
      builder.close(); // }

      auto res = ClusterInfo::instance()->setViewPropertiesCoordinator(
        vocbase().name(), // database name,
        StringUtils::itoa(id()), // cluster-wide view id
        builder.slice()
      );

      if (!res.ok()) {
        return res;
      }
    }

    return updateLinks(properties, partialUpdate);
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

arangodb::Result IResearchViewCoordinator::updateLinks(
    VPackSlice properties, bool partialUpdate
) {
  auto links = properties.get(StaticStrings::LinksField);

  if (links.isNone()) {
    return {};
  }

  CollectionNameResolver resolver(&vocbase());

  auto collections = partialUpdate
    ? _meta._collections
    : decltype(_meta._collections){};

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
          + StringUtils::itoa(id())
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

    auto existingLink = IResearchLinkCoordinator::find(*collection, *this);

    if (link.isNull() && existingLink && partialUpdate) {
      res = dropLink(*existingLink, *collection, builder);
    } else {
      if (existingLink) {
        if (!linkMeta.init(link, error)) {
          return { TRI_ERROR_BAD_PARAMETER, error } ;
        }

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

      res = createLink(*collection, *this, link, builder);
    }

    if (!res.ok()) {
      return res;
    }
  }

  // in case of full update drop unprocessed links
  for (auto const cid : collections) {
    auto collection = resolver.getCollection(cid);

    if (!collection) {
      return { TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND };
    }

    auto link = IResearchLinkCoordinator::find(*collection, *this);

    if (!link) {
      return {
        TRI_ERROR_ARANGO_INDEX_NOT_FOUND,
        std::string("no link between view '") + std::to_string(id()) +
        "' and collection '" + std::to_string(cid) + "' found"
      };
    }

    res = dropLink(*link, *collection, builder);

    if (!res.ok()) {
      return res;
    }
  }

  return res;
}

Result IResearchViewCoordinator::drop(TRI_voc_cid_t cid) {
  auto cid_itr = _meta._collections.find(cid);

  if (cid_itr == _meta._collections.end()) {
    // no such cid
    return { TRI_ERROR_BAD_PARAMETER };
  }

  VPackBuilder builder;
  builder.openObject();
  builder.add(StaticStrings::LinksField, VPackValue(VPackValueType::Object));
  builder.add(StringUtils::itoa(cid), VPackSlice::nullSlice());
  builder.close();
  builder.close();

  return updateLinks(builder.slice(), true);
}

} // iresearch
} // arangodb

