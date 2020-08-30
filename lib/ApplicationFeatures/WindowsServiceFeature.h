////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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
/// @author Wilfried Goesgens
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_APPLICATION_FEATURES_WINDOWS_SERVICE_FEATURE_H
#define ARANGODB_APPLICATION_FEATURES_WINDOWS_SERVICE_FEATURE_H 1

#include "ApplicationFeatures/ApplicationFeature.h"
#include "ApplicationFeatures/ApplicationServer.h"

#include <atomic>

extern SERVICE_STATUS_HANDLE ServiceStatus;

void SetServiceStatus(DWORD dwCurrentState, DWORD dwWin32ExitCode,
                      DWORD dwCheckPoint, DWORD dwWaitHint, DWORD exitCode);

void WINAPI ServiceCtrl(DWORD dwCtrlCode);

namespace arangodb {

class WindowsServiceFeature final : public application_features::ApplicationFeature {
 public:
  explicit WindowsServiceFeature(application_features::ApplicationServer& server);

  void collectOptions(std::shared_ptr<options::ProgramOptions>) override final;
  void validateOptions(std::shared_ptr<options::ProgramOptions>) override final;

 private:
  void installService();
  void StartArangoService(bool WaitForRunning);
  void StopArangoService(bool WaitForShutdown);
  void startupProgress();

  void startupFinished();

  void shutdownBegins();
  void shutdownComplete();
  void shutdownFailure();
  void abortFailure(uint16_t exitCode);
  static void abortService(uint16_t exitCode);

 public:
  bool _installService = false;
  bool _unInstallService = false;
  bool _forceUninstall = false;
  bool _startAsService = false;
  bool _startService = false;
  bool _startWaitService = false;
  bool _stopService = false;
  bool _stopWaitService = false;

  application_features::ApplicationServer* _server;

 private:
  uint16_t _progress;

  /// @brief flag that tells us whether we have been informed about the shutdown before
  std::atomic<bool> _shutdownNoted;
};

}  // namespace arangodb

#endif
