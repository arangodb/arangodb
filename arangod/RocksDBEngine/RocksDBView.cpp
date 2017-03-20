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
#include "RocksDBEngine/RocksDBEngine.h"
//#include "RocksDB/RocksDBLogfileManager.h"
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
    : PhysicalView(logical, VPackSlice::emptyObjectSlice()) {
  throw std::logic_error("not implemented");
}

RocksDBView::~RocksDBView() {
  throw std::logic_error("not implemented");
}

void RocksDBView::getPropertiesVPack(velocypack::Builder& result,
                                     bool includeSystem) const {
  throw std::logic_error("not implemented");
}

void RocksDBView::open() {
  throw std::logic_error("not implemented");
}

void RocksDBView::drop() {
  throw std::logic_error("not implemented");
}

arangodb::Result RocksDBView::updateProperties(VPackSlice const& slice,
                                               bool doSync) {
  throw std::logic_error("not implemented");
  return arangodb::Result{};
}

arangodb::Result RocksDBView::persistProperties() {
  throw std::logic_error("not implemented");
  return arangodb::Result{};
}

PhysicalView* RocksDBView::clone(LogicalView* logical, PhysicalView* physical) {
  return new RocksDBView(logical, physical);
}
