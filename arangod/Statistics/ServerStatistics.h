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
/// @author Dr. Frank Celler
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_STATISTICS_SERVER_STATISTICS_H
#define ARANGOD_STATISTICS_SERVER_STATISTICS_H 1

#include <atomic>
#include <memory>


struct TransactionStatistics {
  TransactionStatistics() : _transactionsStarted(0), _transactionsAborted(0)
                          , _transactionsCommitted(0), _intermediateCommits(0) {};

  std::atomic<std::uint64_t> _transactionsStarted;
  std::atomic<std::uint64_t> _transactionsAborted;
  std::atomic<std::uint64_t> _transactionsCommitted;
  std::atomic<std::uint64_t> _intermediateCommits;
};

struct ServerStatistics {

  ServerStatistics(ServerStatistics const&) = delete;
  ServerStatistics(ServerStatistics &&) = delete;
  ServerStatistics& operator=(ServerStatistics const&) = delete;
  ServerStatistics& operator=(ServerStatistics &&) = delete;

  static ServerStatistics& statistics();
  static void initialize(double);

  TransactionStatistics _transactionsStatistics;
  double _startTime;
  std::atomic<double> _uptime;

  explicit ServerStatistics(double start) : _transactionsStatistics(), _startTime(start), _uptime(0.0) {}
};

#endif
