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

static std::string const ReadStringValue(VPackSlice info, std::string const& name,
                                         std::string const& def) {
  if (!info.isObject()) {
    return def;
  }
  return Helper::getStringValue(info, name, def);
}
}  // namespace

/// @brief This the "copy" constructor used in the cluster
///        it is required to create objects that survive plan
///        modifications and can be freed
///        Can only be given to V8, cannot be used for functionality.
LogicalView::LogicalView(LogicalView const& other)
    : _id(other.id()),
      _planId(other.planId()),
      _type(other.type()),
      _name(other.name()),
      _isDeleted(other._isDeleted),
      _vocbase(other.vocbase()),
      _physical(other.getPhysical()->clone(this, other.getPhysical())) {
  TRI_ASSERT(_physical != nullptr);
}

// @brief Constructor used in coordinator case.
// The Slice contains the part of the plan that
// is relevant for this view
LogicalView::LogicalView(TRI_vocbase_t* vocbase, VPackSlice const& info)
    : _id(ReadId(info)),
      _planId(ReadPlanId(info, _id)),
      _type(ReadStringValue(info, "type", "")),
      _name(ReadStringValue(info, "name", "")),
      _isDeleted(Helper::readBooleanValue(info, "deleted", false)),
      _vocbase(vocbase),
      _physical(EngineSelectorFeature::ENGINE->createPhysicalView(this, info)) {
  TRI_ASSERT(_physical != nullptr);
  if (!IsAllowedName(info)) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_ILLEGAL_NAME);
  }

  // update server's tick value
  TRI_UpdateTickServer(static_cast<TRI_voc_tick_t>(_id));
}

LogicalView::~LogicalView() {}

bool LogicalView::IsAllowedName(VPackSlice parameters) {
  std::string name = ReadStringValue(parameters, "name", "");
  return IsAllowedName(name);
}

bool LogicalView::IsAllowedName(std::string const& name) {
  bool ok;
  char const* ptr;
  size_t length = 0;

  // check allow characters: must start with letter or underscore if system is
  // allowed
  for (ptr = name.c_str(); *ptr; ++ptr) {
    if (length == 0) {
      ok = (*ptr == '_') || ('a' <= *ptr && *ptr <= 'z') || ('A' <= *ptr && *ptr <= 'Z');
    } else {
      ok = (*ptr == '_') || (*ptr == '-') || ('0' <= *ptr && *ptr <= '9') ||
           ('a' <= *ptr && *ptr <= 'z') || ('A' <= *ptr && *ptr <= 'Z');
    }

    if (!ok) {
      return false;
    }

    ++length;
  }

  // invalid name length
  if (length == 0 || length > TRI_COL_NAME_LENGTH) {
    return false;
  }

  return true;
}

TRI_voc_cid_t LogicalView::planId() const { return _planId; }

std::string LogicalView::name() const {
  // TODO Activate this lock. Right now we have some locks outside.
  // READ_LOCKER(readLocker, _lock);
  return _name;
}

std::string LogicalView::dbName() const {
  TRI_ASSERT(_vocbase != nullptr);
  return _vocbase->name();
}

bool LogicalView::deleted() const { return _isDeleted; }

void LogicalView::setDeleted(bool newValue) { _isDeleted = newValue; }

void LogicalView::drop() {
  _isDeleted = true;

  if (getImplementation() != nullptr) {
    getImplementation()->drop();
  }

  TRI_ASSERT(!ServerState::instance()->isCoordinator());
  // StorageEngine* engine = EngineSelectorFeature::ENGINE;
  // engine->destroyView(_vocbase, this);

  _physical->drop();
}

VPackBuilder LogicalView::toVelocyPack(bool includeProperties, bool includeSystem) const {
  VPackBuilder builder;
  builder.openObject();
  toVelocyPack(builder, includeProperties, includeSystem);
  builder.close();
  return builder;
}

void LogicalView::toVelocyPack(VPackBuilder& result, bool includeProperties,
                               bool includeSystem) const {
  // We write into an open object
  TRI_ASSERT(result.isOpenObject());
  // Meta Information
  result.add("id", VPackValue(std::to_string(_id)));
  result.add("name", VPackValue(_name));
  result.add("type", VPackValue(_type));
  if (includeSystem) {
    result.add("deleted", VPackValue(_isDeleted));
    // Cluster Specific
    result.add("planId", VPackValue(std::to_string(_planId)));
    if (getPhysical() != nullptr) {
      // Physical Information
      getPhysical()->getPropertiesVPack(result, includeSystem);
    }
  }

  if (includeProperties && (getImplementation() != nullptr)) {
    // implementation Information
    result.add("properties", VPackValue(VPackValueType::Object));
    getImplementation()->getPropertiesVPack(result);
    result.close();
  }
  TRI_ASSERT(result.isOpenObject());
  // We leave the object open
}

arangodb::Result LogicalView::updateProperties(VPackSlice const& slice,
                                               bool partialUpdate, bool doSync) {
  WRITE_LOCKER(writeLocker, _infoLock);

  TRI_ASSERT(getImplementation() != nullptr);

  // the implementation may filter/change/react to the changes
  arangodb::Result implResult =
      getImplementation()->updateProperties(slice, partialUpdate, doSync);

  if (implResult.ok()) {
    // after this call the properties are stored
    getPhysical()->persistProperties();

    StorageEngine* engine = EngineSelectorFeature::ENGINE;
    engine->changeView(_vocbase, _id, this, doSync);
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
  engine->createView(_vocbase, _id, this);
}

void LogicalView::spawnImplementation(ViewCreator creator,
                                      arangodb::velocypack::Slice const& parameters,
                                      bool isNew) {
  _implementation = creator(this, parameters, isNew);
}
