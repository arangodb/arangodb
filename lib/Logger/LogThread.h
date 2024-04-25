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

#pragma once

#include "Basics/ConditionVariable.h"
#include "Basics/Thread.h"

#include <boost/lockfree/queue.hpp>

#include <atomic>
#include <cstdint>

namespace arangodb {
class LogGroup;
namespace application_features {
class ApplicationServer;
}
namespace basics {
struct ConditionVariable;
}

struct LogMessage;

class LogThread final : public Thread {
  struct MessageEnvelope {
    LogGroup* group;
    LogMessage* msg;
  };

 public:
  explicit LogThread(application_features::ApplicationServer& server,
                     std::string const& name, uint32_t maxQueuedLogMessages);
  ~LogThread();

  bool isSystem() const override { return true; }
  bool isSilent() const override { return true; }
  void run() override;

  bool log(LogGroup&, std::unique_ptr<LogMessage>&);
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
  basics::ConditionVariable _condition;
  boost::lockfree::queue<MessageEnvelope> _messages;
  std::atomic<size_t> _pendingMessages{0};
  uint32_t _maxQueuedLogMessages{10000};
};
}  // namespace arangodb
