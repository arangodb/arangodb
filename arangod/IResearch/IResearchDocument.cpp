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
#include "Basics/Endian.h"
#include "Basics/StaticStrings.h"
#include "Basics/VelocyPackHelper.h"
#include "IResearchCommon.h"
#include "IResearchKludge.h"
#include "IResearchPrimaryKeyFilter.h"
#include "IResearchViewMeta.h"
#include "Logger/LogMacros.h"
#include "Misc.h"
#include "Transaction/Helpers.h"
#include "Transaction/Methods.h"

#include "analysis/token_attributes.hpp"
#include "analysis/token_streams.hpp"

#include "search/term_filter.hpp"

#include "utils/log.hpp"

namespace {

// ----------------------------------------------------------------------------
// --SECTION--                                           Primary key endianness
// ----------------------------------------------------------------------------

constexpr bool const BigEndian = false;

template <bool IsLittleEndian>
struct PrimaryKeyEndianness {
  static uint64_t hostToPk(uint64_t value) {
    return arangodb::basics::hostToLittle(value);
  }
  static uint64_t pkToHost(uint64_t value) {
    return arangodb::basics::littleToHost(value);
  }
};  // PrimaryKeyEndianness

template <>
struct PrimaryKeyEndianness<false> {
  static uint64_t hostToPk(uint64_t value) {
    return arangodb::basics::hostToBig(value);
  }
  static uint64_t pkToHost(uint64_t value) {
    return arangodb::basics::bigToHost(value);
  }
};  // PrimaryKeyEndianness

constexpr bool const Endianness = BigEndian;  // current PK endianness

// ----------------------------------------------------------------------------
// --SECTION--                                       FieldIterator dependencies
// ----------------------------------------------------------------------------

enum AttributeType : uint8_t {
  AT_REG = arangodb::basics::VelocyPackHelper::AttributeBase,  // regular attribute
  AT_KEY = arangodb::basics::VelocyPackHelper::KeyAttribute,    // _key
  AT_REV = arangodb::basics::VelocyPackHelper::RevAttribute,    // _rev
  AT_ID = arangodb::basics::VelocyPackHelper::IdAttribute,      // _id
  AT_FROM = arangodb::basics::VelocyPackHelper::FromAttribute,  // _from
  AT_TO = arangodb::basics::VelocyPackHelper::ToAttribute       // _to
};                                                              // AttributeType

static_assert(arangodb::iresearch::adjacencyChecker<AttributeType>::checkAdjacency<
                  AT_TO, AT_FROM, AT_ID, AT_REV, AT_KEY, AT_REG>(),
              "Values are not adjacent");

irs::string_ref const PK_COLUMN("@_PK");

// wrapper for use objects with the IResearch unbounded_object_pool
template <typename T>
struct AnyFactory {
  typedef std::shared_ptr<T> ptr;

  template <typename... Args>
  static ptr make(Args&&... args) {
    return std::make_shared<T>(std::forward<Args>(args)...);
  }
};  // AnyFactory

size_t const DEFAULT_POOL_SIZE = 8;  // arbitrary value
irs::unbounded_object_pool<AnyFactory<std::string>> BufferPool(DEFAULT_POOL_SIZE);
irs::unbounded_object_pool<AnyFactory<irs::string_token_stream>> StringStreamPool(DEFAULT_POOL_SIZE);
irs::unbounded_object_pool<AnyFactory<irs::null_token_stream>> NullStreamPool(DEFAULT_POOL_SIZE);
irs::unbounded_object_pool<AnyFactory<irs::boolean_token_stream>> BoolStreamPool(DEFAULT_POOL_SIZE);
irs::unbounded_object_pool<AnyFactory<irs::numeric_token_stream>> NumericStreamPool(DEFAULT_POOL_SIZE);
irs::flags NumericStreamFeatures{irs::type<irs::granularity_prefix>::get()};

// appends the specified 'value' to 'out'
inline void append(std::string& out, size_t value) {
  auto const size = out.size();  // intial size
  out.resize(size + 21);         // enough to hold all numbers up to 64-bits
  auto const written = sprintf(&out[size], IR_SIZE_T_SPECIFIER, value);
  out.resize(size + written);
}

inline bool keyFromSlice(VPackSlice keySlice, irs::string_ref& key) {
  // according to Helpers.cpp, see
  // `transaction::helpers::extractKeyFromDocument`
  // `transaction::helpers::extractRevFromDocument`
  // `transaction::helpers::extractIdFromDocument`
  // `transaction::helpers::extractFromFromDocument`
  // `transaction::helpers::extractToFromDocument`

  switch (keySlice.type()) {
    case VPackValueType::SmallInt:               // system attribute
      switch (AttributeType(keySlice.head())) {  // system attribute type
        case AT_REG:
          return false;
        case AT_KEY:
          key = arangodb::StaticStrings::KeyString;
          break;
        case AT_REV:
          key = arangodb::StaticStrings::RevString;
          break;
        case AT_ID:
          key = arangodb::StaticStrings::IdString;
          break;
        case AT_FROM:
          key = arangodb::StaticStrings::FromString;
          break;
        case AT_TO:
          key = arangodb::StaticStrings::ToString;
          break;
        default:
          return false;
      }
      return true;
    case VPackValueType::String:  // regular attribute
      key = arangodb::iresearch::getStringRef(keySlice);
      return true;
    default:  // unsupported
      return false;
  }
}

inline bool canHandleValue(std::string const& key, VPackSlice const& value,
                           arangodb::iresearch::FieldMeta const& context) noexcept {
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
    case VPackValueType::Custom:
      TRI_ASSERT(key == arangodb::StaticStrings::IdString);
      [[fallthrough]];
    case VPackValueType::String:
      return !context._analyzers.empty();
    default:
      return false;
  }
}

// returns 'context' in case if can't find the specified 'field'
inline arangodb::iresearch::FieldMeta const* findMeta(irs::string_ref const& key,
                                                      arangodb::iresearch::FieldMeta const* context) {
  TRI_ASSERT(context);

  auto const* meta = context->_fields.findPtr(key);
  return meta ? meta->get() : context;
}

inline bool inObjectFiltered(std::string& buffer,
                             arangodb::iresearch::FieldMeta const*& context,
                             arangodb::iresearch::IteratorValue const& value) {
  irs::string_ref key;

  if (!keyFromSlice(value.key, key)) {
    return false;
  }

  auto const* meta = findMeta(key, context);

  if (meta == context) {
    return false;
  }

  buffer.append(key.c_str(), key.size());
  context = meta;

  return canHandleValue(buffer, value.value, *context);
}

inline bool inObject(std::string& buffer, arangodb::iresearch::FieldMeta const*& context,
                     arangodb::iresearch::IteratorValue const& value) {
  irs::string_ref key;

  if (!keyFromSlice(value.key, key)) {
    return false;
  }

  buffer.append(key.c_str(), key.size());
  context = findMeta(key, context);

  return canHandleValue(buffer, value.value, *context);
}

inline bool inArrayOrdered(std::string& buffer,
                           arangodb::iresearch::FieldMeta const*& context,
                           arangodb::iresearch::IteratorValue const& value) {
  buffer += arangodb::iresearch::NESTING_LIST_OFFSET_PREFIX;
  append(buffer, value.pos);
  buffer += arangodb::iresearch::NESTING_LIST_OFFSET_SUFFIX;

  return canHandleValue(buffer, value.value, *context);
}

inline bool inArray(std::string& buffer, arangodb::iresearch::FieldMeta const*& context,
                    arangodb::iresearch::IteratorValue const& value) noexcept {
  return canHandleValue(buffer, value.value, *context);
}

typedef bool (*Filter)(std::string& buffer, arangodb::iresearch::FieldMeta const*& context,
                       arangodb::iresearch::IteratorValue const& value);

Filter const valueAcceptors[] = {
    // type == Object, nestListValues == false, // includeAllValues == false
    &inObjectFiltered,
    // type == Object, nestListValues == false, includeAllValues == // true
    &inObject,
    // type == Object, nestListValues == true , // includeAllValues == false
    &inObjectFiltered,
    // type == Object, nestListValues == true , includeAllValues == // true
    &inObject,
    // type == Array , nestListValues == flase, includeAllValues == // false
    &inArray,
    // type == Array , nestListValues == flase, includeAllValues == // true
    &inArray,
    // type == Array , nestListValues == true, // includeAllValues == false
    &inArrayOrdered,
    // type == Array , nestListValues == true, includeAllValues // == true
    &inArrayOrdered};

inline Filter getFilter(VPackSlice value, arangodb::iresearch::FieldMeta const& meta) noexcept {
  TRI_ASSERT(arangodb::iresearch::isArrayOrObject(value));

  return valueAcceptors[4 * value.isArray() + 2 * meta._trackListPositions + meta._includeAllFields];
}

}  // namespace

namespace arangodb {
namespace iresearch {

// ----------------------------------------------------------------------------
// --SECTION--                                             Field implementation
// ----------------------------------------------------------------------------

/*static*/ void Field::setPkValue(Field& field, LocalDocumentId::BaseType const& pk) {
  field._name = PK_COLUMN;
  field._features = &irs::flags::empty_instance();
  field._storeValues = ValueStorage::VALUE;
  field._value =
      irs::bytes_ref(reinterpret_cast<irs::byte_type const*>(&pk), sizeof(pk));
  field._analyzer = StringStreamPool.emplace().release();  // FIXME don't use shared_ptr
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  auto& sstream = dynamic_cast<irs::string_token_stream&>(*field._analyzer);
#else
  auto& sstream = static_cast<irs::string_token_stream&>(*field._analyzer);
#endif
  sstream.reset(field._value);
}

Field::Field(Field&& rhs)
    : _features(rhs._features),
      _analyzer(std::move(rhs._analyzer)),
      _name(std::move(rhs._name)),
      _storeValues(std::move(rhs._storeValues)) {
  rhs._features = nullptr;
}

Field& Field::operator=(Field&& rhs) {
  if (this != &rhs) {
    _features = rhs._features;
    _analyzer = std::move(rhs._analyzer);
    _name = std::move(rhs._name);
    _storeValues = std::move(rhs._storeValues);
    rhs._features = nullptr;
  }

  return *this;
}

// ----------------------------------------------------------------------------
// --SECTION--                                     FieldIterator implementation
// ----------------------------------------------------------------------------

FieldIterator::FieldIterator(arangodb::transaction::Methods& trx)
    : _nameBuffer(BufferPool.emplace().release()),  // FIXME don't use shared_ptr
      _trx(&trx) {
  // initialize iterator's value
}

std::string& FieldIterator::valueBuffer() {
  if (!_valueBuffer) {
    _valueBuffer = BufferPool.emplace().release();  // FIXME don't use shared_ptr
  }

  return *_valueBuffer;
}

void FieldIterator::reset(VPackSlice const& doc, FieldMeta const& linkMeta) {
  // set surrogate analyzers
  _begin = nullptr;
  _end = 1 + _begin;
  // clear stack
  _stack.clear();
  // clear field name
  _nameBuffer->clear();

  if (!isArrayOrObject(doc)) {
    // can't handle plain objects
    return;
  }

  auto const* context = &linkMeta;

  // push the provided 'doc' to stack and initialize current value
  if (!pushAndSetValue(doc, context)) {
    next();
  }
}

void FieldIterator::setBoolValue(VPackSlice const value) {
  TRI_ASSERT(value.isBool());

  auto& name = nameBuffer();

  // mangle name
  arangodb::iresearch::kludge::mangleBool(name);

  // init stream
  auto stream = BoolStreamPool.emplace();
  stream->reset(value.getBool());

  // set field properties
  _value._name = name;
  _value._analyzer = stream.release();  // FIXME don't use shared_ptr
  _value._features = &irs::flags::empty_instance();
}

void FieldIterator::setNumericValue(VPackSlice const value) {
  TRI_ASSERT(value.isNumber());

  auto& name = nameBuffer();

  // mangle name
  arangodb::iresearch::kludge::mangleNumeric(name);

  // init stream
  auto stream = NumericStreamPool.emplace();
  stream->reset(value.getNumber<double>());

  // set field properties
  _value._name = name;
  _value._analyzer = stream.release();  // FIXME don't use shared_ptr
  _value._features = &NumericStreamFeatures;
}

void FieldIterator::setNullValue(VPackSlice const value) {
  TRI_ASSERT(value.isNull());

  auto& name = nameBuffer();

  // mangle name
  arangodb::iresearch::kludge::mangleNull(name);

  // init stream
  auto stream = NullStreamPool.emplace();
  stream->reset();

  // set field properties
  _value._name = name;
  _value._analyzer = stream.release();  // FIXME don't use shared_ptr
  _value._features = &irs::flags::empty_instance();
}

bool FieldIterator::setStringValue(arangodb::velocypack::Slice const value,
                                   FieldMeta::Analyzer const& valueAnalyzer) {
  TRI_ASSERT(  // assert
      (value.isCustom() && nameBuffer() == arangodb::StaticStrings::IdString)  // custom string
      || value.isString());  // verbatim string

  irs::string_ref valueRef;

  if (value.isCustom()) {
    if (_stack.empty()) {
      // base object isn't set
      return false;
    }

    auto const baseSlice = _stack.front().it.slice();
    auto& buffer = valueBuffer();

    buffer = transaction::helpers::extractIdString(  // extract id
        _trx->resolver(),                            // resolver
        value,                                       // value
        baseSlice                                    // base slice
    );

    valueRef = buffer;
  } else {
    valueRef = iresearch::getStringRef(value);
  }

  auto& pool = valueAnalyzer._pool;

  if (!pool) {
    LOG_TOPIC("189da", WARN, iresearch::TOPIC)
        << "got nullptr analyzer factory";

    return false;
  }

  auto& name = nameBuffer();

  // it's important to unconditionally mangle name
  // since we unconditionally unmangle it in 'next'
  iresearch::kludge::mangleStringField(name, valueAnalyzer);

  // init stream
  auto analyzer = pool->get();

  if (!analyzer) {
    LOG_TOPIC("22eee", WARN, arangodb::iresearch::TOPIC)
        << "got nullptr from analyzer factory, name '" << pool->name() << "'";
    return false;
  }

  // init stream
  analyzer->reset(valueRef);

  // set field properties
  _value._name = name;
  _value._analyzer = analyzer;
  _value._features = &(pool->features());

  return true;
}

bool FieldIterator::pushAndSetValue(VPackSlice slice, FieldMeta const*& context) {
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

  // set value
  _begin = nullptr;
  _end = 1 + _begin;  // set surrogate analyzers

  return setAttributeValue(*context);
}

bool FieldIterator::setAttributeValue(FieldMeta const& context) {
  auto const value = topValue().value;

  _value._storeValues = context._storeValues;
  _value._value = irs::bytes_ref::NIL;

  switch (value.type()) {
    case VPackValueType::None:
    case VPackValueType::Illegal:
      return false;
    case VPackValueType::Null:
      setNullValue(value);
      return true;
    case VPackValueType::Bool:
      setBoolValue(value);
      return true;
    case VPackValueType::Array:
    case VPackValueType::Object:
      return true;
    case VPackValueType::Double:
      setNumericValue(value);
      return true;
    case VPackValueType::UTCDate:
    case VPackValueType::External:
    case VPackValueType::MinKey:
    case VPackValueType::MaxKey:
      return false;
    case VPackValueType::Int:
    case VPackValueType::UInt:
    case VPackValueType::SmallInt:
      setNumericValue(value);
      return true;
    case VPackValueType::Custom:
      TRI_ASSERT(nameBuffer() == arangodb::StaticStrings::IdString);
      [[fallthrough]];
    case VPackValueType::String:
      resetAnalyzers(context);  // reset string analyzers
      return setStringValue(value, *_begin);
    default:
      return false;
  }
}

void FieldIterator::next() {
  TRI_ASSERT(valid());

  for (auto const* prev = _begin; ++_begin != _end;) {
    auto& name = nameBuffer();

    // remove previous suffix
    arangodb::iresearch::kludge::demangleStringField(name, *prev);

    // can have multiple analyzers for string values only
    if (setStringValue(topValue().value, *_begin)) {
      return;
    }
  }

  FieldMeta const* context;

  auto& name = nameBuffer();

  auto nextTop = [this, &name]() {
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
  };

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
      name.resize(top().nameLength);
    }

  } while (!pushAndSetValue(topValue().value, context));
}

// ----------------------------------------------------------------------------
// --SECTION--                                DocumentPrimaryKey implementation
// ----------------------------------------------------------------------------

/* static */ irs::string_ref const& DocumentPrimaryKey::PK() noexcept {
  return PK_COLUMN;
}

/*static*/ LocalDocumentId::BaseType DocumentPrimaryKey::encode(LocalDocumentId value) noexcept {
  return PrimaryKeyEndianness<Endianness>::hostToPk(value.id());
}

// PLEASE NOTE that 'in.c_str()' MUST HAVE alignment >= alignof(uint64_t)
// NOTE implementation must match implementation of operator irs::bytes_ref()
/*static*/ bool DocumentPrimaryKey::read(arangodb::LocalDocumentId& value,
                                         irs::bytes_ref const& in) noexcept {
  if (sizeof(arangodb::LocalDocumentId::BaseType) != in.size()) {
    return false;
  }

  // PLEASE NOTE that 'in.c_str()' MUST HAVE alignment >= alignof(uint64_t)
  value = arangodb::LocalDocumentId(PrimaryKeyEndianness<Endianness>::pkToHost(
      *reinterpret_cast<arangodb::LocalDocumentId::BaseType const*>(in.c_str())));

  return true;
}

}  // namespace iresearch
}  // namespace arangodb

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------
