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
////////////////////////////////////////////////////////////////////////////////

#include <fuerte/loop.h>
#include <fuerte/types.h>

#include "debugging.h"

#include <memory>

#ifdef __linux__
#include <sys/prctl.h>
#endif

namespace arangodb { namespace fuerte { inline namespace v1 {

EventLoopService::EventLoopService(unsigned int threadCount, char const* name)
    : _lastUsed(0) {
  _ioContexts.reserve(threadCount);
  _guards.reserve(threadCount);
  _threads.reserve(threadCount);
  for (unsigned i = 0; i < threadCount; i++) {
    _ioContexts.emplace_back(std::make_shared<asio_ns::io_context>(1));
    _guards.emplace_back(asio_ns::make_work_guard(*_ioContexts.back()));
    asio_ns::io_context* ctx = _ioContexts.back().get();
    _threads.emplace_back([=]() {
#ifdef __linux__
      // set name of threadpool thread, so threads can be distinguished from
      // each other
      if (name != nullptr && *name != '\0') {
        prctl(PR_SET_NAME, name, 0, 0, 0);
      }
#endif
      ctx->run();
    });
  }
}

EventLoopService::~EventLoopService() { stop(); }
  
// io_service returns a reference to the boost io_service.
std::shared_ptr<asio_ns::io_context>& EventLoopService::nextIOContext() {
  FUERTE_ASSERT(!_ioContexts.empty());
  return _ioContexts[_lastUsed.fetch_add(1, std::memory_order_relaxed) %
                     _ioContexts.size()];
}

asio_ns::ssl::context& EventLoopService::sslContext() {
  std::lock_guard<std::mutex> guard(_sslContextMutex);
  if (!_sslContext) {
#if (OPENSSL_VERSION_NUMBER >= 0x10100000L)
    _sslContext = std::make_unique<asio_ns::ssl::context>(asio_ns::ssl::context::tls);
#else
    _sslContext = std::make_unique<asio_ns::ssl::context>(asio_ns::ssl::context::sslv23);
#endif
    _sslContext->set_default_verify_paths();
  }
  return *_sslContext;
}

void EventLoopService::stop() {
  // allow run() to exit, wait for threads to finish only then stop the context
  std::for_each(_guards.begin(), _guards.end(), [](auto& g) { g.reset(); });
  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  std::for_each(_ioContexts.begin(), _ioContexts.end(),
                [](auto& c) { c->stop(); });
  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  std::for_each(_threads.begin(), _threads.end(), [](auto& t) {
    if (t.joinable()) {
      t.join();
    }
  });
  _threads.clear();
  _ioContexts.clear();
  _guards.clear();
}

}}}  // namespace arangodb::fuerte::v1
