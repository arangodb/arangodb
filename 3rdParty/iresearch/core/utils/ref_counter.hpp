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

#ifndef IRESEARCH_REF_COUNTER_H
#define IRESEARCH_REF_COUNTER_H

#include <functional>
#include <memory>
#include <unordered_set>
#include <mutex>
#include "shared.hpp"
#include "utils/noncopyable.hpp"
#include "utils/thread_utils.hpp"
#include "utils/memory.hpp"

namespace iresearch {

template<typename Key, typename Hash = std::hash<Key>, typename Equal = std::equal_to<Key>>
class ref_counter : public util::noncopyable { // noncopyable because shared_ptr refs hold reference to internal map keys
 public:
  typedef std::shared_ptr<const Key> ref_t;

  struct equal_to : Equal {
    bool operator()(const ref_t& lhs, const ref_t& rhs) const noexcept {
      assert(lhs && rhs);
      return Equal::operator()(*lhs, *rhs);
    }
  }; // equal_to

  struct hash : Hash {
    size_t operator()(const ref_t& value) const noexcept {
      assert(value);
      return Hash::operator()(*value);
    }
  }; // hash

  ref_t add(Key&& key) {
    SCOPED_LOCK(lock_);

    auto res = refs_.emplace(ref_t(), &key);

    if (res.second) {
      try {
        const_cast<ref_t&>(*res.first) = memory::make_shared<const Key>(std::forward<Key>(key));
      } catch (...) {
        // rollback
        refs_.erase(res.first);
        return ref_t();
      }
    }

    return *res.first;
  }

  bool remove(const Key& key) {
    const ref_t ref(ref_t(), &key); // aliasing ctor

    SCOPED_LOCK(lock_);
    return refs_.erase(ref) > 0;
  }

  bool contains(const Key& key) const noexcept {
    const ref_t ref(ref_t(), &key); // aliasing ctor

    SCOPED_LOCK(lock_);
    return refs_.find(ref) != refs_.end();
  }

  size_t find(const Key& key) const noexcept {
    const ref_t ref(ref_t(), &key); // aliasing ctor

    SCOPED_LOCK(lock_);
    auto itr = refs_.find(ref);

    return itr == refs_.end() ? 0 : (itr->use_count() - 1); // -1 for usage by refs_ itself
  }

  bool empty() const noexcept {
    SCOPED_LOCK(lock_);
    return refs_.empty();
  }

  template<typename Visitor>
  bool visit(const Visitor& visitor, bool remove_unused = false) {
    SCOPED_LOCK(lock_);

    for (auto itr = refs_.begin(), end = refs_.end(); itr != end;) {
      auto& ref = *itr;
      assert(*itr);

      auto visit_next = visitor(*ref, ref.use_count() - 1); // -1 for usage by refs_ itself

      if (remove_unused && ref.unique()) {
        itr = refs_.erase(itr);
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
  mutable std::recursive_mutex lock_; // recursive to allow usage for 'this' from withing visit(...)
  std::unordered_set<ref_t, hash, equal_to> refs_;
}; // ref_counter

}

#endif
