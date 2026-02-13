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
////////////////////////////////////////////////////////////////////////////////

#pragma once

<<<<<<<<HEAD : arangod / Transaction / ManagerFeatureOptions.h
#include <cstddef>
        == == == ==
#include "ApplicationFeatures/ApplicationFeaturePhase.h"
#include "ApplicationFeatures/ApplicationServer.h"
        >>>>>>>> origin /
    devel : arangod / FeaturePhases /
            V8FeaturePhase.h

            namespace arangodb::transaction {

  struct ManagerFeatureOptions {
    static constexpr double maxStreamingIdleTimeout = 120.0;

    < < < < < < < < HEAD : arangod / Transaction /
                           ManagerFeatureOptions.h
                               // max size (in bytes) of streaming transactions
                               size_t streamingMaxTransactionSize =
        512 * 1024 * 1024;  // 512 MiB

    // lock time in seconds
    double streamingLockTimeout = 8.0;

    // idle timeout for streaming transactions, in seconds
    double streamingIdleTimeout = 60.0;
    == == == ==
        explicit V8FeaturePhase(
            application_features::ApplicationServer& server);
    >>>>>>>> origin / devel : arangod / FeaturePhases / V8FeaturePhase.h
  };

}  // namespace arangodb::transaction
