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

#include "RocksDBView.h"
#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/FileUtils.h"
#include "Basics/ReadLocker.h"
#include "Basics/Result.h"
#include "Basics/WriteLocker.h"
#include "Logger/Logger.h"
#include "RocksDBEngine/RocksDBCommon.h"
#include "RocksDBEngine/RocksDBColumnFamily.h"
#include "RocksDBEngine/RocksDBEngine.h"
#include "RocksDBEngine/RocksDBKey.h"
#include "RocksDBEngine/RocksDBValue.h"
#include "RestServer/DatabaseFeature.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "StorageEngine/StorageEngine.h"
#include "Utils/Events.h"
#include "VocBase/ticks.h"

using namespace arangodb;

static std::string ReadPath(VPackSlice info) {
  if (info.isObject()) {
    VPackSlice path = info.get("path");
    if (path.isString()) {
      return path.copyString();
    }
  }
  return "";
}

RocksDBView::RocksDBView(LogicalView* view, VPackSlice const& info)
    : PhysicalView(view, info) {
  std::string path = ReadPath(info);
  if (!path.empty()) {
    setPath(path);
  }
}

RocksDBView::RocksDBView(LogicalView* logical, PhysicalView* physical)
    : PhysicalView(logical, VPackSlice::emptyObjectSlice()) {}

RocksDBView::~RocksDBView() {}

void RocksDBView::getPropertiesVPack(velocypack::Builder& result,
                                     bool includeSystem) const {
  TRI_ASSERT(result.isOpenObject());
  if (includeSystem) {
    result.add("path", VPackValue(_path));
  }
  TRI_ASSERT(result.isOpenObject());
}

void RocksDBView::open() {}

void RocksDBView::drop() {
  auto db = rocksutils::globalRocksDB();
  RocksDBKey key;
  key.constructView(_logicalView->vocbase()->id(), _logicalView->id());

  rocksdb::WriteOptions options;  // TODO: check which options would make sense
  auto status = rocksutils::convertStatus(
      db->Delete(options, RocksDBColumnFamily::definitions(), key.string()));
  if (!status.ok()) {
    THROW_ARANGO_EXCEPTION(status.errorNumber());
  }
}

arangodb::Result RocksDBView::updateProperties(VPackSlice const& slice,
                                               bool doSync) {
  // nothing to do here
  return arangodb::Result{};
}

arangodb::Result RocksDBView::persistProperties() {
  auto db = rocksutils::globalRocksDB();
  RocksDBKey key;
  key.constructView(_logicalView->vocbase()->id(), _logicalView->id());

  VPackBuilder infoBuilder;
  infoBuilder.openObject();
  _logicalView->toVelocyPack(infoBuilder, true, true);
  infoBuilder.close();
  auto value = RocksDBValue::View(infoBuilder.slice());

  rocksdb::WriteOptions options;  // TODO: check which options would make sense
  rocksdb::Status res = db->Put(options, RocksDBColumnFamily::definitions(),
                                key.string(), value.string());

  return rocksutils::convertStatus(res);
}

PhysicalView* RocksDBView::clone(LogicalView* logical, PhysicalView* physical) {
  return new RocksDBView(logical, physical);
}
