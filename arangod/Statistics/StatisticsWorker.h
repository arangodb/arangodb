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

#include "Basics/Thread.h"
#include "Statistics/figures.h"

#include <string>
#include <velocypack/Builder.h>
#include <velocypack/Slice.h>

namespace arangodb {

class StatisticsWorker final : public Thread {
 public:
  StatisticsWorker();

  ~StatisticsWorker() { shutdown(); }
  void run() override;

 private:
  // removes old statistics
  void collectGarbage() const;
  void _collectGarbage(std::string const& collection, double time) const;

  // calculate per second statistics
  void historian() const;
  velocypack::Builder _computePerSeconds(velocypack::Slice const&,
                                         velocypack::Slice const&,
                                         std::string const&) const;
  velocypack::Builder _generateRawStatistics(std::string const& clusterId,
                                      double const& now) const;

  // calculate per 15 seconds statistics
  void historianAverage() const;
  velocypack::Builder _compute15Minute(double start,
                                std::string const& clusterId) const;

  // create statistics collections
  void createCollections() const;
  void _createCollection(std::string const&) const;

  std::shared_ptr<arangodb::velocypack::Builder> _lastEntry(
      std::string const& collection, double start,
      std::string const& clusterId) const;
  velocypack::Builder _avgPercentDistributon(velocypack::Slice const&,
                                             velocypack::Slice const&,
                                             velocypack::Builder const&) const;
  velocypack::Builder _fillDistribution(
      basics::StatisticsDistribution const& dist) const;

  // save one statistics object
  void _saveSlice(velocypack::Slice const&, std::string const&) const;

  uint64_t const HISTORY_INTERVAL = 15 * 60;  // 15 min
  uint64_t const INTERVAL = 10;               // 10 secs

  velocypack::Builder _bytesSentDistribution;
  velocypack::Builder _bytesReceivedDistribution;
  velocypack::Builder _requestTimeDistribution;
};
}

#endif
