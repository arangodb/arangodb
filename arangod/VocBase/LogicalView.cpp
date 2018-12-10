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

#include "Aql/PlanCache.h"
#include "Aql/QueryCache.h"
#include "Basics/StaticStrings.h"
#include "Basics/VelocyPackHelper.h"
#include "Cluster/ClusterInfo.h"
#include "Cluster/ServerState.h"
#include "RestServer/DatabaseFeature.h"
#include "RestServer/ViewTypesFeature.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "StorageEngine/StorageEngine.h"
#include "VocBase/ticks.h"
#include "VocBase/vocbase.h"

#include <velocypack/Iterator.h>

namespace arangodb {

// -----------------------------------------------------------------------------
// --SECTION--                                                       LogicalView
// -----------------------------------------------------------------------------

// @brief Constructor used in coordinator case.
// The Slice contains the part of the plan that
// is relevant for this view
LogicalView::LogicalView(
    TRI_vocbase_t& vocbase,
    VPackSlice const& definition,
    uint64_t planVersion
): LogicalDataSource(
     LogicalView::category(),
     LogicalDataSource::Type::emplace(
       arangodb::basics::VelocyPackHelper::getStringRef(
         definition, StaticStrings::DataSourceType, ""
       )
     ),
     vocbase,
     definition,
     planVersion
   ) {
  // ensure that the 'definition' was used as the configuration source
  if (!definition.isObject()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
      TRI_ERROR_BAD_PARAMETER,
      "got an invalid view definition while constructing LogicalView"
    );
  }

  if (!TRI_vocbase_t::IsAllowedName(definition)) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_ILLEGAL_NAME);
  }

  if (!id()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
      TRI_ERROR_BAD_PARAMETER,
      "got invalid view identifier while constructing LogicalView"
    );
  }

  // update server's tick value
  TRI_UpdateTickServer(static_cast<TRI_voc_tick_t>(id()));
}

/*static*/ LogicalDataSource::Category const& LogicalView::category() noexcept {
  static const Category category;

  return category;
}

/*static*/ Result LogicalView::create(
    LogicalView::ptr& view,
    TRI_vocbase_t& vocbase,
    velocypack::Slice definition
) {
  auto* viewTypes =
    application_features::ApplicationServer::lookupFeature<ViewTypesFeature>();

  if (!viewTypes) {
    return Result(
      TRI_ERROR_INTERNAL,
      "Failure to get 'ViewTypes' feature while creating LogicalView"
    );
  }

  auto type = basics::VelocyPackHelper::getStringRef(
    definition, StaticStrings::DataSourceType, velocypack::StringRef(nullptr, 0)
  );
  auto& factory = viewTypes->factory(LogicalDataSource::Type::emplace(type));

  return factory.create(view, vocbase, definition);
}

/*static*/ bool LogicalView::enumerate(
  TRI_vocbase_t& vocbase,
  std::function<bool(std::shared_ptr<LogicalView> const&)> const& callback
) {
  TRI_ASSERT(callback);

  if (!ServerState::instance()->isCoordinator()) {
    for (auto& view: vocbase.views()) {
      if (!callback(view)) {
        return false;
      }
    }

    return true;
  }

  auto* engine = arangodb::ClusterInfo::instance();

  if (!engine) {
    LOG_TOPIC(ERR, Logger::VIEWS)
      << "failure to get storage engine while enumerating views";

    return false;
  }

  for (auto& view: engine->getViews(vocbase.name())) {
    if (!callback(view)) {
      return false;
    }
  }

  return true;
}

/*static*/ Result LogicalView::instantiate(
    LogicalView::ptr& view,
    TRI_vocbase_t& vocbase,
    velocypack::Slice definition,
    uint64_t planVersion /*= 0*/
) {
  auto* viewTypes =
    application_features::ApplicationServer::lookupFeature<ViewTypesFeature>();

  if (!viewTypes) {
    return Result(
      TRI_ERROR_INTERNAL,
      "Failure to get 'ViewTypes' feature while creating LogicalView"
    );
  }

  auto type = basics::VelocyPackHelper::getStringRef(
    definition, StaticStrings::DataSourceType, velocypack::StringRef(nullptr, 0)
  );
  auto& factory = viewTypes->factory(LogicalDataSource::Type::emplace(type));

  return factory.instantiate(view, vocbase, definition, planVersion);
}

// -----------------------------------------------------------------------------
// --SECTION--                                            LogicalViewClusterInfo
// -----------------------------------------------------------------------------

LogicalViewClusterInfo::LogicalViewClusterInfo(
    TRI_vocbase_t& vocbase,
    VPackSlice const& definition,
    uint64_t planVersion
): LogicalView(vocbase, definition, planVersion) {
  TRI_ASSERT(
    ServerState::instance()->isCoordinator()
    || ServerState::instance()->isDBServer()
  );
}

arangodb::Result LogicalViewClusterInfo::appendVelocyPack(
    arangodb::velocypack::Builder& builder,
    bool detailed,
    bool forPersistence
) const {
  if (!builder.isOpenObject()) {
    return arangodb::Result(
      TRI_ERROR_BAD_PARAMETER,
      std::string("invalid builder provided for LogicalView definition")
    );
  }

  builder.add(
    StaticStrings::DataSourceType,
    arangodb::velocypack::Value(type().name())
  );

  // implementation Information
  if (detailed) {
    auto res = appendVelocyPackDetailed(builder, forPersistence);

    if (!res.ok()) {
      return res;
    }
  }

  // ensure that the object is still open
  if (!builder.isOpenObject()) {
    return arangodb::Result(TRI_ERROR_INTERNAL);
  }

  return arangodb::Result();
}

arangodb::Result LogicalViewClusterInfo::drop() {
  if (deleted()) {
    return Result(); // view already dropped
  }

  auto* engine = arangodb::ClusterInfo::instance();

  if (!engine) {
    return arangodb::Result(
      TRI_ERROR_INTERNAL,
      std::string("failure to get storage engine while dropping View '") + name() + "'"
    );
  }

  try {
    deleted(true); // mark as deleted to avoid double-delete (including recursive calls)

    auto res = dropImpl();

    if (!res.ok()) {
      deleted(false); // not fully deleted

      return res;
    }

    std::string error;
    auto resNum = engine->dropViewCoordinator(
      vocbase().name(), std::to_string(id()), error
    );

    if (TRI_ERROR_NO_ERROR != resNum) {
      deleted(false); // not fully deleted

      if (error.empty()) {
        error = TRI_errno_string(resNum);
      }

      return arangodb::Result(
        resNum,
        std::string("failure during ClusterInfo removal of View in database '") + vocbase().name() + "', error: " + error
      );
    }
  } catch (...) {
    deleted(false); // not fully deleted

    throw;
  }

  return arangodb::Result();
}

arangodb::Result LogicalViewClusterInfo::rename(std::string&&) {
  // renaming a view in a cluster is unsupported
  return TRI_ERROR_CLUSTER_UNSUPPORTED;
}

// -----------------------------------------------------------------------------
// --SECTION--                                          LogicalViewStorageEngine
// -----------------------------------------------------------------------------

LogicalViewStorageEngine::LogicalViewStorageEngine(
    TRI_vocbase_t& vocbase,
    VPackSlice const& definition,
    uint64_t planVersion
): LogicalView(vocbase, definition, planVersion) {
  TRI_ASSERT(
    ServerState::instance()->isDBServer()
    || ServerState::instance()->isSingleServer()
  );
}

LogicalViewStorageEngine::~LogicalViewStorageEngine() {
  if (deleted()) {
    StorageEngine* engine = EngineSelectorFeature::ENGINE;
    TRI_ASSERT(engine);
    // FIXME TODO is this required?
    engine->destroyView(vocbase(), *this);
  }
}

arangodb::Result LogicalViewStorageEngine::appendVelocyPack(
    arangodb::velocypack::Builder& builder,
    bool detailed,
    bool forPersistence
) const {
  if (!builder.isOpenObject()) {
    return arangodb::Result(
      TRI_ERROR_BAD_PARAMETER,
      std::string("invalid builder provided for LogicalView definition")
    );
  }

  builder.add(
    StaticStrings::DataSourceType,
    arangodb::velocypack::Value(type().name())
  );

  // note: includeSystem and forPersistence are not 100% synonymous,
  // however, for our purposes this is an okay mapping; we only set
  // includeSystem if we are persisting the properties
  if (forPersistence) {
    // storage engine related properties
    auto* engine = EngineSelectorFeature::ENGINE;

    if (!engine) {
      return TRI_ERROR_INTERNAL;
    }

    engine->getViewProperties(vocbase(), *this, builder);
  }

  // implementation Information
  if (detailed) {
    auto res = appendVelocyPackDetailed(builder, forPersistence);

    if (!res.ok()) {
      return res;
    }
  }

  // ensure that the object is still open
  if (!builder.isOpenObject()) {
    return arangodb::Result(TRI_ERROR_INTERNAL);
  }

  return arangodb::Result();
}

arangodb::Result LogicalViewStorageEngine::drop() {
  if (deleted()) {
    return Result(); // view already dropped
  }

  try {
    deleted(true); // mark as deleted to avoid double-delete (including recursive calls)

    auto res = dropImpl();

    if (!res.ok()) {
      deleted(false); // not fully deleted

      return res;
    }

    res = vocbase().dropView(id(), true); // true since caller should have checked for 'system'

    if (!res.ok()) {
      deleted(false); // not fully deleted

      return res;
    }
  } catch (...) {
    deleted(false); // not fully deleted

    throw;
  }

  return arangodb::Result();
}

Result LogicalViewStorageEngine::rename(std::string&& newName) {
  TRI_ASSERT(!ServerState::instance()->isCoordinator());
  auto* databaseFeature = application_features::ApplicationServer::lookupFeature<
    DatabaseFeature
  >("Database");

  if (!databaseFeature) {
    return Result(
      TRI_ERROR_INTERNAL,
      "failed to find feature 'Database' while renaming view"
    );
  }

  StorageEngine* engine = EngineSelectorFeature::ENGINE;

  if (!engine) {
    return Result(
      TRI_ERROR_INTERNAL,
      "failed to find a storage engine while renaming view"
    );
  }

  auto doSync = databaseFeature->forceSyncProperties();
  auto oldName = name();
  auto res = vocbase().renameView(id(), newName);

  if (!res.ok()) {
    return res;
  }

  try {
    name(std::move(newName));

    auto res = engine->inRecovery()
      ? Result() : engine->changeView(vocbase(), *this, doSync);

    if (!res.ok()) {
      name(std::move(oldName)); // restore name
      vocbase().renameView(id(), oldName);

      return res;
    }
  } catch (basics::Exception const& ex) {
    name(std::move(oldName));
    vocbase().renameView(id(), oldName);

    return Result(ex.code(), ex.message());
  } catch (...) {
    name(std::move(oldName));
    vocbase().renameView(id(), oldName);

    return Result(TRI_ERROR_INTERNAL, "caught exception while renaming view");;
  }

  return TRI_ERROR_NO_ERROR;
}

arangodb::Result LogicalViewStorageEngine::properties(
    VPackSlice const& slice,
    bool partialUpdate
) {
  TRI_ASSERT(!ServerState::instance()->isCoordinator());

  auto* databaseFeature = application_features::ApplicationServer::lookupFeature<
    DatabaseFeature
  >("Database");

  if (!databaseFeature) {
    return Result(
      TRI_ERROR_INTERNAL,
      "failed to find feature 'Database' while updating collection"
    );
   }

  auto* engine = EngineSelectorFeature::ENGINE;

  if (!engine) {
    return Result(
      TRI_ERROR_INTERNAL,
      "failed to find a storage engine while updating collection"
    );
  }

  auto res = updateProperties(slice, partialUpdate);

  if (!res.ok()) {
    LOG_TOPIC(ERR, Logger::VIEWS) << "failed to update view with properties '"
                                  << slice.toJson() << "'";
    return res;
  }
  LOG_TOPIC(DEBUG, Logger::VIEWS) << "updated view with properties '"
                                  << slice.toJson() << "'";

  auto doSync = !engine->inRecovery() && databaseFeature->forceSyncProperties();

  // after this call the properties are stored
  if (engine->inRecovery()) {
    return arangodb::Result(); // do not modify engine while in recovery
  }

  try {
    engine->changeView(vocbase(), *this, doSync);
  } catch (arangodb::basics::Exception const& e) {
    return Result(e.code(), e.message());
  } catch (...) {
    return Result(TRI_ERROR_INTERNAL, "caught exception while updating view");
  }

  arangodb::aql::PlanCache::instance()->invalidate(&vocbase());
  arangodb::aql::QueryCache::instance()->invalidate(&vocbase());

  return {};
}

} // arangodb

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------
