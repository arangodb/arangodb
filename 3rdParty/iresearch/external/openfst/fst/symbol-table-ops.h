
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

#ifndef FST_LIB_SYMBOL_TABLE_OPS_H_
#define FST_LIB_SYMBOL_TABLE_OPS_H_

#include <vector>
using std::vector;
#include <string>
#include <unordered_set>
using std::unordered_set;
using std::unordered_multiset;


#include <fst/fst.h>
#include <fst/symbol-table.h>


namespace fst {

// Returns a minimal symbol table containing only symbols referenced by the
// passed fst.  Symbols preserve their original numbering, so fst does not
// require relabeling.
template<class Arc>
SymbolTable *PruneSymbolTable(const Fst<Arc> &fst, const SymbolTable &syms,
                              bool input) {
  unordered_set<typename Arc::Label> seen;
  seen.insert(0);  // Always keep epslion
  StateIterator<Fst<Arc> > siter(fst);
  for (; !siter.Done(); siter.Next()) {
    ArcIterator<Fst<Arc> > aiter(fst, siter.Value());
    for (; !aiter.Done(); aiter.Next()) {
      typename Arc::Label sym = (input) ? aiter.Value().ilabel :
                                          aiter.Value().olabel;
      seen.insert(sym);
    }
  }
  SymbolTable *pruned = new SymbolTable(syms.Name() + "_pruned");
  for (SymbolTableIterator stiter(syms); !stiter.Done(); stiter.Next()) {
    typename Arc::Label label = stiter.Value();
    if (seen.find(label) != seen.end()) {
      pruned->AddSymbol(stiter.Symbol(), stiter.Value());
    }
  }
  return pruned;
}

// Relabels a symbol table to make it a contiguous mapping.
SymbolTable *CompactSymbolTable(const SymbolTable &syms);

// Merges two SymbolTables, all symbols from left will be merged into right
// with the same ids.  Symbols in right that have conflicting ids with those
// in left will be assigned to value assigned from the left SymbolTable.
// The returned symbol table will never modify symbol assignments from the left
// side, but may do so on the right.  If right_relabel_output is non-NULL, it
// will be assigned true if the symbols from the right table needed to be
// reassigned.
// A potential use case is to Compose two Fst's that have different symbol
// tables.  You can reconcile them in the following way:
//   Fst<Arc> a, b;
//   bool relabel;
//   SymbolTable *bnew = MergeSymbolTable(a.OutputSymbols(),
//                                        b.InputSymbols(), &relabel);
//   if (relabel) {
//     Relabel(b, bnew, NULL);
//   }
//   b.SetInputSymbols(bnew);
//   delete bnew;
SymbolTable *MergeSymbolTable(const SymbolTable &left, const SymbolTable &right,
                              bool *right_relabel_output = 0);

// Read the symbol table from any Fst::Read()able file, without loading the
// corresponding Fst.  Returns NULL if the Fst does not contain a symbol table
// or the symbol table cannot be read.
SymbolTable *FstReadSymbols(const string &filename, bool input);

}  // namespace fst
#endif  // FST_LIB_SYMBOL_TABLE_OPS_H_
