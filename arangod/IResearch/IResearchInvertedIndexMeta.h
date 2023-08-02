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
/// @author Andrei Lobov
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "IResearch/IResearchDataStoreMeta.h"
#ifdef USE_ENTERPRISE
#include "Enterprise/IResearch/IResearchOptimizeTopK.h"
#endif
#include "IResearch/IResearchLinkMeta.h"
#include "IResearch/IResearchViewStoredValues.h"
#include "IResearch/IResearchViewSort.h"
#include "Containers/FlatHashMap.h"
#include "Containers/FlatHashSet.h"

#include <unicode/locid.h>

namespace arangodb::iresearch {
enum class Consistency { kEventual, kImmediate };

using MissingFieldsContainer = containers::FlatHashSet<std::string_view>;
// "attribute" names are tmp strings so need to store them here.
// but "path" is string_view to meta internals so just string_views.
using MissingFieldsMap =
    containers::FlatHashMap<std::string, MissingFieldsContainer>;

class IResearchInvertedIndexSort final : public IResearchSortBase {
 public:
  IResearchInvertedIndexSort() { _locale.setToBogus(); }

  bool operator==(IResearchInvertedIndexSort const& rhs) const noexcept {
    return IResearchSortBase::operator==(rhs) &&
           std::string_view{_locale.getName()} == rhs._locale.getName();
  }

  void clear() noexcept {
    IResearchSortBase::clear();
    _locale.setToBogus();
    _sortCompression = getDefaultCompression();
  }

  auto sortCompression() const noexcept { return _sortCompression; }

#ifdef USE_ENTERPRISE
  bool cache() const noexcept { return _cache; }
#endif

  std::string_view Locale() const noexcept { return _locale.getName(); }

  size_t memory() const noexcept {
    return sizeof(*this) + IResearchSortBase::memory();
  }

  bool toVelocyPack(velocypack::Builder& builder) const;
  bool fromVelocyPack(velocypack::Slice, std::string& error);

 private:
  irs::type_info::type_id _sortCompression{getDefaultCompression()};
  icu::Locale _locale;
#ifdef USE_ENTERPRISE
  bool _cache{false};
#endif
};

struct InvertedIndexField {
  // ordered so analyzerDefinitions slice is consitent
  // important for checking for changes/syncing etc
  using AnalyzerDefinitions =
      std::set<AnalyzerPool::ptr, FieldMeta::AnalyzerComparer>;

  bool init(VPackSlice slice, AnalyzerDefinitions& analyzerDefinitions,
            LinkVersion version, bool extendedNames,
            IResearchAnalyzerFeature& analyzers,
            InvertedIndexField const& parent,
            std::string_view const defaultVocbase, bool rootMode,
            std::string& errorField);

  bool json(arangodb::ArangodServer& server, VPackBuilder& builder,
            InvertedIndexField const& parent, bool rootMode,
            TRI_vocbase_t const* defaultVocbase = nullptr) const;

  std::string_view path() const noexcept;
  std::string_view attributeString() const;

  std::string toString() const;

  std::string const& analyzerName() const noexcept {
    TRI_ASSERT(_analyzers[0]._pool);
    return _analyzers[0]._pool->name();
  }

  bool operator==(InvertedIndexField const& other) const noexcept;

  FieldMeta::Analyzer const& analyzer() const noexcept { return _analyzers[0]; }

  bool isArray() const noexcept { return _isArray || _hasExpansion; }

  /// @brief nested fields
  std::vector<InvertedIndexField> _fields;
  /// @brief analyzer to apply. Array to comply with old views definition
  std::array<FieldMeta::Analyzer, 1> _analyzers{FieldMeta::Analyzer(nullptr)};
  /// @brief override for field features
  Features _features;
  /// @brief attribute path
  std::vector<basics::AttributeName> _attribute;
  /// @brief AQL expression to be computed as field value
  std::string _expression;
  /// @brief Full mangled path to the value
  std::string _path;
  /// @brief path to attribute before expansion - calculated value
  std::string _attributeName;
  /// @brief start point for non primitive analyzers
  size_t _primitiveOffset{0};
  /// @brief fields ids storage
  // Inverted index always needs field ids in order to
  // execute cross types range queries
  ValueStorage const _storeValues{ValueStorage::ID};
  /// @brief parse all fields recursively
  bool _includeAllFields{false};
  /// @brief array processing variant
  bool _trackListPositions{false};
  /// @brief mark that field value is expected to be an array
  bool _isArray{false};
  /// @brief force computed value to override existing value
  bool _overrideValue{false};
  /// @brief if the field is with expansion - calculated value
  bool _hasExpansion{false};
#ifdef USE_ENTERPRISE
  // is cached column
  bool _cache{false};
#endif
  /// @brief Field is array/value mix as for arangosearch views.
  ///        Field is excluded from inverted index optimizations for filter!
  bool _isSearchField{false};
};

struct IResearchInvertedIndexMeta;

struct IResearchInvertedIndexMetaIndexingContext {
  IResearchInvertedIndexMetaIndexingContext(
      IResearchInvertedIndexMeta const* field, bool add = true);

  void addField(InvertedIndexField const& field, bool nested);

  void setFeatures(Features const& features);

  std::string_view collectionName() const noexcept;

  irs::features_t fieldFeatures() const noexcept {
    return {_fieldFeatures.data(), _fieldFeatures.size()};
  }
  irs::IndexFeatures indexFeatures() const noexcept {
    return features().indexFeatures();
  }

  Features const& features() const noexcept { return _features; }

  bool hasNested() const noexcept { return _hasNested; }

  absl::flat_hash_map<std::string_view,
                      IResearchInvertedIndexMetaIndexingContext>
      _fields;
  absl::flat_hash_map<std::string_view,
                      IResearchInvertedIndexMetaIndexingContext>
      _nested;
  std::array<FieldMeta::Analyzer, 1> const* _analyzers;
  size_t _primitiveOffset;
  IResearchInvertedIndexMeta const* _meta;
  ValueStorage const _storeValues{ValueStorage::ID};
  IResearchInvertedIndexSort const& _sort;
  IResearchViewStoredValues const& _storedValues;
  MissingFieldsMap _missingFieldsMap;
  bool _isArray{false};
  bool _hasNested;
  bool _includeAllFields;
  bool _trackListPositions;
#ifdef USE_ENTERPRISE
  bool _cache;
#endif
  bool _isSearchField;

 private:
  Features _features;
  std::vector<irs::type_info::type_id> _fieldFeatures;
};

struct IResearchInvertedIndexMeta : public IResearchDataStoreMeta,
                                    public InvertedIndexField {
  IResearchInvertedIndexMeta();
  ////////////////////////////////////////////////////////////////////////////////
  /// @brief initialize IResearchInvertedIndexMeta with values from a JSON
  /// description
  ///        return success or set 'errorField' to specific field with error
  ///        on failure state is undefined
  /// @param server underlying application server
  /// @param slice input definition
  /// @param readAnalyzerDefinition allow reading analyzer definitions instead
  ///                               of just name
  /// @param errorField field causing error (out-param)
  /// @param defaultVocbase fallback vocbase for analyzer name normalization
  ///                       nullptr == do not normalize
  ////////////////////////////////////////////////////////////////////////////////
  bool init(arangodb::ArangodServer& server, VPackSlice const& slice,
            bool readAnalyzerDefinition, std::string& errorField,
            std::string_view const defaultVocbase);

  bool dense() const noexcept { return !_sort.empty(); }

  static const IResearchInvertedIndexMeta& DEFAULT();

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief fill and return a JSON description
  /// @param server underlying application server
  /// @param builder output buffer
  /// @param writeAnalyzerDefinition output full analyzer definition instead of
  /// just name
  /// @param defaultVocbase fallback vocbase for analyzer name normalization
  ///                       nullptr == do not normalize
  ////////////////////////////////////////////////////////////////////////////////
  bool json(arangodb::ArangodServer& server, VPackBuilder& builder,
            bool writeAnalyzerDefinition,
            TRI_vocbase_t const* defaultVocbase = nullptr) const;

  bool operator==(IResearchInvertedIndexMeta const& other) const noexcept;

  static bool matchesDefinition(IResearchInvertedIndexMeta const& meta,
                                VPackSlice other, TRI_vocbase_t const& vocbase);

  bool hasNested() const noexcept { return _hasNested; }

#ifdef USE_ENTERPRISE
  bool sortCache() const noexcept { return _sort.cache(); }
#endif

  std::unique_ptr<IResearchInvertedIndexMetaIndexingContext> _indexingContext;

  InvertedIndexField::AnalyzerDefinitions _analyzerDefinitions;
  // sort condition associated with the link (primarySort)
  IResearchInvertedIndexSort _sort;
  // stored values associated with the link
  IResearchViewStoredValues _storedValues;
#ifdef USE_ENTERPRISE
  IResearchOptimizeTopK _optimizeTopK;
#endif
  mutable std::string _collectionName;
  Consistency _consistency{Consistency::kEventual};
  bool _hasNested{false};
#ifdef USE_ENTERPRISE
  bool _pkCache{false};
#endif
};
}  // namespace arangodb::iresearch
