// Copyright 2005-2020 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the 'License');
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an 'AS IS' BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// See www.openfst.org for extensive documentation on this weighted
// finite-state transducer library.
//
// Classes for representing a bijective mapping between an arbitrary entry
// of type T and a signed integral ID.

#ifndef FST_BI_TABLE_H_
#define FST_BI_TABLE_H_

#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <type_traits>
#include <unordered_set>
#include <vector>

#include <fst/log.h>
#include <fst/memory.h>
#include <fst/windows_defs.inc>
#include <unordered_map>
#include <unordered_set>

namespace fst {

// Bitables model bijective mappings between entries of an arbitrary type T and
// an signed integral ID of type I. The IDs are allocated starting from 0 in
// order.
//
// template <class I, class T>
// class BiTable {
//  public:
//
//   // Required constructors.
//   BiTable();
//
//   // Looks up integer ID from entry. If it doesn't exist and insert
//   / is true, adds it; otherwise, returns -1.
//   I FindId(const T &entry, bool insert = true);
//
//   // Looks up entry from integer ID.
//   const T &FindEntry(I) const;
//
//   // Returns number of stored entries.
//   I Size() const;
// };

// An implementation using a hash map for the entry to ID mapping. H is the
// hash function and E is the equality function.
template <class I, class T, class H, class E = std::equal_to<T>>
class HashBiTable {
 public:
  // Reserves space for table_size elements.
  explicit HashBiTable(size_t table_size = 0, const H &h = H(),
                       const E &e = E())
      : hash_func_(h),
        hash_equal_(e),
        entry2id_(table_size, hash_func_, hash_equal_) {
    if (table_size) id2entry_.reserve(table_size);
  }

  HashBiTable(const HashBiTable<I, T, H, E> &table)
      : hash_func_(table.hash_func_),
        hash_equal_(table.hash_equal_),
        entry2id_(table.entry2id_.begin(), table.entry2id_.end(),
                  table.entry2id_.size(), hash_func_, hash_equal_),
        id2entry_(table.id2entry_) {}

  I FindId(const T &entry, bool insert = true) {
    if (!insert) {
      const auto it = entry2id_.find(entry);
      return it == entry2id_.end() ? -1 : it->second - 1;
    }
    I &id_ref = entry2id_[entry];
    if (id_ref == 0) {  // T not found; stores and assigns a new ID.
      id2entry_.push_back(entry);
      id_ref = id2entry_.size();
    }
    return id_ref - 1;  // NB: id_ref = ID + 1.
  }

  const T &FindEntry(I s) const { return id2entry_[s]; }

  I Size() const { return id2entry_.size(); }

  // TODO(riley): Add fancy clear-to-size, as in CompactHashBiTable.
  void Clear() {
    entry2id_.clear();
    id2entry_.clear();
  }

 private:
  H hash_func_;
  E hash_equal_;
  std::unordered_map<T, I, H, E> entry2id_;
  std::vector<T> id2entry_;
};

// Enables alternative hash set representations below.
enum HSType { HS_STL, HS_FLAT };

// Default hash set is STL hash_set.
template <class K, class H, class E, HSType HS>
struct HashSet : public std::unordered_set<K, H, E, PoolAllocator<K>> {
 private:
  using Base = std::unordered_set<K, H, E, PoolAllocator<K>>;
 public:
  using Base::Base;

  void rehash(size_t n) {}
};

// An implementation using a hash set for the entry to ID mapping. The hash set
// holds keys which are either the ID or kCurrentKey. These keys can be mapped
// to entries either by looking up in the entry vector or, if kCurrentKey, in
// current_entry_. The hash and key equality functions map to entries first. H
// is the hash function and E is the equality function.
template <class I, class T, class H, class E = std::equal_to<T>,
          HSType HS = HS_FLAT>
class CompactHashBiTable {
  static_assert(HS == HS_STL || HS == HS_FLAT, "Unsupported hash set type");

 public:
  friend class HashFunc;
  friend class HashEqual;

  // Reserves space for table_size elements.
  explicit CompactHashBiTable(size_t table_size = 0, const H &h = H(),
                              const E &e = E())
      : hash_func_(h),
        hash_equal_(e),
        compact_hash_func_(*this),
        compact_hash_equal_(*this),
        keys_(table_size, compact_hash_func_, compact_hash_equal_) {
    if (table_size) id2entry_.reserve(table_size);
  }

  CompactHashBiTable(const CompactHashBiTable &table)
      : hash_func_(table.hash_func_),
        hash_equal_(table.hash_equal_),
        compact_hash_func_(*this),
        compact_hash_equal_(*this),
        id2entry_(table.id2entry_),
        keys_(table.keys_.begin(), table.keys_.end(), table.keys_.size(),
              compact_hash_func_, compact_hash_equal_) {}

  I FindId(const T &entry, bool insert = true) {
    current_entry_ = &entry;
    if (insert) {
      auto [iter, was_inserted] = keys_.insert(kCurrentKey);
      if (!was_inserted) return *iter;  // Already exists.
      // Overwrites kCurrentKey with a new key value; this is safe because it
      // doesn't affect hashing or equality testing.
      I key = id2entry_.size();
      const_cast<I &>(*iter) = key;
      id2entry_.push_back(entry);
      return key;
    }
    const auto it = keys_.find(kCurrentKey);
    return it == keys_.end() ? -1 : *it;
  }

  const T &FindEntry(I s) const { return id2entry_[s]; }

  I Size() const { return id2entry_.size(); }

  // Clears content; with argument, erases last n IDs.
  void Clear(ssize_t n = -1) {
    if (n < 0 || n >= id2entry_.size()) {  // Clears completely.
      keys_.clear();
      id2entry_.clear();
    } else if (n == id2entry_.size() - 1) {  // Leaves only key 0.
      const T entry = FindEntry(0);
      keys_.clear();
      id2entry_.clear();
      FindId(entry, true);
    } else {
      while (n-- > 0) {
        I key = id2entry_.size() - 1;
        keys_.erase(key);
        id2entry_.pop_back();
      }
      keys_.rehash(0);
    }
  }

 private:
  static_assert(std::is_signed_v<I>, "I must be a signed type");
  // ... otherwise >= kCurrentKey comparisons as used below don't work.
  // TODO(rybach): (1) don't use >= for key comparison, (2) allow unsigned key
  // types.
  static constexpr I kCurrentKey = -1;

  class HashFunc {
   public:
    explicit HashFunc(const CompactHashBiTable &ht) : ht_(&ht) {}

    size_t operator()(I k) const {
      if (k >= kCurrentKey) {
        return (ht_->hash_func_)(ht_->Key2Entry(k));
      } else {
        return 0;
      }
    }

   private:
    const CompactHashBiTable *ht_;
  };

  class HashEqual {
   public:
    explicit HashEqual(const CompactHashBiTable &ht) : ht_(&ht) {}

    bool operator()(I k1, I k2) const {
      if (k1 == k2) {
        return true;
      } else if (k1 >= kCurrentKey && k2 >= kCurrentKey) {
        return (ht_->hash_equal_)(ht_->Key2Entry(k1), ht_->Key2Entry(k2));
      } else {
        return false;
      }
    }

   private:
    const CompactHashBiTable *ht_;
  };

  using KeyHashSet = HashSet<I, HashFunc, HashEqual, HS>;

  const T &Key2Entry(I k) const {
    if (k == kCurrentKey) {
      return *current_entry_;
    } else {
      return id2entry_[k];
    }
  }

  H hash_func_;
  E hash_equal_;
  HashFunc compact_hash_func_;
  HashEqual compact_hash_equal_;
  std::vector<T> id2entry_;
  KeyHashSet keys_;
  const T *current_entry_;
};

// An implementation using a vector for the entry to ID mapping. It is passed a
// function object FP that should fingerprint entries uniquely to an integer
// that can used as a vector index. Normally, VectorBiTable constructs the FP
// object. The user can instead pass in this object.
template <class I, class T, class FP>
class VectorBiTable {
 public:
  // Reserves table_size cells of space.
  explicit VectorBiTable(const FP &fp = FP(), size_t table_size = 0) : fp_(fp) {
    if (table_size) id2entry_.reserve(table_size);
  }

  VectorBiTable(const VectorBiTable<I, T, FP> &table)
      : fp_(table.fp_), fp2id_(table.fp2id_), id2entry_(table.id2entry_) {}

  I FindId(const T &entry, bool insert = true) {
    ssize_t fp = (fp_)(entry);
    if (fp >= fp2id_.size()) fp2id_.resize(fp + 1);
    I &id_ref = fp2id_[fp];
    if (id_ref == 0) {  // T not found.
      if (insert) {     // Stores and assigns a new ID.
        id2entry_.push_back(entry);
        id_ref = id2entry_.size();
      } else {
        return -1;
      }
    }
    return id_ref - 1;  // NB: id_ref = ID + 1.
  }

  const T &FindEntry(I s) const { return id2entry_[s]; }

  I Size() const { return id2entry_.size(); }

  const FP &Fingerprint() const { return fp_; }

 private:
  FP fp_;
  std::vector<I> fp2id_;
  std::vector<T> id2entry_;
};

// An implementation using a vector and a compact hash table. The selecting
// functor S returns true for entries to be hashed in the vector. The
// fingerprinting functor FP returns a unique fingerprint for each entry to be
// hashed in the vector (these need to be suitable for indexing in a vector).
// The hash functor H is used when hashing entry into the compact hash table.
template <class I, class T, class S, class FP, class H, HSType HS = HS_FLAT>
class VectorHashBiTable {
 public:
  friend class HashFunc;
  friend class HashEqual;

  explicit VectorHashBiTable(const S &s = S(), const FP &fp = FP(),
                             const H &h = H(), size_t vector_size = 0,
                             size_t entry_size = 0)
      : selector_(s),
        fp_(fp),
        h_(h),
        hash_func_(*this),
        hash_equal_(*this),
        keys_(0, hash_func_, hash_equal_) {
    if (vector_size) fp2id_.reserve(vector_size);
    if (entry_size) id2entry_.reserve(entry_size);
  }

  VectorHashBiTable(const VectorHashBiTable<I, T, S, FP, H, HS> &table)
      : selector_(table.selector_),
        fp_(table.fp_),
        h_(table.h_),
        id2entry_(table.id2entry_),
        fp2id_(table.fp2id_),
        hash_func_(*this),
        hash_equal_(*this),
        keys_(table.keys_.size(), hash_func_, hash_equal_) {
    keys_.insert(table.keys_.begin(), table.keys_.end());
  }

  I FindId(const T &entry, bool insert = true) {
    if ((selector_)(entry)) {  // Uses the vector if selector_(entry) == true.
      uint64_t fp = (fp_)(entry);
      if (fp2id_.size() <= fp) fp2id_.resize(fp + 1, 0);
      if (fp2id_[fp] == 0) {  // T not found.
        if (insert) {         // Stores and assigns a new ID.
          id2entry_.push_back(entry);
          fp2id_[fp] = id2entry_.size();
        } else {
          return -1;
        }
      }
      return fp2id_[fp] - 1;  // NB: assoc_value = ID + 1.
    } else {                  // Uses the hash table otherwise.
      current_entry_ = &entry;
      const auto it = keys_.find(kCurrentKey);
      if (it == keys_.end()) {
        if (insert) {
          I key = id2entry_.size();
          id2entry_.push_back(entry);
          keys_.insert(key);
          return key;
        } else {
          return -1;
        }
      } else {
        return *it;
      }
    }
  }

  const T &FindEntry(I s) const { return id2entry_[s]; }

  I Size() const { return id2entry_.size(); }

  const S &Selector() const { return selector_; }

  const FP &Fingerprint() const { return fp_; }

  const H &Hash() const { return h_; }

 private:
  static constexpr I kCurrentKey = -1;

  class HashFunc {
   public:
    explicit HashFunc(const VectorHashBiTable &ht) : ht_(&ht) {}

    size_t operator()(I k) const {
      if (k >= kCurrentKey) {
        return (ht_->h_)(ht_->Key2Entry(k));
      } else {
        return 0;
      }
    }

   private:
    const VectorHashBiTable *ht_;
  };

  class HashEqual {
   public:
    explicit HashEqual(const VectorHashBiTable &ht) : ht_(&ht) {}

    bool operator()(I k1, I k2) const {
      if (k1 >= kCurrentKey && k2 >= kCurrentKey) {
        return ht_->Key2Entry(k1) == ht_->Key2Entry(k2);
      } else {
        return k1 == k2;
      }
    }

   private:
    const VectorHashBiTable *ht_;
  };

  using KeyHashSet = HashSet<I, HashFunc, HashEqual, HS>;

  const T &Key2Entry(I k) const {
    if (k == kCurrentKey) {
      return *current_entry_;
    } else {
      return id2entry_[k];
    }
  }

  S selector_;  // True if entry hashed into vector.
  FP fp_;       // Fingerprint used for hashing into vector.
  H h_;         // Hash funcion used for hashing into hash_set.

  std::vector<T> id2entry_;  // Maps state IDs to entry.
  std::vector<I> fp2id_;     // Maps entry fingerprints to IDs.

  // Compact implementation of the hash table mapping entries to state IDs
  // using the hash function h_.
  HashFunc hash_func_;
  HashEqual hash_equal_;
  KeyHashSet keys_;
  const T *current_entry_;
};

// An implementation using a hash map for the entry to ID mapping. This version
// permits erasing of arbitrary states. The entry T must have == defined and
// its default constructor must produce a entry that will never be seen. F is
// the hash function.
template <class I, class T, class F>
class ErasableBiTable {
 public:
  ErasableBiTable() : first_(0) {}

  I FindId(const T &entry, bool insert = true) {
    I &id_ref = entry2id_[entry];
    if (id_ref == 0) {  // T not found.
      if (insert) {     // Stores and assigns a new ID.
        id2entry_.push_back(entry);
        id_ref = id2entry_.size() + first_;
      } else {
        return -1;
      }
    }
    return id_ref - 1;  // NB: id_ref = ID + 1.
  }

  const T &FindEntry(I s) const { return id2entry_[s - first_]; }

  I Size() const { return id2entry_.size(); }

  void Erase(I s) {
    auto &ref = id2entry_[s - first_];
    entry2id_.erase(ref);
    ref = empty_entry_;
    while (!id2entry_.empty() && id2entry_.front() == empty_entry_) {
      id2entry_.pop_front();
      ++first_;
    }
  }

 private:
  std::unordered_map<T, I, F> entry2id_;
  std::deque<T> id2entry_;
  const T empty_entry_;
  I first_;  // I of first element in the deque.
};

}  // namespace fst

#endif  // FST_BI_TABLE_H_
