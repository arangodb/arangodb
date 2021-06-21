////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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
/// @author Dr. Frank Celler
////////////////////////////////////////////////////////////////////////////////

#include <fcntl.h>
#include <stdio.h>
#include <algorithm>
#include <cstring>
#include <iostream>

#include "Basics/operating-system.h"

#ifdef TRI_HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "LogAppenderFile.h"

#include "ApplicationFeatures/ShellColorsFeature.h"
#include "Basics/Exceptions.h"
#include "Basics/FileUtils.h"
#include "Basics/debugging.h"
#include "Basics/files.h"
#include "Basics/tri-strings.h"
#include "Basics/voc-errors.h"
#include "Logger/Logger.h"

using namespace arangodb;
using namespace arangodb::basics;

std::mutex LogAppenderFile::_openAppendersMutex;
std::vector<LogAppenderFile*> LogAppenderFile::_openAppenders;

int LogAppenderFile::_fileMode = S_IRUSR | S_IWUSR | S_IRGRP;
int LogAppenderFile::_fileGroup = 0;

LogAppenderStream::LogAppenderStream(std::string const& filename, int fd)
    : LogAppender(),
      _bufferSize(0),
      _fd(fd),
      _useColors(false),
      _escape(Logger::getUseEscaped()) {}

size_t LogAppenderStream::determineOutputBufferSize(std::string const& message) const {
  if (_escape) {
    return TRI_MaxLengthEscapeControlsCString(message.size());
  }
  return message.size() + 2;
}

size_t LogAppenderStream::writeIntoOutputBuffer(std::string const& message) {
  if (_escape) {
    size_t escapedLength = 0;
    // this is guaranteed to succeed given that we already have a buffer
    TRI_EscapeControlsCString(message.data(), message.size(), _buffer.get(),
                              &escapedLength, true);
    return escapedLength;
  }

  unsigned char const* p = reinterpret_cast<unsigned char const*>(message.data());
  unsigned char const* e = p + message.size();
  char* s = _buffer.get();
  char* q = s;
  while (p < e) {
    unsigned char c = *p++;
    *q++ = c < 0x20 ? ' ' : c;
  }
  *q++ = '\n';
  *q = '\0';
  return q - s;
}

void LogAppenderStream::logMessage(LogMessage const& message) {
  // check max. required output length
  size_t const neededBufferSize = determineOutputBufferSize(message._message);

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
      return;
    }
  }

  TRI_ASSERT(_buffer != nullptr);

  size_t length = writeIntoOutputBuffer(message._message);
  TRI_ASSERT(length <= neededBufferSize);

  this->writeLogMessage(message._level, message._topicId, _buffer.get(), length);

  if (_bufferSize > maxBufferSize) {
    // free the buffer so the Logger is not hogging so much memory
    _buffer.reset();
    _bufferSize = 0;
  }
}

LogAppenderFile::LogAppenderFile(std::string const& filename) 
    : LogAppenderStream(filename, -1), _filename(filename) {
  if (_filename != "+" && _filename != "-") {
    // logging to an actual file
    std::unique_lock<std::mutex> guard(_openAppendersMutex);

    for (auto const& it : _openAppenders) {
      if (it->filename() == _filename) {
        // already have an appender for the same file
        _fd = it->fd();
        break;
      }
    }

    if (_fd == -1) {
      // no existing appender found yet
      int fd = TRI_CREATE(_filename.c_str(), O_APPEND | O_CREAT | O_WRONLY | TRI_O_CLOEXEC,
                          _fileMode);

      if (fd < 0) {
        TRI_ERRORBUF;
        TRI_SYSTEM_ERROR();
        std::cerr << "cannot write to file '" << _filename
                  << "': " << TRI_GET_ERRORBUF << std::endl;

        THROW_ARANGO_EXCEPTION(TRI_ERROR_CANNOT_WRITE_FILE);
      }

#ifdef ARANGODB_HAVE_SETGID
      if (_fileGroup != 0) {
        int result = fchown(fd, -1, _fileGroup);
        if (result != 0) {
          // we cannot log this error here, as we are the logging itself
          // so just to please compilers, we pretend we are using the result
          (void) result;
        }
      }
#endif

      _fd = fd;
      try {
        _openAppenders.emplace_back(this);
      } catch (...) {
        TRI_CLOSE(_fd);
        throw;
      }
    }
  }

  _useColors = ((isatty(_fd) == 1) && Logger::getUseColor());
}

LogAppenderFile::~LogAppenderFile() = default;

void LogAppenderFile::writeLogMessage(LogLevel level, size_t /*topicId*/, char const* buffer, size_t len) {
  bool giveUp = false;

  while (len > 0) {
    ssize_t n = TRI_WRITE(_fd, buffer, static_cast<TRI_write_t>(len));

    if (n < 0) {
      if (allowStdLogging()) {
        fprintf(stderr, "cannot log data: %s\n", TRI_LAST_ERROR_STR);
      }
      return;  // give up, but do not try to log the failure via the Logger
    }
    if (n == 0) {
      if (!giveUp) {
        giveUp = true;
        continue;
      }
    }

    buffer += n;
    len -= n;
  }

  if (level == LogLevel::FATAL) {
    FILE* f = TRI_FDOPEN(_fd, "a");
    if (f != nullptr) {
      // valid file pointer...
      // now flush the file one last time before we shut down
      fflush(f);
    }
  }
}

std::string LogAppenderFile::details() const {
  std::string buffer("More error details may be provided in the logfile '");
  buffer.append(_filename);
  buffer.append("'");

  return buffer;
}

void LogAppenderFile::reopenAll() {
  std::unique_lock<std::mutex> guard(_openAppendersMutex);

  for (auto& it : _openAppenders) {
    int old = it->fd();
    std::string const& filename = it->filename();

    if (filename.empty()) {
      continue;
    }

    if (old <= STDERR_FILENO) {
      continue;
    }

    // rename log file
    std::string backup(filename);
    backup.append(".old");

    std::ignore = FileUtils::remove(backup);
    TRI_RenameFile(filename.c_str(), backup.c_str());

    // open new log file
    int fd = TRI_CREATE(filename.c_str(), O_APPEND | O_CREAT | O_WRONLY | TRI_O_CLOEXEC,
                        _fileMode);

    if (fd < 0) {
      TRI_RenameFile(backup.c_str(), filename.c_str());
      continue;
    }

#ifdef ARANGODB_HAVE_SETGID
    if (_fileGroup != 0) {
      int result = fchown(fd, -1, _fileGroup);
      if (result != 0) {
        // we cannot log this error here, as we are the logging itself
        // so just to please compilers, we pretend we are using the result
        (void) result;
      }
    }
#endif

    if (!Logger::_keepLogRotate) {
      std::ignore = FileUtils::remove(backup);
    }

    // and also tell the appender of the file descriptor change
    it->updateFd(fd);

    if (old > STDERR_FILENO) {
      TRI_CLOSE(old);
    }
  }
}

void LogAppenderFile::closeAll() {
  std::unique_lock<std::mutex> guard(_openAppendersMutex);

  for (auto& it : _openAppenders) {
    int fd = it->fd();
    // set the fd to "disabled"
    // and also tell the appender of the file descriptor change
    it->updateFd(-1);

    if (fd > STDERR_FILENO) {
      fsync(fd);
      TRI_CLOSE(fd);
    }
  }
  _openAppenders.clear();
}

#ifdef ARANGODB_USE_GOOGLE_TESTS
std::vector<std::tuple<int, std::string, LogAppenderFile*>> LogAppenderFile::getAppenders() {
  std::vector<std::tuple<int, std::string, LogAppenderFile*>> result;

  std::unique_lock<std::mutex> guard(_openAppendersMutex);
  for (auto const& it : _openAppenders) {
    result.emplace_back(it->fd(), it->filename(), it);
  }

  return result;
}

void LogAppenderFile::setAppenders(std::vector<std::tuple<int, std::string, LogAppenderFile*>> const& appenders) {
  std::unique_lock<std::mutex> guard(_openAppendersMutex);

  _openAppenders.clear();
  for (auto const& it : appenders) {
    _openAppenders.emplace_back(std::get<2>(it));
  }
}
#endif

LogAppenderStdStream::LogAppenderStdStream(std::string const& filename, int fd)
    : LogAppenderStream(filename, fd) {
  _useColors = ((isatty(_fd) == 1) && Logger::getUseColor());
}

LogAppenderStdStream::~LogAppenderStdStream() {
  // flush output stream on shutdown
  if (allowStdLogging()) {
    FILE* fp = (_fd == STDOUT_FILENO ? stdout : stderr);
    fflush(fp);
  }
}

void LogAppenderStdStream::writeLogMessage(LogLevel level, size_t topicId, char const* buffer, size_t len) {
  writeLogMessage(_fd, _useColors, level, topicId, buffer, len, false);
}

void LogAppenderStdStream::writeLogMessage(int fd, bool useColors,
                                           LogLevel level, size_t /*topicId*/, char const* buffer,
                                           size_t len, bool appendNewline) {
  if (!allowStdLogging()) {
    return;
  }

  char const* nl = (appendNewline ? "\n" : "");
  TRI_ASSERT(buffer != nullptr);
  TRI_ASSERT(nl != nullptr);

  if (*buffer != '\0' || *nl != '\0') {
    // out stream
    FILE* fp = (fd == STDOUT_FILENO ? stdout : stderr);

    if (useColors) {
      // joyful color output
      if (level == LogLevel::FATAL || level == LogLevel::ERR) {
        fprintf(fp, "%s%s%s%s", ShellColorsFeature::SHELL_COLOR_RED, buffer,
                ShellColorsFeature::SHELL_COLOR_RESET, nl);
      } else if (level == LogLevel::WARN) {
        fprintf(fp, "%s%s%s%s", ShellColorsFeature::SHELL_COLOR_YELLOW, buffer,
                ShellColorsFeature::SHELL_COLOR_RESET, nl);
      } else {
        fprintf(fp, "%s%s%s%s", ShellColorsFeature::SHELL_COLOR_RESET, buffer,
                ShellColorsFeature::SHELL_COLOR_RESET, nl);
      }
    } else {
      // non-colored output
      fprintf(fp, "%s%s", buffer, nl);
    }

    if (level == LogLevel::FATAL || level == LogLevel::ERR ||
        level == LogLevel::WARN || level == LogLevel::INFO) {
      // flush the output so it becomes visible immediately
      // at least for log levels that are used seldomly
      // it would probably be overkill to flush everytime we
      // encounter a log message for level DEBUG or TRACE
      fflush(fp);
    }
  }
}


LogAppenderStderr::LogAppenderStderr()
      : LogAppenderStdStream("+", STDERR_FILENO) {}

LogAppenderStdout::LogAppenderStdout()
      : LogAppenderStdStream("-", STDOUT_FILENO) {}
