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

#include "utils/index_utils.hpp"
#include "utils/locale_utils.hpp"

#include "VelocyPackHelper.h"
#include "Basics/StringUtils.h"
#include "velocypack/Builder.h"
#include "velocypack/Iterator.h"
#include "VocBase/LogicalView.h"

#include "IResearchViewMeta.h"

NS_LOCAL

const std::string POLICY_BYTES = "bytes"; // {threshold} > segment_bytes / (all_segment_bytes / #segments)
const std::string POLICY_BYTES_ACCUM = "bytes_accum"; // {threshold} > (segment_bytes + sum_of_merge_candidate_segment_bytes) / all_segment_bytes
const std::string POLICY_COUNT = "count"; // {threshold} > segment_docs{valid} / (all_segment_docs{valid} / #segments)
const std::string POLICY_FILL = "fill"; // {threshold} > #segment_docs{valid} / (#segment_docs{valid} + #segment_docs{removed})

NS_END

NS_BEGIN(arangodb)
NS_BEGIN(iresearch)

IResearchViewMeta::ConsolidationPolicy::ConsolidationPolicy(
    std::string const& type,
    size_t segmentThreshold,
    float threshold
): _segmentThreshold(segmentThreshold),
   _threshold(threshold),
   _type(type) {
  // set up the underlying policy for known types, else policy == false
  if (POLICY_BYTES == type) {
    // {threshold} > segment_bytes / (all_segment_bytes / #segments)
    _policy = irs::index_utils::consolidate_bytes(_threshold);
  } else if (POLICY_BYTES_ACCUM == type) {
    // {threshold} > (segment_bytes + sum_of_merge_candidate_segment_bytes) / all_segment_bytes
    _policy = irs::index_utils::consolidate_bytes_accum(_threshold);
  } else if (POLICY_COUNT == type) {
    // {threshold} > segment_docs{valid} / (all_segment_docs{valid} / #segments)
    _policy = irs::index_utils::consolidate_count(_threshold);
  } else if (POLICY_FILL == type) {
    // {threshold} > #segment_docs{valid} / (#segment_docs{valid} + #segment_docs{removed})
    _policy = irs::index_utils::consolidate_fill(threshold);
  }
}

IResearchViewMeta::ConsolidationPolicy::ConsolidationPolicy(
    IResearchViewMeta::ConsolidationPolicy const& other
) {
  *this = other;
}

IResearchViewMeta::ConsolidationPolicy::ConsolidationPolicy(
    IResearchViewMeta::ConsolidationPolicy&& other
) noexcept {
  *this = std::move(other);
}

IResearchViewMeta::ConsolidationPolicy& IResearchViewMeta::ConsolidationPolicy::operator=(
    IResearchViewMeta::ConsolidationPolicy const& other
) {
  if (this != &other) {
    _segmentThreshold = other._segmentThreshold;
    _policy = other._policy;
    _threshold = other._threshold;
    _type = other._type;
  }

  return *this;
}

IResearchViewMeta::ConsolidationPolicy& IResearchViewMeta::ConsolidationPolicy::operator=(
    IResearchViewMeta::ConsolidationPolicy&& other
) noexcept {
  if (this != &other) {
    _segmentThreshold = std::move(other._segmentThreshold);
    _policy = std::move(other._policy);
    _threshold = std::move(other._threshold);
    _type = std::move(other._type);
  }

  return *this;
}

bool IResearchViewMeta::ConsolidationPolicy::operator==(
    IResearchViewMeta::ConsolidationPolicy const& other
) const noexcept {
  return _policy // null != null
    && _segmentThreshold == other._segmentThreshold
    && _threshold == other._threshold
    && _type == other._type
    ;
}

bool IResearchViewMeta::ConsolidationPolicy::operator!=(
  IResearchViewMeta::ConsolidationPolicy const& other
  ) const noexcept {
  return !(*this == other);
}

bool IResearchViewMeta::ConsolidationPolicy::init(
    arangodb::velocypack::Slice const& slice,
    std::string& errorField,
    ConsolidationPolicy const& defaults
) noexcept {
  if (!slice.isObject()) {
    return false;
  }

  std::string policyType;

  {
    // optional string enum
    static const std::string fieldName("type");

    if (!slice.hasKey(fieldName)) {
      policyType = defaults.type();
    } else {
      auto field = slice.get(fieldName);

      if (!field.isString()) {
        errorField = fieldName;

        return false;
      }

      policyType = field.copyString();
    }
  }

  size_t segmentThreshold = 0;

  {
    // optional size_t
    static const std::string fieldName("segmentThreshold");

    if (!slice.hasKey(fieldName)) {
      segmentThreshold = defaults.segmentThreshold();
    } else {
      auto field = slice.get(fieldName);

      if (!field.isNumber<size_t>()) {
        errorField = fieldName;

        return false;
      }

      segmentThreshold = field.getNumber<size_t>();

      // arangodb::velocypack::Slice::isNumber<size_t>(...) incorrectly validates floating point numbers as fixed
      if (segmentThreshold != field.getNumber<double>()) {
        errorField = fieldName;

        return false;
      }
    }
  }

  float threshold = std::numeric_limits<float>::infinity();

  {
    // optional float
    static const std::string fieldName("threshold");

    if (!slice.hasKey(fieldName)) {
      threshold = defaults.threshold();
    } else {
      auto field = slice.get(fieldName);

      if (!field.isNumber<float>()) {
        errorField = fieldName;

        return false;
      }

      threshold = field.getNumber<float>();

      if (threshold < 0. || threshold > 1.) {
        errorField = fieldName;

        return false;
      }
    }
  }

  auto policy = ConsolidationPolicy(policyType, segmentThreshold, threshold);

  if (!policy.policy()) {
    errorField = "type";

    return false;
  }

  *this = std::move(policy);

  return true;
}

bool IResearchViewMeta::ConsolidationPolicy::json(
    arangodb::velocypack::Builder& builder
) const {
  if (!builder.isOpenObject()) {
    return false;
  }

  builder.add("segmentThreshold", arangodb::velocypack::Value(_segmentThreshold));
  builder.add("threshold", arangodb::velocypack::Value(_threshold));
  builder.add("type", toValuePair(_type));

  return true;
}

irs::index_writer::consolidation_policy_t const& IResearchViewMeta::ConsolidationPolicy::policy() const noexcept {
  return _policy;
}

size_t IResearchViewMeta::ConsolidationPolicy::segmentThreshold() const noexcept {
  return _segmentThreshold;
}

float IResearchViewMeta::ConsolidationPolicy::threshold() const noexcept {
  return _threshold;
}

std::string const& IResearchViewMeta::ConsolidationPolicy::type() const noexcept {
  return _type;
}

IResearchViewMeta::Mask::Mask(bool mask /*=false*/) noexcept
  : _cleanupIntervalStep(mask),
    _consolidationIntervalMsec(mask),
    _consolidationPolicy(mask),
    _locale(mask) {
}

IResearchViewMeta::IResearchViewMeta()
  : _cleanupIntervalStep(10),
    _consolidationIntervalMsec(60 * 1000),
    _consolidationPolicy(POLICY_BYTES_ACCUM, 300, 0.85f),
    _locale(std::locale::classic()) {
}

IResearchViewMeta::IResearchViewMeta(IResearchViewMeta const& defaults)
  : _consolidationPolicy(DEFAULT()._consolidationPolicy) { // arbitrary value overwritten below
  *this = defaults;
}

IResearchViewMeta::IResearchViewMeta(IResearchViewMeta&& other) noexcept
  : _consolidationPolicy(DEFAULT()._consolidationPolicy) { // arbitrary value overwritten below
  *this = std::move(other);
}

IResearchViewMeta& IResearchViewMeta::operator=(IResearchViewMeta&& other) noexcept {
  if (this != &other) {
    _cleanupIntervalStep = std::move(other._cleanupIntervalStep);
    _consolidationIntervalMsec = std::move(other._consolidationIntervalMsec);
    _consolidationPolicy = std::move(other._consolidationPolicy);
    _locale = std::move(other._locale);
  }

  return *this;
}

IResearchViewMeta& IResearchViewMeta::operator=(IResearchViewMeta const& other) {
  if (this != &other) {
    _cleanupIntervalStep = other._cleanupIntervalStep;
    _consolidationIntervalMsec = other._consolidationIntervalMsec;
    _consolidationPolicy = other._consolidationPolicy;
    _locale = other._locale;
  }

  return *this;
}

bool IResearchViewMeta::operator==(IResearchViewMeta const& other) const noexcept {
  if (_cleanupIntervalStep != other._cleanupIntervalStep) {
    return false; // values do not match
  }

  if (_consolidationIntervalMsec != other._consolidationIntervalMsec) {
    return false; // values do not match
  }

  if (_consolidationPolicy != other._consolidationPolicy) {
    return false; // values do not match
  }

  if (irs::locale_utils::language(_locale) != irs::locale_utils::language(other._locale)
      || irs::locale_utils::country(_locale) != irs::locale_utils::country(other._locale)
      || irs::locale_utils::encoding(_locale) != irs::locale_utils::encoding(other._locale)) {
    return false; // values do not match
  }

  return true;
}

bool IResearchViewMeta::operator!=(
  IResearchViewMeta const& other
  ) const noexcept {
  return !(*this == other);
}

/*static*/ const IResearchViewMeta& IResearchViewMeta::DEFAULT() {
  static const IResearchViewMeta meta;

  return meta;
}

bool IResearchViewMeta::init(
  arangodb::velocypack::Slice const& slice,
  std::string& errorField,
  IResearchViewMeta const& defaults /*= DEFAULT()*/,
  Mask* mask /*= nullptr*/
) noexcept {
  if (!slice.isObject()) {
    return false;
  }

  Mask tmpMask;

  if (!mask) {
    mask = &tmpMask;
  }

  {
    // optional size_t
    static const std::string fieldName("cleanupIntervalStep");

    mask->_cleanupIntervalStep = slice.hasKey(fieldName);

    if (!mask->_cleanupIntervalStep) {
      _cleanupIntervalStep = defaults._cleanupIntervalStep;
    } else {
      auto field = slice.get(fieldName);

      if (!getNumber(_cleanupIntervalStep, field)) {
        errorField = fieldName;

        return false;
      }
    }
  }

  {
    // optional size_t
    static const std::string fieldName("consolidationIntervalMsec");

    mask->_consolidationIntervalMsec = slice.hasKey(fieldName);

    if (!mask->_consolidationIntervalMsec) {
      _consolidationIntervalMsec = defaults._consolidationIntervalMsec;
    } else {
      auto field = slice.get(fieldName);

      if (!getNumber(_consolidationIntervalMsec, field)) {
        errorField = fieldName;

        return false;
      }
    }
  }

  {
    // optional object
    static const std::string fieldName("consolidationPolicy");
    std::string errorSubField;

    mask->_consolidationPolicy = slice.hasKey(fieldName);

    if (!mask->_consolidationPolicy) {
      _consolidationPolicy = defaults._consolidationPolicy;
    } else if (!_consolidationPolicy.init(slice.get(fieldName), errorSubField, defaults._consolidationPolicy)) {
      if (errorSubField.empty()) {
        errorField = fieldName;
      } else {
        errorField = fieldName + "=>" + errorSubField;
      }

      return false;
    }
  }
/* FIXME TODO temporarily disable, eventually used for ordering internal data structures
  {
    // optional locale name
    static const std::string fieldName("locale");

    mask->_locale = slice.hasKey(fieldName);

    if (!mask->_locale) {
      _locale = defaults._locale;
    } else {
      auto field = slice.get(fieldName);

      if (!field.isString()) {
        errorField = fieldName;

        return false;
      }

      auto locale = field.copyString();

      try {
        // use UTF-8 encoding since that is what JSON objects use
        _locale = std::locale::classic().name() == locale
          ? std::locale::classic() : irs::locale_utils::locale(locale);
      } catch(...) {
        errorField = fieldName;

        return false;
      }
    }
  }
*/
  return true;
}

bool IResearchViewMeta::json(
  arangodb::velocypack::Builder& builder,
  IResearchViewMeta const* ignoreEqual /*= nullptr*/,
  Mask const* mask /*= nullptr*/
) const {
  if (!builder.isOpenObject()) {
    return false;
  }

  if ((!ignoreEqual || _cleanupIntervalStep != ignoreEqual->_cleanupIntervalStep) && (!mask || mask->_cleanupIntervalStep)) {
    builder.add("cleanupIntervalStep", arangodb::velocypack::Value(_cleanupIntervalStep));
  }

  if ((!ignoreEqual || _consolidationIntervalMsec != ignoreEqual->_consolidationIntervalMsec) && (!mask || mask->_consolidationIntervalMsec)) {
    builder.add("consolidationIntervalMsec", arangodb::velocypack::Value(_consolidationIntervalMsec));
  }

  if ((!ignoreEqual || _consolidationPolicy != ignoreEqual->_consolidationPolicy) && (!mask || mask->_consolidationPolicy)) {
    builder.add(
      "consolidationPolicy",
      arangodb::velocypack::Value(arangodb::velocypack::ValueType::Object)
    );

    if (!_consolidationPolicy.json(builder)) {
      return false;
    }

    builder.close();
  }
/* FIXME TODO temporarily disable, eventually used for ordering internal data structures
  if ((!ignoreEqual || _locale != ignoreEqual->_locale) && (!mask || mask->_locale)) {
    builder.add("locale", arangodb::velocypack::Value(irs::locale_utils::name(_locale)));
  }
*/
  return true;
}

bool IResearchViewMeta::json(
  arangodb::velocypack::ObjectBuilder const& builder,
  IResearchViewMeta const* ignoreEqual /*= nullptr*/,
  Mask const* mask /*= nullptr*/
) const {
  return builder.builder && json(*(builder.builder), ignoreEqual, mask);
}

size_t IResearchViewMeta::memory() const {
  auto size = sizeof(IResearchViewMeta);

  return size;
}

IResearchViewMetaState::Mask::Mask(bool mask /*=false*/) noexcept
  : _collections(mask) {
}

IResearchViewMetaState::IResearchViewMetaState() {
}

IResearchViewMetaState::IResearchViewMetaState(
    IResearchViewMetaState const& defaults
) {
  *this = defaults;
}

IResearchViewMetaState::IResearchViewMetaState(
    IResearchViewMetaState&& other
) noexcept {
  *this = std::move(other);
}

IResearchViewMetaState& IResearchViewMetaState::operator=(
    IResearchViewMetaState&& other
) noexcept {
  if (this != &other) {
    _collections = std::move(other._collections);
  }

  return *this;
}

IResearchViewMetaState& IResearchViewMetaState::operator=(
    IResearchViewMetaState const& other
) {
  if (this != &other) {
    _collections = other._collections;
  }

  return *this;
}

bool IResearchViewMetaState::operator==(
    IResearchViewMetaState const& other
) const noexcept {
  if (_collections != other._collections) {
    return false; // values do not match
  }

  return true;
}

bool IResearchViewMetaState::operator!=(
  IResearchViewMetaState const& other
  ) const noexcept {
  return !(*this == other);
}

/*static*/ const IResearchViewMetaState& IResearchViewMetaState::DEFAULT() {
  static const IResearchViewMetaState meta;

  return meta;
}

bool IResearchViewMetaState::init(
  arangodb::velocypack::Slice const& slice,
  std::string& errorField,
  IResearchViewMetaState const& defaults /*= DEFAULT()*/,
  Mask* mask /*= nullptr*/
) {
  if (!slice.isObject()) {
    errorField = "not an object";
    return false;
  }

  Mask tmpMask;

  if (!mask) {
    mask = &tmpMask;
  }

  {
    // optional uint64 list
    static const std::string fieldName("collections");

    mask->_collections = slice.hasKey(fieldName);

    if (!mask->_collections) {
      _collections = defaults._collections;
    } else {
      auto field = slice.get(fieldName);

      if (!field.isArray()) {
        errorField = fieldName;

        return false;
      }

      _collections.clear(); // reset to match read values exactly

      for (arangodb::velocypack::ArrayIterator itr(field); itr.valid(); ++itr) {
        decltype(_collections)::key_type value;

        if (!getNumber(value, itr.value())) { // [ <collectionId 1> ... <collectionId N> ]
          errorField = fieldName + "=>[" + arangodb::basics::StringUtils::itoa(itr.index()) + "]";

          return false;
        }

        _collections.emplace(value);
      }
    }
  }

  return true;
}

bool IResearchViewMetaState::json(
  arangodb::velocypack::Builder& builder,
  IResearchViewMetaState const* ignoreEqual /*= nullptr*/,
  Mask const* mask /*= nullptr*/
) const {
  if (!builder.isOpenObject()) {
    return false;
  }

  if ((!ignoreEqual || _collections != ignoreEqual->_collections) && (!mask || mask->_collections)) {
    arangodb::velocypack::Builder subBuilder;

    {
      arangodb::velocypack::ArrayBuilder subBuilderWrapper(&subBuilder);

      for (auto& cid: _collections) {
        subBuilderWrapper->add(arangodb::velocypack::Value(cid));
      }
    }

    builder.add("collections", subBuilder.slice());
  }

  return true;
}

bool IResearchViewMetaState::json(
  arangodb::velocypack::ObjectBuilder const& builder,
  IResearchViewMetaState const* ignoreEqual /*= nullptr*/,
  Mask const* mask /*= nullptr*/
) const {
  return builder.builder && json(*(builder.builder), ignoreEqual, mask);
}

size_t IResearchViewMetaState::memory() const {
  auto size = sizeof(IResearchViewMetaState);

  size += sizeof(TRI_voc_cid_t) * _collections.size();

  return size;
}

NS_END // iresearch
NS_END // arangodb

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------