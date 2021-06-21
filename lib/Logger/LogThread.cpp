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
/// @author Dr. Frank Celler
////////////////////////////////////////////////////////////////////////////////

#include "LogThread.h"
#include "Basics/ConditionLocker.h"
#include "Basics/debugging.h"
#include "Logger/LogAppender.h"
#include "Logger/Logger.h"

using namespace arangodb;

LogThread::LogThread(application_features::ApplicationServer& server, std::string const& name)
    : Thread(server, name), _messages(64) {}

LogThread::~LogThread() {
  Logger::_threaded = false;
  Logger::_active = false;

  shutdown();
}

bool LogThread::log(LogGroup& group, std::unique_ptr<LogMessage>& message) {
  TRI_ASSERT(message != nullptr);

  TRI_IF_FAILURE("LogThread::log") {
    // simulate a successful logging, but actually don't log anything
    return true;
  }

  bool const isDirectLogLevel =
             (message->_level == LogLevel::FATAL || message->_level == LogLevel::ERR || message->_level == LogLevel::WARN);

  if (!_messages.push({&group, message.get()})) {
    return false;
  }

  // only release message if adding to the queue succeeded
  // otherwise we would leak here
  message.release();

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
#ifdef ARANGODB_SHOW_LOCK_TIME
  // cppcheck-suppress redundantPointerOp
  basics::ConditionLocker guard(_condition, __FILE__, __LINE__, false);
#else
  // cppcheck-suppress redundantPointerOp
  CONDITION_LOCKER(guard, _condition);
#endif
  guard.signal();
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

    // cppcheck-suppress redundantPointerOp
    CONDITION_LOCKER(guard, _condition);
    guard.wait(waitTime);
  }

  processPendingMessages();
}

bool LogThread::processPendingMessages() {
  bool worked = false;
  MessageEnvelope env{nullptr, nullptr};

  while (_messages.pop(env)) {
    worked = true;
    TRI_ASSERT(env.group != nullptr);
    TRI_ASSERT(env.msg != nullptr);
    try {
      LogAppender::log(*env.group, *env.msg);
    } catch (...) {
    }

    delete env.msg;
  }
  return worked;
}
