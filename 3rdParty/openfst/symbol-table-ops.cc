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

#include <fst/symbol-table-ops.h>

#include <cstdint>
#include <string>

namespace fst {

SymbolTable *MergeSymbolTable(const SymbolTable &left, const SymbolTable &right,
                              bool *right_relabel_output) {
  // MergeSymbolTable detects several special cases  It will return a reference
  // copied version of SymbolTable of left or right if either symbol table is
  // a superset of the other.
  std::unique_ptr<SymbolTable> merged(
      new SymbolTable("merge_" + left.Name() + "_" + right.Name()));
  // Copies everything from the left symbol table.
  bool left_has_all = true;
  bool right_has_all = true;
  bool relabel = false;
  for (const auto &litem : left) {
    merged->AddSymbol(litem.Symbol(), litem.Label());
    if (right_has_all) {
      int64_t key = right.Find(litem.Symbol());
      if (key == kNoSymbol) {
        right_has_all = false;
      } else if (!relabel && key != litem.Label()) {
        relabel = true;
      }
    }
  }
  if (right_has_all) {
    if (right_relabel_output) *right_relabel_output = relabel;
    return right.Copy();
  }
  // Adds all symbols we can from right symbol table.
  std::vector<std::string> conflicts;
  for (const auto &ritem : right) {
    int64_t key = merged->Find(ritem.Symbol());
    if (key != -1) {
      // Symbol already exists, maybe with different value.
      if (key != ritem.Label()) relabel = true;
      continue;
    }
    // Symbol doesn't exist from left.
    left_has_all = false;
    if (!merged->Find(ritem.Label()).empty()) {
      // We can't add this where we want to, add it later, in order.
      conflicts.push_back(ritem.Symbol());
      continue;
    }
    // There is a hole and we can add this symbol with its ID.
    merged->AddSymbol(ritem.Symbol(), ritem.Label());
  }
  if (right_relabel_output) *right_relabel_output = relabel;
  if (left_has_all) return left.Copy();
  // Adds all symbols that conflicted, in order.
  for (const auto &conflict : conflicts) merged->AddSymbol(conflict);
  return merged.release();
}

SymbolTable *CompactSymbolTable(const SymbolTable &syms) {
  std::map<int64_t, std::string> sorted;
  for (const auto &stitem : syms) {
    sorted[stitem.Label()] = stitem.Symbol();
  }
  auto *compact = new SymbolTable(syms.Name() + "_compact");
  int64_t newkey = 0;
  for (const auto &kv : sorted) compact->AddSymbol(kv.second, newkey++);
  return compact;
}

SymbolTable *FstReadSymbols(const std::string &source, bool input_symbols) {
  std::ifstream in(source, std::ios_base::in | std::ios_base::binary);
  if (!in) {
    LOG(ERROR) << "FstReadSymbols: Can't open file " << source;
    return nullptr;
  }
  FstHeader hdr;
  if (!hdr.Read(in, source)) {
    LOG(ERROR) << "FstReadSymbols: Couldn't read header from " << source;
    return nullptr;
  }
  if (hdr.GetFlags() & FstHeader::HAS_ISYMBOLS) {
    std::unique_ptr<SymbolTable> isymbols(SymbolTable::Read(in, source));
    if (isymbols == nullptr) {
      LOG(ERROR) << "FstReadSymbols: Couldn't read input symbols from "
                 << source;
      return nullptr;
    }
    if (input_symbols) return isymbols.release();
  }
  if (hdr.GetFlags() & FstHeader::HAS_OSYMBOLS) {
    std::unique_ptr<SymbolTable> osymbols(SymbolTable::Read(in, source));
    if (osymbols == nullptr) {
      LOG(ERROR) << "FstReadSymbols: Couldn't read output symbols from "
                 << source;
      return nullptr;
    }
    if (!input_symbols) return osymbols.release();
  }
  LOG(ERROR) << "FstReadSymbols: The file " << source
             << " doesn't contain the requested symbols";
  return nullptr;
}

bool AddAuxiliarySymbols(const std::string &prefix, int64_t start_label,
                         int64_t nlabels, SymbolTable *syms) {
  for (int64_t i = 0; i < nlabels; ++i) {
    auto index = i + start_label;
    if (index != syms->AddSymbol(prefix + std::to_string(i), index)) {
      FSTERROR() << "AddAuxiliarySymbols: Symbol table clash";
      return false;
    }
  }
  return true;
}

}  // namespace fst
