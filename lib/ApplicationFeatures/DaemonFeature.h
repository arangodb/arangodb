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

#ifndef ARANGODB_APPLICATION_FEATURES_DAEMON_FEATURE_H
#define ARANGODB_APPLICATION_FEATURES_DAEMON_FEATURE_H 1

#include "ApplicationFeatures/ApplicationFeature.h"

#include "Basics/process-utils.h"

namespace arangodb {
class DaemonFeature final : public application_features::ApplicationFeature {
 public:
  explicit DaemonFeature(application_features::ApplicationServer* server);

 public:
  void collectOptions(std::shared_ptr<options::ProgramOptions>) override final;
  void validateOptions(std::shared_ptr<options::ProgramOptions>) override final;
  void daemonize() override final;
  void unprepare() override final;

 public:
  void setDaemon(bool value) { _daemon = value; }

 private:
  void checkPidFile();
  int forkProcess();
  void writePidFile(int);
  int waitForChildProcess(int);

 public:
  bool _daemon = false;
  std::string _pidFile = "";
  std::string _workingDirectory = ".";

 private:
  std::string _current;
};
}

#endif
