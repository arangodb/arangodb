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

#pragma once

#include "Utils/RenewingJwtToken.h"

#include <chrono>
#include <memory>
#include <thread>

namespace arangodb {

/**
 * Renews a token in the background, independent of requests being sent
 *
 * Example:
 *   auto const renewal = BackgroundJwtRenewal{token, std::chrono::seconds{1}};
 *   // token->renewIfDue() runs every second until renewal is destroyed
 *
 * Destruction stops the thread promptly, without waiting for the interval.
 */
class BackgroundJwtRenewal {
 public:
  BackgroundJwtRenewal(std::shared_ptr<RenewingJwtToken> token,
                       std::chrono::milliseconds checkInterval);

 private:
  std::jthread _thread;
};

}  // namespace arangodb
