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

#include "Utils/Cursor.h"

#include "Assertions/Assert.h"
#include "Basics/system-functions.h"

#include <velocypack/Builder.h>

namespace arangodb {

Cursor::Cursor(CursorId id, size_t batchSize, double ttl, bool hasCount,
               bool isRetriable)
    : _id(id),
      _batchSize(batchSize == 0 ? 1 : batchSize),
      _currentBatchId(0),
      _lastAvailableBatchId(1),
      _ttl(ttl),
      _expires(TRI_microtime() + _ttl),
      _hasCount(hasCount),
      _isRetriable(isRetriable),
      _isDeleted(false),
      _isUsed(false) {}

Cursor::~Cursor() = default;

double Cursor::expires() const noexcept {
  return _expires.load(std::memory_order_relaxed);
}

bool Cursor::isUsed() const noexcept {
  // (1) - this release-store synchronizes-with the acquire-load (2)
  return _isUsed.load(std::memory_order_acquire);
}

bool Cursor::isDeleted() const noexcept { return _isDeleted; }

void Cursor::setDeleted() noexcept { _isDeleted = true; }

bool Cursor::isCurrentBatchId(uint64_t id) const noexcept {
  return id == _currentBatchResult.first;
}

bool Cursor::isNextBatchId(uint64_t id) const {
  return id == _currentBatchResult.first + 1 && id == _lastAvailableBatchId;
}

void Cursor::setLastQueryBatchObject(
    std::shared_ptr<velocypack::Buffer<uint8_t>> buffer) noexcept {
  _currentBatchResult.second = std::move(buffer);
}

std::shared_ptr<velocypack::Buffer<uint8_t>> Cursor::getLastBatch() const {
  return _currentBatchResult.second;
}

uint64_t Cursor::storedBatchId() const { return _currentBatchResult.first; }

void Cursor::handleNextBatchIdValue(velocypack::Builder& builder,
                                    bool hasMore) {
  _currentBatchResult.first = ++_currentBatchId;
  if (hasMore) {
    builder.add("nextBatchId", std::to_string(_currentBatchId + 1));
    _lastAvailableBatchId = _currentBatchId + 1;
  }
}

void Cursor::use() noexcept {
  TRI_ASSERT(!_isDeleted);
  TRI_ASSERT(!_isUsed);

  _isUsed.store(true, std::memory_order_relaxed);
}

void Cursor::release() noexcept {
  TRI_ASSERT(_isUsed);
  _expires.store(TRI_microtime() + _ttl, std::memory_order_relaxed);
  // (2) - this release-store synchronizes-with the acquire-load (1)
  _isUsed.store(false, std::memory_order_release);
}

}  // namespace arangodb
