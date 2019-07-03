////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2016 ArangoDB GmbH, Cologne, Germany
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
/// @author Ewout Prangsma
////////////////////////////////////////////////////////////////////////////////
#pragma once

#ifndef ARANGO_CXX_DRIVER_MESSAGE_STORE_H
#define ARANGO_CXX_DRIVER_MESSAGE_STORE_H 1

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <map>
#include <mutex>

#include <fuerte/helper.h>

namespace arangodb { namespace fuerte { inline namespace v1 {
// MessageStore keeps a thread safe list of all requests that are "in-flight".
template <typename RequestItemT>
class MessageStore {
 public:
  // add a given item to the store (indexed by its ID).
  void add(std::shared_ptr<RequestItemT> item) {
    _map.emplace(item->messageID(), item);
  }

  // findByID returns the item with given ID or nullptr is no such ID is
  // found in the store.
  std::shared_ptr<RequestItemT> findByID(MessageID id) {
    auto found = _map.find(id);
    if (found == _map.end()) {
      // ID not found
      return std::shared_ptr<RequestItemT>();
    }
    return found->second;
  }

  // removeByID removes the item with given ID from the store.
  void removeByID(MessageID id) {
    _map.erase(id);
  }

  // Notify all items that their being cancelled (by calling their onError)
  // and remove all items from the store.
  void cancelAll(const ErrorCondition error = ErrorCondition::Canceled) {
    for (auto& item : _map) {
      item.second->invokeOnError(errorToInt(error));
    }
    _map.clear();
  }

  // size returns the number of elements in the store.
  size_t size() const {
    return _map.size();
  }

  // empty returns true when there are no elements in the store, false
  // otherwise.
  bool empty() const {
    return _map.empty();
 }
  
  /// invoke functor on all entries
  template<typename F>
  inline size_t invokeOnAll(F func, bool unlocked = false) {
    auto it = _map.begin();
    while (it != _map.end()) {
      if (!func(it->second.get())) {
        it = _map.erase(it);
      } else {
        it++;
      }
    }
    return _map.size();
  }

  // keys returns a string representation of all MessageID's in the store.
  std::string keys() const {
    return mapToKeys(_map);
  }

 private:
  std::map<MessageID, std::shared_ptr<RequestItemT>> _map;
};

}}}  // namespace arangodb::fuerte::v1
#endif
