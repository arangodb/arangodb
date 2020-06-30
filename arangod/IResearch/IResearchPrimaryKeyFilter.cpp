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
#include "utils/numeric_utils.hpp"

namespace {

struct typeDefault {
  static constexpr irs::string_ref type_name() noexcept {
    return "::typeDefault";
  }
};

struct typeRecovery {
  static constexpr irs::string_ref type_name() noexcept {
    return "::typeRecovery";
  }
};

}  // namespace

namespace arangodb {
namespace iresearch {

// ----------------------------------------------------------------------------
// --SECTION--                                  PrimaryKeyFilter implementation
// ----------------------------------------------------------------------------

irs::doc_iterator::ptr PrimaryKeyFilter::execute(
    irs::sub_reader const& segment,
    irs::order::prepared const& /*order*/,
    irs::attribute_provider const* /*ctx*/ ) const {
  TRI_ASSERT(!_pkSeen);  // re-execution of a fiter is not expected to ever
                         // occur without a call to prepare(...)
  auto* pkField = segment.field(arangodb::iresearch::DocumentPrimaryKey::PK());

  if (!pkField) {
    // no such field
    return irs::doc_iterator::empty();
  }

  auto term = pkField->iterator();

  auto const pkRef =
      irs::numeric_utils::numeric_traits<LocalDocumentId::BaseType>::raw_ref(_pk);

  if (!term->seek(pkRef)) {
    // no such term
    return irs::doc_iterator::empty();
  }

  auto docs = segment.mask(term->postings(irs::flags::empty_instance()));  // must not match removed docs

  if (!docs->next()) {
    return irs::doc_iterator::empty();
  }

  _pkIterator.reset(docs->value());

  // optimization, since during:
  // * regular runtime should have at most 1 identical live primary key in the
  // entire datastore
  // * recovery should have at most 2 identical live primary keys in the entire
  // datastore
  if (irs::filter::type() == irs::type<typeDefault>::id()) {  // explicitly check type of instance
    TRI_ASSERT(!docs->next());  // primary key duplicates should NOT happen in
                                // the same segment in regular runtime
    _pkSeen = true;  // already matched 1 primary key (should be at most 1 at runtime)
  }

  return irs::memory::to_managed<irs::doc_iterator, false>(
    const_cast<PrimaryKeyIterator*>(&_pkIterator));
}

size_t PrimaryKeyFilter::hash() const noexcept {
  size_t seed = 0;

  irs::hash_combine(seed, filter::hash());
  irs::hash_combine(seed, _pk);

  return seed;
}

irs::filter::prepared::ptr PrimaryKeyFilter::prepare(
    irs::index_reader const& /*index*/,
    irs::order::prepared const& /*ord*/,
    irs::boost_t /*boost*/,
    irs::attribute_provider const* /*ctx*/) const {
  // optimization, since during:
  // * regular runtime should have at most 1 identical primary key in the entire
  // datastore
  // * recovery should have at most 2 identical primary keys in the entire
  // datastore
  if (_pkSeen) {
    return irs::filter::prepared::empty();  // already processed
  }

  return irs::memory::to_managed<const irs::filter::prepared, false>(this);
}

bool PrimaryKeyFilter::equals(filter const& rhs) const noexcept {
  return filter::equals(rhs) && _pk == static_cast<PrimaryKeyFilter const&>(rhs)._pk;
}

/*static*/ irs::type_info PrimaryKeyFilter::type() {
  return arangodb::EngineSelectorFeature::ENGINE &&
                 arangodb::EngineSelectorFeature::ENGINE->inRecovery()
             ? irs::type<typeRecovery>::get()
             : irs::type<typeDefault>::get();
}

// ----------------------------------------------------------------------------
// --SECTION--                         PrimaryKeyFilterContainer implementation
// ----------------------------------------------------------------------------

irs::filter::prepared::ptr PrimaryKeyFilterContainer::prepare(
    irs::index_reader const& rdr,
    irs::order::prepared const& ord,
    irs::boost_t boost,
    irs::attribute_provider const* ctx) const {
  return irs::empty().prepare(rdr, ord, boost, ctx);
}

}  // namespace iresearch
}  // namespace arangodb
