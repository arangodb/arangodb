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

#include "RestServer/ViewTypesFeature.h"
#include "Basics/ReadLocker.h"
#include "Basics/Result.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/Exceptions.h"
#include "Cluster/ClusterInfo.h"
#include "Cluster/ServerState.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "StorageEngine/StorageEngine.h"
#include "VocBase/ticks.h"

using Helper = arangodb::basics::VelocyPackHelper;

namespace {

TRI_voc_cid_t ReadPlanId(VPackSlice info, TRI_voc_cid_t vid) {
  if (!info.isObject()) {
    // ERROR CASE
    return 0;
  }
  VPackSlice id = info.get("planId");
  if (id.isNone()) {
    return vid;
  }

  if (id.isString()) {
    // string cid, e.g. "9988488"
    return arangodb::basics::StringUtils::uint64(id.copyString());
  } else if (id.isNumber()) {
    // numeric cid, e.g. 9988488
    return id.getNumericValue<uint64_t>();
  }
  // TODO Throw error for invalid type?
  return vid;
}

/*static*/ TRI_voc_cid_t ReadViewId(VPackSlice info) {
  if (!info.isObject()) {
    // ERROR CASE
    return 0;
  }

  // Somehow the id is now propagated to dbservers
  TRI_voc_cid_t id = Helper::extractIdValue(info);

  if (id) {
    return id;
  }

  if (arangodb::ServerState::instance()->isDBServer()) {
    return arangodb::ClusterInfo::instance()->uniqid(1);
  }

  if (arangodb::ServerState::instance()->isCoordinator()) {
    return arangodb::ClusterInfo::instance()->uniqid(1);
  }

  return TRI_NewTickServer();
}

} // namespace

namespace arangodb {

// -----------------------------------------------------------------------------
// --SECTION--                                                       LogicalView
// -----------------------------------------------------------------------------

// @brief Constructor used in coordinator case.
// The Slice contains the part of the plan that
// is relevant for this view
LogicalView::LogicalView(
    TRI_vocbase_t* vocbase,
    VPackSlice const& definition,
    uint64_t planVersion
): LogicalDataSource(
     category(),
     LogicalDataSource::Type::emplace(
       arangodb::basics::VelocyPackHelper::getStringRef(definition, "type", "")
     ),
     vocbase,
     ReadViewId(definition),
     ReadPlanId(definition, 0),
     arangodb::basics::VelocyPackHelper::getStringValue(definition, "name", ""),
     planVersion,
     Helper::readBooleanValue(definition, "deleted", false)
   ) {
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
    uint64_t planVersion /*= 0*/,
    PreCommitCallback const& preCommit /*= PreCommitCallback()*/
) {
  auto const* viewTypes =
    application_features::ApplicationServer::getFeature<ViewTypesFeature>("ViewTypes");

  if (!viewTypes) {
    LOG_TOPIC(ERR, Logger::VIEWS)
      << "Failure to get 'ViewTypes' feature while creating LogicalView";

    return nullptr;
  }

  auto const viewType =
    basics::VelocyPackHelper::getStringRef(definition, "type", "");
  auto const& dataSourceType =
    arangodb::LogicalDataSource::Type::emplace(viewType);
  auto const& viewFactory = viewTypes->factory(dataSourceType);

  if (!viewFactory) {
    LOG_TOPIC(ERR, Logger::VIEWS)
      << "Found view type for which there is no factory, type: "
      << viewType.toString();

    return nullptr;
  }

  auto view = viewFactory(vocbase, definition, planVersion);

  if (!view) {
    LOG_TOPIC(ERR, Logger::VIEWS)
      << "Failure to instantiate view of type: " << viewType.toString();

    return nullptr;
  }

  if (preCommit && !preCommit(view)) {
    LOG_TOPIC(ERR, Logger::VIEWS)
      << "Failure during pre-commit callback for view of type: "
      << viewType.toString();

    return nullptr;
  }

  auto res = view->create();

  if (!res.ok()) {
    LOG_TOPIC(ERR, Logger::VIEWS)
      << "Failure during commit of creation for view of type: "
      << viewType.toString();

    return nullptr;
  }

  return view;
}

// -----------------------------------------------------------------------------
// --SECTION--                                               DBServerLogicalView
// -----------------------------------------------------------------------------

DBServerLogicalView::DBServerLogicalView(
    TRI_vocbase_t* vocbase,
    VPackSlice const& definition,
    uint64_t planVersion
): LogicalView(vocbase, definition, planVersion) {
  TRI_ASSERT(!ServerState::instance()->isCoordinator());
}

DBServerLogicalView::~DBServerLogicalView() {
  if (deleted()) {
    StorageEngine* engine = EngineSelectorFeature::ENGINE;
    TRI_ASSERT(engine);

    engine->destroyView(vocbase(), this);
  }
}

arangodb::Result DBServerLogicalView::create() noexcept {
  TRI_ASSERT(!ServerState::instance()->isCoordinator());
  StorageEngine* engine = EngineSelectorFeature::ENGINE;
  TRI_ASSERT(engine);

  try {
    #ifdef ARANGODB_ENABLE_MAINTAINER_MODE
      // during recovery entry is being played back from the engine
      if (!engine->inRecovery()) {
        arangodb::velocypack::Builder builder;
        auto res = engine->getViews(vocbase(), builder);
        TRI_ASSERT(TRI_ERROR_NO_ERROR == res);
        auto slice  = builder.slice();
        TRI_ASSERT(slice.isArray());
        auto viewId = std::to_string(id());

        // We have not yet persisted this view
        for (auto entry: arangodb::velocypack::ArrayIterator(slice)) {
          auto id = arangodb::basics::VelocyPackHelper::getStringRef(
            entry, "id", arangodb::velocypack::StringRef()
          );

          if (!id.compare(viewId)) {
            return arangodb::Result(
              TRI_ERROR_ARANGO_DUPLICATE_IDENTIFIER,
              std::string("view id '") + viewId + "already exists in the sotrage engine"
            );
          }
        }
      }
    #endif

    engine->createView(vocbase(), id(), *this);

    return engine->persistView(vocbase(), *this);
  } catch (std::exception const& e) {
    return arangodb::Result(
      TRI_ERROR_INTERNAL,
      std::string("caught exception during storage engine persistance of view '") + name() + "': " + e.what()
    );
  } catch (...) {
    return arangodb::Result(
      TRI_ERROR_INTERNAL,
      std::string("caught exception during storage engine persistance of view '") + name() + "'"
    );
  }
}

arangodb::Result DBServerLogicalView::drop() {
  TRI_ASSERT(!ServerState::instance()->isCoordinator());
  StorageEngine* engine = EngineSelectorFeature::ENGINE;
  TRI_ASSERT(engine);
  auto res = dropImpl();

  if (res.ok()) {
    deleted(true);
    engine->dropView(vocbase(), this);
  }

  return res;
}

Result DBServerLogicalView::rename(std::string&& newName, bool doSync) {
  TRI_ASSERT(!ServerState::instance()->isCoordinator());
  StorageEngine* engine = EngineSelectorFeature::ENGINE;
  TRI_ASSERT(engine);

  auto oldName = name();

  try {
    name(std::move(newName));

    // store new view definition to disk
    if (!engine->inRecovery()) {
      engine->changeView(vocbase(), id(), *this, doSync);
    }
  } catch (basics::Exception const& ex) {
    name(std::move(oldName));

    return ex.code();
  } catch (...) {
    name(std::move(oldName));

    return TRI_ERROR_INTERNAL;
  }

  // write WAL 'rename' marker
  return engine->renameView(vocbase(), *this, oldName);
}

void DBServerLogicalView::toVelocyPack(
    velocypack::Builder &result,
    bool includeProperties,
    bool includeSystem
) const {
  TRI_ASSERT(!ServerState::instance()->isCoordinator());

  // We write into an open object
  TRI_ASSERT(result.isOpenObject());

  // Meta Information
  result.add("id", VPackValue(std::to_string(id())));
  result.add("name", VPackValue(name()));
  result.add("type", VPackValue(type().name()));

  if (includeSystem) {
    result.add("deleted", VPackValue(deleted()));

    // FIXME not sure if the following is relevant
    // Cluster Specific
    result.add("planId", VPackValue(std::to_string(planId())));

    // storage engine related properties
    StorageEngine* engine = EngineSelectorFeature::ENGINE;
    TRI_ASSERT(engine);
    engine->getViewProperties(vocbase(), this, result);
  }

  if (includeProperties) {
    // implementation Information
    result.add("properties", VPackValue(VPackValueType::Object));
    // note: includeSystem and forPersistence are not 100% synonymous,
    // however, for our purposes this is an okay mapping; we only set
    // includeSystem if we are persisting the properties
    getPropertiesVPack(result, includeSystem);
    result.close();
  }

  TRI_ASSERT(result.isOpenObject()); // We leave the object open
}

arangodb::Result DBServerLogicalView::updateProperties(
    VPackSlice const& slice,
    bool partialUpdate,
    bool doSync
) {
  TRI_ASSERT(!ServerState::instance()->isCoordinator());

  auto res = updateProperties(slice, partialUpdate);

  if (!res.ok()) {
    return res;
  }

  // after this call the properties are stored
  StorageEngine* engine = EngineSelectorFeature::ENGINE;
  TRI_ASSERT(engine);

  if (engine->inRecovery()) {
    return arangodb::Result(); // do not modify engine while in recovery
  }

  try {
    engine->changeView(vocbase(), id(), *this, doSync);
  } catch (arangodb::basics::Exception const& e) {
    return { e.code() };
  } catch (...) {
    return { TRI_ERROR_INTERNAL };
  }

  return {};
}

} // arangodb

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------
