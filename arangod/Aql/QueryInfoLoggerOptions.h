////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2025 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <cstddef>
#include <cstdint>

namespace arangodb::aql {

struct QueryInfoLoggerOptions {
  // maximum number of queries to store before they can be logged
  size_t maxBufferedQueries = 4096;

  // if false, no logging will happen at all. if true, consider all other config
  // options.
  bool logEnabled = false;

  // if true, log queries in system database, otherwise only non-system.
  bool logSystemDatabaseQueries = false;

  // if true, log all slow queries (while honoring _logSystemDatabaseQueries).
  bool logSlowQueries = true;

  // probability with which queries are logged. scaled between 0.0 and 100.0.
  double logProbability = 0.1;

  // push interval, specified in milliseconds
  uint64_t pushInterval = 3'000;

  // collection cleanup interval, specified in milliseconds.
  uint64_t cleanupInterval = 600'000;

  // retention time for which queries are kept in the _queries system collection
  // before they are purged. specified in seconds.
  double retentionTime = 28800.0;
};

}  // namespace arangodb::aql
