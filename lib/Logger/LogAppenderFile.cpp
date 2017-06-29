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

#include "Logger/LogAppenderFile.h"

#include "Basics/Exceptions.h"
#include "Basics/FileUtils.h"
#include "Basics/OpenFilesTracker.h"
#include "Logger/Logger.h"

#include <iostream>

using namespace arangodb;
using namespace arangodb::basics;

std::vector<std::pair<int, std::string>> LogAppenderFile::_fds = {
    {STDOUT_FILENO, "-"}, {STDERR_FILENO, "+"}};

void LogAppenderFile::reopen() {
  for (size_t pos = 2; pos < _fds.size(); ++pos) {
    int old = _fds[pos].first;
    std::string const& filename = _fds[pos].second;

    if (filename.empty()) {
      continue;
    }

    if (old <= STDERR_FILENO) {
      continue;
    }

    // rename log file
    std::string backup(filename);
    backup.append(".old");

    FileUtils::remove(backup);
    FileUtils::rename(filename, backup);

    // open new log file
    int fd = TRI_TRACKED_CREATE_FILE(filename.c_str(),
                        O_APPEND | O_CREAT | O_WRONLY | TRI_O_CLOEXEC,
                        S_IRUSR | S_IWUSR | S_IRGRP);

    if (fd < 0) {
      FileUtils::rename(backup, filename);
      continue;
    }

    if (!Logger::_keepLogRotate) {
      FileUtils::remove(backup);
    }

    _fds[pos].first = fd;

    if (old > STDERR_FILENO) {
      TRI_TRACKED_CLOSE_FILE(old);
    }
  }
}

void LogAppenderFile::close() {
  for (size_t pos = 2; pos < _fds.size(); ++pos) {
    int fd = _fds[pos].first;
    _fds[pos].first = -1;

    if (fd > STDERR_FILENO) {
      TRI_TRACKED_CLOSE_FILE(fd);
    }
  }
}

LogAppenderFile::LogAppenderFile(std::string const& filename,
                                 std::string const& filter)
    : LogAppender(filter), _pos(-1), _bufferSize(0) {
  // logging to stdout
  if (filename == "-") {
    _pos = 0;
  }

  // logging to stderr
  else if (filename == "+") {
    _pos = 1;
  }

  // logging to a file
  else {
    size_t pos = 2;

    for (; pos < _fds.size(); ++pos) {
      if (_fds[pos].second == filename) {
        break;
      }
    }

    if (pos == _fds.size() || _fds[pos].first == -1) {
      int fd = TRI_TRACKED_CREATE_FILE(filename.c_str(),
                          O_APPEND | O_CREAT | O_WRONLY | TRI_O_CLOEXEC,
                          S_IRUSR | S_IWUSR | S_IRGRP);

      if (fd < 0) {
        TRI_ERRORBUF;
        TRI_SYSTEM_ERROR();
        std::cerr << "cannot write to file '" << filename << "': " << TRI_GET_ERRORBUF << std::endl;

        THROW_ARANGO_EXCEPTION(TRI_ERROR_CANNOT_WRITE_FILE);
      }

      if (pos == _fds.size()) {
        _fds.emplace_back(std::make_pair(fd, filename));
      } else {
        _fds[pos].first = fd;
      }

      _pos = static_cast<ssize_t>(pos);
    }
  }
}
  
LogAppenderFile::~LogAppenderFile() {}

bool LogAppenderFile::logMessage(LogLevel level, std::string const& message,
                                 size_t offset) {
  if (_pos < 0) {
    return false;
  }

  int fd = _fds[_pos].first;

  if (fd < 0) {
    return false;
  }

  // check max. required output length
  size_t const neededBufferSize = TRI_MaxLengthEscapeControlsCString(message.size());
  
  // check if we can re-use our already existing buffer
  if (neededBufferSize > _bufferSize) {
    _buffer.reset();
    _bufferSize = 0;
  }

  if (_buffer == nullptr) {
    // create a new buffer
    try {
      // grow buffer exponentially
      _buffer.reset(new char[neededBufferSize * 2]);
      _bufferSize = neededBufferSize * 2;
    } catch (...) {
      // if allocation fails, simply give up
      return false;
    }
  }
  
  TRI_ASSERT(_buffer != nullptr);
  
  size_t escapedLength;
  // this is guaranteed to succeed given that we already have a buffer
  TRI_EscapeControlsCString(message.c_str(), message.size(), _buffer.get(), &escapedLength, true);
  TRI_ASSERT(escapedLength <= neededBufferSize);

  writeLogFile(level, fd, _buffer.get(), static_cast<ssize_t>(escapedLength));

  if (_bufferSize > 16384) {
    // free the buffer so the Logger is not hogging so much memory
    _buffer.reset();
    _bufferSize = 0;
  }

  return (isatty(fd) != 0);
}

std::string LogAppenderFile::details() {
  int fd = _fds[_pos].first;

  if (fd != STDOUT_FILENO && fd != STDERR_FILENO) {
    std::string buffer("More error details may be provided in the logfile '");
    buffer.append(_fds[_pos].second);
    buffer.append("'");

    return buffer;
  }

  return "";
}

void LogAppenderFile::writeLogFile(LogLevel level, int fd, char const* buffer, ssize_t len) {
  bool giveUp = false;

  while (len > 0) {
    ssize_t n = TRI_WRITE(fd, buffer, len);

    if (n < 0) {
      fprintf(stderr, "cannot log data: %s\n", TRI_LAST_ERROR_STR);
      return;  // give up, but do not try to log failure
    } else if (n == 0) {
      if (!giveUp) {
        giveUp = true;
        continue;
      }
    }

    buffer += n;
    len -= n;
  }

  if (level == LogLevel::FATAL) {
    FILE* f = TRI_FDOPEN(fd, "w+");
    if (f != nullptr) {
      fflush(f);
    }
  }
}
