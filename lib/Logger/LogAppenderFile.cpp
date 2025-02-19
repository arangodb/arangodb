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

#include "LogAppenderFile.h"

#include <fcntl.h>
#include <cstring>

#include "Basics/Exceptions.h"
#include "Basics/FileUtils.h"
#include "Basics/files.h"
#include "Basics/operating-system.h"
#include "Logger/Logger.h"

namespace arangodb {

std::mutex LogAppenderFileFactory::_openAppendersMutex;
std::vector<std::shared_ptr<LogAppenderFile>>
    LogAppenderFileFactory::_openAppenders;

int LogAppenderFileFactory::_fileMode = S_IRUSR | S_IWUSR | S_IRGRP;
int LogAppenderFileFactory::_fileGroup = 0;

LogAppenderFile::LogAppenderFile(std::string const& filename, int fd)
    : LogAppenderStream(filename, fd), _filename(filename) {
  _useColors = ((isatty(_fd) == 1) && Logger::getUseColor());
}

LogAppenderFile::~LogAppenderFile() = default;

void LogAppenderFile::writeLogMessage(LogLevel level, size_t /*topicId*/,
                                      std::string const& message) {
  bool giveUp = false;
  char const* buffer = message.c_str();
  size_t len = message.size();

  while (len > 0) {
    auto n = TRI_WRITE(_fd, buffer, static_cast<TRI_write_t>(len));

    if (n < 0) {
      if (Logger::allowStdLogging()) {
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

    TRI_ASSERT(len >= static_cast<size_t>(n));
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

std::shared_ptr<LogAppenderFile> LogAppenderFileFactory::getFileAppender(
    std::string const& filename) {
  TRI_ASSERT(filename != "+");
  TRI_ASSERT(filename != "-");
  // logging to an actual file
  std::unique_lock<std::mutex> guard(_openAppendersMutex);
  for (auto const& it : _openAppenders) {
    if (it->filename() == filename) {
      // already have an appender for the same file
      return it;
    }
  }
  // Does not exist yet, create a new one.
  // NOTE: We hold the lock here, to avoid someone creating
  // an appender on this file.
  int fd = TRI_CREATE(filename.c_str(),
                      O_APPEND | O_CREAT | O_WRONLY | TRI_O_CLOEXEC, _fileMode);
  if (fd < 0) {
    std::cerr << "cannot write to file '" << filename
              << "': " << TRI_LAST_ERROR_STR << std::endl;

    THROW_ARANGO_EXCEPTION(TRI_ERROR_CANNOT_WRITE_FILE);
  }

#ifdef ARANGODB_HAVE_SETGID
  if (_fileGroup != 0) {
    int result = fchown(fd, -1, _fileGroup);
    if (result != 0) {
      // we cannot log this error here, as we are the logging itself
      // so just to please compilers, we pretend we are using the result
      (void)result;
    }
  }
#endif

  try {
    auto appender = std::make_shared<LogAppenderFile>(filename, fd);
    _openAppenders.emplace_back(appender);
    return appender;
  } catch (...) {
    TRI_CLOSE(fd);
    throw;
  }
}

void LogAppenderFileFactory::reopenAll() {
  // We must not log anything in this function or in anything it calls! This
  // is because this is called under the `_appendersLock`.
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

    std::ignore = basics::FileUtils::remove(backup);
    TRI_RenameFile(filename.c_str(), backup.c_str());

    // open new log file
    int fd =
        TRI_CREATE(filename.c_str(),
                   O_APPEND | O_CREAT | O_WRONLY | TRI_O_CLOEXEC, _fileMode);

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
        (void)result;
      }
    }
#endif

    if (!Logger::_keepLogRotate) {
      std::ignore = basics::FileUtils::remove(backup);
    }

    // and also tell the appender of the file descriptor change
    it->updateFd(fd);

    if (old > STDERR_FILENO) {
      TRI_CLOSE(old);
    }
  }
}

void LogAppenderFileFactory::closeAll() {
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
std::vector<std::tuple<int, std::string, std::shared_ptr<LogAppenderFile>>>
LogAppenderFileFactory::getAppenders() {
  std::vector<std::tuple<int, std::string, std::shared_ptr<LogAppenderFile>>>
      result;

  std::unique_lock<std::mutex> guard(_openAppendersMutex);
  for (auto const& it : _openAppenders) {
    result.emplace_back(it->fd(), it->filename(), it);
  }

  return result;
}

void LogAppenderFileFactory::setAppenders(
    std::vector<
        std::tuple<int, std::string, std::shared_ptr<LogAppenderFile>>> const&
        appenders) {
  std::unique_lock<std::mutex> guard(_openAppendersMutex);

  _openAppenders.clear();
  for (auto const& it : appenders) {
    _openAppenders.emplace_back(std::get<2>(it));
  }
}
#endif

}  // namespace arangodb
