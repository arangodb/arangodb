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

#ifndef ARANGOD_MMFILES_WAL_LOGFILE_H
#define ARANGOD_MMFILES_WAL_LOGFILE_H 1

#include "Basics/Common.h"
#include "Logger/Logger.h"
#include "MMFiles/MMFilesDatafile.h"
#include "MMFiles/MMFilesDatafileHelper.h"
#include "MMFiles/MMFilesWalMarker.h"
#include "VocBase/voc-types.h"

namespace arangodb {

class MMFilesWalLogfile {
 public:
  /// @brief typedef for logfile ids
  typedef TRI_voc_fid_t IdType;

  /// @brief logfile status
  enum class StatusType : uint32_t {
    UNKNOWN = 0,
    EMPTY = 1,
    OPEN = 2,
    SEAL_REQUESTED = 3,
    SEALED = 4,
    COLLECTION_REQUESTED = 5,
    COLLECTED = 6
  };

 private:
  MMFilesWalLogfile(MMFilesWalLogfile const&) = delete;
  MMFilesWalLogfile& operator=(MMFilesWalLogfile const&) = delete;

 public:
  /// @brief create a logfile
  MMFilesWalLogfile(MMFilesWalLogfile::IdType, MMFilesDatafile*, StatusType);

  /// @brief destroy a logfile
  ~MMFilesWalLogfile();

  /// @brief create a new logfile
  static MMFilesWalLogfile* createNew(std::string const&, MMFilesWalLogfile::IdType, uint32_t);

  /// @brief open an existing logfile
  static MMFilesWalLogfile* openExisting(std::string const&, MMFilesWalLogfile::IdType,
                                         bool wasCollected, bool ignoreErrors);

  int lockInMemory() { return _df->lockInMemory(); }

  int unlockFromMemory() { return _df->unlockFromMemory(); }

  /// @brief return the filename
  inline std::string filename() const {
    if (_df == nullptr) {
      return "";
    }
    return _df->getName();
  }

  /// @brief return the datafile pointer
  inline MMFilesDatafile* df() const { return _df; }

  /// @brief return the pointer to the logfile contents
  inline char const* data() const {
    TRI_ASSERT(_df != nullptr);
    return _df->data();
  }

  /// @brief return the file descriptor
  inline int fd() const { return _df->fd(); }

  /// @brief return the logfile id
  inline MMFilesWalLogfile::IdType id() const { return _id; }

  /// @brief update the logfile tick status
  inline void update(MMFilesMarker const* marker) {
    TRI_UpdateTicksDatafile(df(), marker);
  }

  /// @brief return the logfile status
  inline MMFilesWalLogfile::StatusType status() const { return _status; }

  /// @brief return the allocated size of the logfile
  inline uint64_t allocatedSize() const {
    return static_cast<uint64_t>(_df->maximalSize());
  }

  /// @brief return the size of the free space in the logfile
  uint64_t freeSize() const {
    if (isSealed()) {
      return 0;
    }

    return static_cast<uint64_t>(allocatedSize() - _df->currentSize() -
                                 MMFilesDatafileHelper::JournalOverhead());
  }

  /// @brief whether or not a marker of the specified size can be written into
  /// the logfile
  bool isWriteable(uint32_t size) const {
    if (isSealed() || freeSize() < static_cast<uint64_t>(size)) {
      return false;
    }

    return true;
  }

  /// @brief whether or not the logfile is sealed
  inline bool isSealed() const {
    return (_status == StatusType::SEAL_REQUESTED || _status == StatusType::SEALED ||
            _status == StatusType::COLLECTION_REQUESTED || _status == StatusType::COLLECTED);
  }

  /// @brief whether or not the logfile can be sealed
  inline bool canBeSealed() const {
    return (_status == StatusType::SEAL_REQUESTED);
  }

  /// @brief whether or not the logfile can be collected
  inline bool canBeCollected(TRI_voc_tick_t releasedTick) const {
    if (releasedTick < df()->maxTick()) {
      return false;
    }

    return (_status == StatusType::SEALED || _status == StatusType::COLLECTION_REQUESTED);
  }

  /// @brief whether or not the logfile can be collected
  inline bool hasBeenReleased(TRI_voc_tick_t releasedTick) const {
    return (releasedTick >= df()->maxTick());
  }

  /// @brief whether or not the logfile can be removed
  inline bool canBeRemoved() const {
    return (_status == StatusType::COLLECTED && _collectQueueSize == 0 && _users == 0);
  }

  /// @brief return the logfile status as a string
  std::string statusText() const { return statusText(status()); }

  /// @brief return the logfile status as a string
  static std::string statusText(StatusType status) {
    switch (status) {
      case StatusType::EMPTY:
        return "empty";
      case StatusType::OPEN:
        return "open";
      case StatusType::SEAL_REQUESTED:
        return "seal-requested";
      case StatusType::SEALED:
        return "sealed";
      case StatusType::COLLECTION_REQUESTED:
        return "collection-requested";
      case StatusType::COLLECTED:
        return "collected";
      case StatusType::UNKNOWN:
      default:
        return "unknown";
    }
  }

  /// @brief change the logfile status, without assertions
  void forceStatus(StatusType status) { _status = status; }

  /// @brief change the logfile status, with assertions
  void setStatus(StatusType status) {
    switch (status) {
      case StatusType::UNKNOWN:
      case StatusType::EMPTY:
        TRI_ASSERT(false);
        break;

      case StatusType::OPEN:
        TRI_ASSERT(_status == StatusType::EMPTY);
        break;

      case StatusType::SEAL_REQUESTED:
        TRI_ASSERT(_status == StatusType::OPEN);
        break;

      case StatusType::SEALED:
        TRI_ASSERT(_status == StatusType::SEAL_REQUESTED);
        break;

      case StatusType::COLLECTION_REQUESTED:
        TRI_ASSERT(_status == StatusType::SEALED);
        break;

      case StatusType::COLLECTED:
        TRI_ASSERT(_status == StatusType::COLLECTION_REQUESTED);
        break;
    }

    LOG_TOPIC("10910", TRACE, arangodb::Logger::ENGINES)
        << "changing logfile status from " << statusText(_status) << " to "
        << statusText(status) << " for logfile " << id();
    _status = status;
  }

  /// @brief reserve space and update the current write position
  char* reserve(size_t);

  /// @brief increase the number of collect operations waiting
  inline void increaseCollectQueueSize() { ++_collectQueueSize; }

  /// @brief decrease the number of collect operations waiting
  inline void decreaseCollectQueueSize() { --_collectQueueSize; }

  /// @brief use a logfile - while there are users, the logfile cannot be
  /// deleted
  inline void use() { ++_users; }

  /// @brief release a logfile - while there are users, the logfile cannot be
  /// deleted
  inline void release() {
    TRI_ASSERT(_users > 0);
    --_users;
  }

  /// @brief the logfile id
  MMFilesWalLogfile::IdType const _id;

  /// @brief the number of logfile users
  std::atomic<int32_t> _users;

  /// @brief the datafile entry
  MMFilesDatafile* _df;

  /// @brief logfile status
  StatusType _status;

  /// @brief number of collect operations waiting
  std::atomic<int64_t> _collectQueueSize;
};

}  // namespace arangodb

#endif
