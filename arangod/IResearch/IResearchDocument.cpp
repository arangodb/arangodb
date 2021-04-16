////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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

#include "IResearchDocument.h"

#include "Basics/Endian.h"
#include "Basics/StaticStrings.h"
#include "Basics/VelocyPackHelper.h"
#include "Cluster/ServerState.h"
#include "IResearch/GeoAnalyzer.h"
#include "IResearch/IResearchCommon.h"
#include "IResearch/IResearchKludge.h"
#include "IResearch/IResearchPrimaryKeyFilter.h"
#include "IResearch/IResearchViewMeta.h"
#include "IResearch/IResearchVPackTermAttribute.h"
#include "Logger/LogMacros.h"
#include "Misc.h"
#include "Transaction/Helpers.h"
#include "Transaction/Methods.h"
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
    // type == Object, nestListValues == false, includeAllValues == false
    &inObjectFiltered,
    // type == Object, nestListValues == false, includeAllValues == true
    &inObject,
    // type == Object, nestListValues == true , includeAllValues == false
    &inObjectFiltered,
    // type == Object, nestListValues == true , includeAllValues == true
    &inObject,
    // type == Array , nestListValues == flase, includeAllValues == false
    &inArray,
    // type == Array , nestListValues == flase, includeAllValues == true
    &inArray,
    // type == Array , nestListValues == true, includeAllValues == false
    &inArrayOrdered,
    // type == Array , nestListValues == true, includeAllValues == true
    &inArrayOrdered};

inline Filter getFilter(VPackSlice value, arangodb::iresearch::FieldMeta const& meta) noexcept {
  TRI_ASSERT(arangodb::iresearch::isArrayOrObject(value));

  return valueAcceptors[4 * value.isArray() + 2 * meta._trackListPositions + meta._includeAllFields];
}

std::string getDocumentId(irs::string_ref collection,
                          VPackSlice document) {
  VPackStringRef const key = arangodb::transaction::helpers::extractKeyPart(document);
  if (key.empty()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "failed to extract key value from document");
  }
  std::string resolved;
  resolved.reserve(collection.size() + 1 + key.size());
  resolved += collection;
  resolved.push_back('/');
  resolved.append(key.data(), key.size());
  return resolved;
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

// ----------------------------------------------------------------------------
// --SECTION--                                     FieldIterator implementation
// ----------------------------------------------------------------------------

FieldIterator::FieldIterator(arangodb::transaction::Methods& trx, irs::string_ref collection,  IndexId linkId)
    : _trx(&trx), _collection(collection), _linkId(linkId),
      _isDBServer(ServerState::instance()->isDBServer()) {

  // initialize iterator's value
}

void FieldIterator::reset(VPackSlice doc, FieldMeta const& linkMeta) {
  _slice = doc;
  _begin = nullptr;
  _end = nullptr;
  _currentTypedAnalyzer = nullptr;
  _currentTypedAnalyzerValue = nullptr;
  _primitiveTypeResetter = nullptr;
  _stack.clear();
  _nameBuffer.clear();

  // push the provided 'doc' on stack and initialize current value
  auto const filter = getFilter(doc, linkMeta);
  _stack.emplace_back(doc, 0, linkMeta, filter);

  next();
}

void FieldIterator::setBoolValue(VPackSlice const value) {
  TRI_ASSERT(value.isBool());

  arangodb::iresearch::kludge::mangleBool(_nameBuffer);

  // init stream
  auto stream = BoolStreamPool.emplace();
  stream->reset(value.getBool());

  // set field properties
  _value._name = _nameBuffer;
  _value._analyzer = stream.release();  // FIXME don't use shared_ptr
  _value._features = &irs::flags::empty_instance();
}

void FieldIterator::setNumericValue(VPackSlice const value) {
  TRI_ASSERT(value.isNumber());

  arangodb::iresearch::kludge::mangleNumeric(_nameBuffer);

  // init stream
  auto stream = NumericStreamPool.emplace();
  stream->reset(value.getNumber<double>());

  // set field properties
  _value._name = _nameBuffer;
  _value._analyzer = stream.release();  // FIXME don't use shared_ptr
  _value._features = &NumericStreamFeatures;
}

void FieldIterator::setNullValue(VPackSlice const value) {
  TRI_ASSERT(value.isNull());

  arangodb::iresearch::kludge::mangleNull(_nameBuffer);

  // init stream
  auto stream = NullStreamPool.emplace();
  stream->reset();

  // set field properties
  _value._name = _nameBuffer;
  _value._analyzer = stream.release();  // FIXME don't use shared_ptr
  _value._features = &irs::flags::empty_instance();
}

bool FieldIterator::setValue(VPackSlice const value,
                             FieldMeta::Analyzer const& valueAnalyzer) {
  TRI_ASSERT(  // assert
      (value.isCustom() && _nameBuffer == arangodb::StaticStrings::IdString)  // custom string
      || value.isObject()
      || value.isArray()
      || value.isString()); // verbatim string

  auto& pool = valueAnalyzer._pool;

  if (!pool) {
    LOG_TOPIC("189da", WARN, iresearch::TOPIC)
        << "got nullptr analyzer factory";

    return false;
  }

  irs::string_ref valueRef;
  AnalyzerValueType valueType{AnalyzerValueType::Undefined};

  switch (value.type()) {
    case VPackValueType::Array: {
      valueRef = iresearch::ref<char>(value);
      valueType = AnalyzerValueType::Array;
    } break;
    case VPackValueType::Object: {
      valueRef = iresearch::ref<char>(value);
      valueType = AnalyzerValueType::Object;
    } break;
    case VPackValueType::String: {
      valueRef = iresearch::getStringRef(value);
      valueType = AnalyzerValueType::String;
    } break;
    case VPackValueType::Custom: {
      TRI_ASSERT(!_slice.isNone());
      if (_isDBServer) {
        if (!_collection.empty()) {
          _valueBuffer = getDocumentId(_collection, _slice);
        } else {
          LOG_TOPIC("fb53c", WARN, arangodb::iresearch::TOPIC)
            << "Value for `_id` attribute could not be indexed for document "
            << transaction::helpers::extractKeyFromDocument(_slice).toString()
            << ". To recover please recreate corresponding ArangoSearch link '" << _linkId << "'";
          return false;
        }
      } else {
         _valueBuffer = transaction::helpers::extractIdString(
             _trx->resolver(), value, _slice);
      }
      valueRef = _valueBuffer;
      valueType = AnalyzerValueType::String;
    } break;
    default:
      TRI_ASSERT(false);
      return false;
  }

  if (!pool->accepts(valueType)) {
    return false;
  }

  // init stream
  auto analyzer = pool->get();

  if (!analyzer) {
    LOG_TOPIC("22eee", WARN, arangodb::iresearch::TOPIC)
        << "got nullptr from analyzer factory, name '" << pool->name() << "'";
    return false;
  }
  if (!analyzer->reset(valueRef)) {
      return false;
  }
  // set field properties
  switch (pool->returnType()) {
    case AnalyzerValueType::Bool:
      {
        if (!analyzer->next()) {
          return false;
        }
        _currentTypedAnalyzer = analyzer.get();
        _currentTypedAnalyzerValue = irs::get<VPackTermAttribute>(*analyzer);
        TRI_ASSERT(_currentTypedAnalyzerValue);
        setBoolValue(_currentTypedAnalyzerValue->value);
        _primitiveTypeResetter = [](irs::token_stream* stream, VPackSlice slice) -> void {
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
          auto bool_stream = dynamic_cast<irs::boolean_token_stream*>(stream);
          TRI_ASSERT(bool_stream);
#else
          auto bool_stream = static_cast<irs::boolean_token_stream*>(stream);
#endif
          TRI_ASSERT(slice.isBool());
          bool_stream->reset(slice.getBool());
        };
      }
      break;
    case AnalyzerValueType::Number:
      {
        if (!analyzer->next()) {
          return false;
        }
        _currentTypedAnalyzer = analyzer.get();
        _currentTypedAnalyzerValue = irs::get<VPackTermAttribute>(*analyzer);
        TRI_ASSERT(_currentTypedAnalyzerValue);
        setNumericValue(_currentTypedAnalyzerValue->value);
        _primitiveTypeResetter = [](irs::token_stream* stream, VPackSlice slice) -> void {
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
          auto number_stream = dynamic_cast<irs::numeric_token_stream*>(stream);
          TRI_ASSERT(number_stream);
#else
          auto number_stream = static_cast<irs::numeric_token_stream*>(stream);
#endif
          TRI_ASSERT(slice.isNumber());
          number_stream->reset(slice.getNumber<double>());
        };
      }
      break;
    default:
      iresearch::kludge::mangleField(_nameBuffer, valueAnalyzer);
      _value._analyzer = analyzer;
      _value._features = &(pool->features());
      _value._name = _nameBuffer;
      break;
  }
  auto* storeFunc = pool->storeFunc();
  if (storeFunc) {
    auto const valueSlice = storeFunc(analyzer.get(), value, _buffer);

    if (!value.isNone()) {
      _value._value = iresearch::ref<irs::byte_type>(valueSlice);
      _value._storeValues = std::max(ValueStorage::VALUE, _value._storeValues);
    }
  }

  return true;
}

void FieldIterator::next() {
  TRI_ASSERT(valid());

  if (_currentTypedAnalyzer) {
     if (_currentTypedAnalyzer->next()) {
       TRI_ASSERT(_primitiveTypeResetter);
       TRI_ASSERT(_currentTypedAnalyzerValue);
       TRI_ASSERT(_value._analyzer.get());
       _primitiveTypeResetter(_value._analyzer.get(), _currentTypedAnalyzerValue->value);
       return;
     } else {
       _currentTypedAnalyzer = nullptr;
     }
  }

  FieldMeta const* context = top().meta;

  // restore value
  _value._storeValues = context->_storeValues;
  _value._value = irs::bytes_ref::NIL;

  while (true) {
setAnalyzers:
    while (_begin != _end) {
      // remove previous suffix
      _nameBuffer.resize(_prefixLength);

      if (setValue(_valueSlice, *_begin++)) {
        return;
      }
    }

    while (true) {
      // pop all exhausted iterators
      while (!top().it.next()) {
        _stack.pop_back();

        if (!valid()) {
          // reached the end
          return;
        }
      }

      auto& level = top();
      auto& it = level.it;
      auto& value = it.value();
      context = level.meta;

      // reset name to previous size
      _nameBuffer.resize(level.nameLength);

      // check if we're in object scope
      if (auto const parent = _stack.data() + _stack.size() - 2;
          parent >= _stack.data() && parent->it.value().value.isObject()) {
        _nameBuffer += NESTING_LEVEL_DELIMITER;
      }

      if (!level.filter(_nameBuffer, context, value)) {
        continue;
      }

      _value._storeValues = context->_storeValues;
      _value._value = irs::bytes_ref::NIL;
      _begin = nullptr;
      _end = nullptr;

      switch (auto const valueSlice = value.value; valueSlice.type()) {
        case VPackValueType::Null:
          setNullValue(valueSlice);
          return;
        case VPackValueType::Bool:
          setBoolValue(valueSlice);
          return;
        case VPackValueType::Object:
        case VPackValueType::Array: {
          _stack.emplace_back(valueSlice, _nameBuffer.size(), *context,
                              getFilter(valueSlice, *context));

          auto const& analyzers = context->_analyzers;
          _begin = analyzers.data() + context->_primitiveOffset;
          _end = analyzers.data() + analyzers.size();

          _prefixLength = _nameBuffer.size(); // save current prefix length
          _valueSlice = valueSlice;

          if (_begin != _end) {
            goto setAnalyzers;
          }
        } break;
        case VPackValueType::Double:
        case VPackValueType::Int:
        case VPackValueType::UInt:
        case VPackValueType::SmallInt:
          setNumericValue(valueSlice);
          return;
        case VPackValueType::Custom:
          TRI_ASSERT(_nameBuffer == arangodb::StaticStrings::IdString);
          [[fallthrough]];
        case VPackValueType::String: {
          auto const& analyzers = context->_analyzers;
          _begin = analyzers.data();
          _end = _begin + context->_primitiveOffset;

          _prefixLength = _nameBuffer.size(); // save current prefix length
          _valueSlice = valueSlice;
          goto setAnalyzers;
        }
        default:
          break;
      }
    }
  }
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

  StoredValue::StoredValue(transaction::Methods const& t, irs::string_ref cn,
                           VPackSlice const doc, IndexId lid)
    : trx(t), document(doc), collection(cn),
      linkId(lid), isDBServer(ServerState::instance()->isDBServer()) {}

bool StoredValue::write(irs::data_output& out) const {
  auto size = fields->size();
  for (auto const& storedValue : *fields) {
    auto slice = get(document, storedValue.second, VPackSlice::nullSlice());
    // null value optimization
    if (1 == size && slice.isNull()) {
      return true;
    }
    // _id field
    if (slice.isCustom()) {
      TRI_ASSERT(1 == storedValue.second.size() &&
                  storedValue.second[0].name == arangodb::StaticStrings::IdString);
      buffer.reset();
      VPackBuilder builder(buffer);
      if (isDBServer) {
        if (!collection.empty()) {
          builder.add(VPackValue(getDocumentId(collection, document)));
        } else {
          LOG_TOPIC("bf98c", WARN, arangodb::iresearch::TOPIC)
            << "Value for `_id` attribute could not be stored for document "
            << transaction::helpers::extractKeyFromDocument(document).toString()
            << ". To recover please recreate corresponding ArangoSearch link '" << linkId << "'";
           return false;
        }
      } else {
          builder.add(VPackValue(transaction::helpers::extractIdString(
              trx.resolver(), slice, document)));
      }

      slice = builder.slice();
      // a builder is destroyed but a buffer is alive
    }
    out.write_bytes(slice.start(), slice.byteSize());
  }
  return true;
}

}  // namespace iresearch
}  // namespace arangodb

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------
