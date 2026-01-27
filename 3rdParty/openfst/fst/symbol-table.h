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
// Classes to provide symbol-to-integer and integer-to-symbol mappings.

#ifndef FST_SYMBOL_TABLE_H_
#define FST_SYMBOL_TABLE_H_

#include <cstdint>
#include <functional>
#include <ios>
#include <iostream>
#include <iterator>
#include <memory>
#include <sstream>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include <fst/compat.h>
#include <fst/flags.h>
#include <fst/log.h>
#include <fstream>
#include <fst/windows_defs.inc>
#include <map>
#include <functional>
#include <string_view>
#include <fst/lock.h>

DECLARE_bool(fst_compat_symbols);

namespace fst {

inline constexpr int64_t kNoSymbol = -1;

class SymbolTable;

struct SymbolTableTextOptions {
  explicit SymbolTableTextOptions(bool allow_negative_labels = false);

  bool allow_negative_labels;
  std::string fst_field_separator;
};

namespace internal {

// Maximum line length in textual symbols file.
inline constexpr int kLineLen = 8096;

// List of symbols with a dense hash for looking up symbol index, rehashing at
// 75% occupancy.
class DenseSymbolMap {
 public:
  DenseSymbolMap();

  std::pair<int64_t, bool> InsertOrFind(std::string_view key);

  int64_t Find(std::string_view key) const;

  size_t Size() const { return symbols_.size(); }

  const std::string &GetSymbol(size_t idx) const { return symbols_[idx]; }

  void RemoveSymbol(size_t idx);

  void ShrinkToFit();

 private:
  static constexpr int64_t kEmptyBucket = -1;

  // num_buckets must be power of 2.
  void Rehash(size_t num_buckets);

  size_t GetHash(std::string_view key) const {
    return str_hash_(key) & hash_mask_;
  }

  const std::hash<std::string_view> str_hash_;
  std::vector<std::string> symbols_;
  std::vector<int64_t> buckets_;
  uint64_t hash_mask_;
};

// Base class for SymbolTable implementations.
// Use either MutableSymbolTableImpl or ConstSymbolTableImpl to derive
// implementation classes.
class SymbolTableImplBase {
 public:
  SymbolTableImplBase() = default;
  virtual ~SymbolTableImplBase() = default;

  // Enforce copying through Copy().
  SymbolTableImplBase(const SymbolTableImplBase &) = delete;
  SymbolTableImplBase &operator=(const SymbolTableImplBase &) = delete;

  virtual std::unique_ptr<SymbolTableImplBase> Copy() const = 0;

  virtual bool Write(std::ostream &strm) const = 0;

  virtual int64_t AddSymbol(std::string_view symbol, int64_t key) = 0;

  virtual int64_t AddSymbol(std::string_view symbol) = 0;

  // Removes the symbol with the specified key. Subsequent Find() calls
  // for this key will return the empty string. Does not affect the keys
  // of other symbols.
  virtual void RemoveSymbol(int64_t key) = 0;

  // Returns the symbol for the specified key, or the empty string if not found.
  virtual std::string Find(int64_t key) const = 0;

  // Returns the key for the specified symbol, or kNoSymbol if not found.
  virtual int64_t Find(std::string_view symbol) const = 0;

  virtual bool Member(int64_t key) const { return !Find(key).empty(); }

  virtual bool Member(std::string_view symbol) const {
    return Find(symbol) != kNoSymbol;
  }

  virtual void AddTable(const SymbolTable &table) = 0;

  virtual int64_t GetNthKey(ssize_t pos) const = 0;

  virtual const std::string &Name() const = 0;

  virtual void SetName(std::string_view new_name) = 0;

  virtual const std::string &CheckSum() const = 0;

  virtual const std::string &LabeledCheckSum() const = 0;

  virtual int64_t AvailableKey() const = 0;

  virtual size_t NumSymbols() const = 0;

  virtual bool IsMutable() const = 0;
};

// Base class for SymbolTable implementations supporting Add/Remove.
class MutableSymbolTableImpl : public SymbolTableImplBase {
 public:
  void AddTable(const SymbolTable &table) override;
  bool IsMutable() const final { return true; }
};

// Base class for immutable SymbolTable implementations.
class ConstSymbolTableImpl : public SymbolTableImplBase {
 public:
  std::unique_ptr<SymbolTableImplBase> Copy() const final;

  int64_t AddSymbol(std::string_view symbol, int64_t key) final;

  int64_t AddSymbol(std::string_view symbol) final;

  void RemoveSymbol(int64_t key) final;

  void SetName(std::string_view new_name) final;
  void AddTable(const SymbolTable &table) final;

  bool IsMutable() const final { return false; }
};

// Default SymbolTable implementation using DenseSymbolMap and std::map.
// Provides the common text and binary format serialization.
class SymbolTableImpl final : public MutableSymbolTableImpl {
 public:
  explicit SymbolTableImpl(std::string_view name)
      : name_(name),
        available_key_(0),
        dense_key_limit_(0),
        check_sum_finalized_(false) {}

  SymbolTableImpl(const SymbolTableImpl &impl)
      : name_(impl.name_),
        available_key_(impl.available_key_),
        dense_key_limit_(impl.dense_key_limit_),
        symbols_(impl.symbols_),
        idx_key_(impl.idx_key_),
        key_map_(impl.key_map_),
        check_sum_finalized_(false) {}

  std::unique_ptr<SymbolTableImplBase> Copy() const override {
    return std::make_unique<SymbolTableImpl>(*this);
  }

  int64_t AddSymbol(std::string_view symbol, int64_t key) override;

  int64_t AddSymbol(std::string_view symbol) override {
    return AddSymbol(symbol, available_key_);
  }

  // Removes the symbol with the given key. The removal is costly
  // (O(NumSymbols)) and may reduce the efficiency of Find() because of a
  // potentially reduced size of the dense key interval.
  void RemoveSymbol(int64_t key) override;

  static SymbolTableImpl *ReadText(
      std::istream &strm, std::string_view name,
      const SymbolTableTextOptions &opts = SymbolTableTextOptions());

  // Reads a binary SymbolTable from stream, using source in error messages.
  static SymbolTableImpl *Read(std::istream &strm, std::string_view source);

  bool Write(std::ostream &strm) const override;

  // Returns the string associated with the key. If the key is out of
  // range (<0, >max), return an empty string.
  std::string Find(int64_t key) const override;

  // Returns the key associated with the symbol; if the symbol
  // does not exists, returns kNoSymbol.
  int64_t Find(std::string_view symbol) const override {
    int64_t idx = symbols_.Find(symbol);
    if (idx == kNoSymbol || idx < dense_key_limit_) return idx;
    return idx_key_[idx - dense_key_limit_];
  }

  int64_t GetNthKey(ssize_t pos) const override {
    if (pos < 0 || static_cast<size_t>(pos) >= symbols_.Size()) {
      return kNoSymbol;
    } else if (pos < dense_key_limit_) {
      return pos;
    }
    return Find(symbols_.GetSymbol(pos));
  }

  const std::string &Name() const override { return name_; }

  void SetName(std::string_view new_name) override {
    name_ = std::string(new_name);
  }

  const std::string &CheckSum() const override {
    MaybeRecomputeCheckSum();
    return check_sum_string_;
  }

  const std::string &LabeledCheckSum() const override {
    MaybeRecomputeCheckSum();
    return labeled_check_sum_string_;
  }

  int64_t AvailableKey() const override { return available_key_; }

  size_t NumSymbols() const override { return symbols_.Size(); }

  void ShrinkToFit();

 private:
  // Recomputes the checksums (both of them) if we've had changes since the last
  // computation (i.e., if check_sum_finalized_ is false).
  // Takes ~2.5 microseconds (dbg) or ~230 nanoseconds (opt) on a 2.67GHz Xeon
  // if the checksum is up-to-date (requiring no recomputation).
  void MaybeRecomputeCheckSum() const;

  std::string name_;
  int64_t available_key_;
  int64_t dense_key_limit_;

  DenseSymbolMap symbols_;
  // Maps index to key for index >= dense_key_limit:
  //   key = idx_key_[index - dense_key_limit]
  std::vector<int64_t> idx_key_;
  // Maps key to index for key >= dense_key_limit_.
  //  index = key_map_[key]
  std::map<int64_t, int64_t> key_map_;

  mutable bool check_sum_finalized_;
  mutable std::string check_sum_string_;
  mutable std::string labeled_check_sum_string_;
  mutable Mutex check_sum_mutex_;
};

}  // namespace internal

// Symbol (string) to integer (and reverse) mapping.
//
// The SymbolTable implements the mappings of labels to strings and reverse.
// SymbolTables are used to describe the alphabet of the input and output
// labels for arcs in a Finite State Transducer.
//
// SymbolTables are reference-counted and can therefore be shared across
// multiple machines. For example a language model grammar G, with a
// SymbolTable for the words in the language model can share this symbol
// table with the lexical representation L o G.
class SymbolTable {
 public:
  class iterator {
   public:
    // TODO(wolfsonkin): Expand `SymbolTable::iterator` to be a random access
    // iterator.
    using iterator_category = std::input_iterator_tag;

    class value_type {
     public:
      // Return the label of the current symbol.
      int64_t Label() const { return key_; }

      // Return the string of the current symbol.
      // TODO(wolfsonkin): Consider adding caching.
      std::string Symbol() const { return table_->Find(key_); }

     private:
      explicit value_type(const SymbolTable &table, ssize_t pos)
          : table_(&table), key_(table.GetNthKey(pos)) {}

      // Sets this item to the pos'th element in the symbol table
      void SetPosition(ssize_t pos) { key_ = table_->GetNthKey(pos); }

      friend class SymbolTable::iterator;

      const SymbolTable *table_;  // Does not own the underlying SymbolTable.
      int64_t key_;
    };

    using difference_type = std::ptrdiff_t;
    using pointer = const value_type *const;
    using reference = const value_type &;

    iterator &operator++() {
      ++pos_;
      if (static_cast<size_t>(pos_) < nsymbols_) iter_item_.SetPosition(pos_);
      return *this;
    }

    iterator operator++(int) {
      iterator retval = *this;
      ++(*this);
      return retval;
    }

    bool operator==(const iterator &that) const { return (pos_ == that.pos_); }

    bool operator!=(const iterator &that) const { return !(*this == that); }

    reference operator*() { return iter_item_; }

    pointer operator->() const { return &iter_item_; }

   private:
    explicit iterator(const SymbolTable &table, ssize_t pos = 0)
        : pos_(pos), nsymbols_(table.NumSymbols()), iter_item_(table, pos) {}

    friend class SymbolTable;

    ssize_t pos_;
    size_t nsymbols_;
    value_type iter_item_;
  };

  using const_iterator = iterator;

  // Constructs symbol table with an optional name.
  explicit SymbolTable(std::string_view name = "<unspecified>")
      : impl_(std::make_shared<internal::SymbolTableImpl>(name)) {}

  virtual ~SymbolTable() {}

  // Reads a text representation of the symbol table from an istream. Pass a
  // name to give the resulting SymbolTable.
  static SymbolTable *ReadText(
      std::istream &strm, std::string_view name,
      const SymbolTableTextOptions &opts = SymbolTableTextOptions()) {
    auto impl =
        fst::WrapUnique(internal::SymbolTableImpl::ReadText(strm, name, opts));
    return impl ? new SymbolTable(std::move(impl)) : nullptr;
  }

  // Reads a text representation of the symbol table.
  static SymbolTable *ReadText(
      const std::string &source,
      const SymbolTableTextOptions &opts = SymbolTableTextOptions());

  // Reads a binary dump of the symbol table from a stream.
  static SymbolTable *Read(std::istream &strm, const std::string &source) {
    auto impl = fst::WrapUnique(internal::SymbolTableImpl::Read(strm, source));
    return impl ? new SymbolTable(std::move(impl)) : nullptr;
  }

  // Reads a binary dump of the symbol table.
  static SymbolTable *Read(const std::string &source) {
    std::ifstream strm(source, std::ios_base::in | std::ios_base::binary);
    if (!strm.good()) {
      LOG(ERROR) << "SymbolTable::Read: Can't open file: " << source;
      return nullptr;
    }
    return Read(strm, source);
  }

  // Creates a reference counted copy.
  virtual SymbolTable *Copy() const { return new SymbolTable(*this); }

  // Adds another symbol table to this table. All keys will be offset by the
  // current available key (highest key in the symbol table). Note string
  // symbols with the same key will still have the same key after the symbol
  // table has been merged, but a different value. Adding symbol tables do not
  // result in changes in the base table.
  void AddTable(const SymbolTable &table) {
    MutateCheck();
    impl_->AddTable(table);
  }

  // Adds a symbol with given key to table. A symbol table also keeps track of
  // the last available key (highest key value in the symbol table).
  int64_t AddSymbol(std::string_view symbol, int64_t key) {
    MutateCheck();
    return impl_->AddSymbol(symbol, key);
  }

  // Adds a symbol to the table. The associated value key is automatically
  // assigned by the symbol table.
  int64_t AddSymbol(std::string_view symbol) {
    MutateCheck();
    return impl_->AddSymbol(symbol);
  }

  // Returns the current available key (i.e., highest key + 1) in the symbol
  // table.
  int64_t AvailableKey() const { return impl_->AvailableKey(); }

  // Return the label-agnostic MD5 check-sum for this table. All new symbols
  // added to the table will result in an updated checksum.
  OPENFST_DEPRECATED("Use `LabeledCheckSum()` instead.")
  const std::string &CheckSum() const { return impl_->CheckSum(); }

  int64_t GetNthKey(ssize_t pos) const { return impl_->GetNthKey(pos); }

  // Returns the string associated with the key; if the key is out of
  // range (<0, >max), returns an empty string.
  std::string Find(int64_t key) const { return impl_->Find(key); }

  // Returns the key associated with the symbol; if the symbol does not exist,
  // kNoSymbol is returned.
  int64_t Find(std::string_view symbol) const { return impl_->Find(symbol); }

  // Same as CheckSum(), but returns an label-dependent version.
  const std::string &LabeledCheckSum() const {
    return impl_->LabeledCheckSum();
  }

  bool Member(int64_t key) const { return impl_->Member(key); }

  bool Member(std::string_view symbol) const { return impl_->Member(symbol); }

  // Returns the name of the symbol table.
  const std::string &Name() const { return impl_->Name(); }

  // Returns the current number of symbols in table (not necessarily equal to
  // AvailableKey()).
  size_t NumSymbols() const { return impl_->NumSymbols(); }

  void RemoveSymbol(int64_t key) {
    MutateCheck();
    return impl_->RemoveSymbol(key);
  }

  // Sets the name of the symbol table.
  void SetName(std::string_view new_name) {
    MutateCheck();
    impl_->SetName(new_name);
  }

  bool Write(std::ostream &strm) const { return impl_->Write(strm); }

  bool Write(const std::string &source) const;

  // Dumps a text representation of the symbol table via a stream.
  bool WriteText(std::ostream &strm, const SymbolTableTextOptions &opts =
                                         SymbolTableTextOptions()) const;

  // Dumps a text representation of the symbol table.
  bool WriteText(const std::string &source) const;

  const_iterator begin() const { return const_iterator(*this, 0); }

  const_iterator end() const { return const_iterator(*this, NumSymbols()); }

  const_iterator cbegin() const { return begin(); }

  const_iterator cend() const { return end(); }

 protected:
  explicit SymbolTable(std::shared_ptr<internal::SymbolTableImplBase> impl)
      : impl_(std::move(impl)) {}

  template <class T = internal::SymbolTableImplBase>
  const T *Impl() const {
    return down_cast<const T *>(impl_.get());
  }

  template <class T = internal::SymbolTableImplBase>
  T *MutableImpl() {
    MutateCheck();
    return down_cast<T *>(impl_.get());
  }

 private:
  void MutateCheck() {
    if (impl_.use_count() == 1 || !impl_->IsMutable()) return;
    std::unique_ptr<internal::SymbolTableImplBase> copy = impl_->Copy();
    CHECK(copy != nullptr);
    impl_ = std::move(copy);
  }

  std::shared_ptr<internal::SymbolTableImplBase> impl_;
};

// Iterator class for symbols in a symbol table.
class OPENFST_DEPRECATED(
    "Use SymbolTable::iterator, a C++ compliant iterator, instead")
    SymbolTableIterator {
 public:
  explicit SymbolTableIterator(const SymbolTable &table)
      : table_(table), iter_(table.begin()), end_(table.end()) {}

  ~SymbolTableIterator() {}

  // Returns whether iterator is done.
  bool Done() const { return (iter_ == end_); }

  // Return the key of the current symbol.
  int64_t Value() const { return iter_->Label(); }

  // Return the string of the current symbol.
  std::string Symbol() const { return iter_->Symbol(); }

  // Advances iterator.
  void Next() { ++iter_; }

  // Resets iterator.
  void Reset() { iter_ = table_.begin(); }

 private:
  const SymbolTable &table_;
  SymbolTable::iterator iter_;
  const SymbolTable::iterator end_;
};

// Relabels a symbol table as specified by the input vector of pairs
// (old label, new label). The new symbol table only retains symbols
// for which a relabeling is explicitly specified.
//
// TODO(allauzen): consider adding options to allow for some form of implicit
// identity relabeling.
template <class Label>
SymbolTable *RelabelSymbolTable(
    const SymbolTable *table,
    const std::vector<std::pair<Label, Label>> &pairs) {
  auto *new_table = new SymbolTable(
      table->Name().empty() ? std::string()
                            : (std::string("relabeled_") + table->Name()));
  for (const auto &[old_label, new_label] : pairs) {
    new_table->AddSymbol(table->Find(old_label), new_label);
  }
  return new_table;
}

// Returns true if the two symbol tables have equal checksums. Passing in
// nullptr for either table always returns true.
bool CompatSymbols(const SymbolTable *syms1, const SymbolTable *syms2,
                   bool warning = true);

// Symbol table serialization.

void SymbolTableToString(const SymbolTable *table, std::string *result);

SymbolTable *StringToSymbolTable(const std::string &str);

}  // namespace fst

#endif  // FST_SYMBOL_TABLE_H_
