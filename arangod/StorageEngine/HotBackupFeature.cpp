////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2018 ArangoDB GmbH, Cologne, Germany
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
/// @author Kaveh Vahedipour
////////////////////////////////////////////////////////////////////////////////

#include "HotBackupFeature.h"

#include "Basics/FileUtils.h"
#include "Basics/Result.h"
#include "Cluster/ServerState.h"
#include "Logger/Logger.h"
#include "ProgramOptions/ProgramOptions.h"
#include "ProgramOptions/Section.h"
#include "IResearch/IResearchView.h"
#include "RestServer/DatabaseFeature.h"
#include "RestServer/ViewTypesFeature.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "RocksDBEngine/RocksDBEngine.h"
#include "Scheduler/Scheduler.h"
#include "Scheduler/SchedulerFeature.h"
#include "VocBase/LogicalView.h"

using namespace arangodb::application_features;
using namespace arangodb::basics;
using namespace arangodb::options;
using namespace arangodb;

namespace {

void removeAllArangoSearchDataForDatabase(TRI_vocbase_t& vocbase) {
  for (auto& view : vocbase.views()) {
    if (!arangodb::LogicalView::cast<arangodb::iresearch::IResearchView>(view.get())) {
      continue;  // not an IResearchView
    }

    auto res = view->drop();  // drop view (including all links)

    if (!res.ok()) {
      LOG_TOPIC(WARN, Logger::BACKUP)
          << "failure to drop view while dropping all ArangoSearch data "
             "for restore operation, message: " << res.errorMessage();
      // continue anyway...
    }
  }
}

bool recreateArangoSearchDataForDatabase(TRI_vocbase_t& vocbase) {
  if (!arangodb::ServerState::instance()->isSingleServer()) {
    return true;  // not applicable for other ServerState roles
  }

  bool success = true;

  for (auto& view : vocbase.views()) {
    LOG_TOPIC(INFO, Logger::BACKUP)
      << "Recreating ArangoSearch index: doing view " << view->name();
    if (!arangodb::LogicalView::cast<arangodb::iresearch::IResearchView>(view.get())) {
      continue;  // not an IResearchView
    }

    arangodb::velocypack::Builder builder;
    arangodb::Result res;

    builder.openObject();
    res = view->properties(builder, arangodb::LogicalDataSource::makeFlags(
                                        arangodb::LogicalDataSource::Serialize::Detailed));  // get JSON with end-user definition
    builder.close();

    if (!res.ok()) {
      LOG_TOPIC(ERR, Logger::BACKUP)
          << "failure to generate persisted definition while recreating "
             "ArangoSearch index after a restore";

      success = false;
      continue;
    }

    res = view->drop();  // drop view (including all links)

    if (!res.ok()) {
      LOG_TOPIC(WARN, Logger::BACKUP)
          << "failure to drop view while recreating ArangoSearch index "
             "after a restore";

      success = false;
      continue;
    }

    // recreate view
    res = arangodb::iresearch::IResearchView::factory().create(view, vocbase,
                                                               builder.slice());

    if (!res.ok()) {
      LOG_TOPIC(ERR, Logger::BACKUP)
          << "failure to recreate view while recreating ArangoSearch "
             "index after a restore, error: "
          << res.errorNumber() << " " << res.errorMessage()
          << ", view definition: " << builder.slice().toString();

      success = false;
    }
  }

  return success;
}

/// @brief returns true if and only if the current restart of the server
/// is one from a hotbackup restore. This essentially tests existence of
/// a file called "RESTORE" in the database directory.
bool isRestoreStart() {
  StorageEngine* engine = EngineSelectorFeature::ENGINE;
  std::string path = arangodb::basics::FileUtils::buildFilename(
      engine->dataPath(), "RESTORE");
  return arangodb::basics::FileUtils::exists(path);
}

/// @brief removes the restore start marker "RESTORE", such that the next
/// startup will be a non-restore startup.
void removeRestoreStartMarker() {
  StorageEngine* engine = EngineSelectorFeature::ENGINE;
  std::string path = arangodb::basics::FileUtils::buildFilename(
      engine->dataPath(), "RESTORE");
  bool res = arangodb::basics::FileUtils::remove(path);
  if (res == false) {
    LOG_TOPIC("54feb", INFO, arangodb::Logger::BACKUP)
      << "Could not remove RESTORE start marker.";
  }
}

void recreateArangoSearchViewsAfterRestore() {
  LOG_TOPIC(INFO, Logger::BACKUP)
    << "Recreating ArangoSearch indexes...";
  DatabaseFeature::DATABASE->enumerateDatabases(
    [](TRI_vocbase_t& vocbase) {
      LOG_TOPIC(INFO, Logger::BACKUP)
        << "Recreating ArangoSearch index for database " << vocbase.name();
      bool res = recreateArangoSearchDataForDatabase(vocbase);
      LOG_TOPIC(INFO, Logger::BACKUP)
        << "Done recreating ArangoSearch index for database " << vocbase.name()
        << ", result was: " << (res ? "GOOD" : "BAD");
    });
  // And remove the RESTORE marker:
  removeRestoreStartMarker();
}

void scheduleRecreateArangoSearchViewsAfterRestore() {
  LOG_TOPIC(INFO, Logger::BACKUP)
    << "This is a restore start of a single server, we need to recreate "
       "all ArangoSearch indexes in the background, scheduling...";
  SchedulerFeature::SCHEDULER->queue(RequestPriority::LOW,
                                     recreateArangoSearchViewsAfterRestore,
                                     false);
}

}

namespace arangodb {

HotBackupFeature::HotBackupFeature(application_features::ApplicationServer& server)
    : ApplicationFeature(server, "HotBackup"), _backupEnabled(false) {
  setOptional(true);
  startsAfter("Upgrade");
  startsAfter("IResearchFeature");
  startsAfter("DatabasePhase");
  startsBefore("GeneralServer");
}

HotBackupFeature::~HotBackupFeature() {}

void HotBackupFeature::collectOptions(std::shared_ptr<ProgramOptions> options) {
  options->addOption("--backup.api-enabled",
                     "whether the backup api is enabled or not",
                     new BooleanParameter(&_backupEnabled));
}

void HotBackupFeature::validateOptions(std::shared_ptr<ProgramOptions> options) {}

void HotBackupFeature::prepare() {
  if (isAPIEnabled()) {
    // enabled sha file creation
    RocksDBEngine* rocksdb =
    application_features::ApplicationServer::getFeature<RocksDBEngine>(
          RocksDBEngine::FeatureName);

    if (rocksdb) {
      rocksdb->setCreateShaFiles(true);
    }
  }
}

void HotBackupFeature::start() {
  // Potentially recreate all ArangoSearch indexes if this is a single
  // server and we are performing a RESTORE restart:
  if (ServerState::instance()->isSingleServer()) {
    if (::isRestoreStart()) {
      ::scheduleRecreateArangoSearchViewsAfterRestore();
    }
  }
}

void HotBackupFeature::beginShutdown() {
  // Call off uploads / downloads?
}

void HotBackupFeature::stop() {}

void HotBackupFeature::unprepare() {}


// create a new transfer record. lock must be held by caller
arangodb::Result HotBackupFeature::createTransferRecordNoLock (
  std::string const& operation, std::string const& remote,
  BackupID const& backupId, TransferID const& transferId,
  std::string const& status) {

  _clipBoardMutex.assertLockedByCurrentThread();

  if (_ongoing.find(backupId) != _ongoing.end()) {
    return arangodb::Result(TRI_ERROR_BAD_PARAMETER,
        "For the given backupId there is already a transfer in progress!");
  }

  // if no such transfer exists create a new one
  if (_clipBoard.find(transferId) != _clipBoard.end()) {
    return arangodb::Result(TRI_ERROR_BAD_PARAMETER,
      "A transfer with the given transferId is already in progress");
  }

  _clipBoard.insert(std::make_pair(transferId, TransferStatus(backupId, operation, remote, status)));
  if (!TransferStatus::isCompletedStatus(status)) {
    _ongoing.emplace(backupId, transferId);
  }

  // We do the cleanup of the _clipBoard here, if there are more than 150
  // entries in it. If so, we start from the end and go backwards, we keep
  // 100 completed ones and erase all other completed ones before that:
  if (_clipBoard.size() > 150) {
    int count = 0;
    std::vector<TransferID> toBeDeleted;
    for (auto it = _clipBoard.rbegin(); it != _clipBoard.rend(); ++it) {
      if (TransferStatus::isCompletedStatus(it->second.status)) {
        ++count;
        if (count > 100) {
          toBeDeleted.push_back(it->first);
        }
      }
    }
    for (auto const& id : toBeDeleted) {
      _clipBoard.erase(id);
    }
  }
  return arangodb::Result();
}


// note new transfer status string to record
arangodb::Result HotBackupFeature::noteTransferRecord (
  std::string const& operation, std::string const& backupId,
  std::string const& transferId, std::string const& status,
  std::string const& remote) {

  // if such transfer with id is found, add status to it.
  // else create a new transfer record

  arangodb::Result res;
  MUTEX_LOCKER(guard, _clipBoardMutex);

  auto const& t = _clipBoard.find(transferId);

  if (t != _clipBoard.end()) {

    TransferStatus& ts = t->second;

    if (ts.isCompleted()) {
      return arangodb::Result(
        TRI_ERROR_HTTP_FORBIDDEN,
        std::string("Transfer with id ") + transferId + " has already ended");
    }

    ts.status = status;

  } else {
    if (remote.empty()) {
      res.reset(
        TRI_ERROR_HTTP_NOT_FOUND, std::string("No transfer with id ") + transferId);
    } else {
      res = createTransferRecordNoLock(operation, remote, backupId, transferId, status);
    }
  }

  return res;

}


// add new transfer progress to record
arangodb::Result HotBackupFeature::noteTransferRecord (
  std::string const& operation, std::string const& backupId,
  std::string const& transferId, size_t const& done, size_t const& total) {

  MUTEX_LOCKER(guard, _clipBoardMutex);

  auto t = _clipBoard.find(transferId);

  // note transfer progress only if job has not failed / finished / cancelled

  if (t == _clipBoard.end()) {
    return arangodb::Result(
      TRI_ERROR_HTTP_NOT_FOUND, std::string("No ongoing transfer with id ") + transferId);
  }
  TransferStatus& ts = t->second;
  if (ts.isCompleted()) {
    return arangodb::Result(
      TRI_ERROR_HTTP_FORBIDDEN,
      std::string("Transfer with id ") + transferId + " has already finished");
  }
  ts.done = done;
  ts.total = total;
  ts.timeStamp = timepointToString(std::chrono::system_clock::now());
  return arangodb::Result();

}

// add new final result to transfer record
arangodb::Result HotBackupFeature::noteTransferRecord (
  std::string const& operation, std::string const& backupId,
  std::string const& transferId, arangodb::Result const& result) {

  MUTEX_LOCKER(guard, _clipBoardMutex);
  auto t = _clipBoard.find(transferId);

  if (t == _clipBoard.end()) {
    return arangodb::Result(
      TRI_ERROR_HTTP_NOT_FOUND, std::string("No transfer with id ") + transferId);
  }

  TransferStatus& ts = t->second;

  if (ts.isCompleted()) {
    return arangodb::Result(
      TRI_ERROR_HTTP_FORBIDDEN,
      std::string("Transfer with id ") + transferId + " has already ended");
  }

  // Last status
  if (result.ok()) {
    ts.status = "COMPLETED";
  } else {
    ts.errorMessage = result.errorMessage();
    ts.errorNumber = result.errorNumber();
    ts.status = "FAILED";
  }
  _ongoing.erase(ts.backupId);

  return arangodb::Result();
}


arangodb::Result HotBackupFeature::getTransferRecord(
  TransferID const& id, VPackBuilder& report) const {

  if (!report.isEmpty()) {
    report.clear();
  }

  // Get transfer record.
  // Report last entry in _clipboard/progress or next to last in archive
  // If transfer is still in _clipboard, it is still ongoing. We can report last status or progress.
  // Else we need to find the next to last message if failed 

  MUTEX_LOCKER(guard, _clipBoardMutex);

  auto t = _clipBoard.find(id);
  if (t == _clipBoard.end()) {
    return arangodb::Result(
      TRI_ERROR_HTTP_NOT_FOUND, std::string("No transfer with id ") + id);
  }

  TransferStatus const& ts = t->second;

  {
    VPackObjectBuilder r(&report);
    report.add("Timestamp", VPackValue(ts.started));
    report.add(
      (ts.operation == "Upload") ? "UploadId" : "DownloadId",
      VPackValue(t->first));
    report.add("Cancelled", VPackValue(ts.status == "CANCELLED"));
    report.add("BackupId", VPackValue(ts.backupId));
    report.add(VPackValue("DBServers"));
    {
      VPackObjectBuilder dbservers(&report);
      report.add(VPackValue("SNGL"));
      {
        VPackObjectBuilder sngl(&report);
        report.add("Status", VPackValue(ts.status));
        if (ts.total != 0) {
          report.add(VPackValue("Progress"));
          {
            VPackObjectBuilder o(&report);
            report.add("Total", VPackValue(ts.total));
            report.add("Done", VPackValue(ts.done));
            report.add("Time", VPackValue(ts.timeStamp));
          }
        }
        if (ts.status == "FAILED") {
          report.add("Error", VPackValue(ts.errorNumber));
          report.add("ErrorMessage", VPackValue(ts.errorMessage));
        }
      }
    }
  }

  return arangodb::Result();
}

// cancel a transfer 
arangodb::Result HotBackupFeature::cancel(std::string const& transferId) {

  // If not alredy otherwise done, cancel the job by adding last entry
  
  MUTEX_LOCKER(guard, _clipBoardMutex);
  auto t = _clipBoard.find(transferId);

  if (t == _clipBoard.end()) {
    return Result(
      TRI_ERROR_HTTP_NOT_FOUND,
      std::string("cancellation failed: no transfer with id ") + transferId);
  }

  TransferStatus& ts = t->second;

  if (ts.isCompleted()) {
    return Result(
      TRI_ERROR_HTTP_FORBIDDEN,
      std::string("Transfer with id ") + transferId + " has already been completed");
  }

  ts.status = "CANCELLED";
  return arangodb::Result();

}


// Check if we have been cancelled asynchronously
bool HotBackupFeature::cancelled(std::string const& transferId) const {

  MUTEX_LOCKER(guard, _clipBoardMutex);
  auto t = _clipBoard.find(transferId);

  if (t != _clipBoard.end()) {
    TransferStatus const& ts = t->second;
    return ts.status == "CANCELLED";
  }

  return false;
}

void HotBackupFeature::removeAllArangoSearchData() {
  if (!arangodb::ServerState::instance()->isSingleServer() &&
      !arangodb::ServerState::instance()->isDBServer()) {
    return;   // not applicable for other ServerState roles
  }

  DatabaseFeature::DATABASE->enumerateDatabases(::removeAllArangoSearchDataForDatabase);
}

}  // namespace arangodb

