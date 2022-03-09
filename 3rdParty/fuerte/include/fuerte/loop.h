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
/// @author Jan Christoph Uhde
/// @author Ewout Prangsma
////////////////////////////////////////////////////////////////////////////////
#pragma once

#ifndef ARANGO_CXX_DRIVER_SERVER
#define ARANGO_CXX_DRIVER_SERVER

#include <fuerte/asio_ns.h>

#include <mutex>
#include <thread>
#include <utility>

// run / runWithWork / poll for Loop mapping to ioservice
// free function run with threads / with thread group barrier and work

namespace arangodb { namespace fuerte { inline namespace v1 {

// need partial rewrite so it can be better integrated in client applications

typedef asio_ns::executor_work_guard<asio_ns::io_context::executor_type>
    asio_work_guard;

/// @brief EventLoopService implements single-threaded event loops
/// Idea is to shard connections across io context's to avoid
/// unnecessary synchronization overhead. Please note that on
/// linux epoll has max 64 instances, so there is a limit on the
/// number of io_context instances.
class EventLoopService {
 public:
  // Initialize an EventLoopService with a given number of threads
  //  and a given number of io_contexts
  explicit EventLoopService(unsigned int threadCount = 1,
                            char const* name = "");
  virtual ~EventLoopService();

  // Prevent copying
  EventLoopService(EventLoopService const& other) = delete;
  EventLoopService& operator=(EventLoopService const& other) = delete;

  // io_service returns a reference to the boost io_service.
  std::shared_ptr<asio_ns::io_context>& nextIOContext() {
    return _ioContexts[_lastUsed.fetch_add(1, std::memory_order_relaxed) %
                       _ioContexts.size()];
  }

  asio_ns::ssl::context& sslContext();

  // stop and join threads
  void stop();

 private:
  /// number of last used io_context
  std::atomic<uint32_t> _lastUsed;

  /// protect ssl context creation
  std::mutex _sslContextMutex;
  /// global SSL context to use here
  std::unique_ptr<asio_ns::ssl::context> _sslContext;

  /// io contexts
  std::vector<std::shared_ptr<asio_ns::io_context>> _ioContexts;
  /// Threads powering each io_context
  std::vector<std::thread> _threads;
  /// Used to keep the io-context alive.
  std::vector<asio_work_guard> _guards;
};
}}}  // namespace arangodb::fuerte::v1
#endif
