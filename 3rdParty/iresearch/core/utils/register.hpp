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

  NS_LOCAL
    typedef iresearch::async_utils::busywait_mutex mutex_t;
  NS_END
#else
  #include <mutex>

  NS_LOCAL
    typedef std::mutex mutex_t;

  NS_END
#endif

NS_ROOT

// Generic class representing globally-stored correspondence
// between object of KeyType and EntryType
template< typename KeyType, typename EntryType, typename RegisterType >
class generic_register: public singleton<RegisterType> {
 public:
  typedef KeyType key_type;
  typedef EntryType entry_type;
  typedef std::unordered_map<key_type, entry_type> register_map_t;
  typedef std::function<bool(const key_type& key)> visitor_t;

  virtual ~generic_register() { }

  // @return successful new registration (false == 'key' already registered)
  bool set(const key_type& key, const entry_type& entry) {
    std::lock_guard<mutex_t> lock(mutex_);
    return reg_map_.emplace(key, entry).second;
  }

  entry_type get(const key_type& key) const {
    const entry_type* entry = lookup(key);
    return entry ? *entry : load_entry_from_so(key);
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

    auto* this_ptr = const_cast<generic_register<KeyType, EntryType, RegisterType>*>(this);

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
    typename register_map_t::const_iterator it = reg_map_.find(key);
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

NS_END

#endif