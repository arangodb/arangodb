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

#include <iostream>

#include "Basics/Exceptions.h"
#include "Basics/FileUtils.h"

using namespace arangodb;
using namespace arangodb::basics;

LogAppenderFile::LogAppenderFile(std::string const& filename,
                                 std::string const& filter)
    : LogAppender(filter), _filename(filename), _fd(-1) {
  // logging to stdout
  if (_filename == "+") {
    _fd.store(STDOUT_FILENO);
  }

  // logging to stderr
  else if (_filename == "-") {
    _fd.store(STDERR_FILENO);
  }

  // logging to file
  else {
    int fd = TRI_CREATE(filename.c_str(),
                        O_APPEND | O_CREAT | O_WRONLY | TRI_O_CLOEXEC,
                        S_IRUSR | S_IWUSR | S_IRGRP);

    if (fd < 0) {
      std::cerr << "cannot write to file '" << filename << "'" << std::endl;

      THROW_ARANGO_EXCEPTION(TRI_ERROR_CANNOT_WRITE_FILE);
    }

    _fd.store(fd);
  }
}

void LogAppenderFile::logMessage(LogLevel level, std::string const& message,
                                 size_t offset) {
  int fd = _fd.load();

  if (fd < 0) {
    return;
  }

  size_t escapedLength;
  char* escaped =
      TRI_EscapeControlsCString(TRI_UNKNOWN_MEM_ZONE, message.c_str(),
                                message.size(), &escapedLength, true, false);

  if (escaped != nullptr) {
    writeLogFile(fd, escaped, static_cast<ssize_t>(escapedLength));
    TRI_FreeString(TRI_UNKNOWN_MEM_ZONE, escaped);
  }
}

void LogAppenderFile::reopenLog() {
  if (_filename.empty()) {
    return;
  }

  if (_fd <= STDERR_FILENO) {
    return;
  }

  // rename log file
  std::string backup(_filename);
  backup.append(".old");

  FileUtils::remove(backup);
  FileUtils::rename(_filename, backup);

  // open new log file
  int fd = TRI_CREATE(_filename.c_str(),
                      O_APPEND | O_CREAT | O_WRONLY | TRI_O_CLOEXEC,
                      S_IRUSR | S_IWUSR | S_IRGRP);

  if (fd < 0) {
    TRI_RenameFile(backup.c_str(), _filename.c_str());
    return;
  }

  int old = std::atomic_exchange(&_fd, fd);

  if (old > STDERR_FILENO) {
    TRI_CLOSE(old);
  }
}

void LogAppenderFile::closeLog() {
  int fd = _fd.exchange(-1);

  if (fd > STDERR_FILENO) {
    TRI_CLOSE(fd);
  }
}

std::string LogAppenderFile::details() {
  if (_filename.empty()) {
    return "";
  }

  int fd = _fd.load();

  if (fd != STDOUT_FILENO && fd != STDERR_FILENO) {
    std::string buffer("More error details may be provided in the logfile '");
    buffer.append(_filename);
    buffer.append("'");

    return buffer;
  }

  return "";
}

void LogAppenderFile::writeLogFile(int fd, char const* buffer, ssize_t len) {
  bool giveUp = false;

  while (len > 0) {
    ssize_t n;

    n = TRI_WRITE(fd, buffer, len);

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
}
