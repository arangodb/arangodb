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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Basics/Thread.h"
#include "V8Server/V8Executor.h"
#include "RestServer/arangod.h"

#include <mutex>

struct TRI_vocbase_t;

namespace arangodb {
class V8ExecutorGuard;
class V8LineEditor;
}  // namespace arangodb

namespace arangodb {
namespace application_features {
class ApplicationServer;
}

class ConsoleThread final : public ServerThread<ArangodServer> {
  ConsoleThread(const ConsoleThread&) = delete;
  ConsoleThread& operator=(const ConsoleThread&) = delete;

 public:
  ConsoleThread(Server&, TRI_vocbase_t*);
  ~ConsoleThread();

  void run() override;
  bool isSilent() const override { return true; }

  void userAbort() { _userAborted.store(true); }

  static arangodb::V8LineEditor* serverConsole;
  static std::mutex serverConsoleMutex;

 private:
  void inner(V8ExecutorGuard&);

  TRI_vocbase_t* _vocbase;
  std::atomic<bool> _userAborted;
};
}  // namespace arangodb
