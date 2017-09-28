////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2017 ArangoDB GmbH, Cologne, Germany
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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_REPLICATION_WAL_ACCESS_SYNCER_H
#define ARANGOD_REPLICATION_WAL_ACCESS_SYNCER_H 1

#include "Replication/TailingSyncer.h"
#include "Replication/ReplicationApplierConfiguration.h"

namespace arangodb {
struct ReplicationApplierState;

class WalAccessSyncer : public TailingSyncer {
 public:
  WalAccessSyncer(ReplicationApplierConfiguration const*,
                  TRI_voc_tick_t initialTick, bool useTick);

  ~WalAccessSyncer();

 public:
  /// @brief run method, performs continuous synchronization
  int run();

 private:
  /// @brief called before marker is processed
  void preApplyMarker(TRI_voc_tick_t firstRegularTick,
                     TRI_voc_tick_t newTick) override;
  /// @brief called after a marker was processed
  void postApplyMarker(uint64_t processedMarkers, bool skipped) override;

  /// @brief set the applier progress
  void setProgress(char const*);
  void setProgress(std::string const&);

  /// @brief save the current applier state
  int saveApplierState();

  /// @brief get local replication applier state
  int getLocalState(std::string&);

  /// @brief perform a continuous sync with the master
  int runContinuousSync(std::string&);

  /// @brief fetch the initial master state
  int fetchMasterState(std::string&, TRI_voc_tick_t, TRI_voc_tick_t,
                       TRI_voc_tick_t&);

  /// @brief run the continuous synchronization
  int followMasterLog(std::string&, TRI_voc_tick_t&, TRI_voc_tick_t, uint64_t&,
                      bool&, bool&);
  
  TRI_vocbase_t* vocbase() const {
    TRI_ASSERT(_vocbaseCache.size() == 1);
    return _vocbaseCache.begin()->second.vocbase();
  }

 private:

  // current state
  std::unique_ptr<ReplicationApplierState> _applierState;
  
  /// @brief use the initial tick
  bool _useTick;

  /// @brief whether or not the applier should be verbose
  bool _verbose;

  /// @brief whether or not the replication state file has been written at least
  /// once with non-empty values. this is required in situations when the
  /// replication applier is manually started and the master has absolutely no
  /// new data to provide, and the slave get shut down. in that case, the state
  /// file would never have been written with the initial start tick, so the
  /// start tick would be lost. re-starting the slave and the replication
  /// applier
  /// with the ticks from the file would then result in a "no start tick
  /// provided"
  /// error
  bool _hasWrittenState;
};
}

#endif
