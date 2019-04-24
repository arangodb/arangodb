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

#ifndef ARANGODB_IRESEARCH__IRESEARCH_LINK_META_H
#define ARANGODB_IRESEARCH__IRESEARCH_LINK_META_H 1

#include <locale>
#include <mutex>
#include <vector>

#include "analysis/analyzer.hpp"
#include "utils/object_pool.hpp"

#include "Containers.h"
#include "IResearchAnalyzerFeature.h"

namespace arangodb {
namespace velocypack {

class Builder;         // forward declarations
struct ObjectBuilder;  // forward declarations
class Slice;           // forward declarations

}  // namespace velocypack
}  // namespace arangodb

namespace arangodb {
namespace iresearch {

// -----------------------------------------------------------------------------
// --SECTION--                                                      public
// types
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief enum of possible ways to store values in the view
////////////////////////////////////////////////////////////////////////////////
enum class ValueStorage : uint32_t {
  NONE = 0,  // do not store values in the view
  ID,        // only store value existance
  FULL,      // store full value in the view
};

////////////////////////////////////////////////////////////////////////////////
/// @brief metadata describing how to process a field in a collection
////////////////////////////////////////////////////////////////////////////////
struct IResearchLinkMeta {
  struct Analyzer {
    IResearchAnalyzerFeature::AnalyzerPool::ptr _pool;
    std::string _shortName; // vocbase-dependent short analyzer name
    Analyzer(); // identity analyzer
    Analyzer( // constructor
      IResearchAnalyzerFeature::AnalyzerPool::ptr const& pool, // pool
      std::string&& shortName // short name (cached for use during insert(...))
    ) noexcept: _pool(pool), _shortName(std::move(shortName)) {}
    operator bool() const noexcept { return false == !_pool; }
  };
  struct Mask {
    bool _analyzers;
    bool _fields;
    bool _includeAllFields;
    bool _trackListPositions;
    bool _storeValues;
    explicit Mask(bool mask = false) noexcept;
  };

  typedef std::vector<Analyzer> Analyzers;

  // can't use IResearchLinkMeta as value type since it's incomplete type so far
  typedef UnorderedRefKeyMap<char, UniqueHeapInstance<IResearchLinkMeta>> Fields;

  Analyzers _analyzers;  // analyzers to apply to every field
  Fields _fields;  // explicit list of fields to be indexed with optional overrides
  bool _includeAllFields;    // include all fields or only fields listed in
                             // '_fields'
  bool _trackListPositions;  // append relative offset in list to attribute name
                             // (as opposed to without offset)
  ValueStorage _storeValues;  // how values should be stored inside the view
  // NOTE: if adding fields don't forget to modify the default constructor !!!
  // NOTE: if adding fields don't forget to modify the copy assignment operator !!!
  // NOTE: if adding fields don't forget to modify the move assignment operator !!!
  // NOTE: if adding fields don't forget to modify the comparison operator !!!
  // NOTE: if adding fields don't forget to modify IResearchLinkMeta::Mask !!!
  // NOTE: if adding fields don't forget to modify IResearchLinkMeta::Mask constructor !!!
  // NOTE: if adding fields don't forget to modify the init(...) function !!!
  // NOTE: if adding fields don't forget to modify the json(...) function !!!
  // NOTE: if adding fields don't forget to modify the memSize() function !!!

  IResearchLinkMeta();
  IResearchLinkMeta(IResearchLinkMeta const& other);
  IResearchLinkMeta(IResearchLinkMeta&& other) noexcept;

  IResearchLinkMeta& operator=(IResearchLinkMeta&& other) noexcept;
  IResearchLinkMeta& operator=(IResearchLinkMeta const& other);

  bool operator==(IResearchLinkMeta const& other) const noexcept;
  bool operator!=(IResearchLinkMeta const& other) const noexcept;

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief return default IResearchLinkMeta values
  ////////////////////////////////////////////////////////////////////////////////
  static const IResearchLinkMeta& DEFAULT();

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief initialize IResearchLinkMeta with values from a JSON description
  ///        return success or set 'errorField' to specific field with error
  ///        on failure state is undefined
  /// @param defaultVocbase fallback vocbase for analyzer name normalization
  ///                       nullptr == do not normalize
  /// @param mask if set reflects which fields were initialized from JSON
  ////////////////////////////////////////////////////////////////////////////////
  bool init( // initialize meta
    arangodb::velocypack::Slice const& slice, // definition
    bool readAnalyzerDefinition, // allow reading analyzer definitions instead of just name
    std::string& errorField, // field causing error (out-param)
    TRI_vocbase_t const* defaultVocbase = nullptr, // fallback vocbase
    IResearchLinkMeta const& defaults = DEFAULT(), // inherited defaults
    Mask* mask = nullptr // initialized fields (out-param)
  );

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief fill and return a JSON description of a IResearchLinkMeta object
  ///        do not fill values identical to ones available in 'ignoreEqual'
  ///        or (if 'mask' != nullptr) values in 'mask' that are set to false
  ///        elements are appended to an existing object
  ///        return success or set TRI_set_errno(...) and return false
  /// @param defaultVocbase fallback vocbase for analyzer name normalization
  ///                       nullptr == do not normalize
  ////////////////////////////////////////////////////////////////////////////////
  bool json( // append meta jSON
    arangodb::velocypack::Builder& builder, // output buffer (out-param)
    bool writeAnalyzerDefinition, // output full analyzer definition instead of just name
    IResearchLinkMeta const* ignoreEqual = nullptr, // values to ignore if equal
    TRI_vocbase_t const* defaultVocbase = nullptr, // fallback vocbase
    Mask const* mask = nullptr // values to ignore always
  ) const;

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief amount of memory in bytes occupied by this iResearch Link meta
  ////////////////////////////////////////////////////////////////////////////////
  size_t memory() const noexcept;
};  // IResearchLinkMeta

}  // namespace iresearch
}  // namespace arangodb

#endif