////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2017 ArangoDB GmbH, Cologne, Germany
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
/// @author Manuel Baesler
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_STATISTICS_STATISTICS_WORKER_H
#define ARANGOD_STATISTICS_STATISTICS_WORKER_H 1

#include "Basics/ConditionVariable.h"
#include "Basics/Thread.h"
#include "Statistics/figures.h"

#include <velocypack/Builder.h>
#include <velocypack/Slice.h>

struct TRI_vocbase_t;

namespace arangodb {

class StatisticsWorker final : public Thread {
 public:
  explicit StatisticsWorker(TRI_vocbase_t& vocbase);
  ~StatisticsWorker() { shutdown(); }

  void run() override;
  void beginShutdown() override;
  void generateRawStatistics(std::string& result, double const& now);

 private:
  // removes old statistics
  void collectGarbage();
  void collectGarbage(std::string const& collection, double time) const;

  // calculate per second statistics
  void historian();
  void computePerSeconds(velocypack::Builder& result, velocypack::Slice const& current,
                         velocypack::Slice const& prev);
  void generateRawStatistics(velocypack::Builder& result, double const& now);

  // calculate per 15 seconds statistics
  void historianAverage();
  void compute15Minute(velocypack::Builder& builder, double start);

  // create statistics collections
  void createCollections() const;
  void createCollection(std::string const&) const;

  std::shared_ptr<arangodb::velocypack::Builder> lastEntry(std::string const& collection,
                                                           double start) const;

  void avgPercentDistributon(velocypack::Builder& result, velocypack::Slice const&,
                             velocypack::Slice const&, velocypack::Builder const&) const;

  velocypack::Builder fillDistribution(statistics::Distribution const& dist) const;

  // save one statistics object
  void saveSlice(velocypack::Slice const&, std::string const&) const;

  void appendHistogram(
    std::string& result, statistics::Distribution const& dist,
    std::string const& label, std::initializer_list<std::string> const& les) const;
  void appendMetric(
    std::string& result, std::string const& val, std::string const& label) const;

  static constexpr uint64_t STATISTICS_INTERVAL = 10;    // 10 secs
  static constexpr uint64_t GC_INTERVAL = 8 * 60;        //  8 mins
  static constexpr uint64_t HISTORY_INTERVAL = 15 * 60;  // 15 mins
  static constexpr double INTERVAL = 10.0;               // 10 secs

  enum GarbageCollectionTask { GC_STATS, GC_STATS_RAW, GC_STATS_15 };

  GarbageCollectionTask _gcTask;  // type of garbage collection task to run
  arangodb::basics::ConditionVariable _cv;
  velocypack::Builder _bytesSentDistribution;
  velocypack::Builder _bytesReceivedDistribution;
  velocypack::Builder _requestTimeDistribution;

  // builder object used to create bind variables. this is reused for each query
  std::shared_ptr<velocypack::Builder> _bindVars;

  // a reusable builder to save a few memory allocations per statistics
  // invocation
  velocypack::Builder _rawBuilder;
  velocypack::Builder _tempBuilder;

  std::string _clusterId;
  TRI_vocbase_t& _vocbase;  // vocbase for querying/persisting statistics collections
};

}  // namespace arangodb

#endif
