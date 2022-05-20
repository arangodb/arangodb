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

#include "IResearchLinkMeta.h"
#include "IResearchViewSort.h"
#include "IResearchViewStoredValues.h"

namespace arangodb::iresearch {

enum class Consistency {
  kEventual, kImmediate
};

struct IResearchInvertedIndexMeta {
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


  using AnalyzerDefinitions = std::set<AnalyzerPool::ptr, FieldMeta::AnalyzerComparer>;

  struct FieldRecord {
    FieldRecord(std::vector<basics::AttributeName> const& path,
                FieldMeta::Analyzer&& a, std::vector<FieldRecord>&& nested,
                std::optional<Features>&& features, std::string&& expression,
                bool isArray, bool includeAllFields, bool trackListPositions);

    std::string toString() const;

    std::string const& analyzerName() const noexcept {
      TRI_ASSERT(_analyzer._pool);
      return _analyzer._shortName;
    }

    bool namesMatch(FieldRecord const& other) const noexcept;

    bool isIdentical(std::vector<basics::AttributeName> const& path,
                     irs::string_ref analyzerName) const noexcept;

    FieldMeta::Analyzer const& analyzer() const noexcept {
      return _analyzer;
    }

    bool isArray() const noexcept {
      TRI_ASSERT(!_attribute.empty());
      return isArray || _attribute.back().shouldExpand;
    }

    auto const& attribute() const noexcept {
      return _attribute;
    }

    auto const& expansion() const noexcept {
      return _expansion;
    }

    auto const& nested() const noexcept {
      return _nested;
    }

    auto const& features() const noexcept {
      return _features;
    }

    std::vector<arangodb::basics::AttributeName> combinedName() const;

   private:
    /// @brief nested fields
    std::vector<FieldRecord> _nested;
    /// @brief attribute path
    std::vector<basics::AttributeName> _attribute;
    /// @brief array sub-path in case of expansion (maybe empty)
    std::vector<basics::AttributeName> _expansion;
    /// @brief AQL expression to be computed as field value
    std::string _expression;
    /// @brief analyzer to apply
    FieldMeta::Analyzer _analyzer;
    /// @brief override for field features
    std::optional<Features> _features;
    /// @brief mark that field value is expected to be an array
    bool _isArray{false};
    /// @brief parse fields recursively
    bool _includeAllFields;
    /// @brief array processing variant
    bool _trackListPositions;
  };

  using Fields = std::vector<FieldRecord>;

  bool operator==(IResearchInvertedIndexMeta const& other) const noexcept;

  static bool matchesFieldsDefinition(IResearchInvertedIndexMeta const& meta,
                                      VPackSlice other);

  AnalyzerDefinitions _analyzerDefinitions;

  Fields _fields;
  // sort condition associated with the link (primarySort)
  IResearchViewSort _sort;
  // stored values associated with the link
  IResearchViewStoredValues _storedValues;
  irs::type_info::type_id _sortCompression{getDefaultCompression()};
  // Not used in the inverted index but need this member for compliance with
  // the LinkMeta interface
  irs::string_ref _collectionName;
  // the version of the iresearch interface e.g. which how data is stored in
  // iresearch (default == MAX) IResearchInvertedIndexMeta
  uint32_t _version{static_cast<uint32_t>(LinkVersion::MAX)};
  Consistency _consistency{Consistency::kEventual};
  std::string _defaultAnalyzerName;
  std::optional<Features> _features;
};
}  // namespace arangodb::iresearch
