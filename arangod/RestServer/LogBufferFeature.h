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

#include "RestServer/arangod.h"
#include "Logger/LogAppender.h"
#include "Logger/LogLevel.h"

#include <cstdint>
#include <memory>
#include <vector>

namespace application_features {
class ApplicationServer;
}
namespace options {
class ProgramOptions;
}

namespace arangodb {

struct LogBuffer {
  uint64_t _id;
  LogLevel _level;
  uint32_t _topicId;
  double _timestamp;
  char _message[512];

  LogBuffer();
};

class LogBufferFeature final : public ArangodFeature {
 public:
  static constexpr std::string_view name() noexcept { return "LogBuffer"; }

  static constexpr uint32_t BufferSize = 2048;

  explicit LogBufferFeature(Server& server);

  void collectOptions(std::shared_ptr<options::ProgramOptions>) override final;
  void prepare() override;

  /// @brief return all buffered log entries
  std::vector<LogBuffer> entries(LogLevel, uint64_t start, bool upToLevel,
                                 std::string const& searchString);

  /// @brief clear all log entries
  void clear();

 private:
  std::shared_ptr<LogAppender> _inMemoryAppender;
  std::shared_ptr<LogAppender> _metricsCounter;
  std::string _minInMemoryLogLevel;
  bool _useInMemoryAppender;
};

}  // namespace arangodb
