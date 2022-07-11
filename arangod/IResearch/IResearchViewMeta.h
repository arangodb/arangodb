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
#include <unordered_set>

#include "IResearchDataStoreMeta.h"
#include "IResearchViewSort.h"
#include "IResearchViewStoredValues.h"

#include <velocypack/Builder.h>

#include "VocBase/Identifiers/DataSourceId.h"
#include "VocBase/voc-types.h"

namespace arangodb {
namespace velocypack {

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
/// @brief metadata describing the IResearch view
////////////////////////////////////////////////////////////////////////////////
struct IResearchViewMeta : public IResearchDataStoreMeta {
  struct Mask : public IResearchDataStoreMeta::Mask {
    bool _primarySort;
    bool _storedValues;
    bool _primarySortCompression;
    explicit Mask(bool mask = false) noexcept;
  };

  IResearchViewSort _primarySort;
  IResearchViewStoredValues _storedValues;
  irs::type_info::type_id _primarySortCompression{};
  // NOTE: if adding fields don't forget to modify the default constructor !!!
  // NOTE: if adding fields don't forget to modify the copy constructor !!!
  // NOTE: if adding fields don't forget to modify the move constructor !!!
  // NOTE: if adding fields don't forget to modify the comparison operator !!!
  // NOTE: if adding fields don't forget to modify IResearchLinkMeta::Mask !!!
  // NOTE: if adding fields don't forget to modify IResearchLinkMeta::Mask
  // constructor !!! NOTE: if adding fields don't forget to modify the init(...)
  // function !!! NOTE: if adding fields don't forget to modify the json(...)
  // function !!! NOTE: if adding fields don't forget to modify the memory()
  // function !!!

  IResearchViewMeta(IResearchViewMeta&& other) noexcept = delete;
  IResearchViewMeta& operator=(IResearchViewMeta&& other) noexcept = delete;
  IResearchViewMeta& operator=(IResearchViewMeta const& other) = delete;

  IResearchViewMeta();
  IResearchViewMeta(IResearchViewMeta const& other);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief ctor with '*Tag' not thread-safe for '&& other'
  //////////////////////////////////////////////////////////////////////////////
  struct FullTag {};
  IResearchViewMeta(FullTag, IResearchViewMeta&& other) noexcept;
  struct PartialTag {};
  IResearchViewMeta(PartialTag, IResearchViewMeta&& other) noexcept;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief store methods not thread-safe for 'this', and not for '&& other'
  //////////////////////////////////////////////////////////////////////////////
  void storeFull(IResearchViewMeta const& other);
  void storeFull(IResearchViewMeta&& other) noexcept;

  bool operator==(IResearchViewMeta const& other) const noexcept;
  bool operator!=(IResearchViewMeta const& other) const noexcept;

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief return default IResearchViewMeta values
  ////////////////////////////////////////////////////////////////////////////////
  static const IResearchViewMeta& DEFAULT();

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief initialize IResearchViewMeta with values from a JSON description
  ///        return success or set 'errorField' to specific field with error
  ///        on failure state is undefined
  /// @param mask if set reflects which fields were initialized from JSON
  ////////////////////////////////////////////////////////////////////////////////
  bool init(velocypack::Slice slice, std::string& errorField,
            IResearchViewMeta const& defaults = DEFAULT(),
            Mask* mask = nullptr) noexcept;

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief fill and return a JSON description of a IResearchViewMeta object
  ///        do not fill values identical to ones available in 'ignoreEqual'
  ///        or (if 'mask' != nullptr) values in 'mask' that are set to false
  ///        elements are appended to an existing object
  ///        return success or set TRI_set_errno(...) and return false
  ////////////////////////////////////////////////////////////////////////////////
  bool json(velocypack::Builder& builder,
            IResearchViewMeta const* ignoreEqual = nullptr,
            Mask const* mask = nullptr) const;

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief amount of memory in bytes occupied by this iResearch Link meta
  ////////////////////////////////////////////////////////////////////////////////
  size_t memory() const;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief internal configuration state of an IResearch view instance
///        not directly modifiable by user
////////////////////////////////////////////////////////////////////////////////
struct IResearchViewMetaState {
  struct Mask {
    bool _collections;
    explicit Mask(bool mask = false) noexcept;
  };

  // collection links added to this view via IResearchLink
  // creation (may contain no-longer valid cids)
  std::unordered_set<DataSourceId> _collections;
  // NOTE: if adding fields don't forget to modify the default constructor !!!
  // NOTE: if adding fields don't forget to modify the copy constructor !!!
  // NOTE: if adding fields don't forget to modify the move constructor !!!
  // NOTE: if adding fields don't forget to modify the comparison operator !!!
  // NOTE: if adding fields don't forget to modify IResearchLinkMetaState::Mask
  // !!! NOTE: if adding fields don't forget to modify
  // IResearchLinkMetaState::Mask constructor !!! NOTE: if adding fields don't
  // forget to modify the init(...) function !!! NOTE: if adding fields don't
  // forget to modify the json(...) function !!! NOTE: if adding fields don't
  // forget to modify the memory() function !!!

  IResearchViewMetaState() = default;
  IResearchViewMetaState(IResearchViewMetaState const& other);
  IResearchViewMetaState(IResearchViewMetaState&& other) noexcept;

  IResearchViewMetaState& operator=(IResearchViewMetaState&& other) noexcept;
  IResearchViewMetaState& operator=(IResearchViewMetaState const& other);

  bool operator==(IResearchViewMetaState const& other) const noexcept;
  bool operator!=(IResearchViewMetaState const& other) const noexcept;

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief initialize IResearchViewMeta with values from a JSON description
  ///        return success or set 'errorField' to specific field with error
  ///        on failure state is undefined
  /// @param mask if set reflects which fields were initialized from JSON
  ////////////////////////////////////////////////////////////////////////////////
  bool init(VPackSlice slice, std::string& errorField, Mask* mask = nullptr);

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief fill and return a JSON description of a IResearchViewMeta object
  ///        do not fill values identical to ones available in 'ignoreEqual'
  ///        or (if 'mask' != nullptr) values in 'mask' that are set to false
  ///        elements are appended to an existing object
  ///        return success or set TRI_set_errno(...) and return false
  ////////////////////////////////////////////////////////////////////////////////
  bool json(VPackBuilder& builder,
            IResearchViewMetaState const* ignoreEqual = nullptr,
            Mask const* mask = nullptr) const;

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief amount of memory in bytes occupied by this iResearch Link meta
  ////////////////////////////////////////////////////////////////////////////////
  size_t memory() const;
};

}  // namespace iresearch
}  // namespace arangodb
