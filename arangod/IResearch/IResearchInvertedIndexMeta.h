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
/// @author Andrei Lobov
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "IResearchDataStoreMeta.h"
#include "IResearchLinkMeta.h"
#include "IResearchViewStoredValues.h"

#include <unicode/locid.h>

namespace arangodb::iresearch {

enum class Consistency { kEventual, kImmediate };

class IResearchInvertedIndexSort {
 public:
  IResearchInvertedIndexSort() = default;
  IResearchInvertedIndexSort(const IResearchInvertedIndexSort&) = default;
  IResearchInvertedIndexSort(IResearchInvertedIndexSort&&) = default;
  IResearchInvertedIndexSort& operator=(const IResearchInvertedIndexSort&) =
      default;
  IResearchInvertedIndexSort& operator=(IResearchInvertedIndexSort&&) = default;

  bool operator==(IResearchInvertedIndexSort const& rhs) const noexcept {
    return _fields == rhs._fields && _directions == rhs._directions &&
           std::string_view(_locale.getName()) ==
               std::string_view(rhs._locale.getName());
  }

  bool operator!=(IResearchInvertedIndexSort const& rhs) const noexcept {
    return !(*this == rhs);
  }

  auto sortCompression() const noexcept { return _sortCompression; }

  void clear() noexcept {
    _fields.clear();
    _directions.clear();
    _locale.setToBogus();
    _sortCompression = getDefaultCompression();
  }

  size_t size() const noexcept {
    TRI_ASSERT(_fields.size() == _directions.size());
    return _fields.size();
  }

  bool empty() const noexcept {
    TRI_ASSERT(_fields.size() == _directions.size());
    return _fields.empty();
  }

  void emplace_back(std::vector<basics::AttributeName>&& field,
                    bool direction) {
    _fields.emplace_back(std::move(field));
    _directions.emplace_back(direction);
  }

  template<typename Visitor>
  bool visit(Visitor visitor) const {
    for (size_t i = 0, size = this->size(); i < size; ++i) {
      if (!visitor(_fields[i], _directions[i])) {
        return false;
      }
    }

    return true;
  }

  std::vector<std::vector<basics::AttributeName>> const& fields()
      const noexcept {
    return _fields;
  }

  std::vector<basics::AttributeName> const& field(size_t i) const noexcept {
    TRI_ASSERT(i < this->size());

    return _fields[i];
  }

  bool direction(size_t i) const noexcept {
    TRI_ASSERT(i < this->size());

    return _directions[i];
  }

  std::string_view Locale() const noexcept { return _locale.getName(); }

  size_t memory() const noexcept;

  bool toVelocyPack(velocypack::Builder& builder) const;
  bool fromVelocyPack(velocypack::Slice, std::string& error);

 private:
  std::vector<std::vector<basics::AttributeName>> _fields;
  std::vector<bool> _directions;
  irs::type_info::type_id _sortCompression{getDefaultCompression()};
  icu::Locale _locale;
};

struct InvertedIndexField {
  // FIXME: turn into delegate ctor once attributes are not needed
  InvertedIndexField()
      : _includeAllFields(false),
        _trackListPositions(false),
        _isArray(false),
        _overrideValue(false) {}

  InvertedIndexField(std::vector<basics::AttributeName> const& path,
                     FieldMeta::Analyzer&& a,
                     std::vector<InvertedIndexField>&& nested,
                     std::optional<Features>&& features,
                     std::string&& expression, bool isArray,
                     bool includeAllFields, bool trackListPositions,
                     bool overrideValue, bool isPrimitiveAnalyzer,
                     std::string_view parentName);

  std::string_view path() const noexcept;
  std::string attributeString() const;

  std::string toString() const;

  std::string const& analyzerName() const noexcept {
    TRI_ASSERT(_analyzers[0]._pool);
    return _analyzers[0]._shortName;
  }

  bool namesMatch(InvertedIndexField const& other) const noexcept;

  bool isIdentical(std::vector<basics::AttributeName> const& path,
                   irs::string_ref analyzerName) const noexcept;

  FieldMeta::Analyzer const& analyzer() const noexcept { return _analyzers[0]; }

  bool isArray() const noexcept {
    TRI_ASSERT(!_attribute.empty());
    return _isArray || _attribute.back().shouldExpand;
  }

  auto const& attribute() const noexcept { return _attribute; }

  auto const& expansion() const noexcept { return _expansion; }

  auto const& expression() const noexcept { return _expression; }

  auto const& features() const noexcept { return _features; }

  auto trackListPositions() const noexcept { return _trackListPositions; }

  auto includeAllFields() const noexcept { return _includeAllFields; }

  auto overrideValue() const noexcept { return _overrideValue; }

  std::vector<arangodb::basics::AttributeName> combinedName() const;

  /// @brief nested fields
  std::vector<InvertedIndexField> _fields;
  /// @brief analyzer to apply. Array to comply with old views definition
  absl::InlinedVector<FieldMeta::Analyzer, 1> _analyzers;
  /// @brief override for field features
  std::optional<Features> _features;
  /// @brief start point for non primitive analyzers
  size_t _primitiveOffset{0};
  /// @brief fields ids storage
  // Inverted index always needs field ids in order to
  // execute cross types range queries
  ValueStorage const _storeValues{ValueStorage::ID};
  /// @brief parse fields recursively
  bool _includeAllFields;
  /// @brief array processing variant
  bool _trackListPositions;

 private:
  /// @brief mark that field value is expected to be an array
  bool _isArray;
  /// @brief force computed value to override existing value
  bool _overrideValue;
  /// @brief attribute path
  std::vector<basics::AttributeName> _attribute;
  /// @brief array sub-path in case of expansion (maybe empty)
  std::vector<basics::AttributeName> _expansion;
  /// @brief AQL expression to be computed as field value
  std::string _expression;
  /// @brief Full mangled path to the value
  std::string _path;
};

struct IResearchInvertedIndexMeta : public IResearchDataStoreMeta {
  IResearchInvertedIndexMeta() = default;
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
            irs::string_ref const defaultVocbase);

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
  /// @param defaultVocbase fallback vocbase
  ////////////////////////////////////////////////////////////////////////////////
  bool json(arangodb::ArangodServer& server, VPackBuilder& builder,
            bool writeAnalyzerDefinition,
            TRI_vocbase_t const* defaultVocbase = nullptr) const;

  using AnalyzerDefinitions =
      std::set<AnalyzerPool::ptr, FieldMeta::AnalyzerComparer>;

  bool operator==(IResearchInvertedIndexMeta const& other) const noexcept;

  static bool matchesFieldsDefinition(IResearchInvertedIndexMeta const& meta,
                                      VPackSlice other);

  std::vector<InvertedIndexField> const& fields() const noexcept {
    return _fields._fields;
  }

  bool hasNested() const noexcept { return _hasNested; }

  /// @brief custom conversion to match FieldIterator expectations
  operator InvertedIndexField const&() const noexcept { return _fields; }

  AnalyzerDefinitions _analyzerDefinitions;

  InvertedIndexField _fields;
  // sort condition associated with the link (primarySort)
  IResearchInvertedIndexSort _sort;
  // stored values associated with the link
  IResearchViewStoredValues _storedValues;
  // Not used in the inverted index but need this member for compliance with
  // the LinkMeta interface
  irs::string_ref _collectionName;
  // the version of the iresearch interface e.g. which how data is stored in
  // iresearch (default == MAX) IResearchInvertedIndexMeta
  uint32_t _version{static_cast<uint32_t>(LinkVersion::MAX)};
  Consistency _consistency{Consistency::kEventual};
  std::string _defaultAnalyzerName;
  std::optional<Features> _features;
  bool _hasNested{false};
};
}  // namespace arangodb::iresearch
