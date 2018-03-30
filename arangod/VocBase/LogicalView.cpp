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

using namespace arangodb;
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

}  // namespace

/*static*/ TRI_voc_cid_t LogicalView::readViewId(VPackSlice info) {
  if (!info.isObject()) {
    // ERROR CASE
    return 0;
  }

  // Somehow the id is now propagated to dbservers
  TRI_voc_cid_t id = Helper::extractIdValue(info);

  if (id == 0) {
    if (ServerState::instance()->isDBServer()) {
      id = ClusterInfo::instance()->uniqid(1);
    } else if (ServerState::instance()->isCoordinator()) {
      id = ClusterInfo::instance()->uniqid(1);
    } else {
      id = TRI_NewTickServer();
    }
  }
  return id;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       LogicalView
// -----------------------------------------------------------------------------

/*static*/ std::shared_ptr<LogicalView> LogicalView::create(
    TRI_vocbase_t& vocbase,
    velocypack::Slice definition,
    bool isNew
) {
  auto const* viewTypes = application_features::ApplicationServer::getFeature
      <ViewTypesFeature>("ViewTypes");
  TRI_ASSERT(viewTypes);

  auto const viewType = arangodb::basics::VelocyPackHelper::getStringRef(
    definition, "type", ""
  );

  auto const& dataSourceType = arangodb::LogicalDataSource::Type::emplace(
    viewType
  );

  auto const& viewFactory = viewTypes->factory(dataSourceType);

  if (!viewFactory) {
    LOG_TOPIC(ERR, Logger::VIEWS)
      << "Found view type for which there is no factory, type: "
      << viewType.toString();
    return nullptr;
  }

  return viewFactory(vocbase, definition, isNew);
}

/*static*/ LogicalDataSource::Category const& LogicalView::category() noexcept {
  static const Category category;

  return category;
}

// @brief Constructor used in coordinator case.
// The Slice contains the part of the plan that
// is relevant for this view
LogicalView::LogicalView(TRI_vocbase_t* vocbase, VPackSlice const& info)
    : LogicalDataSource(
        category(),
        LogicalDataSource::Type::emplace(
          arangodb::basics::VelocyPackHelper::getStringRef(info, "type", "")
        ),
        vocbase,
        LogicalView::readViewId(info),
        ReadPlanId(info, 0),
        arangodb::basics::VelocyPackHelper::getStringValue(info, "name", ""),
        Helper::readBooleanValue(info, "deleted", false)
      ) {
  if (!TRI_vocbase_t::IsAllowedName(info)) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_ILLEGAL_NAME);
  }

  // update server's tick value
  TRI_UpdateTickServer(static_cast<TRI_voc_tick_t>(id()));
}

// -----------------------------------------------------------------------------
// --SECTION--                                               DBServerLogicalView
// -----------------------------------------------------------------------------

DBServerLogicalView::DBServerLogicalView(
    TRI_vocbase_t* vocbase,
    VPackSlice const& info,
    bool isNew
) : LogicalView(vocbase, info),
    _isNew(isNew) {
}

DBServerLogicalView::~DBServerLogicalView() {
  if (deleted()) {
    StorageEngine* engine = EngineSelectorFeature::ENGINE;
    TRI_ASSERT(engine);
    engine->destroyView(vocbase(), this);
  }
}

void DBServerLogicalView::open() {
  // Coordinators are not allowed to have local views!
  TRI_ASSERT(!ServerState::instance()->isCoordinator());

  if (!_isNew) {
    return;
  }

  StorageEngine* engine = EngineSelectorFeature::ENGINE;
  TRI_ASSERT(engine);
  engine->createView(vocbase(), id(), this);
  _isNew = false;
}

void DBServerLogicalView::drop() {
  TRI_ASSERT(!ServerState::instance()->isCoordinator());
  StorageEngine* engine = EngineSelectorFeature::ENGINE;
  TRI_ASSERT(engine);
  engine->dropView(vocbase(), this);
}

Result DBServerLogicalView::rename(std::string&& newName, bool doSync) {
  auto oldName = name();

  try {
    StorageEngine* engine = EngineSelectorFeature::ENGINE;
    TRI_ASSERT(engine != nullptr);

    name(std::move(newName));

    if (!engine->inRecovery()) {
      engine->changeView(vocbase(), id(), this, doSync);
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

void DBServerLogicalView::toVelocyPack(
    velocypack::Builder &result,
    bool /*includeProperties*/,
    bool includeSystem
) const {
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
    TRI_ASSERT(engine );
    engine->getViewProperties(vocbase(), this, result);
  }
}

arangodb::Result DBServerLogicalView::updateProperties(
    VPackSlice const& /*slice*/,
    bool /*partialUpdate*/,
    bool doSync
) {
  // after this call the properties are stored
  StorageEngine* engine = EngineSelectorFeature::ENGINE;
  TRI_ASSERT(engine);

  try {
    engine->changeView(vocbase(), id(), this, doSync);
  } catch (arangodb::basics::Exception const& e) {
    return { e.code() };
  } catch (...) {
    return { TRI_ERROR_INTERNAL };
  }

  return {};
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------
