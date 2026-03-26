////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2016 by EMC Corporation, All Rights Reserved
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
/// Copyright holder is EMC Corporation
///
/// @author Andrey Abramov
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <functional>
#include <memory>
#include <mutex>

#include "shared.hpp"
#include "utils/assert.hpp"
#include "utils/noncopyable.hpp"

#include <absl/container/flat_hash_set.h>

namespace irs {

template<typename Key,
         typename Hash = typename absl::flat_hash_set<Key>::hasher,
         typename Equal = typename absl::flat_hash_set<Key, Hash>::key_equal>
class ref_counter : public util::noncopyable {
 public:
  using ref_t = std::shared_ptr<const Key>;

  struct equal_to : Equal {
    using is_transparent = void;

    using Equal::operator();

    template<typename T>
    bool operator()(const T& lhs, const ref_t& rhs) const noexcept {
      IRS_ASSERT(rhs);
      return Equal::operator()(lhs, *rhs);
    }

    template<typename T>
    bool operator()(const ref_t& lhs, const T& rhs) const noexcept {
      IRS_ASSERT(lhs);
      return Equal::operator()(*lhs, rhs);
    }

    bool operator()(const ref_t& lhs, const ref_t& rhs) const noexcept {
      IRS_ASSERT(lhs);
      IRS_ASSERT(rhs);
      return Equal::operator()(*lhs, *rhs);
    }
  };

  struct hash : Hash {
    using is_transparent = void;

    using Hash::operator();

    size_t operator()(const ref_t& value) const noexcept {
      IRS_ASSERT(value);
      return Hash::operator()(*value);
    }
  };

  template<typename T>
  ref_t add(T&& key) {
    std::lock_guard lock{lock_};

    auto it = refs_.lazy_emplace(key, [&](const auto& ctor) {
      ctor(std::make_shared<const Key>(std::forward<T>(key)));
    });

    return *it;
  }

  template<typename T>
  bool remove(T&& key) {
    std::lock_guard lock{lock_};
    return refs_.erase(std::forward<T>(key)) > 0;
  }

  template<typename T>
  bool contains(T&& key) const noexcept {
    std::lock_guard lock{lock_};
    return refs_.contains(std::forward<T>(key));
  }

  template<typename T>
  size_t find(T&& key) const noexcept {
    std::lock_guard lock{lock_};
    auto itr = refs_.find(std::forward<T>(key));

    return itr == refs_.end()
             ? 0
             : (itr->use_count() - 1);  // -1 for usage by refs_ itself
  }

  bool empty() const noexcept {
    std::lock_guard lock{lock_};
    return refs_.empty();
  }

  template<typename Visitor>
  bool visit(const Visitor& visitor, bool remove_unused = false) {
    std::lock_guard lock{lock_};

    for (auto itr = refs_.begin(), end = refs_.end(); itr != end;) {
      auto& ref = *itr;
      IRS_ASSERT(*itr);

      // -1 for usage by refs_ itself
      auto visit_next = visitor(*ref, ref.use_count() - 1);

      if (remove_unused && ref.use_count() == 1) {
        const auto erase_me = itr++;
        refs_.erase(erase_me);
      } else {
        ++itr;
      }

      if (!visit_next) {
        return false;
      }
    }

    return true;
  }

 private:
  // recursive to allow usage for 'this' from withing visit(...)
  mutable std::recursive_mutex lock_;
  absl::flat_hash_set<ref_t, hash, equal_to> refs_;
};

}  // namespace irs
