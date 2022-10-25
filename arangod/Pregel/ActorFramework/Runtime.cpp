
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

#include "Runtime.h"
#include "Actor.h"

#include <fmt/core.h>

namespace arangodb::actor_framework {

auto Runtime::run() -> void {
  fmt::print("Message processor\n");
  size_t processed = 0;
  size_t i = 0;
  do {
    processed = 0;

    auto& a = actors[i];
    auto pid = a.actor->pid;

    std::vector<MsgBase> msgs;
    for (auto&& m : a.inbox) {
      msgs.push_back(std::move(m));
    }
    a.inbox.clear();
    processed += msgs.size();
    for (auto&& m : msgs) {
      a.actor->process(m.sender, std::move(m.payload));
    }

    fmt::print("processing {} messages for {} done\n", msgs.size(), pid.pid);

    i = (i + 1) % actors.size();
  } while (true);
}
}  // namespace arangodb::actor_framework
