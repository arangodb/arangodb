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
/// @author Julia Volmer
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include "Pregel/Worker/Handler.h"
#include "Pregel/Worker/Messages.h"
#include "Pregel/Worker/State.h"

namespace arangodb::pregel::worker {

template<typename V, typename E, typename M>
struct WorkerActor {
  using State = WorkerState<V, E, M>;
  using Message = message::WorkerMessages;
  template<typename Runtime>
  using Handler = WorkerHandler<V, E, M, Runtime>;
  static constexpr auto typeName() -> std::string_view { return "WorkerActor"; }
};

}  // namespace arangodb::pregel::worker
