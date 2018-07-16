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
#include "IResearch/IResearchAttributes.h"
#include "IResearch/IResearchDocument.h"
#include "IResearch/IResearchFeature.h"
#include "Logger/Logger.h"
#include "Transaction/Methods.h"
#include "velocypack/Iterator.h"
#include "velocypack/Parser.h"
#include "velocypack/Slice.h"
#include "VocBase/LocalDocumentId.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/ManagedDocumentResult.h"

NS_LOCAL

irs::string_ref const ATTRIBUTE_SCORER_NAME("@");

////////////////////////////////////////////////////////////////////////////////
/// @brief lazy-computed score from within Prepared::less(...)
////////////////////////////////////////////////////////////////////////////////
struct Score {
  void (*compute)(arangodb::iresearch::attribute::AttributePath* attr, arangodb::iresearch::attribute::Transaction* trx, Score& score);
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

  explicit Prepared(
    size_t const (&order)[arangodb::iresearch::AttributeScorer::ValueType::eLast]
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
    irs::attribute_view const& doc_attrs
  ) const override;

 private:
  irs::attribute_view::ref<arangodb::iresearch::attribute::AttributePath>::type& _attr; // a jSON array representation of the attribute path
  mutable size_t _nextOrder;
  mutable size_t _order[arangodb::iresearch::AttributeScorer::ValueType::eLast + 1]; // type precedence order, +1 for unordered/unsuported types
  bool _reverse;
  irs::attribute_view::ref<arangodb::iresearch::attribute::Transaction>::type& _trx;

  size_t precedence(arangodb::velocypack::Slice const& slice) const;
};

Prepared::Prepared(
    size_t const (&order)[arangodb::iresearch::AttributeScorer::ValueType::eLast]
): _attr(attributes().emplace<arangodb::iresearch::attribute::AttributePath>()), // mark sort as requiring attribute path
   _nextOrder(arangodb::iresearch::AttributeScorer::ValueType::eLast + 1), // past default set values, +1 for values unasigned by AttributeScorer
   _reverse(false),
   _trx(attributes().emplace<arangodb::iresearch::attribute::Transaction>()) { // mark sort as requiring transaction
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
  lhs.compute(_attr.get(), _trx.get(), const_cast<score_t&>(lhs));
  rhs.compute(_attr.get(), _trx.get(), const_cast<score_t&>(rhs));

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
    static void noop(arangodb::iresearch::attribute::AttributePath* attr, arangodb::iresearch::attribute::Transaction* trx, Score& score) {}
    static void invoke(arangodb::iresearch::attribute::AttributePath* attr, arangodb::iresearch::attribute::Transaction* trx, Score& score) {
      score.compute = &Compute::noop; // do not recompute score again

      if (!attr) {
        LOG_TOPIC(WARN, arangodb::iresearch::IResearchFeature::IRESEARCH) << "failed to find attribute path while computing document score, doc_id '" << score.docId << "'";
        return; // transaction not provided, cannot compute value
      }

      arangodb::velocypack::Slice attrPath;

      try {
        attrPath = attr->value.slice();
      } catch(...) {
        return; // failure converting builder into slice
      }

      if (!attrPath.isArray()) {
        LOG_TOPIC(WARN, arangodb::iresearch::IResearchFeature::IRESEARCH) << "failed to parse attribute path as an array while computing document score, doc_id '" << score.docId << "'";
        return; // attribute path not initialized or incorrect argument format
      }

      if (!score.reader) {
        LOG_TOPIC(WARN, arangodb::iresearch::IResearchFeature::IRESEARCH) << "failed to find reader while computing document score, doc_id '" << score.docId << "'";
        return; // score value not initialized, see errors during initialization
      }

      if (!trx) {
        LOG_TOPIC(WARN, arangodb::iresearch::IResearchFeature::IRESEARCH) << "failed to find transaction while computing document score, doc_id '" << score.docId << "'";
        return; // transaction not provided, cannot compute value
      }

      arangodb::iresearch::DocumentPrimaryKey docPk;
      irs::bytes_ref tmpRef;

      const auto* column = score.reader->column_reader(score.pkColId);

      if (!column) {
        LOG_TOPIC(WARN, arangodb::iresearch::IResearchFeature::IRESEARCH) << "failed to find primary key column while computing document score, doc_id '" << score.docId << "'";
        return; // not a valid PK column
      }

      auto values = column->values();

      if (!values(score.docId, tmpRef) || !docPk.read(tmpRef)) {
        LOG_TOPIC(WARN, arangodb::iresearch::IResearchFeature::IRESEARCH) << "failed to read document primary key while computing document score, doc_id '" << score.docId << "'";

        return; // not a valid document reference
      }

      static const std::string unknown("<unknown>");

      trx->value.addCollectionAtRuntime(docPk.cid(), unknown);

      auto* collection = trx->value.documentCollection(docPk.cid());

      if (!collection) {
        LOG_TOPIC(WARN, arangodb::iresearch::IResearchFeature::IRESEARCH) << "failed to find collection while computing document score, cid '" << docPk.cid() << "', rid '" << docPk.rid() << "'";

        return; // not a valid collection reference
      }

      arangodb::LocalDocumentId colToken(docPk.rid());
      arangodb::ManagedDocumentResult docResult;

      if (!collection->readDocument(&(trx->value), colToken, docResult)) {
        LOG_TOPIC(WARN, arangodb::iresearch::IResearchFeature::IRESEARCH) << "failed to read document while computing document score, cid '" << docPk.cid() << "', rid '" << docPk.rid() << "'";

        return; // not a valid document
      }

      arangodb::velocypack::Slice doc(docResult.vpack());

      for (arangodb::velocypack::ArrayIterator itr(attrPath); itr.valid(); ++itr) {
        auto entry = *itr;

        if (doc.isArray() && entry.isNumber<uint64_t>()) {
          doc = doc.at(entry.getNumber<uint64_t>());
          score.slice = doc;
        } else if (doc.isObject() && entry.isString()) {
          doc = doc.get(arangodb::iresearch::getStringRef(entry));
          score.slice = doc;
        } else { // array with non-numeric offset or object with non-string offset
          score.slice = arangodb::velocypack::Slice::noneSlice();
        }

        if (score.slice.isNone()) {
          break; // missing attribute, cannot evaluate path further
        }
      }
    }
  };

  score.compute = &Compute::invoke;
  score.reader = nullptr; // unset for the case where the object is reused
  score.slice = arangodb::velocypack::Slice(); // inilialize to an unsupported value
}

irs::sort::scorer::ptr Prepared::prepare_scorer(
    irs::sub_reader const& segment,
    irs::term_reader const& field,
    irs::attribute_store const& query_attrs,
    irs::attribute_view const& doc_attrs
) const {
  class Scorer: public irs::sort::scorer {
   public:
    Scorer(
        irs::sub_reader const& reader,
        irs::document const* doc
    ) : _doc(doc), _reader(reader) {
      TRI_ASSERT(doc);
    }

    virtual void score(irs::byte_type* score_buf) override {
      auto& score = *reinterpret_cast<score_t*>(score_buf);
      auto* pkColMeta = _reader.column(arangodb::iresearch::DocumentPrimaryKey::PK());

      if (!_doc) {
        LOG_TOPIC(WARN, arangodb::iresearch::IResearchFeature::IRESEARCH) << "encountered a document without a doc_id value while scoring a document for iResearch view, ignoring";
        score.reader = nullptr;
      } else if (!pkColMeta) {
        LOG_TOPIC(WARN, arangodb::iresearch::IResearchFeature::IRESEARCH) << "encountered a sub-reader without a primary key column while scoring a document for iResearch view, ignoring";
        score.reader = nullptr;
      } else {
        score.docId = _doc->value;
        score.pkColId = pkColMeta->id;
        score.reader = &_reader;
      }
    }

   private:
    irs::document const* _doc;
    irs::sub_reader const& _reader;
  };

  return irs::sort::scorer::make<Scorer>(segment, doc_attrs.get<irs::document>().get());
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

void setArangoDBTypeSortOrder(arangodb::iresearch::AttributeScorer& scorer) {
  // ArangoDB default type sort order:
  // null < bool < number < string < array/list < object/document
  scorer.orderNext(arangodb::iresearch::AttributeScorer::ValueType::NIL);
  scorer.orderNext(arangodb::iresearch::AttributeScorer::ValueType::BOOLEAN);
  scorer.orderNext(arangodb::iresearch::AttributeScorer::ValueType::NUMBER);
  scorer.orderNext(arangodb::iresearch::AttributeScorer::ValueType::STRING);
  scorer.orderNext(arangodb::iresearch::AttributeScorer::ValueType::ARRAY);
  scorer.orderNext(arangodb::iresearch::AttributeScorer::ValueType::OBJECT);
}

NS_END

NS_BEGIN(arangodb)
NS_BEGIN(iresearch)

DEFINE_SORT_TYPE_NAMED(AttributeScorer, ATTRIBUTE_SCORER_NAME);
REGISTER_SCORER_JSON(AttributeScorer, AttributeScorer::make);
REGISTER_SCORER_TEXT(AttributeScorer, AttributeScorer::make);

/*static*/ irs::sort::ptr AttributeScorer::make(
    std::vector<irs::stored_attribute::ptr>& storedAttrBuf,
    bool arangodbTypeOrder /*= false*/
) {
  PTR_NAMED(AttributeScorer, ptr);

  #ifdef ARANGODB_ENABLE_MAINTAINER_MODE
    auto* scorer = dynamic_cast<AttributeScorer*>(ptr.get());
  #else
    auto* scorer = static_cast<AttributeScorer*>(ptr.get());
  #endif

  if (scorer) {
    scorer->_storedAttrBuf = &storedAttrBuf;

    if (arangodbTypeOrder) {
      setArangoDBTypeSortOrder(*scorer);
    }
  }

  return ptr;
}

// FIXME TODO split into separate functions for text and jSON formats
/*static*/ irs::sort::ptr AttributeScorer::make(const irs::string_ref& args) {
  try {
    PTR_NAMED(AttributeScorer, ptr);

    #ifdef ARANGODB_ENABLE_MAINTAINER_MODE
      auto* scorer = dynamic_cast<AttributeScorer*>(ptr.get());
    #else
      auto* scorer = static_cast<AttributeScorer*>(ptr.get());
    #endif

    if (!scorer) {
      LOG_TOPIC(WARN, arangodb::iresearch::IResearchFeature::IRESEARCH) << "Failed to create AttributeScorer instance";

      return nullptr; // malloc failure
    }

    if (args.null()) {
      setArangoDBTypeSortOrder(*scorer);

      return ptr;
    }

    static std::unordered_map<irs::string_ref, ValueType> const valueTypes = {
      { "array",   ValueType::ARRAY   },
      { "bool", ValueType::BOOLEAN },
      { "boolean", ValueType::BOOLEAN },
      { "null",    ValueType::NIL     },
      { "numeric", ValueType::NUMBER  },
      { "object",  ValueType::OBJECT  },
      { "string",  ValueType::STRING  },
      { "unknown", ValueType::UNKNOWN },
    };

    // try parsing as a string containing a single type value
    {
      auto typeItr = valueTypes.find(args);

      if (typeItr != valueTypes.end()) {
        scorer->orderNext(typeItr->second);

        return ptr;
      }
    }

    // velocypack::Parser::fromJson(...) will throw exception on parse error
    auto json = arangodb::velocypack::Parser::fromJson(args.c_str(), args.size());
    auto slice = json ? json->slice() : arangodb::velocypack::Slice();

    if (!slice.isArray()) {
      LOG_TOPIC(WARN, arangodb::iresearch::IResearchFeature::IRESEARCH) << "Failed to parse AttributeScorer argument as an array";

      return nullptr; // incorrect argument format
    }

    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr, ++i) {
      auto entry = *itr;

      if (!entry.isString()) {
        LOG_TOPIC(WARN, arangodb::iresearch::IResearchFeature::IRESEARCH) << "Failed to parse AttributeScorer argument [" << i << "] as a string";

        return nullptr;
      }

      auto type = getStringRef(entry);
      auto typeItr = valueTypes.find(type);

      if (typeItr == valueTypes.end()) {
        LOG_TOPIC(WARN, arangodb::iresearch::IResearchFeature::IRESEARCH) << "Failed to parse AttributeScorer argument [" << type << "] as a supported enum value, not one of: 'array', 'boolean', 'null', 'numeric', 'object', 'string', 'unknown'";

        return nullptr;
      }

      scorer->orderNext(typeItr->second);
    }

    return ptr;
  } catch (...) {
    LOG_TOPIC(WARN, arangodb::iresearch::IResearchFeature::IRESEARCH) << "Caught error while constructing AttributeScorer from jSON arguments: " << args.c_str();
    IR_LOG_EXCEPTION();
  }

  return nullptr;
}

AttributeScorer::AttributeScorer()
  : irs::sort(AttributeScorer::type()),
    _nextOrder(0),
    _storedAttrBuf(nullptr) {
  std::fill_n(_order, (size_t)(ValueType::eLast), ValueType::eLast);
}

AttributeScorer& AttributeScorer::attributeNext(uint64_t offset) {
  _attribute.emplace_back(AttributeItem{
    offset,
    std::numeric_limits<uint64_t>::max() // offset into jSON array
  });

  return *this;
}

AttributeScorer& AttributeScorer::attributeNext(
    irs::string_ref const& attribute
) {
  auto offset = _buf.size();

  _buf.append(attribute.c_str(), attribute.size());
  _attribute.emplace_back(AttributeItem{offset, attribute.size()});

  return *this;
}

AttributeScorer& AttributeScorer::orderNext(ValueType type) noexcept {
  if (_order[type] == ValueType::eLast) {
    _order[type] = _nextOrder++; // can never be > ValueType::eLast
  }

  return *this;
}

irs::sort::prepared::ptr AttributeScorer::prepare() const {
  auto prepared = Prepared::make<Prepared>(_order );

  if (!prepared || _attribute.empty()) {
    return prepared; // attr should be set on Prepared instance
  }

  auto& attrs = prepared->attributes();
  auto* attr = attrs.get<arangodb::iresearch::attribute::AttributePath>();

  if (!attr) {
    return nullptr; // algorithm error, Prepared shuld be requesting attribute
  }

  if (!_storedAttrBuf) {
    return nullptr; // stored attribute buffer required for adding new attributes
  }

  try {
    auto storedAttr = arangodb::iresearch::attribute::AttributePath::make();

    #ifdef ARANGODB_ENABLE_MAINTAINER_MODE
      auto* storedAttrPath = dynamic_cast<arangodb::iresearch::attribute::AttributePath*>(storedAttr.get());
    #else
      auto* storedAttrPath = static_cast<arangodb::iresearch::attribute::AttributePath*>(storedAttr.get());
    #endif

    if (!storedAttrPath) {
      return nullptr; // malloc failure
    }

    auto& builder = storedAttrPath->value();

    builder.openArray();

    for (auto& entry: _attribute) {
      if (std::numeric_limits<size_t>::max() == entry._size) { // offset into jSON array
         builder.add(arangodb::velocypack::Value(entry._offset));
      } else {
        builder.add(arangodb::iresearch::toValuePair(
          &(_buf[entry._offset]),
          entry._size
        ));
      }
    }

    builder.close();
    _storedAttrBuf->emplace_back(std::move(storedAttr));
    *attr = std::move(storedAttrPath);
  } catch (...) {
    return nullptr; // jSON build failure
  }

  return prepared;
}

NS_END // iresearch
NS_END // arangodb

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------
