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

#include "IResearchDocument.h"

#include "Basics/Endian.h"
#include "Basics/StaticStrings.h"
#include "VelocyPackHelper.h"
#include "Cluster/ServerState.h"
#include "IResearchAnalyzerFeature.h"
#include "IResearch/GeoAnalyzer.h"
#include "IResearch/IResearchCommon.h"
#include "IResearch/IResearchKludge.h"
#include "IResearch/IResearchPrimaryKeyFilter.h"
#include "IResearch/IResearchViewMeta.h"
#include "IResearch/IResearchVPackTermAttribute.h"
#include "IResearch/VelocyPackHelper.h"
#include "Logger/LogMacros.h"
#include "Misc.h"
#include "Transaction/Helpers.h"
#include "Transaction/Methods.h"

#include "search/term_filter.hpp"

#include "utils/log.hpp"
#include "Basics/DownCast.h"

namespace {

// ----------------------------------------------------------------------------
// --SECTION--                                           Primary key endianness
// ----------------------------------------------------------------------------

constexpr bool const BigEndian = false;

template<bool IsLittleEndian>
struct PrimaryKeyEndianness {
  static uint64_t hostToPk(uint64_t value) {
    return arangodb::basics::hostToLittle(value);
  }
  static uint64_t pkToHost(uint64_t value) {
    return arangodb::basics::littleToHost(value);
  }
};  // PrimaryKeyEndianness

template<>
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

irs::string_ref const PK_COLUMN("@_PK");
size_t constexpr DEFAULT_POOL_SIZE = 8;  // arbitrary value
irs::unbounded_object_pool<arangodb::iresearch::AnalyzerPool::Builder>
    StringStreamPool(DEFAULT_POOL_SIZE);
irs::unbounded_object_pool<arangodb::iresearch::AnalyzerPool::Builder>
    NullStreamPool(DEFAULT_POOL_SIZE);
irs::unbounded_object_pool<arangodb::iresearch::AnalyzerPool::Builder>
    BoolStreamPool(DEFAULT_POOL_SIZE);
irs::unbounded_object_pool<arangodb::iresearch::AnalyzerPool::Builder>
    NumericStreamPool(DEFAULT_POOL_SIZE);
std::initializer_list<irs::type_info::type_id> NumericStreamFeatures{
    irs::type<irs::granularity_prefix>::id()};

// appends the specified 'value' to 'out'
inline void append(std::string& out, size_t value) {
  auto const size = out.size();  // intial size
  out.resize(size + 21);         // enough to hold all numbers up to 64-bits
  auto const written = sprintf(&out[size], IR_SIZE_T_SPECIFIER, value);
  out.resize(size + written);
}

inline bool canHandleValue(
    std::string const& key, VPackSlice const& value,
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
inline arangodb::iresearch::FieldMeta const* findMeta(
    irs::string_ref key, arangodb::iresearch::FieldMeta const* context) {
  TRI_ASSERT(context);

  auto const* meta = context->_fields.findPtr(key);
  return meta ? meta->get() : context;
}

inline bool inObjectFiltered(std::string& buffer,
                             arangodb::iresearch::FieldMeta const*& context,
                             arangodb::iresearch::IteratorValue const& value) {
  irs::string_ref key;

  if (!arangodb::iresearch::keyFromSlice(value.key, key)) {
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

inline bool inObject(std::string& buffer,
                     arangodb::iresearch::FieldMeta const*& context,
                     arangodb::iresearch::IteratorValue const& value) {
  irs::string_ref key;

  if (!arangodb::iresearch::keyFromSlice(value.key, key)) {
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

inline bool inArray(std::string& buffer,
                    arangodb::iresearch::FieldMeta const*& context,
                    arangodb::iresearch::IteratorValue const& value) noexcept {
  return canHandleValue(buffer, value.value, *context);
}

typedef bool (*Filter)(std::string& buffer,
                       arangodb::iresearch::FieldMeta const*& context,
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

inline Filter getFilter(VPackSlice value,
                        arangodb::iresearch::FieldMeta const& meta) noexcept {
  TRI_ASSERT(arangodb::iresearch::isArrayOrObject(value));

  return valueAcceptors[4 * value.isArray() + 2 * meta._trackListPositions +
                        meta._includeAllFields];
}

typedef bool (*InvertedIndexFilter)(std::string& buffer,
                       arangodb::iresearch::IResearchInvertedIndexMeta::FieldRecord const*& context,
                       arangodb::iresearch::IteratorValue const& value);

template<bool defaultAccept>
inline bool acceptAll(
    std::string& buffer,
    arangodb::iresearch::IResearchInvertedIndexMeta::FieldRecord const*&
        context,
    arangodb::iresearch::IteratorValue const& value) {
  irs::string_ref key;

  if (!arangodb::iresearch::keyFromSlice(value.key, key)) {
    return false;
  }

  buffer.append(key.c_str(), key.size());
  for (auto& nested : context->_fields) {
    auto fullName = nested.path();
    // FIXME: starts_with will not work in general/ need to check up to first nesting delimiter!
    if (fullName.starts_with(buffer)) {
      if (buffer == nested.attributeString()) {
        if (value.value.isObject() &&
            !nested.analyzer()->accepts(
                arangodb::iresearch::AnalyzerValueType::Object)) {
          THROW_ARANGO_EXCEPTION_FORMAT(
              TRI_ERROR_NOT_IMPLEMENTED,
              "Inverted index does not support indexing objects and configured "
              "analyzer does "
              "not accept objects. Please use another analyzer to process an "
              "object or exclude field '%s' "
              "from index definition",
              buffer.c_str());
        } else if (nested.isArray() && !value.value.isArray()) {
          // we were expecting an array but something else were given
          // this case is just skipped. Just like regular indicies do
          return false;
        } else if (value.value.isArray() && !nested.isArray() &&
                   nested.nested().empty()) {
          if (!nested.analyzer()->accepts(
                  arangodb::iresearch::AnalyzerValueType::Array)) {
            THROW_ARANGO_EXCEPTION_FORMAT(
                TRI_ERROR_NOT_IMPLEMENTED,
                "Configured analyzer does not accepts arrays and field has no "
                "expansion set. "
                "Please use another analyzer to process an array or exclude "
                "field '%s' "
                "from index definition or enable expansion",
                buffer.c_str());
          }
        }
      }
      if (fullName == buffer) {
        context = &nested;
      }
      return true;
    }
  }
  return defaultAccept;
}

inline bool inArrayInverted(std::string& buffer,
                           arangodb::iresearch::IResearchInvertedIndexMeta::FieldRecord const*& context,
                           arangodb::iresearch::IteratorValue const& value) {
  if (context->trackListPositions()) {
    buffer += arangodb::iresearch::NESTING_LIST_OFFSET_PREFIX;
    append(buffer, value.pos);
    buffer += arangodb::iresearch::NESTING_LIST_OFFSET_SUFFIX;
  } else {
    buffer += "[*]";
  }
  return true;
}

InvertedIndexFilter const valueAcceptorsInverted[] = {
  &acceptAll<false>,
  &acceptAll<true>,
  &inArrayInverted,
  &inArrayInverted
};

inline InvertedIndexFilter getFilter(
    VPackSlice value,
    arangodb::iresearch::IResearchInvertedIndexMeta::FieldRecord const&
        meta) noexcept {
  TRI_ASSERT(arangodb::iresearch::isArrayOrObject(value));

  return valueAcceptorsInverted
      [value.isArray() * 2 +
       meta._includeAllFields];
}

std::string getDocumentId(irs::string_ref collection, VPackSlice document) {
  std::string_view const key =
      arangodb::transaction::helpers::extractKeyPart(document);
  if (key.empty()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                   "failed to extract key value from document");
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

/*static*/ void Field::setPkValue(Field& field,
                                  LocalDocumentId::BaseType const& pk) {
  field._name = PK_COLUMN;
  field._indexFeatures = irs::IndexFeatures::NONE;
  field._fieldFeatures = {};
  field._storeValues = ValueStorage::VALUE;
  field._value =
      irs::bytes_ref(reinterpret_cast<irs::byte_type const*>(&pk), sizeof(pk));
  field._analyzer = StringStreamPool.emplace(AnalyzerPool::StringStreamTag());
  auto& sstream = basics::downCast<irs::string_token_stream>(*field._analyzer);
  sstream.reset(field._value);
}

// ----------------------------------------------------------------------------
// --SECTION--                                     FieldIterator implementation
// ----------------------------------------------------------------------------
template<typename IndexMetaStruct, typename LevelMeta>
FieldIterator<IndexMetaStruct, LevelMeta>::FieldIterator(arangodb::transaction::Methods& trx,
                             irs::string_ref collection, IndexId linkId)
    : _trx(&trx),
      _collection(collection),
      _linkId(linkId),
      _isDBServer(ServerState::instance()->isDBServer()),
      _disableFlush(false) {
  // initialize iterator's value
}

template<typename IndexMetaStruct, typename LevelMeta>
void FieldIterator<IndexMetaStruct, LevelMeta>::reset(VPackSlice doc, IndexMetaStruct const& linkMeta) {
  _slice = doc;
  _begin = nullptr;
  _end = nullptr;
  _currentTypedAnalyzer.reset();
  _currentTypedAnalyzerValue = nullptr;
  _primitiveTypeResetter = nullptr;
  _stack.clear();
  _nameBuffer.clear();
  _disableFlush = false;

  // push the provided 'doc' on stack and initialize current value
  auto const filter = getFilter(doc, static_cast<LevelMeta const&>(linkMeta));
#ifdef USE_ENTERPRISE
  // this is set for root level as general mark.
  _hasNested = MetaTraits::hasNested(linkMeta);
  pushLevel(doc, static_cast<LevelMeta const&>(linkMeta), filter);
#else
  _stack.emplace_back(doc, 0, static_cast<LevelMeta const&>(linkMeta), filter);
#endif
  next();
}

template<typename IndexMetaStruct, typename LevelMeta>
void FieldIterator<IndexMetaStruct, LevelMeta>::setBoolValue(VPackSlice const value) {
  TRI_ASSERT(value.isBool());

  arangodb::iresearch::kludge::mangleBool(_nameBuffer);

  // init stream
  auto stream = BoolStreamPool.emplace(AnalyzerPool::BooleanStreamTag());
  static_cast<irs::boolean_token_stream*>(stream.get())->reset(value.getBool());

  // set field properties
  _value._name = _nameBuffer;
  _value._analyzer = std::move(stream);
  _value._indexFeatures = irs::IndexFeatures::NONE;
  _value._fieldFeatures = {};
}

template<typename IndexMetaStruct, typename LevelMeta>
void FieldIterator<IndexMetaStruct, LevelMeta>::setNumericValue(VPackSlice const value) {
  TRI_ASSERT(value.isNumber());

  arangodb::iresearch::kludge::mangleNumeric(_nameBuffer);

  // init stream
  auto stream = NumericStreamPool.emplace(AnalyzerPool::NumericStreamTag());
  static_cast<irs::numeric_token_stream*>(stream.get())
      ->reset(value.getNumber<double>());

  // set field properties
  _value._name = _nameBuffer;
  _value._analyzer = std::move(stream);
  _value._indexFeatures = irs::IndexFeatures::NONE;
  _value._fieldFeatures = {NumericStreamFeatures.begin(),
                           NumericStreamFeatures.size()};
}

template<typename IndexMetaStruct, typename LevelMeta>
void FieldIterator<IndexMetaStruct, LevelMeta>::setNullValue(VPackSlice const value) {
  TRI_ASSERT(value.isNull());

  arangodb::iresearch::kludge::mangleNull(_nameBuffer);

  // init stream
  auto stream = NullStreamPool.emplace(AnalyzerPool::NullStreamTag());
  static_cast<irs::null_token_stream*>(stream.get())->reset();

  // set field properties
  _value._name = _nameBuffer;
  _value._analyzer = std::move(stream);
  _value._indexFeatures = irs::IndexFeatures::NONE;
  _value._fieldFeatures = {};
}

template<typename IndexMetaStruct, typename LevelMeta>
bool FieldIterator<IndexMetaStruct, LevelMeta>::setValue(VPackSlice const value,
                             FieldMeta::Analyzer const& valueAnalyzer) {
  TRI_ASSERT(  // assert
      (value.isCustom() &&
       _nameBuffer == arangodb::StaticStrings::IdString)  // custom string
      || value.isObject() || value.isArray() ||
      value.isString());  // verbatim string

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
              << ". To recover please recreate corresponding ArangoSearch link "
                 "'"
              << _linkId << "'";
          return false;
        }
      } else {
        _valueBuffer = transaction::helpers::extractIdString(_trx->resolver(),
                                                             value, _slice);
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
    case AnalyzerValueType::Bool: {
      if (!analyzer->next()) {
        return false;
      }
      _currentTypedAnalyzer = std::move(analyzer);
      _currentTypedAnalyzerValue =
          irs::get<VPackTermAttribute>(*_currentTypedAnalyzer);
      TRI_ASSERT(_currentTypedAnalyzerValue);
      setBoolValue(_currentTypedAnalyzerValue->value);
      _primitiveTypeResetter = [](irs::token_stream* stream,
                                  VPackSlice slice) -> void {
        TRI_ASSERT(stream);
        TRI_ASSERT(slice.isBool());
        auto* bool_stream = basics::downCast<irs::boolean_token_stream>(stream);
        bool_stream->reset(slice.getBool());
      };
    } break;
    case AnalyzerValueType::Number: {
      if (!analyzer->next()) {
        return false;
      }
      _currentTypedAnalyzer = std::move(analyzer);
      _currentTypedAnalyzerValue =
          irs::get<VPackTermAttribute>(*_currentTypedAnalyzer);
      TRI_ASSERT(_currentTypedAnalyzerValue);
      setNumericValue(_currentTypedAnalyzerValue->value);
      _primitiveTypeResetter = [](irs::token_stream* stream,
                                  VPackSlice slice) -> void {
        TRI_ASSERT(stream);
        TRI_ASSERT(slice.isNumber());
        auto* number_stream =
            basics::downCast<irs::numeric_token_stream>(stream);
        number_stream->reset(slice.getNumber<double>());
      };
    } break;
    default: {
      if constexpr (std::is_same_v<IndexMetaStruct, IResearchInvertedIndexMeta>) {
        iresearch::kludge::mangleField(_nameBuffer, false, valueAnalyzer);
      } else {
        iresearch::kludge::mangleField(_nameBuffer, true, valueAnalyzer);
      }
      _value._analyzer = std::move(analyzer);
      _value._fieldFeatures = pool->fieldFeatures();
      _value._indexFeatures = pool->indexFeatures();
      _value._name = _nameBuffer;
    } break;
  }
  auto* storeFunc = pool->storeFunc();
  if (storeFunc) {
    auto const valueSlice =
        storeFunc(_currentTypedAnalyzer ? _currentTypedAnalyzer.get()
                                        : _value._analyzer.get(),
                  value, _buffer);

    if (!valueSlice.isNone()) {
      _value._value = iresearch::ref<irs::byte_type>(valueSlice);
      _value._storeValues = std::max(ValueStorage::VALUE, _value._storeValues);
    }
  }

  return true;
}

template<typename IndexMetaStruct, typename LevelMeta>
void FieldIterator<IndexMetaStruct, LevelMeta>::next() {
  TRI_ASSERT(valid());

  if (_currentTypedAnalyzer) {
    if (_currentTypedAnalyzer->next()) {
      TRI_ASSERT(_primitiveTypeResetter);
      TRI_ASSERT(_currentTypedAnalyzerValue);
      TRI_ASSERT(_value._analyzer.get());
      _primitiveTypeResetter(_value._analyzer.get(),
                             _currentTypedAnalyzerValue->value);
      return;
    } else {
      _currentTypedAnalyzer.reset();
    }
  }

  auto const* context = top().meta;

  // restore value
  _value._storeValues = context->_storeValues;
  _value._value = irs::bytes_ref::NIL;
#ifdef USE_ENTERPRISE
  _value._root = false;
#endif
  _needDoc = false;
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
#ifdef USE_ENTERPRISE
        popLevel();
#else
        _stack.pop_back();
#endif
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

      if constexpr (std::is_same_v<IndexMetaStruct,
                                   IResearchInvertedIndexMeta>) {
        if (level.filter == &acceptOnlyObjects) {
          // Requesting nested document
          _needDoc = true;
        } 
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
#ifdef USE_ENTERPRISE
          // FIXME: move to setRoot or smth
          if constexpr (std::is_same_v<IndexMetaStruct,
                                       IResearchInvertedIndexMeta>) {
            if (level.isRoot) {
              _value._root = true;
              kludge::mangleNested(_nameBuffer);
              _value._name = _nameBuffer;
              _value._analyzer.reset();
              _value._indexFeatures = irs::IndexFeatures::NONE;
              _value._fieldFeatures = {};
              return;
            }
          }
          auto filter = getFilter(valueSlice, *context);
          bool setAnalyzers = pushLevel(valueSlice, *context, filter);
#else
          bool setAnalyzers = true;
          _stack.emplace_back(valueSlice, _nameBuffer.size(), *context,
                              false, getFilter(valueSlice, *context));
#endif
          if (setAnalyzers) {
            auto const& analyzers = context->_analyzers;
            _begin = analyzers.data() + context->_primitiveOffset;
            _end = analyzers.data() + analyzers.size();
          } 
          _prefixLength = _nameBuffer.size();  // save current prefix length
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

          _prefixLength = _nameBuffer.size();  // save current prefix length
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

/*static*/ LocalDocumentId::BaseType DocumentPrimaryKey::encode(
    LocalDocumentId value) noexcept {
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
      *reinterpret_cast<arangodb::LocalDocumentId::BaseType const*>(
          in.c_str())));

  return true;
}

StoredValue::StoredValue(transaction::Methods const& t, irs::string_ref cn,
                         VPackSlice const doc, IndexId lid)
    : trx(t),
      document(doc),
      collection(cn),
      linkId(lid),
      isDBServer(ServerState::instance()->isDBServer()) {}

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
                 storedValue.second[0].name ==
                     arangodb::StaticStrings::IdString);
      buffer.reset();
      VPackBuilder builder(buffer);
      if (isDBServer) {
        if (!collection.empty()) {
          builder.add(VPackValue(getDocumentId(collection, document)));
        } else {
          LOG_TOPIC("bf98c", WARN, arangodb::iresearch::TOPIC)
              << "Value for `_id` attribute could not be stored for document "
              << transaction::helpers::extractKeyFromDocument(document)
                     .toString()
              << ". To recover please recreate corresponding ArangoSearch link "
                 "'"
              << linkId << "'";
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
//
//InvertedIndexFieldIterator::InvertedIndexFieldIterator(
//    arangodb::transaction::Methods&, irs::string_ref,
//    IndexId indexId)
//    : _indexId(indexId) {
//  // we need id column for range queries
//  _value._storeValues = ValueStorage::ID;
//}
//
//void InvertedIndexFieldIterator::next() {
//  TRI_ASSERT(valid());
//  if (_currentTypedAnalyzer) {
//    if (_currentTypedAnalyzer->next()) {
//      TRI_ASSERT(_primitiveTypeResetter);
//      TRI_ASSERT(_currentTypedAnalyzerValue);
//      TRI_ASSERT(_value._analyzer.get());
//      _primitiveTypeResetter(_value._analyzer.get(),
//                             _currentTypedAnalyzerValue->value);
//      return;
//    }
//    _currentTypedAnalyzer.reset();
//  }
//  while (_begin != _end) {
//    _valueSlice = VPackSlice::noneSlice();
//    while (!_arrayStack.empty()) {
//      if (_arrayStack.back().valid()) {
//        if (_begin->expansion().empty()) {
//          _valueSlice = *_arrayStack.back();
//        } else {
//          // for array subobjects we index "null" in case of absence as declared
//          // for other indicies
//          _valueSlice = get(*_arrayStack.back(), _begin->expansion(),
//                            VPackSlice::nullSlice());
//        }
//        ++_arrayStack.back();
//        _nameBuffer.resize(_prefixLength);  // FIXME: just clear should work!
//        break;
//      }
//      _arrayStack.pop_back();
//    }
//    if (_arrayStack.empty()) {
//      while (_valueSlice.isNone()) {
//        if (++_begin == _end) {
//          TRI_ASSERT(!valid());
//          return;  // exhausted
//        }
//        _valueSlice = get(_slice, _begin->attribute(), VPackSlice::noneSlice());
//        if (!_valueSlice.isNone() && !_valueSlice.isArray() &&
//            _begin->isArray()) {
//          _valueSlice = VPackSlice::noneSlice();
//        }
//      }
//      _nameBuffer.clear();
//    }
//    if (!_valueSlice.isNone()) {
//      if (_nameBuffer.empty()) {
//#ifdef USE_ENTERPRISE
//        //if (!_parentNameBuffer.empty()) {
//        //  _nameBuffer = _parentNameBuffer;
//        //  _nameBuffer += NESTING_LEVEL_DELIMITER;
//        //}
//#endif
//        bool isFirst = true;
//        for (auto& a : _begin->attribute()) {
//          if (!isFirst) {
//            _nameBuffer += NESTING_LEVEL_DELIMITER;
//          }
//          _nameBuffer.append(a.name);
//          isFirst = false;
//        }
//        if (!_begin->expansion().empty()) {
//          _nameBuffer.append("[*]");
//        }
//        for (auto& a : _begin->expansion()) {
//          _nameBuffer += NESTING_LEVEL_DELIMITER;
//          _nameBuffer.append(a.name);
//        }
//      }
//      TRI_ASSERT(_begin->analyzer()._pool);
//      switch (_valueSlice.type()) {
//        case VPackValueType::Null:
//          setNullValue();
//          return;
//        case VPackValueType::Bool:
//          setBoolValue(_valueSlice);
//          return;
//        case VPackValueType::Object:
//          if (setValue(_valueSlice, _begin->analyzer())) {
//            return;
//          }
//          THROW_ARANGO_EXCEPTION_FORMAT(
//              TRI_ERROR_NOT_IMPLEMENTED,
//              "Inverted index does not support indexing objects and configured "
//              "analyzer does "
//              "not accept objects. Please use another analyzer to process an "
//              "object or exclude field '%s' "
//              "from index definition",
//              _nameBuffer.c_str());
//          return;  // never reached
//        case VPackValueType::Array: {
//#ifdef USE_ENTERPRISE
//          if (!_begin->nested().empty()) {
//            return;
//          }
//#endif
//          if (_begin->isArray() && _arrayStack.empty()) {
//            _arrayStack.push_back(VPackArrayIterator(_valueSlice));
//            _prefixLength = _nameBuffer.size();
//          } else if (setValue(_valueSlice, _begin->analyzer())) {
//            return;
//          } else {
//            THROW_ARANGO_EXCEPTION_FORMAT(
//                TRI_ERROR_NOT_IMPLEMENTED,
//                "Configured analyzer does not accepts arrays and field has no "
//                "expansion set. "
//                "Please use another analyzer to process an array or exclude "
//                "field '%s' "
//                "from index definition or enable expansion",
//                _nameBuffer.c_str());
//          }
//          break;
//        }
//        case VPackValueType::Double:
//        case VPackValueType::Int:
//        case VPackValueType::UInt:
//        case VPackValueType::SmallInt:
//          setNumericValue(_valueSlice);
//          return;
//        case VPackValueType::String: {
//          setValue(_valueSlice, _begin->analyzer());
//          return;
//        }
//        default:
//          break;
//      }
//    }
//  }
//}
//
//void InvertedIndexFieldIterator::setBoolValue(VPackSlice const value) {
//  TRI_ASSERT(value.isBool());
//
//  arangodb::iresearch::kludge::mangleBool(_nameBuffer);
//
//  // init stream
//  auto stream = BoolStreamPool.emplace(AnalyzerPool::BooleanStreamTag());
//  static_cast<irs::boolean_token_stream*>(stream.get())->reset(value.getBool());
//
//  // set field properties
//  _value._name = _nameBuffer;
//  _value._analyzer = std::move(stream);
//  _value._indexFeatures = irs::IndexFeatures::NONE;
//  _value._fieldFeatures = {};
//}
//
//void InvertedIndexFieldIterator::setNumericValue(VPackSlice const value) {
//  TRI_ASSERT(value.isNumber());
//
//  arangodb::iresearch::kludge::mangleNumeric(_nameBuffer);
//
//  // init stream
//  auto stream = NumericStreamPool.emplace(AnalyzerPool::NumericStreamTag());
//  static_cast<irs::numeric_token_stream*>(stream.get())
//      ->reset(value.getNumber<double>());
//
//  // set field properties
//  _value._name = _nameBuffer;
//  _value._analyzer = std::move(stream);  // FIXME don't use shared_ptr
//  _value._indexFeatures = irs::IndexFeatures::NONE;
//  _value._fieldFeatures = {NumericStreamFeatures.begin(),
//                           NumericStreamFeatures.size()};
//}
//
//void InvertedIndexFieldIterator::setNullValue() {
//  arangodb::iresearch::kludge::mangleNull(_nameBuffer);
//
//  // init stream
//  auto stream = NullStreamPool.emplace(AnalyzerPool::NullStreamTag());
//  static_cast<irs::null_token_stream*>(stream.get())->reset();
//
//  // set field properties
//  _value._name = _nameBuffer;
//  _value._analyzer = std::move(stream);  // FIXME don't use shared_ptr
//  _value._indexFeatures = irs::IndexFeatures::NONE;
//  _value._fieldFeatures = {};
//}
//
//bool InvertedIndexFieldIterator::setValue(
//    VPackSlice const value, FieldMeta::Analyzer const& valueAnalyzer) {
//  TRI_ASSERT(value.isObject() || value.isArray() || value.isString());
//
//  auto& pool = valueAnalyzer._pool;
//
//  if (!pool) {
//    LOG_TOPIC("189db", WARN, iresearch::TOPIC)
//        << "got nullptr analyzer factory";
//
//    return false;
//  }
//
//  irs::string_ref valueRef;
//  AnalyzerValueType valueType{AnalyzerValueType::Undefined};
//
//  switch (value.type()) {
//    case VPackValueType::Array: {
//      valueRef = iresearch::ref<char>(value);
//      valueType = AnalyzerValueType::Array;
//    } break;
//    case VPackValueType::Object: {
//      valueRef = iresearch::ref<char>(value);
//      valueType = AnalyzerValueType::Object;
//    } break;
//    case VPackValueType::String: {
//      valueRef = iresearch::getStringRef(value);
//      valueType = AnalyzerValueType::String;
//    } break;
//    default:
//      TRI_ASSERT(false);
//      return false;
//  }
//
//  if (!pool->accepts(valueType)) {
//    return false;
//  }
//
//  // init stream
//  auto analyzer = pool->get();
//
//  if (!analyzer) {
//    LOG_TOPIC("22eeb", WARN, arangodb::iresearch::TOPIC)
//        << "got nullptr from analyzer factory, name '" << pool->name() << "'";
//    return false;
//  }
//  if (!analyzer->reset(valueRef)) {
//    return false;
//  }
//  // set field properties
//  switch (pool->returnType()) {
//    case AnalyzerValueType::Bool: {
//      if (!analyzer->next()) {
//        return false;
//      }
//      _currentTypedAnalyzer = std::move(analyzer);
//      _currentTypedAnalyzerValue =
//          irs::get<VPackTermAttribute>(*_currentTypedAnalyzer);
//      TRI_ASSERT(_currentTypedAnalyzerValue);
//      setBoolValue(_currentTypedAnalyzerValue->value);
//      _primitiveTypeResetter = [](irs::token_stream* stream,
//                                  VPackSlice slice) -> void {
//        TRI_ASSERT(stream);
//        TRI_ASSERT(slice.isBool());
//        auto* bool_stream = basics::downCast<irs::boolean_token_stream>(stream);
//        bool_stream->reset(slice.getBool());
//      };
//    } break;
//    case AnalyzerValueType::Number: {
//      if (!analyzer->next()) {
//        return false;
//      }
//      _currentTypedAnalyzer = std::move(analyzer);
//      _currentTypedAnalyzerValue =
//          irs::get<VPackTermAttribute>(*_currentTypedAnalyzer);
//      TRI_ASSERT(_currentTypedAnalyzerValue);
//      setNumericValue(_currentTypedAnalyzerValue->value);
//      _primitiveTypeResetter = [](irs::token_stream* stream,
//                                  VPackSlice slice) -> void {
//        TRI_ASSERT(stream);
//        TRI_ASSERT(slice.isNumber());
//        auto* number_stream =
//            basics::downCast<irs::numeric_token_stream>(stream);
//        number_stream->reset(slice.getNumber<double>());
//      };
//    } break;
//    default: {
//      iresearch::kludge::mangleField(_nameBuffer, false, valueAnalyzer);
//      _value._analyzer = std::move(analyzer);
//      _value._fieldFeatures = pool->fieldFeatures();
//      _value._indexFeatures = pool->indexFeatures();
//      _value._name = _nameBuffer;
//    } break;
//  }
//  auto* storeFunc = pool->storeFunc();
//  if (storeFunc) {
//    auto const valueSlice =
//        storeFunc(_currentTypedAnalyzer ? _currentTypedAnalyzer.get()
//                                        : _value._analyzer.get(),
//                  value, _buffer);
//
//    if (!valueSlice.isNone()) {
//      _value._value = iresearch::ref<irs::byte_type>(valueSlice);
//      _value._storeValues = std::max(ValueStorage::VALUE, _value._storeValues);
//    }
//  }
//  return true;
//}

}  // namespace iresearch
}  // namespace arangodb

#if USE_ENTERPRISE
#include "Enterprise/IResearch/IResearchDocumentEE.hpp"
#endif

template arangodb::iresearch::FieldIterator<arangodb::iresearch::FieldMeta,
                                            arangodb::iresearch::FieldMeta>;

template arangodb::iresearch::FieldIterator<arangodb::iresearch::IResearchInvertedIndexMeta,
                                            arangodb::iresearch::IResearchInvertedIndexMeta::FieldRecord>;

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------
