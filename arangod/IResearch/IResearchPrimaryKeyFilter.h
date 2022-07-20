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

#pragma once

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
class PrimaryKeyFilter final : public irs::filter,
                               public irs::filter::prepared {
 public:
  static constexpr irs::string_ref type_name() noexcept {
    return "arangodb::iresearch::PrimaryKeyFilter";
  }

  static irs::type_info type(StorageEngine& engine);

  PrimaryKeyFilter(StorageEngine& engine,
                   arangodb::LocalDocumentId const& value, bool nested) noexcept
      : irs::filter(PrimaryKeyFilter::type(engine)),
        _pk(DocumentPrimaryKey::encode(value)),
        _pkSeen(false),
        _nested(nested) {}

  virtual irs::doc_iterator::ptr execute(
      irs::sub_reader const& segment, irs::Order const& /*order*/,
      irs::ExecutionMode,
      irs::attribute_provider const* /*ctx*/) const override;

  virtual size_t hash() const noexcept override;

  using irs::filter::prepare;
  virtual filter::prepared::ptr prepare(
      irs::index_reader const& index, irs::Order const& /*ord*/,
      irs::score_t /*boost*/,
      irs::attribute_provider const* /*ctx*/) const override;

 protected:
  bool equals(filter const& rhs) const noexcept override;

 private:
  struct PrimaryKeyIterator final : public irs::doc_iterator {
    PrimaryKeyIterator() = default;

    virtual bool next() noexcept override {
      if (_count != 0) {
        ++_doc.value;
        --_count;
        return true;
      }

      _doc.value = irs::doc_limits::eof();
      return false;
    }

    virtual irs::doc_id_t seek(irs::doc_id_t) noexcept override {
      TRI_ASSERT(false);
      // We don't expect this is ever called for removals.
      _count = 0;
      _doc.value = irs::doc_limits::eof();
      return irs::doc_limits::eof();
    }

    virtual irs::doc_id_t value() const noexcept override { return _doc.value; }

    virtual irs::attribute* get_mutable(
        irs::type_info::type_id id) noexcept override {
      return irs::type<irs::document>::id() == id ? &_doc : nullptr;
    }

    void reset(irs::doc_id_t begin, irs::doc_id_t end) noexcept {
      if (ADB_LIKELY(irs::doc_limits::valid(begin) &&
                     !irs::doc_limits::eof(begin) && begin <= end)) {
        _doc.value = begin - 1;
        _count = end - begin + 1;
      } else {
        _count = 0;
      }
    }

    // We intentionally violate iresearch iterator specification
    // to keep memory footprint as small as possible.
    irs::document _doc;
    irs::doc_id_t _count;
  };

  mutable LocalDocumentId::BaseType _pk;
  mutable PrimaryKeyIterator _pkIterator;
  // true == do not perform further execution (first-match optimization)
  mutable bool _pkSeen;
  bool _nested;
};

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
      : irs::filter(irs::type<PrimaryKeyFilterContainer>::get()) {}
  PrimaryKeyFilterContainer(PrimaryKeyFilterContainer&&) = default;
  PrimaryKeyFilterContainer& operator=(PrimaryKeyFilterContainer&&) = default;

  PrimaryKeyFilter& emplace(StorageEngine& engine,
                            arangodb::LocalDocumentId value, bool nested) {
    _filters.emplace_back(engine, value, nested);

    return _filters.back();
  }

  bool empty() const noexcept { return _filters.empty(); }

  void clear() noexcept { _filters.clear(); }

  virtual filter::prepared::ptr prepare(
      irs::index_reader const& rdr, irs::Order const& ord, irs::score_t boost,
      irs::attribute_provider const* ctx) const override;

 private:
  std::deque<PrimaryKeyFilter> _filters;  // pointers remain valid
};                                        // PrimaryKeyFilterContainer

}  // namespace iresearch
}  // namespace arangodb
