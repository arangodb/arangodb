// bi-table.h

// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// Copyright 2005-2010 Google, Inc.
// Author: riley@google.com (Michael Riley)
//
// \file
// Classes for representing a bijective mapping between an arbitrary entry
// of type T and a signed integral ID.

#ifndef FST_LIB_BI_TABLE_H__
#define FST_LIB_BI_TABLE_H__

#include <deque>
using std::deque;
#include <functional>
#include <unordered_map>
using std::unordered_map;
using std::unordered_multimap;
#include <unordered_set>
using std::unordered_set;
using std::unordered_multiset;
#include <vector>
using std::vector;

#include <fst/memory.h>
#include <unordered_set>
using std::unordered_set;
using std::unordered_multiset;

namespace fst {

// BI TABLES - these determine a bijective mapping between an
// arbitrary entry of type T and an signed integral ID of type I. The IDs are
// allocated starting from 0 in order.
//
// template <class I, class T>
// class BiTable {
//  public:
//
//   // Required constructors.
//   BiTable();
//
//   // Lookup integer ID from entry. If it doesn't exist and 'insert'
//   / is true, then add it. Otherwise return -1.
//   I FindId(const T &entry, bool insert = true);
//   // Lookup entry from integer ID.
//   const T &FindEntry(I) const;
//   // # of stored entries.
//   I Size() const;
// };

// An implementation using a hash map for the entry to ID mapping.
// H is the hash function and E is the equality function.
// If passed to the constructor, ownership is given to this class.

template <class I, class T, class H, class E = std::equal_to<T> >
class HashBiTable {
 public:
  // Reserves space for 'table_size' elements.
  explicit HashBiTable(size_t table_size = 0, H *h = 0, E *e = 0)
      : hash_func_(h),
        hash_equal_(e),
        entry2id_(table_size, (h ? *h : H()), (e ? *e : E())) {
    if (table_size)
      id2entry_.reserve(table_size);
  }

  HashBiTable(const HashBiTable<I, T, H, E> &table)
      : hash_func_(table.hash_func_ ? new H(*table.hash_func_) : 0),
        hash_equal_(table.hash_equal_ ? new E(*table.hash_equal_) : 0),
        entry2id_(table.entry2id_.begin(), table.entry2id_.end(),
                  table.entry2id_.size(),
                  (hash_func_ ? *hash_func_ : H()),
                  (hash_equal_ ? *hash_equal_ : E())),
        id2entry_(table.id2entry_) { }

  ~HashBiTable() {
    delete hash_func_;
    delete hash_equal_;
  }

  I FindId(const T &entry, bool insert = true) {
    if (!insert) {
      typename unordered_map<T, I, H, E>::const_iterator it = entry2id_.find(entry);
      return it == entry2id_.end() ? -1 : it->second - 1;
    }
    I &id_ref = entry2id_[entry];
    if (id_ref == 0) {  // T not found  store and assign it a new ID
      id2entry_.push_back(entry);
      id_ref = id2entry_.size();
    }
    return id_ref - 1;  // NB: id_ref = ID + 1
  }

  const T &FindEntry(I s) const {
    return id2entry_[s];
  }

  I Size() const { return id2entry_.size(); }

  // TODO(riley): Add fancy clear-to-size as in CompactHashBiTable.
  void Clear() {
    entry2id_.clear();
    id2entry_.clear();
  }


 private:
  H *hash_func_;
  E *hash_equal_;
  unordered_map<T, I, H, E> entry2id_;
  vector<T> id2entry_;

  void operator=(const HashBiTable<I, T, H, E> &table);  // disallow
};


// Enables alternative hash set representations below.
typedef enum { HS_STL = 0, HS_DENSE = 1, HS_SPARSE = 2 } HSType;

// Default hash set is STL hash_set
template<class K, class H, class E, HSType>
struct  HashSet : public unordered_set<K, H, E, PoolAllocator<K> > {
  HashSet(size_t n = 0, const H &h = H(), const E &e = E())
      : unordered_set<K, H, E, PoolAllocator<K> >(n, h, e)  { }

  void rehash(size_t n) { }
};


// An implementation using a hash set for the entry to ID mapping.
// The hash set holds 'keys' which are either the ID or kCurrentKey.
// These keys can be mapped to entrys either by looking up in the
// entry vector or, if kCurrentKey, in current_entry_ member. The hash
// and key equality functions map to entries first. H
// is the hash function and E is the equality function. If passed to
// the constructor, ownership is given to this class.
template <class I, class T, class H,
          class E = std::equal_to<T>, HSType HS = HS_DENSE>
class CompactHashBiTable {
 public:
  friend class HashFunc;
  friend class HashEqual;

  // Reserves space for 'table_size' elements.
  explicit CompactHashBiTable(size_t table_size = 0, H *h = 0, E *e = 0)
      : hash_func_(h),
        hash_equal_(e),
        compact_hash_func_(*this),
        compact_hash_equal_(*this),
        keys_(table_size, compact_hash_func_, compact_hash_equal_) {
    if (table_size)
      id2entry_.reserve(table_size);
  }

  CompactHashBiTable(const CompactHashBiTable<I, T, H, E, HS> &table)
      : hash_func_(table.hash_func_ ? new H(*table.hash_func_) : 0),
        hash_equal_(table.hash_equal_ ? new E(*table.hash_equal_) : 0),
        compact_hash_func_(*this),
        compact_hash_equal_(*this),
        keys_(table.keys_.size(), compact_hash_func_, compact_hash_equal_),
        id2entry_(table.id2entry_) {
    keys_.insert(table.keys_.begin(), table.keys_.end());
 }

  ~CompactHashBiTable() {
    delete hash_func_;
    delete hash_equal_;
  }

  I FindId(const T &entry, bool insert = true) {
    current_entry_ = &entry;
    typename KeyHashSet::const_iterator it = keys_.find(kCurrentKey);
    if (it == keys_.end()) {  // T not found
      if (insert) {           // store and assign it a new ID
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

  const T &FindEntry(I s) const { return id2entry_[s]; }

  I Size() const { return id2entry_.size(); }

  // Clear content. With argument, erases last n IDs.
  void Clear(ssize_t n = -1) {
    if (n < 0 || n > id2entry_.size())
      n = id2entry_.size();
    while (n-- > 0) {
      I key = id2entry_.size() - 1;
      keys_.erase(key);
      id2entry_.pop_back();
    }
    keys_.rehash(0);
  }

 private:
  static const I kCurrentKey;    // -1
  static const I kEmptyKey;      // -2
  static const I kDeletedKey;    // -3

  class HashFunc {
   public:
    HashFunc(const CompactHashBiTable &ht) : ht_(&ht) {}

    size_t operator()(I k) const {
      if (k >= kCurrentKey) {
        return (*ht_->hash_func_)(ht_->Key2Entry(k));
      } else {
        return 0;
      }
    }

   private:
    const CompactHashBiTable *ht_;
  };

  class HashEqual {
   public:
    HashEqual(const CompactHashBiTable &ht) : ht_(&ht) {}

    bool operator()(I k1, I k2) const {
      if (k1 >= kCurrentKey && k2 >= kCurrentKey) {
        return (*ht_->hash_equal_)(ht_->Key2Entry(k1), ht_->Key2Entry(k2));
      } else {
        return k1 == k2;
      }
    }
   private:
    const CompactHashBiTable *ht_;
  };

  typedef HashSet<I, HashFunc, HashEqual, HS> KeyHashSet;

  const T &Key2Entry(I k) const {
    if (k == kCurrentKey)
      return *current_entry_;
    else
      return id2entry_[k];
  }

  H *hash_func_;
  E *hash_equal_;
  HashFunc compact_hash_func_;
  HashEqual compact_hash_equal_;
  KeyHashSet keys_;
  vector<T> id2entry_;
  const T *current_entry_;

  void operator=(const CompactHashBiTable<I, T, H, E, HS> &table);  // disallow
};


template <class I, class T, class H, class E, HSType HS>
const I CompactHashBiTable<I, T, H, E, HS>::kCurrentKey = -1;

template <class I, class T, class H, class E, HSType HS>
const I CompactHashBiTable<I, T, H, E, HS>::kEmptyKey = -2;

template <class I, class T, class H, class E, HSType HS>
const I CompactHashBiTable<I, T, H, E, HS>::kDeletedKey = -3;


// An implementation using a vector for the entry to ID mapping.
// It is passed a function object FP that should fingerprint entries
// uniquely to an integer that can used as a vector index. Normally,
// VectorBiTable constructs the FP object.  The user can instead
// pass in this object; in that case, VectorBiTable takes its
// ownership.
template <class I, class T, class FP>
class VectorBiTable {
 public:
  // Reserves space for 'table_size' elements.
  explicit VectorBiTable(FP *fp = 0, size_t table_size = 0)
      : fp_(fp ? fp : new FP()) {
    if (table_size)
      id2entry_.reserve(table_size);
  }

  VectorBiTable(const VectorBiTable<I, T, FP> &table)
      : fp_(table.fp_ ? new FP(*table.fp_) : 0),
        fp2id_(table.fp2id_),
        id2entry_(table.id2entry_) { }

  ~VectorBiTable() { delete fp_; }

  I FindId(const T &entry, bool insert = true) {
    ssize_t fp = (*fp_)(entry);
    if (fp >= fp2id_.size())
      fp2id_.resize(fp + 1);
    I &id_ref = fp2id_[fp];
    if (id_ref == 0) {  // T not found
      if (insert) {     // store and assign it a new ID
        id2entry_.push_back(entry);
        id_ref = id2entry_.size();
      } else {
        return -1;
      }
    }
    return id_ref - 1;  // NB: id_ref = ID + 1
  }

  const T &FindEntry(I s) const { return id2entry_[s]; }

  I Size() const { return id2entry_.size(); }

  const FP &Fingerprint() const { return *fp_; }

 private:
  FP *fp_;
  vector<I> fp2id_;
  vector<T> id2entry_;

  void operator=(const VectorBiTable<I, T, FP> &table);  // disallow
};


// An implementation using a vector and a compact hash table. The
// selecting functor S returns true for entries to be hashed in the
// vector.  The fingerprinting functor FP returns a unique fingerprint
// for each entry to be hashed in the vector (these need to be
// suitable for indexing in a vector).  The hash functor H is used
// when hashing entry into the compact hash table.  If passed to the
// constructor, ownership is given to this class.
template <class I, class T, class S, class FP, class H, HSType HS = HS_DENSE>
class VectorHashBiTable {
 public:
  friend class HashFunc;
  friend class HashEqual;

  explicit VectorHashBiTable(S *s, FP *fp, H *h,
                             size_t vector_size = 0,
                             size_t entry_size = 0)
      : selector_(s),
        fp_(fp),
        h_(h),
        hash_func_(*this),
        hash_equal_(*this),
        keys_(0, hash_func_, hash_equal_) {
    if (vector_size)
      fp2id_.reserve(vector_size);
    if (entry_size)
      id2entry_.reserve(entry_size);
 }

  VectorHashBiTable(const VectorHashBiTable<I, T, S, FP, H, HS> &table)
      : selector_(new S(table.s_)),
        fp_(table.fp_ ? new FP(*table.fp_) : 0),
        h_(table.h_ ? new H(*table.h_) : 0),
        id2entry_(table.id2entry_),
        fp2id_(table.fp2id_),
        hash_func_(*this),
        hash_equal_(*this),
        keys_(table.keys_.size(), hash_func_, hash_equal_) {
     keys_.insert(table.keys_.begin(), table.keys_.end());
  }

  ~VectorHashBiTable() {
    delete selector_;
    delete fp_;
    delete h_;
  }

  I FindId(const T &entry, bool insert = true) {
    if ((*selector_)(entry)) {  // Use the vector if 'selector_(entry) == true'
      uint64 fp = (*fp_)(entry);
      if (fp2id_.size() <= fp)
        fp2id_.resize(fp + 1, 0);
      if (fp2id_[fp] == 0) {         // T not found
        if (insert) {                // store and assign it a new ID
          id2entry_.push_back(entry);
          fp2id_[fp] = id2entry_.size();
        } else {
          return -1;
        }
      }
      return fp2id_[fp] - 1;  // NB: assoc_value = ID + 1
    } else {  // Use the hash table otherwise.
      current_entry_ = &entry;
      typename KeyHashSet::const_iterator it = keys_.find(kCurrentKey);
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

  const T &FindEntry(I s) const {
    return id2entry_[s];
  }

  I Size() const { return id2entry_.size(); }

  const S &Selector() const { return *selector_; }

  const FP &Fingerprint() const { return *fp_; }

  const H &Hash() const { return *h_; }

 private:
  static const I kCurrentKey;  // -1
  static const I kEmptyKey;    // -2

  class HashFunc {
   public:
    HashFunc(const VectorHashBiTable &ht) : ht_(&ht) {}

    size_t operator()(I k) const {
      if (k >= kCurrentKey) {
        return (*(ht_->h_))(ht_->Key2Entry(k));
      } else {
        return 0;
      }
    }
   private:
    const VectorHashBiTable *ht_;
  };

  class HashEqual {
   public:
    HashEqual(const VectorHashBiTable &ht) : ht_(&ht) {}

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

  typedef HashSet<I, HashFunc, HashEqual, HS> KeyHashSet;

  const T &Key2Entry(I k) const {
    if (k == kCurrentKey)
      return *current_entry_;
    else
      return id2entry_[k];
  }

  S *selector_;  // Returns true if entry hashed into vector
  FP *fp_;       // Fingerprint used when hashing entry into vector
  H *h_;         // Hash function used when hashing entry into hash_set

  vector<T> id2entry_;  // Maps state IDs to entry
  vector<I> fp2id_;     // Maps entry fingerprints to IDs

  // Compact implementation of the hash table mapping entrys to
  // state IDs using the hash function 'h_'
  HashFunc hash_func_;
  HashEqual hash_equal_;
  KeyHashSet keys_;
  const T *current_entry_;

  // disallow
  void operator=(const VectorHashBiTable<I, T, S, FP, H, HS> &table);
};

template <class I, class T, class S, class FP, class H, HSType HS>
const I VectorHashBiTable<I, T, S, FP, H, HS>::kCurrentKey = -1;

template <class I, class T, class S, class FP, class H, HSType HS>
const I VectorHashBiTable<I, T, S, FP, H, HS>::kEmptyKey = -3;


// An implementation using a hash map for the entry to ID
// mapping. This version permits erasing of arbitrary states.  The
// entry T must have == defined and its default constructor must
// produce a entry that will never be seen. F is the hash function.
template <class I, class T, class F>
class ErasableBiTable {
 public:
  ErasableBiTable() : first_(0) {}

  I FindId(const T &entry, bool insert = true) {
    I &id_ref = entry2id_[entry];
    if (id_ref == 0) {  // T not found
      if (insert) {     // store and assign it a new ID
        id2entry_.push_back(entry);
        id_ref = id2entry_.size() + first_;
      } else {
        return -1;
      }
    }
    return id_ref - 1;  // NB: id_ref = ID + 1
  }

  const T &FindEntry(I s) const { return id2entry_[s - first_]; }

  I Size() const { return id2entry_.size(); }

  void Erase(I s) {
    T &entry = id2entry_[s - first_];
    typename unordered_map<T, I, F>::iterator it =
        entry2id_.find(entry);
    entry2id_.erase(it);
    id2entry_[s - first_] = empty_entry_;
    while (!id2entry_.empty() && id2entry_.front() == empty_entry_) {
      id2entry_.pop_front();
      ++first_;
    }
  }

 private:
  unordered_map<T, I, F> entry2id_;
  deque<T> id2entry_;
  const T empty_entry_;
  I first_;        // I of first element in the deque;

  // disallow
  void operator=(const ErasableBiTable<I, T, F> &table);  //disallow
};

}  // namespace fst

#endif  // FST_LIB_BI_TABLE_H__
