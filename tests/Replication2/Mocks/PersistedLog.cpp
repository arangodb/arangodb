//
// Created by lars on 10/08/2021.
//

#include "PersistedLog.h"

#include <yaclib/async/make.hpp>
#include <yaclib/async/contract.hpp>

using namespace arangodb;
using namespace arangodb::replication2;
using namespace arangodb::replication2::replicated_log;
using namespace arangodb::replication2::test;

auto MockLog::insert(PersistedLogIterator& iter, WriteOptions const& opts)
    -> arangodb::Result {
  auto lastIndex = LogIndex{0};

  while (auto entry = iter.next()) {
    auto const res = _storage.try_emplace(entry->logIndex(), entry.value());
    TRI_ASSERT(res.second);

    TRI_ASSERT(entry->logIndex() > lastIndex);
    lastIndex = entry->logIndex();
    if (opts.waitForSync) {
      _writtenWithWaitForSync.insert(entry->logIndex());
    }
  }

  return {};
}

template<typename I>
struct MockLogContainerIterator : PersistedLogIterator {
  MockLogContainerIterator(MockLog::storeType store, LogIndex start)
      : _store(std::move(store)),
        _current(_store.lower_bound(start)),
        _end(_store.end()) {}

  auto next() -> std::optional<PersistingLogEntry> override {
    if (_current != _end) {
      auto it = _current;
      ++_current;
      return it->second;
    }
    return std::nullopt;
  }

  MockLog::storeType _store;
  I _current;
  I _end;
};

auto MockLog::read(replication2::LogIndex start)
    -> std::unique_ptr<PersistedLogIterator> {
  return std::make_unique<MockLogContainerIterator<iteratorType>>(_storage,
                                                                  start);
}

auto MockLog::removeFront(replication2::LogIndex stop)
    -> yaclib::Future<Result> {
  _storage.erase(_storage.begin(), _storage.lower_bound(stop));
  return yaclib::MakeFuture<Result>();
}

auto MockLog::removeBack(replication2::LogIndex start) -> Result {
  _storage.erase(_storage.lower_bound(start), _storage.end());
  return {};
}

auto MockLog::drop() -> Result {
  _storage.clear();
  return {};
}

void MockLog::setEntry(replication2::LogIndex idx, replication2::LogTerm term,
                       replication2::LogPayload payload) {
  _storage.emplace(std::piecewise_construct, std::forward_as_tuple(idx),
                   std::forward_as_tuple(term, idx, std::move(payload)));
}

MockLog::MockLog(replication2::LogId id) : MockLog(id, {}) {}

MockLog::MockLog(replication2::GlobalLogIdentifier gid)
    : PersistedLog(std::move(gid)) {}

MockLog::MockLog(replication2::LogId id, MockLog::storeType storage)
    : PersistedLog(GlobalLogIdentifier("", id)), _storage(std::move(storage)) {}

AsyncMockLog::AsyncMockLog(replication2::LogId id)
    : MockLog(id), _asyncWorker([this] { this->runWorker(); }) {}

AsyncMockLog::~AsyncMockLog() noexcept { stop(); }

void MockLog::setEntry(replication2::PersistingLogEntry entry) {
  _storage.emplace(entry.logIndex(), std::move(entry));
}

auto MockLog::insertAsync(std::unique_ptr<PersistedLogIterator> iter,
                          WriteOptions const& opts) -> yaclib::Future<Result> {
  return yaclib::MakeFuture(insert(*iter, opts));
}

auto AsyncMockLog::insertAsync(std::unique_ptr<PersistedLogIterator> iter,
                               WriteOptions const& opts)
    -> yaclib::Future<Result> {
  auto entry = std::make_shared<QueueEntry>();
  auto [f, p] = yaclib::MakeContract<Result>();
  entry->promise = std::move(p);
  entry->opts = opts;
  entry->iter = std::move(iter);

  {
    std::unique_lock guard(_mutex);
    TRI_ASSERT(!_stopped);
    TRI_ASSERT(!_stopping);
    _queue.emplace_back(entry);
    _cv.notify_all();
  }

  return std::move(f);
}

void AsyncMockLog::runWorker() {
  bool cont = true;
  while (cont) {
    std::vector<std::shared_ptr<QueueEntry>> queue;
    {
      std::unique_lock guard(_mutex);
      if (_queue.empty()) {
        if (_stopping.load()) {
          _stopped = true;
          return;
        }
        _cv.wait(guard);
      } else {
        std::swap(queue, _queue);
      }
    }
    for (auto& entry : queue) {
      auto res = insert(*entry->iter, entry->opts);
      TRI_ASSERT(entry->promise.Valid());
      std::move(entry->promise).Set(res);
    }
  }
}

auto DelayedMockLog::insertAsync(
    std::unique_ptr<replication2::PersistedLogIterator> iter,
    PersistedLog::WriteOptions const& opts) -> yaclib::Future<Result> {
  TRI_ASSERT(!_pending.has_value());
  auto [f, p] = yaclib::MakeContract<Result>();
  _pending.emplace(std::move(iter), opts, std::move(p));
  return std::move(f);
}

void DelayedMockLog::runAsyncInsert() {
  TRI_ASSERT(_pending.has_value());
  MockLog::insertAsync(std::move(_pending->iter), _pending->options)
      .DetachInline([this](auto&& res) {
        auto promise = std::move(_pending->promise);
        _pending.reset();
        TRI_ASSERT(promise.Valid());
        std::move(promise).Set(std::move(res));
      });
}

DelayedMockLog::PendingRequest::PendingRequest(
    std::unique_ptr<replication2::PersistedLogIterator> iter,
    WriteOptions options, yaclib::Promise<Result>&& promise)
    : iter(std::move(iter)), options(options), promise(std::move(promise)) {}
