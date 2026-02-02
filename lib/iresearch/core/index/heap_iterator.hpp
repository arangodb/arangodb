////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2019 ArangoDB GmbH, Cologne, Germany
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
/// @author Andrey Abramov
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <algorithm>
#include <vector>

#include "iterators.hpp"
#include "shared.hpp"

namespace irs {

template<typename Context>
class ExternalMergeIterator {
  using Value = typename Context::Value;

 public:
  template<typename... Args>
  explicit ExternalMergeIterator(Args&&... args)
    : ctx_{std::forward<Args>(args)...} {}

  bool Initilized() const noexcept { return !tree_.empty(); }

  void Reset(std::span<Value> values) noexcept {
    size_ = 0;
    tree_.clear();
    values_ = values;
  }

  bool Next() {
    if (IRS_UNLIKELY(size_ == 0)) {
      return LazyReset();
    }
    auto& lead = Lead();
    auto position = values_.size() + (&lead - values_.data());
    if (IRS_UNLIKELY(!ctx_(lead))) {
      if (--size_ == 0) {
        return false;
      }
      tree_[position] = nullptr;
    }
    while ((position /= 2) != 0) {
      tree_[position] = Compute(position);
    }
    return true;
  }

  IRS_FORCE_INLINE Value& Lead() const noexcept {
    IRS_ASSERT(size_ > 0);
    IRS_ASSERT(tree_.size() > 1);
    IRS_ASSERT(tree_[1] != nullptr);
    return *tree_[1];
  }

  size_t Size() const noexcept { return size_; }

 private:
  IRS_FORCE_INLINE Value* Compute(size_t position) {
    auto* lhs = tree_[2 * position];
    auto* rhs = tree_[2 * position + 1];
    if ((lhs != nullptr) & (rhs != nullptr)) {
      return ctx_(*lhs, *rhs) ? lhs : rhs;
    }
    return reinterpret_cast<Value*>(reinterpret_cast<uintptr_t>(lhs) |
                                    reinterpret_cast<uintptr_t>(rhs));
  }

  IRS_NO_INLINE bool LazyReset() {
    IRS_ASSERT(size_ == 0);
    if (!tree_.empty()) {
      return false;
    }
    size_ = values_.size();
    if (size_ == 0) {
      return false;
    }
    // bottom-up min segment tree construction
    tree_.resize(size_ * 2);
    // init leafs
    auto* values = values_.data();
    for (auto it = tree_.begin() + size_, end = tree_.end(); it != end; ++it) {
      if (ctx_(*values)) {
        *it = values;
      } else {
        *it = nullptr;
        --size_;
      }
      ++values;
    }
    // compute tree
    for (auto position = values_.size() - 1; position != 0; --position) {
      tree_[position] = Compute(position);
    }
    // stub node for faster compute
    tree_[0] = nullptr;
    return size_ != 0;
  }

  IRS_NO_UNIQUE_ADDRESS Context ctx_;
  size_t size_{0};
  std::vector<Value*> tree_;
  std::span<Value> values_;
};

}  // namespace irs
