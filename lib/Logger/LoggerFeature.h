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

#ifndef ARANGODB_LOGGER_LOGGER_FEATURE_H
#define ARANGODB_LOGGER_LOGGER_FEATURE_H 1

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "ApplicationFeatures/ApplicationFeature.h"

namespace arangodb {
namespace application_features {
class ApplicationServer;
}
namespace options {
class ProgramOptions;
}

class LoggerFeature final : public application_features::ApplicationFeature {
 public:
  LoggerFeature(application_features::ApplicationServer& server, bool threaded);
  ~LoggerFeature();

  void collectOptions(std::shared_ptr<options::ProgramOptions>) override final;
  void loadOptions(std::shared_ptr<options::ProgramOptions>,
                   char const* binaryPath) override final;
  void validateOptions(std::shared_ptr<options::ProgramOptions>) override final;
  void prepare() override final;
  void unprepare() override final;

  void setBackgrounded(bool backgrounded) { _backgrounded = backgrounded; }
  void disableThreaded() { _threaded = false; }
  void setSupervisor(bool supervisor) { _supervisor = supervisor; }

  bool isAPIEnabled() const { return _apiEnabled; }
  bool onlySuperUser() const { return _apiSwitch == "jwt"; }

 private:
  std::vector<std::string> _output;
  std::vector<std::string> _levels;
  std::string _prefix;
  std::string _hostname;
  std::string _file;
  std::string _fileMode;
  std::string _fileGroup;
  std::string _timeFormatString;
  uint32_t _maxEntryLength = 128U * 1048576U;
  bool _useJson = false;
  bool _useLocalTime = false;
  bool _useColor = true;
  bool _useEscaped = true;
  bool _lineNumber = false;
  bool _shortenFilenames = true;
  bool _processId = true;
  bool _threadId = false;
  bool _threadName = false;
  bool _performance = false;
  bool _keepLogRotate = false;
  bool _foregroundTty = false;
  bool _forceDirect = false;
  bool _useMicrotime = false;
  bool _showIds = true;
  bool _showRole = false;
  bool _logRequestParameters = true;
  bool _supervisor = false;
  bool _backgrounded = false;
  bool _threaded = false;
  std::string _apiSwitch = "true";
  bool _apiEnabled = true;
};

}  // namespace arangodb

#endif
