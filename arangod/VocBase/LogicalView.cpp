////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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
#include "RestServer/DatabaseFeature.h"
#include "RestServer/ViewTypesFeature.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "StorageEngine/StorageEngine.h"
#include "Utils/Events.h"
#include "Utils/ExecContext.h"
#include "VocBase/ticks.h"
#include "VocBase/vocbase.h"

#include <velocypack/Iterator.h>
#include <velocypack/StringRef.h>
#include <velocypack/velocypack-aliases.h>

namespace arangodb {

// -----------------------------------------------------------------------------
// --SECTION--                                                       LogicalView
// -----------------------------------------------------------------------------

// @brief Constructor used in coordinator case.
// The Slice contains the part of the plan that
// is relevant for this view
LogicalView::LogicalView(TRI_vocbase_t& vocbase, VPackSlice const& definition, uint64_t planVersion)
    : LogicalDataSource(LogicalView::category(),
                        LogicalDataSource::Type::emplace(arangodb::basics::VelocyPackHelper::getStringRef(
                            definition, StaticStrings::DataSourceType, VPackStringRef())),
                        vocbase, definition, planVersion) {
  // ensure that the 'definition' was used as the configuration source
  if (!definition.isObject()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_BAD_PARAMETER,
        "got an invalid view definition while constructing LogicalView");
  }

  if (!TRI_vocbase_t::IsAllowedName(definition)) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_ILLEGAL_NAME);
  }

  if (!id()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_BAD_PARAMETER,
        "got invalid view identifier while constructing LogicalView");
  }

  // update server's tick value
  TRI_UpdateTickServer(static_cast<TRI_voc_tick_t>(id()));
}

Result LogicalView::appendVelocyPack(velocypack::Builder& builder,
                                     Serialization context) const {
  if (!builder.isOpenObject()) {
    return Result(TRI_ERROR_BAD_PARAMETER,
                  std::string(
                      "invalid builder provided for LogicalView definition"));
  }

  builder.add(StaticStrings::DataSourceType, arangodb::velocypack::Value(type().name()));

  return appendVelocyPackImpl(builder, context);
}

bool LogicalView::canUse(arangodb::auth::Level const& level) {
  // as per https://github.com/arangodb/backlog/issues/459
  return ExecContext::current().canUseDatabase(vocbase().name(), level); // can use vocbase

  /* FIXME TODO per-view authentication checks disabled as per https://github.com/arangodb/backlog/issues/459
  return !ctx // authentication not enabled
    || (ctx->canUseDatabase(vocbase.name(), level) // can use vocbase
        && (ctx->canUseCollection(vocbase.name(), name(), level)) // can use view
       );
  */
}

/*static*/ LogicalDataSource::Category const& LogicalView::category() noexcept {
  static const Category category;

  return category;
}

/*static*/ Result LogicalView::create(LogicalView::ptr& view, TRI_vocbase_t& vocbase,
                                      velocypack::Slice definition) {
  if (!vocbase.server().hasFeature<ViewTypesFeature>()) {
    std::string name;
    if (definition.isObject()) {
      name = basics::VelocyPackHelper::getStringValue(definition, StaticStrings::DataSourceName,
                                                      "");
    }
    events::CreateView(vocbase.name(), name, TRI_ERROR_INTERNAL);
    return Result(
        TRI_ERROR_INTERNAL,
        "Failure to get 'ViewTypes' feature while creating LogicalView");
  }
  auto& viewTypes = vocbase.server().getFeature<ViewTypesFeature>();

  auto type =
      basics::VelocyPackHelper::getStringRef(definition, StaticStrings::DataSourceType,
                                             velocypack::StringRef(nullptr, 0));
  auto& factory = viewTypes.factory(LogicalDataSource::Type::emplace(type));

  return factory.create(view, vocbase, definition);
}

Result LogicalView::drop() {
  if (deleted()) {
    return Result();  // view already dropped
  }

  try {
    deleted(true);  // mark as deleted to avoid double-delete (including
                    // recursive calls)

    auto res = dropImpl();

    if (!res.ok()) {
      deleted(false);  // not fully deleted

      return res;
    }
  } catch (...) {
    deleted(false);  // not fully deleted

    throw;
  }

  return Result();
}

/*static*/ bool LogicalView::enumerate(
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

  if (!vocbase.server().hasFeature<ClusterFeature>()) {
    LOG_TOPIC("694fd", ERR, Logger::VIEWS)
        << "failure to get storage engine while enumerating views";

    return false;
  }
  auto& engine = vocbase.server().getFeature<ClusterFeature>().clusterInfo();

  for (auto& view : engine.getViews(vocbase.name())) {
    if (!callback(view)) {
      return false;
    }
  }

  return true;
}

/*static*/ Result LogicalView::instantiate(LogicalView::ptr& view, TRI_vocbase_t& vocbase,
                                           velocypack::Slice definition) {
  if (!vocbase.server().hasFeature<ViewTypesFeature>()) {
    return Result(
        TRI_ERROR_INTERNAL,
        "Failure to get 'ViewTypes' feature while creating LogicalView");
  }
  auto& viewTypes = vocbase.server().getFeature<ViewTypesFeature>();

  auto type =
      basics::VelocyPackHelper::getStringRef(definition, StaticStrings::DataSourceType,
                                             velocypack::StringRef(nullptr, 0));
  auto& factory = viewTypes.factory(LogicalDataSource::Type::emplace(type));

  return factory.instantiate(view, vocbase, definition);
}

Result LogicalView::rename(std::string&& newName) {
  auto oldName = name();

  try {
    name(std::move(newName));

    auto res = renameImpl(oldName);

    if (!res.ok()) {
      name(std::move(oldName));  // restore name

      return res;
    }
  } catch (...) {
    name(std::move(oldName));

    throw;
  }

  return Result();
}

// -----------------------------------------------------------------------------
// --SECTION--                                      LogicalViewHelperClusterInfo
// -----------------------------------------------------------------------------

/*static*/ Result LogicalViewHelperClusterInfo::construct(LogicalView::ptr& view,
                                                          TRI_vocbase_t& vocbase,
                                                          velocypack::Slice const& definition) noexcept {
  try {
    if (!vocbase.server().hasFeature<ClusterFeature>()) {
      return Result(TRI_ERROR_INTERNAL,
                    std::string("failure to find storage engine while creating "
                                "arangosearch View in database '") +
                        vocbase.name() + "'");
    }
    auto& engine = vocbase.server().getFeature<ClusterFeature>().clusterInfo();

    LogicalView::ptr impl;
    auto res = LogicalView::instantiate(impl, vocbase, definition);

    if (!res.ok()) {
      return res;
    }

    if (!impl) {
      return arangodb::Result(TRI_ERROR_INTERNAL,
                              std::string(
                                  "failure during instantiation while creating "
                                  "arangosearch View in database '") +
                                  vocbase.name() + "'");
    }

    velocypack::Builder builder;

    builder.openObject();
    // include links so that Agency will always have a full definition
    res = impl->properties(builder, LogicalDataSource::Serialization::Persistence);

    if (!res.ok()) {
      return res;
    }

    builder.close();
    res = engine.createViewCoordinator(  // create view
        vocbase.name(), std::to_string(impl->id()), builder.slice()  // args
    );

    if (!res.ok()) {
      return res;
    }

    view = engine.getView(vocbase.name(),
                          std::to_string(impl->id()));  // refresh view from Agency

    if (view) {
      view->open();  // open view to match the behavior in
                     // StorageEngine::openExistingDatabase(...) and original
                     // behavior of TRI_vocbase_t::createView(...)
    }

    return Result();
  } catch (basics::Exception const& e) {
    return Result(e.code());  // noexcept constructor
  } catch (...) {
    // NOOP
  }

  return Result(TRI_ERROR_INTERNAL);  // noexcept constructor
}

/*static*/ Result LogicalViewHelperClusterInfo::drop(LogicalView const& view) noexcept {
  try {
    if (!view.vocbase().server().hasFeature<ClusterFeature>()) {
      return Result(
          TRI_ERROR_INTERNAL,
          std::string("failure to find storage engine while dropping view '") +
              view.name() + "' from database '" + view.vocbase().name() + "'");
    }
    auto& engine = view.vocbase().server().getFeature<ClusterFeature>().clusterInfo();

    return engine.dropViewCoordinator(                    // drop view
        view.vocbase().name(), std::to_string(view.id())  // args
    );
  } catch (basics::Exception const& e) {
    return Result(e.code());  // noexcept constructor
  } catch (...) {
    // NOOP
  }

  return Result(TRI_ERROR_INTERNAL);  // noexcept constructor
}

/*static*/ Result LogicalViewHelperClusterInfo::properties(velocypack::Builder& builder,
                                                           LogicalView const& view) noexcept {
  return Result();  // NOOP
}

/*static*/ Result LogicalViewHelperClusterInfo::properties(LogicalView const& view) noexcept {
  try {
    if (!view.vocbase().server().hasFeature<ClusterFeature>()) {
      return Result(TRI_ERROR_INTERNAL,
                    std::string("failure to find storage engine while updating "
                                "definition of view '") +
                        view.name() + "' from database '" +
                        view.vocbase().name() + "'");
    }
    auto& engine = view.vocbase().server().getFeature<ClusterFeature>().clusterInfo();

    velocypack::Builder builder;

    builder.openObject();

    auto res = view.properties(builder, LogicalDataSource::Serialization::Persistence);

    if (!res.ok()) {
      return res;
    }

    builder.close();

    return engine.setViewPropertiesCoordinator(view.vocbase().name(),
                                               std::to_string(view.id()),
                                               builder.slice());
  } catch (basics::Exception const& e) {
    return Result(e.code());  // noexcept constructor
  } catch (...) {
    // NOOP
  }

  return Result(TRI_ERROR_INTERNAL);  // noexcept constructor
}

/*static*/ Result LogicalViewHelperClusterInfo::rename(LogicalView const& view,
                                                       std::string const& oldName) noexcept {
  return Result(TRI_ERROR_CLUSTER_UNSUPPORTED);  // renaming a view in a cluster
                                                 // is not supported
}

// -----------------------------------------------------------------------------
// --SECTION--                                    LogicalViewHelperStorageEngine
// -----------------------------------------------------------------------------

/*static*/ Result LogicalViewHelperStorageEngine::construct(LogicalView::ptr& view,
                                                            TRI_vocbase_t& vocbase,
                                                            velocypack::Slice const& definition) noexcept {
  try {
    TRI_set_errno(TRI_ERROR_NO_ERROR);  // reset before calling createView(...)
    auto impl = vocbase.createView(definition);

    if (!impl) {
      return Result(TRI_ERROR_NO_ERROR == TRI_errno() ? TRI_ERROR_INTERNAL : TRI_errno(),
                    std::string("failure during instantiation while creating "
                                "arangosearch View in database '") +
                        vocbase.name() + "'");
    }

    view = impl;

    return Result();
  } catch (basics::Exception const& e) {
    return Result(e.code());  // noexcept constructor
  } catch (...) {
    // NOOP
  }

  return Result(TRI_ERROR_INTERNAL);  // noexcept constructor
}

/*static*/ Result LogicalViewHelperStorageEngine::destruct(LogicalView const& view) noexcept {
  if (!view.deleted()) {
    return Result();  // NOOP
  }

  return Result();
}

/*static*/ Result LogicalViewHelperStorageEngine::drop(LogicalView const& view) noexcept {
  try {
    return view.vocbase().dropView(view.id(), true);  // true since caller should have checked for 'system'
  } catch (basics::Exception const& e) {
    return Result(e.code());  // noexcept constructor
  } catch (...) {
    // NOOP
  }

  return Result(TRI_ERROR_INTERNAL);  // noexcept constructor
}

/*static*/ Result LogicalViewHelperStorageEngine::properties(velocypack::Builder& builder,
                                                             LogicalView const& view) noexcept {
  try {
    if (!builder.isOpenObject()) {
      return Result(TRI_ERROR_BAD_PARAMETER,
                    std::string(
                        "invalid builder provided for LogicalView definition"));
    }

    auto* engine = EngineSelectorFeature::ENGINE;

    if (!engine) {
      return Result(TRI_ERROR_INTERNAL,
                    std::string("failed to find a storage engine while "
                                "querying definition of view '") +
                        view.name() + "' in database '" +
                        view.vocbase().name() + "'");
    }

    return Result();
  } catch (basics::Exception const& e) {
    return Result(e.code());  // noexcept constructor
  } catch (...) {
    // NOOP
  }

  return Result(TRI_ERROR_INTERNAL);  // noexcept constructor
}

/*static*/ Result LogicalViewHelperStorageEngine::properties(LogicalView const& view) noexcept {
  try {
    if (!view.vocbase().server().hasFeature<DatabaseFeature>()) {
      return Result(TRI_ERROR_INTERNAL,
                    std::string("failed to find feature 'Database' while "
                                "updating definition of view '") +
                        view.name() + "' in database '" +
                        view.vocbase().name() + "'");
    }
    auto& databaseFeature = view.vocbase().server().getFeature<DatabaseFeature>();

    if (!view.vocbase().server().hasFeature<EngineSelectorFeature>() ||
        !view.vocbase().server().getFeature<EngineSelectorFeature>().selected()) {
      return Result(TRI_ERROR_INTERNAL,
                    std::string("failed to find a storage engine while "
                                "updating definition of view '") +
                        view.name() + "' in database '" +
                        view.vocbase().name() + "'");
    }
    auto& engine = view.vocbase().server().getFeature<EngineSelectorFeature>().engine();

    auto doSync = databaseFeature.forceSyncProperties();

    if (engine.inRecovery()) {
      return Result();  // do not modify engine while in recovery
    }

    return engine.changeView(view.vocbase(), view, doSync);
  } catch (basics::Exception const& e) {
    return Result(e.code());  // noexcept constructor
  } catch (...) {
    // NOOP
  }

  return Result(TRI_ERROR_INTERNAL);  // noexcept constructor
}

/*static*/ Result LogicalViewHelperStorageEngine::rename(LogicalView const& view,
                                                         std::string const& oldName) noexcept {
  try {
    return view.vocbase().renameView(view.id(), oldName);
  } catch (basics::Exception const& e) {
    return Result(e.code());  // noexcept constructor
  } catch (...) {
    // NOOP
  }

  return Result(TRI_ERROR_INTERNAL);  // noexcept constructor
}

}  // namespace arangodb

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------
