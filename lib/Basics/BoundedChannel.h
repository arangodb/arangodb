////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
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
/// @author Lars Maier
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <vector>

namespace arangodb {

// This implementation is fine for io heavy workloads but might not be
// sufficient for computational expensive operations with a lot of concurrent
// push/pop operations.
template<typename T>
struct BoundedChannel {
  explicit BoundedChannel(std::size_t queueSize) : _queue(queueSize) {}

  void producerBegin() noexcept {
    std::unique_lock guard(_mutex);
    _numProducer += 1;
  }

  void producerEnd() noexcept {
    std::unique_lock guard(_mutex);
    _numProducer -= 1;
    if (_numProducer == 0) {
      // wake up all waiting workers
      _stopped = true;
      _writeCv.notify_all();
    }
  }

  void stop() {
    std::unique_lock guard(_mutex);
    _stopped = true;
    _writeCv.notify_all();
    _readCv.notify_all();
  }

  // Pops an item from the queue. If the channel is stopped, returns nullptr.
  // Second value is true, if the pop call blocked.
  std::pair<std::unique_ptr<T>, bool> pop() noexcept {
    std::unique_lock guard(_mutex);
    bool blocked = false;
    while (!_stopped || _consumeIndex < _produceIndex) {
      if (_consumeIndex < _produceIndex) {
        // there is something to eat
        auto ours = std::move(_queue[_consumeIndex++ % _queue.size()]);
        // notify any pending producers
        _readCv.notify_one();
        return std::make_pair(std::move(ours), blocked);
      } else {
        blocked = true;
        _writeCv.wait(guard);
      }
    }
    return std::make_pair(nullptr, blocked);
  }

  // First value is false if the value was pushed. Otherwise true, which means
  // the channel is stopped. Workers should terminate. The second value is true
  // if pushing blocked.
  [[nodiscard]] std::pair<bool, bool> push(std::unique_ptr<T> item) {
    std::unique_lock guard(_mutex);
    bool blocked = false;
    while (!_stopped) {
      if (_produceIndex < _queue.size() + _consumeIndex) {
        // there is space to put something in
        _queue[_produceIndex++ % _queue.size()] = std::move(item);
        _writeCv.notify_one();
        return std::make_pair(false, blocked);
      } else {
        blocked = true;
        _readCv.wait(guard);
      }
    }
    return std::make_pair(true, blocked);
  }

  std::mutex _mutex;
  std::condition_variable _writeCv;
  std::condition_variable _readCv;
  std::vector<std::unique_ptr<T>> _queue;
  bool _stopped = false;
  std::size_t _numProducer = 0, _consumeIndex = 0, _produceIndex = 0;
};

template<typename T>
struct BoundedChannelProducerGuard {
  explicit BoundedChannelProducerGuard(BoundedChannel<T>& ch) : channel(&ch) {
    channel->producerBegin();
  }

  BoundedChannelProducerGuard() = default;
  BoundedChannelProducerGuard(BoundedChannelProducerGuard&&) noexcept = default;
  BoundedChannelProducerGuard(BoundedChannelProducerGuard const&) = delete;
  BoundedChannelProducerGuard& operator=(BoundedChannelProducerGuard&&) =
      default;
  BoundedChannelProducerGuard& operator=(BoundedChannelProducerGuard const&) =
      delete;

  ~BoundedChannelProducerGuard() { fire(); }

  void fire() {
    if (channel) {
      channel->producerEnd();
      channel.reset();
    }
  }

 private:
  struct noop {
    template<typename V>
    void operator()(V*) {}
  };
  std::unique_ptr<BoundedChannel<T>, noop> channel = nullptr;
};

}  // namespace arangodb
