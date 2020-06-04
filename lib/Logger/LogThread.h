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

#ifndef ARANGODB_LOGGER_LOG_THREAD_H
#define ARANGODB_LOGGER_LOG_THREAD_H 1

#include "Basics/ConditionVariable.h"
#include "Basics/Thread.h"

#include <boost/lockfree/queue.hpp>

namespace arangodb {
namespace application_features {
class ApplicationServer;
}
namespace basics {
class ConditionVariable;
}

struct LogMessage;

class LogThread final : public Thread {
 public:
  explicit LogThread(application_features::ApplicationServer& server,
                     std::string const& name);
  ~LogThread();

 public:
  bool isSystem() const override { return true; }
  bool isSilent() const override { return true; }
  void run() override;
  
  bool log(std::unique_ptr<LogMessage>&);
  // flush all pending log messages
  void flush() noexcept;

  // whether or not the log thread has messages queued
  bool hasMessages() const noexcept;
  // wake up the log thread from the outside
  void wakeup() noexcept;
 
  // handle all queued messages - normally this should not be called
  // by anyone, except from the crash handler
  bool processPendingMessages();

 private:
  arangodb::basics::ConditionVariable _condition;
  boost::lockfree::queue<LogMessage*> _messages;
};
}  // namespace arangodb

#endif
