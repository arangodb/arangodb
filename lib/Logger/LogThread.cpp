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

arangodb::basics::ConditionVariable* LogThread::CONDITION = nullptr;
boost::lockfree::queue<LogMessage*>* LogThread::MESSAGES = nullptr;

LogThread::LogThread(std::string const& name) : Thread(name), _messages(0) {
  MESSAGES = &_messages;
  CONDITION = &_condition;
}

LogThread::~LogThread() {
  Logger::_threaded = false;
  Logger::_active = false;

  beginShutdown();
  shutdown();
}

void LogThread::log(std::unique_ptr<LogMessage>& message) {
  if (MESSAGES->push(message.get())) {
    // only release message if adding to the queue succeeded
    // otherwise we would leak here
    message.release();
  }
}

void LogThread::flush() {
  int tries = 0;

  while (++tries < 500) {
    if (MESSAGES->empty()) {
      break;
    }

    CONDITION_LOCKER(guard, *CONDITION);
    guard.signal();
  }
}

void LogThread::wakeup() {
  CONDITION_LOCKER(guard, *CONDITION);
  guard.signal();
}

bool LogThread::hasMessages() {
  return (!MESSAGES->empty());
}

void LogThread::run() {
  LogMessage* msg;

  while (!isStopping() && Logger::_active.load()) {
    while (_messages.pop(msg)) {
      try {
        LogAppender::log(msg);
      } catch (...) {
      }

      delete msg;
    }

    CONDITION_LOCKER(guard, *CONDITION);
    guard.wait(25 * 1000);
  }

  while (_messages.pop(msg)) {
    delete msg;
  }
}
