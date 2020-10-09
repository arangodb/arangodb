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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_REPLICATION_REPLICATION_METRICS_FEATURE_H
#define ARANGOD_REPLICATION_REPLICATION_METRICS_FEATURE_H 1

#include "ApplicationFeatures/ApplicationFeature.h"
#include "RestServer/Metrics.h"

#include <cstdint>

namespace arangodb {
namespace application_features {
class ApplicationServer;
}

class ReplicationMetricsFeature final : public application_features::ApplicationFeature {
 public:
  explicit ReplicationMetricsFeature(application_features::ApplicationServer& server);

  struct InitialSyncStats {
    explicit InitialSyncStats(ReplicationMetricsFeature& feature, bool autoPublish) noexcept
        : feature(feature), autoPublish(autoPublish) {}
    
    // will update the system-wide statistics with the current values
    ~InitialSyncStats() noexcept;

    ReplicationMetricsFeature& feature;

    /// @brief updates the system-wide metrics
    void publish();
    
    /// @brief resets the local statistics
    void reset() noexcept;

    InitialSyncStats& operator+=(InitialSyncStats const& other) noexcept;

    // total number of requests to /_api/replication/dump
    uint64_t numDumpRequests = 0;
    // total number of bytes received for dump requests
    uint64_t numDumpBytesReceived = 0;
    // total number of markers processed for dump requests
    uint64_t numDumpDocuments = 0;
    // total time spent waiting for responses to /_api/replication/dump
    double waitedForDump = 0.0;
    // total time spent for locally applying dump markers
    double waitedForDumpApply = 0.0;

    // total number of requests to /_api/replication/keys?type=keys
    uint64_t numKeysRequests = 0;
    // total number of requests to /_api/replication/keys?type=docs
    uint64_t numDocsRequests = 0;
    // total number of documents that for which document data were requested
    uint64_t numDocsRequested = 0;
    // total number of insert operations performed during sync
    uint64_t numDocsInserted = 0;
    // total number of remove operations performed during sync
    uint64_t numDocsRemoved = 0;
    // total number of bytes received for keys and docs
    uint64_t numSyncBytesReceived = 0;
    // total number of failed connection attempts
    uint64_t numFailedConnects = 0;
    // total time spent waiting on response for initial call to
    // /_api/replication/keys
    double waitedForInitial = 0.0;
    // total time spent waiting for responses to /_api/replication/keys?type=keys
    double waitedForKeys = 0.0;
    // total time spent waiting for responses to /_api/replication/keys?type=docs
    double waitedForDocs = 0.0;
    double waitedForInsertions = 0.0;
    double waitedForRemovals = 0.0;
    double waitedForKeyLookups = 0.0;

    bool autoPublish;
  };

  struct TailingSyncStats {
    explicit TailingSyncStats(ReplicationMetricsFeature& feature, bool autoPublish) noexcept
        : feature(feature), autoPublish(autoPublish) {}
    
    // will update the system-wide statistics with the current values
    ~TailingSyncStats() noexcept;

    ReplicationMetricsFeature& feature;
    
    /// @brief updates the system-wide metrics
    void publish();
    
    /// @brief resets the local statistics
    void reset() noexcept;
    
    TailingSyncStats& operator+=(TailingSyncStats const& other) noexcept;

    // total number of requests to /_api/wal/tail
    uint64_t numTailingRequests = 0;
    // required follow tick value ... is not present on leader ...
    uint64_t numFollowTickNotPresent = 0;
    uint64_t numProcessedMarkers = 0;
    uint64_t numProcessedDocuments = 0;
    uint64_t numProcessedRemovals = 0;
    // total number of bytes received for tailing requests
    uint64_t numTailingBytesReceived = 0;
    uint64_t numFailedConnects = 0;
    double waitedForTailing = 0.0;
    double waitedForTailingApply = 0.0;

    bool autoPublish;
  };
  
 private:
  // dump statistics
  
  // total number of requests to /_api/replication/dump
  Counter& _numDumpRequests;
  // total number of bytes received for dump requests
  Counter& _numDumpBytesReceived;
  // total number of markers processed for dump requests
  Counter& _numDumpDocuments;
  // total time spent waiting for responses to /_api/replication/dump
  Counter& _waitedForDump;
  // total time spent for locally applying dump markers
  Counter& _waitedForDumpApply;

  // initial sync statistics
  
  // total number of requests to /_api/replication/keys?type=keys
  Counter& _numSyncKeysRequests;
  // total number of requests to /_api/replication/keys?type=docs
  Counter& _numSyncDocsRequests;
  // total number of documents that for which document data were requested
  Counter& _numSyncDocsRequested;
  // total number of insert operations performed during sync
  Counter& _numSyncDocsInserted;
  // total number of remove operations performed during sync
  Counter& _numSyncDocsRemoved;
  // total number of bytes received for keys and docs requests
  Counter& _numSyncBytesReceived;
  // total time spent waiting on response for initial call to
  // /_api/replication/keys
  Counter& _waitedForSyncInitial;
  // total time spent waiting for responses to /_api/replication/keys?type=keys
  Counter& _waitedForSyncKeys;
  // total time spent waiting for responses to /_api/replication/keys?type=docs
  Counter& _waitedForSyncDocs;
  Counter& _waitedForSyncInsertions;
  Counter& _waitedForSyncRemovals;
  Counter& _waitedForSyncKeyLookups;
  
  // tailing statistics
  
  // total number of requests to tailing API
  Counter& _numTailingRequests;
  // required follow tick value ... is not present on leader ...
  Counter& _numTailingFollowTickNotPresent;
  // total number of processed markers during tailing
  Counter& _numTailingProcessedMarkers;
  // total number of processed document markers during tailing
  Counter& _numTailingProcessedDocuments;
  // total number of processed removals markers during tailing
  Counter& _numTailingProcessedRemovals;
  // total number of bytes received for tailing requests
  Counter& _numTailingBytesReceived;
  // total number of failed connection attempts during tailing syncing
  Counter& _numFailedConnects;
  // total time spent waiting for tail requests
  Counter& _waitedForTailing;
  // total time spent waiting for applying tailing markers
  Counter& _waitedForTailingApply;
};

} // namespace arangodb

#endif
