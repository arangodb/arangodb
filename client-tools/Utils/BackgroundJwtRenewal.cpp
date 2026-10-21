////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2026 ArangoDB GmbH, Cologne, Germany
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

#include "BackgroundJwtRenewal.h"

#include <condition_variable>
#include <mutex>
#include <stop_token>
#include <utility>

namespace arangodb {

BackgroundJwtRenewal::BackgroundJwtRenewal(
    std::shared_ptr<RenewingJwtToken> token,
    std::chrono::milliseconds checkInterval)
    : _thread([token = std::move(token), checkInterval](std::stop_token stop) {
        std::mutex mutex;
        std::condition_variable_any wakeUp;
        auto lock = std::unique_lock{mutex};
        // wait_for returns true as soon as stopping is requested and false
        // once the interval has passed
        while (!wakeUp.wait_for(lock, stop, checkInterval,
                                [&stop] { return stop.stop_requested(); })) {
          token->renewIfDue();
        }
      }) {}

}  // namespace arangodb
