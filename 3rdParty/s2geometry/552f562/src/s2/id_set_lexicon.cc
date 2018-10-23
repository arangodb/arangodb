// Copyright 2016 Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS-IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

// Author: ericv@google.com (Eric Veach)

#include "s2/id_set_lexicon.h"

#include <algorithm>
#include <vector>

#include "s2/base/logging.h"

IdSetLexicon::IdSetLexicon() {
}

IdSetLexicon::~IdSetLexicon() {
}

// We define the copy/move constructors and assignment operators explicitly
// in order to avoid copying/moving the temporary storage vector "tmp_".

IdSetLexicon::IdSetLexicon(const IdSetLexicon& x) : id_sets_(x.id_sets_) {
}

IdSetLexicon::IdSetLexicon(IdSetLexicon&& x) : id_sets_(std::move(x.id_sets_)) {
}

IdSetLexicon& IdSetLexicon::operator=(const IdSetLexicon& x) {
  id_sets_ = x.id_sets_;
  return *this;
}

IdSetLexicon& IdSetLexicon::operator=(IdSetLexicon&& x) {
  id_sets_ = std::move(x.id_sets_);
  return *this;
}

void IdSetLexicon::Clear() {
  id_sets_.Clear();
}

int32 IdSetLexicon::AddInternal(std::vector<int32>* ids) {
  if (ids->empty()) {
    // Empty sets have a special id chosen not to conflict with other ids.
    return kEmptySetId;
  } else if (ids->size() == 1) {
    // Singleton sets are represented by their element.
    return (*ids)[0];
  } else {
    // Canonicalize the set by sorting and removing duplicates.
    std::sort(ids->begin(), ids->end());
    ids->erase(std::unique(ids->begin(), ids->end()), ids->end());
    // Non-singleton sets are represented by the bitwise complement of the id
    // returned by SequenceLexicon.
    return ~id_sets_.Add(*ids);
  }
}

IdSetLexicon::IdSet IdSetLexicon::id_set(int32 set_id) const {
  if (set_id >= 0) {
    return IdSet(set_id);
  } else if (set_id == kEmptySetId) {
    return IdSet();
  } else {
    auto sequence = id_sets_.sequence(~set_id);
    S2_DCHECK_NE(0, sequence.size());
    return IdSet(&*sequence.begin(), &*sequence.begin() + sequence.size());
  }
}
