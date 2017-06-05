
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
// All Rights Reserved.
//
// Author : Johan Schalkwyk
//
// \file
// Classes to provide symbol-to-integer and integer-to-symbol mappings.

#include <fst/symbol-table.h>

#include <fst/util.h>

DEFINE_bool(fst_compat_symbols, true,
            "Require symbol tables to match when appropriate");
DEFINE_string(fst_field_separator, "\t ",
              "Set of characters used as a separator between printed fields");

namespace fst {

// Maximum line length in textual symbols file.
const int kLineLen = 8096;

// Identifies stream data as a symbol table (and its endianity)
static const int32 kSymbolTableMagicNumber = 2125658996;

SymbolTableTextOptions::SymbolTableTextOptions()
    : allow_negative(false), fst_field_separator(FLAGS_fst_field_separator) { }

SymbolTableImpl* SymbolTableImpl::ReadText(istream &strm,
                                           const string &filename,
                                           const SymbolTableTextOptions &opts) {
  SymbolTableImpl* impl = new SymbolTableImpl(filename);

  int64 nline = 0;
  char line[kLineLen];
  while (!strm.getline(line, kLineLen).fail()) {
    ++nline;
    vector<char *> col;
    string separator = opts.fst_field_separator + "\n";
    SplitToVector(line, separator.c_str(), &col, true);
    if (col.size() == 0)  // empty line
      continue;
    if (col.size() != 2) {
      LOG(ERROR) << "SymbolTable::ReadText: Bad number of columns ("
                 << col.size() << "), "
                 << "file = " << filename << ", line = " << nline
                 << ":<" << line << ">";
      delete impl;
      return 0;
    }
    const char *symbol = col[0];
    const char *value = col[1];
    char *p;
    int64 key = strtoll(value, &p, 10);
    if (p < value + strlen(value) ||
        (!opts.allow_negative && key < 0) || key == -1) {
      LOG(ERROR) << "SymbolTable::ReadText: Bad non-negative integer \""
                 << value << "\", "
                 << "file = " << filename << ", line = " << nline;
      delete impl;
      return 0;
    }
    impl->AddSymbol(symbol, key);
  }

  return impl;
}

void SymbolTableImpl::MaybeRecomputeCheckSum() const {
  {
    ReaderMutexLock check_sum_lock(&check_sum_mutex_);
    if (check_sum_finalized_)
      return;
  }

  // We'll aquire an exclusive lock to recompute the checksums.
  MutexLock check_sum_lock(&check_sum_mutex_);
  if (check_sum_finalized_)  // Another thread (coming in around the same time
    return;                  // might have done it already).  So we recheck.

  // Calculate the original label-agnostic check sum.
  CheckSummer check_sum;
  for (int64 i = 0; i < symbols_.size(); ++i) {
    const string& sym = symbols_.GetSymbol(i);
    check_sum.Update(sym.data(), sym.size());
    check_sum.Update("", 1);
  }
  check_sum_string_ = check_sum.Digest();

  // Calculate the safer, label-dependent check sum.
  CheckSummer labeled_check_sum;
  for (int64 i = 0; i < dense_key_limit_; i++) {
    ostringstream line;
    line << symbols_.GetSymbol(i) << '\t' << i;
    labeled_check_sum.Update(line.str().data(), line.str().size());
  }
  for (map<int64, int64>::const_iterator i
         = key_map_.begin(); i != key_map_.end(); ++i) {
    // TODO(tombagby, 2013-11-22) This line maintains a bug that ignores
    // negative labels in the checksum that too many tests rely on.
    if (i->first < dense_key_limit_) continue;

    ostringstream line;
    line << symbols_.GetSymbol(i->second) << '\t' << i->first;
    labeled_check_sum.Update(line.str().data(), line.str().size());
  }
  labeled_check_sum_string_ = labeled_check_sum.Digest();

  check_sum_finalized_ = true;
}

int64 SymbolTableImpl::AddSymbol(const string& symbol, int64 key) {
  if (key == -1) return key;
  const pair<int64, bool>& insert_key = symbols_.InsertOrFind(symbol);
  if (!insert_key.second) {
    int64 key_already = GetNthKey(insert_key.first);
    if (key_already == key) return key;
    VLOG(1) << "SymbolTable::AddSymbol: symbol = " << symbol
            << " already in symbol_map_ with key = " << key_already
            << " but supplied new key = " << key << " (ignoring new key)";
    return key_already;
  }
  if (key == (symbols_.size() - 1)  && key == dense_key_limit_) {
    dense_key_limit_++;
  } else {
    idx_key_.push_back(key);
    key_map_[key] = symbols_.size() - 1;
  }
  if (key >= available_key_) {
    available_key_ = key + 1;
  }
  check_sum_finalized_ = false;
  return key;
}

SymbolTableImpl* SymbolTableImpl::Read(istream &strm,
                                       const SymbolTableReadOptions& opts) {
  int32 magic_number = 0;
  ReadType(strm, &magic_number);
  if (strm.fail()) {
    LOG(ERROR) << "SymbolTable::Read: read failed";
    return 0;
  }
  string name;
  ReadType(strm, &name);
  SymbolTableImpl* impl = new SymbolTableImpl(name);
  ReadType(strm, &impl->available_key_);
  int64 size;
  ReadType(strm, &size);
  if (strm.fail()) {
    LOG(ERROR) << "SymbolTable::Read: read failed";
    delete impl;
    return 0;
  }

  string symbol;
  int64 key;
  impl->check_sum_finalized_ = false;
  for (size_t i = 0; i < size; ++i) {
    ReadType(strm, &symbol);
    ReadType(strm, &key);
    if (strm.fail()) {
      LOG(ERROR) << "SymbolTable::Read: read failed";
      delete impl;
      return 0;
    }
    impl->AddSymbol(symbol, key);
  }
  return impl;
}

bool SymbolTableImpl::Write(ostream &strm) const {
  WriteType(strm, kSymbolTableMagicNumber);
  WriteType(strm, name_);
  WriteType(strm, available_key_);
  int64 size = symbols_.size();
  WriteType(strm, size);
  for (int64 i = 0; i < symbols_.size(); ++i) {
    int64 key = (i < dense_key_limit_) ? i : idx_key_[i - dense_key_limit_];
    WriteType(strm, symbols_.GetSymbol(i));
    WriteType(strm, key);
  }
  strm.flush();
  if (strm.fail()) {
    LOG(ERROR) << "SymbolTable::Write: write failed";
    return false;
  }
  return true;
}

const int64 SymbolTable::kNoSymbol;

void SymbolTable::AddTable(const SymbolTable& table) {
  MutateCheck();
  for (SymbolTableIterator iter(table); !iter.Done(); iter.Next())
    impl_->AddSymbol(iter.Symbol());
}

bool SymbolTable::WriteText(ostream &strm,
                            const SymbolTableTextOptions &opts) const {
  if (opts.fst_field_separator.empty()) {
    LOG(ERROR) << "Missing required field separator";
    return false;
  }
  bool once_only = false;
  for (SymbolTableIterator iter(*this); !iter.Done(); iter.Next()) {
    ostringstream line;
    if (iter.Value() < 0 && !opts.allow_negative && !once_only) {
      LOG(WARNING) << "Negative symbol table entry when not allowed";
      once_only = true;
    }
    line << iter.Symbol() << opts.fst_field_separator[0] << iter.Value()
         << '\n';
    strm.write(line.str().data(), line.str().length());
  }
  return true;
}

namespace internal {

DenseSymbolMap::DenseSymbolMap()
    : empty_(-1),
      buckets_(1 << 4),
      hash_mask_(buckets_.size() - 1) {
  std::uninitialized_fill(buckets_.begin(), buckets_.end(), empty_);
}

DenseSymbolMap::DenseSymbolMap(const DenseSymbolMap& x)
    : empty_(-1),
      symbols_(x.symbols_.size()),
      buckets_(x.buckets_),
      hash_mask_(x.hash_mask_),
      size_(x.size_) {
  for (int i = 0; i < symbols_.size(); i++) {
    size_t sz = strlen(x.symbols_[i]) + 1;
    char *cpy = new char[sz];
    memcpy(cpy, x.symbols_[i], sz);
    symbols_[i] = cpy;
  }
}

DenseSymbolMap::~DenseSymbolMap() {
  for (int i = 0; i < symbols_.size(); i++) {
    delete[] symbols_[i];
  }
}

pair<int64, bool> DenseSymbolMap::InsertOrFind(const string& key) {
  static const float kMaxOccupancyRatio = 0.75;  // Grow when 75% full.
  if (symbols_.size() >= kMaxOccupancyRatio * buckets_.size()) Rehash();
  size_t idx = str_hash_(key) & hash_mask_;
  while (buckets_[idx] != empty_) {
    const int64 stored_value = buckets_[idx];
    if (!strcmp(symbols_[stored_value], key.c_str())) {
      return std::make_pair(stored_value, false);
    }
    idx = (idx + 1) & hash_mask_;
  }
  int64 next = symbols_.size();
  buckets_[idx] = next;
  symbols_.push_back(NewSymbol(key));
  return std::make_pair(next, true);
}

int64 DenseSymbolMap::Find(const string& key) const {
  size_t idx = str_hash_(key) & hash_mask_;
  while (buckets_[idx] != empty_) {
    const int64 stored_value = buckets_[idx];
    if (!strcmp(symbols_[stored_value], key.c_str())) {
      return stored_value;
    }
    idx = (idx + 1) & hash_mask_;
  }
  return buckets_[idx];
}

void DenseSymbolMap::Rehash() {
  size_t sz = buckets_.size();
  buckets_.resize(2 * sz);
  hash_mask_ = buckets_.size() - 1;
  std::uninitialized_fill(buckets_.begin(), buckets_.end(), empty_);
  for (int i = 0; i < symbols_.size(); i++) {
    size_t idx = str_hash_(string(symbols_[i])) & hash_mask_;
    while (buckets_[idx] != empty_) {
      idx = (idx + 1) & hash_mask_;
    }
    buckets_[idx] = i;
  }
}

const char* DenseSymbolMap::NewSymbol(const string& sym) {
  size_t num = sym.size() + 1;
  char *newstr = new char[num];
  memcpy(newstr, sym.c_str(), num);
  return newstr;
}

}  // namespace internal
}  // namespace fst
