////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "RocksDBLogger.h"
#include "Basics/StringRef.h"
#include "Logger/Logger.h"

using namespace arangodb;

RocksDBLogger::RocksDBLogger(rocksdb::InfoLogLevel level) : rocksdb::Logger(level), _enabled(true) {}
RocksDBLogger::~RocksDBLogger() {}
  
void RocksDBLogger::Logv(const rocksdb::InfoLogLevel logLevel, char const* format, va_list ap) {
  if (logLevel < GetInfoLogLevel()) {
    return;
  }
  if (!_enabled) {
    return;
  }
  
  static constexpr size_t prefixSize = 9; // strlen("rocksdb: ");
  // truncate all log messages after this length
  char buffer[4096];
  memcpy(&buffer[0], "rocksdb: \0", strlen("rocksdb: \0")); // add trailing \0 byte already for safety

  va_list backup;
  va_copy(backup, ap);
  int length = vsnprintf(&buffer[0] + prefixSize, sizeof(buffer) - prefixSize - 1, format, backup);
  va_end(backup);
  buffer[sizeof(buffer) - 1] = '\0';  // Windows

  if (length == 0) {
    return;
  }

  size_t l = static_cast<size_t>(length) + prefixSize;
  if (l >= sizeof(buffer)) {
    // truncation!
    l = sizeof(buffer) - 1;
  }

  TRI_ASSERT(l > 0 && l < sizeof(buffer));
  if (buffer[l - 1] == '\n' || buffer[l - 1] == '\0') {
    // strip tailing \n or \0 in log message
    --l;
  }

  switch (logLevel) {
    case rocksdb::InfoLogLevel::DEBUG_LEVEL:
      LOG_TOPIC(DEBUG, arangodb::Logger::ROCKSDB) << StringRef(buffer, l);
      break;
    case rocksdb::InfoLogLevel::INFO_LEVEL:
      LOG_TOPIC(INFO, arangodb::Logger::ROCKSDB) << StringRef(buffer, l);
      break;
    case rocksdb::InfoLogLevel::WARN_LEVEL:
      LOG_TOPIC(WARN, arangodb::Logger::ROCKSDB) << StringRef(buffer, l);
      break;
    case rocksdb::InfoLogLevel::ERROR_LEVEL:
    case rocksdb::InfoLogLevel::FATAL_LEVEL:
      LOG_TOPIC(ERR, arangodb::Logger::ROCKSDB) << StringRef(buffer, l);
      break;
    default: { 
      // ignore other levels 
    }
  }
}

void RocksDBLogger::Logv(char const* format, va_list ap) {
  // forward to the level-aware method
  Logv(rocksdb::InfoLogLevel::INFO_LEVEL, format, ap);
}
