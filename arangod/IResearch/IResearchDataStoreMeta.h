////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2022 ArangoDB GmbH, Cologne, Germany
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

#include <velocypack/Builder.h>
#include "index/index_writer.hpp"

namespace arangodb::iresearch {

struct IResearchDataStoreMeta {
  class ConsolidationPolicy {
   public:
    ConsolidationPolicy() = default;
    ConsolidationPolicy(irs::ConsolidationPolicy&& policy,
                        VPackBuilder&& properties) noexcept
        : _policy(std::move(policy)), _properties(std::move(properties)) {}

    irs::ConsolidationPolicy const& policy() const noexcept { return _policy; }

    VPackSlice properties() const noexcept { return _properties.slice(); }

   private:
    irs::ConsolidationPolicy _policy;  // policy instance (false == disable)
    VPackBuilder _properties;          // normalized policy definition
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
