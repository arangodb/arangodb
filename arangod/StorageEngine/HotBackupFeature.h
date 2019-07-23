////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2018 ArangoDB GmbH, Cologne, Germany
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

#ifndef ARANGOD_STORAGE_ENGINE_HOT_BACKUP_FEATURE_H
#define ARANGOD_STORAGE_ENGINE_HOT_BACKUP_FEATURE_H 1

#include "ApplicationFeatures/ApplicationFeature.h"

#include "Agency/TimeString.h"
#include "Basics/Mutex.h"
#include "Scheduler/Scheduler.h"

#include <map>

namespace arangodb {

class HotBackupFeature : virtual public application_features::ApplicationFeature {
public:

  explicit HotBackupFeature(application_features::ApplicationServer& server);
  ~HotBackupFeature();

  void collectOptions(std::shared_ptr<options::ProgramOptions>) override final;
  void validateOptions(std::shared_ptr<options::ProgramOptions>) override final;
  void prepare() override final;
  void start() override final;
  void beginShutdown() override final;
  void stop() override final;
  void unprepare() override final;

  using TransferID = std::string;
  using BackupID = std::string;
  using TimeStamp = std::string;
 
 private:

  /**
   * @brief create a new transfer record
   * @param  operation   upload / download
   * @param  remote      remote address
   * @param  backupId    backup id
   * @param  transferId  transfer id
   * @return             result
   */
  arangodb::Result createTransferRecordNoLock(
    std::string const& operation, std::string const& remote,
    BackupID const& backupId, TransferID const& transferId,
    std::string const& status);

 public:
  /**
   * @brief  update change to transfer record with status string
   * @param  operation   upload / download
   * @param  backupId    backup id
   * @param  transferId  transfer id
   * @param  status      status string
   * @return             result
   */
  arangodb::Result noteTransferRecord(
    std::string const& operation, BackupID const& backupId,
    TransferID const& transferId, std::string const& status,
    std::string const& remote);

  /**
   * @brief  update change to transfer record with progress counters
   * @param  operation   upload / download
   * @param  backupId    backup id
   * @param  transferId  transfer id
   * @param  done        number done
   * @param  total       total number to be done
   * @return             result
   */
  arangodb::Result noteTransferRecord(
    std::string const& operation, BackupID const& backupId,
    TransferID const& transferId, size_t const& done, size_t const& total);

  /**
   * @brief  final entry in transfer record and move to archive
   * @param  operation   upload / download
   * @param  backupId    backup id
   * @param  transferId  transfer id
   * @return             result
   */
  arangodb::Result noteTransferRecord(
    std::string const& operation, BackupID const& backupId,
    TransferID const& transferId, arangodb::Result const& result);
  
  /**
   * @brief  get transfer record
   * @param  id          transfer id
   * @param  reports     container for reporting
   * @return             result
   */
  arangodb::Result getTransferRecord(
    TransferID const& id, VPackBuilder& reports) const;

  /**
   * @brief asynchronously cancel the transfer
   * @param  transferId  transfer id
   * @param              result
   */
  arangodb::Result cancel (TransferID const& transferId);

  /**
   * @brief check if job has been cancelled in meantime
   * @param  transferId  transfer id
   */
  bool cancelled (TransferID const& transferId) const;

private:
  
  mutable arangodb::Mutex _clipBoardMutex; // lock for all _clipBoard and _ongoing

  struct TransferStatus {
    BackupID backupId;
    std::string operation;
    std::string remote;
    std::string status;  // can be ACK, STARTED, COMPLETED, FAILED or CANCELLED
    int errorNumber;
    std::string errorMessage;
    size_t done;
    size_t total;
    TimeStamp started;     // start of transfer
    TimeStamp timeStamp;   // last update

    TransferStatus();
    TransferStatus(std::string const& backupId, std::string const& op, std::string const& remote, std::string const& status)
      : backupId(backupId), operation(op), remote(remote), status(status),
        errorNumber(0), done(0), total(0),
        started(timepointToString(std::chrono::system_clock::now())) {
    }

    static bool isCompletedStatus(std::string const& s) {
      return s != "ACK" && s != "STARTED";
    }

    bool isCompleted() const {
      return isCompletedStatus(status);
    }
  };

  // This is the central place that tracks operations. It contains both ongoing
  // and older operations, but only ever up to 100 completed operations.
  std::map<TransferID, TransferStatus> _clipBoard;
  // This is the index from backupId to transferId and contains only ongoing
  // operations. It is basically used to make sure that at the same time only
  // one operation is happening for the same backup snapshot.
  std::map<BackupID, TransferID> _ongoing;

  bool _backupEnabled;

  // The hotbackup feature can at any given time have up to one lock cleaner
  // which will eventually clean up a write transaction lock somebody holds.
  // Whenever a lock is acquired, the WorkHandle is registered here, such that
  // the HotBackupFeature can cancel it on shutdown.
  Scheduler::WorkHandle _lockCleaner;
public:
  bool isAPIEnabled() { return _backupEnabled; }
  void registerLockCleaner(Scheduler::WorkHandle& handle) {
    _lockCleaner = handle;   // cancel any previous one
  }

  /// @brief returns true if and only if the current restart of the server
  /// is one from a hotbackup restore. This essentially tests existence of
  /// a file called "RESTORE" in the database directory.
  bool isRestoreStart();

  /// @brief removes the restore start marker "RESTORE", such that the next
  /// startup will be a non-restore startup.
  void removeRestoreStartMarker();
};

} // namespaces

#endif
