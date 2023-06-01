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

namespace arangodb::iresearch {

class PrimaryKeysFilterBase : public irs::filter,
                              public irs::filter::prepared,
                              public irs::doc_iterator {
 public:
  void emplace(LocalDocumentId value) {
    _pks.emplace_back(DocumentPrimaryKey::encode(value));
  }

  bool empty() const noexcept { return _pks.empty(); }

 protected:
  static constexpr std::string_view type_name() noexcept {
    return "arangodb::iresearch::PrimaryKeyFilterContainer";
  }

  irs::type_info::type_id type() const noexcept final {
    return irs::type<PrimaryKeysFilterBase>::id();
  }

  filter::prepared::ptr prepare(
      irs::IndexReader const& rdr, irs::Scorers const& ord, irs::score_t boost,
      irs::attribute_provider const* ctx) const final {
    if (_pks.empty()) {
      return irs::filter::prepared::empty();
    }
    return irs::memory::to_managed<irs::filter::prepared const>(*this);
  }

  irs::doc_iterator::ptr execute(irs::ExecutionContext const& ctx) const final {
    _segment = &ctx.segment;
    _pkField = _segment->field(DocumentPrimaryKey::PK());
    if (IRS_UNLIKELY(!_pkField)) {  // TODO(MBkkt) assert?
      return irs::doc_iterator::empty();
    }
    _pos = 0;
    _doc.value = irs::doc_limits::invalid();
    _last_doc = irs::doc_limits::invalid();
    return irs::memory::to_managed<irs::doc_iterator>(
        const_cast<PrimaryKeysFilterBase&>(*this));
  }

  void visit(irs::SubReader const&, irs::PreparedStateVisitor&,
             irs::score_t) const final {}

  irs::attribute* get_mutable(irs::type_info::type_id id) noexcept final {
    return irs::type<irs::document>::id() == id ? &_doc : nullptr;
  }

  irs::doc_id_t value() const noexcept final { return _doc.value; }

  irs::doc_id_t seek(irs::doc_id_t) noexcept final {
    TRI_ASSERT(false);
    return _doc.value = irs::doc_limits::eof();
  }

  mutable std::vector<LocalDocumentId::BaseType> _pks;

  // Iterator over different LocalDocumentId
  mutable irs::SubReader const* _segment{};  // TODO(MBkkt) maybe doc_mask?
  mutable irs::term_reader const* _pkField{};
  mutable size_t _pos{0};

  // Iterator over different doc_id
  mutable irs::document _doc;
  mutable irs::doc_id_t _last_doc{irs::doc_limits::invalid()};
};

template<bool Nested>
class PrimaryKeysFilter final : public PrimaryKeysFilterBase {
 public:
  PrimaryKeysFilter() = default;

 private:
  bool next() final;
};

}  // namespace arangodb::iresearch
