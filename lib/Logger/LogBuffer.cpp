////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2016 ArangoDB GmbH, Cologne, Germany
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

#include "LogBuffer.h"

#include <iostream>

#include "Basics/MutexLocker.h"
#include "Logger/LogAppender.h"
#include "Logger/Logger.h"

using namespace arangodb;

Mutex LogBuffer::_ringBufferLock;
uint64_t LogBuffer::_ringBufferId = 0;
LogBuffer LogBuffer::_ringBuffer[RING_BUFFER_SIZE];

static void logEntry(LogMessage* message) {
  auto timestamp = time(0);

  MUTEX_LOCKER(guard, LogBuffer::_ringBufferLock);

  uint64_t n = LogBuffer::_ringBufferId++;
  LogBuffer* ptr = &LogBuffer::_ringBuffer[n % LogBuffer::RING_BUFFER_SIZE];

  ptr->_id = n;
  ptr->_level = message->_level;
  ptr->_timestamp = timestamp;
  TRI_CopyString(ptr->_message, message->_message.c_str() + message->_offset,
                 sizeof(ptr->_message) - 1);
  ptr->_topicId = message->_topicId;
}

std::vector<LogBuffer> LogBuffer::entries(LogLevel level, uint64_t start,
                                          bool upToLevel) {
  std::vector<LogBuffer> result;

  MUTEX_LOCKER(guard, LogBuffer::_ringBufferLock);

  size_t s = 0;
  size_t n;

  if (LogBuffer::_ringBufferId >= LogBuffer::RING_BUFFER_SIZE) {
    s = (LogBuffer::_ringBufferId) % LogBuffer::RING_BUFFER_SIZE;
    n = LogBuffer::RING_BUFFER_SIZE;
  } else {
    n = static_cast<size_t>(LogBuffer::_ringBufferId);
  }

  for (size_t i = s; 0 < n; --n) {
    LogBuffer& p = LogBuffer::_ringBuffer[i];

    if (p._id >= start) {
      if (upToLevel) {
        if ((int)p._level <= (int)level) {
          result.emplace_back(p);
        }
      } else {
        if (p._level == level) {
          result.emplace_back(p);
        }
      }
    }

    ++i;

    if (i >= LogBuffer::RING_BUFFER_SIZE) {
      i = 0;
    }
  }

  return result;
}

void LogBuffer::initialize() { LogAppender::addLogger(logEntry); }
