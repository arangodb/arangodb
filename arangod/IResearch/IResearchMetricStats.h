////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
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
/// @author Valery Mironov
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include "Cluster/ClusterFeature.h"
#include "Cluster/ClusterInfo.h"
#include "IResearch/IResearchDataStore.h"
#include "Metrics/Guard.h"
#include "RestServer/arangod.h"
#include "ApplicationFeatures/ApplicationServer.h"
#include "Metrics/Batch.h"
#include "Metrics/GaugeBuilder.h"
#include "Metrics/Guard.h"
#include "Metrics/MetricsFeature.h"

namespace arangodb::iresearch {

class MetricStats : public metrics::Guard<IResearchDataStore::Stats> {
  static constexpr std::string_view kShard = ",shard=\"";

 public:
  static constexpr size_t kSize = 5;
  static constexpr std::array<std::string_view, kSize> kName = {
      "arangodb_search_num_docs",       //
      "arangodb_search_num_live_docs",  //
      "arangodb_search_num_segments",   //
      "arangodb_search_num_files",      //
      "arangodb_search_index_size",     //
  };

  // toVPack
  static bool skip(ArangodServer& server, std::string_view labels) {
    auto& ci = server.getFeature<ClusterFeature>().clusterInfo();
    auto start = labels.find(kShard);
    TRI_ASSERT(start != std::string_view::npos);
    start += kShard.size();
    TRI_ASSERT(start < labels.size());
    std::string /*TODO(MBkkt) Fix cluster info interface*/ shardId{
        labels.substr(start, labels.size() - start - 1)};
    auto r = ci.getResponsibleServer(shardId);
    if (r->empty()) {
      return true;  // TODO(MBkkt) We should fix cluster info :(
    }
    if ((*r)[0] != ServerState::instance()->getId()) {
      // We want collect only leader shards stats
      return true;
    }
    return false;
  }

  static std::string_view coordinatorLabels(std::string_view labels) {
    // We want merge different shard
    auto const start = labels.find(kShard);
    TRI_ASSERT(start != std::string_view::npos);
    return labels.substr(0, start);
  }

  using DataToValue = uint64_t (*)(Data const&);
  static constexpr std::array<DataToValue, kSize> kToValue = {
      [](IResearchDataStore::Stats const& stats) { return stats.numDocs; },
      [](IResearchDataStore::Stats const& stats) { return stats.numLiveDocs; },
      [](IResearchDataStore::Stats const& stats) { return stats.numSegments; },
      [](IResearchDataStore::Stats const& stats) { return stats.numFiles; },
      [](IResearchDataStore::Stats const& stats) { return stats.indexSize; },
  };

  // toPrometheus
  using DataToString = std::string (*)(Data const&);
  static constexpr std::array<DataToString, kSize> kToString = {
      [](IResearchDataStore::Stats const& stats) {
        return std::to_string(kToValue[0](stats));
      },
      [](IResearchDataStore::Stats const& stats) {
        return std::to_string(kToValue[1](stats));
      },
      [](IResearchDataStore::Stats const& stats) {
        return std::to_string(kToValue[2](stats));
      },
      [](IResearchDataStore::Stats const& stats) {
        return std::to_string(kToValue[3](stats));
      },
      [](IResearchDataStore::Stats const& stats) {
        return std::to_string(kToValue[4](stats));
      },
  };

  // TODO(MBkkt) Remove these arrays when we make generation maps from docs
  static constexpr std::array<std::string_view, kSize> kHelp = {
      "Number of documents",         //
      "Number of live documents",    //
      "Number of segments",          //
      "Number of files",             //
      "Size of the index in bytes",  //
  };

  static constexpr std::array<std::string_view, kSize> kType = {
      "gauge", "gauge", "gauge", "gauge", "gauge",
  };
};

DECLARE_GAUGE(arangodb_search_num_docs, uint64_t, "Number of documents");
DECLARE_GAUGE(arangodb_search_num_live_docs, uint64_t,
              "Number of live documents");
DECLARE_GAUGE(arangodb_search_num_segments, uint64_t, "Number of segments");
DECLARE_GAUGE(arangodb_search_num_files, uint64_t, "Number of files");
DECLARE_GAUGE(arangodb_search_index_size, uint64_t,
              "Size of the index in bytes");
DECLARE_GAUGE(arangodb_search_num_failed_commits, uint64_t,
              "Number of failed commits");
DECLARE_GAUGE(arangodb_search_num_failed_cleanups, uint64_t,
              "Number of failed cleanups");
DECLARE_GAUGE(arangodb_search_num_failed_consolidations, uint64_t,
              "Number of failed consolidations");
DECLARE_GAUGE(arangodb_search_commit_time, uint64_t,
              "Average time of few last commits");
DECLARE_GAUGE(arangodb_search_cleanup_time, uint64_t,
              "Average time of few last cleanups");
DECLARE_GAUGE(arangodb_search_consolidation_time, uint64_t,
              "Average time of few last consolidations");

inline constexpr std::string_view kSearchStats = "arangodb_search_link_stats";

}  // namespace arangodb::iresearch
