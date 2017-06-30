//////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2017 EMC Corporation
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
/// Copyright holder is EMC Corporation
///
/// @author Andrey Abramov
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#include "IResearchDocument.h"
#include "IResearchViewMeta.h"

#include "Aql/AstNode.h"
#include "Aql/Function.h"
#include "Aql/SortCondition.h"
#include "Logger/Logger.h"
#include "Logger/LogMacros.h"

#include "analysis/token_attributes.hpp"
#include "search/boolean_filter.hpp"
#include "search/scorers.hpp"
#include "search/term_filter.hpp"
#include "search/range_filter.hpp"
#include "search/granular_range_filter.hpp"

#include "utils/log.hpp"
#include "utils/numeric_utils.hpp"

#define Swap8Bytes(val) \
 ( (((val) >> 56) & UINT64_C(0x00000000000000FF)) | (((val) >> 40) & UINT64_C(0x000000000000FF00)) | \
   (((val) >> 24) & UINT64_C(0x0000000000FF0000)) | (((val) >>  8) & UINT64_C(0x00000000FF000000)) | \
   (((val) <<  8) & UINT64_C(0x000000FF00000000)) | (((val) << 24) & UINT64_C(0x0000FF0000000000)) | \
   (((val) << 40) & UINT64_C(0x00FF000000000000)) | (((val) << 56) & UINT64_C(0xFF00000000000000)) )

NS_LOCAL

irs::string_ref const ATTRIBUTE_SCORER_NAME("@");

////////////////////////////////////////////////////////////////////////////////
/// @brief sort object based on attribute value
////////////////////////////////////////////////////////////////////////////////
class AttributeScorer: public irs::sort {
 public:
  DECLARE_SORT_TYPE();

  // for use with irs::order::add<T>(...) and default args (static build)
  DECLARE_FACTORY_DEFAULT(arangodb::transaction::Methods& trx);

  enum ValueType { ARRAY, BOOLEAN, NIL, NUMBER, OBJECT, STRING, UNKNOWN, eLast};

  explicit AttributeScorer(arangodb::transaction::Methods& trx);

  void orderNext(ValueType type) noexcept;
  virtual sort::prepared::ptr prepare() const override;

 private:
  // lazy-computed score from within Prepared::less(...)
  struct Score {
    void (*compute)(arangodb::transaction::Methods& trx, Score& score);
    irs::doc_id_t const* document;
    irs::sub_reader const* reader;
    arangodb::velocypack::Slice slice;
  };
  class Prepared: public irs::sort::prepared_base<Score> {
   public:
    DECLARE_FACTORY(Prepared);

    Prepared(
      std::string const& attr,
      size_t const (&order)[ValueType::eLast],
      bool reverse,
      arangodb::transaction::Methods& trx
    );

    virtual void add(score_t& dst, const score_t& src) const override;
    virtual irs::flags const& features() const override;
    virtual bool less(const score_t& lhs, const score_t& rhs) const override;
    virtual collector::ptr prepare_collector() const override;
    virtual void prepare_score(score_t& score) const override;
    virtual scorer::ptr prepare_scorer(
      irs::sub_reader const& segment,
      irs::term_reader const& field,
      irs::attribute_store const& query_attrs,
      irs::attribute_store const& doc_attrs
    ) const override;

   private:
    std::string _attr;
    mutable size_t _nextOrder;
    mutable size_t _order[ValueType::eLast + 1]; // type precedence order, +1 for unordered/unsuported types
    bool _reverse;
    arangodb::transaction::Methods& _trx;

    size_t precedence(arangodb::velocypack::Slice const& slice) const;
  };

  std::string _attr;
  size_t _nextOrder;
  size_t _order[ValueType::eLast]; // type precedence order
  arangodb::transaction::Methods& _trx;
};

AttributeScorer::Prepared::Prepared(
    std::string const& attr,
    size_t const (&order)[ValueType::eLast],
    bool reverse,
    arangodb::transaction::Methods& trx
): _attr(attr),
   _nextOrder(ValueType::eLast + 1), // past default set values, +1 for values unasigned by AttributeScorer
   _reverse(reverse),
   _trx(trx) {
  std::memcpy(_order, order, sizeof(order));
}

void AttributeScorer::Prepared::add(score_t& dst, const score_t& src) const {
  // NOOP
}

irs::flags const& AttributeScorer::Prepared::features() const {
  return irs::flags::empty_instance();
}

bool AttributeScorer::Prepared::less(
    const score_t& lhs,
    const score_t& rhs
) const {
  lhs.compute(_trx, const_cast<score_t&>(lhs));
  rhs.compute(_trx, const_cast<score_t&>(rhs));

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

irs::sort::collector::ptr AttributeScorer::Prepared::prepare_collector() const {
  return nullptr;
}

void AttributeScorer::Prepared::prepare_score(score_t& score) const {
  struct Compute {
    static void noop(arangodb::transaction::Methods& trx, Score& score) {}
    static void invoke(arangodb::transaction::Methods& trx, Score& score) {
      if (score.document && score.reader) {
        // FIXME TODO
        // read PK
        // read JSON from transaction
        //score.slice = find attribute in the document
      }

      score.compute = &Compute::noop; // do not recompute score again
    }
  };

  score.compute = &Compute::invoke;
}

irs::sort::scorer::ptr AttributeScorer::Prepared::prepare_scorer(
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

      score.document = doc ? doc->value : nullptr;
      score.reader = &_reader;
    }

   private:
    irs::attribute_store::ref<irs::document> const& _doc;
    irs::sub_reader const& _reader;
  };

  return scorer::make<Scorer>(segment, doc_attrs.get<irs::document>());
}

size_t AttributeScorer::Prepared::precedence(
    arangodb::velocypack::Slice const& slice
) const {
  ValueType type = ValueType::eLast; // unsuported type equal-precedence order

  if (slice.isArray()) {
    type = ValueType::ARRAY;
  } else if (slice.isBoolean()) {
    type = ValueType::BOOLEAN;
  } else if (slice.isNull()) {
    type = ValueType::NIL;
  } else if (slice.isNone()) {
    type = ValueType::UNKNOWN;
  } else if (slice.isObject()) {
    type = ValueType::OBJECT;
  } else if (slice.isString()) {
    type = ValueType::STRING;
  }

  // if unasigned, assign presedence in a first-come-first-serve order
  if (_order[type] == ValueType::eLast) {
    _order[type] = _nextOrder++;
  }

  return _order[type];
}

DEFINE_SORT_TYPE_NAMED(AttributeScorer, ATTRIBUTE_SCORER_NAME);

/*static*/ irs::sort::ptr AttributeScorer::make(
  arangodb::transaction::Methods& trx
) {
  PTR_NAMED(AttributeScorer, ptr, trx);

  return ptr;
}

AttributeScorer::AttributeScorer(arangodb::transaction::Methods& trx)
  : irs::sort(AttributeScorer::type()), _nextOrder(0), _trx(trx) {
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

////////////////////////////////////////////////////////////////////////////////
/// @brief the delimiter used to separate jSON nesting levels when generating
///        flat iResearch field names
////////////////////////////////////////////////////////////////////////////////
std::string const NESTING_LEVEL_DELIMITER(".");

////////////////////////////////////////////////////////////////////////////////
/// @brief the prefix used to denote start of jSON list offset when generating
///        flat iResearch field names
////////////////////////////////////////////////////////////////////////////////
std::string const NESTING_LIST_OFFSET_PREFIX("[");

////////////////////////////////////////////////////////////////////////////////
/// @brief the suffix used to denote end of jSON list offset when generating
///        flat iResearch field names
////////////////////////////////////////////////////////////////////////////////
std::string const NESTING_LIST_OFFSET_SUFFIX("]");

irs::string_ref const CID_FIELD("@_CID");
irs::string_ref const RID_FIELD("@_REV");
irs::string_ref const PK_COLUMN("@_PK");

inline irs::bytes_ref toBytesRef(uint64_t const& value) {
  return irs::bytes_ref(
    reinterpret_cast<irs::byte_type const*>(&value),
    sizeof(value)
  );
}

inline void ensureLittleEndian(uint64_t& value) {
  if (irs::numeric_utils::is_big_endian()) {
    value = Swap8Bytes(value);
  }
}

inline irs::bytes_ref toBytesRefLE(uint64_t& value) {
  ensureLittleEndian(value);
  return toBytesRef(value);
}

// wrapper for use objects with the IResearch unbounded_object_pool
template<typename T>
struct AnyFactory {
  typedef std::shared_ptr<T> ptr;

  template<typename... Args>
  static ptr make(Args&&... args) {
    return std::make_shared<T>(std::forward<Args>(args)...);
  }
}; // AnyFactory

const size_t DEFAULT_POOL_SIZE = 8; // arbitrary value
irs::unbounded_object_pool<AnyFactory<std::string>> BufferPool(DEFAULT_POOL_SIZE);
irs::unbounded_object_pool<AnyFactory<irs::string_token_stream>> StringStreamPool(DEFAULT_POOL_SIZE);
irs::unbounded_object_pool<AnyFactory<irs::null_token_stream>> NullStreamPool(DEFAULT_POOL_SIZE);
irs::unbounded_object_pool<AnyFactory<irs::boolean_token_stream>> BoolStreamPool(DEFAULT_POOL_SIZE);
irs::unbounded_object_pool<AnyFactory<irs::numeric_token_stream>> NumericStreamPool(DEFAULT_POOL_SIZE);
irs::flags NumericStreamFeatures{ irs::granularity_prefix::type() };

// appends the specified 'value' to 'out'
inline void append(std::string& out, size_t value) {
  auto const size = out.size(); // intial size
  out.resize(size + 21); // enough to hold all numbers up to 64-bits
  auto const written = sprintf(&out[size], IR_SIZE_T_SPECIFIER, value);
  out.resize(size + written);
}

inline bool canHandleValue(
    VPackSlice const& value,
    arangodb::iresearch::IResearchLinkMeta const& context
) noexcept {
  switch (value.type()) {
    case VPackValueType::None:
    case VPackValueType::Illegal:
      return false;
    case VPackValueType::Null:
    case VPackValueType::Bool:
    case VPackValueType::Array:
    case VPackValueType::Object:
    case VPackValueType::Double:
      return true;
    case VPackValueType::UTCDate:
    case VPackValueType::External:
    case VPackValueType::MinKey:
    case VPackValueType::MaxKey:
      return false;
    case VPackValueType::Int:
    case VPackValueType::UInt:
    case VPackValueType::SmallInt:
      return true;
    case VPackValueType::String:
      return !context._tokenizers.empty();
    case VPackValueType::Binary:
    case VPackValueType::BCD:
    case VPackValueType::Custom:
    default:
      return false;
  }
}

// returns 'context' in case if can't find the specified 'field'
inline arangodb::iresearch::IResearchLinkMeta const* findMeta(
    irs::string_ref const& key,
    arangodb::iresearch::IResearchLinkMeta const* context
) {
  TRI_ASSERT(context);

  auto const* meta = context->_fields.findPtr(key);
  return meta ? meta->get() : context;
}

inline bool inObjectFiltered(
    std::string& buffer,
    arangodb::iresearch::IResearchLinkMeta const*& context,
    arangodb::iresearch::IteratorValue const& value
) {
  auto const key = arangodb::iresearch::getStringRef(value.key);

  auto const* meta = findMeta(key, context);

  if (meta == context) {
    return false;
  }

  buffer.append(key.c_str(), key.size());
  context = meta;

  return canHandleValue(value.value, *context);
}

inline bool inObject(
    std::string& buffer,
    arangodb::iresearch::IResearchLinkMeta const*& context,
    arangodb::iresearch::IteratorValue const& value
) {
  auto const key = arangodb::iresearch::getStringRef(value.key);

  buffer.append(key.c_str(), key.size());
  context = findMeta(key, context);

  return canHandleValue(value.value, *context);
}

inline bool inArrayOrdered(
    std::string& buffer,
    arangodb::iresearch::IResearchLinkMeta const*& context,
    arangodb::iresearch::IteratorValue const& value
) {
  buffer += NESTING_LIST_OFFSET_PREFIX;
  append(buffer, value.pos);
  buffer += NESTING_LIST_OFFSET_SUFFIX;

  return canHandleValue(value.value, *context);
}

inline bool inArray(
    std::string& /*buffer*/,
    arangodb::iresearch::IResearchLinkMeta const*& context,
    arangodb::iresearch::IteratorValue const& value
) noexcept {
  return canHandleValue(value.value, *context);
}

typedef bool(*Filter)(
  std::string& buffer,
  arangodb::iresearch::IResearchLinkMeta const*& context,
  arangodb::iresearch::IteratorValue const& value
);

Filter const valueAcceptors[] = {
  &inObjectFiltered, // type == Object, nestListValues == false, includeAllValues == false
  &inObject,         // type == Object, nestListValues == false, includeAllValues == true
  &inObjectFiltered, // type == Object, nestListValues == true , includeAllValues == false
  &inObject,         // type == Object, nestListValues == true , includeAllValues == true
  &inArray,          // type == Array , nestListValues == flase, includeAllValues == false
  &inArray,          // type == Array , nestListValues == flase, includeAllValues == true
  &inArrayOrdered,   // type == Array , nestListValues == true,  includeAllValues == false
  &inArrayOrdered    // type == Array , nestListValues == true,  includeAllValues == true
};

inline Filter getFilter(
  VPackSlice value,
  arangodb::iresearch::IResearchLinkMeta const& meta
) noexcept {
  TRI_ASSERT(arangodb::iresearch::isArrayOrObject(value));

  return valueAcceptors[
    4 * value.isArray()
      + 2 * meta._nestListValues
      + meta._includeAllFields
   ];
}

typedef arangodb::iresearch::IResearchLinkMeta::TokenizerPool const* TokenizerPoolPtr;

void mangleNull(std::string& name) {
  static irs::string_ref const SUFFIX("\0_n", 3);
  name.append(SUFFIX.c_str(), SUFFIX.size());
}

void setNullValue(
    VPackSlice const& value,
    std::string& name,
    arangodb::iresearch::Field& field
) {
  TRI_ASSERT(value.isNull());

  // mangle name
  mangleNull(name);

  // init stream
  auto stream = NullStreamPool.emplace();
  stream->reset();

  // set field properties
  field._name = name;
  field._tokenizer =  stream;
  field._features = &irs::flags::empty_instance();
}

void mangleBool(std::string& name) {
  static irs::string_ref const SUFFIX("\0_b", 3);
  name.append(SUFFIX.c_str(), SUFFIX.size());
}

void setBoolValue(
    VPackSlice const& value,
    std::string& name,
    arangodb::iresearch::Field& field
) {
  TRI_ASSERT(value.isBool());

  // mangle name
  mangleBool(name);

  // init stream
  auto stream = BoolStreamPool.emplace();
  stream->reset(value.getBool());

  // set field properties
  field._name = name;
  field._tokenizer =  stream;
  field._features = &irs::flags::empty_instance();
}

void mangleNumeric(std::string& name) {
  static irs::string_ref const SUFFIX("\0_d", 3);
  name.append(SUFFIX.c_str(), SUFFIX.size());
}

void setNumericValue(
    VPackSlice const& value,
    std::string& name,
    arangodb::iresearch::Field& field
) {
  TRI_ASSERT(value.isNumber());

  // mangle name
  mangleNumeric(name);

  // init stream
  auto stream = NumericStreamPool.emplace();
  stream->reset(value.getNumber<double>());

  // set field properties
  field._name = name;
  field._tokenizer =  stream;
  field._features = &NumericStreamFeatures;
}

void mangleStringField(
    std::string& name,
    TokenizerPoolPtr pool
) {
  name += '\0';
  name += pool->name();
  name += pool->args();
}

void unmangleStringField(
    std::string& name,
    TokenizerPoolPtr pool
) {
  // +1 for preceding '\0'
  auto const suffixSize = 1 + pool->name().size() + pool->args().size();

  TRI_ASSERT(name.size() >= suffixSize);
  name.resize(name.size() - suffixSize);
}

bool setStringValue(
    VPackSlice const& value,
    std::string& name,
    arangodb::iresearch::Field& field,
    TokenizerPoolPtr pool
) {
  TRI_ASSERT(value.isString());

  // it's important to unconditionally mangle name
  // since we unconditionally unmangle it in 'next'
  mangleStringField(name, pool);

  // init stream
  auto analyzer = pool->tokenizer();

  if (!analyzer) {
    LOG_TOPIC(WARN, arangodb::Logger::FIXME)
      << "got nullptr from tokenizer factory, name='"
      << pool->name() << "', args='"
      << pool->args() << "'";
    return false;
  }

  // init stream
  analyzer->reset(arangodb::iresearch::getStringRef(value));

  // set field properties
  field._name = name;
  field._tokenizer =  analyzer;
  field._features = &(pool->features());

  return true;
}

void setIdValue(
    uint64_t& value,
    irs::token_stream& tokenizer
) {
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  auto& sstream = dynamic_cast<irs::string_token_stream&>(tokenizer);
#else
  auto& sstream = static_cast<irs::string_token_stream&>(tokenizer);
#endif

  sstream.reset(toBytesRefLE(value));
}

// ----------------------------------------------------------------------------
// --SECTION--                                        FilerFactory dependencies
// ----------------------------------------------------------------------------

template<bool Preorder, typename Visitor>
bool visit(arangodb::aql::AstNode const& root, Visitor visitor) {
  if (Preorder && !visitor(root)) {
    return false;
  }

  if (Preorder && !visitor(root)) {
    return false;
  }

  size_t const n = root.numMembers();

  for (size_t i = 0; i < n; ++i) {
    auto const* member = root.getMemberUnchecked(i);
    TRI_ASSERT(member);

    if (!visit<Preorder>(*member, visitor)) {
      return false;
    }
  }

  if (!Preorder && !visitor(root)) {
    return false;
  }

  return true;
};

inline irs::bytes_ref toBytesRef(arangodb::aql::AstNode const& node) {
  TRI_ASSERT(arangodb::aql::NODE_TYPE_VALUE == node.type
             && arangodb::aql::VALUE_TYPE_STRING == node.value.type);

  return irs::bytes_ref(
    reinterpret_cast<irs::byte_type const*>(node.getStringValue()),
    node.getStringLength()
  );
}

inline bool checkAttributeAccess(arangodb::aql::AstNode const& node) {
  return arangodb::aql::NODE_TYPE_ATTRIBUTE_ACCESS == node.type;
}

// generates field name from the specified 'node' and value 'type'
std::string nameFromAttributeAccess(
    arangodb::aql::AstNode const& node,
    arangodb::aql::AstNodeValueType type
) {
  TRI_ASSERT(arangodb::aql::NODE_TYPE_ATTRIBUTE_ACCESS == node.type);

  std::string name;

  auto visitor = [&name](arangodb::aql::AstNode const& node) mutable {
    if (arangodb::aql::NODE_TYPE_ATTRIBUTE_ACCESS == node.type) {
      name.append(node.getStringValue(), node.getStringLength());
      name += '.';
    }
    return true;
  };

  visit<false>(node, visitor);
  name.pop_back(); // remove extra '.'

  switch (type) {
    case arangodb::aql::VALUE_TYPE_NULL:
      mangleNull(name);
      break;
    case arangodb::aql::VALUE_TYPE_BOOL:
      mangleBool(name);
      break;
    case arangodb::aql::VALUE_TYPE_INT:
    case arangodb::aql::VALUE_TYPE_DOUBLE:
      mangleNumeric(name);
      break;
    case arangodb::aql::VALUE_TYPE_STRING:
      //mangleStringField(name);
      break;
  }

  return name;
}

bool processSubnode(
  irs::boolean_filter* filter,
  arangodb::aql::AstNode const& node
);

template<typename Filter>
bool fromGroup(
  irs::boolean_filter* filter,
  arangodb::aql::AstNode const& node
);

irs::by_term& byTerm(
    irs::by_term& filter,
    arangodb::aql::AstNode const& attributeNode,
    arangodb::aql::AstNode const& valueNode
) {
  switch (valueNode.value.type) {
    case arangodb::aql::VALUE_TYPE_NULL:
      filter.term(irs::null_token_stream::value_null());
      break;
    case arangodb::aql::VALUE_TYPE_BOOL:
      filter.term(valueNode.getBoolValue()
        ? irs::boolean_token_stream::value_true()
        : irs::boolean_token_stream::value_false());
      break;
    case arangodb::aql::VALUE_TYPE_INT:
    case arangodb::aql::VALUE_TYPE_DOUBLE: {
      irs::numeric_token_stream stream;
      irs::term_attribute const* term = stream.attributes().get<irs::term_attribute>().get();
      TRI_ASSERT(term);
      stream.reset(valueNode.getDoubleValue());
      stream.next();

      filter.term(term->value());
     } break;
    case arangodb::aql::VALUE_TYPE_STRING:
      filter.term(toBytesRef(valueNode));
      break;
  }

  filter.field(
    nameFromAttributeAccess(attributeNode, valueNode.value.type)
  );

  return filter;
}

template<irs::Bound Bound, bool Include>
bool byRange(
    irs::boolean_filter* filter,
    arangodb::aql::AstNode const& attributeNode,
    arangodb::aql::AstNode const& valueNode
) {
  switch (valueNode.value.type) {
    case arangodb::aql::VALUE_TYPE_NULL: {
      if (filter) {
        auto& range = filter->add<irs::by_range>();

        range.field(nameFromAttributeAccess(attributeNode, valueNode.value.type));
        range.term<Bound>(irs::null_token_stream::value_null());
        range.include<Bound>(Include);;
      }

      return true;
    }
    case arangodb::aql::VALUE_TYPE_BOOL: {
      if (filter) {
        auto& range = filter->add<irs::by_range>();
        auto const& value = valueNode.getBoolValue()
                          ? irs::boolean_token_stream::value_true()
                          : irs::boolean_token_stream::value_false();

        range.field(nameFromAttributeAccess(attributeNode, valueNode.value.type));
        range.term<Bound>(value);
        range.include<Bound>(Include);
      }

      return true;
    }
    case arangodb::aql::VALUE_TYPE_INT:
    case arangodb::aql::VALUE_TYPE_DOUBLE: {
      if (filter) {
        auto& range = filter->add<irs::by_granular_range>();
        irs::numeric_token_stream stream;

        stream.reset(valueNode.getDoubleValue());
        range.field(nameFromAttributeAccess(attributeNode, valueNode.value.type));
        range.insert<Bound>(stream);
        range.include<Bound>(Include);
      }

      return true;
    }
    case arangodb::aql::VALUE_TYPE_STRING: {
      if (filter) {
        auto& range = filter->add<irs::by_range>();

        range.field(nameFromAttributeAccess(attributeNode, valueNode.value.type));
        range.term<Bound>(toBytesRef(valueNode));
        range.include<Bound>(Include);
      }

      return true;
    }
  }

  return false;
}

template<irs::Bound Bound, bool Include>
bool fromInterval(
    irs::boolean_filter* filter,
    arangodb::aql::AstNode const& node
) {
  TRI_ASSERT(
    2 == node.numMembers()
    && ((arangodb::aql::NODE_TYPE_OPERATOR_BINARY_LT == node.type && irs::Bound::MAX == Bound && !Include)
     || (arangodb::aql::NODE_TYPE_OPERATOR_BINARY_LE == node.type && irs::Bound::MAX == Bound && Include)
     || (arangodb::aql::NODE_TYPE_OPERATOR_BINARY_GT == node.type && irs::Bound::MIN == Bound && !Include)
     || (arangodb::aql::NODE_TYPE_OPERATOR_BINARY_GE == node.type && irs::Bound::MIN == Bound && Include))
  );

  auto const* attributeNode = node.getMemberUnchecked(0);
  TRI_ASSERT(attributeNode);

  if (!checkAttributeAccess(*attributeNode)) {
    return false;
  }

  auto const* valueNode = node.getMemberUnchecked(1);
  TRI_ASSERT(attributeNode);

  if (!valueNode->isConstant()) {
    return false;
  }

  return byRange<Bound, Include>(filter, *attributeNode, *valueNode);
}

bool fromBinaryEq(
    irs::boolean_filter* filter,
    arangodb::aql::AstNode const& node
) {
  TRI_ASSERT(
    2 == node.numMembers()
    && (arangodb::aql::NODE_TYPE_OPERATOR_BINARY_EQ == node.type
    || arangodb::aql::NODE_TYPE_OPERATOR_BINARY_NE == node.type));

  auto* attributeNode = node.getMemberUnchecked(0);
  TRI_ASSERT(attributeNode);

  if (!checkAttributeAccess(*attributeNode)) {
    return false;
  }

  auto* valueNode = node.getMemberUnchecked(1);
  TRI_ASSERT(attributeNode);

  if (!valueNode->isConstant()) {
    return false;
  }

  if (filter) {
    auto& termFilter = arangodb::aql::NODE_TYPE_OPERATOR_BINARY_NE == node.type
                     ? filter->add<irs::Not>().filter<irs::by_term>()
                     : filter->add<irs::by_term>();

    byTerm(termFilter, *attributeNode, *valueNode);
  }

  return true;
}

bool fromRange(
    irs::boolean_filter* filter,
    arangodb::aql::AstNode const& node
) {
  TRI_ASSERT(2 == node.numMembers() && arangodb::aql::NODE_TYPE_RANGE == node.type);

  auto* min = node.getMemberUnchecked(0);
  TRI_ASSERT(min);
  auto* max = node.getMemberUnchecked(1);
  TRI_ASSERT(max);

  if (min->value.type != max->value.type) {
    // type mismatch
    return false;
  }

  //FIXME TODO
  return false;
}

bool fromArrayIn(
    irs::boolean_filter* filter,
    arangodb::aql::AstNode const& node
) {
  TRI_ASSERT(
    2 == node.numMembers()
    && (arangodb::aql::NODE_TYPE_OPERATOR_BINARY_IN == node.type
    || arangodb::aql::NODE_TYPE_OPERATOR_BINARY_NIN == node.type));

  auto* attributeNode = node.getMemberUnchecked(0);
  TRI_ASSERT(attributeNode);

  if (!checkAttributeAccess(*attributeNode)) {
    return false;
  }

  auto* valueNode = node.getMemberUnchecked(1);
  TRI_ASSERT(valueNode);

  size_t const n = valueNode->numMembers();

  if (!n) {
    // nothing to do
    return true;
  }

  if (filter) {
    filter = arangodb::aql::NODE_TYPE_OPERATOR_BINARY_NIN == node.type
           ? &static_cast<irs::boolean_filter&>(filter->add<irs::Not>().filter<irs::And>())
           : &static_cast<irs::boolean_filter&>(filter->add<irs::Or>());
  }

  for (size_t i = 0; i < n; ++i) {
    auto const* elementNode = valueNode->getMemberUnchecked(i);
    TRI_ASSERT(valueNode);

    if (elementNode->type != arangodb::aql::NODE_TYPE_VALUE
        || !elementNode->isConstant()) {
      return false;
    }

    if (filter) {
      byTerm(filter->add<irs::by_term>(), *attributeNode, *elementNode);
    }
  }

  return true;
}

bool fromValue(
    irs::boolean_filter* filter,
    arangodb::aql::AstNode const& node
) {
  TRI_ASSERT(arangodb::aql::NODE_TYPE_VALUE == node.type
             || arangodb::aql::NODE_TYPE_ARRAY == node.type);

  if (!filter) {
    return true; // nothing more to validate
  } else if (node.isTrue()) {
    filter->add<irs::all>();
  } else {
    // FIXME empty query
    filter->add<irs::Not>();
  }

  return true;
}

bool fromNegation(
    irs::boolean_filter* filter,
    arangodb::aql::AstNode const& node
) {
  TRI_ASSERT(
    1 == node.numMembers()
    && arangodb::aql::NODE_TYPE_OPERATOR_UNARY_NOT == node.type
  );

  auto const* member = node.getMemberUnchecked(0);
  TRI_ASSERT(member);

  if (filter) {
    filter = &filter->add<irs::Not>().filter<irs::And>();
  }

  return processSubnode(filter, *member);
}

bool fromBinaryAnd(
    irs::boolean_filter* filter,
    arangodb::aql::AstNode const& node
) {
  TRI_ASSERT(arangodb::aql::NODE_TYPE_OPERATOR_BINARY_AND == node.type);
  TRI_ASSERT(2 == node.numMembers());

  auto* lhs = node.getMemberUnchecked(0);
  TRI_ASSERT(lhs);
  auto* rhs = node.getMemberUnchecked(1);
  TRI_ASSERT(rhs);

  bool const includeMin = arangodb::aql::NODE_TYPE_OPERATOR_BINARY_GE == lhs->type;
  bool const includeMax = arangodb::aql::NODE_TYPE_OPERATOR_BINARY_LE == lhs->type;

  if ((includeMin || arangodb::aql::NODE_TYPE_OPERATOR_BINARY_GT == lhs->type)
      && (includeMax || arangodb::aql::NODE_TYPE_OPERATOR_BINARY_LT == lhs->type)) {
    TRI_ASSERT(2 == lhs->numMembers());
    auto const* lhsAttribute = lhs->getMemberUnchecked(0);
    auto const* lhsValue = lhs->getMemberUnchecked(1);
    TRI_ASSERT(2 == rhs->numMembers());
    auto const* rhsAttribute = rhs->getMemberUnchecked(0);
    auto const* rhsValue = rhs->getMemberUnchecked(1);

    if (lhsValue->value.type == rhsValue->value.type) {

    }
    // FIXME range case
  }

  // treat as ordinal 'And'
  return fromGroup<irs::And>(filter, node);
}

template<typename Filter>
bool fromGroup(
    irs::boolean_filter* filter,
    arangodb::aql::AstNode const& node
) {
  TRI_ASSERT(arangodb::aql::NODE_TYPE_OPERATOR_BINARY_AND == node.type
   || arangodb::aql::NODE_TYPE_OPERATOR_BINARY_OR == node.type
   || arangodb::aql::NODE_TYPE_OPERATOR_NARY_AND == node.type
   || arangodb::aql::NODE_TYPE_OPERATOR_NARY_OR == node.type);

  size_t const n = node.numMembers();

  if (!n) {
    // nothing to do
    return true;
  }

  if (filter) {
    filter = &filter->add<Filter>();
  }

  for (size_t i = 0; i < n; ++i) {
    auto const* valueNode = node.getMemberUnchecked(i);
    TRI_ASSERT(valueNode);

    if (!processSubnode(filter, *valueNode)) {
      return false;
    }
  }

  return true;
}

bool processSubnode(
    irs::boolean_filter* filter,
    arangodb::aql::AstNode const& node
) {
  switch (node.type) {
    case arangodb::aql::NODE_TYPE_OPERATOR_UNARY_NOT: // unary minus
      return fromNegation(filter, node);
    case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_AND: // logical and
      return fromBinaryAnd(filter, node);
    case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_OR: // logical or
      return fromGroup<irs::Or>(filter, node);
    case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_EQ: // compare ==
    case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_NE: // compare !=
      return fromBinaryEq(filter, node);
    case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_LT: // compare <
      return fromInterval<irs::Bound::MAX, false>(filter, node);
    case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_LE: // compare <=
      return fromInterval<irs::Bound::MAX, true>(filter, node);
    case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_GT: // compare >
      return fromInterval<irs::Bound::MIN, false>(filter, node);
    case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_GE: // compare >=
      return fromInterval<irs::Bound::MIN, true>(filter, node);
    case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_IN: // compare in
    case arangodb::aql::NODE_TYPE_OPERATOR_BINARY_NIN: // compare not in
      return fromArrayIn(filter, node);
    case arangodb::aql::NODE_TYPE_OPERATOR_TERNARY: // ternary
      break;
    case arangodb::aql::NODE_TYPE_VALUE : // value
    case arangodb::aql::NODE_TYPE_ARRAY: // array
      return fromValue(filter, node);
    case arangodb::aql::NODE_TYPE_FCALL: // function call
      // FIXME TODO
      break;
    case arangodb::aql::NODE_TYPE_FCALL_USER: // user function call
      // FIXME TODO
      break;
    case arangodb::aql::NODE_TYPE_RANGE: // range
      return fromRange(filter, node);
    case arangodb::aql::NODE_TYPE_OPERATOR_NARY_AND: // n-ary and
      return fromGroup<irs::And>(filter, node);
    case arangodb::aql::NODE_TYPE_OPERATOR_NARY_OR: // n-ary or
      return fromGroup<irs::Or>(filter, node);
    default:
      break;
  }

  // unsupported type
  return false;
}

// ----------------------------------------------------------------------------
// --SECTION--                                        OrderFactory dependencies
// ----------------------------------------------------------------------------

bool fromFCall(
    arangodb::iresearch::OrderFactory::OrderContext* ctx,
    arangodb::aql::AstNode const& node,
    bool reverse,
    arangodb::iresearch::IResearchViewMeta const& meta
) {
  TRI_ASSERT(arangodb::aql::NODE_TYPE_FCALL == node.type);
  auto* fn = static_cast<arangodb::aql::Function*>(node.getData());

  if (!fn) {
    return false; // no function
  }

  auto& name = fn->externalName;

  for (size_t i = 0, count = node.numMembers(); i < count; ++i) {
    auto* member = node.getMemberUnchecked(i);

    if (!member) {
      return false;
    }

    // FIXME TODO
  }

  auto argsCount = node.numMembers();

  // FIXME TODO
/*
  if (order) {
    order->add(scorer);
    scorer->reverse(reverse);
  }
*/
  return false;
}

bool fromFCallUser(
    arangodb::iresearch::OrderFactory::OrderContext* ctx,
    arangodb::aql::AstNode const& node,
    bool reverse,
    arangodb::iresearch::IResearchViewMeta const& meta
) {
  TRI_ASSERT(arangodb::aql::NODE_TYPE_FCALL_USER == node.type);

  if (arangodb::aql::VALUE_TYPE_STRING != node.value.type
      || 1 < node.numMembers()) {
    return false; // no function name
  }

  irs::string_ref name(node.getStringValue(), node.getStringLength());
  irs::sort::ptr scorer;

  if (!node.numMembers()) {
    scorer = irs::scorers::get(name, irs::string_ref::nil);

    if (scorer) {
      scorer = irs::scorers::get(name, "{}"); // pass arg as json
    }
  } else {
    auto* arg = node.getMemberUnchecked(0);

    if (!arg) {
      return false; // invalid arg
    }

    if (arangodb::aql::NODE_TYPE_VALUE == arg->type
        && arangodb::aql::VALUE_TYPE_STRING == arg->value.type) {
      irs::string_ref value(arg->getStringValue(), arg->getStringLength());

      scorer = irs::scorers::get(name, value);
    }

    if (!scorer) {
      arangodb::velocypack::Builder builder;

      builder.openObject();
      arg->toVelocyPackValue(builder);
      builder.close();
      scorer = irs::scorers::get(name, builder.toJson()); // pass arg as json
    }
  }

  if (!scorer) {
    return false; // failed find scorer
  }

  if (ctx) {
    ctx->order.add(scorer);
    scorer->reverse(reverse);
  }

  return true;
}

bool fromValue(
    arangodb::iresearch::OrderFactory::OrderContext* ctx,
    arangodb::aql::AstNode const& node,
    bool reverse,
    arangodb::iresearch::IResearchViewMeta const& meta
) {
  TRI_ASSERT(arangodb::aql::NODE_TYPE_VALUE == node.type);

  if (node.value.type != arangodb::aql::VALUE_TYPE_STRING) {
    return false; // unsupported value
  }

  if (ctx) {
    auto& scorer = ctx->order.add<AttributeScorer>(ctx->trx);
    scorer.reverse(reverse);

    // ArangoDB default type sort order:
    // null < bool < number < string < array/list < object/document
    scorer.orderNext(AttributeScorer::ValueType::NIL);
    scorer.orderNext(AttributeScorer::ValueType::BOOLEAN);
    scorer.orderNext(AttributeScorer::ValueType::NUMBER);
    scorer.orderNext(AttributeScorer::ValueType::STRING);
    scorer.orderNext(AttributeScorer::ValueType::ARRAY);
    scorer.orderNext(AttributeScorer::ValueType::OBJECT);
  }

  return true;
}

bool processSubnode(
    arangodb::iresearch::OrderFactory::OrderContext* ctx,
    arangodb::aql::AstNode const& node,
    arangodb::iresearch::IResearchViewMeta const& meta
) {
  if (arangodb::aql::NODE_TYPE_SORT_ELEMENT != node.type
      || 2 != node.numMembers()) {
    return false;
  }

  auto* ascending = node.getMemberUnchecked(1);

  if (!ascending || arangodb::aql::AstNodeType::NODE_TYPE_VALUE != ascending->type) {
    return false;
  }

  bool reverse;

  switch (ascending->value.type) {
    case arangodb::aql::VALUE_TYPE_BOOL:
      reverse = ascending->value.value._bool;
      break;
    case arangodb::aql::VALUE_TYPE_STRING:
      if (ascending->stringEquals("ASC", true)) {
        reverse = false;
        break;
      } else if (ascending->stringEquals("DESC", true)) {
        reverse = true;
        break;
      }
      // fall through
    default:
      return false; // unsupported value type
  }

  auto* expression = node.getMemberUnchecked(0);

  if (!expression) {
    return false;
  }

  switch (expression->type) {
    case arangodb::aql::NODE_TYPE_FCALL: // function call
      return fromFCall(ctx, *expression, reverse, meta);
    case arangodb::aql::NODE_TYPE_FCALL_USER: // user function call
      return fromFCallUser(ctx, *expression, reverse, meta);
    case arangodb::aql::NODE_TYPE_VALUE:
      return fromValue(ctx, *expression, reverse, meta);
    default:
      {} // NOOP
  }

  return false;
}

NS_END

NS_BEGIN(arangodb)
NS_BEGIN(iresearch)

// ----------------------------------------------------------------------------
// --SECTION--                                             Field implementation
// ----------------------------------------------------------------------------

/*static*/ void Field::setCidValue(Field& field, TRI_voc_cid_t& cid) {
  field._name = CID_FIELD;
  setIdValue(cid, *field._tokenizer);
  field._boost = 1.f;
  field._features = &irs::flags::empty_instance();
}

/*static*/ void Field::setCidValue(
    Field& field,
    TRI_voc_cid_t& cid,
    Field::init_stream_t
) {
  field._tokenizer = StringStreamPool.emplace();
  setCidValue(field, cid);
}

/*static*/ void Field::setRidValue(Field& field, TRI_voc_rid_t& rid) {
  field._name = RID_FIELD;
  setIdValue(rid, *field._tokenizer);
  field._boost = 1.f;
  field._features = &irs::flags::empty_instance();
}

/*static*/ void Field::setRidValue(
    Field& field,
    TRI_voc_rid_t& rid,
    Field::init_stream_t
) {
  field._tokenizer = StringStreamPool.emplace();
  setRidValue(field, rid);
}

Field::Field(Field&& rhs)
  : _features(rhs._features),
    _tokenizer(std::move(rhs._tokenizer)),
    _name(std::move(rhs._name)),
    _boost(rhs._boost) {
  rhs._features = nullptr;
}

Field& Field::operator=(Field&& rhs) {
  if (this != &rhs) {
    _features = rhs._features;
    _tokenizer = std::move(rhs._tokenizer);
    _name = std::move(rhs._name);
    _boost= rhs._boost;
    rhs._features = nullptr;
  }
  return *this;
}

// ----------------------------------------------------------------------------
// --SECTION--                                     FieldIterator implementation
// ----------------------------------------------------------------------------

/*static*/ FieldIterator const FieldIterator::END;

FieldIterator::FieldIterator(): _name(BufferPool.emplace()) {
  // initialize iterator's value
}

FieldIterator::FieldIterator(
    VPackSlice const& doc,
    IResearchLinkMeta const& linkMeta
): FieldIterator() {
  reset(doc, linkMeta);
}

void FieldIterator::reset(
    VPackSlice const& doc,
    IResearchLinkMeta const& linkMeta
) {
  // set surrogate tokenizers
  _begin = nullptr;
  _end = 1 + _begin;
  // clear stack
  _stack.clear();
  // clear field name
  _name->clear();

  if (!isArrayOrObject(doc)) {
    // can't handle plain objects
    return;
  }

  auto const* context = &linkMeta;

  // push the provided 'doc' to stack and initialize current value
  if (!push(doc, context) || !setValue(topValue().value, *context)) {
    next();
  }
}

IResearchLinkMeta const* FieldIterator::nextTop() {
  auto& name = nameBuffer();
  auto& level = top();
  auto& it = level.it;
  auto const* context = level.meta;
  auto const filter = level.filter;

  name.resize(level.nameLength);
  while (it.next() && !filter(name, context, it.value())) {
    // filtered out
    name.resize(level.nameLength);
  }

  return context;
}

bool FieldIterator::push(VPackSlice slice, IResearchLinkMeta const*& context) {
  auto& name = nameBuffer();

  while (isArrayOrObject(slice)) {
    if (!name.empty() && !slice.isArray()) {
      name += NESTING_LEVEL_DELIMITER;
    }

    auto const filter = getFilter(slice, *context);

    _stack.emplace_back(slice, name.size(), *context, filter);

    auto& it = top().it;

    if (!it.valid()) {
      // empty object or array, skip it
      return false;
    }

    auto& value = it.value();

    if (!filter(name, context, value)) {
      // filtered out
      TRI_ASSERT(context);
      return false;
    }

    slice = value.value;
  }

  TRI_ASSERT(context);
  return true;
}

bool FieldIterator::setValue(
    VPackSlice const& value,
    IResearchLinkMeta const& context
) {
  _begin = nullptr;
  _end = 1 + _begin;               // set surrogate tokenizers
  _value._boost = context._boost;  // set boost

  switch (value.type()) {
    case VPackValueType::None:
    case VPackValueType::Illegal:
      return false;
    case VPackValueType::Null:
      setNullValue(value, nameBuffer(), _value);
      return true;
    case VPackValueType::Bool:
      setBoolValue(value, nameBuffer(), _value);
      return true;
    case VPackValueType::Array:
    case VPackValueType::Object:
      return true;
    case VPackValueType::Double:
      setNumericValue(value, nameBuffer(), _value);
      return true;
    case VPackValueType::UTCDate:
    case VPackValueType::External:
    case VPackValueType::MinKey:
    case VPackValueType::MaxKey:
      return false;
    case VPackValueType::Int:
    case VPackValueType::UInt:
    case VPackValueType::SmallInt:
      setNumericValue(value, nameBuffer(), _value);
      return true;
    case VPackValueType::String:
      resetTokenizers(context); // reset string tokenizers
      return setStringValue(value, nameBuffer(), _value, _begin);
    case VPackValueType::Binary:
    case VPackValueType::BCD:
    case VPackValueType::Custom:
    default:
      return false;
  }

  return false;
}

void FieldIterator::next() {
  TRI_ASSERT(valid());

  TokenizerIterator const prev = _begin;

  while (++_begin != _end) {
    auto& name = nameBuffer();

    // remove previous suffix
    unmangleStringField(name, prev);

    // can have multiple tokenizers for string values only
    if (setStringValue(topValue().value, name, _value, _begin)) {
      return;
    }
  }

  IResearchLinkMeta const* context;

  do {
    // advance top iterator
    context = nextTop();

    // pop all exhausted iterators
    for (; !top().it.valid(); context = nextTop()) {
      _stack.pop_back();

      if (!valid()) {
        // reached the end
        return;
      }

      // reset name to previous size
      nameBuffer().resize(top().nameLength);
    }

  } while (!push(topValue().value, context)
           || !setValue(topValue().value, *context));
}

// ----------------------------------------------------------------------------
// --SECTION--                                DocumentPrimaryKey implementation
// ----------------------------------------------------------------------------

/* static */ irs::string_ref const& DocumentPrimaryKey::PK() {
  return PK_COLUMN;
}

DocumentPrimaryKey::DocumentPrimaryKey(
    TRI_voc_cid_t cid,
    TRI_voc_rid_t rid
) noexcept
  : _keys{ cid, rid } {
  static_assert(sizeof(_keys) == sizeof(cid) + sizeof(rid), "Invalid size");
}

bool DocumentPrimaryKey::read(irs::bytes_ref const& in) noexcept {
  if (sizeof(_keys) != in.size()) {
    return false;
  }

  std::memcpy(_keys, in.c_str(), sizeof(_keys));

  return true;
}

bool DocumentPrimaryKey::write(irs::data_output& out) const {
  out.write_bytes(
    reinterpret_cast<const irs::byte_type*>(_keys),
    sizeof(_keys)
  );

  return true;
}

// ----------------------------------------------------------------------------
// --SECTION--                                      FilerFactory implementation
// ----------------------------------------------------------------------------

/*static*/ irs::filter::ptr FilterFactory::filter(TRI_voc_cid_t cid) {
  auto filter = irs::by_term::make();

  // filter matching on cid
  static_cast<irs::by_term&>(*filter)
    .field(CID_FIELD) // set field
    .term(toBytesRefLE(cid)); // set value

  return std::move(filter);
}

/*static*/ irs::filter::ptr FilterFactory::filter(
    TRI_voc_cid_t cid,
    TRI_voc_rid_t rid
) {
  auto filter = irs::And::make();

  // filter matching on cid and rid
  static_cast<irs::And&>(*filter).add<irs::by_term>()
    .field(CID_FIELD) // set field
    .term(toBytesRefLE(cid));   // set value

  static_cast<irs::And&>(*filter).add<irs::by_term>()
    .field(RID_FIELD) // set field
    .term(toBytesRefLE(rid));   // set value

  return std::move(filter);
}

/*static*/ bool FilterFactory::filter(
    irs::boolean_filter* filter,
    arangodb::aql::AstNode const& node
) {
  if (arangodb::aql::NODE_TYPE_FILTER != node.type) {
    // wrong root node type
    return false;
  }

  if (1 != node.numMembers()) {
    //. wrong number of members
    return false;
  }

  auto const* member = node.getMemberUnchecked(0);

  return member && processSubnode(filter, *member);
}

// ----------------------------------------------------------------------------
// --SECTION--                                      OrderFactory implementation
// ----------------------------------------------------------------------------

/*static*/ bool OrderFactory::order(
  arangodb::iresearch::OrderFactory::OrderContext* ctx,
  arangodb::aql::SortCondition const& node,
  IResearchViewMeta const& meta
) {
  for (size_t i = 0, count = node.numAttributes(); i < count; ++i) {
    auto field = node.field(i);
    auto* variable = std::get<0>(field);
    auto* member = std::get<1>(field);
    //auto ascending = std::get<3>(field);
    // FIXME TODO check what is the expected node type

    if (!member || !processSubnode(ctx, *member, meta)) {
      return false;
    }
  }

  return true;
}

NS_END // iresearch
NS_END // arangodb

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------