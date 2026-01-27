////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2022 ArangoDB GmbH, Cologne, Germany
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

#include <mutex>  // unique_lock
#include <shared_mutex>

#include "shared.hpp"
#include "store/directory.hpp"

#include <absl/container/flat_hash_map.h>

namespace irs {

template<typename Value>
class CachingHelper {
 public:
  explicit CachingHelper(size_t max_count) noexcept : max_count_{max_count} {}

  template<typename Visitor>
  bool Visit(std::string_view key, Visitor&& visitor) const noexcept {
    const size_t key_hash = cache_.hash_ref()(key);

    {
      std::shared_lock lock{mutex_};
      if (auto it = cache_.find(key, key_hash); it != cache_.end()) {
        return visitor(it->second);
      }
    }

    return false;
  }

  template<typename Constructor>
  bool Put(std::string_view key, Constructor&& ctor) noexcept {
    bool is_new = false;

    if (std::unique_lock lock{mutex_}; cache_.size() < max_count_) {
      try {
        cache_.lazy_emplace(key, [&](const auto& map_ctor) {
          is_new = true;
          map_ctor(key, ctor());
        });
      } catch (...) {
      }
    }

    return is_new;
  }

  void Remove(std::string_view key) noexcept {
    const size_t key_hash = cache_.hash_ref()(key);

    std::lock_guard lock{mutex_};
    if (const auto it = cache_.find(key, key_hash); it != cache_.end()) {
      cache_.erase(it);
    }
  }

  void Rename(std::string_view src, std::string_view dst) noexcept {
    const auto src_hash = cache_.hash_ref()(src);

    std::lock_guard lock{mutex_};
    if (auto src_it = cache_.find(src, src_hash); src_it != cache_.end()) {
      auto tmp = std::move(src_it->second);
      cache_.erase(src_it);
      try {
        IRS_ASSERT(!cache_.contains(dst));
        cache_[dst] = std::move(tmp);
      } catch (...) {
      }
    }
  }

  void Clear() const {
    std::lock_guard lock{mutex_};
    cache_.clear();
  }

  size_t Count() const noexcept {
    std::lock_guard lock{mutex_};
    return cache_.size();
  }

  size_t MaxCount() const noexcept { return max_count_; }

 private:
  mutable std::shared_mutex mutex_;
  mutable absl::flat_hash_map<std::string, Value> cache_;
  size_t max_count_;
};

template<typename Impl, typename Value>
class CachingDirectoryBase : public Impl {
 public:
  using ImplType = Impl;

  template<typename... Args>
  explicit CachingDirectoryBase(size_t max_count, Args&&... args)
    : Impl{std::forward<Args>(args)...}, cache_{max_count} {}

  bool remove(std::string_view name) noexcept final {
#ifdef _MSC_VER
    cache_.Remove(name);  // On Windows it's important to first close the handle
    return Impl::remove(name);
#else
    if (Impl::remove(name)) {
      cache_.Remove(name);
      return true;
    }
    return false;
#endif
  }

  bool rename(std::string_view src, std::string_view dst) noexcept final {
#ifdef _MSC_VER
    cache_.Remove(src);  // On Windows it's impossible to move opened file
    return Impl::rename(src, dst);
#else
    if (Impl::rename(src, dst)) {
      cache_.Rename(src, dst);
      return true;
    }
    return false;
#endif
  }

  bool exists(bool& result, std::string_view name) const noexcept final {
    if (cache_.Visit(name, [&](auto&) noexcept {
          result = true;
          return true;
        })) {
      return true;
    }

    return Impl::exists(result, name);
  }

  const auto& Cache() const noexcept { return cache_; }

 protected:
  mutable CachingHelper<Value> cache_;
};

}  // namespace irs
