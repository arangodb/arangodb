////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2016 ArangoDB GmbH, Cologne, Germany
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

#ifndef ARANGODB_LOGGER_LOG_BUFFER_FEATURE_H
#define ARANGODB_LOGGER_LOG_BUFFER_FEATURE_H 1

#include "ApplicationFeatures/ApplicationFeature.h"
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
  time_t _timestamp;
  char _message[256];

  LogBuffer(); 
};

class LogBufferFeature final : public application_features::ApplicationFeature {
 public:
  static constexpr uint32_t BufferSize = 2048;
  
  explicit LogBufferFeature(application_features::ApplicationServer& server);
  ~LogBufferFeature() = default;

  void prepare() override;

  /// @brief return all buffered log entries
  std::vector<LogBuffer> entries(LogLevel, uint64_t start, bool upToLevel);

 private:
  std::shared_ptr<LogAppender> _inMemoryAppender;
};

}  // namespace arangodb

#endif
