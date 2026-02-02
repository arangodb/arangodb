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
#include "Logger/LogContext.h"
#include "Utils/ExecContext.h"
#include "ActivityRegistry/registry.h"

namespace arangodb {

/**
   Global context in arangodb

   In an asynchronous coroutine we need to capture this context when suspending
   and resetting it when resuming to make sure that the global variables are set
   correctly.
 */
struct Context {
  std::shared_ptr<ExecContext const> _execContext;
  async_registry::CurrentRequester _requester;
  LogContext _logContext;
  activity_registry::ActivityId _currentlyExecutingActivity;

  Context()
      : _execContext{ExecContext::currentAsShared()},
        _requester{std::move(*async_registry::get_current_coroutine())},
        _logContext{LogContext::current()},
        _currentlyExecutingActivity{
            activity_registry::Registry::currentlyExecutingActivity()} {}

  Context(Context const& other) = delete;
  auto operator=(Context const& other) -> Context& = delete;
  auto operator=(Context&& other) noexcept -> Context& = default;
  Context(Context&& other) = default;

  auto set() -> void {
    ExecContext::set(_execContext);
    if (_requester != *async_registry::get_current_coroutine()) {
      *async_registry::get_current_coroutine() = _requester;
    }
    LogContext::setCurrent(_logContext);
    activity_registry::Registry::setCurrentlyExecutingActivity(
        _currentlyExecutingActivity);
  }

  auto update() -> void {
    _execContext = ExecContext::currentAsShared();
    if (_requester != *async_registry::get_current_coroutine()) {
      _requester = *async_registry::get_current_coroutine();
    }
    _logContext = LogContext::current();
    _currentlyExecutingActivity =
        activity_registry::Registry::currentlyExecutingActivity();
  }

  ~Context() = default;
};

}  // namespace arangodb
