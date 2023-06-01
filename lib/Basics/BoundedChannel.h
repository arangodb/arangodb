
#pragma once
#include <cstdint>
#include <memory>
#include <vector>
#include <mutex>
#include <condition_variable>

namespace arangodb {

// TODO there will be people crying because this is not ideal. Like calling
//  notify, while holding the mutex. I don't care right now and probably not
//  in the future. This is used in IO heavy workloads, where the latency impact
//  can't even be measured.
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
      // wake up all waiting worker
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

  // TODO indicate if popping blocked
  std::unique_ptr<T> pop() noexcept {
    std::unique_lock guard(_mutex);
    while (!_stopped || _consumeIndex < _produceIndex) {
      if (_consumeIndex < _produceIndex) {
        // there is something to eat
        auto ours = std::move(_queue[_consumeIndex++ % _queue.size()]);
        // notify any pending producers
        _readCv.notify_one();
        return ours;
      } else {
        _writeCv.wait(guard);
      }
    }
    return nullptr;
  }

  // TODO indicate if pushing blocked
  void push(std::unique_ptr<T> item) {
    std::unique_lock guard(_mutex);
    while (!_stopped) {
      if (_produceIndex < _queue.size() + _consumeIndex) {
        // there is space to put something in
        _queue[_produceIndex++ % _queue.size()] = std::move(item);
        _writeCv.notify_one();
        return;
      } else {
        _readCv.wait(guard);
      }
    }
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
