////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <type_traits>
#include <vector>

namespace arangodb {

class LogContext {
 private:
  struct Impl;
  
 public:
  /// @brief sets the key/value pair in the LogContext for the current scope.
  /// If an entry with the specified key already exists it is overwritten,
  /// otherwise a new key/value pair is added.
  /// Returns a `Scoped` instance that restores the previous state. That is,
  /// if the value of an existing entry was overwritten, the old value is
  /// restored, otherwise the key/value pair is removed.
  struct ScopedValue {
    ScopedValue(std::string key, std::string value);
    ~ScopedValue();
  private:
    std::size_t _idx;
    std::optional<std::string> _oldValue;
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
    std::string _newValue;
#endif
  };

  /// @brief merges the key/value pairs from the given context into the local
  /// LogContext for the current scope.
  struct ScopedMerge {
    ScopedMerge(LogContext ctx);
   private:
    std::vector<ScopedValue> _values;
  };

  /// @brief sets the given LogContext as the thread's current context for the
  /// current scope; restores the previous LogContext upon destruction.
  struct ScopedContext {
    ScopedContext(LogContext ctx);
    ~ScopedContext();
   private:
    std::shared_ptr<Impl> _oldContext;
  };
  
  LogContext();

  /// @brief iterates through the local LogContext's key/value pairs and calls
  /// the callback function for each of them.
  void each(std::function<void(std::string const&, std::string const&)> const&) const;
  
  /// @brief returns the local LogContext
  static LogContext& current();
  
  /// @brief sets the given context as the current LogContext.
  static void setCurrent(LogContext ctx);
  
 private:
  LogContext(std::shared_ptr<Impl> impl) : _impl(std::move(impl)) {}
  
  auto set(std::string key, std::string value) -> std::pair<std::size_t, std::optional<std::string>>;
  
  void restore(std::size_t index, std::optional<std::string>&& value);
  
  void ensureUniqueOwner();
  
  std::shared_ptr<Impl> _impl;
};

template <typename Func>
auto withLogContext(Func&& func) {
  return [func = std::forward<Func>(func), ctx = LogContext::current()](auto&&... args) mutable {
    LogContext::ScopedContext ctxGuard(ctx);   
    return std::forward<Func>(func)(std::forward<decltype(args)>(args)...);
  };
}

}  // namespace arangodb

