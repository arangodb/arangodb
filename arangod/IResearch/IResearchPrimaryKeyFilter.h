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

#ifndef ARANGOD_IRESEARCH__IRESEARCH_PRIMARY_KEY_FILTER_H
#define ARANGOD_IRESEARCH__IRESEARCH_PRIMARY_KEY_FILTER_H 1

#include "VocBase/voc-types.h"
#include "IResearchDocument.h"

#include "search/filter.hpp"
#include "utils/type_limits.hpp"

namespace arangodb {
namespace iresearch {

///////////////////////////////////////////////////////////////////////////////
/// @class PrimaryKeyFilter
/// @brief iresearch filter optimized for filtering on primary keys
///////////////////////////////////////////////////////////////////////////////
class PrimaryKeyFilter final
    : public irs::filter,
      public irs::filter::prepared {
 public:
  DECLARE_FILTER_TYPE();

  PrimaryKeyFilter(TRI_voc_cid_t cid, TRI_voc_rid_t id) noexcept
    : irs::filter(PrimaryKeyFilter::type()),
      _pk(cid, id) { // ensure proper endianness
  }

// ----------------------------------------------------------------------------
// --SECTION--                                            irs::filter::prepared
// ----------------------------------------------------------------------------

  virtual irs::doc_iterator::ptr execute(
    irs::sub_reader const& segment,
    irs::order::prepared const& /*order*/,
    irs::attribute_view const& /*ctx*/
  ) const override;

// ----------------------------------------------------------------------------
// --SECTION--                                                      irs::filter
// ----------------------------------------------------------------------------

  virtual size_t hash() const noexcept override;

  using irs::filter::prepare;
  virtual filter::prepared::ptr prepare(
    irs::index_reader const& index,
    irs::order::prepared const& /*ord*/,
    irs::boost::boost_t /*boost*/,
    irs::attribute_view const& /*ctx*/
  ) const override;

 protected:
  bool equals(filter const& rhs) const noexcept override;

 private:
  struct PrimaryKeyIterator final : public irs::doc_iterator {
    PrimaryKeyIterator() = default;

    virtual bool next() noexcept override {
      _doc = _next;
      _next = irs::type_limits<irs::type_t::doc_id_t>::eof();
      return !irs::type_limits<irs::type_t::doc_id_t>::eof(_doc);
    }

    virtual irs::doc_id_t seek(irs::doc_id_t target) noexcept override {
      _doc = target <= _next
        ? _next
        : irs::type_limits<irs::type_t::doc_id_t>::eof();

      return _doc;
    }

    virtual irs::doc_id_t value() const noexcept override {
      return _doc;
    }

    virtual irs::attribute_view const& attributes() const noexcept override {
      return irs::attribute_view::empty_instance();
    }

    void reset(irs::doc_id_t doc) noexcept {
      _doc = irs::type_limits<irs::type_t::doc_id_t>::invalid();
      _next = doc;
    }

    mutable irs::doc_id_t _doc{ irs::type_limits<irs::type_t::doc_id_t>::invalid() };
    mutable irs::doc_id_t _next{ irs::type_limits<irs::type_t::doc_id_t>::eof() };
  }; // PrimaryKeyIterator

  mutable DocumentPrimaryKey _pk; // !_pk.first -> do not perform further execution (first-match optimization)
  mutable PrimaryKeyIterator _pkIterator;
}; // PrimaryKeyFilter

///////////////////////////////////////////////////////////////////////////////
/// @class PrimaryKeyFilterContainer
/// @brief container for storing 'PrimaryKeyFilter's, does nothing as a filter
///////////////////////////////////////////////////////////////////////////////
class PrimaryKeyFilterContainer final : public irs::empty {
 public:
  DECLARE_FILTER_TYPE();

  PrimaryKeyFilterContainer() = default;
  PrimaryKeyFilterContainer(PrimaryKeyFilterContainer&&) = default;
  PrimaryKeyFilterContainer& operator=(PrimaryKeyFilterContainer&&) = default;

  PrimaryKeyFilter& emplace(TRI_voc_cid_t cid, TRI_voc_rid_t rid) {
    _filters.emplace_back(cid, rid);

    return _filters.back();
  }

  bool empty() const noexcept {
    return _filters.empty();
  }

  void clear() noexcept {
    _filters.clear();
  }

 private:
  std::deque<PrimaryKeyFilter> _filters; // pointers remain valid
}; // PrimaryKeyFilterContainer

} // iresearch
} // arangodb

#endif // ARANGOD_IRESEARCH__IRESEARCH_PRIMARY_KEY_FILTER_H 
