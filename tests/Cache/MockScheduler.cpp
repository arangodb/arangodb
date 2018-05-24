////////////////////////////////////////////////////////////////////////////////
/// @brief helper for cache suite
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2017 ArangoDB GmbH, Cologne, Germany
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
/// @author Daniel H. Larkin
/// @author Copyright 2017, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "MockScheduler.h"
#include "Basics/Common.h"

#include <asio/io_service.hpp>
#include <boost/bind.hpp>

#undef sleep
#include <boost/thread/thread.hpp>

#include <memory>

using namespace arangodb::cache;

MockScheduler::MockScheduler(size_t threads)
    : _ioService(new asio::io_context()),
      _serviceGuard(new asio::io_context::work(*_ioService)) {
  for (size_t i = 0; i < threads; i++) {
    auto worker = std::bind(static_cast<size_t (asio::io_context::*)()>(
                                &asio::io_context::run),
                            _ioService.get());
    _group.emplace_back(new std::thread(worker));
  }
}

MockScheduler::~MockScheduler() {
  _serviceGuard.reset();
  for (auto g : _group) {
    g->join();
    delete g;
  }
  _ioService->stop();
}

void MockScheduler::post(std::function<void()> fn) { _ioService->post(fn); }
