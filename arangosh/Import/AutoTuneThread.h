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
/// @author Matthew Von-Maszewski
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_IMPORT_AUTOTUNE_THREAD_H
#define ARANGODB_IMPORT_AUTOTUNE_THREAD_H 1

#include "Basics/ConditionVariable.h"
#include "Basics/Thread.h"
#include "Logger/Logger.h"

namespace arangodb {
namespace application_features {
class ApplicationServer;
}

namespace import {

class ImportHelper;

class AutoTuneThread final : public arangodb::Thread {
 private:
  AutoTuneThread(AutoTuneThread const&) = delete;
  AutoTuneThread& operator=(AutoTuneThread const&) = delete;

 public:
  explicit AutoTuneThread(application_features::ApplicationServer& server,
                          ImportHelper& importHelper);

  ~AutoTuneThread();

  void beginShutdown() override;

  void paceSends();

 protected:
  void run() override;

  ImportHelper& _importHelper;
  basics::ConditionVariable _condition;
  std::chrono::steady_clock::time_point _nextSend;
  std::chrono::microseconds _pace;
};
}  // namespace import
}  // namespace arangodb
#endif
