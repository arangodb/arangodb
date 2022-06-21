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

#include "VocBase/voc-types.h"

#include "IResearchCommon.h"

#ifdef USE_ENTERPRISE
#include "Enterprise/IResearch/IResearchDocumentEE.h"
#endif

#include "IResearchAnalyzerFeature.h"
#include "IResearchLinkMeta.h"
#include "IResearchInvertedIndexMeta.h"
#include "IResearchVPackTermAttribute.h"
#include "VelocyPackHelper.h"

#include "VocBase/Identifiers/IndexId.h"
#include "VocBase/Identifiers/LocalDocumentId.h"
#include "analysis/token_attributes.hpp"
#include "search/filter.hpp"
#include "store/data_output.hpp"
#include "index/norm.hpp"

namespace iresearch {

class boolean_filter;
struct data_output;
class token_stream;
class numeric_token_stream;
class boolean_token_stream;

namespace analysis {
class analyzer;
}  // namespace analysis
}  // namespace iresearch

namespace arangodb {
namespace aql {

struct AstNode;       // forward declaration
class SortCondition;  // forward declaration

}  // namespace aql
}  // namespace arangodb

namespace arangodb {
namespace transaction {

class Methods;  // forward declaration

}  // namespace transaction
}  // namespace arangodb

namespace arangodb {
namespace iresearch {

////////////////////////////////////////////////////////////////////////////////
/// @brief indexed/stored document field adapter for IResearch
////////////////////////////////////////////////////////////////////////////////
struct Field {
  static void setPkValue(Field& field, LocalDocumentId::BaseType const& pk);

  irs::string_ref const& name() const noexcept {
    TRI_ASSERT(!_name.null());
    return _name;
  }

  irs::features_t features() const noexcept { return _fieldFeatures; }

  irs::IndexFeatures index_features() const noexcept { return _indexFeatures; }

  irs::token_stream& get_tokens() const {
    TRI_ASSERT(_analyzer);
    return *_analyzer;
  }

  bool write(irs::data_output& out) const {
    if (!_value.null()) {
      out.write_bytes(_value.c_str(), _value.size());
    }

    return true;
  }

  AnalyzerPool::CacheType::ptr _analyzer;
  irs::string_ref _name;
  irs::bytes_ref _value;
  ValueStorage _storeValues;
  irs::features_t _fieldFeatures;
  irs::IndexFeatures _indexFeatures;
#ifdef USE_ENTERPRISE
  bool _root{false};
#endif
};  // Field

using MissingFieldsContainer = absl::flat_hash_set<std::string_view>;
// "attribute" names are tmp strings so need to store them here.
// but "path" is string_view to meta internals so just string_views.
using MissingFieldsMap = absl::flat_hash_map<std::string, MissingFieldsContainer>;



////////////////////////////////////////////////////////////////////////////////
/// @brief allows to iterate over the provided VPack accoring the specified
///        IResearchLinkMeta
////////////////////////////////////////////////////////////////////////////////
template<typename IndexMetaStruct, typename LevelMeta>
class FieldIterator {
 public:
  explicit FieldIterator(arangodb::transaction::Methods& trx,
                         irs::string_ref collection, IndexId linkId);

  Field const& operator*() const noexcept { return _value; }

  FieldIterator& operator++() {
    next();
    return *this;
  }

  // We don't support postfix increment since it requires
  // deep copy of all buffers and analyzers which is quite
  // expensive and useless

  bool valid() const noexcept { return !_stack.empty(); }

  void reset(velocypack::Slice slice, IndexMetaStruct const& linkMeta);

  bool  disableFlush() const noexcept {
    return _disableFlush;
  }

#ifdef USE_ENTERPRISE
  bool onRootLevel() const noexcept;

  bool hasNested() const noexcept;

  bool needDoc() const noexcept {
    return _needDoc;
  }

  void setDisableFlush() noexcept {
    _disableFlush = true;
  }
#endif

 private:
  using AnalyzerIterator = FieldMeta::Analyzer const*;

  using Filter = bool (*)(std::string& buffer, LevelMeta const*& rootMeta,
                          IteratorValue const& value);

  using PrimitiveTypeResetter = void (*)(irs::token_stream* stream,
                                         VPackSlice slice);

  enum class LevelType {
    NORMAL = 0,
    ROOT = 1,
    NESTED_ROOT = 2,
    NESTED_FIELDS = 3,
    NESTED_OBJECTS = 4
  };

  struct Level {
    Level(velocypack::Slice slice, size_t nameLength, LevelMeta const& meta,
          Filter levelFilter, LevelType levelType,
          std::optional<arangodb::iresearch::MissingFieldsContainer>&& missingTracker)
        : it(slice),
          nameLength(nameLength),
          meta(&meta),
          filter(levelFilter),
          type(levelType),
          missingFields(missingTracker) {
    }

    Iterator it;
    size_t nameLength;      // length of the name at the current level
    LevelMeta const* meta;  // metadata
    Filter filter;
    LevelType type;
    // TODO(Dronplane): Try to avoid copy.
    // But it will need to decide how to conveyr "erase" on upper levels.
    std::optional<MissingFieldsContainer> missingFields;
  };  // Level

  Level& top() noexcept {
    TRI_ASSERT(!_stack.empty());
    return _stack.back();
  }

#ifdef USE_ENTERPRISE
  using MetaTraits = IndexMetaTraits<LevelMeta>;
  void setRoot();
#endif

  void popLevel();
  bool pushLevel(VPackSlice value, LevelMeta const& meta, Filter filter);
  void fieldSeen(std::string& name);

  // disallow copy and assign
  FieldIterator(FieldIterator const&) = delete;
  FieldIterator& operator=(FieldIterator const&) = delete;

  void next();
  bool setValue(VPackSlice const value,
                FieldMeta::Analyzer const& valueAnalyzer);
  void setNullValue(VPackSlice const value);
  void setNumericValue(VPackSlice const value);
  void setBoolValue(VPackSlice const value);

  VPackSlice _slice;  // input slice
  VPackSlice _valueSlice;
  AnalyzerIterator _begin{};
  AnalyzerIterator _end{};
  std::vector<Level> _stack;
  size_t _prefixLength{};
  // buffer for field name
  std::string _nameBuffer;
  // need temporary buffer for custom types in VelocyPack
  std::string _valueBuffer;
  // buffer for stored values
  VPackBuffer<uint8_t> _buffer;
  arangodb::transaction::Methods* _trx;
  irs::string_ref _collection;
  Field _value;  // iterator's value
  IndexId _linkId;

  // Support for outputting primitive type from analyzer
  AnalyzerPool::CacheType::ptr _currentTypedAnalyzer;
  VPackTermAttribute const* _currentTypedAnalyzerValue{nullptr};
  PrimitiveTypeResetter _primitiveTypeResetter{nullptr};

  bool _isDBServer;
  bool _disableFlush;
  MissingFieldsMap _missingFieldsMap;
 #ifdef USE_ENTERPRISE
  std::vector<std::string> _nestingBuffers;
  bool _needDoc{false};
  bool _hasNested{false};
#endif
};  // FieldIterator

////////////////////////////////////////////////////////////////////////////////
/// @brief allows to iterate over the provided VPack according to the specified
///        IResearchInvertedIndexMeta
////////////////////////////////////////////////////////////////////////////////
// FIXME: remove after "gather nulls is implemented"
//class InvertedIndexFieldIterator {
// public:
//  // must match interface of FieldIterator to make usable template insert
//  // implementation
//  Field const& operator*() const noexcept { return _value; }
//
//  InvertedIndexFieldIterator& operator++() {
//    next();
//    return *this;
//  }
//
//  // we don't need trx as we don't index the _id attribute.
//  // but we require it here just to match signature of "FieldIterator" in
//  // general
//  explicit InvertedIndexFieldIterator(arangodb::transaction::Methods&,
//                                      irs::string_ref collection,
//                                      IndexId indexId);
//
//#ifdef USE_ENTERPRISE
//  bool hasNested() const noexcept {
//    return valid() &&
//           std::any_of(_fieldsMeta->_fields._fields.begin(), _fieldsMeta->_fields._fields.end(),
//                       [](InvertedIndexField const& f) {
//                         return !f.nested().empty();
//                       });
//  }
//
//  bool onRootLevel() const noexcept;
//  
//  bool needDoc() const noexcept {
//    return false;
//  }
//#endif
//
//  bool valid() const noexcept { return _fieldsMeta && _begin != _end; }
//
//
//  void reset(VPackSlice slice,
//             IResearchInvertedIndexMeta const& fieldsMeta) {
//    _slice = slice;
//    _fieldsMeta = &fieldsMeta;
//    TRI_ASSERT(!_fieldsMeta->_fields._fields.empty());
//    _begin = _fieldsMeta->_fields._fields.data() - 1;
//    _end = _fieldsMeta->_fields._fields.data() + _fieldsMeta->_fields._fields.size();
//    next();
//  }
//
// private:
//  void next();
//
//#ifdef USE_ENTERPRISE
//  bool nextNested();
//#endif
//
//  bool setValue(VPackSlice const value,
//                FieldMeta::Analyzer const& valueAnalyzer);
//  void setNullValue();
//  void setNumericValue(VPackSlice const value);
//  void setBoolValue(VPackSlice const value);
//
//  // Support for outputting primitive type from analyzer
//  using PrimitiveTypeResetter = void (*)(irs::token_stream* stream,
//                                         VPackSlice slice);
//
//  size_t _prefixLength{};
//  InvertedIndexField const* _begin{nullptr};
//  InvertedIndexField const* _end{nullptr};
//  IResearchInvertedIndexMeta const* _fieldsMeta{nullptr};
//  Field _value;       // iterator's value
//  VPackSlice _slice;  // input slice
//  VPackSlice _valueSlice;
//  IndexId _indexId;
//  AnalyzerPool::CacheType::ptr _currentTypedAnalyzer;
//  VPackTermAttribute const* _currentTypedAnalyzerValue{nullptr};
//  PrimitiveTypeResetter _primitiveTypeResetter{nullptr};
//  std::vector<VPackArrayIterator> _arrayStack;
//  std::string _nameBuffer;
//  VPackBuffer<uint8_t> _buffer;  // buffer for stored values
//};

////////////////////////////////////////////////////////////////////////////////
/// @brief represents stored primary key of the ArangoDB document
////////////////////////////////////////////////////////////////////////////////
struct DocumentPrimaryKey {
  static irs::string_ref const& PK() noexcept;  // stored primary key column

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief encodes a specified PK value
  /// @returns encoded value
  ////////////////////////////////////////////////////////////////////////////////
  static LocalDocumentId::BaseType encode(LocalDocumentId value) noexcept;

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief reads and decodes PK from a specified buffer
  /// @returns 'true' on success, 'false' otherwise
  /// @note PLEASE NOTE that 'in.c_str()' MUST HAVE alignment >=
  /// alignof(uint64_t)
  ////////////////////////////////////////////////////////////////////////////////
  static bool read(LocalDocumentId& value, irs::bytes_ref const& in) noexcept;

  DocumentPrimaryKey() = delete;
};  // DocumentPrimaryKey

struct StoredValue {
  StoredValue(transaction::Methods const& t, irs::string_ref cn,
              VPackSlice const doc, IndexId lid);

  bool write(irs::data_output& out) const;

  irs::string_ref const& name() const noexcept { return fieldName; }

  mutable VPackBuffer<uint8_t> buffer;
  transaction::Methods const& trx;
  velocypack::Slice const document;
  irs::string_ref fieldName;
  irs::string_ref collection;
  std::vector<std::pair<std::string, std::vector<basics::AttributeName>>> const*
      fields;
  IndexId linkId;
  bool isDBServer;
};  // StoredValue

}  // namespace iresearch
}  // namespace arangodb
