// verify.h

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
// Function to verify an Fst's contents

#ifndef FST_LIB_VERIFY_H__
#define FST_LIB_VERIFY_H__

#include <fst/fst.h>
#include <fst/test-properties.h>


namespace fst {

// Verifies that an Fst's contents are sane.
template<class Arc>
bool Verify(const Fst<Arc> &fst, bool allow_negative_labels = false) {
  typedef typename Arc::Label Label;
  typedef typename Arc::Weight Weight;
  typedef typename Arc::StateId StateId;

  StateId start = fst.Start();
  const SymbolTable *isyms = fst.InputSymbols();
  const SymbolTable *osyms = fst.OutputSymbols();

  // Count states
  StateId ns = 0;
  for (StateIterator< Fst<Arc> > siter(fst);
       !siter.Done();
       siter.Next())
    ++ns;

  if (start == kNoStateId && ns > 0) {
    LOG(ERROR) << "Verify: Fst start state ID unset";
    return false;
  } else if (start >= ns) {
    LOG(ERROR) << "Verify: Fst start state ID exceeds number of states";
    return false;
  }

  for (StateIterator< Fst<Arc> > siter(fst);
       !siter.Done();
       siter.Next()) {
    StateId s = siter.Value();
    size_t na = 0;
    for (ArcIterator< Fst<Arc> > aiter(fst, s);
         !aiter.Done();
         aiter.Next()) {
      const Arc &arc =aiter.Value();
      if (!allow_negative_labels && arc.ilabel < 0) {
        LOG(ERROR) << "Verify: Fst input label ID of arc at position "
                   << na << " of state " << s << " is negative";
        return false;
      } else if (isyms && isyms->Find(arc.ilabel) == "") {
        LOG(ERROR) << "Verify: Fst input label ID " << arc.ilabel
                   << " of arc at position " << na << " of state " <<  s
                   << " is missing from input symbol table \""
                   << isyms->Name() << "\"";
        return false;
      } else if (!allow_negative_labels && arc.olabel < 0) {
        LOG(ERROR) << "Verify: Fst output label ID of arc at position "
                   << na << " of state " << s << " is negative";
        return false;
      } else if (osyms && osyms->Find(arc.olabel) == "") {
        LOG(ERROR) << "Verify: Fst output label ID " << arc.olabel
                   << " of arc at position " << na << " of state " <<  s
                   << " is missing from output symbol table \""
                   << osyms->Name() << "\"";
        return false;
      } else if (!arc.weight.Member()) {
        LOG(ERROR) << "Verify: Fst weight of arc at position "
                   << na << " of state " << s << " is invalid";
        return false;
      } else if (arc.nextstate < 0) {
        LOG(ERROR) << "Verify: Fst destination state ID of arc at position "
                   << na << " of state " << s << " is negative";
        return false;
      } else if (arc.nextstate >= ns) {
        LOG(ERROR) << "Verify: Fst destination state ID of arc at position "
                   << na << " of state " << s
                   << " exceeds number of states";
        return false;
      }
      ++na;
    }
    if (!fst.Final(s).Member()) {
      LOG(ERROR) << "Verify: Fst final weight of state " << s << " is invalid";
      return false;
    }
  }
  uint64 fst_props = fst.Properties(kFstProperties, false);
  if (fst_props & kError) {
    LOG(ERROR) << "Verify: Fst error property is set";
    return false;
  }

  uint64 known_props;
  uint64 test_props = ComputeProperties(fst, kFstProperties, &known_props,
                                        false);
  if (!CompatProperties(fst_props, test_props)) {
    LOG(ERROR) << "Verify: stored Fst properties incorrect "
               << "(props1 = stored props, props2 = tested)";
    return false;
  } else {
    return true;
  }
}

}  // namespace fst

#endif  // FST_LIB_VERIFY_H__
