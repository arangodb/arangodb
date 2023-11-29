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
/// @author Manuel Pöter
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <string>
#include <cstdint>
#include <memory>

#include "Actor/Actor.h"
#include "Actor/ActorList.h"
#include "Actor/ActorID.h"
#include "Actor/Assert.h"
#include "Actor/BaseRuntime.h"
#include "Actor/LocalActorPID.h"

namespace arangodb::actor {

struct LocalRuntime : BaseRuntime<LocalRuntime, LocalActorPID> {
  using Base = BaseRuntime<LocalRuntime, LocalActorPID>;

  LocalRuntime() = delete;
  LocalRuntime(LocalRuntime const&) = delete;
  LocalRuntime(LocalRuntime&&) = delete;
  LocalRuntime(std::string runtimeID, std::shared_ptr<IScheduler> scheduler)
      : Base(std::move(runtimeID), std::move(scheduler)) {}

  template<typename Inspector>
  friend inline auto inspect(Inspector& f, LocalRuntime& x) {
    return f.apply(static_cast<Base&>(x));
  }

 private:
  template<typename Derived, typename PID>
  friend struct BaseRuntime;

  ActorPID makePid(ActorID id) { return ActorPID{.id = id}; }

  template<typename ActorMessage>
  auto doDispatch(ActorPID sender, ActorPID receiver, ActorMessage&& message,
                  IgnoreDispatchFailure ignoreFailure) -> void {
    dispatchLocally(sender, receiver, std::move(message), ignoreFailure);
  }
};

};  // namespace arangodb::actor
