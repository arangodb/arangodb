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

#include "Basics/ReadLocker.h"
#include "Basics/Result.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/WriteLocker.h"
#include "Cluster/ClusterInfo.h"
#include "Cluster/ServerState.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "StorageEngine/StorageEngine.h"
#include "VocBase/PhysicalView.h"
#include "VocBase/ticks.h"

using namespace arangodb;
using Helper = arangodb::basics::VelocyPackHelper;

namespace {

static TRI_voc_cid_t ReadId(VPackSlice info) {
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

static TRI_voc_cid_t ReadPlanId(VPackSlice info, TRI_voc_cid_t vid) {
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

/// @brief This the "copy" constructor used in the cluster
///        it is required to create objects that survive plan
///        modifications and can be freed
///        Can only be given to V8, cannot be used for functionality.
LogicalView::LogicalView(LogicalView const& other)
    : LogicalDataSource(other),
      _physical(other.getPhysical()->clone(this, other.getPhysical())) {
  TRI_ASSERT(_physical != nullptr);
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
        ReadId(info),
        ReadPlanId(info, 0),
        arangodb::basics::VelocyPackHelper::getStringValue(info, "name", ""),
        Helper::readBooleanValue(info, "deleted", false)
      ),
      _physical(EngineSelectorFeature::ENGINE->createPhysicalView(this, info)) {
  TRI_ASSERT(_physical != nullptr);
  if (!TRI_vocbase_t::IsAllowedName(info)) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_ILLEGAL_NAME);
  }

  // update server's tick value
  TRI_UpdateTickServer(static_cast<TRI_voc_tick_t>(id()));
}

LogicalView::~LogicalView() {}

/*static*/ LogicalDataSource::Category const& LogicalView::category() noexcept {
  static const Category category;

  return category;
}

Result LogicalView::rename(std::string&& newName, bool doSync) {
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

void LogicalView::drop() {
  deleted(true);

  if (getImplementation() != nullptr) {
    getImplementation()->drop();
  }

  TRI_ASSERT(!ServerState::instance()->isCoordinator());
  // StorageEngine* engine = EngineSelectorFeature::ENGINE;
  // engine->destroyView(_vocbase, this);

  _physical->drop();
}

void LogicalView::toVelocyPack(VPackBuilder& result, bool includeProperties,
                               bool includeSystem) const {
  // We write into an open object
  TRI_ASSERT(result.isOpenObject());
  // Meta Information
  result.add("id", VPackValue(std::to_string(id())));
  result.add("name", VPackValue(name()));
  result.add("type", VPackValue(type().name()));

  if (includeSystem) {
    result.add("deleted", VPackValue(deleted()));
    // Cluster Specific
    result.add("planId", VPackValue(std::to_string(planId())));

    if (getPhysical() != nullptr) {
      // Physical Information
      getPhysical()->getPropertiesVPack(result, includeSystem);
    }
  }

  if (includeProperties && (getImplementation() != nullptr)) {
    // implementation Information
    result.add("properties", VPackValue(VPackValueType::Object));
    // note: includeSystem and forPersistence are not 100% synonymous,
    // however, for our purposes this is an okay mapping; we only set
    // includeSystem if we are persisting the properties
    getImplementation()->getPropertiesVPack(result, includeSystem);
    result.close();
  }
  TRI_ASSERT(result.isOpenObject());
  // We leave the object open
}

arangodb::Result LogicalView::updateProperties(VPackSlice const& slice,
                                               bool partialUpdate,
                                               bool doSync) {
  WRITE_LOCKER(writeLocker, _infoLock);

  TRI_ASSERT(getImplementation() != nullptr);

  // the implementation may filter/change/react to the changes
  arangodb::Result implResult =
      getImplementation()->updateProperties(slice, partialUpdate, doSync);

  if (implResult.ok()) {
    // after this call the properties are stored
    StorageEngine* engine = EngineSelectorFeature::ENGINE;
    TRI_ASSERT(engine != nullptr);

    if (!engine->inRecovery()) {
      getPhysical()->persistProperties();
    }

    engine->changeView(vocbase(), id(), this, doSync);
  }

  return implResult;
}

/// @brief Persist the connected physical view
///        This should be called AFTER the view is successfully
///        created and only on Single/DBServer
void LogicalView::persistPhysicalView() {
  // Coordinators are not allowed to have local views!
  TRI_ASSERT(!ServerState::instance()->isCoordinator());

  // We have not yet persisted this view
  TRI_ASSERT(getPhysical()->path().empty());
  StorageEngine* engine = EngineSelectorFeature::ENGINE;
  engine->createView(vocbase(), id(), this);
}

void LogicalView::spawnImplementation(
    ViewCreator creator, arangodb::velocypack::Slice const& parameters,
    bool isNew) {
  _implementation = creator(this, parameters.get("properties"), isNew);
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------