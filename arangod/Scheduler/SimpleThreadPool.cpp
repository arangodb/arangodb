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
////////////////////////////////////////////////////////////////////////////////
#include <algorithm>

#ifdef TRI_HAVE_SYS_PRCTL_H
#include <sys/prctl.h>
#endif

#include "Logger/LogMacros.h"

#include "SimpleThreadPool.h"

using namespace arangodb;

ThreadPool::ThreadPool(const char* name, std::size_t threadCount)
    : _threads(threadCount) {
  std::generate_n(std::back_inserter(_threads), threadCount, [&]() {
    auto thread = std::jthread([this]() noexcept {
      auto stoken = _stop.get_token();
      while (auto item = pop(stoken)) {
        item->invoke();
      }
    });

#ifdef TRI_HAVE_SYS_PRCTL_H
    pthread_setname_np(thread.native_handle(), name);
#endif

    return thread;
  });
}

ThreadPool::~ThreadPool() {
  std::unique_lock guard(_mutex);
  _stop.request_stop();
  _cv.notify_all();
}

void ThreadPool::push(std::unique_ptr<WorkItem>&& task) noexcept {
  std::unique_lock guard(_mutex);
  _tasks.emplace_back(std::move(task));
  _cv.notify_one();
}

auto ThreadPool::pop(std::stop_token stoken) noexcept
    -> std::unique_ptr<WorkItem> {
  std::unique_lock guard(_mutex);
  bool more = _cv.wait(guard, stoken, [&] { return !_tasks.empty(); });
  if (more) {
    auto task = std::move(_tasks.front());
    _tasks.pop_front();
    return task;
  }
  // stop was requested
  return nullptr;
}
