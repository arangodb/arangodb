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
#include "StorageEngine/EngineSelectorFeature.h"
#include "StorageEngine/StorageEngine.h"

#include "index/index_reader.hpp"
#include "utils/hash_utils.hpp"

namespace {

::iresearch::type_id typeDefault;
::iresearch::type_id typeRecovery;

}

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
  TRI_ASSERT(_pk.first); // re-execution of a fiter is not expected to ever occur without a call to prepare(...)
  auto* pkField = segment.field(arangodb::iresearch::DocumentPrimaryKey::PK());

  if (!pkField) {
    // no such field
    return irs::doc_iterator::empty();
  }

  auto term = pkField->iterator();

  if (!term->seek(static_cast<irs::bytes_ref>(_pk))) {
    // no such term
    return irs::doc_iterator::empty();
  }

  auto docs = segment.mask(term->postings(irs::flags::empty_instance())); // must not match removed docs

  if (!docs->next()) {
    return irs::doc_iterator::empty();
  }

  _pkIterator.reset(docs->value());

  // optimization, since during:
  // * regular runtime should have at most 1 identical live primary key in the entire datastore
  // * recovery should have at most 2 identical live primary keys in the entire datastore
  if (irs::filter::type() == typeDefault) { // explicitly check type of instance
    TRI_ASSERT(!docs->next()); // primary key duplicates should NOT happen in the same segment in regular runtime
    _pk.first = 0; // already matched 1 primary key (should be at most 1 at runtime)
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
    irs::index_reader const& /*index*/,
    irs::order::prepared const& /*ord*/,
    irs::boost::boost_t /*boost*/,
    irs::attribute_view const& /*ctx*/
) const {
  // optimization, since during:
  // * regular runtime should have at most 1 identical primary key in the entire datastore
  // * recovery should have at most 2 identical primary keys in the entire datastore
  if (!_pk.first) {
    return irs::filter::prepared::empty(); // already processed
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

/*static*/ ::iresearch::type_id const& PrimaryKeyFilter::type() {
  return arangodb::EngineSelectorFeature::ENGINE
         && arangodb::EngineSelectorFeature::ENGINE->inRecovery()
    ? typeRecovery : typeDefault
    ;
}

} // iresearch
} // arangodb
