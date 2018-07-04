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

bool equalConsolidationPolicies(
  arangodb::iresearch::IResearchViewMeta::CommitMeta::ConsolidationPolicies const& lhs,
  arangodb::iresearch::IResearchViewMeta::CommitMeta::ConsolidationPolicies const& rhs
) noexcept {
  if (lhs.size() != rhs.size()) {
    return false;
  }

  typedef arangodb::iresearch::IResearchViewMeta::CommitMeta::ConsolidationPolicy ConsolidationPolicy;
  struct PtrEquals {
    bool operator()(ConsolidationPolicy const * const& lhs, ConsolidationPolicy const * const& rhs) const {
      return *lhs == *rhs;
    }
  };
  struct PtrHash {
    size_t operator()(ConsolidationPolicy const * const& value) const {
      return ConsolidationPolicy::Hash()(*value);
    }
  };

  std::unordered_multiset<ConsolidationPolicy const *, PtrHash, PtrEquals> expected;

  for (auto& entry: lhs) {
    expected.emplace(&entry);
  }

  for (auto& entry: rhs) {
    auto itr = expected.find(&entry);

    if (itr == expected.end()) {
      return false; // values do not match
    }

    expected.erase(itr); // ensure same count of duplicates
  }

  return true;
}

bool initCommitMeta(
  arangodb::iresearch::IResearchViewMeta::CommitMeta& meta,
  arangodb::velocypack::Slice const& slice,
  std::string& errorField,
  arangodb::iresearch::IResearchViewMeta::CommitMeta const& defaults
) noexcept {
  bool tmpSeen;

  {
    // optional size_t
    static const std::string fieldName("cleanupIntervalStep");

    if (!arangodb::iresearch::getNumber(meta._cleanupIntervalStep, slice, fieldName, tmpSeen, defaults._cleanupIntervalStep)) {
      errorField = fieldName;

      return false;
    }
  }

  {
    // optional size_t
    static const std::string fieldName("commitIntervalMsec");
    bool tmpBool;

    if (!arangodb::iresearch::getNumber(meta._commitIntervalMsec, slice, fieldName, tmpBool, defaults._commitIntervalMsec)) {
      errorField = fieldName;

      return false;
    }
  }

  {
    // optional size_t
    static const std::string fieldName("commitTimeoutMsec");
    bool tmpBool;

    if (!arangodb::iresearch::getNumber(meta._commitTimeoutMsec, slice, fieldName, tmpBool, defaults._commitTimeoutMsec)) {
      errorField = fieldName;

      return false;
    }
  }

  {
    // optional enum->{size_t,float} map
    static const std::string fieldName("consolidate");

    if (!slice.hasKey(fieldName)) {
      meta._consolidationPolicies = defaults._consolidationPolicies;
    } else {
      auto field = slice.get(fieldName);

      if (!field.isObject()) {
        errorField = fieldName;

        return false;
      }

      meta._consolidationPolicies.clear(); // reset to match read values exactly

      for (arangodb::velocypack::ObjectIterator itr(field); itr.valid(); ++itr) {
        auto key = itr.key();

        if (!key.isString()) {
          errorField = fieldName + "=>[" + arangodb::basics::StringUtils::itoa(itr.index()) + "]";

          return false;
        }

        typedef arangodb::iresearch::IResearchViewMeta::CommitMeta::ConsolidationPolicy ConsolidationPolicy;

        static const std::unordered_map<std::string, ConsolidationPolicy::Type> policies = {
          { "bytes", ConsolidationPolicy::Type::BYTES },
          { "bytes_accum", ConsolidationPolicy::Type::BYTES_ACCUM },
          { "count", ConsolidationPolicy::Type::COUNT },
          { "fill", ConsolidationPolicy::Type::FILL },
        };

        auto name = key.copyString();
        auto policyItr = policies.find(name);
        auto value = itr.value();

        if (!value.isObject() || policyItr == policies.end()) {
          errorField = fieldName + "=>" + name;

          return false;
        }

        static const ConsolidationPolicy& defaultPolicy = ConsolidationPolicy::DEFAULT(policyItr->second);
        size_t segmentThreshold = 0;

        {
          // optional size_t
          static const std::string subFieldName("segmentThreshold");

          if (!arangodb::iresearch::getNumber(segmentThreshold, value, subFieldName, tmpSeen, defaultPolicy.segmentThreshold())) {
            errorField = fieldName + "=>" + name + "=>" + subFieldName;

            return false;
          }
        }

        float threshold = std::numeric_limits<float>::infinity();

        {
          // optional float
          static const std::string subFieldName("threshold");

          if (!arangodb::iresearch::getNumber(threshold, value, subFieldName, tmpSeen, defaultPolicy.threshold()) || threshold < 0. || threshold > 1.) {
            errorField = fieldName + "=>" + name + "=>" + subFieldName;

            return false;
          }
        }

        // add only enabled policies
        if (segmentThreshold) {
          meta._consolidationPolicies.emplace_back(policyItr->second, segmentThreshold, threshold);
        }
      }
    }
  }

  return true;
}

bool jsonCommitMeta(
  arangodb::velocypack::Builder& builder,
  arangodb::iresearch::IResearchViewMeta::CommitMeta const& meta
) {
  if (!builder.isOpenObject()) {
    return false;
  }

  builder.add("cleanupIntervalStep", arangodb::velocypack::Value(meta._cleanupIntervalStep));
  builder.add("commitIntervalMsec", arangodb::velocypack::Value(meta._commitIntervalMsec));
  builder.add("commitTimeoutMsec", arangodb::velocypack::Value(meta._commitTimeoutMsec));

  typedef arangodb::iresearch::IResearchViewMeta::CommitMeta::ConsolidationPolicy ConsolidationPolicy;
  struct ConsolidationPolicyHash { size_t operator()(ConsolidationPolicy::Type const& value) const noexcept { return size_t(value); } }; // for GCC compatibility
  static const std::unordered_map<ConsolidationPolicy::Type, std::string, ConsolidationPolicyHash> policies = {
    { ConsolidationPolicy::Type::BYTES, "bytes" },
    { ConsolidationPolicy::Type::BYTES_ACCUM, "bytes_accum" },
    { ConsolidationPolicy::Type::COUNT, "count" },
    { ConsolidationPolicy::Type::FILL, "fill" },
  };

  arangodb::velocypack::Builder subBuilder;

  {
    arangodb::velocypack::ObjectBuilder subBuilderWrapper(&subBuilder);

    for (auto& policy: meta._consolidationPolicies) {
      if (!policy.segmentThreshold()) {
        continue; // do not output disabled consolidation policies
      }

      auto itr = policies.find(policy.type());

      if (itr != policies.end()) {
        arangodb::velocypack::Builder policyBuilder;

        {
          arangodb::velocypack::ObjectBuilder policyBuilderWrapper(&policyBuilder);

          policyBuilderWrapper->add("segmentThreshold", arangodb::velocypack::Value(policy.segmentThreshold()));
          policyBuilderWrapper->add("threshold", arangodb::velocypack::Value(policy.threshold()));
        }

        subBuilderWrapper->add(itr->second, policyBuilder.slice());
      }
    }
  }

  builder.add("consolidate", subBuilder.slice());

  return true;
}

NS_END

NS_BEGIN(arangodb)
NS_BEGIN(iresearch)

size_t IResearchViewMeta::CommitMeta::ConsolidationPolicy::Hash::operator()(
    IResearchViewMeta::CommitMeta::ConsolidationPolicy const& value
) const {
  auto segmentThreshold = value.segmentThreshold();
  auto threshold = value.threshold();
  auto type = value.type();

  return std::hash<decltype(segmentThreshold)>{}(segmentThreshold)
    ^ std::hash<decltype(threshold)>{}(threshold)
    ^ std::hash<size_t>{}(size_t(type))
    ;
}

IResearchViewMeta::CommitMeta::ConsolidationPolicy::ConsolidationPolicy(
    IResearchViewMeta::CommitMeta::ConsolidationPolicy::Type type,
    size_t segmentThreshold,
    float threshold
): _segmentThreshold(segmentThreshold), _threshold(threshold), _type(type) {
  switch (type) {
   case Type::BYTES:
    _policy = irs::index_utils::consolidate_bytes(_threshold);
    break;
   case Type::BYTES_ACCUM:
    _policy = irs::index_utils::consolidate_bytes_accum(_threshold);
    break;
   case Type::COUNT:
    _policy = irs::index_utils::consolidate_count(_threshold);
    break;
   case Type::FILL:
    _policy = irs::index_utils::consolidate_fill(_threshold);
    break;
   default:
    // internal logic error here!!! do not know how to initialize policy
    // should have a case for every declared type
    throw std::runtime_error(std::string("internal error, unsupported consolidation type '") + arangodb::basics::StringUtils::itoa(size_t(_type)) + "'");
  }
}

IResearchViewMeta::CommitMeta::ConsolidationPolicy::ConsolidationPolicy(
    IResearchViewMeta::CommitMeta::ConsolidationPolicy const& other
) {
  *this = other;
}

IResearchViewMeta::CommitMeta::ConsolidationPolicy::ConsolidationPolicy(
    IResearchViewMeta::CommitMeta::ConsolidationPolicy&& other
) noexcept {
  *this = std::move(other);
}

IResearchViewMeta::CommitMeta::ConsolidationPolicy& IResearchViewMeta::CommitMeta::ConsolidationPolicy::operator=(
    IResearchViewMeta::CommitMeta::ConsolidationPolicy const& other
) {
  if (this != &other) {
    _segmentThreshold = other._segmentThreshold;
    _policy = other._policy;
    _threshold = other._threshold;
    _type = other._type;
  }

  return *this;
}

IResearchViewMeta::CommitMeta::ConsolidationPolicy& IResearchViewMeta::CommitMeta::ConsolidationPolicy::operator=(
    IResearchViewMeta::CommitMeta::ConsolidationPolicy&& other
) noexcept {
  if (this != &other) {
    _segmentThreshold = std::move(other._segmentThreshold);
    _policy = std::move(other._policy);
    _threshold = std::move(other._threshold);
    _type = std::move(other._type);
  }

  return *this;
}

bool IResearchViewMeta::CommitMeta::ConsolidationPolicy::operator==(
    IResearchViewMeta::CommitMeta::ConsolidationPolicy const& other
) const noexcept {
  return _type == other._type
    && _segmentThreshold == other._segmentThreshold
    && _threshold == other._threshold
    ;
}

/*static*/ const IResearchViewMeta::CommitMeta::ConsolidationPolicy& IResearchViewMeta::CommitMeta::ConsolidationPolicy::DEFAULT(
    IResearchViewMeta::CommitMeta::ConsolidationPolicy::Type type
) {
  switch (type) {
    case Type::BYTES:
    {
      static const ConsolidationPolicy policy(type, 300, 0.85f);
      return policy;
    }
  case Type::BYTES_ACCUM:
    {
      static const ConsolidationPolicy policy(type, 300, 0.85f);
      return policy;
    }
  case Type::COUNT:
    {
      static const ConsolidationPolicy policy(type, 300, 0.85f);
      return policy;
    }
  case Type::FILL:
    {
      static const ConsolidationPolicy policy(type, 300, 0.85f);
      return policy;
    }
  default:
    // internal logic error here!!! do not know how to initialize policy
    // should have a case for every declared type
    throw std::runtime_error(std::string("internal error, unsupported consolidation type '") + arangodb::basics::StringUtils::itoa(size_t(type)) + "'");
  }
}

size_t IResearchViewMeta::CommitMeta::ConsolidationPolicy::segmentThreshold() const noexcept {
  return _segmentThreshold;
}

irs::index_writer::consolidation_policy_t const& IResearchViewMeta::CommitMeta::ConsolidationPolicy::policy() const noexcept {
  return _policy;
}

float IResearchViewMeta::CommitMeta::ConsolidationPolicy::threshold() const noexcept {
  return _threshold;
}

IResearchViewMeta::CommitMeta::ConsolidationPolicy::Type IResearchViewMeta::CommitMeta::ConsolidationPolicy::type() const noexcept {
  return _type;
}

bool IResearchViewMeta::CommitMeta::operator==(
  CommitMeta const& other
) const noexcept {
  return _cleanupIntervalStep == other._cleanupIntervalStep
      && _commitIntervalMsec == other._commitIntervalMsec
      && _commitTimeoutMsec == other._commitTimeoutMsec
      && equalConsolidationPolicies(_consolidationPolicies, other._consolidationPolicies);
}

bool IResearchViewMeta::CommitMeta::operator!=(
  CommitMeta const& other
  ) const noexcept {
  return !(*this == other);
}

IResearchViewMeta::Mask::Mask(bool mask /*=false*/) noexcept
  : _commit(mask),
    _locale(mask) {
}

IResearchViewMeta::IResearchViewMeta()
  : _locale(std::locale::classic()) {
  _commit._cleanupIntervalStep = 10;
  _commit._commitIntervalMsec = 60 * 1000;
  _commit._commitTimeoutMsec = 5 * 1000;
  _commit._consolidationPolicies.emplace_back(CommitMeta::ConsolidationPolicy::DEFAULT(CommitMeta::ConsolidationPolicy::Type::BYTES));
  _commit._consolidationPolicies.emplace_back(CommitMeta::ConsolidationPolicy::DEFAULT(CommitMeta::ConsolidationPolicy::Type::BYTES_ACCUM));
  _commit._consolidationPolicies.emplace_back(CommitMeta::ConsolidationPolicy::DEFAULT(CommitMeta::ConsolidationPolicy::Type::COUNT));
  _commit._consolidationPolicies.emplace_back(CommitMeta::ConsolidationPolicy::DEFAULT(CommitMeta::ConsolidationPolicy::Type::FILL));
}

IResearchViewMeta::IResearchViewMeta(IResearchViewMeta const& defaults) {
  *this = defaults;
}

IResearchViewMeta::IResearchViewMeta(IResearchViewMeta&& other) noexcept {
  *this = std::move(other);
}

IResearchViewMeta& IResearchViewMeta::operator=(IResearchViewMeta&& other) noexcept {
  if (this != &other) {
    _commit = std::move(other._commit);
    _locale = std::move(other._locale);
  }

  return *this;
}

IResearchViewMeta& IResearchViewMeta::operator=(IResearchViewMeta const& other) {
  if (this != &other) {
    _commit = other._commit;
    _locale = other._locale;
  }

  return *this;
}

bool IResearchViewMeta::operator==(IResearchViewMeta const& other) const noexcept {
  if (_commit != other._commit) {
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
    errorField = "not an object";
    return false;
  }

  Mask tmpMask;

  if (!mask) {
    mask = &tmpMask;
  }

  {
    // optional jSON object
    static const std::string fieldName("commit");

    mask->_commit = slice.hasKey(fieldName);

    if (!mask->_commit) {
      _commit = defaults._commit;
    } else {
      auto field = slice.get(fieldName);

      if (!field.isObject()) {
        errorField = fieldName;

        return false;
      }

      std::string errorSubField;

      if (!initCommitMeta(_commit, field, errorSubField, defaults._commit)) {
        errorField = fieldName + "=>" + errorSubField;

        return false;
      }
    }
  }

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
          ? std::locale::classic()
          : irs::locale_utils::locale(locale, true);
      } catch(...) {
        errorField = fieldName;

        return false;
      }
    }
  }

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

  if ((!ignoreEqual || _commit != ignoreEqual->_commit) && (!mask || mask->_commit)) {
    arangodb::velocypack::Builder subBuilder;

    {
      arangodb::velocypack::ObjectBuilder subBuilderWrapper(&subBuilder);

      if (!jsonCommitMeta(*(subBuilderWrapper.builder), _commit)) {
        return false;
      }
    }

    builder.add("commit", subBuilder.slice());
  }

  if ((!ignoreEqual || _locale != ignoreEqual->_locale) && (!mask || mask->_locale)) {
    builder.add("locale", arangodb::velocypack::Value(irs::locale_utils::name(_locale)));
  }

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
) noexcept {
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