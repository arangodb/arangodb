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
#include "Aql/PlanCache.h"
#include "Aql/QueryCache.h"
#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/FileUtils.h"
#include "Basics/PerformanceLogScope.h"
#include "Basics/ReadLocker.h"
#include "Basics/Result.h"
#include "Basics/StaticStrings.h"
#include "Basics/Timers.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/WriteLocker.h"
#include "Basics/encoding.h"
#include "Basics/process-utils.h"
#include "Cluster/ClusterMethods.h"
#include "Cluster/CollectionLockState.h"
#include "Logger/Logger.h"
#include "MMFiles/MMFilesCollectionReadLocker.h"
#include "MMFiles/MMFilesCollectionWriteLocker.h"
#include "MMFiles/MMFilesDatafile.h"
#include "MMFiles/MMFilesDatafileHelper.h"
#include "MMFiles/MMFilesDocumentOperation.h"
#include "MMFiles/MMFilesDocumentPosition.h"
#include "MMFiles/MMFilesIndexElement.h"
#include "MMFiles/MMFilesLogfileManager.h"
#include "MMFiles/MMFilesPrimaryIndex.h"
#include "MMFiles/MMFilesToken.h"
#include "MMFiles/MMFilesTransactionState.h"
#include "RestServer/DatabaseFeature.h"
#include "Scheduler/Scheduler.h"
#include "Scheduler/SchedulerFeature.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "StorageEngine/StorageEngine.h"
#include "Transaction/Helpers.h"
#include "Transaction/Methods.h"
#include "Utils/CollectionNameResolver.h"
#include "Utils/Events.h"
#include "Utils/OperationOptions.h"
#include "Utils/SingleCollectionTransaction.h"
#include "Transaction/StandaloneContext.h"
#include "VocBase/KeyGenerator.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/ticks.h"
#include "Indexes/IndexIterator.h"

using namespace arangodb;
using Helper = arangodb::basics::VelocyPackHelper;

arangodb::Result MMFilesView::updateProperties(VPackSlice const& slice,
                                               bool doSync) {
  return {};
}

arangodb::Result MMFilesView::persistProperties() noexcept {
  int res = TRI_ERROR_NO_ERROR;
  try {
    VPackBuilder infoBuilder;
    infoBuilder.openObject();
    _logicalView->toVelocyPack(infoBuilder);
    infoBuilder.close();

    MMFilesViewMarker marker(TRI_DF_MARKER_VPACK_CHANGE_VIEW, _logicalView->vocbase()->id(), _logicalView->id(), infoBuilder.slice());
    MMFilesWalSlotInfoCopy slotInfo =
        MMFilesLogfileManager::instance()->allocateAndWrite(marker, false);
    res = slotInfo.errorCode;
  } catch (arangodb::basics::Exception const& ex) {
    res = ex.code();
  } catch (...) {
    res = TRI_ERROR_INTERNAL;
  }

  if (res != TRI_ERROR_NO_ERROR) {
    // TODO: what to do here
    LOG_TOPIC(WARN, arangodb::Logger::FIXME) << "could not save collection view marker in log: "
              << TRI_errno_string(res);
    return {res, TRI_errno_string(res)};
  }
  return {};
}

PhysicalView* MMFilesView::clone(LogicalView* logical, PhysicalView* physical){
  return new MMFilesView(logical, physical);
}

MMFilesView::MMFilesView(LogicalView* view,
                         VPackSlice const& info)
    : PhysicalView(view, info) {
  auto pathSlice = info.get("path");
  if (pathSlice.isString()) {
    _path = pathSlice.copyString();
  }
}

MMFilesView::MMFilesView(LogicalView* logical,
                         PhysicalView* physical)
    : PhysicalView(logical, VPackSlice::emptyObjectSlice()) {}

MMFilesView::~MMFilesView() {}

void MMFilesView::getPropertiesVPack(velocypack::Builder& result) const {
  TRI_ASSERT(result.isOpenObject());
  result.add("path", VPackValue(_path));
  TRI_ASSERT(result.isOpenObject());
}
  
/// @brief opens an existing view
void MMFilesView::open(bool ignoreErrors) {}

void MMFilesView::drop() {}
