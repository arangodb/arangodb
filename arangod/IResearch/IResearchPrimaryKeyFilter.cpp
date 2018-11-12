////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2018 ArangoDB GmbH, Cologne, Germany
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

#include "index/index_reader.hpp"
#include "utils/hash_utils.hpp"

namespace arangodb {
namespace iresearch {

// ----------------------------------------------------------------------------
// --SECTION--                                  PrimaryKeyFilter implementation
// ----------------------------------------------------------------------------

irs::doc_iterator::ptr PrimaryKeyFilter::execute(
    irs::sub_reader const& segment,
    irs::order::prepared const& /*order*/,
    irs::attribute_view const& /*ctx*/
) const {
  if (&segment != _pkIterator._pkSegment) {
    return irs::doc_iterator::empty();
  }

  // aliasing constructor
  return irs::doc_iterator::ptr(
    irs::doc_iterator::ptr(),
    const_cast<PrimaryKeyIterator*>(&_pkIterator)
  );
}

size_t PrimaryKeyFilter::hash() const noexcept {
  size_t seed = 0;
  irs::hash_combine(seed, filter::hash());
  irs::hash_combine(seed, _pk.first);
  irs::hash_combine(seed, _pk.second);
  return seed;
}

irs::filter::prepared::ptr PrimaryKeyFilter::prepare(
    irs::index_reader const& index,
    irs::order::prepared const& /*ord*/,
    irs::boost::boost_t /*boost*/,
    irs::attribute_view const& /*ctx*/
) const {
  irs::bytes_ref const pkRef(
    reinterpret_cast<irs::byte_type const*>(&_pk), sizeof(_pk)
  );

  for (auto& segment : index) {
    auto* pkField = segment.field(arangodb::iresearch::DocumentPrimaryKey::PK());

    if (!pkField) {
      continue;
    }

    auto term = pkField->iterator();

    if (!term->seek(pkRef)) {
      continue;
    }

    auto docs = term->postings(irs::flags::empty_instance());

    if (docs->next()) {
      _pkIterator.reset(segment, docs->value());
    }

    break;
  }

  // aliasing constructor
  return irs::filter::prepared::ptr(irs::filter::prepared::ptr(), this);
}

bool PrimaryKeyFilter::equals(filter const& rhs) const noexcept {
  auto const& trhs = static_cast<PrimaryKeyFilter const&>(rhs);

  return filter::equals(rhs)
    && _pk.first == trhs._pk.first
    && _pk.second == trhs._pk.second;
}

DEFINE_FILTER_TYPE(PrimaryKeyFilter);

} // iresearch
} // arangodb
