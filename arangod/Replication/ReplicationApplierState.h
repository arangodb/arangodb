////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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

#ifndef ARANGOD_REPLICATION_REPLICATION_APPLIER_STATE_H
#define ARANGOD_REPLICATION_REPLICATION_APPLIER_STATE_H 1

#include "Basics/Common.h"
#include "Basics/StaticStrings.h"
#include "Basics/voc-errors.h"
#include "Replication/common-defines.h"
#include "VocBase/Identifiers/ServerId.h"
#include "VocBase/voc-types.h"

#include <velocypack/Builder.h>
#include <velocypack/Value.h>
#include <velocypack/velocypack-aliases.h>

namespace arangodb {

/// @brief state information about replication application
struct ReplicationApplierState {
  enum class ActivityPhase {
    INACTIVE,  /// sleeping
    INITIAL,   /// running initial syncer
    TAILING,   /// running tailing syncer
    SHUTDOWN   /// cleaning up
  };

  ReplicationApplierState();
  ~ReplicationApplierState();

  ReplicationApplierState(ReplicationApplierState const& other) = delete;
  ReplicationApplierState& operator=(ReplicationApplierState const& other);

  void reset(bool resetPhase, bool reducedSet = false);
  void toVelocyPack(arangodb::velocypack::Builder& result, bool full) const;

  bool hasProcessedSomething() const {
    return (_lastProcessedContinuousTick > 0 ||
            _lastAppliedContinuousTick > 0 || _safeResumeTick > 0);
  }

  TRI_voc_tick_t _lastProcessedContinuousTick;
  TRI_voc_tick_t _lastAppliedContinuousTick;
  TRI_voc_tick_t _lastAvailableContinuousTick;
  TRI_voc_tick_t _safeResumeTick;
  ActivityPhase _phase;
  bool _preventStart;
  bool _stopInitialSynchronization;

  std::string _progressMsg;
  char _progressTime[24];
  ServerId _serverId;
  char _startTime[24];

  /// performs initial sync or running tailing syncer
  bool isActive() const {
    return (_phase == ActivityPhase::INITIAL || _phase == ActivityPhase::TAILING);
  }

  /// performs initial sync or running tailing syncer
  bool isInitializing() const { return _phase == ActivityPhase::INITIAL; }

  /// performs tailing sync
  bool isTailing() const { return (_phase == ActivityPhase::TAILING); }

  bool isShuttingDown() const { return (_phase == ActivityPhase::SHUTDOWN); }

  void setError(int code, std::string const& msg) { _lastError.set(code, msg); }

  void setStartTime();

  // last error that occurred during replication
  struct LastError {
    LastError() : code(TRI_ERROR_NO_ERROR), message() { time[0] = '\0'; }

    void reset() {
      code = TRI_ERROR_NO_ERROR;
      message.clear();
      time[0] = '\0';
      TRI_GetTimeStampReplication(time, sizeof(time) - 1);
    }

    void set(int errorCode, std::string const& msg) {
      code = errorCode;
      message = msg;
      time[0] = '\0';
      TRI_GetTimeStampReplication(time, sizeof(time) - 1);
    }

    void toVelocyPack(arangodb::velocypack::Builder& result) const {
      result.add(VPackValue(VPackValueType::Object));
      result.add(StaticStrings::ErrorNum, VPackValue(code));

      if (code > 0) {
        result.add("time", VPackValue(time));
        if (!message.empty()) {
          result.add(StaticStrings::ErrorMessage, VPackValue(message));
        }
      }
      result.close();
    }

    int code;
    std::string message;
    char time[24];
  } _lastError;

  // counters
  uint64_t _failedConnects;
  uint64_t _totalRequests;
  uint64_t _totalFailedConnects;
  uint64_t _totalEvents;
  uint64_t _totalDocuments;
  uint64_t _totalRemovals;
  uint64_t _totalResyncs;
  uint64_t _totalSkippedOperations;

  /// @brief total time spend in applyLog()
  double _totalApplyTime;

  /// @brief number of times we called applyLog()
  uint64_t _totalApplyInstances;

  /// @brief total time spend for fetching data from leader
  double _totalFetchTime;

  /// @brief number of times data was fetched from leader
  uint64_t _totalFetchInstances;
};

}  // namespace arangodb

#endif
