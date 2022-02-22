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

  struct FieldRecord {
    FieldRecord(std::vector<basics::AttributeName> const& path,
                FieldMeta::Analyzer&& a);

    std::string toString() const;

    bool isIdentical(std::vector<basics::AttributeName> const& path,
                     irs::string_ref analyzerName) const noexcept;

    /// @brief attribute path
    std::vector<basics::AttributeName> attribute;
    /// @brief array sub-path in case of expansion (maybe empty)
    std::vector<basics::AttributeName> expansion;
    /// @brief analyzer to apply
    FieldMeta::Analyzer analyzer;
  };

  using Fields = std::vector<FieldRecord>;

  bool operator==(IResearchInvertedIndexMeta const& other) const noexcept;

  static bool matchesFieldsDefinition(IResearchInvertedIndexMeta const& meta,
                                      VPackSlice other);

  std::set<AnalyzerPool::ptr, FieldMeta::AnalyzerComparer> _analyzerDefinitions;
  Fields _fields;
  // sort condition associated with the link (primarySort)
  IResearchViewSort _sort;
  // stored values associated with the link
  IResearchViewStoredValues _storedValues;
  irs::type_info::type_id _sortCompression{getDefaultCompression()};
  irs::string_ref _collectionName;
  // the version of the iresearch interface e.g. which how data is stored in
  // iresearch (default == MAX) IResearchInvertedIndexMeta
  uint32_t _version{static_cast<uint32_t>(LinkVersion::MAX)};
};
}  // namespace arangodb::iresearch
