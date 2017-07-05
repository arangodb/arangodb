////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2017 ArangoDB GmbH, Cologne, Germany
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

#include "AttributeScorer.h"

#include "analysis/token_attributes.hpp"
#include "index/field_meta.hpp"
#include "IResearch/IResearchDocument.h"
#include "Logger/Logger.h"
#include "StorageEngine/DocumentIdentifierToken.h"
#include "Transaction/Methods.h"
#include "velocypack/Slice.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/ManagedDocumentResult.h"

NS_LOCAL

irs::string_ref const ATTRIBUTE_SCORER_NAME("@");

////////////////////////////////////////////////////////////////////////////////
/// @brief lazy-computed score from within Prepared::less(...)
////////////////////////////////////////////////////////////////////////////////
struct Score {
  void (*compute)(std::string const& attr, arangodb::transaction::Methods& trx, Score& score);
  irs::doc_id_t docId;
  irs::field_id pkColId;
  irs::sub_reader const* reader;
  arangodb::velocypack::Slice slice;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief implementation of an iResearch prepared scorer for AttributeScorer
////////////////////////////////////////////////////////////////////////////////
class Prepared: public irs::sort::prepared_base<Score> {
 public:
  DECLARE_FACTORY(Prepared);

  Prepared(
    std::string const& attr,
    size_t const (&order)[arangodb::iresearch::AttributeScorer::ValueType::eLast],
    bool reverse,
    arangodb::transaction::Methods& trx
  );

  virtual void add(score_t& dst, const score_t& src) const override;
  virtual irs::flags const& features() const override;
  virtual bool less(const score_t& lhs, const score_t& rhs) const override;
  virtual irs::sort::collector::ptr prepare_collector() const override;
  virtual void prepare_score(score_t& score) const override;
  virtual irs::sort::scorer::ptr prepare_scorer(
    irs::sub_reader const& segment,
    irs::term_reader const& field,
    irs::attribute_store const& query_attrs,
    irs::attribute_store const& doc_attrs
  ) const override;

 private:
  std::string _attr;
  mutable size_t _nextOrder;
  mutable size_t _order[arangodb::iresearch::AttributeScorer::ValueType::eLast + 1]; // type precedence order, +1 for unordered/unsuported types
  bool _reverse;
  arangodb::transaction::Methods& _trx;

  size_t precedence(arangodb::velocypack::Slice const& slice) const;
};

Prepared::Prepared(
    std::string const& attr,
    size_t const (&order)[arangodb::iresearch::AttributeScorer::ValueType::eLast],
    bool reverse,
    arangodb::transaction::Methods& trx
): _attr(attr),
   _nextOrder(arangodb::iresearch::AttributeScorer::ValueType::eLast + 1), // past default set values, +1 for values unasigned by AttributeScorer
   _reverse(reverse),
   _trx(trx) {
  std::memcpy(_order, order, sizeof(order));
}

void Prepared::add(score_t& dst, const score_t& src) const {
  TRI_ASSERT(
    !dst.reader // if score is initialized then it must match exactly
    || (dst.docId == src.docId && dst.pkColId == src.pkColId && dst.reader == src.reader)
  );
  dst = src; // copy over score (initialize an uninitialized score)
}

irs::flags const& Prepared::features() const {
  return irs::flags::empty_instance();
}

bool Prepared::less(const score_t& lhs, const score_t& rhs) const {
  lhs.compute(_attr, _trx, const_cast<score_t&>(lhs));
  rhs.compute(_attr, _trx, const_cast<score_t&>(rhs));

  if (lhs.slice.isBoolean() && rhs.slice.isBoolean()) {
      return _reverse
        ? lhs.slice.getBoolean() > rhs.slice.getBoolean()
        : lhs.slice.getBoolean() < rhs.slice.getBoolean()
        ;
  }

  if (lhs.slice.isNumber() && rhs.slice.isNumber()) {
      return _reverse
        ? lhs.slice.getNumber<double>() > rhs.slice.getNumber<double>()
        : lhs.slice.getNumber<double>() < rhs.slice.getNumber<double>()
        ;
  }

  if (lhs.slice.isString() && rhs.slice.isString()) {
    arangodb::velocypack::ValueLength lhsLength;
    arangodb::velocypack::ValueLength rhsLength;
    auto* lhsValue = lhs.slice.getString(lhsLength);
    auto* rhsValue = rhs.slice.getString(rhsLength);
    irs::string_ref lhsStr(lhsValue, lhsLength);
    irs::string_ref rhsStr(rhsValue, rhsLength);

    return _reverse ? lhsStr > rhsStr : lhsStr < rhsStr;
  }

  // no way to compare values for order, compare by presedence
  return _reverse
    ? precedence(lhs.slice) > precedence(rhs.slice)
    : precedence(lhs.slice) < precedence(rhs.slice)
    ;
}

irs::sort::collector::ptr Prepared::prepare_collector() const {
  return nullptr;
}

void Prepared::prepare_score(score_t& score) const {
  struct Compute {
    static void noop(std::string const& attr, arangodb::transaction::Methods& trx, Score& score) {}
    static void invoke(std::string const& attr, arangodb::transaction::Methods& trx, Score& score) {
      score.compute = &Compute::noop; // do not recompute score again
      score.slice = arangodb::velocypack::Slice(); // inilialize to an unsupported value

      if (!score.reader) {
        return; // score value not initialized, see errors during initialization
      }

      arangodb::iresearch::DocumentPrimaryKey docPk;
      irs::bytes_ref tmpRef;

      if (!score.reader->values(score.pkColId)(score.docId, tmpRef) || !docPk.read(tmpRef)) {
        LOG_TOPIC(WARN, arangodb::Logger::FIXME) << "failed to read document primary key while computing document score, doc_id '" << score.docId << "'";

        return; // not a valid document reference
      }

      static const std::string unknown("<unknown>");

      trx.addCollectionAtRuntime(docPk.cid(), unknown);

      auto* collection = trx.documentCollection(docPk.cid());

      if (!collection) {
        LOG_TOPIC(WARN, arangodb::Logger::FIXME) << "failed to find collection while computing document score, cid '" << docPk.cid() << "', rid '" << docPk.rid() << "'";

        return; // not a valid collection reference
      }

      arangodb::DocumentIdentifierToken colToken;
      arangodb::ManagedDocumentResult docResult;

      colToken._data = docPk.rid();

      if (!collection->readDocument(&trx, colToken, docResult)) {
        LOG_TOPIC(WARN, arangodb::Logger::FIXME) << "failed to read document while computing document score, cid '" << docPk.cid() << "', rid '" << docPk.rid() << "'";

        return; // not a valid document
      }

      arangodb::velocypack::Slice doc(docResult.vpack());

      if (doc.hasKey(attr)) {
        score.slice = doc.get(attr);
      }
    }
  };

  score.compute = &Compute::invoke;
  score.reader = nullptr; // unset for the case where the object is reused
}

irs::sort::scorer::ptr Prepared::prepare_scorer(
    irs::sub_reader const& segment,
    irs::term_reader const& field,
    irs::attribute_store const& query_attrs,
    irs::attribute_store const& doc_attrs
) const {
  class Scorer: public irs::sort::scorer {
   public:
    Scorer(
        irs::sub_reader const& reader,
        irs::attribute_store::ref<irs::document> const& doc
    ): _doc(doc), _reader(reader) {
    }
    virtual void score(irs::byte_type* score_buf) override {
      auto& score = *reinterpret_cast<score_t*>(score_buf);
      auto* doc = _doc.get();
      auto* pkColMeta = _reader.column(arangodb::iresearch::DocumentPrimaryKey::PK());

      if (!doc) {
        LOG_TOPIC(WARN, arangodb::Logger::FIXME) << "encountered a document without a doc_id value while scoring a document for iResearch view, ignoring";
        score.reader = nullptr;
      } else if (!pkColMeta) {
        LOG_TOPIC(WARN, arangodb::Logger::FIXME) << "encountered a sub-reader without a primary key column while scoring a document for iResearch view, ignoring";
        score.reader = nullptr;
      } else {
        score.docId = *(doc->value);
        score.pkColId = pkColMeta->id;
        score.reader = &_reader;
      }
    }

   private:
    irs::attribute_store::ref<irs::document> const& _doc;
    irs::sub_reader const& _reader;
  };

  return irs::sort::scorer::make<Scorer>(segment, doc_attrs.get<irs::document>());
}

size_t Prepared::precedence(arangodb::velocypack::Slice const& slice) const {
  auto type = arangodb::iresearch::AttributeScorer::ValueType::eLast; // unsuported type equal-precedence order

  if (slice.isArray()) {
    type = arangodb::iresearch::AttributeScorer::ValueType::ARRAY;
  } else if (slice.isBoolean()) {
    type = arangodb::iresearch::AttributeScorer::ValueType::BOOLEAN;
  } else if (slice.isNull()) {
    type = arangodb::iresearch::AttributeScorer::ValueType::NIL;
  } else if (slice.isNone()) {
    type = arangodb::iresearch::AttributeScorer::ValueType::UNKNOWN;
  } else if (slice.isNumber()) {
    type = arangodb::iresearch::AttributeScorer::ValueType::NUMBER;
  } else if (slice.isObject()) {
    type = arangodb::iresearch::AttributeScorer::ValueType::OBJECT;
  } else if (slice.isString()) {
    type = arangodb::iresearch::AttributeScorer::ValueType::STRING;
  }

  // if unasigned, assign presedence in a first-come-first-serve order
  if (_order[type] == arangodb::iresearch::AttributeScorer::ValueType::eLast) {
    _order[type] = _nextOrder++;
  }

  return _order[type];
}

NS_END

NS_BEGIN(arangodb)
NS_BEGIN(iresearch)

DEFINE_SORT_TYPE_NAMED(AttributeScorer, ATTRIBUTE_SCORER_NAME);

/*static*/ irs::sort::ptr AttributeScorer::make(
  arangodb::transaction::Methods& trx,
  irs::string_ref const& attr
) {
  PTR_NAMED(AttributeScorer, ptr, trx, attr);

  return ptr;
}

AttributeScorer::AttributeScorer(
    arangodb::transaction::Methods& trx,
    irs::string_ref const& attr
): irs::sort(AttributeScorer::type()), _attr(attr), _nextOrder(0), _trx(trx) {
  std::fill_n(_order, (size_t)(ValueType::eLast), ValueType::eLast);
}

void AttributeScorer::orderNext(ValueType type) noexcept {
  if (_order[type] == ValueType::eLast) {
    _order[type] = _nextOrder++; // can never be > ValueType::eLast
  }
}

irs::sort::prepared::ptr AttributeScorer::prepare() const {
  return Prepared::make<Prepared>(_attr, _order, reverse(), _trx);
}

NS_END // iresearch
NS_END // arangodb

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------