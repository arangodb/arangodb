////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2013 triAGENS GmbH, Cologne, Germany
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
#include "Logger/LogAppender.h"
#include "Logger/Logger.h"

using namespace arangodb;

LogThread::LogThread(application_features::ApplicationServer& server, std::string const& name)
    : Thread(server, name), _messages(0) {}

LogThread::~LogThread() {
  Logger::_threaded = false;
  Logger::_active = false;

  shutdown();
}

bool LogThread::log(std::unique_ptr<LogMessage>& message) {
  if (_messages.push(message.get())) {
    // only release message if adding to the queue succeeded
    // otherwise we would leak here
    message.release();
    return true;
  }
  return false;
}

void LogThread::flush() noexcept {
  int tries = 0;

  while (++tries < 5 && hasMessages()) {
    wakeup();
  }
}

void LogThread::wakeup() {
  // cppcheck-suppress redundantPointerOp
  CONDITION_LOCKER(guard, _condition);
  guard.signal();
}

bool LogThread::hasMessages() const noexcept { return !_messages.empty(); }

void LogThread::run() {
  while (!isStopping() && Logger::_active.load()) {
    processPendingMessages();

    // cppcheck-suppress redundantPointerOp
    CONDITION_LOCKER(guard, _condition);
    guard.wait(25 * 1000);
  }

  processPendingMessages();
}

void LogThread::processPendingMessages() {
  LogMessage* msg = nullptr;

  while (_messages.pop(msg)) {
    TRI_ASSERT(msg != nullptr);
    try {
      LogAppender::log(*msg);
    } catch (...) {
    }

    delete msg;
  }
}
