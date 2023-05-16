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

namespace arangodb {
namespace iresearch {

irs::doc_id_t getRemovalBoundary(irs::SubReader const&, irs::doc_id_t, bool);
#ifndef USE_ENTERPRISE
IRS_FORCE_INLINE irs::doc_id_t getRemovalBoundary(irs::SubReader const&,
                                                  irs::doc_id_t doc, bool) {
  return doc;
}
#endif

PrimaryKeyFilter::PrimaryKeyFilter(LocalDocumentId value, bool nested) noexcept
    : _pk{DocumentPrimaryKey::encode(value)}, _nested{nested} {}

irs::doc_iterator::ptr PrimaryKeyFilter::execute(
    irs::ExecutionContext const& ctx) const {
  // re-execution of a fiter is not expected to ever
  // occur without a call to prepare(...)
  TRI_ASSERT(!irs::doc_limits::valid(_pkIterator.value()));
  auto& segment = ctx.segment;

  auto* pkField = segment.field(DocumentPrimaryKey::PK());

  if (IRS_UNLIKELY(!pkField)) {
    // no such field
    return irs::doc_iterator::empty();
  }

  auto const pkRef =
      irs::numeric_utils::numeric_traits<LocalDocumentId::BaseType>::raw_ref(
          _pk);

  irs::doc_id_t doc{irs::doc_limits::eof()};
  pkField->read_documents(pkRef, {&doc, 1});

  if (irs::doc_limits::eof(doc) || segment.docs_mask()->contains(doc)) {
    // no such term
    return irs::doc_iterator::empty();
  }

  _pkIterator.reset(getRemovalBoundary(segment, doc, _nested), doc);

  return irs::memory::to_managed<irs::doc_iterator>(_pkIterator);
}

size_t PrimaryKeyFilter::hash() const noexcept {
  size_t seed = 0;

  irs::hash_combine(seed, filter::hash());
  irs::hash_combine(seed, _pk);

  return seed;
}

irs::filter::prepared::ptr PrimaryKeyFilter::prepare(
    irs::IndexReader const& /*index*/, irs::Scorers const& /*ord*/,
    irs::score_t /*boost*/, irs::attribute_provider const* /*ctx*/) const {
  // optimization, since during regular runtime should have at most 1 identical
  // primary key in the entire datastore
  if (!irs::doc_limits::valid(_pkIterator.value())) {
    return irs::memory::to_managed<irs::filter::prepared const>(*this);
  }

  // already processed
  return irs::filter::prepared::empty();
}

bool PrimaryKeyFilter::equals(filter const& rhs) const noexcept {
  return filter::equals(rhs) &&
         _pk == basics::downCast<PrimaryKeyFilter>(rhs)._pk;
}

irs::filter::prepared::ptr PrimaryKeyFilterContainer::prepare(
    irs::IndexReader const& rdr, irs::Scorers const& ord, irs::score_t boost,
    irs::attribute_provider const* ctx) const {
  return irs::empty().prepare(rdr, ord, boost, ctx);
}

}  // namespace iresearch
}  // namespace arangodb
