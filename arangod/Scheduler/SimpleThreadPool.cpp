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

#include "Logger/LogMacros.h"

#include "SimpleThreadPool.h"

using namespace arangodb;

ThreadPool::ThreadPool(std::size_t threadCount) : _threads(threadCount) {
  std::generate_n(std::back_inserter(_threads), threadCount,
                  [&]() { return makeThread(); });
}

ThreadPool::~ThreadPool() {
  std::unique_lock guard(_mutex);
  _stop.request_stop();
  _cv.notify_all();
}

void ThreadPool::push(std::unique_ptr<Task>&& task) noexcept {
  std::unique_lock guard(_mutex);
  _tasks.emplace_back(std::move(task));
  _cv.notify_one();
}

auto ThreadPool::pop(std::stop_token stoken) noexcept -> std::unique_ptr<Task> {
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

thread_local ThreadPool* ThreadPool::myThreadPool = nullptr;

std::jthread ThreadPool::makeThread() noexcept {
  return std::jthread([this]() noexcept {
    myThreadPool = this;
    auto stoken = _stop.get_token();
    while (auto item = pop(stoken)) {
      item->execute();

      // exit point for detached threads
      if (myThreadPool == nullptr) {
        break;
      }
    }
  });
}

void ThreadPool::detachSelf() noexcept {
  std::unique_lock guard(_mutex);
  auto self =
      std::find_if(_threads.begin(), _threads.end(), [](auto const& other) {
        return std::this_thread::get_id() == other.get_id();
      });

  if (self != _threads.end()) {
    auto replacement = makeThread();
    std::swap(replacement, *self);
    myThreadPool = nullptr;  // detach this thread from the thread pool
    replacement.detach();
  }
}
