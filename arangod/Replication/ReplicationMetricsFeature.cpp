////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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

#include "ReplicationMetricsFeature.h"
#include "ApplicationFeatures/ApplicationServer.h"
#include "FeaturePhases/BasicFeaturePhaseServer.h"
#include "RestServer/MetricsFeature.h"

using namespace arangodb::application_features;
using namespace arangodb::options;

DECLARE_COUNTER(arangodb_replication_dump_requests,
                "Number of replication dump requests");
DECLARE_COUNTER(arangodb_replication_dump_bytes_received,
                "Number of bytes received in replication dump requests");
DECLARE_COUNTER(arangodb_replication_dump_documents,
                "Number of documents received in replication dump requests");
DECLARE_COUNTER(arangodb_replication_dump_request_time,
                "Wait time for replication requests [ms]");
DECLARE_COUNTER(arangodb_replication_dump_apply_time,
                "Accumulated time needed to apply replication dump data [ms]");
DECLARE_COUNTER(arangodb_replication_initial_sync_keys_requests,
                "Number of replication initial sync keys requests");
DECLARE_COUNTER(arangodb_replication_initial_sync_docs_requests,
                "Number of replication initial sync docs requests");
DECLARE_COUNTER(arangodb_replication_initial_sync_docs_requested,
                "Number of documents requested by replication initial sync");
DECLARE_COUNTER(arangodb_replication_initial_sync_docs_inserted,
                "Number of documents inserted by replication initial sync");
DECLARE_COUNTER(arangodb_replication_initial_sync_docs_removed,
                "Number of documents removed by replication initial sync");
DECLARE_COUNTER(arangodb_replication_initial_sync_bytes_received,
                "Number of bytes received during replication initial sync");
DECLARE_COUNTER(
    arangodb_replication_initial_chunks_requests_time,
    "Wait time for replication key chunks determination requests [ms]");
DECLARE_COUNTER(arangodb_replication_initial_keys_requests_time,
                "Wait time for replication keys requests [ms]");
DECLARE_COUNTER(arangodb_replication_initial_docs_requests_time,
                "Time needed to apply replication docs data [ms]");
DECLARE_COUNTER(
    arangodb_replication_initial_insert_apply_time,
    "Time needed to apply replication initial sync insertions [ms]");
DECLARE_COUNTER(arangodb_replication_initial_remove_apply_time,
                "Time needed to apply replication initial sync removals [ms]");
DECLARE_COUNTER(arangodb_replication_initial_lookup_time,
                "Time needed for replication initial sync key lookups [ms]");
DECLARE_COUNTER(arangodb_replication_tailing_requests,
                "Number of replication tailing requests");
DECLARE_COUNTER(
    arangodb_replication_tailing_follow_tick_failures,
    "Number of replication tailing failures due to missing tick on leader");
DECLARE_COUNTER(arangodb_replication_tailing_markers,
                "Number of replication tailing markers processed");
DECLARE_COUNTER(
    arangodb_replication_tailing_documents,
    "Number of replication tailing document inserts/replaces processed");
DECLARE_COUNTER(arangodb_replication_tailing_removals,
                "Number of replication tailing document removals processed");
DECLARE_COUNTER(arangodb_replication_tailing_bytes_received,
                "Number of bytes received for replication tailing requests");
DECLARE_COUNTER(arangodb_replication_failed_connects,
                "Number of failed connection attempts and response errors "
                "during replication");
DECLARE_COUNTER(arangodb_replication_tailing_request_time,
                "Wait time for replication tailing requests [ms]");
DECLARE_COUNTER(arangodb_replication_tailing_apply_time,
                "Time needed to apply replication tailing data [ms]");
DECLARE_COUNTER(
    arangodb_replication_synchronous_requests_total_time,
    "Total time needed for all synchronous replication requests [ns]");
DECLARE_COUNTER(arangodb_replication_synchronous_requests_total_number,
                "Total number of synchronous replication requests");
namespace arangodb {

ReplicationMetricsFeature::ReplicationMetricsFeature(arangodb::application_features::ApplicationServer& server)
    : ApplicationFeature(server, "ReplicationMetrics"),
      _numDumpRequests(server.getFeature<arangodb::MetricsFeature>().add(
          arangodb_replication_dump_requests{})),
      _numDumpBytesReceived(server.getFeature<arangodb::MetricsFeature>().add(
          arangodb_replication_dump_bytes_received{})),
      _numDumpDocuments(server.getFeature<arangodb::MetricsFeature>().add(
          arangodb_replication_dump_documents{})),
      _waitedForDump(server.getFeature<arangodb::MetricsFeature>().add(
          arangodb_replication_dump_request_time{})),
      _waitedForDumpApply(server.getFeature<arangodb::MetricsFeature>().add(
          arangodb_replication_dump_apply_time{})),
      _numSyncKeysRequests(server.getFeature<arangodb::MetricsFeature>().add(
          arangodb_replication_initial_sync_keys_requests{})),
      _numSyncDocsRequests(server.getFeature<arangodb::MetricsFeature>().add(
          arangodb_replication_initial_sync_docs_requests{})),
      _numSyncDocsRequested(server.getFeature<arangodb::MetricsFeature>().add(
          arangodb_replication_initial_sync_docs_requested{})),
      _numSyncDocsInserted(server.getFeature<arangodb::MetricsFeature>().add(
          arangodb_replication_initial_sync_docs_inserted{})),
      _numSyncDocsRemoved(server.getFeature<arangodb::MetricsFeature>().add(
          arangodb_replication_initial_sync_docs_removed{})),
      _numSyncBytesReceived(server.getFeature<arangodb::MetricsFeature>().add(
          arangodb_replication_initial_sync_bytes_received{})),
      _waitedForSyncInitial(server.getFeature<arangodb::MetricsFeature>().add(
          arangodb_replication_initial_chunks_requests_time{})),
      _waitedForSyncKeys(server.getFeature<arangodb::MetricsFeature>().add(
          arangodb_replication_initial_keys_requests_time{})),
      _waitedForSyncDocs(server.getFeature<arangodb::MetricsFeature>().add(
          arangodb_replication_initial_docs_requests_time{})),
      _waitedForSyncInsertions(server.getFeature<arangodb::MetricsFeature>().add(
          arangodb_replication_initial_insert_apply_time{})),
      _waitedForSyncRemovals(server.getFeature<arangodb::MetricsFeature>().add(
          arangodb_replication_initial_remove_apply_time{})),
      _waitedForSyncKeyLookups(server.getFeature<arangodb::MetricsFeature>().add(
          arangodb_replication_initial_lookup_time{})),
      _numTailingRequests(server.getFeature<arangodb::MetricsFeature>().add(
          arangodb_replication_tailing_requests{})),
      _numTailingFollowTickNotPresent(
          server.getFeature<arangodb::MetricsFeature>().add(
              arangodb_replication_tailing_follow_tick_failures{})),
      _numTailingProcessedMarkers(server.getFeature<arangodb::MetricsFeature>().add(
          arangodb_replication_tailing_markers{})),
      _numTailingProcessedDocuments(
          server.getFeature<arangodb::MetricsFeature>().add(
              arangodb_replication_tailing_documents{})),
      _numTailingProcessedRemovals(server.getFeature<arangodb::MetricsFeature>().add(
          arangodb_replication_tailing_removals{})),
      _numTailingBytesReceived(server.getFeature<arangodb::MetricsFeature>().add(
          arangodb_replication_tailing_bytes_received{})),
      _numFailedConnects(server.getFeature<arangodb::MetricsFeature>().add(
          arangodb_replication_failed_connects{})),
      _waitedForTailing(server.getFeature<arangodb::MetricsFeature>().add(
          arangodb_replication_tailing_request_time{})),
      _waitedForTailingApply(server.getFeature<arangodb::MetricsFeature>().add(
          arangodb_replication_tailing_apply_time{})),
      _syncTimeTotal(server.getFeature<arangodb::MetricsFeature>().add(
          arangodb_replication_synchronous_requests_total_time{})),
      _syncOpsTotal(server.getFeature<arangodb::MetricsFeature>().add(
          arangodb_replication_synchronous_requests_total_number{})) {
  setOptional(true);
  startsAfter<BasicFeaturePhaseServer>();
}

ReplicationMetricsFeature::InitialSyncStats::~InitialSyncStats() noexcept {
  if (autoPublish) {
    publish();
  }
}

/// @brief updates the system-wide metrics
void ReplicationMetricsFeature::InitialSyncStats::publish() {
  feature._numDumpRequests += numDumpRequests;
  feature._numDumpBytesReceived += numDumpBytesReceived;
  feature._numDumpDocuments +=  numDumpDocuments;
  feature._waitedForDump += static_cast<uint64_t>(waitedForDump * 1000);
  feature._waitedForDumpApply += static_cast<uint64_t>(waitedForDumpApply * 1000);
  
  feature._numSyncKeysRequests += numKeysRequests;
  feature._numSyncDocsRequests += numDocsRequests;
  feature._numSyncDocsRequested += numDocsRequested;
  feature._numSyncDocsInserted += numDocsInserted;
  feature._numSyncDocsRemoved += numDocsRemoved;
  feature._numSyncBytesReceived += numSyncBytesReceived;
  feature._numFailedConnects += numFailedConnects;
  feature._waitedForSyncInitial += static_cast<uint64_t>(waitedForInitial * 1000);
  feature._waitedForSyncKeys += static_cast<uint64_t>(waitedForKeys * 1000);
  feature._waitedForSyncDocs += static_cast<uint64_t>(waitedForDocs * 1000);
  feature._waitedForSyncInsertions += static_cast<uint64_t>(waitedForInsertions * 1000);
  feature._waitedForSyncRemovals += static_cast<uint64_t>(waitedForRemovals * 1000);
  feature._waitedForSyncKeyLookups += static_cast<uint64_t>(waitedForKeyLookups * 1000);

  reset();
}

void ReplicationMetricsFeature::InitialSyncStats::reset() noexcept {
  numDumpRequests = 0;
  numDumpBytesReceived = 0;
  numDumpDocuments = 0;
  waitedForDump = 0.0;
  waitedForDumpApply = 0.0;
  
  numKeysRequests = 0;
  numDocsRequests = 0;
  numDocsRequested = 0;
  numDocsInserted = 0;
  numDocsRemoved = 0;
  numSyncBytesReceived = 0;
  numFailedConnects = 0;
  waitedForInitial = 0.0;
  waitedForKeys = 0.0;
  waitedForDocs = 0.0;
  waitedForInsertions = 0.0;
  waitedForRemovals = 0.0;
  waitedForKeyLookups = 0.0;
}

ReplicationMetricsFeature::InitialSyncStats& ReplicationMetricsFeature::InitialSyncStats::operator+=(ReplicationMetricsFeature::InitialSyncStats const& other) noexcept {
  numDumpRequests += other.numDumpRequests;
  numDumpBytesReceived += other.numDumpBytesReceived;
  numDumpDocuments += other.numDumpDocuments;
  waitedForDump += other.waitedForDump;
  waitedForDumpApply += other.waitedForDumpApply;
  
  numKeysRequests += other.numKeysRequests;
  numDocsRequests += other.numDocsRequests;
  numDocsRequested += other.numDocsRequested;
  numDocsInserted += other.numDocsInserted;
  numDocsRemoved += other.numDocsRemoved;
  numSyncBytesReceived += other.numSyncBytesReceived;
  numFailedConnects += other.numFailedConnects;
  waitedForInitial += other.waitedForInitial;
  waitedForKeys += other.waitedForKeys;
  waitedForDocs += other.waitedForDocs;
  waitedForInsertions += other.waitedForInsertions;
  waitedForRemovals += other.waitedForRemovals;
  waitedForKeyLookups += other.waitedForKeyLookups;

  return *this;
}

ReplicationMetricsFeature::TailingSyncStats::~TailingSyncStats() noexcept {
  if (autoPublish) {
    publish();
  }
}

/// @brief updates the system-wide metrics
void ReplicationMetricsFeature::TailingSyncStats::publish() {
  feature._numTailingRequests += numTailingRequests;
  feature._numTailingFollowTickNotPresent += numFollowTickNotPresent;
  feature._numTailingProcessedMarkers += numProcessedMarkers;
  feature._numTailingProcessedDocuments += numProcessedDocuments;
  feature._numTailingProcessedRemovals += numProcessedRemovals;
  feature._numTailingBytesReceived += numTailingBytesReceived;
  feature._numFailedConnects += numFailedConnects;
  feature._waitedForTailing += static_cast<uint64_t>(waitedForTailing * 1000);
  feature._waitedForTailingApply += static_cast<uint64_t>(waitedForTailingApply * 1000);

  reset();
}

void ReplicationMetricsFeature::TailingSyncStats::reset() noexcept {
  numTailingRequests = 0;
  numFollowTickNotPresent = 0;
  numProcessedMarkers = 0;
  numProcessedDocuments = 0;
  numProcessedRemovals = 0;
  numTailingBytesReceived = 0;
  numFailedConnects = 0;
  waitedForTailing = 0;
  waitedForTailingApply = 0;
}

ReplicationMetricsFeature::TailingSyncStats& ReplicationMetricsFeature::TailingSyncStats::operator+=(ReplicationMetricsFeature::TailingSyncStats const& other) noexcept {
  numTailingRequests += other.numTailingRequests;
  numFollowTickNotPresent += other.numFollowTickNotPresent;
  numProcessedMarkers += other.numProcessedMarkers;
  numProcessedDocuments += other.numProcessedDocuments;
  numProcessedRemovals += other.numProcessedRemovals;
  numTailingBytesReceived += other.numTailingBytesReceived;
  numFailedConnects += other.numFailedConnects;
  waitedForTailing += other.waitedForTailing;
  waitedForTailingApply += other.waitedForTailingApply;
  
  return *this;
}

Counter& ReplicationMetricsFeature::synchronousTimeTotal() {
  return _syncTimeTotal;
}

Counter& ReplicationMetricsFeature::synchronousOpsTotal() {
  return _syncOpsTotal;
}

}  // namespace arangodb
