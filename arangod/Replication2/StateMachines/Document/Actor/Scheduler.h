////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2023-2023 ArangoDB GmbH, Cologne, Germany
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
/// @author Manuel Pöter
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <memory>
#include "Actor/IScheduler.h"
#include "Replication2/IScheduler.h"

namespace arangodb {
class Scheduler;
}

namespace arangodb::replication2::replicated_state::document::actor {

struct Scheduler : arangodb::actor::IScheduler {
  explicit Scheduler(std::shared_ptr<replication2::IScheduler> scheduler);
  void queue(arangodb::actor::LazyWorker&& worker) override;
  void delay(std::chrono::seconds delay,
             std::function<void(bool)>&& fn) override;

 private:
  std::shared_ptr<replication2::IScheduler> _scheduler;
};

}  // namespace arangodb::replication2::replicated_state::document::actor
