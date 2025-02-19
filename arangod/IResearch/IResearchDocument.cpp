////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
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
#include "IResearch/IResearchLinkMeta.h"
#include "IResearch/IResearchInvertedIndexMeta.h"
#include "Logger/LogMacros.h"
#include "Misc.h"
#include "Transaction/Helpers.h"
#include "Transaction/Methods.h"

#include "search/term_filter.hpp"

#include "utils/log.hpp"
#include "Basics/DownCast.h"

#include <absl/strings/str_cat.h>

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

size_t constexpr DEFAULT_POOL_SIZE = 8;  // arbitrary value
irs::unbounded_object_pool<arangodb::iresearch::AnalyzerPool::Builder>
    StringStreamPool(DEFAULT_POOL_SIZE);
irs::unbounded_object_pool<arangodb::iresearch::AnalyzerPool::Builder>
    NullStreamPool(DEFAULT_POOL_SIZE);
irs::unbounded_object_pool<arangodb::iresearch::AnalyzerPool::Builder>
    BoolStreamPool(DEFAULT_POOL_SIZE);
irs::unbounded_object_pool<arangodb::iresearch::AnalyzerPool::Builder>
    NumericStreamPool(DEFAULT_POOL_SIZE);
static constexpr std::initializer_list<irs::type_info::type_id>
    NumericStreamFeatures{irs::type<irs::granularity_prefix>::id()};

bool canHandleValue(std::string const& key, VPackSlice const& value,
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

bool canHandleValue(
    std::string const& key, VPackSlice const& value,
    arangodb::iresearch::IResearchInvertedIndexMetaIndexingContext const&
        context) noexcept {
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
      return true;
    default:
      return false;
  }
}

// returns 'context' in case if can't find the specified 'field'
arangodb::iresearch::FieldMeta const* findMeta(
    std::string_view key, arangodb::iresearch::FieldMeta const* context) {
  TRI_ASSERT(context);

  auto meta = context->_fields.find(key);
  return meta != context->_fields.end() ? &meta->second : context;
}

bool inObjectFiltered(std::string& buffer,
                      arangodb::iresearch::FieldMeta const*& context,
                      arangodb::iresearch::IteratorValue const& value) {
  std::string_view key;

  if (!arangodb::iresearch::keyFromSlice(value.key, key)) {
    return false;
  }

  auto const* meta = findMeta(key, context);

  if (meta == context) {
    return false;
  }

  buffer.append(key.data(), key.size());
  context = meta;

  return canHandleValue(buffer, value.value, *context);
}

#ifdef USE_ENTERPRISE
bool inNestedObjectFiltered(std::string& buffer,
                            arangodb::iresearch::FieldMeta const*& context,
                            arangodb::iresearch::IteratorValue const& value);
#endif

bool inObject(std::string& buffer,
              arangodb::iresearch::FieldMeta const*& context,
              arangodb::iresearch::IteratorValue const& value) {
  std::string_view key;

  if (!arangodb::iresearch::keyFromSlice(value.key, key)) {
    return false;
  }

  buffer.append(key.data(), key.size());
  context = findMeta(key, context);

  return canHandleValue(buffer, value.value, *context);
}

bool inArrayOrdered(std::string& buffer,
                    arangodb::iresearch::FieldMeta const*& context,
                    arangodb::iresearch::IteratorValue const& value) {
  // NESTING_LIST_OFFSET_PREFIX value.pos NESTING_LIST_OFFSET_SUFFIX
  absl::StrAppend(&buffer, "[", value.pos, "]");
  return canHandleValue(buffer, value.value, *context);
}

bool inArray(std::string& buffer,
             arangodb::iresearch::FieldMeta const*& context,
             arangodb::iresearch::IteratorValue const& value) noexcept {
  return canHandleValue(buffer, value.value, *context);
}

using Filter = bool (*)(std::string& buffer,
                        arangodb::iresearch::FieldMeta const*& context,
                        arangodb::iresearch::IteratorValue const& value);

Filter const valueAcceptors[] = {
    // type == Object, trackListPositions == false, includeAllValues == false
    &inObjectFiltered,
    // type == Object, trackListPositions == false, includeAllValues == true
    &inObject,
    // type == Object, trackListPositions == true , includeAllValues == false
    &inObjectFiltered,
    // type == Object, trackListPositions == true , includeAllValues == true
    &inObject,
    // type == Array , trackListPositions == false, includeAllValues == false
    &inArray,
    // type == Array , trackListPositions == false, includeAllValues == true
    &inArray,
    // type == Array , trackListPositions == true , includeAllValues == false
    &inArrayOrdered,
    // type == Array , trackListPositions == true , includeAllValues == true
    &inArrayOrdered};

Filter getFilter(VPackSlice value, arangodb::iresearch::FieldMeta const& meta,
                 bool nested) noexcept {
  TRI_ASSERT(arangodb::iresearch::isArrayOrObject(value));

#ifdef USE_ENTERPRISE
  if (nested) {
    return &inNestedObjectFiltered;
  }
#endif

  return valueAcceptors[4 * value.isArray() + 2 * meta._trackListPositions +
                        meta._includeAllFields];
}

using InvertedIndexFilter = bool (*)(
    std::string& buffer,
    arangodb::iresearch::IResearchInvertedIndexMetaIndexingContext const*&
        context,
    arangodb::iresearch::IteratorValue const& value);

template<bool defaultAccept, bool nested>
bool acceptAll(
    std::string& buffer,
    arangodb::iresearch::IResearchInvertedIndexMetaIndexingContext const*&
        context,
    arangodb::iresearch::IteratorValue const& value) {
  std::string_view key;

  if (!arangodb::iresearch::keyFromSlice(value.key, key)) {
    return false;
  }

  buffer.append(key.data(), key.size());
  auto& container = nested ? context->_nested : context->_fields;
  auto subContext = container.find(key);
  if (subContext != container.end()) {
    context = &subContext->second;
    if (!context->_nested.empty() && context->_fields.empty()) {
      // this is just nested root. But not indexed by itself
      return false;
    }
    if (!context->_isSearchField && context->_isArray &&
        !value.value.isArray()) {
      // we were expecting an array but something else were given
      // this case is just skipped. Just like regular indicies do.
      return false;
    } else if (!context->_isSearchField && value.value.isObject() &&
               !context->_includeAllFields && context->_fields.empty() &&
               !context->_analyzers->front()._pool->accepts(
                   arangodb::iresearch::AnalyzerValueType::Object)) {
      THROW_ARANGO_EXCEPTION_FORMAT(
          TRI_ERROR_NOT_IMPLEMENTED,
          "Inverted index does not support indexing objects and "
          "configured analyzer does not accept objects. Please use "
          "another analyzer to process an object or exclude field '%s' "
          " from index definition",
          buffer.c_str());
    } else if (!context->_isSearchField && value.value.isArray() &&
               !context->_isArray &&
               !context->_analyzers->front()._pool->accepts(
                   arangodb::iresearch::AnalyzerValueType::Array)) {
      THROW_ARANGO_EXCEPTION_FORMAT(
          TRI_ERROR_NOT_IMPLEMENTED,
          "Configured analyzer does not accepts arrays and field has no "
          "expansion set. "
          "Please use another analyzer to "
          " process an array or exclude "
          "field '%s' "
          "from index definition or enable expansion",
          buffer.c_str());
    }
  }

  if (subContext == container.end() && !defaultAccept) {
    return false;
  }

  return canHandleValue(buffer, value.value, *context);
}

bool inArrayInverted(
    std::string& buffer,
    arangodb::iresearch::IResearchInvertedIndexMetaIndexingContext const*&
        context,
    arangodb::iresearch::IteratorValue const& value) {
  if (context->_trackListPositions) {
    // NESTING_LIST_OFFSET_PREFIX value.pos NESTING_LIST_OFFSET_SUFFIX
    absl::StrAppend(&buffer, "[", value.pos, "]");
  } else {
    if (!context->_isSearchField) {
      buffer.append("[*]");
    }
  }
  return true;
}

InvertedIndexFilter const valueAcceptorsInverted[] = {
    &acceptAll<false, false>, &acceptAll<true, false>, &inArrayInverted,
    &inArrayInverted};

InvertedIndexFilter getFilter(
    VPackSlice value,
    arangodb::iresearch::IResearchInvertedIndexMetaIndexingContext const& meta,
    bool nested) noexcept {
  TRI_ASSERT(arangodb::iresearch::isArrayOrObject(value));
  if (nested) {
    return &acceptAll<false, true>;
  }
  return valueAcceptorsInverted[value.isArray() * 2 + meta._includeAllFields];
}

std::string getDocumentId(std::string_view collection, VPackSlice document) {
  auto const key = arangodb::transaction::helpers::extractKeyPart(document);
  if (key.empty()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                   "failed to extract key value from document");
  }
  return absl::StrCat(collection, "/", key);
}

}  // namespace

namespace arangodb {
namespace iresearch {

// ----------------------------------------------------------------------------
// --SECTION--                                             Field implementation
// ----------------------------------------------------------------------------

void Field::setPkValue(Field& field, LocalDocumentId::BaseType const& pk) {
  field._name = PK_COLUMN;
  field._indexFeatures = irs::IndexFeatures::NONE;
  field._fieldFeatures = {};
  field._storeValues = ValueStorage::VALUE;
  field._value =
      irs::bytes_view(reinterpret_cast<irs::byte_type const*>(&pk), sizeof(pk));
  field._analyzer = StringStreamPool.emplace(AnalyzerPool::StringStreamTag());
  auto& sstream = basics::downCast<irs::string_token_stream>(*field._analyzer);
  sstream.reset(field._value);
}

// ----------------------------------------------------------------------------
// --SECTION--                                     FieldIterator implementation
// ----------------------------------------------------------------------------
template<typename IndexMetaStruct>
FieldIterator<IndexMetaStruct>::FieldIterator(std::string_view collection,
                                              IndexId indexId)
    : _collection{collection}, _indexId{indexId} {
  // initialize iterator's value
}

template<typename IndexMetaStruct>
void FieldIterator<IndexMetaStruct>::reset(VPackSlice doc,
                                           IndexMetaStruct const& linkMeta) {
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
  auto const filter = getFilter(doc, linkMeta, false);
  if constexpr (std::is_same_v<IndexMetaStruct,
                               IResearchInvertedIndexMetaIndexingContext>) {
    _missingFieldsMap = linkMeta._missingFieldsMap;
  }
#ifdef USE_ENTERPRISE
  // this is set for root level as general mark.
  _hasNested = linkMeta._hasNested;
#endif
  pushLevel(doc, linkMeta, filter);
  next();
}

template<typename IndexMetaStruct>
void FieldIterator<IndexMetaStruct>::setBoolValue(VPackSlice const value) {
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

template<typename IndexMetaStruct>
void FieldIterator<IndexMetaStruct>::setNumericValue(VPackSlice const value) {
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

template<typename IndexMetaStruct>
void FieldIterator<IndexMetaStruct>::setNullValue(VPackSlice const value) {
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

template<typename IndexMetaStruct>
bool FieldIterator<IndexMetaStruct>::setValue(
    VPackSlice const value, FieldMeta::Analyzer const& valueAnalyzer,
    IndexMetaStruct const& context) {
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

  std::string_view valueRef;
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
      valueRef = value.stringView();
      valueType = AnalyzerValueType::String;
    } break;
    case VPackValueType::Custom: {
      if (ADB_UNLIKELY(_collection.empty())) {
        LOG_TOPIC("fb53c", WARN, arangodb::iresearch::TOPIC)
            << "Value for `_id` attribute could not be indexed for document "
            << transaction::helpers::extractKeyFromDocument(_slice).toString()
            << ". To recover please recreate corresponding index '" << _indexId
            << "'";
        return false;
      }
      _valueBuffer = getDocumentId(_collection, _slice);
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
      if constexpr (std::is_same_v<IndexMetaStruct,
                                   IResearchInvertedIndexMetaIndexingContext>) {
        iresearch::kludge::mangleField(_nameBuffer, false, valueAnalyzer);
      } else {
        iresearch::kludge::mangleField(_nameBuffer, true, valueAnalyzer);
      }
      _value._analyzer = std::move(analyzer);
      if constexpr (std::is_same_v<IndexMetaStruct,
                                   IResearchInvertedIndexMetaIndexingContext>) {
        _value._fieldFeatures = context.fieldFeatures();
        _value._indexFeatures = context.indexFeatures();
      } else {
        _value._fieldFeatures = pool->fieldFeatures();
        _value._indexFeatures = pool->features().indexFeatures();
      }
      _value._name = _nameBuffer;
    } break;
  }
  if (auto* storeFunc = pool->storeFunc(); storeFunc) {
    TRI_ASSERT(_currentTypedAnalyzer == nullptr);
    auto const bytes = storeFunc(_value._analyzer.get(), value);
    if (!irs::IsNull(bytes)) {
      _value._value = bytes;
      _value._storeValues = std::max(ValueStorage::VALUE, _value._storeValues);
    }
  }
  return true;
}

#ifndef USE_ENTERPRISE
template<typename IndexMetaStruct>
bool FieldIterator<IndexMetaStruct>::pushLevel(
    VPackSlice value, IndexMetaStruct const& meta,
    FieldIterator<IndexMetaStruct>::Filter filter) {
  std::optional<MissingFieldsContainer> missing;
  // missing fields are gathered for "root" e.g. empty stack
  // and for array->object in stack
  if (_stack.empty() ||
      (value.isObject() && _stack.size() > 1 &&
       _stack[_stack.size() - 2].it.value().value.isArray())) {
    auto key = _stack.empty() ? std::string_view("")
                              : std::string_view(_nameBuffer.data(),
                                                 _stack.back().nameLength);
    auto missingMapEntry = _missingFieldsMap.find(key);
    if (missingMapEntry != _missingFieldsMap.end()) {
      missing = std::make_optional(missingMapEntry->second);
    }
  }
  _stack.emplace_back(value, _nameBuffer.size(), meta, filter,
                      LevelType::kNormal, std::move(missing));
  return true;
}

template<typename IndexMetaStruct>
void FieldIterator<IndexMetaStruct>::popLevel() {
  _stack.pop_back();
}
#endif

template<typename IndexMetaStruct>
void FieldIterator<IndexMetaStruct>::fieldSeen(std::string& name) {
  if constexpr (!std::is_same_v<IResearchInvertedIndexMetaIndexingContext,
                                IndexMetaStruct>) {
    return;
  }
  auto it = _stack.rbegin();
  while (it != _stack.rend()) {
    if (it->missingFields) {
      it->missingFields->erase(name);
    }
    ++it;
  }
}

template<typename IndexMetaStruct>
void FieldIterator<IndexMetaStruct>::next() {
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
  _value._value = irs::bytes_view{};
#ifdef USE_ENTERPRISE
  _value._root = false;
  _needDoc = false;
#endif
  while (true) {
  setAnalyzers:
    while (_begin != _end) {
      // remove previous suffix
      TRI_ASSERT(context);
      _nameBuffer.resize(_prefixLength);
      if (setValue(_valueSlice, *_begin++, *context)) {
        return;
      }
    }
    while (true) {
      // pop all exhausted iterators
      while (!top().it.next()) {
        // need to emit "missing" fields as NULLs if index requires so
        if (top().missingFields && !top().missingFields->empty()) {
#ifdef USE_ENTERPRISE
          switch (processNestedNulls()) {
            case NestedNullsResult::kContinue:
              continue;
            case NestedNullsResult::kReturn:
              return;
            case NestedNullsResult::kNone:
              // NO-OP
              break;
          }
#endif
          _nameBuffer = *top().missingFields->begin();
          fieldSeen(_nameBuffer);
          setNullValue(VPackSlice::nullSlice());
          return;
        }
        popLevel();
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

      auto const filterRes = level.filter(_nameBuffer, context, value);
      // Filter will add a new part. But even if filter decided
      // to skip field - we must track it as seen and not emit null
      // for explicitly discarded values. Like skipping non-array fields
      // for expansion fields in the index as the field is definately not
      // missing.
      fieldSeen(_nameBuffer);
      if (!filterRes) {
        continue;
      }
#ifdef USE_ENTERPRISE
      if (level.type == LevelType::kNestedObjects) {
        // Requesting nested document
        _needDoc = true;
      }
#endif
      _value._storeValues = context->_storeValues;
      _value._value = irs::bytes_view{};
      _begin = nullptr;
      _end = nullptr;
      switch (auto const valueSlice = value.value; valueSlice.type()) {
        case VPackValueType::Null:
          setNullValue(valueSlice);
          return;
        case VPackValueType::Bool:
          setBoolValue(valueSlice);
          return;

        case VPackValueType::Array:
#ifdef USE_ENTERPRISE
          if (level.type == LevelType::kNestedRoot) {
            setRoot();
            return;
          }
#endif
          [[fallthrough]];
        case VPackValueType::Object: {
          auto filter = getFilter(valueSlice, *context,
                                  level.type == LevelType::kNestedObjects);
          bool setAnalyzers = pushLevel(valueSlice, *context, filter);
          if (setAnalyzers) {
            // FIXME(Dronplane): use traits
            if constexpr (std::is_same_v<
                              IndexMetaStruct,
                              IResearchInvertedIndexMetaIndexingContext>) {
              auto const& analyzers = *context->_analyzers;
              _begin = analyzers.data() + context->_primitiveOffset;
              _end = analyzers.data() + analyzers.size();
            } else {
              auto const& analyzers = context->_analyzers;
              _begin = analyzers.data() + context->_primitiveOffset;
              _end = analyzers.data() + analyzers.size();
            }
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
          // FIXME(Dronplane): use traits
          if constexpr (std::is_same_v<
                            IndexMetaStruct,
                            IResearchInvertedIndexMetaIndexingContext>) {
            _begin = context->_analyzers->data();
          } else {
            _begin = context->_analyzers.data();
          }
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

/* static */ std::string_view const& DocumentPrimaryKey::PK() noexcept {
  return PK_COLUMN;
}

LocalDocumentId::BaseType DocumentPrimaryKey::encode(
    LocalDocumentId value) noexcept {
  return PrimaryKeyEndianness<Endianness>::hostToPk(value.id());
}

// PLEASE NOTE that 'in.c_str()' MUST HAVE alignment >= alignof(uint64_t)
// NOTE implementation must match implementation of operator irs::bytes_view()
bool DocumentPrimaryKey::read(arangodb::LocalDocumentId& value,
                              irs::bytes_view const& in) noexcept {
  if (sizeof(arangodb::LocalDocumentId::BaseType) != in.size()) {
    return false;
  }

  // PLEASE NOTE that 'in.c_str()' MUST HAVE alignment >= alignof(uint64_t)
  value = arangodb::LocalDocumentId(PrimaryKeyEndianness<Endianness>::pkToHost(
      *reinterpret_cast<arangodb::LocalDocumentId::BaseType const*>(
          in.data())));

  return true;
}

Value::Value(std::string_view c, IndexId i, velocypack::Slice d)
    : collection{c}, indexId{i}, document{d} {}

bool Value::writeSlice(irs::data_output& out, VPackSlice slice) const {
  // _id field, anyway will be slow so unlikely
  if (ADB_UNLIKELY(slice.isCustom())) {
    buffer.reset();
    VPackBuilder builder(buffer);
    if (ADB_UNLIKELY(collection.empty())) {
      LOG_TOPIC("bf98c", WARN, TOPIC)
          << "Value for `_id` attribute could not be stored for document "
          << transaction::helpers::extractKeyFromDocument(document).toString()
          << ". To recover please recreate corresponding index '" << indexId
          << "'";
      return false;
    }
    builder.add(VPackValue(getDocumentId(collection, document)));
    slice = builder.slice();
    // a builder is destroyed but a buffer is alive
  }
  out.write_bytes(slice.start(), slice.byteSize());
  return true;
}

bool StoredValue::write(irs::data_output& out) const {
  auto size = fields->size();
  for (auto const& storedValue : *fields) {
    auto slice = get(document, storedValue.second, VPackSlice::nullSlice());
    // null value optimization
    if (ADB_UNLIKELY(1 == size && slice.isNull())) {
      return true;
    }
    if (!writeSlice(out, slice)) {
      return false;
    }
  }
  return true;
}

}  // namespace iresearch
}  // namespace arangodb

#if USE_ENTERPRISE
#include "Enterprise/IResearch/IResearchDocumentEE.hpp"
#endif

template class arangodb::iresearch::FieldIterator<
    arangodb::iresearch::FieldMeta>;

template class arangodb::iresearch::FieldIterator<
    arangodb::iresearch::IResearchInvertedIndexMetaIndexingContext>;
