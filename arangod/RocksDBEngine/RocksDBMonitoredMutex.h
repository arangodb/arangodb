#pragma once

#include <rocksdb/status.h>
#include <rocksdb/utilities/transaction_db_mutex.h>

#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <optional>

#include "infra/ResourceMonitorScope.h"
#include "infra/RocksDBMonitors.h"

namespace arangodb {

class RocksDBMonitoredMutex final : public rocksdb::TransactionDBMutex {
 public:
  rocksdb::Status Lock() override {
    _mutex.lock();
    enterScope();
    return rocksdb::Status::OK();
  }

  rocksdb::Status TryLockFor(int64_t timeout) override {
    if (timeout == 0) {
      if (!_mutex.try_lock()) {
        return rocksdb::Status::TimedOut(
            rocksdb::Status::SubCode::kMutexTimeout);
      }
    } else {
      _mutex.lock();
    }
    enterScope();
    return rocksdb::Status::OK();
  }

  void UnLock() override {
    _scope.reset();
    _mutex.unlock();
  }

 private:
  friend class RocksDBMonitoredCondVar;

  void enterScope() {
    if (infra::detail::currentResourceMonitor() != &infra::rocksdbMonitor()) {
      _scope.emplace(infra::ResourceMonitorRef(infra::rocksdbMonitor()));
    }
  }
  std::mutex _mutex;
  std::optional<infra::ResourceMonitorScope> _scope;
};

class RocksDBMonitoredCondVar final : public rocksdb::TransactionDBCondVar {
 public:
  rocksdb::Status Wait(
      std::shared_ptr<rocksdb::TransactionDBMutex> mutex) override {
    return WaitFor(std::move(mutex), -1);
  }

  rocksdb::Status WaitFor(std::shared_ptr<rocksdb::TransactionDBMutex> mutex,
                          int64_t timeout) override {
    auto* m = static_cast<RocksDBMonitoredMutex*>(mutex.get());
    // mutex is released while waiting, another thread may take it and use
    // _scope, so we need to close ours first.
    m->_scope.reset();

    rocksdb::Status s;
    std::unique_lock<std::mutex> lock(m->_mutex, std::adopt_lock);
    if (timeout < 0) {
      _cv.wait(lock);
    } else if (_cv.wait_for(lock, std::chrono::microseconds(timeout)) ==
               std::cv_status::timeout) {
      s = rocksdb::Status::TimedOut(rocksdb::Status::SubCode::kMutexTimeout);
    }
    lock.release();
    m->enterScope();
    return s;
  }

  void Notify() override { _cv.notify_one(); }
  void NotifyAll() override { _cv.notify_all(); }

 private:
  std::condition_variable _cv;
};

class RocksDBMonitoredMutexFactory final
    : public rocksdb::TransactionDBMutexFactory {
 public:
  std::shared_ptr<rocksdb::TransactionDBMutex> AllocateMutex() override {
    return std::make_shared<RocksDBMonitoredMutex>();
  }

  std::shared_ptr<rocksdb::TransactionDBCondVar> AllocateCondVar() override {
    return std::make_shared<RocksDBMonitoredCondVar>();
  }
};

}  // namespace arangodb