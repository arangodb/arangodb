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

#include "IResearchDataStoreMeta.h"
#include "IResearchCommon.h"
#include "VelocyPackHelper.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/StringUtils.h"
#include <velocypack/Builder.h>
#include <velocypack/Iterator.h>
#include <velocypack/Parser.h>
#include "utils/index_utils.hpp"

namespace {

using namespace arangodb;
using namespace arangodb::iresearch;

// {threshold} > (segment_bytes + // sum_of_merge_candidate_segment_bytes) /
// all_segment_bytes
constexpr std::string_view kPolicyBytesAccum = "bytes_accum";

// scoring policy based on byte size and live docs
constexpr std::string_view kPolicyTier = "tier";

template<typename T>
IResearchDataStoreMeta::ConsolidationPolicy createConsolidationPolicy(
    VPackSlice slice, std::string& errorField);

template<>
IResearchDataStoreMeta::ConsolidationPolicy
createConsolidationPolicy<irs::index_utils::consolidate_bytes_accum>(
    VPackSlice slice, std::string& errorField) {
  irs::index_utils::consolidate_bytes_accum options;
  velocypack::Builder properties;

  {
    // optional float
    constexpr std::string_view kFieldName = "threshold";

    if (slice.hasKey(kFieldName)) {
      auto field = slice.get(kFieldName);

      if (!field.isNumber<float>()) {
        errorField = kFieldName;

        return {};
      }

      options.threshold = field.getNumber<float>();

      if (options.threshold < 0. || options.threshold > 1.) {
        errorField = kFieldName;

        return {};
      }
    }
  }

  properties.openObject();
  properties.add("type", velocypack::Value(kPolicyBytesAccum));
  properties.add("threshold", velocypack::Value(options.threshold));
  properties.close();

  return {irs::index_utils::consolidation_policy(options),
          std::move(properties)};
}

template<>
IResearchDataStoreMeta::ConsolidationPolicy
createConsolidationPolicy<irs::index_utils::consolidate_tier>(
    VPackSlice slice, std::string& errorField) {
  irs::index_utils::consolidate_tier options;
  VPackBuilder properties;

  {
    // optional size_t
    constexpr std::string_view kFieldName = "segmentsBytesFloor";

    if (slice.hasKey(kFieldName)) {
      auto field = slice.get(kFieldName);

      if (!field.isNumber<size_t>()) {
        errorField = kFieldName;

        return {};
      }

      options.floor_segment_bytes = field.getNumber<size_t>();
    }
  }

  {
    // optional size_t
    constexpr std::string_view kFieldName = "segmentsBytesMax";

    if (slice.hasKey(kFieldName)) {
      auto field = slice.get(kFieldName);

      if (!field.isNumber<size_t>()) {
        errorField = kFieldName;

        return {};
      }

      options.max_segments_bytes = field.getNumber<size_t>();
    }
  }

  {
    // optional size_t
    constexpr std::string_view kFieldName = "segmentsMax";

    if (slice.hasKey(kFieldName)) {
      auto field = slice.get(kFieldName);

      if (!field.isNumber<size_t>()) {
        errorField = kFieldName;

        return {};
      }

      options.max_segments = field.getNumber<size_t>();
    }
  }

  {
    // optional size_t
    constexpr std::string_view kFieldName = "segmentsMin";

    if (slice.hasKey(kFieldName)) {
      auto field = slice.get(kFieldName);

      if (!field.isNumber<size_t>()) {
        errorField = kFieldName;

        return {};
      }

      options.min_segments = field.getNumber<size_t>();
    }
  }

  {
    // optional double
    constexpr std::string_view kFieldName = "minScore";

    if (slice.hasKey(kFieldName)) {
      auto field = slice.get(kFieldName);

      if (!field.isNumber<double_t>()) {
        errorField = kFieldName;

        return {};
      }

      options.min_score = field.getNumber<double_t>();
    }
  }

  properties.openObject();
  properties.add("type", VPackValue(kPolicyTier));
  properties.add("segmentsBytesFloor", VPackValue(options.floor_segment_bytes));
  properties.add("segmentsBytesMax", VPackValue(options.max_segments_bytes));
  properties.add("segmentsMax", VPackValue(options.max_segments));
  properties.add("segmentsMin", VPackValue(options.min_segments));
  properties.add("minScore", VPackValue(options.min_score));
  properties.close();

  return {irs::index_utils::consolidation_policy(options),
          std::move(properties)};
}

}  // namespace

namespace arangodb::iresearch {
IResearchDataStoreMeta::Mask::Mask(bool mask /*=false*/) noexcept
    : _cleanupIntervalStep(mask),
      _commitIntervalMsec(mask),
      _consolidationIntervalMsec(mask),
      _consolidationPolicy(mask),
      _version(mask),
      _writebufferActive(mask),
      _writebufferIdle(mask),
      _writebufferSizeMax(mask) {}

IResearchDataStoreMeta::IResearchDataStoreMeta()
    : _cleanupIntervalStep(2),
      _commitIntervalMsec(1000),
      _consolidationIntervalMsec(1000),
      _version(static_cast<uint32_t>(ViewVersion::MAX)),
      _writebufferActive(0),
      _writebufferIdle(64),
      _writebufferSizeMax(32 * (size_t(1) << 20)) {  // 32MB
  std::string errorField;

  // cppcheck-suppress useInitializationList
  _consolidationPolicy =
      createConsolidationPolicy<irs::index_utils::consolidate_tier>(
          velocypack::Parser::fromJson("{ \"type\": \"tier\" }")->slice(),
          errorField);
  assert(_consolidationPolicy.policy());  // ensure above syntax is correct
}

void IResearchDataStoreMeta::storeFull(IResearchDataStoreMeta const& other) {
  if (this == &other) {
    return;
  }
  _cleanupIntervalStep = other._cleanupIntervalStep;
  _commitIntervalMsec = other._commitIntervalMsec;
  _consolidationIntervalMsec = other._consolidationIntervalMsec;
  _consolidationPolicy = other._consolidationPolicy;
  _version = other._version;
  _writebufferActive = other._writebufferActive;
  _writebufferIdle = other._writebufferIdle;
  _writebufferSizeMax = other._writebufferSizeMax;
}

void IResearchDataStoreMeta::storeFull(
    IResearchDataStoreMeta&& other) noexcept {
  if (this == &other) {
    return;
  }
  _cleanupIntervalStep = other._cleanupIntervalStep;
  _commitIntervalMsec = other._commitIntervalMsec;
  _consolidationIntervalMsec = other._consolidationIntervalMsec;
  _consolidationPolicy = std::move(other._consolidationPolicy);
  _version = other._version;
  _writebufferActive = other._writebufferActive;
  _writebufferIdle = other._writebufferIdle;
  _writebufferSizeMax = other._writebufferSizeMax;
}

void IResearchDataStoreMeta::storePartial(
    IResearchDataStoreMeta&& other) noexcept {
  if (this == &other) {
    return;
  }
  _cleanupIntervalStep = other._cleanupIntervalStep;
  _commitIntervalMsec = other._commitIntervalMsec;
  _consolidationIntervalMsec = other._consolidationIntervalMsec;
  _consolidationPolicy = std::move(other._consolidationPolicy);
}

bool IResearchDataStoreMeta::json(
    velocypack::Builder& builder,
    IResearchDataStoreMeta const* ignoreEqual /*= nullptr*/,
    IResearchDataStoreMeta::Mask const* mask /*= nullptr*/) const {
  if (!builder.isOpenObject()) {
    return false;
  }

  if ((!ignoreEqual ||
       _cleanupIntervalStep != ignoreEqual->_cleanupIntervalStep) &&
      (!mask || mask->_cleanupIntervalStep)) {
    builder.add(StaticStrings::CleanupIntervalStep,
                velocypack::Value(_cleanupIntervalStep));
  }

  if ((!ignoreEqual ||
       _commitIntervalMsec != ignoreEqual->_commitIntervalMsec) &&
      (!mask || mask->_commitIntervalMsec)) {
    builder.add(StaticStrings::CommitIntervalMsec,
                velocypack::Value(_commitIntervalMsec));
  }

  if ((!ignoreEqual ||
       _consolidationIntervalMsec != ignoreEqual->_consolidationIntervalMsec) &&
      (!mask || mask->_consolidationIntervalMsec)) {
    builder.add(StaticStrings::ConsolidationIntervalMsec,
                velocypack::Value(_consolidationIntervalMsec));
  }

  if ((!ignoreEqual ||
       !basics::VelocyPackHelper::equal(
           _consolidationPolicy.properties(),
           ignoreEqual->_consolidationPolicy.properties(), false)) &&
      (!mask || mask->_consolidationPolicy)) {
    builder.add(StaticStrings::ConsolidationPolicy,
                _consolidationPolicy.properties());
  }

  if ((!ignoreEqual || _version != ignoreEqual->_version) &&
      (!mask || mask->_version)) {
    builder.add(StaticStrings::VersionField, velocypack::Value(_version));
  }

  if ((!ignoreEqual || _writebufferActive != ignoreEqual->_writebufferActive) &&
      (!mask || mask->_writebufferActive)) {
    builder.add(StaticStrings::WritebufferActive,
                velocypack::Value(_writebufferActive));
  }

  if ((!ignoreEqual || _writebufferIdle != ignoreEqual->_writebufferIdle) &&
      (!mask || mask->_writebufferIdle)) {
    builder.add(StaticStrings::WritebufferIdle,
                velocypack::Value(_writebufferIdle));
  }

  if ((!ignoreEqual ||
       _writebufferSizeMax != ignoreEqual->_writebufferSizeMax) &&
      (!mask || mask->_writebufferSizeMax)) {
    builder.add(StaticStrings::WritebufferSizeMax,
                velocypack::Value(_writebufferSizeMax));
  }

  return true;
}

bool IResearchDataStoreMeta::init(velocypack::Slice slice,
                                  std::string& errorField,
                                  IResearchDataStoreMeta const& defaults,
                                  Mask* mask) noexcept {
  if (!slice.isObject()) {
    errorField = "Object is expected";
    return false;
  }

  Mask tmpMask;

  if (!mask) {
    mask = &tmpMask;
  }

  {
    // optional uint32_t
    constexpr std::string_view kFieldName{StaticStrings::VersionField};

    mask->_version = slice.hasKey(kFieldName);

    if (!mask->_version) {
      _version = defaults._version;
    } else {
      auto field = slice.get(kFieldName);

      if (!getNumber(_version, field)) {
        errorField = kFieldName;

        return false;
      }
    }
  }

  {
    // optional size_t
    constexpr std::string_view kFieldName{StaticStrings::CleanupIntervalStep};

    mask->_cleanupIntervalStep = slice.hasKey(kFieldName);

    if (!mask->_cleanupIntervalStep) {
      _cleanupIntervalStep = defaults._cleanupIntervalStep;
    } else {
      auto field = slice.get(kFieldName);

      if (!getNumber(_cleanupIntervalStep, field)) {
        errorField = kFieldName;

        return false;
      }
    }
  }

  {
    // optional size_t
    constexpr std::string_view kFieldName{StaticStrings::CommitIntervalMsec};

    mask->_commitIntervalMsec = slice.hasKey(kFieldName);

    if (!mask->_commitIntervalMsec) {
      _commitIntervalMsec = defaults._commitIntervalMsec;
    } else {
      auto field = slice.get(kFieldName);

      if (!getNumber(_commitIntervalMsec, field)) {
        errorField = kFieldName;

        return false;
      }
    }
  }

  {
    // optional size_t
    constexpr std::string_view kFieldName{
        StaticStrings::ConsolidationIntervalMsec};

    mask->_consolidationIntervalMsec = slice.hasKey(kFieldName);

    if (!mask->_consolidationIntervalMsec) {
      _consolidationIntervalMsec = defaults._consolidationIntervalMsec;
    } else {
      auto field = slice.get(kFieldName);

      if (!getNumber(_consolidationIntervalMsec, field)) {
        errorField = kFieldName;

        return false;
      }
    }
  }

  {
    // optional object
    constexpr std::string_view kFieldName{StaticStrings::ConsolidationPolicy};
    std::string errorSubField;

    mask->_consolidationPolicy = slice.hasKey(kFieldName);

    if (!mask->_consolidationPolicy) {
      _consolidationPolicy = defaults._consolidationPolicy;
    } else {
      auto field = slice.get(kFieldName);

      if (!field.isObject()) {
        errorField = kFieldName;

        return false;
      }

      // required string enum
      constexpr std::string_view kTypeFieldName("type");

      if (!field.hasKey(kTypeFieldName)) {
        errorField =
            std::string{kFieldName} + "." + std::string{kTypeFieldName};

        return false;
      }

      auto typeField = field.get(kTypeFieldName);

      if (!typeField.isString()) {
        errorField =
            std::string{kFieldName} + "." + std::string{kTypeFieldName};

        return false;
      }

      auto const type = typeField.stringView();

      if (kPolicyBytesAccum == type) {
        _consolidationPolicy = createConsolidationPolicy<
            irs::index_utils::consolidate_bytes_accum>(field, errorSubField);
      } else if (kPolicyTier == type) {
        _consolidationPolicy =
            createConsolidationPolicy<irs::index_utils::consolidate_tier>(
                field, errorSubField);
      } else {
        errorField =
            std::string{kFieldName} + "." + std::string{kTypeFieldName};

        return false;
      }

      if (!_consolidationPolicy.policy()) {
        if (errorSubField.empty()) {
          errorField = kFieldName;
        } else {
          errorField = std::string{kFieldName} + "." + errorSubField;
        }

        return false;
      }
    }
  }

  {
    // optional size_t
    constexpr std::string_view kFieldName(StaticStrings::WritebufferActive);

    mask->_writebufferActive = slice.hasKey(kFieldName);

    if (!mask->_writebufferActive) {
      _writebufferActive = defaults._writebufferActive;
    } else {
      auto field = slice.get(kFieldName);

      if (!getNumber(_writebufferActive, field)) {
        errorField = kFieldName;

        return false;
      }
    }
  }

  {
    // optional size_t
    constexpr std::string_view kFieldName(StaticStrings::WritebufferIdle);

    mask->_writebufferIdle = slice.hasKey(kFieldName);

    if (!mask->_writebufferIdle) {
      _writebufferIdle = defaults._writebufferIdle;
    } else {
      auto field = slice.get(kFieldName);

      if (!getNumber(_writebufferIdle, field)) {
        errorField = kFieldName;

        return false;
      }
    }
  }

  {
    // optional size_t
    constexpr std::string_view kFieldName(StaticStrings::WritebufferSizeMax);

    mask->_writebufferSizeMax = slice.hasKey(kFieldName);

    if (!mask->_writebufferSizeMax) {
      _writebufferSizeMax = defaults._writebufferSizeMax;
    } else {
      auto field = slice.get(kFieldName);

      if (!getNumber(_writebufferSizeMax, field)) {
        errorField = kFieldName;

        return false;
      }
    }
  }
  return true;
}

bool IResearchDataStoreMeta::operator==(
    IResearchDataStoreMeta const& other) const noexcept {
  if (_consolidationIntervalMsec != other._consolidationIntervalMsec ||
      _cleanupIntervalStep != other._cleanupIntervalStep ||
      _commitIntervalMsec != other._commitIntervalMsec ||
      _version != other._version ||
      _writebufferActive != other._writebufferActive ||
      _writebufferIdle != other._writebufferIdle ||
      _writebufferSizeMax != other._writebufferSizeMax) {
    return false;
  }

  try {
    if (!basics::VelocyPackHelper::equal(
            _consolidationPolicy.properties(),
            other._consolidationPolicy.properties(), false)) {
      return false;  // values do not match
    }
  } catch (...) {
    return false;  // exception during match
  }
  return true;
}

bool IResearchDataStoreMeta::operator!=(
    IResearchDataStoreMeta const& other) const noexcept {
  return !(*this == other);
}

}  // namespace arangodb::iresearch
