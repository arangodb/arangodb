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

#ifndef ARANGODB_IRESEARCH__IRESEARCH_VIEW_META_H
#define ARANGODB_IRESEARCH__IRESEARCH_VIEW_META_H 1

#include <locale>
#include <unordered_set>

#include "index/index_writer.hpp"
#include "VocBase/voc-types.h"

NS_BEGIN(arangodb)
NS_BEGIN(velocypack)

class Builder; // forward declarations
struct ObjectBuilder; // forward declarations
class Slice; // forward declarations

NS_END // velocypack
NS_END // arangodb

NS_BEGIN(arangodb)
NS_BEGIN(iresearch)

// -----------------------------------------------------------------------------
// --SECTION--                                                      public types
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief metadata describing the IResearch view
////////////////////////////////////////////////////////////////////////////////
struct IResearchViewMeta {
  class ConsolidationPolicy {
   public:
    ConsolidationPolicy(
      std::string const& type,
      size_t segmentThreshold,
      float threshold
    );
    ConsolidationPolicy(ConsolidationPolicy const& other);
    ConsolidationPolicy(ConsolidationPolicy&& other) noexcept;
    ConsolidationPolicy& operator=(ConsolidationPolicy const& other);
    ConsolidationPolicy& operator=(ConsolidationPolicy&& other) noexcept;
    bool operator==(ConsolidationPolicy const& other) const noexcept;
    bool operator!=(ConsolidationPolicy const& other) const noexcept;

    ////////////////////////////////////////////////////////////////////////////
    /// @brief initialize ConsolidationPolicy with values from a JSON definition
    /// @return success or set 'errorField' to the specific field with the error
    ///         on failure state is undefined
    ////////////////////////////////////////////////////////////////////////////
    bool init(
      arangodb::velocypack::Slice const& slice,
      std::string& errorField,
      ConsolidationPolicy const& defaults
    ) noexcept;

    ////////////////////////////////////////////////////////////////////////////
    /// @brief fill and return a JSON definition of a ConsolidationPolicy object
    /// @return success or set TRI_set_errno(...) and return false
    ////////////////////////////////////////////////////////////////////////////
    bool json(arangodb::velocypack::Builder& builder) const;

    ////////////////////////////////////////////////////////////////////////////
    /// @return the iresearch policy instance or false if not initialized
    ////////////////////////////////////////////////////////////////////////////
    irs::index_writer::consolidation_policy_t const& policy() const noexcept;

    size_t segmentThreshold() const noexcept;
    float threshold() const noexcept;
    std::string const& type() const noexcept;

   private:
    irs::index_writer::consolidation_policy_t _policy;
    size_t _segmentThreshold; // apply policy if number of segments is >= value (0 == disable)
    float _threshold; // consolidation policy threshold
    std::string _type; // consolidation policy type
  };

  struct Mask {
    bool _cleanupIntervalStep;
    bool _consolidationIntervalMsec;
    bool _consolidationPolicy;
    bool _locale;
    explicit Mask(bool mask = false) noexcept;
  };

  size_t _cleanupIntervalStep; // issue cleanup after <count> commits (0 == disable)
  size_t _consolidationIntervalMsec; // issue consolidation after <interval> milliseconds (0 == disable)
  ConsolidationPolicy _consolidationPolicy; // the consolidation policy to use
  std::locale _locale; // locale used for ordering processed attribute names
  // NOTE: if adding fields don't forget to modify the default constructor !!!
  // NOTE: if adding fields don't forget to modify the copy constructor !!!
  // NOTE: if adding fields don't forget to modify the move constructor !!!
  // NOTE: if adding fields don't forget to modify the comparison operator !!!
  // NOTE: if adding fields don't forget to modify IResearchLinkMeta::Mask !!!
  // NOTE: if adding fields don't forget to modify IResearchLinkMeta::Mask constructor !!!
  // NOTE: if adding fields don't forget to modify the init(...) function !!!
  // NOTE: if adding fields don't forget to modify the json(...) function !!!
  // NOTE: if adding fields don't forget to modify the memory() function !!!

  IResearchViewMeta();
  IResearchViewMeta(IResearchViewMeta const& other);
  IResearchViewMeta(IResearchViewMeta&& other) noexcept;

  IResearchViewMeta& operator=(IResearchViewMeta&& other) noexcept;
  IResearchViewMeta& operator=(IResearchViewMeta const& other);

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
  bool init(
    arangodb::velocypack::Slice const& slice,
    std::string& errorField,
    IResearchViewMeta const& defaults = DEFAULT(),
    Mask* mask = nullptr
  ) noexcept;

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief fill and return a JSON description of a IResearchViewMeta object
  ///        do not fill values identical to ones available in 'ignoreEqual'
  ///        or (if 'mask' != nullptr) values in 'mask' that are set to false
  ///        elements are appended to an existing object
  ///        return success or set TRI_set_errno(...) and return false
  ////////////////////////////////////////////////////////////////////////////////
  bool json(
    arangodb::velocypack::Builder& builder,
    IResearchViewMeta const* ignoreEqual = nullptr,
    Mask const* mask = nullptr
  ) const;

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief fill and return a JSON description of a IResearchViewMeta object
  ///        do not fill values identical to ones available in 'ignoreEqual'
  ///        or (if 'mask' != nullptr) values in 'mask' that are set to false
  ///        elements are appended to an existing object
  ///        return success or set TRI_set_errno(...) and return false
  ////////////////////////////////////////////////////////////////////////////////
  bool json(
    arangodb::velocypack::ObjectBuilder const& builder,
    IResearchViewMeta const* ignoreEqual = nullptr,
    Mask const* mask = nullptr
  ) const;

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

  std::unordered_set<TRI_voc_cid_t> _collections; // collection links added to this view via IResearchLink creation (may contain no-longer valid cids)
  // NOTE: if adding fields don't forget to modify the default constructor !!!
  // NOTE: if adding fields don't forget to modify the copy constructor !!!
  // NOTE: if adding fields don't forget to modify the move constructor !!!
  // NOTE: if adding fields don't forget to modify the comparison operator !!!
  // NOTE: if adding fields don't forget to modify IResearchLinkMetaState::Mask !!!
  // NOTE: if adding fields don't forget to modify IResearchLinkMetaState::Mask constructor !!!
  // NOTE: if adding fields don't forget to modify the init(...) function !!!
  // NOTE: if adding fields don't forget to modify the json(...) function !!!
  // NOTE: if adding fields don't forget to modify the memory() function !!!

  IResearchViewMetaState();
  IResearchViewMetaState(IResearchViewMetaState const& other);
  IResearchViewMetaState(IResearchViewMetaState&& other) noexcept;

  IResearchViewMetaState& operator=(IResearchViewMetaState&& other) noexcept;
  IResearchViewMetaState& operator=(IResearchViewMetaState const& other);

  bool operator==(IResearchViewMetaState const& other) const noexcept;
  bool operator!=(IResearchViewMetaState const& other) const noexcept;

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief return default IResearchViewMeta values
  ////////////////////////////////////////////////////////////////////////////////
  static const IResearchViewMetaState& DEFAULT();

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief initialize IResearchViewMeta with values from a JSON description
  ///        return success or set 'errorField' to specific field with error
  ///        on failure state is undefined
  /// @param mask if set reflects which fields were initialized from JSON
  ////////////////////////////////////////////////////////////////////////////////
  bool init(
    arangodb::velocypack::Slice const& slice,
    std::string& errorField,
    IResearchViewMetaState const& defaults = DEFAULT(),
    Mask* mask = nullptr
  );

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief fill and return a JSON description of a IResearchViewMeta object
  ///        do not fill values identical to ones available in 'ignoreEqual'
  ///        or (if 'mask' != nullptr) values in 'mask' that are set to false
  ///        elements are appended to an existing object
  ///        return success or set TRI_set_errno(...) and return false
  ////////////////////////////////////////////////////////////////////////////////
  bool json(
    arangodb::velocypack::Builder& builder,
    IResearchViewMetaState const* ignoreEqual = nullptr,
    Mask const* mask = nullptr
  ) const;

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief fill and return a JSON description of a IResearchViewMeta object
  ///        do not fill values identical to ones available in 'ignoreEqual'
  ///        or (if 'mask' != nullptr) values in 'mask' that are set to false
  ///        elements are appended to an existing object
  ///        return success or set TRI_set_errno(...) and return false
  ////////////////////////////////////////////////////////////////////////////////
  bool json(
    arangodb::velocypack::ObjectBuilder const& builder,
    IResearchViewMetaState const* ignoreEqual = nullptr,
    Mask const* mask = nullptr
  ) const;

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief amount of memory in bytes occupied by this iResearch Link meta
  ////////////////////////////////////////////////////////////////////////////////
  size_t memory() const;
};

NS_END // iresearch
NS_END // arangodb

#endif
