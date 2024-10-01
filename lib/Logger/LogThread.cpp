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
/// @author Dr. Frank Celler
////////////////////////////////////////////////////////////////////////////////

#include "LogThread.h"
#include "Basics/ScopeGuard.h"
#include "Basics/debugging.h"
#include "Logger/LogMessage.h"
#include "Logger/Logger.h"

using namespace arangodb;

LogThread::LogThread(std::string const& name, uint32_t maxQueuedLogMessages)
    : Thread(name),
      _messages(64),
      _maxQueuedLogMessages(maxQueuedLogMessages) {}

LogThread::~LogThread() {
  Logger::_active = false;

  // make sure there are no memory leaks on uncontrolled shutdown
  MessageEnvelope env{nullptr, nullptr};
  while (_messages.pop(env)) {
    delete env.msg;
  }

  shutdown();
}

bool LogThread::log(LogGroup& group, std::unique_ptr<LogMessage>& message) {
  TRI_ASSERT(message != nullptr);

  TRI_IF_FAILURE("LogThread::log") {
    // simulate a successful logging, but actually don't log anything
    return true;
  }

  bool isDirectLogLevel =
      (message->_level == LogLevel::FATAL || message->_level == LogLevel::ERR ||
       message->_level == LogLevel::WARN);

  auto numMessages =
      _pendingMessages.fetch_add(1, std::memory_order_relaxed) + 1;

  auto rollback = scopeGuard([&]() noexcept {
    // roll back the counter update
    _pendingMessages.fetch_sub(1, std::memory_order_relaxed);

    // and inform logger that we dropped a message
    // the callback function is noexcept, too.
    Logger::onDroppedMessage();
  });

  if (numMessages >= _maxQueuedLogMessages && !isDirectLogLevel) {
    // log queue is full, and the message is not important enough
    // to push it anyway. this will execute the rollback.
    return false;
  }

  if (!_messages.push({&group, message.get()})) {
    // unable to push message onto the queue.
    // this will also execute the rollback.
    return false;
  }

  rollback.cancel();

  // only release message if adding to the queue succeeded
  // otherwise we would leak here
  std::ignore = message.release();

  if (isDirectLogLevel) {
    this->flush();
  }
  return true;
}

void LogThread::flush() noexcept {
  int tries = 0;

  while (++tries < 5 && hasMessages()) {
    wakeup();
  }
}

void LogThread::wakeup() noexcept {
  std::lock_guard guard{_condition.mutex};
  _condition.cv.notify_one();
}

bool LogThread::hasMessages() const noexcept { return !_messages.empty(); }

void LogThread::run() {
  constexpr uint64_t initialWaitTime = 25 * 1000;
  constexpr uint64_t maxWaitTime = 100 * 1000;

  uint64_t waitTime = initialWaitTime;
  while (!isStopping() && Logger::_active.load()) {
    bool worked = processPendingMessages();
    if (worked) {
      waitTime = initialWaitTime;
    } else {
      waitTime *= 2;
      waitTime = std::min(maxWaitTime, waitTime);
    }

    std::unique_lock guard{_condition.mutex};
    _condition.cv.wait_for(guard, std::chrono::microseconds{waitTime});
  }

  processPendingMessages();
}

bool LogThread::processPendingMessages() {
  bool worked = false;
  MessageEnvelope env{nullptr, nullptr};

  while (_messages.pop(env)) {
    _pendingMessages.fetch_sub(1, std::memory_order_relaxed);
    worked = true;
    TRI_ASSERT(env.group != nullptr);
    TRI_ASSERT(env.msg != nullptr);
    try {
      Logger::log(*env.group, *env.msg);
    } catch (...) {
    }

    delete env.msg;
  }
  return worked;
}
