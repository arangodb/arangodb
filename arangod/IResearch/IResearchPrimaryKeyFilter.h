////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
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
  static constexpr std::string_view type_name() noexcept {
    return "arangodb::iresearch::PrimaryKeyFilter";
  }

  PrimaryKeyFilter(LocalDocumentId value, bool nested) noexcept;

  irs::type_info::type_id type() const noexcept final {
    return irs::type<PrimaryKeyFilter>::id();
  }
  irs::doc_iterator::ptr execute(irs::ExecutionContext const& ctx) const final;

  size_t hash() const noexcept final;

  using irs::filter::prepare;
  filter::prepared::ptr prepare(
      irs::IndexReader const& index, irs::Scorers const& /*ord*/,
      irs::score_t /*boost*/,
      irs::attribute_provider const* /*ctx*/) const final;

  void visit(irs::SubReader const&, irs::PreparedStateVisitor&,
             irs::score_t) const final {
    // NOOP
  }

 private:
  bool equals(filter const& rhs) const noexcept final;

  struct PrimaryKeyIterator final : public irs::doc_iterator {
    PrimaryKeyIterator() = default;

    bool next() noexcept final {
      if (_count != 0) {
        ++_doc.value;
        --_count;
        return true;
      }

      _doc.value = irs::doc_limits::eof();
      return false;
    }

    irs::doc_id_t seek(irs::doc_id_t) noexcept final {
      TRI_ASSERT(false);
      // We don't expect this is ever called for removals.
      _count = 0;
      _doc.value = irs::doc_limits::eof();
      return irs::doc_limits::eof();
    }

    irs::doc_id_t value() const noexcept final { return _doc.value; }

    irs::attribute* get_mutable(irs::type_info::type_id id) noexcept final {
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

    void reset() noexcept {
      _doc.value = irs::doc_limits::eof();
      _count = 0;
    }

    // We intentionally violate iresearch iterator specification
    // to keep memory footprint as small as possible.
    irs::document _doc;
    irs::doc_id_t _count{0};
  };

  mutable LocalDocumentId::BaseType _pk;
  mutable PrimaryKeyIterator _pkIterator;
  bool _nested;
};

///////////////////////////////////////////////////////////////////////////////
/// @class PrimaryKeyFilterContainer
/// @brief container for storing 'PrimaryKeyFilter's, does nothing as a filter
///////////////////////////////////////////////////////////////////////////////
class PrimaryKeyFilterContainer final : public irs::filter {
 public:
  static constexpr std::string_view type_name() noexcept {
    return "arangodb::iresearch::PrimaryKeyFilterContainer";
  }

  PrimaryKeyFilterContainer() = default;
  PrimaryKeyFilterContainer(PrimaryKeyFilterContainer&&) = default;
  PrimaryKeyFilterContainer& operator=(PrimaryKeyFilterContainer&&) = default;

  PrimaryKeyFilter& emplace(LocalDocumentId value, bool nested) {
    return _filters.emplace_back(value, nested);
  }

  bool empty() const noexcept { return _filters.empty(); }

  void clear() noexcept { _filters.clear(); }

  irs::type_info::type_id type() const noexcept final {
    return irs::type<PrimaryKeyFilterContainer>::id();
  }

  filter::prepared::ptr prepare(irs::IndexReader const& rdr,
                                irs::Scorers const& ord, irs::score_t boost,
                                irs::attribute_provider const* ctx) const final;

 private:
  std::deque<PrimaryKeyFilter> _filters;  // pointers remain valid
};

}  // namespace iresearch
}  // namespace arangodb
