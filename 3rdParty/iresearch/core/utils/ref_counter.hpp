//
// IResearch search engine 
// 
// Copyright (c) 2016 by EMC Corporation, All Rights Reserved
// 
// This software contains the intellectual property of EMC Corporation or is licensed to
// EMC Corporation from third parties. Use of this software and the intellectual property
// contained therein is expressly limited to the terms and conditions of the License
// Agreement under which it is provided by or on behalf of EMC.
// 

#ifndef IRESEARCH_REF_COUNTER_H
#define IRESEARCH_REF_COUNTER_H

#include <functional>
#include <memory>
#include <unordered_map>
#include <mutex>
#include "shared.hpp"
#include "utils/noncopyable.hpp"
#include "utils/thread_utils.hpp"

NS_ROOT

template<typename Key, typename Hash = std::hash<Key>, typename Equal = std::equal_to<Key>>
class ref_counter: public util::noncopyable { // noncopyable because shared_ptr refs hold reference to internal map keys
 public:
  typedef std::shared_ptr<const Key> ref_t;
  ref_t add(Key&& key) {
    scoped_lock_t lock(lock_);
    auto itr = refs_.emplace(std::piecewise_construct, std::forward_as_tuple(key), std::forward_as_tuple(nullptr));

    if (itr.second) {
      static auto noop_deleter = [](const Key*) {};

      itr.first->second.reset(&(itr.first->first), noop_deleter);
    }

   return itr.first->second;
  }
  bool contains(const Key& key) {
    scoped_lock_t lock(lock_);
    return refs_.find(key) != refs_.end();
  }
  size_t find(const Key& key) {
    scoped_lock_t lock(lock_);
    auto itr = refs_.find(key);

    return itr == refs_.end() ? 0 : (itr->second.use_count() - 1); // -1 for usage by refs_ itself
  }
  bool empty() {
    scoped_lock_t lock(lock_);
    return refs_.empty();
  }
  bool visit(const std::function<bool(const Key&, size_t)>& visitor, bool remove_unused = false) {
    scoped_lock_t lock(lock_);

    for (auto itr = refs_.begin(), end = refs_.end(); itr != end;) {
      auto visit_next = visitor(itr->first, itr->second.use_count() - 1); // -1 for usage by refs_ itself

      if (remove_unused && itr->second.unique()) {
        itr = refs_.erase(itr);
      }
      else {
        ++itr;
      }

      if (!visit_next) {
        return false;
      }
    }

    return true;
  }
 private:
  std::recursive_mutex lock_; // recursive to allow usage for 'this' from withing visit(...)
  typedef std::lock_guard<decltype(lock_)> scoped_lock_t;
  std::unordered_map<Key, ref_t, Hash, Equal> refs_;
};

NS_END

#endif
