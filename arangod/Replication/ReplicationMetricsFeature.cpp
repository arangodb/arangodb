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

#include "ReplicationMetricsFeature.h"
#include "ApplicationFeatures/ApplicationServer.h"
#include "FeaturePhases/BasicFeaturePhaseServer.h"
#include "RestServer/MetricsFeature.h"

using namespace arangodb::application_features;
using namespace arangodb::options;

namespace arangodb {

ReplicationMetricsFeature::ReplicationMetricsFeature(arangodb::application_features::ApplicationServer& server)
    : ApplicationFeature(server, "ReplicationMetrics"),
      _numDumpRequests(
        server.getFeature<arangodb::MetricsFeature>().counter(
          "arangodb_replication_dump_requests", 0, "Number of replication dump requests")),
      _numDumpBytesReceived(
        server.getFeature<arangodb::MetricsFeature>().counter(
          "arangodb_replication_dump_bytes_received", 0, "Number of bytes received in replication dump requests")),
      _numDumpDocuments(
        server.getFeature<arangodb::MetricsFeature>().counter(
          "arangodb_replication_dump_documents", 0, "Number of documents received in replication dump requests")),
      _waitedForDump(
        server.getFeature<arangodb::MetricsFeature>().counter(
          "arangodb_replication_dump_request_time", 0,
          "Wait time for replication requests [ms]")),
      _waitedForDumpApply(
        server.getFeature<arangodb::MetricsFeature>().counter(
          "arangodb_replication_dump_apply_time", 0,
          "time needed to apply replication dump data [ms]")),
      _numSyncKeysRequests(
        server.getFeature<arangodb::MetricsFeature>().counter(
          "arangodb_replication_initial_sync_keys_requests", 0, "Number of replication initial sync keys requests")),
      _numSyncDocsRequests(
        server.getFeature<arangodb::MetricsFeature>().counter(
          "arangodb_replication_initial_sync_docs_requests", 0, "Number of replication initial sync docs requests")),
      _numSyncDocsRequested(
        server.getFeature<arangodb::MetricsFeature>().counter(
          "arangodb_replication_initial_sync_docs_requested", 0, "Number of documents requested by replication initial sync")),
      _numSyncDocsInserted(
        server.getFeature<arangodb::MetricsFeature>().counter(
          "arangodb_replication_initial_sync_docs_inserted", 0, "Number of documents inserted by replication initial sync")),
      _numSyncDocsRemoved(
        server.getFeature<arangodb::MetricsFeature>().counter(
          "arangodb_replication_initial_sync_docs_removed", 0, "Number of documents removed by replication initial sync")),
      _numSyncBytesReceived(
        server.getFeature<arangodb::MetricsFeature>().counter(
          "arangodb_replication_initial_sync_bytes_received", 0, "Number of bytes received during replication initial sync")),
      _waitedForSyncInitial(
        server.getFeature<arangodb::MetricsFeature>().counter(
          "arangodb_replication_initial_chunks_requests_time", 0,
          "Wait time for replication key chunks determination requests [ms]")),
      _waitedForSyncKeys(
        server.getFeature<arangodb::MetricsFeature>().counter(
          "arangodb_replication_initial_keys_requests_time", 0,
          "Wait time for replication keys requests [ms]")),
      _waitedForSyncDocs(
        server.getFeature<arangodb::MetricsFeature>().counter(
          "arangodb_replication_initial_docs_requests_time", 0,
          "Time needed to apply replication docs data [ms]")),
      _waitedForSyncInsertions(
        server.getFeature<arangodb::MetricsFeature>().counter(
          "arangodb_replication_initial_insert_apply_time", 0,
          "Time needed to apply replication initial sync insertions [ms]")),
      _waitedForSyncRemovals(
        server.getFeature<arangodb::MetricsFeature>().counter(
          "arangodb_replication_initial_remove_apply_time", 0,
          "Time needed to apply replication initial sync removals [ms]")),
      _waitedForSyncKeyLookups(
        server.getFeature<arangodb::MetricsFeature>().counter(
          "arangodb_replication_initial_lookup_time", 0,
          "Time needed for replication initial sync key lookups [ms]")),
      _numTailingRequests(
        server.getFeature<arangodb::MetricsFeature>().counter(
          "arangodb_replication_tailing_requests", 0, "Number of replication tailing requests")),
      _numTailingFollowTickNotPresent(
        server.getFeature<arangodb::MetricsFeature>().counter(
          "arangodb_replication_tailing_follow_tick_failures", 0, "Number of replication tailing failures due to missing tick on leader")),
      _numTailingProcessedMarkers(
        server.getFeature<arangodb::MetricsFeature>().counter(
          "arangodb_replication_tailing_markers", 0, "Number of replication tailing markers processed")),
      _numTailingProcessedDocuments(
        server.getFeature<arangodb::MetricsFeature>().counter(
          "arangodb_replication_tailing_documents", 0, "Number of replication tailing document inserts/replaces processed")),
      _numTailingProcessedRemovals(
        server.getFeature<arangodb::MetricsFeature>().counter(
          "arangodb_replication_tailing_removals", 0, "Number of replication tailing document removals processed")),
      _numTailingBytesReceived(
        server.getFeature<arangodb::MetricsFeature>().counter(
          "arangodb_replication_tailing_bytes_received", 0, "Number of bytes received for replication tailing requests")),
      _numFailedConnects(
        server.getFeature<arangodb::MetricsFeature>().counter(
          "arangodb_replication_failed_connects", 0, "Number of failed connection attempts and response errors during replication")),
      _waitedForTailing(
        server.getFeature<arangodb::MetricsFeature>().counter(
          "arangodb_replication_tailing_request_time", 0,
          "Wait time for replication tailing requests [ms]")),
      _waitedForTailingApply(
        server.getFeature<arangodb::MetricsFeature>().counter(
          "arangodb_replication_tailing_apply_time", 0,
          "Time needed to apply replication tailing data [ms]")) {
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

} // namespace arangodb
