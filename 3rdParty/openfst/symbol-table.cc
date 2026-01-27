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

#include <fst/symbol-table.h>

#include <cstdint>

#include <fst/flags.h>
#include <fst/log.h>

#include <fstream>
#include <fst/util.h>
#include <string_view>
#include <fst/lock.h>

DEFINE_bool(fst_compat_symbols, true,
            "Require symbol tables to match when appropriate");
DEFINE_string(fst_field_separator, "\t ",
              "Set of characters used as a separator between printed fields");

namespace fst {

SymbolTableTextOptions::SymbolTableTextOptions(bool allow_negative_labels)
    : allow_negative_labels(allow_negative_labels),
      fst_field_separator(FST_FLAGS_fst_field_separator) {}

namespace internal {

// Identifies stream data as a symbol table (and its endianity).
static constexpr int32_t kSymbolTableMagicNumber = 2125658996;

DenseSymbolMap::DenseSymbolMap()
    : str_hash_(),
      buckets_(1 << 4, kEmptyBucket),
      hash_mask_(buckets_.size() - 1) {}

std::pair<int64_t, bool> DenseSymbolMap::InsertOrFind(std::string_view key) {
  static constexpr float kMaxOccupancyRatio = 0.75;  // Grows when 75% full.
  if (Size() >= kMaxOccupancyRatio * buckets_.size()) {
    Rehash(buckets_.size() * 2);
  }
  size_t idx = GetHash(key);
  while (buckets_[idx] != kEmptyBucket) {
    const auto stored_value = buckets_[idx];
    if (symbols_[stored_value] == key) return {stored_value, false};
    idx = (idx + 1) & hash_mask_;
  }
  const auto next = Size();
  buckets_[idx] = next;
  symbols_.emplace_back(key);
  return {next, true};
}

int64_t DenseSymbolMap::Find(std::string_view key) const {
  size_t idx = str_hash_(key) & hash_mask_;
  while (buckets_[idx] != kEmptyBucket) {
    const auto stored_value = buckets_[idx];
    if (symbols_[stored_value] == key) return stored_value;
    idx = (idx + 1) & hash_mask_;
  }
  return buckets_[idx];
}

void DenseSymbolMap::Rehash(size_t num_buckets) {
  buckets_.resize(num_buckets);
  hash_mask_ = buckets_.size() - 1;
  std::fill(buckets_.begin(), buckets_.end(), kEmptyBucket);
  for (size_t i = 0; i < Size(); ++i) {
    size_t idx = GetHash(symbols_[i]);
    while (buckets_[idx] != kEmptyBucket) {
      idx = (idx + 1) & hash_mask_;
    }
    buckets_[idx] = i;
  }
}

void DenseSymbolMap::RemoveSymbol(size_t idx) {
  symbols_.erase(symbols_.begin() + idx);
  Rehash(buckets_.size());
}

void DenseSymbolMap::ShrinkToFit() { symbols_.shrink_to_fit(); }

void MutableSymbolTableImpl::AddTable(const SymbolTable &table) {
  for (const auto &item : table) {
    AddSymbol(item.Symbol());
  }
}

std::unique_ptr<SymbolTableImplBase> ConstSymbolTableImpl::Copy() const {
  LOG(FATAL) << "ConstSymbolTableImpl can't be copied";
  return nullptr;
}

int64_t ConstSymbolTableImpl::AddSymbol(std::string_view symbol, int64_t key) {
  LOG(FATAL) << "ConstSymbolTableImpl does not support AddSymbol";
  return kNoSymbol;
}

int64_t ConstSymbolTableImpl::AddSymbol(std::string_view symbol) {
  return AddSymbol(symbol, kNoSymbol);
}

void ConstSymbolTableImpl::RemoveSymbol(int64_t key) {
  LOG(FATAL) << "ConstSymbolTableImpl does not support RemoveSymbol";
}

void ConstSymbolTableImpl::SetName(std::string_view new_name) {
  LOG(FATAL) << "ConstSymbolTableImpl does not support SetName";
}

void ConstSymbolTableImpl::AddTable(const SymbolTable &table) {
  LOG(FATAL) << "ConstSymbolTableImpl does not support AddTable";
}

SymbolTableImpl *SymbolTableImpl::ReadText(std::istream &strm,
                                           std::string_view name,
                                           const SymbolTableTextOptions &opts) {
  auto impl = std::make_unique<SymbolTableImpl>(name);
  int64_t nline = 0;
  char line[kLineLen];
  const auto separator = opts.fst_field_separator + "\n";
  while (!strm.getline(line, kLineLen).fail()) {
    ++nline;
    std::vector<std::string_view> col =
        StrSplit(line, ByAnyChar(separator), SkipEmpty());
    if (col.empty()) continue;  // Empty line.
    if (col.size() != 2) {
      LOG(ERROR) << "SymbolTable::ReadText: Bad number of columns ("
                 << col.size() << "), "
                 << "file = " << name << ", line = " << nline << ":<" << line
                 << ">";
      return nullptr;
    }
    std::string_view symbol = col[0];
    std::string_view value = col[1];
    const auto maybe_key = ParseInt64(value);
    if (!maybe_key.has_value() ||
        (!opts.allow_negative_labels && *maybe_key < 0) ||
        *maybe_key == kNoSymbol) {
      LOG(ERROR) << "SymbolTable::ReadText: Bad non-negative integer \""
                 << value << "\", "
                 << "file = " << name << ", line = " << nline;
      return nullptr;
    }
    impl->AddSymbol(symbol, *maybe_key);
  }
  impl->ShrinkToFit();
  return impl.release();
}

void SymbolTableImpl::MaybeRecomputeCheckSum() const {
  {
    ReaderMutexLock check_sum_lock(&check_sum_mutex_);
    if (check_sum_finalized_) return;
  }
  // We'll acquire an exclusive lock to recompute the checksums.
  MutexLock check_sum_lock(&check_sum_mutex_);
  if (check_sum_finalized_) {  // Another thread (coming in around the same time
    return;                    // might have done it already). So we recheck.
  }
  // Calculates the original label-agnostic checksum.
  CheckSummer check_sum;
  for (size_t i = 0; i < symbols_.Size(); ++i) {
    check_sum.Update(symbols_.GetSymbol(i));
    check_sum.Update(std::string_view{"\0", 1});
  }
  check_sum_string_ = check_sum.Digest();
  // Calculates the safer, label-dependent checksum.
  CheckSummer labeled_check_sum;
  for (int64_t i = 0; i < dense_key_limit_; ++i) {
    std::ostringstream line;
    line << symbols_.GetSymbol(i) << '\t' << i;
    labeled_check_sum.Update(line.str());
  }
  using citer = std::map<int64_t, int64_t>::const_iterator;
  for (citer it = key_map_.begin(); it != key_map_.end(); ++it) {
    // TODO(tombagby, 2013-11-22) This line maintains a bug that ignores
    // negative labels in the checksum that too many tests rely on.
    if (it->first < dense_key_limit_) continue;
    std::ostringstream line;
    line << symbols_.GetSymbol(it->second) << '\t' << it->first;
    labeled_check_sum.Update(line.str());
  }
  labeled_check_sum_string_ = labeled_check_sum.Digest();
  check_sum_finalized_ = true;
}

std::string SymbolTableImpl::Find(int64_t key) const {
  int64_t idx = key;
  if (key < 0 || key >= dense_key_limit_) {
    const auto it = key_map_.find(key);
    if (it == key_map_.end()) return "";
    idx = it->second;
  }
  if (idx < 0 || idx >= symbols_.Size()) return "";
  return symbols_.GetSymbol(idx);
}

int64_t SymbolTableImpl::AddSymbol(std::string_view symbol, int64_t key) {
  if (key == kNoSymbol) return key;
  if (const auto &[insert_key, inserted] = symbols_.InsertOrFind(symbol);
      !inserted) {
    const auto key_already = GetNthKey(insert_key);
    if (key_already == key) return key;
    VLOG(1) << "SymbolTable::AddSymbol: symbol = " << symbol
            << " already in symbol_map_ with key = " << key_already
            << " but supplied new key = " << key << " (ignoring new key)";
    return key_already;
  }
  if (key + 1 == static_cast<int64_t>(symbols_.Size()) &&
      key == dense_key_limit_) {
    ++dense_key_limit_;
  } else {
    idx_key_.push_back(key);
    key_map_[key] = symbols_.Size() - 1;
  }
  if (key >= available_key_) available_key_ = key + 1;
  check_sum_finalized_ = false;
  return key;
}

// TODO(rybach): Consider a more efficient implementation which re-uses holes in
// the dense-key range or re-arranges the dense-key range from time to time.
void SymbolTableImpl::RemoveSymbol(const int64_t key) {
  auto idx = key;
  if (key < 0 || key >= dense_key_limit_) {
    auto iter = key_map_.find(key);
    if (iter == key_map_.end()) return;
    idx = iter->second;
    key_map_.erase(iter);
  }
  if (idx < 0 || idx >= static_cast<int64_t>(symbols_.Size())) return;
  symbols_.RemoveSymbol(idx);
  // Removed one symbol, all indexes > idx are shifted by -1.
  for (auto &k : key_map_) {
    if (k.second > idx) --k.second;
  }
  if (key >= 0 && key < dense_key_limit_) {
    // Removal puts a hole in the dense key range. Adjusts range to [0, key).
    const auto new_dense_key_limit = key;
    for (int64_t i = key + 1; i < dense_key_limit_; ++i) {
      key_map_[i] = i - 1;
    }
    // Moves existing values in idx_key to new place.
    idx_key_.resize(symbols_.Size() - new_dense_key_limit);
    for (int64_t i = symbols_.Size(); i >= dense_key_limit_; --i) {
      idx_key_[i - new_dense_key_limit - 1] = idx_key_[i - dense_key_limit_];
    }
    // Adds indexes for previously dense keys.
    for (int64_t i = new_dense_key_limit; i < dense_key_limit_ - 1; ++i) {
      idx_key_[i - new_dense_key_limit] = i + 1;
    }
    dense_key_limit_ = new_dense_key_limit;
  } else {
    // Remove entry for removed index in idx_key.
    for (size_t i = idx - dense_key_limit_; i + 1 < idx_key_.size(); ++i) {
      idx_key_[i] = idx_key_[i + 1];
    }
    idx_key_.pop_back();
  }
  if (key == available_key_ - 1) available_key_ = key;
}

SymbolTableImpl *SymbolTableImpl::Read(std::istream &strm,
                                       std::string_view source) {
  int32_t magic_number = 0;
  ReadType(strm, &magic_number);
  if (strm.fail()) {
    LOG(ERROR) << "SymbolTable::Read: Read failed: " << source;
    return nullptr;
  }
  std::string name;
  ReadType(strm, &name);
  auto impl = std::make_unique<SymbolTableImpl>(name);
  ReadType(strm, &impl->available_key_);
  int64_t size;
  ReadType(strm, &size);
  if (strm.fail()) {
    LOG(ERROR) << "SymbolTable::Read: Read failed: " << source;
    return nullptr;
  }
  std::string symbol;
  int64_t key;
  impl->check_sum_finalized_ = false;
  for (int64_t i = 0; i < size; ++i) {
    ReadType(strm, &symbol);
    ReadType(strm, &key);
    if (strm.fail()) {
      LOG(ERROR) << "SymbolTable::Read: Read failed: " << source;
      return nullptr;
    }
    impl->AddSymbol(symbol, key);
  }
  impl->ShrinkToFit();
  return impl.release();
}

bool SymbolTableImpl::Write(std::ostream &strm) const {
  WriteType(strm, kSymbolTableMagicNumber);
  WriteType(strm, name_);
  WriteType(strm, available_key_);
  const int64_t size = symbols_.Size();
  WriteType(strm, size);
  for (int64_t i = 0; i < dense_key_limit_; ++i) {
    WriteType(strm, symbols_.GetSymbol(i));
    WriteType(strm, i);
  }
  for (const auto &p : key_map_) {
    WriteType(strm, symbols_.GetSymbol(p.second));
    WriteType(strm, p.first);
  }
  strm.flush();
  if (strm.fail()) {
    LOG(ERROR) << "SymbolTable::Write: Write failed";
    return false;
  }
  return true;
}

void SymbolTableImpl::ShrinkToFit() { symbols_.ShrinkToFit(); }

}  // namespace internal

SymbolTable *SymbolTable::ReadText(const std::string &source,
                                   const SymbolTableTextOptions &opts) {
  std::ifstream strm(source, std::ios_base::in);
  if (!strm.good()) {
    LOG(ERROR) << "SymbolTable::ReadText: Can't open file: " << source;
    return nullptr;
  }
  return ReadText(strm, source, opts);
}

bool SymbolTable::Write(const std::string &source) const {
  if (!source.empty()) {
    std::ofstream strm(source,
                             std::ios_base::out | std::ios_base::binary);
    if (!strm) {
      LOG(ERROR) << "SymbolTable::Write: Can't open file: " << source;
      return false;
    }
    if (!Write(strm)) {
      LOG(ERROR) << "SymbolTable::Write: Write failed: " << source;
      return false;
    }
    return true;
  } else {
    return Write(std::cout);
  }
}

bool SymbolTable::WriteText(std::ostream &strm,
                            const SymbolTableTextOptions &opts) const {
  if (opts.fst_field_separator.empty()) {
    LOG(ERROR) << "Missing required field separator";
    return false;
  }
  bool once_only = false;
  for (const auto &item : *this) {
    std::ostringstream line;
    if (item.Label() < 0 && !opts.allow_negative_labels && !once_only) {
      LOG(WARNING) << "Negative symbol table entry when not allowed";
      once_only = true;
    }
    line << item.Symbol() << opts.fst_field_separator[0] << item.Label()
         << '\n';
    strm.write(line.str().data(), line.str().length());
  }
  return true;
}

bool SymbolTable::WriteText(const std::string &source) const {
  if (!source.empty()) {
    std::ofstream strm(source);
    if (!strm) {
      LOG(ERROR) << "SymbolTable::WriteText: Can't open file: " << source;
      return false;
    }
    if (!WriteText(strm, SymbolTableTextOptions())) {
      LOG(ERROR) << "SymbolTable::WriteText: Write failed: " << source;
      return false;
    }
    return true;
  } else {
    return WriteText(std::cout, SymbolTableTextOptions());
  }
}

bool CompatSymbols(const SymbolTable *syms1, const SymbolTable *syms2,
                   bool warning) {
  // Flag can explicitly override this check.
  if (!FST_FLAGS_fst_compat_symbols) return true;
  if (syms1 && syms2 &&
      (syms1->LabeledCheckSum() != syms2->LabeledCheckSum())) {
    if (warning) {
      LOG(WARNING) << "CompatSymbols: Symbol table checksums do not match. "
                   << "Table sizes are " << syms1->NumSymbols() << " and "
                   << syms2->NumSymbols();
    }
    return false;
  } else {
    return true;
  }
}

void SymbolTableToString(const SymbolTable *table, std::string *result) {
  std::ostringstream ostrm;
  table->Write(ostrm);
  *result = ostrm.str();
}

SymbolTable *StringToSymbolTable(const std::string &str) {
  std::istringstream istrm(str);
  // TODO(jrosenstock): Change to source="string".
  return SymbolTable::Read(istrm, /*source=*/"");
}

}  // namespace fst
