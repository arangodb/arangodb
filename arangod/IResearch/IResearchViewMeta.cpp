////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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

#include "utils/index_utils.hpp"
#include "utils/locale_utils.hpp"

#include "Basics/StringUtils.h"
#include "Basics/VelocyPackHelper.h"
#include "IResearchCommon.h"
#include "VelocyPackHelper.h"
#include "VocBase/LogicalView.h"

#include <velocypack/Builder.h>
#include <velocypack/Iterator.h>
#include <velocypack/Parser.h>

#include "IResearchViewMeta.h"

namespace {

// {threshold} > (segment_bytes + // sum_of_merge_candidate_segment_bytes) / all_segment_bytes
const std::string POLICY_BYTES_ACCUM = "bytes_accum";

// scoring policy based on byte size and live docs
const std::string POLICY_TIER = "tier";

template <typename T>
arangodb::iresearch::IResearchViewMeta::ConsolidationPolicy createConsolidationPolicy(
    arangodb::velocypack::Slice const& slice, std::string& errorField);

template <>
arangodb::iresearch::IResearchViewMeta::ConsolidationPolicy
createConsolidationPolicy<irs::index_utils::consolidate_bytes_accum>(
    arangodb::velocypack::Slice const& slice, std::string& errorField) {
  irs::index_utils::consolidate_bytes_accum options;
  arangodb::velocypack::Builder properties;

  {
    // optional float
    static const std::string fieldName("threshold");

    if (slice.hasKey(fieldName)) {
      auto field = slice.get(fieldName);

      if (!field.isNumber<float>()) {
        errorField = fieldName;

        return arangodb::iresearch::IResearchViewMeta::ConsolidationPolicy();
      }

      options.threshold = field.getNumber<float>();

      if (options.threshold < 0. || options.threshold > 1.) {
        errorField = fieldName;

        return arangodb::iresearch::IResearchViewMeta::ConsolidationPolicy();
      }
    }
  }

  properties.openObject();
  properties.add("type", arangodb::velocypack::Value(POLICY_BYTES_ACCUM));
  properties.add("threshold", arangodb::velocypack::Value(options.threshold));
  properties.close();

  return arangodb::iresearch::IResearchViewMeta::ConsolidationPolicy{
      irs::index_utils::consolidation_policy(options), std::move(properties)};
}

template <>
arangodb::iresearch::IResearchViewMeta::ConsolidationPolicy createConsolidationPolicy<irs::index_utils::consolidate_tier>(
    arangodb::velocypack::Slice const& slice, std::string& errorField) {
  irs::index_utils::consolidate_tier options;
  arangodb::velocypack::Builder properties;

  {
    // optional size_t
    static const std::string fieldName("segmentsBytesFloor");

    if (slice.hasKey(fieldName)) {
      auto field = slice.get(fieldName);

      if (!field.isNumber<size_t>()) {
        errorField = fieldName;

        return arangodb::iresearch::IResearchViewMeta::ConsolidationPolicy();
      }

      options.floor_segment_bytes = field.getNumber<size_t>();
    }
  }

  {
    // optional size_t
    static const std::string fieldName("segmentsBytesMax");

    if (slice.hasKey(fieldName)) {
      auto field = slice.get(fieldName);

      if (!field.isNumber<size_t>()) {
        errorField = fieldName;

        return arangodb::iresearch::IResearchViewMeta::ConsolidationPolicy();
      }

      options.max_segments_bytes = field.getNumber<size_t>();
    }
  }

  {
    // optional size_t
    static const std::string fieldName("segmentsMax");

    if (slice.hasKey(fieldName)) {
      auto field = slice.get(fieldName);

      if (!field.isNumber<size_t>()) {
        errorField = fieldName;

        return arangodb::iresearch::IResearchViewMeta::ConsolidationPolicy();
      }

      options.max_segments = field.getNumber<size_t>();
    }
  }

  {
    // optional size_t
    static const std::string fieldName("segmentsMin");

    if (slice.hasKey(fieldName)) {
      auto field = slice.get(fieldName);

      if (!field.isNumber<size_t>()) {
        errorField = fieldName;

        return arangodb::iresearch::IResearchViewMeta::ConsolidationPolicy();
      }

      options.min_segments = field.getNumber<size_t>();
    }
  }

  {
    // optional double
    static const std::string fieldName("minScore");

    if (slice.hasKey(fieldName)) {
      auto field = slice.get(fieldName);

      if (!field.isNumber<double_t>()) {
        errorField = fieldName;

        return arangodb::iresearch::IResearchViewMeta::ConsolidationPolicy();
      }

      options.min_score = field.getNumber<double_t>();
    }
  }

  properties.openObject();
  properties.add("type", VPackValue(POLICY_TIER));
  properties.add("segmentsBytesFloor", VPackValue(options.floor_segment_bytes));
  properties.add("segmentsBytesMax", VPackValue(options.max_segments_bytes));
  properties.add("segmentsMax", VPackValue(options.max_segments));
  properties.add("segmentsMin", VPackValue(options.min_segments));
  properties.add("minScore", VPackValue(options.min_score));
  properties.close();

  return arangodb::iresearch::IResearchViewMeta::ConsolidationPolicy{
      irs::index_utils::consolidation_policy(options), std::move(properties)};
}

}  // namespace

namespace arangodb {
namespace iresearch {

IResearchViewMeta::Mask::Mask(bool mask /*=false*/) noexcept
    : _cleanupIntervalStep(mask),
      _commitIntervalMsec(mask),
      _consolidationIntervalMsec(mask),
      _consolidationPolicy(mask),
      _locale(mask),
      _version(mask),
      _writebufferActive(mask),
      _writebufferIdle(mask),
      _writebufferSizeMax(mask),
      _primarySort(mask),
      _storedValues(mask),
      _primarySortCompression(mask) {}

IResearchViewMeta::IResearchViewMeta()
    : _cleanupIntervalStep(2),
      _commitIntervalMsec(1000),
      _consolidationIntervalMsec(1000),
      _locale(std::locale::classic()),
      _version(LATEST_VERSION),
      _writebufferActive(0),
      _writebufferIdle(64),
      _writebufferSizeMax(32 * (size_t(1) << 20)),  // 32MB
      _primarySortCompression{getDefaultCompression()} {
  std::string errorField;

  // cppcheck-suppress useInitializationList
  _consolidationPolicy =
      createConsolidationPolicy<irs::index_utils::consolidate_tier>(
          arangodb::velocypack::Parser::fromJson(
              "{ \"type\": \"tier\" }")
              ->slice(),
          errorField);
  assert(_consolidationPolicy.policy());  // ensure above syntax is correct
}

IResearchViewMeta::IResearchViewMeta(IResearchViewMeta const& defaults)
    : _consolidationPolicy(DEFAULT()._consolidationPolicy) {  // arbitrary value overwritten below
  *this = defaults;
}

IResearchViewMeta::IResearchViewMeta(IResearchViewMeta&& other) noexcept
    : _consolidationPolicy(DEFAULT()._consolidationPolicy) {  // arbitrary value overwritten below
  *this = std::move(other);
}

IResearchViewMeta& IResearchViewMeta::operator=(IResearchViewMeta&& other) noexcept {
  if (this != &other) {
    _cleanupIntervalStep = std::move(other._cleanupIntervalStep);
    _commitIntervalMsec = std::move(other._commitIntervalMsec);
    _consolidationIntervalMsec = std::move(other._consolidationIntervalMsec);
    _consolidationPolicy = std::move(other._consolidationPolicy);
    _locale = std::move(other._locale);
    _version = std::move(other._version);
    _writebufferActive = std::move(other._writebufferActive);
    _writebufferIdle = std::move(other._writebufferIdle);
    _writebufferSizeMax = std::move(other._writebufferSizeMax);
    _primarySort = std::move(other._primarySort);
    _storedValues = std::move(other._storedValues);
    _primarySortCompression = std::move(other._primarySortCompression);
  }

  return *this;
}

IResearchViewMeta& IResearchViewMeta::operator=(IResearchViewMeta const& other) {
  if (this != &other) {
    _cleanupIntervalStep = other._cleanupIntervalStep;
    _commitIntervalMsec = other._commitIntervalMsec;
    _consolidationIntervalMsec = other._consolidationIntervalMsec;
    _consolidationPolicy = other._consolidationPolicy;
    _locale = other._locale;
    _version = other._version;
    _writebufferActive = other._writebufferActive;
    _writebufferIdle = other._writebufferIdle;
    _writebufferSizeMax = other._writebufferSizeMax;
    _primarySort = other._primarySort;
    _storedValues = other._storedValues;
    _primarySortCompression = other._primarySortCompression;
  }

  return *this;
}

bool IResearchViewMeta::operator==(IResearchViewMeta const& other) const noexcept {
  if (_cleanupIntervalStep != other._cleanupIntervalStep) {
    return false;  // values do not match
  }

  if (_commitIntervalMsec != other._commitIntervalMsec) {
    return false; // values do not match
  }

  if (_consolidationIntervalMsec != other._consolidationIntervalMsec) {
    return false;  // values do not match
  }

  try {
    if (!basics::VelocyPackHelper::equal(_consolidationPolicy.properties(), other._consolidationPolicy.properties(), false)) {
      return false; // values do not match
    }
  } catch (...) {
    return false; // exception during match
  }

  if (irs::locale_utils::language(_locale) != irs::locale_utils::language(other._locale) ||
      irs::locale_utils::country(_locale) != irs::locale_utils::country(other._locale) ||
      irs::locale_utils::encoding(_locale) != irs::locale_utils::encoding(other._locale)) {
    return false;
  }

  if (_version != other._version ||
      _writebufferActive != other._writebufferActive ||
      _writebufferIdle != other._writebufferIdle ||
      _writebufferSizeMax != other._writebufferSizeMax ||
      _primarySort != other._primarySort ||
      _storedValues != other._storedValues) {
    return false;
  }

  return true;
}

bool IResearchViewMeta::operator!=(IResearchViewMeta const& other) const noexcept {
  return !(*this == other);
}

/*static*/ const IResearchViewMeta& IResearchViewMeta::DEFAULT() {
  static const IResearchViewMeta meta;

  return meta;
}

bool IResearchViewMeta::init(arangodb::velocypack::Slice const& slice, std::string& errorField,
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
    // optional uint32_t
    static const std::string fieldName(StaticStrings::VersionField);

    mask->_version = slice.hasKey(fieldName);

    if (!mask->_version) {
      _version = defaults._version;
    } else {
      auto field = slice.get(fieldName);

      if (!getNumber(_version, field)) {
        errorField = fieldName;

        return false;
      }
    }
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
    static const std::string fieldName("commitIntervalMsec");

    mask->_commitIntervalMsec = slice.hasKey(fieldName);

    if (!mask->_commitIntervalMsec) {
      _commitIntervalMsec = defaults._commitIntervalMsec;
    } else {
      auto field = slice.get(fieldName);

      if (!getNumber(_commitIntervalMsec, field)) {
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
    } else {
      auto field = slice.get(fieldName);

      if (!field.isObject()) {
        errorField = fieldName;

        return false;
      }

      // required string enum
      static const std::string typeFieldName("type");

      if (!field.hasKey(typeFieldName)) {
        errorField = fieldName + "." + typeFieldName;

        return false;
      }

      auto typeField = field.get(typeFieldName);

      if (!typeField.isString()) {
        errorField = fieldName + "." + typeFieldName;

        return false;
      }

      auto type = typeField.copyString();

      if (POLICY_BYTES_ACCUM == type) {
        _consolidationPolicy =
            createConsolidationPolicy<irs::index_utils::consolidate_bytes_accum>(field, errorSubField);
      } else if (POLICY_TIER == type) {
        _consolidationPolicy =
            createConsolidationPolicy<irs::index_utils::consolidate_tier>(field, errorSubField);
      } else {
        errorField = fieldName + "." + typeFieldName;

        return false;
      }

      if (!_consolidationPolicy.policy()) {
        if (errorSubField.empty()) {
          errorField = fieldName;
        } else {
          errorField = fieldName + "." + errorSubField;
        }

        return false;
      }
    }
  }
  /* FIXME TODO temporarily disable, eventually used for ordering internal data
    structures
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

  {
    // optional size_t
    static const std::string fieldName("writebufferActive");

    mask->_writebufferActive = slice.hasKey(fieldName);

    if (!mask->_writebufferActive) {
      _writebufferActive = defaults._writebufferActive;
    } else {
      auto field = slice.get(fieldName);

      if (!getNumber(_writebufferActive, field)) {
        errorField = fieldName;

        return false;
      }
    }
  }

  {
    // optional size_t
    static const std::string fieldName("writebufferIdle");

    mask->_writebufferIdle = slice.hasKey(fieldName);

    if (!mask->_writebufferIdle) {
      _writebufferIdle = defaults._writebufferIdle;
    } else {
      auto field = slice.get(fieldName);

      if (!getNumber(_writebufferIdle, field)) {
        errorField = fieldName;

        return false;
      }
    }
  }

  {
    // optional size_t
    static const std::string fieldName("writebufferSizeMax");

    mask->_writebufferSizeMax = slice.hasKey(fieldName);

    if (!mask->_writebufferSizeMax) {
      _writebufferSizeMax = defaults._writebufferSizeMax;
    } else {
      auto field = slice.get(fieldName);

      if (!getNumber(_writebufferSizeMax, field)) {
        errorField = fieldName;

        return false;
      }
    }
  }

  {
    // optional object
    static VPackStringRef const fieldName("primarySort");
    std::string errorSubField;

    auto const field = slice.get(fieldName);
    mask->_primarySort = !field.isNone();

    if (!mask->_primarySort) {
      _primarySort = defaults._primarySort;
    } else if (!_primarySort.fromVelocyPack(field, errorSubField)) {
      errorField = fieldName.toString();
      if (!errorSubField.empty()) {
        errorField += errorSubField;
      }

      return false;
    }
  }

  {
    // optional object
    static VPackStringRef const fieldName("storedValues");
    std::string errorSubField;

    auto const field = slice.get(fieldName);
    mask->_storedValues = !field.isNone();

    if (!mask->_storedValues) {
      _storedValues = defaults._storedValues;
    } else if (!_storedValues.fromVelocyPack(field, errorSubField)) {
      errorField = fieldName.toString();
      if (!errorSubField.empty()) {
        errorField += errorSubField;
      }

      return false;
    }
  }
  {
    // optional string (only if primarySort present)
    static VPackStringRef const fieldName("primarySortCompression");
    auto const field = slice.get(fieldName);
    mask->_primarySortCompression = !field.isNone();
    if (mask->_primarySortCompression) {
      _primarySortCompression = nullptr;
      if (field.isString()) {
        _primarySortCompression = columnCompressionFromString(field.copyString());
      }
      if (ADB_UNLIKELY(nullptr == _primarySortCompression)) {
        errorField += ".primarySortCompression";
        return false;
      }
    }
  }
  return true;
}

bool IResearchViewMeta::json(arangodb::velocypack::Builder& builder,
                             IResearchViewMeta const* ignoreEqual /*= nullptr*/,
                             Mask const* mask /*= nullptr*/) const {
  if (!builder.isOpenObject()) {
    return false;
  }

  if ((!ignoreEqual || _cleanupIntervalStep != ignoreEqual->_cleanupIntervalStep) &&
      (!mask || mask->_cleanupIntervalStep)) {
    builder.add("cleanupIntervalStep", arangodb::velocypack::Value(_cleanupIntervalStep));
  }

  if ((!ignoreEqual || _commitIntervalMsec != ignoreEqual->_commitIntervalMsec) // if requested or different
      && (!mask || mask->_commitIntervalMsec)) {
    builder.add( // add value
      "commitIntervalMsec", arangodb::velocypack::Value(_commitIntervalMsec) // args
    );
  }

  if ((!ignoreEqual || _consolidationIntervalMsec != ignoreEqual->_consolidationIntervalMsec) &&
      (!mask || mask->_consolidationIntervalMsec)) {
    builder.add("consolidationIntervalMsec",
                arangodb::velocypack::Value(_consolidationIntervalMsec));
  }

  if ((!ignoreEqual || !arangodb::basics::VelocyPackHelper::equal(
          _consolidationPolicy.properties(),
          ignoreEqual->_consolidationPolicy.properties(), false)) &&
      (!mask || mask->_consolidationPolicy)) {
    builder.add("consolidationPolicy", _consolidationPolicy.properties());
  }
  /* FIXME TODO temporarily disable, eventually used for ordering internal data
    structures if ((!ignoreEqual || _locale != ignoreEqual->_locale) && (!mask
    || mask->_locale)) { builder.add("locale",
    arangodb::velocypack::Value(irs::locale_utils::name(_locale)));
    }
  */

  if ((!ignoreEqual || _version != ignoreEqual->_version) && (!mask || mask->_version)) {
    builder.add(StaticStrings::VersionField, arangodb::velocypack::Value(_version));
  }

  if ((!ignoreEqual || _writebufferActive != ignoreEqual->_writebufferActive) &&
      (!mask || mask->_writebufferActive)) {
    builder.add("writebufferActive", arangodb::velocypack::Value(_writebufferActive));
  }

  if ((!ignoreEqual || _writebufferIdle != ignoreEqual->_writebufferIdle) &&
      (!mask || mask->_writebufferIdle)) {
    builder.add("writebufferIdle", arangodb::velocypack::Value(_writebufferIdle));
  }

  if ((!ignoreEqual || _writebufferSizeMax != ignoreEqual->_writebufferSizeMax) &&
      (!mask || mask->_writebufferSizeMax)) {
    builder.add("writebufferSizeMax", arangodb::velocypack::Value(_writebufferSizeMax));
  }

  if ((!ignoreEqual || _primarySort != ignoreEqual->_primarySort) && (!mask || mask->_primarySort)) {
    velocypack::ArrayBuilder arrayScope(&builder, "primarySort");
    if (!_primarySort.toVelocyPack(builder)) {
      return false;
    }
  }

  if ((!ignoreEqual || _storedValues != ignoreEqual->_storedValues) && (!mask || mask->_storedValues)) {
    velocypack::ArrayBuilder arrayScope(&builder, "storedValues");
    if (!_storedValues.toVelocyPack(builder)) {
      return false;
    }
  }

  if ((!ignoreEqual || _primarySortCompression != ignoreEqual->_primarySortCompression) && 
      (!mask || mask->_primarySortCompression)) {
    auto compression = columnCompressionToString(_primarySortCompression);
    addStringRef(builder, "primarySortCompression", compression);
  }

  return true;
}

size_t IResearchViewMeta::memory() const {
  auto size = sizeof(IResearchViewMeta);

  return size;
}

IResearchViewMetaState::Mask::Mask(bool mask /*=false*/) noexcept
    : _collections(mask) {}

IResearchViewMetaState::IResearchViewMetaState() = default;

IResearchViewMetaState::IResearchViewMetaState(IResearchViewMetaState const& defaults) {
  *this = defaults;
}

IResearchViewMetaState::IResearchViewMetaState(IResearchViewMetaState&& other) noexcept {
  *this = std::move(other);
}

IResearchViewMetaState& IResearchViewMetaState::operator=(IResearchViewMetaState&& other) noexcept {
  if (this != &other) {
    _collections = std::move(other._collections);
  }

  return *this;
}

IResearchViewMetaState& IResearchViewMetaState::operator=(IResearchViewMetaState const& other) {
  if (this != &other) {
    _collections = other._collections;
  }

  return *this;
}

bool IResearchViewMetaState::operator==(IResearchViewMetaState const& other) const noexcept {
  if (_collections != other._collections) {
    return false;  // values do not match
  }

  return true;
}

bool IResearchViewMetaState::operator!=(IResearchViewMetaState const& other) const noexcept {
  return !(*this == other);
}

/*static*/ const IResearchViewMetaState& IResearchViewMetaState::DEFAULT() {
  static const IResearchViewMetaState meta;

  return meta;
}

bool IResearchViewMetaState::init(arangodb::velocypack::Slice const& slice,
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

      _collections.clear();  // reset to match read values exactly

      for (arangodb::velocypack::ArrayIterator itr(field); itr.valid(); ++itr) {
        decltype(_collections)::key_type value;

        DataSourceId::BaseType tmp;
        if (!getNumber(tmp,
                       itr.value())) {  // [ <collectionId 1> ... <collectionId N> ]
          errorField = fieldName + "[" +
                       arangodb::basics::StringUtils::itoa(itr.index()) + "]";

          return false;
        }
        value = DataSourceId{tmp};

        _collections.emplace(value);
      }
    }
  }

  return true;
}

bool IResearchViewMetaState::json(arangodb::velocypack::Builder& builder,
                                  IResearchViewMetaState const* ignoreEqual /*= nullptr*/,
                                  Mask const* mask /*= nullptr*/
                                  ) const {
  if (!builder.isOpenObject()) {
    return false;
  }

  if ((!ignoreEqual || _collections != ignoreEqual->_collections) &&
      (!mask || mask->_collections)) {
    arangodb::velocypack::Builder subBuilder;

    {
      arangodb::velocypack::ArrayBuilder subBuilderWrapper(&subBuilder);

      for (auto& cid : _collections) {
        subBuilderWrapper->add(arangodb::velocypack::Value(cid.id()));
      }
    }

    builder.add("collections", subBuilder.slice());
  }

  return true;
}

size_t IResearchViewMetaState::memory() const {
  auto size = sizeof(IResearchViewMetaState);

  size += sizeof(DataSourceId) * _collections.size();

  return size;
}

}  // namespace iresearch
}  // namespace arangodb

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------
