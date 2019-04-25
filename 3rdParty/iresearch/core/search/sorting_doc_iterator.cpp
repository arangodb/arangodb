////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2019 ArangoDB GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Andrey Abramov
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#include "sorting_doc_iterator.hpp"

NS_ROOT

bool sorting_doc_iterator::next() {
  if (doc_limits::eof(doc_)) {
    return false;
  }

  while (lead_) {
    auto begin = heap_.begin();
    auto it = heap_.end() - lead_--;

    segment& lead = *it;
    if (!lead.next()) {
      if (!remove_lead(it)) {
        doc_ = doc_limits::eof();
        attrs_ = &attribute_view::empty_instance();
        return false;
      }
      continue;
    }

    std::push_heap(begin, ++it, less_);
  }

  assert(!heap_.empty());
  std::pop_heap(heap_.begin(), heap_.end(), less_);

  const segment& lead = heap_.back();
  doc_ = lead.value();
  attrs_ = &lead.attributes();
  lead_ = 1;

  return true;
}

NS_END // ROOT
