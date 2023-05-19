////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "RestServer/arangod.h"

#include <shared_mutex>

namespace arangodb {

class FoxxFeature final : public ArangodFeature {
 public:
  static constexpr std::string_view name() noexcept { return "FoxxQueues"; }

  explicit FoxxFeature(Server& server);

  void collectOptions(std::shared_ptr<options::ProgramOptions>) override final;
  void validateOptions(std::shared_ptr<options::ProgramOptions>) override final;
  void prepare() override final;

  // return poll interval for foxx queues. returns a negative number if
  // foxx queues are turned off
  double pollInterval() const noexcept;

  bool startupWaitForSelfHeal() const noexcept;

  bool foxxEnabled() const noexcept;

  // store the locally applied version of the queue. this method will make sure
  // we will not go below any version that we have already seen.
  uint64_t setQueueVersion(uint64_t version) noexcept;

  // return the locally applied queue version.
  uint64_t queueVersion() const noexcept;

  // send a queue version increment command to the agency. this way all other
  // coordinators in the cluster get notified of queue updates.
  // note: this method will only do something if there have been some local
  // queue updates. once the agency was informed successfully, this method
  // will reset the tracked number of updates back to 0.
  void bumpQueueVersionIfRequired();

  // track an insert into a Foxx queue on this coordinator. this will increase
  // a counter. whenever the counter is > 0, it will be eventually flushed by
  // the Foxx queues manager thread, who will send a version increase command
  // to the agency. this way all other coordinators in the cluster get notified
  // of queue updates.
  void trackLocalQueueInsert() noexcept;

 private:
  mutable std::shared_mutex _queueLock;
  uint64_t _queueVersion;
  uint64_t _localQueueInserts;

  double _queuesPollInterval;
  bool _queuesEnabled;
  bool _startupWaitForSelfHeal;
  bool _foxxEnabled;
};

}  // namespace arangodb
