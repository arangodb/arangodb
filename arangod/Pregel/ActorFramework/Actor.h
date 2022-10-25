////////////////////////////////////////////////////////////////////////////////
///
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
/// @author Markus Pfeiffer
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include "Pid.h"
#include "Runtime.h"
#include "Message.h"
#include "fmt/core.h"

#include <memory>

namespace arangodb::actor_framework {

struct Runtime;

struct ActorBase {
  PidT pid;
  PidT parentPid;
  Runtime& rt;

  ActorBase(PidT pid, Runtime& rt) : pid(pid), rt(rt) {}
  virtual ~ActorBase() = default;

  template<typename M, typename... Args>
  void send(PidT recipient, Args... args) {
    rt.send(pid, recipient, std::make_unique<M>(std::forward<Args>(args)...));
  }

  virtual void process(PidT from, std::unique_ptr<MsgPayloadBase> msg) = 0;
};

}  // namespace arangodb::actor_framework
