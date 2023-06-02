////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
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

#include "IResearchPrimaryKeyFilter.h"
#include "Basics/DownCast.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "StorageEngine/StorageEngine.h"

#include "index/index_reader.hpp"
#include "utils/hash_utils.hpp"
#include "utils/numeric_utils.hpp"

namespace arangodb::iresearch {

#ifdef USE_ENTERPRISE
irs::doc_id_t getRemovalBoundary(irs::SubReader const&, irs::doc_id_t, bool);
#endif

template<bool Nested>
bool PrimaryKeysFilter<Nested>::next() {
  if constexpr (Nested) {
    if (IRS_UNLIKELY(_doc.value != _last_doc)) {
      TRI_ASSERT(_doc.value < _last_doc);
      ++_doc.value;
      return true;
    }
  }
  while (true) {
    if (IRS_UNLIKELY(_pos == _pks.size())) {
      _doc.value = irs::doc_limits::eof();
      if (IRS_UNLIKELY(_pks.empty())) {
        _pks = {};
      }
      return false;
    }
    auto& pk = _pks[_pos];

    // TODO(MBkkt) In theory we can optimize it and read multiple pk at once
    auto const pkRef =
        irs::numeric_utils::numeric_traits<LocalDocumentId::BaseType>::raw_ref(
            pk);
    auto doc = irs::doc_limits::eof();
    _pkField->read_documents(pkRef, {&doc, 1});
    if (irs::doc_limits::eof(doc) || _segment->docs_mask()->contains(doc)) {
      ++_pos;
      continue;
    }

    // pk iterator is one-shot
    pk = _pks.back();
    _pks.pop_back();

#ifdef USE_ENTERPRISE  // TODO(MBkkt) Make getRemovalBoundary<Nested>(...)?
    _doc.value = getRemovalBoundary(*_segment, doc, Nested);
#else
    _doc.value = doc;
#endif
    if constexpr (Nested) {
      _last_doc = doc;
    }
    return true;
  }
}

template class PrimaryKeysFilter<true>;
template class PrimaryKeysFilter<false>;

}  // namespace arangodb::iresearch
