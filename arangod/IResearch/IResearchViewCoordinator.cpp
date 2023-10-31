////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
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
#include "IResearchLinkHelper.h"
#include "IResearchLinkCoordinator.h"

#include <velocypack/Builder.h>
#include <velocypack/Collection.h>
#include <velocypack/Parser.h>

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/StaticStrings.h"
#include "Cluster/ClusterFeature.h"
#include "Cluster/ClusterInfo.h"
#include "Cluster/ClusterMethods.h"
#include "Cluster/ServerState.h"
#include "IResearch/IResearchFeature.h"
#include "IResearch/IResearchLink.h"
#include "IResearch/VelocyPackHelper.h"
#include "Logger/LogMacros.h"
#include "RestServer/ViewTypesFeature.h"
#include "Transaction/Methods.h"
#include "Transaction/StandaloneContext.h"
#include "Utils/ExecContext.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/Methods/Indexes.h"

#include <absl/strings/str_cat.h>

namespace arangodb::iresearch {
namespace {

bool equalPartial(IResearchViewMeta const& lhs, IResearchViewMeta const& rhs) {
  if (lhs._cleanupIntervalStep != rhs._cleanupIntervalStep ||
      lhs._commitIntervalMsec != rhs._commitIntervalMsec ||
      lhs._consolidationIntervalMsec != rhs._consolidationIntervalMsec) {
    return false;
  }
  try {
    if (!basics::VelocyPackHelper::equal(lhs._consolidationPolicy.properties(),
                                         rhs._consolidationPolicy.properties(),
                                         false)) {
      return false;
    }
  } catch (...) {
    return false;
  }
  return true;
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
/// @brief IResearchView-specific implementation of a ViewFactory
////////////////////////////////////////////////////////////////////////////////
struct IResearchViewCoordinator::ViewFactory final : arangodb::ViewFactory {
  Result create(LogicalView::ptr& view, TRI_vocbase_t& vocbase,
                VPackSlice definition, bool isUserRequest) const final {
    auto& server = vocbase.server();
    if (!server.hasFeature<ClusterFeature>()) {
      return {TRI_ERROR_INTERNAL,
              absl::StrCat("failure to find 'ClusterInfo' instance while "
                           "creating arangosearch View in database '",
                           vocbase.name(), "'")};
    }
    auto& ci = server.getFeature<ClusterFeature>().clusterInfo();
    auto properties = definition.isObject()
                          ? definition
                          : velocypack::Slice::emptyObjectSlice();
    auto links = properties.get(StaticStrings::LinksField);
    if (links.isNone()) {
      links = velocypack::Slice::emptyObjectSlice();
    }
    auto r = IResearchLinkHelper::validateLinks(vocbase, links);
    if (!r.ok()) {
      return r;
    }
    LogicalView::ptr impl;
    r = cluster_helper::construct(impl, vocbase, definition, isUserRequest);
    if (!r.ok()) {
      return r;
    }
    TRI_ASSERT(impl);
    // create links on a best-effort basis
    // link creation failure does not cause view creation failure
    try {
      containers::FlatHashSet<DataSourceId> collections;
      r = IResearchLinkHelper::updateLinks(collections, *impl, links,
                                           getDefaultVersion(isUserRequest));
      if (!r.ok()) {
        LOG_TOPIC("39d88", WARN, iresearch::TOPIC)
            << "failed to create links while creating arangosearch view '"
            << impl->name() << "': " << r.errorNumber() << " "
            << r.errorMessage();
        // TODO return if error?
      }
    } catch (basics::Exception const& e) {
      LOG_TOPIC("09bb9", WARN, iresearch::TOPIC)
          << "caught exception while creating links while creating "
             "arangosearch view '"
          << impl->name() << "': " << e.code() << " " << e.message();
    } catch (std::exception const& e) {
      LOG_TOPIC("6b99b", WARN, iresearch::TOPIC)
          << "caught exception while creating links while creating "
             "arangosearch view '"
          << impl->name() << "': " << e.what();
    } catch (...) {
      LOG_TOPIC("61ae6", WARN, iresearch::TOPIC)
          << "caught exception while creating links while creating "
             "arangosearch view '"
          << impl->name() << "'";
    }
    // refresh view from Agency to get latest state with populated collections
    view = ci.getView(vocbase.name(), absl::AlphaNum{impl->id().id()}.Piece());
    // view might be already dropped
    if (view) {
      // open view to match the behavior in StorageEngine::openExistingDatabase
      // and original behavior of TRI_vocbase_t::createView
      view->open();
    } else {
      return {TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND,
              absl::StrCat("ArangoSearch view '", impl->name(),
                           "' was dropped during creation "
                           "from database '" +
                               vocbase.name() + "'")};
    }
    return {};
  }

  Result instantiate(LogicalView::ptr& view, TRI_vocbase_t& vocbase,
                     velocypack::Slice definition,
                     bool /*isUserRequest*/) const final {
    std::string error;
    // TODO make_shared instead of new
    auto impl = std::shared_ptr<IResearchViewCoordinator>(
        new IResearchViewCoordinator(vocbase, definition));
    if (!impl->_meta.init(definition, error)) {
      return {TRI_ERROR_BAD_PARAMETER,
              absl::StrCat(
                  "failed to initialize arangosearch View '", impl->name(),
                  (error.empty()
                       ? "' from definition: "
                       : absl::StrCat("' from definition, error in attribute '",
                                      error, "': ")),
                  definition.toString())};
    }
    view = impl;
    return {};
  }
};

Result IResearchViewCoordinator::appendVPackImpl(VPackBuilder& build,
                                                 Serialization ctx,
                                                 bool safe) const {
  if (ctx == Serialization::List) {
    return {};  // nothing more to output
  }
  if (ctx == Serialization::Properties || ctx == Serialization::Inventory) {
    std::shared_lock lock{_mutex};
    // verify that the current user has access on all linked collections
    ExecContext const& exec = ExecContext::current();
    if (!exec.isSuperuser()) {
      for (auto& entry : _collections) {
        if (!exec.canUseCollection(vocbase().name(),
                                   entry.second->collectionName,
                                   auth::Level::RO)) {
          return {TRI_ERROR_FORBIDDEN};
        }
      }
    }
    VPackBuilder tmp;
    build.add(StaticStrings::LinksField, VPackValue(VPackValueType::Object));
    auto const accept = [](std::string_view key) {
      return key != iresearch::StaticStrings::AnalyzerDefinitionsField &&
#ifdef USE_ENTERPRISE
             key != iresearch::StaticStrings::kOptimizeTopKField &&
             key != iresearch::StaticStrings::kPrimarySortCacheField &&
             key != iresearch::StaticStrings::kCachePrimaryKeyField &&
#endif
             key != iresearch::StaticStrings::PrimarySortField &&
             key != iresearch::StaticStrings::PrimarySortCompressionField &&
             key != iresearch::StaticStrings::StoredValuesField &&
             key != iresearch::StaticStrings::VersionField &&
             key != iresearch::StaticStrings::CollectionNameField;
    };
    for (auto& entry : _collections) {
      auto linkSlice = entry.second->linkDefinition.slice();
      if (ctx == Serialization::Properties) {
        tmp.clear();
        tmp.openObject();
        if (!mergeSliceSkipKeys(tmp, linkSlice, accept)) {
          return {TRI_ERROR_INTERNAL,
                  absl::StrCat("failed to generate externally visible link "
                               "definition for arangosearch View '",
                               name(), "'")};
        }
        linkSlice = tmp.close().slice();
      }
      build.add(entry.second->collectionName, linkSlice);
    }
    build.close();
  }
  if (!build.isOpenObject()) {
    return {TRI_ERROR_BAD_PARAMETER,
            "invalid builder provided for "
            "IResearchViewCoordinator definition"};
  }
  VPackBuilder sanitizedBuilder;
  sanitizedBuilder.openObject();
  IResearchViewMeta::Mask mask(true);
  auto const propertiesAccept = +[](std::string_view key) -> bool {
    return key != StaticStrings::VersionField;
  };
  auto const persistenceAccept = +[](std::string_view) -> bool {
    return true;  // for breakpoint
  };
  auto* acceptor = (ctx == Serialization::Persistence ||
                    ctx == Serialization::PersistenceWithInProgress)
                       ? persistenceAccept
                       : propertiesAccept;
  if (!_meta.json(sanitizedBuilder, nullptr, &mask) ||
      !mergeSliceSkipKeys(build, sanitizedBuilder.close().slice(), *acceptor)) {
    return {TRI_ERROR_INTERNAL,
            absl::StrCat("failure to generate definition while generating "
                         "properties jSON for IResearch View in database '",
                         vocbase().name(), "'")};
  }
  return {};
}

ViewFactory const& IResearchViewCoordinator::factory() {
  static ViewFactory const factory;
  return factory;
}

Result IResearchViewCoordinator::link(IResearchLinkCoordinator const& link) {
  TRI_IF_FAILURE("IResearchLink::alwaysDangling") { return {}; }
  auto& collection = link.collection();
  auto const& cname = collection.name();
  if (!ClusterMethods::includeHiddenCollectionInLink(cname)) {
    return {TRI_ERROR_NO_ERROR};
  }
  velocypack::Builder builder;
  builder.openObject();
  // generate user-visible definition, agency will not see links
  auto r = link.properties(builder, true);
  if (!r.ok()) {
    return r;
  }
  builder.close();
  auto const acceptor = [](std::string_view key) -> bool {
    return key != arangodb::StaticStrings::IndexId &&
           key != arangodb::StaticStrings::IndexType &&
           key != StaticStrings::ViewIdField;
  };
  auto cid = collection.id();
  velocypack::Builder sanitizedBuild;
  sanitizedBuild.openObject();
  // strip internal keys (added in IResearchLink::properties(...))
  // from externally visible link definition
  if (!mergeSliceSkipKeys(sanitizedBuild, builder.slice(), acceptor)) {
    return {TRI_ERROR_INTERNAL,
            absl::StrCat(
                "failed to generate externally visible link definition while "
                "emplace to collection '",
                cid.id(), "' into arangosearch View '", name(), "'")};
  }
  sanitizedBuild.close();
  std::lock_guard lock{_mutex};
  auto entry = _collections.try_emplace(
      cid, std::make_unique<Data>(cname, std::move(sanitizedBuild),
                                  link.isBuilding()));
  if (!entry.second) {
    return {TRI_ERROR_ARANGO_DUPLICATE_IDENTIFIER,
            absl::StrCat("duplicate entry while emplacing collection '",
                         cid.id(), "' into arangosearch View '", name(), "'")};
  }
  return {};
}

Result IResearchViewCoordinator::renameImpl(std::string const&) {
  TRI_ASSERT(false);
  return {TRI_ERROR_CLUSTER_UNSUPPORTED};
}

Result IResearchViewCoordinator::unlink(DataSourceId) noexcept {
  return {};  // for breakpoint
}

IResearchViewCoordinator::IResearchViewCoordinator(TRI_vocbase_t& vocbase,
                                                   velocypack::Slice info)
    : LogicalView(*this, vocbase, info) {
  TRI_ASSERT(ServerState::instance()->isCoordinator());
}

bool IResearchViewCoordinator::visitCollections(
    CollectionVisitor const& visitor) const {
  std::shared_lock lock{_mutex};
  for (auto& entry : _collections) {
    if (!visitor(entry.first, nullptr)) {
      return false;
    }
  }
  return true;
}

bool IResearchViewCoordinator::isBuilding() const {
  std::shared_lock lock{_mutex};
  for (auto& entry : _collections) {
    if (entry.second->isBuilding) {
      return true;
    }
  }
  return false;
}

Result IResearchViewCoordinator::properties(velocypack::Slice slice,
                                            bool isUserRequest,
                                            bool partialUpdate) {
  auto& server = vocbase().server();
  if (!server.hasFeature<ClusterFeature>()) {
    return {
        TRI_ERROR_INTERNAL,
        absl::StrCat(
            "failure to get storage engine while updating arangosearch view '",
            name(), "'")};
  }
  auto& engine = server.getFeature<ClusterFeature>().clusterInfo();
  try {
    auto links = slice.get(StaticStrings::LinksField);
    if (links.isNone()) {
      links = velocypack::Slice::emptyObjectSlice();
    }
    auto r = IResearchLinkHelper::validateLinks(vocbase(), links);
    if (!r.ok()) {
      return r;
    }
    // check link auth as per https://github.com/arangodb/backlog/issues/459
    auto const& exec = ExecContext::current();
    if (!exec.isSuperuser()) {  // check existing links
      std::shared_lock lock{_mutex};
      for (auto& entry : _collections) {
        auto const& name = vocbase().name();
        auto collection = engine.getCollection(
            name, absl::AlphaNum{entry.first.id()}.Piece());
        if (collection &&
            !exec.canUseCollection(name, collection->name(), auth::Level::RO)) {
          return {
              TRI_ERROR_FORBIDDEN,
              absl::StrCat(
                  "while updating arangosearch definition, error: collection '",
                  collection->name(), "' not authorized for read access")};
        }
      }
    }
    // TODO read necessary members from slice in single call
    std::string error;
    IResearchViewMeta meta;
    auto const& defaults = partialUpdate ? _meta : IResearchViewMeta::DEFAULT();
    if (!meta.init(slice, error, defaults)) {
      return {TRI_ERROR_BAD_PARAMETER,
              absl::StrCat(
                  "failed to update arangosearch view '", name(),
                  (error.empty()
                       ? "' from definition: "
                       : absl::StrCat("' from definition, error in attribute '",
                                      error, "': ")),
                  slice.toString())};
    }
    // only trigger persisting of properties if they have changed
    if (!equalPartial(_meta, meta)) {
      IResearchViewMeta oldMeta{IResearchViewMeta::PartialTag{},
                                std::move(_meta)};
      _meta.storePartial(std::move(meta));  // update meta for persistence
      r = cluster_helper::properties(*this, false);
      _meta.storePartial(std::move(oldMeta));  // restore meta
      if (!r.ok()) {
        return r;
      }
    }
    if (links.isEmptyObject() && partialUpdate) {
      return {};
    }
    // .........................................................................
    // update links if requested (on a best-effort basis) indexing of
    // collections is done in different threads so no locks can be held and
    // rollback is not possible as a result it's also possible for links to be
    // simultaneously modified via a different call-flow (e.g. from collections)
    // .........................................................................
    containers::FlatHashSet<DataSourceId> currentCids;
    if (!partialUpdate) {
      std::shared_lock lock{_mutex};
      for (auto& entry : _collections) {
        currentCids.emplace(entry.first);
      }
    }
    containers::FlatHashSet<DataSourceId> collections;
    return IResearchLinkHelper::updateLinks(collections, *this, links,
                                            getDefaultVersion(isUserRequest),
                                            currentCids);
  } catch (basics::Exception const& e) {
    LOG_TOPIC("714b3", WARN, iresearch::TOPIC)
        << "caught exception while updating properties for arangosearch view '"
        << name() << "': " << e.code() << " " << e.message();
    return {e.code(),
            absl::StrCat("error updating properties for arangosearch view '",
                         name(), "': ", e.message())};
  } catch (std::exception const& e) {
    LOG_TOPIC("86a5c", WARN, iresearch::TOPIC)
        << "caught exception while updating properties for arangosearch view '"
        << name() << "': " << e.what();
    return {TRI_ERROR_BAD_PARAMETER,
            absl::StrCat("error updating properties for arangosearch view '",
                         name(), "': ", e.what())};
  } catch (...) {
    LOG_TOPIC("17b66", WARN, iresearch::TOPIC)
        << "caught exception while updating properties for arangosearch view '"
        << name() << "'";
    return {TRI_ERROR_BAD_PARAMETER,
            absl::StrCat("error updating properties for arangosearch view '",
                         name(), "'")};
  }
}

Result IResearchViewCoordinator::dropImpl() {
  auto& server = vocbase().server();
  if (!server.hasFeature<ClusterFeature>()) {
    return {
        TRI_ERROR_INTERNAL,
        absl::StrCat(
            "failure to get storage engine while dropping arangosearch view '",
            name(), "'")};
  }
  auto& engine = server.getFeature<ClusterFeature>().clusterInfo();
  // drop links first
  containers::FlatHashSet<DataSourceId> currentCids;
  visitCollections([&](DataSourceId cid, LogicalView::Indexes*) {
    currentCids.emplace(cid);
    return true;
  });
  // check link auth as per https://github.com/arangodb/backlog/issues/459
  ExecContext const& exec = ExecContext::current();
  if (!exec.isSuperuser()) {
    for (auto& entry : currentCids) {
      auto const& name = vocbase().name();
      auto collection =
          engine.getCollection(name, absl::AlphaNum{entry.id()}.Piece());
      if (collection &&
          !exec.canUseCollection(name, collection->name(), auth::Level::RO)) {
        return {TRI_ERROR_FORBIDDEN};
      }
    }
  }
  containers::FlatHashSet<DataSourceId> collections;
  auto r = IResearchLinkHelper::updateLinks(
      collections, *this, velocypack::Slice::emptyObjectSlice(),
      LinkVersion::MAX,
      // we don't care of link version due to removal only request
      currentCids);
  if (!r.ok()) {
    return {r.errorNumber(),
            absl::StrCat(
                "failed to remove links while removing arangosearch view '",
                name(), "': ", r.errorMessage())};
  }
  return cluster_helper::drop(*this);
}

}  // namespace arangodb::iresearch
