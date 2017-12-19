
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
// Author: sorenj@google.com (Jeffrey Sorensen)

#include <fst/symbol-table-ops.h>

namespace fst {

SymbolTable *MergeSymbolTable(const SymbolTable &left, const SymbolTable &right,
                              bool *right_relabel_output) {
  // MergeSymbolTable detects several special cases.  It will return a reference
  // copied version of SymbolTable of left or right if either symbol table is
  // a superset of the other.
  SymbolTable *merged = new SymbolTable("merge_" + left.Name() + "_" +
                                        right.Name());
  // copy everything from the left symbol table
  bool left_has_all = true, right_has_all = true, relabel = false;
  SymbolTableIterator liter(left);
  for (; !liter.Done(); liter.Next()) {
    merged->AddSymbol(liter.Symbol(), liter.Value());
    if (right_has_all) {
      int64 key = right.Find(liter.Symbol());
      if (key == -1) {
        right_has_all = false;
      } else if (!relabel && key != liter.Value()) {
        relabel = true;
      }
    }
  }
  if (right_has_all) {
    delete merged;
    if (right_relabel_output != NULL) {
      *right_relabel_output = relabel;
    }
    return right.Copy();
  }
  // add all symbols we can from right symbol table
  vector<string> conflicts;
  SymbolTableIterator riter(right);
  for (; !riter.Done(); riter.Next()) {
    int64 key = merged->Find(riter.Symbol());
    if (key != -1) {
      // Symbol already exists, maybe with different value
      if (key != riter.Value()) {
        relabel = true;
      }
      continue;
    }
    // Symbol doesn't exist from left
    left_has_all = false;
    if (!merged->Find(riter.Value()).empty()) {
      // we can't add this where we want to, add it later, in order
      conflicts.push_back(riter.Symbol());
      continue;
    }
    // there is a hole and we can add this symbol with its id
    merged->AddSymbol(riter.Symbol(), riter.Value());
  }
  if (right_relabel_output != NULL) {
    *right_relabel_output = relabel;
  }
  if (left_has_all) {
    delete merged;
    return left.Copy();
  }
  // Add all symbols that conflicted, in order
  for (int i= 0; i < conflicts.size(); ++i) {
    merged->AddSymbol(conflicts[i]);
  }
  return merged;
}

SymbolTable *CompactSymbolTable(const SymbolTable &syms) {
  map<int, string> sorted;
  SymbolTableIterator stiter(syms);
  for (; !stiter.Done(); stiter.Next()) {
    sorted[stiter.Value()] = stiter.Symbol();
  }
  SymbolTable *compact = new SymbolTable(syms.Name() + "_compact");
  uint64 newkey = 0;
  for (map<int, string>::const_iterator si = sorted.begin();
       si != sorted.end(); ++si) {
    compact->AddSymbol(si->second, newkey++);
  }
  return compact;
}

SymbolTable *FstReadSymbols(const string &filename, bool input_symbols) {
  ifstream in(filename.c_str(), std::ios_base::in | std::ios_base::binary);
  if (!in) {
    LOG(ERROR) << "FstReadSymbols: Can't open file " << filename;
    return NULL;
  }
  FstHeader hdr;
  if (!hdr.Read(in, filename)) {
    LOG(ERROR) << "FstReadSymbols: Couldn't read header from " << filename;
    return NULL;
  }
  if (hdr.GetFlags() & FstHeader::HAS_ISYMBOLS) {
    SymbolTable *isymbols = SymbolTable::Read(in, filename);
    if (isymbols == NULL) {
      LOG(ERROR) << "FstReadSymbols: Could not read input symbols from "
                 << filename;
      return NULL;
    }
    if (input_symbols) {
      return isymbols;
    }
    delete isymbols;
  }
  if (hdr.GetFlags() & FstHeader::HAS_OSYMBOLS) {
    SymbolTable *osymbols = SymbolTable::Read(in, filename);
    if (osymbols == NULL) {
      LOG(ERROR) << "FstReadSymbols: Could not read output symbols from "
                 << filename;
      return NULL;
    }
    if (!input_symbols) {
      return osymbols;
    }
    delete osymbols;
  }
  LOG(ERROR) << "FstReadSymbols: The file " << filename
             << " doesn't contain the requested symbols";
  return NULL;
}

}  // namespace fst
