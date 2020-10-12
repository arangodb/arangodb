////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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
/// @author Daniel H. Larkin
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGO_SHARED_COUNTER_H
#define ARANGO_SHARED_COUNTER_H 1

#include <atomic>
#include <functional>

#include "Basics/Common.h"
#include "Basics/SharedAtomic.h"
#include "Basics/Thread.h"
#include "Basics/debugging.h"
#include "Basics/fasthash.h"

namespace arangodb {
namespace basics {

template <uint64_t stripes = 64, bool everywhereNonNegative = false>
struct SharedCounter {
  typedef std::function<uint64_t()> IdFunc;
  static uint64_t DefaultIdFunc() {
    return fasthash64_uint64(Thread::currentThreadNumber(), 0xdeadbeefdeadbeefULL);
  }

  SharedCounter() : SharedCounter(DefaultIdFunc) {}

  explicit SharedCounter(IdFunc f) : _id(f) {
    for (_mask = 1; _mask <= stripes; _mask <<= 1) {
    }
    TRI_ASSERT(_mask > stripes);
    _mask >>= 1;
    TRI_ASSERT(_mask <= stripes);
    _mask -= 1;

    for (size_t i = 0; i < stripes; i++) {
      _data[i].store(0, std::memory_order_relaxed);
    }
  }

  explicit SharedCounter(SharedCounter<stripes> const& other) { copy(other); }

  SharedCounter<stripes>& operator=(SharedCounter<stripes> const& other) {
    copy(other);
    return *this;
  }

  void add(int64_t arg, std::memory_order order = std::memory_order_seq_cst) {
    _data[_id() & _mask].fetch_add(arg, order);
  }

  void sub(int64_t arg, std::memory_order order = std::memory_order_seq_cst) {
    int64_t prev = _data[_id() & _mask].fetch_sub(arg, order);
    TRI_ASSERT(!everywhereNonNegative || (prev - arg >= 0));
  }

  int64_t value(std::memory_order order = std::memory_order_seq_cst) const {
    int64_t sum = 0;
    for (size_t i = 0; i < stripes; i++) {
      sum += _data[i].load(order);
    }
    return sum;
  }

  bool nonZero(std::memory_order order = std::memory_order_seq_cst) const {
    int64_t sum = 0;
    for (size_t i = 0; i < stripes; i++) {
      sum += _data[i].load(order);
      if (everywhereNonNegative && sum > 0) {
        return true;
      }
    }
    return (sum != 0);
  }

  void reset(std::memory_order order = std::memory_order_seq_cst) {
    for (size_t i = 0; i < stripes; i++) {
      _data[i].store(0, order);
    }
  }

 private:
  SharedAtomic<int64_t> _data[stripes];
  IdFunc _id;
  uint64_t _mask;

  void copy(SharedCounter<stripes> const& other) {
    if (this != &other) {
      _id = other._id;
      _mask = other._mask;
      for (size_t i = 0; i < stripes; i++) {
        _data[i].store(other._data[i].load(std::memory_order_acquire), std::memory_order_release);
      }
    }
  }
};

}  // namespace basics
}  // namespace arangodb

#endif
