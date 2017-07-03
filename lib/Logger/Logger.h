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
/// @author Achim Brandt
/// @author Dr. Frank Celler
///
/// Portions of the code are:
///
/// Copyright (c) 1999, Google Inc.
/// All rights reserved.
//
/// Redistribution and use in source and binary forms, with or without
/// modification, are permitted provided that the following conditions are
/// met:
//
///     * Redistributions of source code must retain the above copyright
///       notice, this list of conditions and the following disclaimer.
///     * Redistributions in binary form must reproduce the above
///       copyright notice, this list of conditions and the following
///       disclaimer
///       in the documentation and/or other materials provided with the
///       distribution.
///     * Neither the name of Google Inc. nor the names of its
///       contributors may be used to endorse or promote products derived
///       from this software without specific prior written permission.
///
/// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
/// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
/// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
/// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
/// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
/// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
/// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
/// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
/// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
/// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
/// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
///
/// Author: Ray Sidney
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_LOGGER_LOGGER_H
#define ARANGODB_LOGGER_LOGGER_H 1

#include "Basics/Common.h"

#include "Basics/Mutex.h"
#include "Logger/LogLevel.h"
#include "Logger/LogMacros.h"
#include "Logger/LogThread.h"
#include "Logger/LogTopic.h"

namespace arangodb {

////////////////////////////////////////////////////////////////////////////////
/// @brief message container
////////////////////////////////////////////////////////////////////////////////

struct LogMessage {
  LogMessage(LogMessage const&) = delete;
  LogMessage& operator=(LogMessage const&) = delete;

  LogMessage(LogLevel level, size_t topicId, std::string&& message,
             size_t offset)
      : _level(level),
        _topicId(topicId),
        _message(std::move(message)),
        _offset(offset) {}

  LogLevel _level;
  size_t _topicId;
  std::string const _message;
  size_t _offset;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief Logger
///
/// This class provides various static members which can be used as logging
/// streams. Output to the logging stream is appended by using the operator <<,
/// as soon as a line is completed, an implicit endl will be used to flush the
/// stream. Each line of output is prefixed by some informational data.
///
///
/// options:
///    log.level info
///    log.level compactor=debug
///    log.level replication=trace
///
///    log.output compactor=file:/a/b/c
///    log.output replication=syslog:xxxx
///    log.output performance=+
///    log.output file:/c/d/ef
///
/// deprecated:
///
///     log.file x          => log.output file:x
///     log.requests-file y => log.output requests=file:y
///     log.performance     => log.level performance=info
////////////////////////////////////////////////////////////////////////////////

class Logger {
  Logger(Logger const&) = delete;
  Logger& operator=(Logger const&) = delete;

  friend class LoggerStream;
  friend class LogThread;
  friend class LogAppenderFile;

 public:
  static LogTopic AGENCY;
  static LogTopic AGENCYCOMM;
  static LogTopic AUTHENTICATION;
  static LogTopic AUTHORIZATION;
  static LogTopic CACHE;
  static LogTopic CLUSTER;
  static LogTopic COLLECTOR;
  static LogTopic COMMUNICATION;
  static LogTopic COMPACTOR;
  static LogTopic CONFIG;
  static LogTopic DATAFILES;
  static LogTopic DEVEL;
  static LogTopic ENGINES;
  static LogTopic FIXME;
  static LogTopic GRAPHS;
  static LogTopic HEARTBEAT;
  static LogTopic MEMORY;
  static LogTopic MMAP;
  static LogTopic PERFORMANCE;
  static LogTopic PREGEL;
  static LogTopic QUERIES;
  static LogTopic REPLICATION;
  static LogTopic REQUESTS;
  static LogTopic ROCKSDB;
  static LogTopic SSL;
  static LogTopic STARTUP;
  static LogTopic SUPERVISION;
  static LogTopic SYSCALL;
  static LogTopic THREADS;
  static LogTopic TRANSACTIONS;
  static LogTopic V8;
  static LogTopic VIEWS;

 public:
  struct FIXED {
    explicit FIXED(double value, int precision = 6)
        : _value(value), _precision(precision){};
    double _value;
    int _precision;
  };

  struct BINARY {
    BINARY(void const* baseAddress, size_t size)
        : baseAddress(baseAddress), size(size){}
    explicit BINARY(std::string const& data) : BINARY(data.c_str(), data.size()) {}
    void const* baseAddress;
    size_t size;
  };

  struct RANGE {
    RANGE(void const* baseAddress, size_t size)
        : baseAddress(baseAddress), size(size){}
    void const* baseAddress;
    size_t size;
  };

  struct LINE {
    explicit LINE(int line) : _line(line){}
    int _line;
  };

  struct FILE {
    explicit FILE(char const* file) : _file(file){}
    char const* _file;
  };

  struct FUNCTION {
    explicit FUNCTION(char const* function) : _function(function){}
    char const* _function;
  };

 public:
  static LogLevel logLevel();
  static std::vector<std::pair<std::string, LogLevel>> logLevelTopics();
  static void setLogLevel(LogLevel);
  static void setLogLevel(std::string const&);
  static void setLogLevel(std::vector<std::string> const&);

  static void setOutputPrefix(std::string const&);
  static void setShowLineNumber(bool);
  static void setShortenFilenames(bool);
  static void setShowThreadIdentifier(bool);
  static void setUseLocalTime(bool);
  static bool getUseLocalTime() {return _useLocalTime;};
  static void setUseMicrotime(bool);
  static bool getUseMicrotime() {return _useMicrotime;};
  static void setKeepLogrotate(bool);

  static std::string const& translateLogLevel(LogLevel);

  static void log(char const* function, char const* file, long int line,
                  LogLevel level, size_t topicId, std::string const& message);

  static bool isEnabled(LogLevel level) {
    return (int)level <= (int)_level.load(std::memory_order_relaxed);
  }

  static bool isEnabled(LogLevel level, LogTopic const& topic) {
    return (int)level <= (int)((topic.level() == LogLevel::DEFAULT)
                                   ? _level.load(std::memory_order_relaxed)
                                   : topic.level());
  }

 public:
  static void initialize(bool);
  static void shutdown();
  static void flush();

 private:
  static Mutex _initializeMutex;

  // these varaibles might be changed asynchronously
  static std::atomic<bool> _active;
  static std::atomic<LogLevel> _level;

  // these variables must be set before calling initialized
  static bool _showLineNumber;
  static bool _shortenFilenames;
  static bool _showThreadIdentifier;
  static bool _threaded;
  static bool _useLocalTime;
  static bool _keepLogRotate;
  static bool _useMicrotime;
  static std::string _outputPrefix;

  static std::unique_ptr<LogThread> _loggingThread;
};
}

#include "Logger/LoggerStream.h"

#endif
