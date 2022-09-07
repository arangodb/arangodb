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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "LogicalView.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/StaticStrings.h"
#include "Basics/VelocyPackHelper.h"
#include "Cluster/ClusterFeature.h"
#include "Cluster/ClusterInfo.h"
#include "Cluster/ServerState.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "RestServer/DatabaseFeature.h"
#include "RestServer/ViewTypesFeature.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "StorageEngine/StorageEngine.h"
#include "Utils/Events.h"
#include "Utils/ExecContext.h"
#include "Utilities/NameValidator.h"
#include "VocBase/ticks.h"
#include "VocBase/vocbase.h"

#include <velocypack/Iterator.h>

#include <utility>

namespace arangodb {
namespace {

template<typename Func>
Result safeCall(Func&& func) {
  try {
    return std::forward<Func>(func)();
  } catch (basics::Exception const& e) {
    return {e.code(), e.what()};
  } catch (std::exception const& e) {
    return {TRI_ERROR_INTERNAL, e.what()};
  } catch (...) {
    return {TRI_ERROR_INTERNAL};
  }
}

}  // namespace

// @brief Constructor used in coordinator case.
// The Slice contains the part of the plan that
// is relevant for this view
LogicalView::LogicalView(std::pair<ViewType, std::string_view> typeInfo,
                         TRI_vocbase_t& vocbase, velocypack::Slice definition)
    : LogicalDataSource(*this, vocbase, definition),
      _typeInfo{std::move(typeInfo)} {
  // ensure that the 'definition' was used as the configuration source
  if (!definition.isObject()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_BAD_PARAMETER,
        "got an invalid view definition while constructing LogicalView");
  }
  bool extendedNames =
      vocbase.server().getFeature<DatabaseFeature>().extendedNamesForViews();
  if (!ViewNameValidator::isAllowedName(/*allowSystem*/ false, extendedNames,
                                        name())) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_ILLEGAL_NAME);
  }
  if (!id()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_BAD_PARAMETER,
        "got invalid view identifier while constructing LogicalView");
  }
  // update server's tick value
  TRI_UpdateTickServer(id().id());
}

Result LogicalView::appendVPack(velocypack::Builder& build, Serialization ctx,
                                bool safe) const {
  if (!build.isOpenObject()) {
    return {TRI_ERROR_BAD_PARAMETER,
            "invalid builder provided for LogicalView definition"};
  }
  build.add(StaticStrings::DataSourceType, velocypack::Value(_typeInfo.second));
  return appendVPackImpl(build, ctx, safe);
}

bool LogicalView::canUse(auth::Level const& level) {
  return ExecContext::current().canUseDatabase(vocbase().name(), level);
  // TODO per-view authentication checks disabled as per
  // https://github.com/arangodb/backlog/issues/459
  // return !ctx || (  // authentication not enabled
  //   ctx->canUseDatabase(vocbase.name(), level) &&  // can use vocbase
  //   ctx->canUseCollection(vocbase.name(), name(), level)  // can use view
  // ));
}

Result LogicalView::create(LogicalView::ptr& view, TRI_vocbase_t& vocbase,
                           velocypack::Slice definition, bool isUserRequest) {
  auto& server = vocbase.server();
  if (!server.hasFeature<ViewTypesFeature>()) {
    std::string name;
    if (definition.isObject()) {
      name = basics::VelocyPackHelper::getStringValue(
          definition, StaticStrings::DataSourceName, "");
    }
    events::CreateView(vocbase.name(), name, TRI_ERROR_INTERNAL);
    return {TRI_ERROR_INTERNAL,
            "Failure to get 'ViewTypes' feature while creating LogicalView"};
  }
  auto& viewTypes = server.getFeature<ViewTypesFeature>();
  auto type = basics::VelocyPackHelper::getStringView(
      definition, StaticStrings::DataSourceType, {});
  auto& factory = viewTypes.factory(type);
  return factory.create(view, vocbase, definition, isUserRequest);
}

Result LogicalView::drop() {
  if (deleted()) {
    return {};  // view already dropped
  }
  // mark as deleted to avoid double-delete (including recursive calls)
  deleted(true);
  try {
    auto r = dropImpl();
    if (!r.ok()) {
      deleted(false);
    }
    return r;
  } catch (...) {
    deleted(false);
    throw;
  }
}

bool LogicalView::enumerate(
    TRI_vocbase_t& vocbase,
    std::function<bool(std::shared_ptr<LogicalView> const&)> const& callback) {
  TRI_ASSERT(callback);
  if (!ServerState::instance()->isCoordinator()) {
    for (auto& view : vocbase.views()) {
      if (!callback(view)) {
        return false;
      }
    }
    return true;
  }
  auto& server = vocbase.server();
  if (!server.hasFeature<ClusterFeature>()) {
    LOG_TOPIC("694fd", ERR, Logger::VIEWS)
        << "failure to get storage engine while enumerating views";
    return false;
  }
  auto& engine = server.getFeature<ClusterFeature>().clusterInfo();
  for (auto& view : engine.getViews(vocbase.name())) {
    if (!callback(view)) {
      return false;
    }
  }
  return true;
}

Result LogicalView::instantiate(LogicalView::ptr& view, TRI_vocbase_t& vocbase,
                                velocypack::Slice definition,
                                bool isUserRequest) {
  auto& server = vocbase.server();
  if (!server.hasFeature<ViewTypesFeature>()) {
    return {TRI_ERROR_INTERNAL,
            "Failure to get 'ViewTypes' feature while creating LogicalView"};
  }
  auto& viewTypes = server.getFeature<ViewTypesFeature>();
  auto type = basics::VelocyPackHelper::getStringView(
      definition, StaticStrings::DataSourceType, {});
  auto& factory = viewTypes.factory(type);
  return factory.instantiate(view, vocbase, definition, isUserRequest);
}

Result LogicalView::rename(std::string&& newName) {
  auto oldName = name();
  try {
    name(std::move(newName));
    auto r = renameImpl(oldName);
    if (!r.ok()) {
      name(std::move(oldName));
    }
    return r;
  } catch (...) {
    name(std::move(oldName));
    throw;
  }
}

namespace cluster_helper {

Result construct(LogicalView::ptr& view, TRI_vocbase_t& vocbase,
                 velocypack::Slice definition, bool isUserRequest) noexcept {
  auto& server = vocbase.server();
  if (!server.hasFeature<ClusterFeature>()) {
    return {TRI_ERROR_INTERNAL,
            "failure to find storage engine while creating arangosearch View "
            "in database '" +
                vocbase.name() + "'"};
  }
  return safeCall([&]() -> Result {
    auto& engine = server.getFeature<ClusterFeature>().clusterInfo();
    LogicalView::ptr impl;
    auto r = LogicalView::instantiate(impl, vocbase, definition, isUserRequest);
    if (!r.ok()) {
      return r;
    }
    if (!impl) {
      return {TRI_ERROR_INTERNAL,
              "failure during instantiation while creating arangosearch View "
              "in database '" +
                  vocbase.name() + "'"};
    }
    velocypack::Builder b;
    b.openObject();
    // include links so that Agency will always have a full definition
    r = impl->properties(b, LogicalDataSource::Serialization::Persistence);
    if (!r.ok()) {
      return r;
    }
    auto const id = std::to_string(impl->id().id());
    r = engine.createViewCoordinator(vocbase.name(), id, b.close().slice());
    if (!r.ok()) {
      return r;
    }
    // refresh view from Agency
    view = engine.getView(vocbase.name(), id);
    TRI_ASSERT(view);
    if (view) {
      // open view to match the behavior in StorageEngine::openExistingDatabase
      // and original behavior of TRI_vocbase_t::createView
      view->open();
    }
    return {};
  });
}

Result drop(LogicalView const& view) noexcept {
  auto& vocbase = view.vocbase();
  auto& server = vocbase.server();
  if (!server.hasFeature<ClusterFeature>()) {
    return {TRI_ERROR_INTERNAL,
            "failure to find storage engine while dropping view '" +
                view.name() + "' from database '" + vocbase.name() + "'"};
  }
  return safeCall([&] {
    auto& engine = server.getFeature<ClusterFeature>().clusterInfo();
    return engine.dropViewCoordinator(vocbase.name(),
                                      std::to_string(view.id().id()));
  });
}

Result properties(LogicalView const& view, bool safe) noexcept {
  auto& vocbase = view.vocbase();
  auto& server = vocbase.server();
  if (!server.hasFeature<ClusterFeature>()) {
    return {TRI_ERROR_INTERNAL,
            "failure to find storage engine while dropping view '" +
                view.name() + "' from database '" + vocbase.name() + "'"};
  }
  return safeCall([&] {
    velocypack::Builder build;
    build.openObject();
    auto r = view.properties(
        build, LogicalDataSource::Serialization::Persistence, safe);
    if (!r.ok()) {
      return r;
    }
    auto& engine = server.getFeature<ClusterFeature>().clusterInfo();
    return engine.setViewPropertiesCoordinator(
        vocbase.name(), std::to_string(view.id().id()), build.close().slice());
  });
}

}  // namespace cluster_helper
namespace storage_helper {

Result construct(LogicalView::ptr& view, TRI_vocbase_t& vocbase,
                 velocypack::Slice definition, bool isUserRequest) noexcept {
  return safeCall([&]() -> Result {
    TRI_set_errno(TRI_ERROR_NO_ERROR);  // reset before calling createView(...)
    auto impl = vocbase.createView(definition, isUserRequest);
    if (!impl) {
      return {
          TRI_errno() == TRI_ERROR_NO_ERROR ? TRI_ERROR_INTERNAL : TRI_errno(),
          "failure during instantiation while creating arangosearch View in "
          "database '" +
              vocbase.name() + "'"};
    }
    view = impl;
    return {};
  });
}

Result drop(LogicalView const& view) noexcept {
  return safeCall([&] {  // true since caller should have checked for 'system'
    return view.vocbase().dropView(view.id(), true);
  });
}

Result properties(LogicalView const& view, bool safe) noexcept {
  auto& vocbase = view.vocbase();
  auto& server = vocbase.server();
  if (!server.hasFeature<EngineSelectorFeature>()) {
    return {
        TRI_ERROR_INTERNAL,
        "failed to find storage engine while updating definition of view '" +
            view.name() + "' in database '" + vocbase.name() + "'"};
  }
  return safeCall([&]() -> Result {
    auto& engine = server.getFeature<EngineSelectorFeature>().engine();
    if (engine.inRecovery()) {
      return {};
    }
    velocypack::Builder b;
    b.openObject();
    auto r = view.properties(
        b, LogicalDataSource::Serialization::PersistenceWithInProgress, safe);
    if (!r.ok()) {
      return r;
    }
    return engine.changeView(view, b.close().slice());
  });
}

Result rename(LogicalView const& view, std::string const& oldName) noexcept {
  return safeCall([&] {  // for breakpoint
    return view.vocbase().renameView(view.id(), oldName);
  });
}

}  // namespace storage_helper
}  // namespace arangodb
