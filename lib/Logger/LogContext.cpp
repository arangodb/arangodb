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

#include <memory>
#include <string>
#include <vector>

using namespace arangodb;

namespace {
  thread_local LogContext localLogContext;
}

LogContext::ScopedValue::~ScopedValue() {
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  TRI_ASSERT(_oldTail == LogContext::current()._tail.get());
#endif
  LogContext::current().popTail();
}

LogContext::ScopedContext::ScopedContext(LogContext ctx) {
  if (ctx._tail != LogContext::current()._tail) {
    _oldTail = ctx._tail;
    LogContext::setCurrent(std::move(ctx));
  }
}

LogContext::ScopedContext::~ScopedContext() {
  if (_oldTail) {
    LogContext::setCurrent(LogContext(std::move(_oldTail)));
  }
}

void LogContext::pushEntry(std::shared_ptr<Entry> entry) {
  entry->_prev = std::move(_tail);
  _tail = std::move(entry);
}

void LogContext::popTail() {
  TRI_ASSERT(_tail != nullptr);
  _tail = _tail->_prev;
}

void LogContext::visit(Visitor const& visitor) const {
  doVisit(visitor, _tail);
}

void LogContext::doVisit(Visitor const& visitor, std::shared_ptr<Entry> const& entry) const {
  if (entry != nullptr) {
    doVisit(visitor, entry->_prev);
    entry->visit(visitor);
  }
}

LogContext& LogContext::current() { return localLogContext; }

void LogContext::setCurrent(LogContext ctx) {
  localLogContext = ctx;
}
