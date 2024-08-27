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
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <array>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "Basics/ReadWriteLock.h"
#include "Basics/ResultT.h"
#include "Logger/LogGroup.h"
#include "Logger/LogLevel.h"

namespace arangodb {
class LogAppender;
struct LogMessage;
class LogTopic;
}  // namespace arangodb

namespace arangodb::logger {

struct Appenders {
  void addAppender(LogGroup const&, std::string const& definition);

  void addGlobalAppender(LogGroup const&, std::shared_ptr<LogAppender>);

  void logGlobal(LogGroup const&, LogMessage const&);
  void log(LogGroup const&, LogMessage const&);

  void reopen();
  void shutdown();

  bool haveAppenders(LogGroup const& group, size_t topicId);

  auto getAppender(LogGroup const& group, std::string const& definition)
      -> std::shared_ptr<LogAppender>;
  auto getAppenders(LogGroup const& group)
      -> std::unordered_map<std::string, std::shared_ptr<LogAppender>>;

  void foreach (std::function<void(LogAppender&)> const& f);

 private:
  enum class Type {
    kUnknown,
    kFile,
    kStderr,
    kStdout,
    kSyslog,
  };

  struct AppenderConfig {
    std::string output;
    LogTopic* topic = nullptr;
    Type type = Type::kUnknown;
    std::unordered_map<LogTopic*, LogLevel> levels;
  };
  auto parseDefinition(std::string definition) -> ResultT<AppenderConfig>;
  auto parseLogLevels(std::string const& definition)
      -> ResultT<std::unordered_map<LogTopic*, LogLevel>>;

  std::shared_ptr<LogAppender> buildAppender(LogGroup const&,
                                             AppenderConfig const& config);

  struct Group {
    std::vector<std::shared_ptr<LogAppender>> globalAppenders;
    std::unordered_map<size_t, std::vector<std::shared_ptr<LogAppender>>>
        topics2appenders;
    std::unordered_map<std::string, std::shared_ptr<LogAppender>>
        definition2appenders;
  };

  arangodb::basics::ReadWriteLock _appendersLock;
  std::array<Group, LogGroup::Count> _groups;
};
}  // namespace arangodb::logger
