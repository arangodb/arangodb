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
////////////////////////////////////////////////////////////////////////////////

#ifndef IRESEARCH_REGISTER_H
#define IRESEARCH_REGISTER_H

#include "singleton.hpp"
#include "so_utils.hpp"
#include "log.hpp"
#include "string.hpp"

#include <memory>
#include <unordered_map>
#include <vector>
#include <string>

// use busywait implementation for Win32 since std::mutex cannot be used in calls going through dllmain()
#ifdef _WIN32
#include "async_utils.hpp"

namespace {
  typedef irs::async_utils::busywait_mutex mutex_t;
}
#else
#include <mutex>

namespace {
  typedef std::mutex mutex_t;
}
#endif

namespace iresearch {

// Generic class representing globally-stored correspondence
// between object of KeyType and EntryType
template<
  typename KeyType,
  typename EntryType,
  typename RegisterType,
  typename Hash = std::hash<KeyType>,
  typename Pred = std::equal_to<KeyType>
> class generic_register : public singleton<RegisterType> {
 public:
  typedef KeyType key_type;
  typedef EntryType entry_type;
  typedef Hash hash_type;
  typedef Pred pred_type;
  typedef std::unordered_map<key_type, entry_type, hash_type, pred_type> register_map_t;
  typedef std::function<bool(const key_type& key)> visitor_t;

  virtual ~generic_register() = default;

  // @return the entry registered under the key and inf an insertion took place
  std::pair<entry_type, bool> set(
      const key_type& key,
      const entry_type& entry) {
    std::lock_guard<mutex_t> lock(mutex_);
    auto itr = reg_map_.emplace(key, entry);
    return std::make_pair(itr.first->second, itr.second);
  }

  entry_type get(const key_type& key, bool load_library) const {
    const entry_type* entry = lookup(key);

    if (entry) {
      return *entry;
    }

    if (load_library) {
      return load_entry_from_so(key);
    }

    IR_FRMT_ERROR("%s : key not found", __FUNCTION__);

    return entry_type();
  }

  bool visit(const visitor_t& visitor) {
    std::lock_guard<mutex_t> lock(mutex_);

    for (auto& entry: reg_map_) {
      if (!visitor(entry.first)) {
        return false;
      }
    }

    return true;
  }

 protected:
  typedef std::function<bool(const key_type& key, const entry_type& value)> protected_visitor_t;

  // Should override this in order to initialize with new library handle
  virtual bool add_so_handle(void* /* handle */) { return true; }

  // Should override this in order to load entry from shared object
  virtual entry_type load_entry_from_so(const key_type& key) const {
    const auto file = key_to_filename(key);

    void* handle = load_library(file.c_str(), 1);

    if (nullptr == handle) {
      IR_FRMT_ERROR("%s : load failed", __FUNCTION__);
      return entry_type();
    }

    auto* this_ptr = const_cast<generic_register<KeyType, EntryType, RegisterType, Hash, Pred>*>(this);

    {
      std::lock_guard<mutex_t> lock(mutex_);

      this_ptr->so_handles_.emplace_back(handle, [] (void* handle)->void {iresearch::free_library(handle); });
    }

    if (!this_ptr->add_so_handle(handle)) {
      IR_FRMT_ERROR("%s : init failed in shared object : %s", __FUNCTION__, file.c_str());
      return entry_type();
    }

    // Here we assume that shared object constructs global object
    // that performs registration
    const entry_type* entry = lookup(key);
    if (nullptr == entry) {
      IR_FRMT_ERROR("%s : lookup failed in shared object : %s", __FUNCTION__, file.c_str());
      return entry_type();
    }

    return *entry;
  }

  // Should convert key to corresponded shared object file name
  virtual std::string key_to_filename(const key_type& /* key */) const {
    return std::string();
  }

  virtual const entry_type* lookup(const key_type& key) const {
    std::lock_guard<mutex_t> lock(mutex_);
    auto it = reg_map_.find(key);
    return reg_map_.end() == it ? nullptr : &it->second;
  }

  bool visit(const protected_visitor_t& visitor) {
    std::lock_guard<mutex_t> lock(mutex_);

    for (auto& entry: reg_map_) {
      if (!visitor(entry.first, entry.second)) {
        return false;
      }
    }

    return true;
  }

 private:
  mutable mutex_t mutex_;
  register_map_t reg_map_;
  std::vector<std::unique_ptr<void, std::function<void(void*)>>> so_handles_;
}; // generic_register

// A generic_registrar capable of storing an associated tag for each entry
template<
  typename KeyType,
  typename EntryType,
  typename TagType,
  typename RegisterType,
  typename Hash = std::hash<KeyType>,
  typename Pred = std::equal_to<KeyType>
> class tagged_generic_register : public generic_register<KeyType, EntryType, RegisterType, Hash, Pred> {
 public:
  typedef typename irs::generic_register<KeyType, EntryType, RegisterType, Hash, Pred> parent_type;
  typedef typename parent_type::key_type key_type;
  typedef typename parent_type::entry_type entry_type;
  typedef typename parent_type::hash_type hash_type;
  typedef typename parent_type::pred_type pred_type;
  typedef TagType tag_type;

  // @return the entry registered under the key and if an insertion took place
  std::pair<entry_type, bool> set(
      const key_type& key,
      const entry_type& entry,
      const tag_type* tag = nullptr) {
    auto itr = parent_type::set(key, entry);

    if (tag && itr.second) {
      std::lock_guard<mutex_t> lock(mutex_);
      tag_map_.emplace(key, *tag);
    }

    return itr;
  }

  const tag_type* tag(const key_type& key) const {
    std::lock_guard<mutex_t> lock(mutex_);
    auto itr = tag_map_.find(key);

    return itr == tag_map_.end() ? nullptr : &(itr->second);
  }

  private:
   typedef std::unordered_map<key_type, tag_type, hash_type, pred_type> tag_map_t;
   mutable mutex_t mutex_;
   tag_map_t tag_map_;
};

}

#endif
