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
///
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Aql/ExecutionState.h"
#include "Basics/Result.h"
#include "VocBase/voc-types.h"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

namespace arangodb {

namespace transaction {
class Context;
}

namespace velocypack {
template<typename T>
class Buffer;
class Builder;
class Slice;
}  // namespace velocypack

using CursorId = TRI_voc_tick_t;

class Cursor {
 public:
  Cursor(Cursor const&) = delete;
  Cursor& operator=(Cursor const&) = delete;

  Cursor(CursorId id, size_t batchSize, double ttl, bool hasCount,
         bool isRetriable);

  virtual ~Cursor();

 public:
  CursorId id() const noexcept { return _id; }

  size_t batchSize() const noexcept { return _batchSize; }

  bool hasCount() const noexcept { return _hasCount; }

  bool isRetriable() const noexcept { return _isRetriable; }

  double ttl() const noexcept { return _ttl; }

  double expires() const noexcept;

  bool isUsed() const noexcept;

  bool isDeleted() const noexcept;

  void setDeleted() noexcept;

  bool isCurrentBatchId(uint64_t id) const noexcept;

  bool isNextBatchId(uint64_t id) const;

  void setLastQueryBatchObject(
      std::shared_ptr<velocypack::Buffer<uint8_t>> buffer) noexcept;

  std::shared_ptr<velocypack::Buffer<uint8_t>> getLastBatch() const;

  uint64_t storedBatchId() const;

  void handleNextBatchIdValue(velocypack::Builder& builder, bool hasMore);

  void use() noexcept;

  void release() noexcept;

  virtual void kill() {}

  // Debug method to kill a query at a specific position
  // during execution. It internally asserts that the query
  // is actually visible through other APIS (e.g. current queries)
  // so user actually has a chance to kill it here.
  virtual void debugKillQuery() {}

  virtual uint64_t memoryUsage() const noexcept = 0;

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

  virtual bool allowDirtyReads() const noexcept { return false; }

 protected:
  CursorId const _id;
  size_t const _batchSize;
  size_t _currentBatchId;
  size_t _lastAvailableBatchId;
  double const _ttl;
  std::atomic<double> _expires;
  bool const _hasCount;
  bool const _isRetriable;
  bool _isDeleted;
  std::atomic<bool> _isUsed;
  std::pair<uint64_t, std::shared_ptr<velocypack::Buffer<uint8_t>>>
      _currentBatchResult;
};
}  // namespace arangodb
