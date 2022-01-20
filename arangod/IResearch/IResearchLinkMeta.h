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

#include <locale>
#include <mutex>
#include <vector>

#include "analysis/analyzer.hpp"
#include "utils/compression.hpp"
#include "utils/object_pool.hpp"
#include "utils/range.hpp"

#include "Containers.h"
#include "IResearchAnalyzerFeature.h"
#include "IResearchViewSort.h"
#include "IResearchViewStoredValues.h"
#include "IResearchCompression.h"

namespace arangodb {
namespace velocypack {

class Builder;
struct ObjectBuilder;
class Slice;

}  // namespace velocypack
}  // namespace arangodb

namespace arangodb {
namespace iresearch {

// Possible ways to store values in the view.
enum class ValueStorage : uint32_t {
  NONE = 0,  // do not store values in the view
  ID,        // only store value existance
  VALUE      // store full value in the view
};

struct FieldMeta {
  // can't use FieldMeta as value type since it's incomplete type so far
  typedef UnorderedRefKeyMap<char, UniqueHeapInstance<FieldMeta>> Fields;

  struct Analyzer {
    Analyzer(AnalyzerPool::ptr const& pool)
        : Analyzer(pool, pool ? std::string{pool->name()} : std::string{}) {}
    Analyzer(AnalyzerPool::ptr const& pool, std::string&& shortName) noexcept
        : _pool(pool), _shortName(std::move(shortName)) {}

    operator bool() const noexcept { return false == !_pool; }

    AnalyzerPool const* operator->() const noexcept { return _pool.get(); }
    AnalyzerPool* operator->() noexcept { return _pool.get(); }

    AnalyzerPool::ptr _pool;
    std::string _shortName;  // vocbase-independent short analyzer name
  };

  struct AnalyzerComparer {
    using is_transparent = void;

    bool operator()(AnalyzerPool::ptr const& lhs,
                    AnalyzerPool::ptr const& rhs) const noexcept {
      return lhs->name() < rhs->name();
    }

    bool operator()(AnalyzerPool::ptr const& lhs,
                    irs::string_ref const& rhs) const noexcept {
      return lhs->name() < rhs;
    }

    bool operator()(irs::string_ref const& lhs,
                    AnalyzerPool::ptr const& rhs) const noexcept {
      return lhs < rhs->name();
    }
  };

  struct Mask {
    explicit Mask(bool mask = false) noexcept
        : _analyzers(mask),
          _fields(mask),
          _includeAllFields(mask),
          _trackListPositions(mask),
          _storeValues(mask) {}

    bool _analyzers;
    bool _fields;
    bool _includeAllFields;
    bool _trackListPositions;
    bool _storeValues;
  };

  FieldMeta() = default;
  FieldMeta(FieldMeta const&) = default;
  FieldMeta(FieldMeta&&) = default;

  FieldMeta& operator=(FieldMeta const&) = default;
  FieldMeta& operator=(FieldMeta&&) = default;

  bool operator==(FieldMeta const& rhs) const noexcept;
  bool operator!=(FieldMeta const& rhs) const noexcept {
    return !(*this == rhs);
  }

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief initialize FieldMeta with values from a JSON description
  ///        return success or set 'errorField' to specific field with error
  ///        on failure state is undefined
  /// @param server underlying application server
  /// @param slice input definition
  /// @param errorField field causing error (out-param)
  /// @param defaultVocbase fallback vocbase for analyzer name normalization
  ///                       nullptr == do not normalize
  /// @param defaults inherited defaults
  /// @param mask if set reflects which fields were initialized from JSON
  /// @param referencedAnalyzers analyzers referenced in this link
  ////////////////////////////////////////////////////////////////////////////////
  bool init(application_features::ApplicationServer& server,
            velocypack::Slice const& slice, std::string& errorField,
            irs::string_ref defaultVocbase, LinkVersion version,
            FieldMeta const& defaults,
            std::set<AnalyzerPool::ptr, AnalyzerComparer>& referencedAnalyzers,
            Mask* mask);

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief fill and return a JSON description of a FieldMeta object
  ///        do not fill values identical to ones available in 'ignoreEqual'
  ///        or (if 'mask' != nullptr) values in 'mask' that are set to false
  ///        elements are appended to an existing object
  ///        return success or set TRI_set_errno(...) and return false
  /// @param server underlying application server
  /// @param builder output buffer
  /// @param defaultVocbase fallback vocbase for analyzer name normalization
  ///                       nullptr == do not normalize
  /// @param ignoreEqual values to ignore if equal
  /// @param defaultVocbase fallback vocbase
  /// @param mask if set reflects which fields were initialized from JSON
  ////////////////////////////////////////////////////////////////////////////////
  bool json(application_features::ApplicationServer& server,
            velocypack::Builder& builder,
            FieldMeta const* ignoreEqual = nullptr,
            TRI_vocbase_t const* defaultVocbase = nullptr,
            Mask const* mask = nullptr) const;

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief amount of memory in bytes occupied by this FieldMeta
  ////////////////////////////////////////////////////////////////////////////////
  size_t memory() const noexcept;

  // Analyzers to apply to every field.
  std::vector<Analyzer> _analyzers;
  // Offset of the first non-primitive analyzer.
  size_t _primitiveOffset{0};
  // Explicit list of fields to be indexed with optional overrides.
  Fields _fields;
  // How values should be stored inside the view.
  ValueStorage _storeValues{ValueStorage::NONE};
  // Include all fields or only fields listed in '_fields'.
  bool _includeAllFields{false};
  // Append relative offset in list to attribute name (as opposed to without
  // offset).
  bool _trackListPositions{false};
};

////////////////////////////////////////////////////////////////////////////////
/// @brief metadata describing how to process a field in a collection
////////////////////////////////////////////////////////////////////////////////
struct IResearchLinkMeta : public FieldMeta {
  struct Mask : public FieldMeta::Mask {
    explicit Mask(bool mask = false) noexcept
        : FieldMeta::Mask(mask),
          _analyzerDefinitions(mask),
          _sort(mask),
          _storedValues(mask),
          _sortCompression(mask),
          _collectionName(mask),
          _version(mask) {}

    bool _analyzerDefinitions;
    bool _sort;
    bool _storedValues;
    bool _sortCompression;
    bool _collectionName;
    bool _version;
  };

  std::set<AnalyzerPool::ptr, FieldMeta::AnalyzerComparer> _analyzerDefinitions;
  IResearchViewSort _sort;
  IResearchViewStoredValues _storedValues;
  irs::type_info::type_id _sortCompression{getDefaultCompression()};
  // The version of the iresearch interface e.g. which how
  // data is stored in iresearch (default == 0).
  uint32_t _version;

  // Linked collection name. Stored here for cluster deployment only.
  // For sigle server collection could be renamed so can`t store it here or
  // syncronisation will be needed. For cluster rename is not possible so
  // there is no problem but solved recovery issue - we will be able to index
  // _id attribute without doing agency request for collection name
  std::string _collectionName;

  IResearchLinkMeta();
  IResearchLinkMeta(IResearchLinkMeta const& other) = default;
  IResearchLinkMeta(IResearchLinkMeta&& other) noexcept = default;

  IResearchLinkMeta& operator=(IResearchLinkMeta&& other) = default;
  IResearchLinkMeta& operator=(IResearchLinkMeta const& other) = default;

  bool operator==(IResearchLinkMeta const& rhs) const noexcept;
  bool operator!=(IResearchLinkMeta const& rhs) const noexcept {
    return !(*this == rhs);
  }

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief initialize IResearchLinkMeta with values from a JSON description
  ///        return success or set 'errorField' to specific field with error
  ///        on failure state is undefined
  /// @param slice definition
  /// @param erroField field causing error (out-param)
  /// @param defaultVocbase fallback vocbase for analyzer name normalization
  ///                       nullptr == do not normalize
  /// @param defaultVersion fallback version if not present in definition
  /// @param mask if set reflects which fields were initialized from JSON
  ////////////////////////////////////////////////////////////////////////////////
  bool init(application_features::ApplicationServer& server, VPackSlice slice,
            std::string& errorField,
            irs::string_ref defaultVocbase = irs::string_ref::NIL,
            LinkVersion defaultVersion = LinkVersion::MIN,
            Mask* mask = nullptr);

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief fill and return a JSON description of a IResearchLinkMeta object
  ///        do not fill values identical to ones available in 'ignoreEqual'
  ///        or (if 'mask' != nullptr) values in 'mask' that are set to false
  ///        elements are appended to an existing object
  ///        return success or set TRI_set_errno(...) and return false
  /// @param builder output buffer (out-param)
  /// @param writeAnalyzerDefinition output full analyzer definition instead of
  /// just name
  /// @param ignoreEqual values to ignore if equal
  /// @param defaultVocbase fallback vocbase for analyzer name normalization
  ///                       nullptr == do not normalize
  /// @param mask if set reflects which fields were initialized from JSON
  ////////////////////////////////////////////////////////////////////////////////
  bool json(application_features::ApplicationServer& server,
            velocypack::Builder& builder, bool writeAnalyzerDefinition,
            IResearchLinkMeta const* ignoreEqual = nullptr,
            TRI_vocbase_t const* defaultVocbase = nullptr,
            Mask const* mask = nullptr) const;

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief amount of memory in bytes occupied by this IResearchLinkMeta
  ////////////////////////////////////////////////////////////////////////////////
  size_t memory() const noexcept;
};

}  // namespace iresearch
}  // namespace arangodb
