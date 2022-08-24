////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2022 ArangoDB GmbH, Cologne, Germany
///
/// The Programs (which include both the software and documentation) contain
/// proprietary information of ArangoDB GmbH; they are provided under a license
/// agreement containing restrictions on use and disclosure and are also
/// protected by copyright, patent and other intellectual and industrial
/// property laws. Reverse engineering, disassembly or decompilation of the
/// Programs, except to the extent required to obtain interoperability with
/// other independently created software or as specified by law, is prohibited.
///
/// It shall be the licensee's responsibility to take all appropriate fail-safe,
/// backup, redundancy, and other measures to ensure the safe use of
/// applications if the Programs are used for purposes such as nuclear,
/// aviation, mass transit, medical, or other inherently dangerous applications,
/// and ArangoDB GmbH disclaims liability for any damages caused by such use of
/// the Programs.
///
/// This software is the confidential and proprietary information of ArangoDB
/// GmbH. You shall not disclose such confidential and proprietary information
/// and shall use it only in accordance with the terms of the license agreement
/// you entered into with ArangoDB GmbH.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Andrei Lobov
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <velocypack/Builder.h>
#include "index/index_writer.hpp"

namespace arangodb::iresearch {

struct IResearchDataStoreMeta {
  class ConsolidationPolicy {
   public:
    ConsolidationPolicy() = default;
    ConsolidationPolicy(irs::index_writer::consolidation_policy_t&& policy,
                        VPackBuilder&& properties) noexcept
        : _policy(std::move(policy)), _properties(std::move(properties)) {}

    irs::index_writer::consolidation_policy_t const& policy() const noexcept {
      return _policy;
    }

    VPackSlice properties() const noexcept { return _properties.slice(); }

   private:
    irs::index_writer::consolidation_policy_t
        _policy;               // policy instance (false == disable)
    VPackBuilder _properties;  // normalized policy definition
  };

  IResearchDataStoreMeta();

  IResearchDataStoreMeta(IResearchDataStoreMeta&& other) noexcept = delete;
  IResearchDataStoreMeta& operator=(IResearchDataStoreMeta&& other) noexcept =
      delete;
  IResearchDataStoreMeta& operator=(IResearchDataStoreMeta const& other) =
      delete;

  void storeFull(IResearchDataStoreMeta const& other);
  void storeFull(IResearchDataStoreMeta&& other) noexcept;
  void storePartial(IResearchDataStoreMeta&& other) noexcept;

  struct Mask {
    bool _cleanupIntervalStep;
    bool _commitIntervalMsec;
    bool _consolidationIntervalMsec;
    bool _consolidationPolicy;
    bool _version;
    bool _writebufferActive;
    bool _writebufferIdle;
    bool _writebufferSizeMax;
    explicit Mask(bool mask = false) noexcept;
  };

  bool operator==(IResearchDataStoreMeta const& other) const noexcept;
  bool operator!=(IResearchDataStoreMeta const& other) const noexcept;

  bool init(velocypack::Slice slice, std::string& errorField,
            IResearchDataStoreMeta const& defaults, Mask* mask) noexcept;

  bool json(velocypack::Builder& builder,
            IResearchDataStoreMeta const* ignoreEqual = nullptr,
            Mask const* mask = nullptr) const;

  size_t _cleanupIntervalStep{};
  // issue cleanup after <count> commits (0 == disable)
  size_t _commitIntervalMsec{};
  // issue commit after <interval> milliseconds (0 == disable)
  size_t _consolidationIntervalMsec{};
  // issue consolidation after <interval> milliseconds (0 == disable)
  ConsolidationPolicy _consolidationPolicy;  // the consolidation policy to use
  uint32_t _version{};  // the version of the iresearch interface e.g. which
                        // how data is stored in iresearch (default == latest)
  size_t _writebufferActive{};  // maximum number of concurrent segments
  // before segment acquisition blocks,
  // e.g. max number of concurrent transactions (0 == unlimited)
  size_t _writebufferIdle{};  // maximum number of segments cached in the pool
  size_t _writebufferSizeMax{};  // maximum memory byte size per segment
  // before a segment flush is triggered (0 == unlimited)
};

}  // namespace arangodb::iresearch
