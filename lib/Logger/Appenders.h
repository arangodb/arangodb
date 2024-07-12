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
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "Basics/ReadWriteLock.h"
#include "Basics/Result.h"
#include "Logger/LogGroup.h"

namespace arangodb {
class LogAppender;
struct LogMessage;
class LogTopic;
}  // namespace arangodb

namespace arangodb::logger {

struct Appenders {
  void addAppender(LogGroup const&, std::string const& definition);

  void addGlobalAppender(LogGroup const&, std::shared_ptr<LogAppender>);

  std::shared_ptr<LogAppender> buildAppender(LogGroup const&,
                                             std::string const& output);

  void logGlobal(LogGroup const&, LogMessage const&);
  void log(LogGroup const&, LogMessage const&);

  void reopen();
  void shutdown();

  bool haveAppenders(LogGroup const&, size_t topicId);

 private:
  Result parseDefinition(std::string const& definition, std::string& topicName,
                         std::string& output, LogTopic*& topic);

  arangodb::basics::ReadWriteLock _appendersLock;
  std::array<std::vector<std::shared_ptr<LogAppender>>, LogGroup::Count>
      _globalAppenders;
  std::array<std::map<size_t, std::vector<std::shared_ptr<LogAppender>>>,
             LogGroup::Count>
      _topics2appenders;
  std::array<std::map<std::string, std::shared_ptr<LogAppender>>,
             LogGroup::Count>
      _definition2appenders;
};
}  // namespace arangodb::logger
