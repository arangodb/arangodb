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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Aql/ExecutionState.h"
#include "Assertions/Assert.h"
#include "Basics/Common.h"
#include "Basics/Result.h"
#include "Basics/system-functions.h"
#include "Utils/DatabaseGuard.h"
#include "VocBase/voc-types.h"

#include "velocypack/Buffer.h"
#include <velocypack/Iterator.h>
#include <velocypack/Builder.h>
#include <atomic>

namespace arangodb {

namespace transaction {
class Context;
}

namespace velocypack {
class Builder;
class Slice;
}  // namespace velocypack

typedef TRI_voc_tick_t CursorId;

class Cursor {
 public:
  Cursor(Cursor const&) = delete;
  Cursor& operator=(Cursor const&) = delete;

  Cursor(CursorId id, size_t batchSize, double ttl, bool hasCount,
         bool isRetriable)
      : _id(id),
        _batchSize(batchSize == 0 ? 1 : batchSize),
        _currentBatchId(0),
        _ttl(ttl),
        _expires(TRI_microtime() + _ttl),
        _hasCount(hasCount),
        _isDeleted(false),
        _isRetriable(isRetriable),
        _isUsed(false) {}

  virtual ~Cursor() = default;

 public:
  CursorId id() const { return _id; }

  inline size_t batchSize() const { return _batchSize; }

  bool hasCount() const noexcept { return _hasCount; }

  bool isRetriable() const noexcept { return _isRetriable; }

  double ttl() const noexcept { return _ttl; }

  double expires() const noexcept {
    return _expires.load(std::memory_order_relaxed);
  }

  inline bool isUsed() const {
    // (1) - this release-store synchronizes-with the acquire-load (2)
    return _isUsed.load(std::memory_order_acquire);
  }

  inline bool isDeleted() const { return _isDeleted; }

  void setDeleted() { _isDeleted = true; }

  void setLastQueryBatchObject(
      std::shared_ptr<velocypack::Buffer<uint8_t>> buffer) {
    _currentBatchResult.second = std::move(buffer);
  }

  std::pair<std::shared_ptr<VPackBufferUInt8>, Result> getLastBatchResult(
      std::string const& batchId) {
    if (_currentBatchResult.first != batchId) {
      return {nullptr, Result(TRI_ERROR_HTTP_NOT_FOUND, "batch id not found")};
    }
    return {_currentBatchResult.second, TRI_ERROR_NO_ERROR};
  }

  void handleNextBatchIdValue(VPackBuilder& builder, bool hasMore) {
    if (isRetriable()) {
      _currentBatchResult.first = std::to_string(++_currentBatchId);
      if (hasMore) {
        builder.add("nextBatchId", std::to_string(_currentBatchId + 1));
      }
    }
  }

  void use() {
    TRI_ASSERT(!_isDeleted);
    TRI_ASSERT(!_isUsed);

    _isUsed.store(true, std::memory_order_relaxed);
  }

  void release() noexcept {
    TRI_ASSERT(_isUsed);
    _expires.store(TRI_microtime() + _ttl, std::memory_order_relaxed);
    // (2) - this release-store synchronizes-with the acquire-load (1)
    _isUsed.store(false, std::memory_order_release);
  }

  virtual void kill() {}

  // Debug method to kill a query at a specific position
  // during execution. It internally asserts that the query
  // is actually visible through other APIS (e.g. current queries)
  // so user actually has a chance to kill it here.
  virtual void debugKillQuery() {}

  virtual size_t count() const = 0;

  virtual std::shared_ptr<transaction::Context> context() const = 0;

  /**
   * @brief Dump the cursor result, async version. The caller needs to be
   * contiueable
   *
   * @param result The Builder to write the result to
   * @param continueHandler The function that is posted on scheduler to contiue
   * this execution.
   *
   * @return First: ExecutionState either DONE or WAITING. On Waiting we need to
   * free this thread on DONE we have a result. Second: Result If State==DONE
   * this contains Error information or NO_ERROR. On NO_ERROR result is filled.
   */
  virtual std::pair<aql::ExecutionState, Result> dump(
      velocypack::Builder& result) = 0;

  /**
   * @brief Dump the cursor result. This is guaranteed to return the result in
   * this thread.
   *
   * @param result the Builder to write the result to
   *
   * @return ErrorResult, if something goes wrong
   */
  virtual Result dumpSync(velocypack::Builder& result) = 0;

  /// Set wakeup handler on streaming cursor
  virtual void setWakeupHandler(std::function<bool()> const& cb) {}
  virtual void resetWakeupHandler() {}

  virtual bool allowDirtyReads() const { return false; }

 protected:
  CursorId const _id;
  size_t const _batchSize;
  size_t _currentBatchId;
  double _ttl;
  std::atomic<double> _expires;
  bool const _hasCount;
  bool _isDeleted;
  bool _isRetriable;
  std::atomic<bool> _isUsed;
  std::pair<std::string, std::shared_ptr<velocypack::Buffer<uint8_t>>>
      _currentBatchResult;
};
}  // namespace arangodb
