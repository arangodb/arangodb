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

#include "LogContext.h"

#include "Basics/debugging.h"

#include <string>
#include <vector>

using namespace arangodb;

namespace {
  thread_local LogContext localLogContext;
}

struct LogContext::Impl {
  std::vector<std::pair<std::string, std::string>> values;
};

LogContext::ScopedValue::ScopedValue(std::string key, std::string value) {
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  _newValue = value;
#endif
  std::tie(_idx, _oldValue) = LogContext::current().set(std::move(key), std::move(value));
}

LogContext::ScopedValue::~ScopedValue() {
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  TRI_ASSERT(_idx < LogContext::current()._impl->values.size());
  TRI_ASSERT(LogContext::current()._impl->values[_idx].second == _newValue);
#endif
  LogContext::current().restore(_idx, std::move(_oldValue));
}

LogContext::ScopedMerge::ScopedMerge(LogContext ctx) {
  _values.reserve(ctx._impl->values.size());
  for (auto const& v : ctx._impl->values) {
    _values.emplace_back(v.first, v.second);
  }
}

LogContext::ScopedContext::ScopedContext(LogContext ctx) {
  if (ctx._impl != LogContext::current()._impl) {
    _oldContext = ctx._impl;
    LogContext::setCurrent(std::move(ctx));
  }
}

LogContext::ScopedContext::~ScopedContext() {
  if (_oldContext) {
    LogContext::setCurrent(LogContext(std::move(_oldContext)));
  }
}
  
LogContext::LogContext() : _impl(std::make_shared<Impl>()) {}

std::pair<std::size_t, std::optional<std::string>> LogContext::set(std::string key, std::string value) {
  ensureUniqueOwner();
  std::optional<std::string> result;
  for (std::size_t i = 0; i < _impl->values.size(); ++i) {
    auto& v = _impl->values[i];
    if (v.first == key) {
      std::swap(v.second, value);
      result = std::move(value);
      return {i, result};
    }
  }
  _impl->values.emplace_back(std::move(key), std::move(value));
  return {_impl->values.size() - 1, result};
}
  
void LogContext::restore(std::size_t index, std::optional<std::string>&& value) {
  ensureUniqueOwner();
  TRI_ASSERT(index < _impl->values.size());
  if (!value.has_value()) {
    TRI_ASSERT(index == _impl->values.size() - 1);
    _impl->values.pop_back();
  } else {
    _impl->values[index].second = std::move(value.value());
  }
}

void LogContext::ensureUniqueOwner() {
  if (!_impl.unique()) {
    // this is a very simple copy-on-write implementation, but we can easily switch
    // to something more elaborate like `immer` later.
    _impl = std::make_shared<Impl>(*_impl);
  }
}

void LogContext::each(std::function<void(std::string const&, std::string const&)> cb) {
  for (auto& v : _impl->values) {
    cb(v.first, v.second);
  }
}

LogContext& LogContext::current() { return localLogContext; }

void LogContext::setCurrent(LogContext ctx) {
  localLogContext = ctx;
}
