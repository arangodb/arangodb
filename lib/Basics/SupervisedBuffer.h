////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2025 ArangoDB GmbH, Cologne, Germany
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
/// @author Julia Puget
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <memory>
#include <velocypack/Buffer.h>
#include "ResourceUsage.h"

namespace arangodb::velocypack {

class SupervisedBuffer : public Buffer<uint8_t> {
 public:
  SupervisedBuffer() = delete;

  explicit SupervisedBuffer(arangodb::ResourceMonitor& monitor)
      : _usageScope{monitor, 0} {
    // account the initial inline capacity one time (192 bytes)
    _usageScope.increase(this->capacity());
  }

  uint8_t* stealWithMemoryAccounting(ResourceUsageScope& owningScope) {
    std::size_t tracked = 0;
    // the buffer's usage scope is gonna account for 0 bytes now and give the
    // value it accounted for to the owning scope, so the amount of memory that
    // the buffer allocated which was accounted by its scope is now gonna be
    // accounted by the owning scope.
    // We do not have an atomic operation here, so we will first decrease on
    // current scope and hand it over to the new scope. We did not do it the
    // other way round, as this would double the reported memory for a short
    // time, which could cause an perfectly fine operation to OOM. The memory is
    // never used twice.
    try {
      tracked = _usageScope.tracked();
      _usageScope.decrease(tracked);
      owningScope.increase(tracked);
    } catch (...) {
      // We need to fix the accounting back to the old scope, as we did not
      // perform the move. This is a highly unlikely situation, where in the
      // non-atomic switch of memory ownership someone allocates memory, which
      // causes the increase to break.
      _usageScope.increase(tracked);
      throw;
    }
    // steal the underlying buffer, detaches the heap allocation and resets
    // capacity to the inline value
    uint8_t* ptr = Buffer<uint8_t>::steal();

    // maintain the _local value that the buffer has now after stealing (192
    // bytes)
    _usageScope.increase(this->capacity());
    return ptr;
  }

  uint8_t* steal() noexcept override {
    TRI_ASSERT(false)
        << "raw steal() call not permitted in Supervised Buffer, please use "
           "stealWithMemoryAccounting(ResourceUsageScope& )";

    uint8_t* ptr = Buffer<uint8_t>::steal();
    _usageScope.revert();
    return ptr;
  }

 private:
  void grow(ValueLength length) override {
    auto beforeCap = this->capacity();
    auto beforeSize = this->size();

    // got the precharge values from looking into Buffer::grow()
    constexpr double growthFactor = 1.5;
    ValueLength newLen = beforeSize + length;
    if (newLen < static_cast<ValueLength>(growthFactor * beforeSize)) {
      newLen = static_cast<ValueLength>(growthFactor * beforeSize);
    }

    // how much we expect grow() will allocate
    ValueLength prechargeAmount =
        (newLen > beforeCap) ? (newLen - beforeCap) : 0;

    if (prechargeAmount > 0) {
      _usageScope.increase(
          prechargeAmount);  // may throw here if exceeds memory limit
    }

    try {
      Buffer<uint8_t>::grow(length);

      auto afterCap = this->capacity();
      // the expected amount might not be the actual amount allocated so we
      // adjust
      if (afterCap > beforeCap) {
        auto actualAmount = afterCap - beforeCap;
        if (actualAmount > prechargeAmount) {
          _usageScope.increase(actualAmount - prechargeAmount);
        } else if (actualAmount < prechargeAmount) {
          _usageScope.decrease(prechargeAmount - actualAmount);
        }
      } else if (prechargeAmount > 0) {
        _usageScope.decrease(prechargeAmount);
      }
    } catch (...) {
      if (prechargeAmount > 0) {
        _usageScope.decrease(prechargeAmount);  // undo precharge if it throws
      }
      throw;
    }
  }

  void clear() noexcept override {
    auto before = this->capacity();
    Buffer<uint8_t>::clear();
    auto after = this->capacity();
    // if before > after, means that it has released usage from the heap
    if (before > after) {
      _usageScope.decrease(before - after);
    }
  }

  ResourceUsageScope _usageScope;
};

}  // namespace arangodb::velocypack
