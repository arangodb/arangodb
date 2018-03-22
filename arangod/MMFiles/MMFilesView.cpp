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

#include "MMFilesView.h"
#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/FileUtils.h"
#include "Basics/ReadLocker.h"
#include "Basics/Result.h"
#include "Basics/WriteLocker.h"
#include "Logger/Logger.h"
#include "MMFiles/MMFilesEngine.h"
#include "MMFiles/MMFilesLogfileManager.h"
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

MMFilesView::MMFilesView(LogicalView* view, VPackSlice const& info)
    : PhysicalView(view, info) {
  std::string path = ReadPath(info);
  if (!path.empty()) {
    setPath(path);
  }
}

MMFilesView::MMFilesView(LogicalView* logical, PhysicalView* physical)
    : PhysicalView(logical, VPackSlice::emptyObjectSlice()) {}

MMFilesView::~MMFilesView() {
  if (_logicalView->deleted()) {
    try {
      StorageEngine* engine = EngineSelectorFeature::ENGINE;
      std::string directory =
          static_cast<MMFilesEngine*>(engine)->viewDirectory(
              _logicalView->vocbase()->id(), _logicalView->id());
      TRI_RemoveDirectory(directory.c_str());
    } catch (...) {
      // must ignore errors here as we are in the destructor
    }
  }
}

void MMFilesView::getPropertiesVPack(velocypack::Builder& result,
                                     bool includeSystem) const {
  TRI_ASSERT(result.isOpenObject());
  if (includeSystem) {
    result.add("path", VPackValue(_path));
  }
  TRI_ASSERT(result.isOpenObject());
}

PhysicalView* MMFilesView::clone(LogicalView* logical, PhysicalView* physical) {
  return new MMFilesView(logical, physical);
}
