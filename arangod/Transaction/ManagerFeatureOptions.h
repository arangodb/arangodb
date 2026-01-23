////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <cstddef>

namespace arangodb::transaction {

struct ManagerFeatureOptions {
  static constexpr size_t defaultStreamingMaxTransactionSize =
      512 * 1024 * 1024;  // 512 MiB
  static constexpr double defaultStreamingIdleTimeout = 60.0;
  static constexpr double maxStreamingIdleTimeout = 120.0;

  // max size (in bytes) of streaming transactions
  size_t streamingMaxTransactionSize = defaultStreamingMaxTransactionSize;

  // lock time in seconds
  double streamingLockTimeout = 8.0;

  // idle timeout for streaming transactions, in seconds
  double streamingIdleTimeout = defaultStreamingIdleTimeout;
};

}  // namespace arangodb::transaction
