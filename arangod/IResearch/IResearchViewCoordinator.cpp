////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
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

#include <velocypack/Builder.h>
#include <velocypack/Collection.h>
#include <velocypack/Parser.h>

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/StaticStrings.h"
#include "Basics/StringUtils.h"
#include "Cluster/ClusterFeature.h"
#include "Cluster/ClusterInfo.h"
#include "Cluster/ClusterMethods.h"
#include "Cluster/ServerState.h"
#include "IResearch/IResearchFeature.h"
#include "IResearch/IResearchLink.h"
#include "IResearch/VelocyPackHelper.h"
#include "Transaction/Methods.h"
#include "Transaction/StandaloneContext.h"
#include "Utils/ExecContext.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/Methods/Indexes.h"

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
struct IResearchViewCoordinator::ViewFactory : public arangodb::ViewFactory {
  virtual Result create(LogicalView::ptr& view, TRI_vocbase_t& vocbase,
                        VPackSlice definition,
                        bool isUserRequest) const override {
    if (!vocbase.server().hasFeature<ClusterFeature>()) {
      return Result(
          TRI_ERROR_INTERNAL,
          std::string("failure to find 'ClusterInfo' instance while creating "
                      "arangosearch View in database '") +
              vocbase.name() + "'");
    }
    auto& ci = vocbase.server().getFeature<ClusterFeature>().clusterInfo();

    auto properties =
        definition.isObject()
            ? definition
            : velocypack::Slice::emptyObjectSlice();  // if no 'info' then
                                                      // assume defaults
    auto links = properties.hasKey(StaticStrings::LinksField)
                     ? properties.get(StaticStrings::LinksField)
                     : velocypack::Slice::emptyObjectSlice();
    auto res = IResearchLinkHelper::validateLinks(vocbase, links);

    if (!res.ok()) {
      return res;
    }

    LogicalView::ptr impl;

    res = LogicalViewHelperClusterInfo::construct(impl, vocbase, definition);

    if (!res.ok()) {
      return res;
    }

    // create links on a best-effor basis
    // link creation failure does not cause view creation failure
    try {
      std::unordered_set<DataSourceId> collections;

      res = IResearchLinkHelper::updateLinks(collections, *impl, links,
                                             getDefaultVersion(isUserRequest));

      if (!res.ok()) {
        LOG_TOPIC("39d88", WARN, iresearch::TOPIC)
            << "failed to create links while creating arangosearch view '"
            << impl->name() << "': " << res.errorNumber() << " "
            << res.errorMessage();
      }
    } catch (basics::Exception const& e) {
      LOG_TOPIC("09bb9", WARN, iresearch::TOPIC)
          << "caught exception while creating links while creating "
             "arangosearch view '"
          << impl->name() << "': " << e.code() << " " << e.what();
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

    view = ci.getView(
        vocbase.name(),
        std::to_string(impl->id().id()));  // refresh view from Agency

    if (view) {
      view->open();  // open view to match the behavior in
                     // StorageEngine::openExistingDatabase(...) and original
                     // behavior of TRI_vocbase_t::createView(...)
    }

    return Result();
  }

  virtual Result instantiate(LogicalView::ptr& view, TRI_vocbase_t& vocbase,
                             VPackSlice definition) const override {
    std::string error;
    auto impl = std::shared_ptr<IResearchViewCoordinator>(
        new IResearchViewCoordinator(vocbase, definition));

    if (!impl->_meta.init(definition, error)) {
      return Result(
          TRI_ERROR_BAD_PARAMETER,
          error.empty()
              ? (std::string("failed to initialize arangosearch View '") +
                 impl->name() + "' from definition: " + definition.toString())
              : (std::string("failed to initialize arangosearch View '") +
                 impl->name() + "' from definition, error in attribute '" +
                 error + "': " + definition.toString()));
    }

    view = impl;

    return Result();
  }
};

Result IResearchViewCoordinator::appendVelocyPackImpl(
    velocypack::Builder& builder, Serialization context) const {
  if (Serialization::List == context) {
    // nothing more to output
    return {};
  }

  std::function<bool(irs::string_ref)> const propertiesAcceptor =
      [](irs::string_ref key) -> bool {
    return std::string_view{key} != StaticStrings::VersionField;
  };
  std::function<bool(irs::string_ref)> const persistenceAcceptor =
      [](irs::string_ref) -> bool { return true; };

  const std::function<bool(irs::string_ref)> linkPropertiesAcceptor =
      [](std::string_view key) -> bool {
    return key != iresearch::StaticStrings::AnalyzerDefinitionsField &&
           key != iresearch::StaticStrings::PrimarySortField &&
           key != iresearch::StaticStrings::PrimarySortCompressionField &&
           key != iresearch::StaticStrings::StoredValuesField &&
           key != iresearch::StaticStrings::VersionField &&
           key != iresearch::StaticStrings::CollectionNameField;
  };

  auto* acceptor = &propertiesAcceptor;

  if (context == Serialization::Persistence ||
      context == Serialization::PersistenceWithInProgress) {
    auto res = LogicalViewHelperClusterInfo::properties(builder, *this);

    if (!res.ok()) {
      return res;
    }

    acceptor = &persistenceAcceptor;
  }

  if (context == Serialization::Properties ||
      context == Serialization::Inventory) {
    // verify that the current user has access on all linked collections
    ExecContext const& exec = ExecContext::current();
    if (!exec.isSuperuser()) {
      for (auto& entry : _collections) {  // TODO Data race?
        if (!exec.canUseCollection(vocbase().name(), entry.second.first,
                                   auth::Level::RO)) {
          return Result(TRI_ERROR_FORBIDDEN);
        }
      }
    }

    VPackBuilder tmp;

    // '_collections' can be asynchronously modified
    auto lock = irs::make_shared_lock(_mutex);

    builder.add(StaticStrings::LinksField, VPackValue(VPackValueType::Object));
    for (auto& entry : _collections) {
      auto linkSlice = entry.second.second.slice();

      if (context == Serialization::Properties) {
        tmp.clear();
        tmp.openObject();
        if (!mergeSliceSkipKeys(tmp, linkSlice, linkPropertiesAcceptor)) {
          return {TRI_ERROR_INTERNAL,
                  "failed to generate externally visible link definition for "
                  "arangosearch View '" +
                      name() + "'"};
        }

        linkSlice = tmp.close().slice();
      }

      builder.add(entry.second.first, linkSlice);
    }
    builder.close();
  }

  if (!builder.isOpenObject()) {
    return {TRI_ERROR_BAD_PARAMETER,
            "invalid builder provided for "
            "IResearchViewCoordinator definition"};
  }

  VPackBuilder sanitizedBuilder;
  sanitizedBuilder.openObject();
  IResearchViewMeta::Mask mask(true);
  if (!_meta.json(sanitizedBuilder, nullptr, &mask) ||
      !mergeSliceSkipKeys(builder, sanitizedBuilder.close().slice(),
                          *acceptor)) {
    return {TRI_ERROR_INTERNAL,
            std::string("failure to generate definition while generating "
                        "properties jSON for IResearch View in database '")
                .append(vocbase().name())
                .append("'")};
  }

  return {};
}

/*static*/ ViewFactory const& IResearchViewCoordinator::factory() {
  static const ViewFactory factory;

  return factory;
}

Result IResearchViewCoordinator::link(IResearchLink const& link) {
  if (!ClusterMethods::includeHiddenCollectionInLink(
          link.collection().name())) {
    return TRI_ERROR_NO_ERROR;
  }
  std::function<bool(irs::string_ref key)> const acceptor =
      [](std::string_view key) -> bool {
    return key != arangodb::StaticStrings::IndexId &&
           key != arangodb::StaticStrings::IndexType &&
           key != StaticStrings::ViewIdField;
  };
  velocypack::Builder builder;

  builder.openObject();

  // generate user-visible definition, agency will not see links
  auto res = link.properties(builder, true);

  if (!res.ok()) {
    return res;
  }

  builder.close();

  auto cid = link.collection().id();
  velocypack::Builder sanitizedBuilder;

  sanitizedBuilder.openObject();

  // strip internal keys (added in IResearchLink::properties(...)) from
  // externally visible link definition
  if (!mergeSliceSkipKeys(sanitizedBuilder, builder.slice(), acceptor)) {
    return Result(           // result
        TRI_ERROR_INTERNAL,  // code
        std::string("failed to generate externally visible link definition "
                    "while emplacing collection '") +
            std::to_string(cid.id()) + "' into arangosearch View '" + name() +
            "'");
  }

  sanitizedBuilder.close();

  // '_collections' can be asynchronously read
  auto lock = irs::make_lock_guard(_mutex);
  auto [it, emplaced] = _collections.try_emplace(cid, link.collection().name(),
                                                 std::move(sanitizedBuilder));
  UNUSED(it);

  if (!emplaced) {
    return {TRI_ERROR_ARANGO_DUPLICATE_IDENTIFIER,
            "duplicate entry while emplacing collection '" +
                std::to_string(cid.id()) + "' into arangosearch View '" +
                name() + "'"};
  }

  return Result();
}

Result IResearchViewCoordinator::renameImpl(std::string const& oldName) {
  return LogicalViewHelperClusterInfo::rename(*this, oldName);
}

Result IResearchViewCoordinator::unlink(DataSourceId) noexcept {
  return Result();  // NOOP since no internal store
}

IResearchViewCoordinator::IResearchViewCoordinator(TRI_vocbase_t& vocbase,
                                                   velocypack::Slice info)
    : LogicalView(*this, vocbase, info) {
  TRI_ASSERT(ServerState::instance()->isCoordinator());
}

bool IResearchViewCoordinator::visitCollections(
    CollectionVisitor const& visitor) const {
  // '_collections' can be asynchronously modified
  auto lock = irs::make_shared_lock(_mutex);

  for (auto& entry : _collections) {
    if (!visitor(entry.first)) {
      return false;
    }
  }

  return true;
}

Result IResearchViewCoordinator::properties(velocypack::Slice slice,
                                            bool isUserRequest,
                                            bool partialUpdate) {
  if (!vocbase().server().hasFeature<ClusterFeature>()) {
    return {TRI_ERROR_INTERNAL,
            std::string("failure to get storage engine while "
                        "updating arangosearch view '") +
                name() + "'"};
  }
  auto& engine = vocbase().server().getFeature<ClusterFeature>().clusterInfo();

  try {
    auto links = slice.hasKey(StaticStrings::LinksField)
                     ? slice.get(StaticStrings::LinksField)
                     : velocypack::Slice::emptyObjectSlice();
    auto res = IResearchLinkHelper::validateLinks(vocbase(), links);

    if (!res.ok()) {
      return res;
    }

    // check link auth as per https://github.com/arangodb/backlog/issues/459
    ExecContext const& exe = ExecContext::current();
    if (!exe.isSuperuser()) {
      // check existing links
      for (auto& entry : _collections) {  // TODO Data race?
        auto collection = engine.getCollection(
            vocbase().name(), std::to_string(entry.first.id()));

        if (collection &&
            !exe.canUseCollection(vocbase().name(), collection->name(),
                                  auth::Level::RO)) {
          return Result(
              TRI_ERROR_FORBIDDEN,
              std::string("while updating arangosearch definition, error: "
                          "collection '") +
                  collection->name() + "' not authorized for read access");
        }
      }
    }
    // TODO read necessary members from slice in single call
    std::string error;
    IResearchViewMeta meta;

    auto const& defaults = partialUpdate ? _meta : IResearchViewMeta::DEFAULT();

    if (!meta.init(slice, error, defaults)) {
      return Result(TRI_ERROR_BAD_PARAMETER,
                    error.empty()
                        ? (std::string("failed to update arangosearch view '") +
                           name() + "' from definition: " + slice.toString())
                        : (std::string("failed to update arangosearch view '") +
                           name() + "' from definition, error in attribute '" +
                           error + "': " + slice.toString()));
    }
    // only trigger persisting of properties if they have changed
    if (!equalPartial(_meta, meta)) {
      IResearchViewMeta oldMeta{IResearchViewMeta::PartialTag{},
                                std::move(_meta)};
      _meta.storePartial(std::move(meta));  // update meta for persistence
      auto result = LogicalViewHelperClusterInfo::properties(*this);
      _meta.storePartial(std::move(oldMeta));  // restore meta
      if (!result.ok()) {
        return result;
      }
    }

    if (links.isEmptyObject() && partialUpdate) {
      return {};  // nothing more to do
    }

    // ...........................................................................
    // update links if requested (on a best-effort basis)
    // indexing of collections is done in different threads so no locks can be
    // held and rollback is not possible as a result it's also possible for
    // links to be simultaneously modified via a different callflow (e.g. from
    // collections)
    // ...........................................................................

    velocypack::Builder currentLinks;
    std::unordered_set<DataSourceId> currentCids;

    {
      // '_collections' can be asynchronously modified
      auto lock = irs::make_shared_lock(_mutex);

      currentLinks.openObject();

      for (auto& entry : _collections) {
        currentCids.emplace(entry.first);
        currentLinks.add(entry.second.first, entry.second.second.slice());
      }

      currentLinks.close();
    }

    std::unordered_set<DataSourceId> collections;

    if (partialUpdate) {
      return IResearchLinkHelper::updateLinks(collections, *this, links,
                                              getDefaultVersion(isUserRequest));
    }

    return IResearchLinkHelper::updateLinks(collections, *this, links,
                                            getDefaultVersion(isUserRequest),
                                            currentCids);
  } catch (basics::Exception& e) {
    LOG_TOPIC("714b3", WARN, iresearch::TOPIC)
        << "caught exception while updating properties for arangosearch view '"
        << name() << "': " << e.code() << " " << e.what();

    return {e.code(),
            std::string("error updating properties for arangosearch view '") +
                name() + "'"};
  } catch (std::exception const& e) {
    LOG_TOPIC("86a5c", WARN, iresearch::TOPIC)
        << "caught exception while updating properties for arangosearch view '"
        << name() << "': " << e.what();

    return {TRI_ERROR_BAD_PARAMETER,
            std::string("error updating properties for arangosearch view '") +
                name() + "'"};
  } catch (...) {
    LOG_TOPIC("17b66", WARN, iresearch::TOPIC)
        << "caught exception while updating properties for arangosearch view '"
        << name() << "'";

    return {TRI_ERROR_BAD_PARAMETER,
            std::string("error updating properties for arangosearch view '") +
                name() + "'"};
  }
}

Result IResearchViewCoordinator::dropImpl() {
  if (!vocbase().server().hasFeature<ClusterFeature>()) {
    return Result(TRI_ERROR_INTERNAL,
                  std::string("failure to get storage engine while "
                              "dropping arangosearch view '") +
                      name() + "'");
  }
  auto& engine = vocbase().server().getFeature<ClusterFeature>().clusterInfo();

  // drop links first
  {
    std::unordered_set<DataSourceId> currentCids;

    visitCollections([&currentCids](DataSourceId cid) -> bool {
      currentCids.emplace(cid);
      return true;
    });

    // check link auth as per https://github.com/arangodb/backlog/issues/459
    ExecContext const& exe = ExecContext::current();
    if (!exe.isSuperuser()) {
      for (auto& entry : currentCids) {
        auto collection =
            engine.getCollection(vocbase().name(), std::to_string(entry.id()));

        if (collection &&
            !exe.canUseCollection(vocbase().name(), collection->name(),
                                  auth::Level::RO)) {
          return Result(TRI_ERROR_FORBIDDEN);
        }
      }
    }

    std::unordered_set<DataSourceId> collections;
    auto res = IResearchLinkHelper::updateLinks(
        collections, *this, velocypack::Slice::emptyObjectSlice(),
        LinkVersion::MAX,  // we don't care of link version due to removal only
                           // request
        currentCids);

    if (!res.ok()) {
      return {res.errorNumber(),
              basics::StringUtils::concatT(
                  "failed to remove links while removing arangosearch view '",
                  name(), "': ", res.errorMessage())};
    }
  }

  return LogicalViewHelperClusterInfo::drop(*this);
}

}  // namespace arangodb::iresearch
