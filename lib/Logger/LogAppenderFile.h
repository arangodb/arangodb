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

#include <memory>
#include <mutex>
#include <string>
#include <tuple>
#include <vector>

#include "Logger/LogLevel.h"
#include "Logger/LogAppenderStream.h"

namespace arangodb {
struct LogMessage;

class LogAppenderFile : public LogAppenderStream {
  friend struct LogAppenderFileFactory;

 public:
  explicit LogAppenderFile(std::string const& filename, int fd);
  ~LogAppenderFile();

  void writeLogMessage(LogLevel level, size_t topicId,
                       std::string const& message) override final;

  std::string details() const override final;

  std::string const& filename() const { return _filename; }

 private:
  std::string const _filename;
};

struct LogAppenderFileFactory {
  static std::shared_ptr<LogAppenderFile> getFileAppender(
      std::string const& filename);

  static void reopenAll();
  static void closeAll();

#ifdef ARANGODB_USE_GOOGLE_TESTS
  static std::vector<
      std::tuple<int, std::string, std::shared_ptr<LogAppenderFile>>>
  getAppenders();

  static void setAppenders(
      std::vector<std::tuple<int, std::string,
                             std::shared_ptr<LogAppenderFile>>> const& fds);
#endif

  static void setFileMode(int mode) { _fileMode = mode; }
  static void setFileGroup(int group) { _fileGroup = group; }

 private:
  static std::mutex _openAppendersMutex;
  static std::vector<std::shared_ptr<LogAppenderFile>> _openAppenders;

  static int _fileMode;
  static int _fileGroup;

 private:
  // Static class, never construct it.
  LogAppenderFileFactory() = delete;
};

}  // namespace arangodb
