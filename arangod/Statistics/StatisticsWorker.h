////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2016 ArangoDB GmbH, Cologne, Germany
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

#include <string>

namespace arangodb {

class StatisticsWorker final : public Thread {

  public:
    StatisticsWorker();

    ~StatisticsWorker() { shutdown(); }
    void run() override;

  private:
    void collectGarbage();
    void historian();
    void historianAverage();
    void createCollections();
    void _createCollection(std::string const&);
    void _collectGarbage(std::string const& collection, double time);
    std::shared_ptr<arangodb::velocypack::Builder> _lastEntry(std::string const& collection, double start, std::string const& clusterId);
    VPackBuilder _compute15Minute(double start, std::string const& clusterId);
    VPackBuilder _computePerSeconds(VPackSlice const&, VPackSlice const&, std::string const&);
    VPackBuilder _avgPercentDistributon(VPackSlice const&, VPackSlice const&, VPackBuilder const&);

    VPackBuilder _generateRawStatistics(std::string const& clusterId, double const& now);
    VPackBuilder _fillDistribution(basics::StatisticsDistribution const& dist);

    void _saveSlice(VPackSlice const&, std::string const&);

    uint64_t const HISTORY_INTERVAL = 15 * 60; // 15 min
    uint64_t const INTERVAL = 10;

    VPackBuilder _bytesSentDistribution;
    VPackBuilder _bytesReceivedDistribution;
    VPackBuilder _requestTimeDistribution;
};

}

#endif
