////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2019 ArangoDB GmbH, Cologne, Germany
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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_GENERAL_SERVER_IO_CONTEXT_H
#define ARANGOD_GENERAL_SERVER_IO_CONTEXT_H 1

#include "Basics/Common.h"
#include "Basics/Mutex.h"
#include "Basics/Thread.h"
#include "Basics/asio_ns.h"

namespace arangodb {
namespace rest {
  
class IoContext {
  friend class IoThread;

 private:
  
  class IoThread final : public Thread {
  public:
    explicit IoThread(IoContext& iocontext);
    ~IoThread();
    void run() override;
    
  private:
    IoContext& _iocontext;
  };
  
public:
  
  asio_ns::io_context io_context;
  
 private:
  
  IoThread _thread;
  asio_ns::io_context::work _asioWork;
  std::atomic<unsigned> _clients;

 public:
  IoContext();
  ~IoContext();
  
  int clients() const noexcept {
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

#endif
