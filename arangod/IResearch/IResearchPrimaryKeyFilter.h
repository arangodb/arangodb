////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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

#ifndef ARANGOD_IRESEARCH__IRESEARCH_PRIMARY_KEY_FILTER_H
#define ARANGOD_IRESEARCH__IRESEARCH_PRIMARY_KEY_FILTER_H 1

#include "IResearchDocument.h"
#include "VocBase/voc-types.h"

#include "search/filter.hpp"
#include "utils/type_limits.hpp"

namespace arangodb {
class StorageEngine;
namespace iresearch {

///////////////////////////////////////////////////////////////////////////////
/// @class PrimaryKeyFilter
/// @brief iresearch filter optimized for filtering on primary keys
///////////////////////////////////////////////////////////////////////////////
class PrimaryKeyFilter final
    : public irs::filter,
    public irs::filter::prepared {
 public:
  static constexpr irs::string_ref type_name() noexcept {
    return "arangodb::iresearch::PrimaryKeyFilter";
  }

  static irs::type_info type(StorageEngine& engine);

  PrimaryKeyFilter(StorageEngine& engine, arangodb::LocalDocumentId const& value) noexcept
      : irs::filter(PrimaryKeyFilter::type(engine)),
        _pk(DocumentPrimaryKey::encode(value)),
        _pkSeen(false) {}

  // ----------------------------------------------------------------------------
  // --SECTION-- irs::filter::prepared
  // ----------------------------------------------------------------------------

  virtual irs::doc_iterator::ptr execute(
      irs::sub_reader const& segment,
      irs::order::prepared const& /*order*/,
      irs::attribute_provider const* /*ctx*/) const override;

  // ----------------------------------------------------------------------------
  // --SECTION-- irs::filter
  // ----------------------------------------------------------------------------

  virtual size_t hash() const noexcept override;

  using irs::filter::prepare;
  virtual filter::prepared::ptr prepare(
    irs::index_reader const& index,
    irs::order::prepared const& /*ord*/,
    irs::boost_t /*boost*/,
    irs::attribute_provider const* /*ctx*/) const override;

 protected:
  bool equals(filter const& rhs) const noexcept override;

 private:
  struct PrimaryKeyIterator final : public irs::doc_iterator {
    PrimaryKeyIterator() = default;

    virtual bool next() noexcept override {
      _doc = _next;
      _next = irs::doc_limits::eof();
      return !irs::doc_limits::eof(_doc);
    }

    virtual irs::doc_id_t seek(irs::doc_id_t target) noexcept override {
      _doc = target <= _next ? _next : irs::doc_limits::eof();

      return _doc;
    }

    virtual irs::doc_id_t value() const noexcept override { return _doc; }

    virtual irs::attribute* get_mutable(irs::type_info::type_id) noexcept override {
      return nullptr;
    }

    void reset(irs::doc_id_t doc) noexcept {
      _doc = irs::doc_limits::invalid();
      _next = doc;
    }

    mutable irs::doc_id_t _doc{irs::doc_limits::invalid()};
    mutable irs::doc_id_t _next{irs::doc_limits::eof()};
  };  // PrimaryKeyIterator

  mutable LocalDocumentId::BaseType _pk;
  mutable PrimaryKeyIterator _pkIterator;
  mutable bool _pkSeen;  // true == do not perform further execution
                         // (first-match optimization)
};                       // PrimaryKeyFilter

///////////////////////////////////////////////////////////////////////////////
/// @class PrimaryKeyFilterContainer
/// @brief container for storing 'PrimaryKeyFilter's, does nothing as a filter
///////////////////////////////////////////////////////////////////////////////
class PrimaryKeyFilterContainer final : public irs::filter {
 public:
  static constexpr irs::string_ref type_name() noexcept {
    return "arangodb::iresearch::PrimaryKeyFilterContainer";
  }

  PrimaryKeyFilterContainer()
    : irs::filter(irs::type<PrimaryKeyFilterContainer>::get()) {
  }
  PrimaryKeyFilterContainer(PrimaryKeyFilterContainer&&) = default;
  PrimaryKeyFilterContainer& operator=(PrimaryKeyFilterContainer&&) = default;

  PrimaryKeyFilter& emplace(StorageEngine& engine, arangodb::LocalDocumentId const& value) {
    _filters.emplace_back(engine, value);

    return _filters.back();
  }

  bool empty() const noexcept { return _filters.empty(); }

  void clear() noexcept { _filters.clear(); }

  virtual filter::prepared::ptr prepare(
    irs::index_reader const& rdr,
    irs::order::prepared const& ord,
    irs::boost_t boost,
    irs::attribute_provider const* ctx) const override;

 private:
  std::deque<PrimaryKeyFilter> _filters;  // pointers remain valid
};                                        // PrimaryKeyFilterContainer

}  // namespace iresearch
}  // namespace arangodb

#endif  // ARANGOD_IRESEARCH__IRESEARCH_PRIMARY_KEY_FILTER_H
