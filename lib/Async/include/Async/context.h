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
/// @author Julia Volmer
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include "Async/Registry/promise.h"
#include "TaskMonitoring/task.h"
#include "Utils/ExecContext.h"

namespace arangodb {

/**
   Global context in arangodb

   In an asyncronous coroutine we need to capture this context when suspending
   and resetting it when resuming to make sure that the global variables are set
   correctly.
 */
struct Context {
  std::shared_ptr<ExecContext const> _execContext;
  async_registry::Requester _requester;
  task_monitoring::Task* _task;

  Context()
      : _execContext{ExecContext::currentAsShared()},
        _requester{*async_registry::get_current_coroutine()},
        _task{*task_monitoring::get_current_task()} {}

  auto set() -> void {
    ExecContext::set(_execContext);
    *async_registry::get_current_coroutine() = _requester;
    *task_monitoring::get_current_task() = _task;
  }
};

}  // namespace arangodb
