////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2026 ArangoDB GmbH, Cologne, Germany
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

#include <cstdint>
#include <memory>
#include <string>
#include <utility>

#include "Basics/BasicThread.h"

namespace arangodb {
namespace application_features {
class ApplicationServer;
}
class ExecContext;

/// @brief Thread base classes that make the decision which ExecContext a
/// dedicated thread runs under explicit, once, at thread construction.
///
/// The execContext constructor argument is deliberately REQUIRED. Pass
/// - ExecContext::superuserAsShared() for threads that execute
///   authorization-relevant code (AQL queries, transactions,
///   Collections::/Indexes::/Databases:: methods, V8, ...),
/// - nullptr as an explicit statement "infrastructure thread -- must never
///   run authorization-relevant code" (the ExecContext::set() call is then
///   skipped entirely),
/// - a captured context (e.g. ExecContext::currentAsShared()) if the thread
///   deliberately acts on behalf of whoever constructs it.
///
/// beforeRun() installs the context once on the new thread; no scope/guard
/// is needed because the thread-local dies with the thread.

class Thread : public BasicThread {
 public:
  Thread(std::string const& name,
         std::shared_ptr<ExecContext const> execContext,
         bool deleteOnExit = false, std::uint32_t terminationTimeout = INFINITE)
      : BasicThread(name, deleteOnExit, terminationTimeout),
        _execContext(std::move(execContext)) {}

 protected:
  void beforeRun() override;

 private:
  std::shared_ptr<ExecContext const> _execContext;
};

class ServerThread : public Thread {
 public:
  using Server = application_features::ApplicationServer;

  ServerThread(Server& server, std::string const& name,
               std::shared_ptr<ExecContext const> execContext,
               bool deleteOnExit = false,
               std::uint32_t terminationTimeout = INFINITE)
      : Thread(name, std::move(execContext), deleteOnExit, terminationTimeout),
        _server(server) {}

  Server& server() noexcept { return _server; }

 protected:
  Server& _server;
};

}  // namespace arangodb
