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

#include "ApplicationFeatures/ShellColorsFeature.h"
#include "Basics/Exceptions.h"
#include "Basics/FileUtils.h"
#include "Basics/OpenFilesTracker.h"
#include "Basics/tri-strings.h"
#include "Logger/Logger.h"

#include <iostream>

using namespace arangodb;
using namespace arangodb::basics;

std::vector<std::pair<int, std::string>> LogAppenderFile::_fds = {};

LogAppenderStream::LogAppenderStream(std::string const& filename,
                                     std::string const& filter, int fd)
    : LogAppender(filter), _bufferSize(0), _fd(fd), _isTty(false) {}
  
bool LogAppenderStream::logMessage(LogLevel level, std::string const& message,
                                   size_t offset) {
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

  this->writeLogMessage(level, _buffer.get(), escapedLength);

  if (_bufferSize > maxBufferSize) {
    // free the buffer so the Logger is not hogging so much memory
    _buffer.reset();
    _bufferSize = 0;
  }

  return _isTty;
}

LogAppenderFile::LogAppenderFile(std::string const& filename, std::string const& filter)
    : LogAppenderStream(filename, filter, -1), _filename(filename) {

  if (_filename != "+" && _filename != "-") {
    // logging to an actual file
    size_t pos = 0;
    for (auto& it : _fds) {
      if (it.second == _filename) {
        // already have an appender for the same file
        _fd = it.first;
        break;
      }
      ++pos;
    }

    if (_fd == -1) {
      // no existing appender found yet
      int fd = TRI_TRACKED_CREATE_FILE(_filename.c_str(),
                          O_APPEND | O_CREAT | O_WRONLY | TRI_O_CLOEXEC,
                          S_IRUSR | S_IWUSR | S_IRGRP);

      if (fd < 0) {
        TRI_ERRORBUF;
        TRI_SYSTEM_ERROR();
        std::cerr << "cannot write to file '" << _filename << "': " << TRI_GET_ERRORBUF << std::endl;

        THROW_ARANGO_EXCEPTION(TRI_ERROR_CANNOT_WRITE_FILE);
      }

      _fds.emplace_back(std::make_pair(fd, _filename));
      _fd = fd;
    }
  }
  
  _isTty = (isatty(_fd) == 1);
}

void LogAppenderFile::writeLogMessage(LogLevel level, char const* buffer, size_t len) {
  bool giveUp = false;
  int fd = _fd;

  while (len > 0) {
    ssize_t n = TRI_WRITE(fd, buffer, static_cast<TRI_write_t>(len));

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

std::string LogAppenderFile::details() {
  std::string buffer("More error details may be provided in the logfile '");
  buffer.append(_filename);
  buffer.append("'");

  return buffer;
}

void LogAppenderFile::reopenAll() {
  for (auto& it : _fds) {
    int old = it.first;
    std::string const& filename = it.second;

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

    it.first = fd;

    if (old > STDERR_FILENO) {
      TRI_TRACKED_CLOSE_FILE(old);
    }
  }
}

void LogAppenderFile::closeAll() {
  for (auto& it : _fds) {
    int fd = it.first;
    it.first = -1;

    if (fd > STDERR_FILENO) {
      TRI_TRACKED_CLOSE_FILE(fd);
    }
  }
}
  
LogAppenderStdStream::LogAppenderStdStream(std::string const& filename, std::string const& filter, int fd)
    : LogAppenderStream(filename, filter, fd) {
  _isTty = (isatty(_fd) == 1);
}

LogAppenderStdStream::~LogAppenderStdStream() {
  // flush output stream on shutdown
  FILE* fp = (_fd == STDOUT_FILENO ? stdout : stderr);
  fflush(fp);
}
  
void LogAppenderStdStream::writeLogMessage(LogLevel level, char const* buffer, size_t len) {
  writeLogMessage(_fd, _isTty, level, buffer, len, false);
}

void LogAppenderStdStream::writeLogMessage(int fd, bool isTty, LogLevel level, char const* buffer, size_t len, bool appendNewline) {
  // out stream
  FILE* fp = (fd == STDOUT_FILENO ? stdout : stderr);
  char const* nl = (appendNewline ? "\n" : "");

  if (isTty) {
    // joyful color output
    if (level == LogLevel::FATAL || level == LogLevel::ERR) {
      fprintf(fp, "%s%s%s%s", ShellColorsFeature::SHELL_COLOR_RED, buffer, ShellColorsFeature::SHELL_COLOR_RESET, nl);
    } else if (level == LogLevel::WARN) {
      fprintf(fp, "%s%s%s%s", ShellColorsFeature::SHELL_COLOR_YELLOW, buffer, ShellColorsFeature::SHELL_COLOR_RESET, nl);
    } else {
      fprintf(fp, "%s%s%s%s", ShellColorsFeature::SHELL_COLOR_RESET, buffer, ShellColorsFeature::SHELL_COLOR_RESET, nl);
    }
  } else {
    // non-colored output
    fprintf(fp, "%s%s", buffer, nl);
  }

  if (level == LogLevel::FATAL || level == LogLevel::ERR || level == LogLevel::WARN) {
    // flush the output so it becomes visible immediately
    fflush(fp);
  }
}
