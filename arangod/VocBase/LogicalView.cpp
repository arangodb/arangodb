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
     category(),
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

/*static*/ std::shared_ptr<LogicalView> LogicalView::create(
    TRI_vocbase_t& vocbase,
    velocypack::Slice definition,
    bool isNew,
    uint64_t planVersion /*= 0*/,
    PreCommitCallback const& preCommit /*= PreCommitCallback()*/
) {
  auto const* viewTypes =
    application_features::ApplicationServer::lookupFeature<ViewTypesFeature>();

  if (!viewTypes) {
    LOG_TOPIC(ERR, Logger::VIEWS)
      << "Failure to get 'ViewTypes' feature while creating LogicalView";

    return nullptr;
  }

  auto const viewType = basics::VelocyPackHelper::getStringRef(
    definition, StaticStrings::DataSourceType, ""
  );
  auto const& dataSourceType =
    arangodb::LogicalDataSource::Type::emplace(viewType);
  auto const& viewFactory = viewTypes->factory(dataSourceType);

  if (!viewFactory) {
    TRI_set_errno(TRI_ERROR_BAD_PARAMETER);
    LOG_TOPIC(ERR, Logger::VIEWS)
      << "Found view type for which there is no factory, type: "
      << viewType.toString();

    return nullptr;
  }

  auto view = viewFactory(vocbase, definition, isNew, planVersion, preCommit);

  if (!view) {
    LOG_TOPIC(ERR, Logger::VIEWS)
      << "Failure to instantiate view of type: " << viewType.toString();

    return nullptr;
  }

  LOG_TOPIC(DEBUG, Logger::VIEWS)
    << "created '" << viewType.toString() << "' view '" << view->name() << "' ("
    << view->guid() << ")";

  return view;
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

/*static*/ arangodb::Result LogicalViewStorageEngine::create(
    LogicalViewStorageEngine const& view
) {
  TRI_ASSERT(!ServerState::instance()->isCoordinator());
  StorageEngine* engine = EngineSelectorFeature::ENGINE;

  if (!engine) {
    return arangodb::Result(
      TRI_ERROR_INTERNAL,
      std::string("failure to get storage engine during storage engine persistance of view '") + view.name() + "'"
    );
  }

  try {
    return engine->createView(view.vocbase(), view.id(), view);
  } catch (std::exception const& e) {
    return arangodb::Result(
      TRI_ERROR_INTERNAL,
      std::string("caught exception during storage engine persistance of view '") + view.name() + "': " + e.what()
    );
  } catch (...) {
    return arangodb::Result(
      TRI_ERROR_INTERNAL,
      std::string("caught exception during storage engine persistance of view '") + view.name() + "'"
    );
  }
}

arangodb::Result LogicalViewStorageEngine::drop() {
  TRI_ASSERT(!ServerState::instance()->isCoordinator());
  StorageEngine* engine = EngineSelectorFeature::ENGINE;
  TRI_ASSERT(engine);
  auto res = dropImpl();

  if (res.ok()) {
    deleted(true);
    engine->dropView(vocbase(), *this);
  }

  return res;
}

Result LogicalViewStorageEngine::rename(std::string&& newName, bool doSync) {
  TRI_ASSERT(!ServerState::instance()->isCoordinator());
  StorageEngine* engine = EngineSelectorFeature::ENGINE;
  TRI_ASSERT(engine);

  auto oldName = name();

  try {
    name(std::move(newName));

    // store new view definition to disk
    if (!engine->inRecovery()) {
      // write WAL 'change' marker
      return engine->changeView(vocbase(), *this, doSync);
    }
  } catch (basics::Exception const& ex) {
    name(std::move(oldName));

    return ex.code();
  } catch (...) {
    name(std::move(oldName));

    return TRI_ERROR_INTERNAL;
  }

  return TRI_ERROR_NO_ERROR;
}

arangodb::Result LogicalViewStorageEngine::updateProperties(
    VPackSlice const& slice,
    bool partialUpdate,
    bool doSync
) {
  TRI_ASSERT(!ServerState::instance()->isCoordinator());

  auto res = updateProperties(slice, partialUpdate);

  if (!res.ok()) {
    LOG_TOPIC(ERR, Logger::VIEWS) << "failed to update view with properties '"
                                  << slice.toJson() << "'";
    return res;
  }
  LOG_TOPIC(DEBUG, Logger::VIEWS) << "updated view with properties '"
                                  << slice.toJson() << "'";

  // after this call the properties are stored
  StorageEngine* engine = EngineSelectorFeature::ENGINE;
  TRI_ASSERT(engine);

  if (engine->inRecovery()) {
    return arangodb::Result(); // do not modify engine while in recovery
  }

  try {
    engine->changeView(vocbase(), *this, doSync);
  } catch (arangodb::basics::Exception const& e) {
    return { e.code() };
  } catch (...) {
    return { TRI_ERROR_INTERNAL };
  }

  arangodb::aql::PlanCache::instance()->invalidate(&vocbase());
  arangodb::aql::QueryCache::instance()->invalidate(&vocbase());

  return {};
}

} // arangodb

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------
