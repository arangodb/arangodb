////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2017 ArangoDB GmbH, Cologne, Germany
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
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#ifndef IRESEARCH_HASH_TABLE_H
#define IRESEARCH_HASH_TABLE_H

#include <functional>
#include "utils/ebo.hpp"

NS_ROOT

template<typename Key, typename Hash, typename Equal>
struct hash_base : public compact_pair<Hash, Equal> {
  typedef compact_pair<Hash, Equal> rep_t;
  typedef Key key_t;
  typedef typename rep_t::first_type hash_t;
  typedef typename rep_t::second_type equal_t;

  size_t hash(const key_t& key) const {
    return rep_t::first()(key);
  }

  bool equal(const key_t& lhs, const key_t& rhs) const {
    return rep_t::second()(lhs, rhs);
  }
};

template<
  typename Key,
  typename Hash = std::hash<Key>,
  typename Equal = std::equal_to<Key>
> class hash_set : public hash_base<Key, Hash, Equal> {
 public:
  typedef hash_base<Key, Hash, Equal> base_t;
  typedef typename base_t::key_t key_t;

  explicit hash_set(size_t size)
    : table_(size) {
  }

  const key_t* insert(const key_t& key) {
    const auto hash = base_t::hash(key);
    const auto pos = table_.begin() + hash % mask_;

    const auto end = table_.end();
    auto it = pos;
    do {
      auto& slot = *it;
      if (!slot) {
        slot = key;
        ++size_;
        return &slot;
      } else if (base_t::equal(slot, key)) {
        return &slot;
      }
      ++it;
    } while (it < end);

    for (it = table_.begin(); it != pos; ++it) {
      auto& slot = *it;
      if (!slot) {
        slot = key;
        ++size_;
        return &slot;
      } else if (base_t::equal(slot, key)) {
        return &slot;
      }
      ++it;
    }

    return nullptr;
  }

  bool remove(const key_t& key) {
    const auto hash = base_t::hash(key);
    auto pos = table_.begin() + hash % mask_;

    const auto end = table_.end();
    auto it = pos;
    do {
      auto& slot = *it;

      if (!slot) {
        return false;
      } else if (base_t::equal(slot, key)) {
        slot = key_t();
        --size_;
        return true;
      }

      ++it;
    } while (it < end);

    for (it = table_.begin(); it != pos; ++it) {
      auto& slot = *it;

      if (!slot) {
        return false;
      } else if (base_t::equal(slot, key)) {
        slot = key_t();
        --size_;
        return true;
      }

      ++it;
    }

    return false;
  }

  void clear() {
    table_.clear();
    size_ = 0;
  }

  inline bool contains(const key_t& key) const {
//    const auto hash = base_t::hash(key);
    const auto pos = table_[ key->hash % mask_ ];
    return pos && pos == key;

//    if (*pos && *pos == key) {
//      return true;
////    } else if (!*pos) {
////      return false;
//    }

//    for (auto it = pos + 1, end = table_.end(); it != end; ++it) {
//      auto& slot = *it;
//
//      if (!slot) {
//        return false;
//      } else if (base_t::equal(slot, key)) {
//        return true;
//      }
//
//      ++it;
//    }
//
//    for (auto it = table_.begin(); it != pos; ++it) {
//      auto& slot = *it;
//
//      if (!slot) {
//        return false;
//      } else if (base_t::equal(slot, key)) {
//        return true;
//      }
//
//      ++it;
//    }
//
//    return false;
  }

  size_t size() const NOEXCEPT {
    return size_;
  }

  size_t empty() const NOEXCEPT {
    return 0 == size_;
  }

 private:
  std::vector<key_t> table_;
  size_t size_{};
  size_t mask_{ table_.size() - 1 };
}; // hash_set

NS_END // ROOT

#endif // IRESEARCH_HASH_TABLE_H
