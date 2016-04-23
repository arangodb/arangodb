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

#ifndef ARANGODB_LOGGER_LOG_BUFFER_H
#define ARANGODB_LOGGER_LOG_BUFFER_H 1

#include "Basics/Common.h"

#include "Basics/Mutex.h"
#include "Logger/LogLevel.h"

namespace arangodb {

struct LogBuffer {
  static size_t const RING_BUFFER_SIZE = 10240;
  static Mutex _ringBufferLock;
  static uint64_t _ringBufferId;
  static LogBuffer _ringBuffer[];

  static std::vector<LogBuffer> entries(LogLevel, uint64_t start,
                                        bool upToLevel);
  static void initialize();

  uint64_t _id;
  LogLevel _level;
  time_t _timestamp;
  char _message[256];
};
}

#endif
