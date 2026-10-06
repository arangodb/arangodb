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

#include "Basics/asio_ns.h"
#include "Utils/Thread.h"

#include <atomic>

namespace arangodb {
namespace rest {

class IoContext {
  friend class IoThread;

 private:
  class IoThread final : public Thread {
   public:
    explicit IoThread(IoContext&);
    explicit IoThread(IoThread const&);
    ~IoThread();
    void run() override;

    // allow IoThreads to start during server prepare phase
    bool isSystem() const override { return true; }

   private:
    IoContext& _iocontext;
  };

 public:
  asio_ns::io_context io_context;

 private:
  IoThread _thread;
  asio_ns::executor_work_guard<asio_ns::io_context::executor_type> _work;
  std::atomic<unsigned> _clients;

 public:
  explicit IoContext();
  explicit IoContext(IoContext const&);
  ~IoContext();

  unsigned clients() const noexcept {
    return _clients.load(std::memory_order_acquire);
  }

  void incClients() noexcept {
    _clients.fetch_add(1, std::memory_order_release);
  }

  void decClients() noexcept {
    _clients.fetch_sub(1, std::memory_order_release);
  }

  void start();
  void stop();
  bool runningInThisThread() const { return _thread.runningInThisThread(); }
};

}  // namespace rest
}  // namespace arangodb
