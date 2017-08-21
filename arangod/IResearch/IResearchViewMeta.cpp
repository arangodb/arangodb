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

#include "search/scorers.hpp"
#include "utils/index_utils.hpp"
#include "utils/locale_utils.hpp"

#include "VelocyPackHelper.h"
#include "Basics/StringUtils.h"
#include "velocypack/Builder.h"
#include "velocypack/Iterator.h"

#include "IResearchViewMeta.h"

NS_LOCAL

////////////////////////////////////////////////////////////////////////////////
/// @brief functrs for initializing scorers
////////////////////////////////////////////////////////////////////////////////
struct ScorerMeta {
  // @return success
  typedef std::function<bool(irs::flags& flags)> fnFeatures_f;
  typedef irs::iql::order_function fnScorer_t;
  typedef fnScorer_t::contextual_function_t fnScorer_f;
  typedef fnScorer_t::contextual_function_args_t fnScoreArgs_t;
  bool _isDefault;
  irs::flags _features;
  fnScorer_t const& _scorer;
  ScorerMeta(
    irs::flags const& features, fnScorer_t const& scorer, bool isDefault = false
  ) : _isDefault(isDefault), _features(features), _scorer(scorer) {}
};

std::unordered_multimap<std::string, ScorerMeta> const& allKnownScorers() {
  static const struct AllScorers {
    std::unordered_multimap<std::string, ScorerMeta> _scorers;
    AllScorers() {
//      auto visitor = [this](
//        std::string const& name,
//        iresearch::flags const& features,
//        iresearch::iql::order_function const& builder,
//        bool isDefault
//        )->bool {
//        _scorers.emplace(name, ScorerMeta(features, builder, isDefault));
//        return true;
//      };
      static ScorerMeta::fnScorer_f FIXME_SCORER = [](
        iresearch::order& order,
        const std::locale&,
        void* cookie,
        bool ascending,
        const ScorerMeta::fnScoreArgs_t& args
        )->bool {
        return false;
      };

      irs::scorers::visit([this](const irs::string_ref& name)->bool{
        _scorers.emplace(name, ScorerMeta(irs::flags::empty_instance(), FIXME_SCORER));
        return true;
      });
      //iResearchDocumentAdapter::visitScorers(visitor); FIXME TODO
    }
  } KNOWN_SCORERS;

  return KNOWN_SCORERS._scorers;
}

bool equalConsolidationPolicies(
  arangodb::iresearch::IResearchViewMeta::CommitBaseMeta::ConsolidationPolicies const& lhs,
  arangodb::iresearch::IResearchViewMeta::CommitBaseMeta::ConsolidationPolicies const& rhs
) noexcept {
  if (lhs.size() != rhs.size()) {
    return false;
  }

  typedef arangodb::iresearch::IResearchViewMeta::CommitBaseMeta::ConsolidationPolicy ConsolidationPolicy;
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

bool initCommitBaseMeta(
  arangodb::iresearch::IResearchViewMeta::CommitBaseMeta& meta,
  arangodb::velocypack::Slice const& slice,
  std::string& errorField,
  arangodb::iresearch::IResearchViewMeta::CommitBaseMeta const& defaults
) noexcept {
  bool tmpSeen;

  {
    // optional size_t
    static const std::string fieldName("cleanupIntervalStep");

    if (!arangodb::iresearch::getNumber(meta._cleanupIntervalStep, slice, fieldName, tmpSeen, meta._cleanupIntervalStep)) {
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

        typedef arangodb::iresearch::IResearchViewMeta::CommitBaseMeta::ConsolidationPolicy ConsolidationPolicy;

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
        size_t intervalStep = 0;

        {
          // optional size_t
          static const std::string subFieldName("intervalStep");

          if (!arangodb::iresearch::getNumber(intervalStep, value, subFieldName, tmpSeen, defaultPolicy.intervalStep())) {
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
        if (intervalStep) {
          meta._consolidationPolicies.emplace_back(policyItr->second, intervalStep, threshold);
        }
      }
    }
  }

  return true;
}

bool jsonCommitBaseMeta(
  arangodb::velocypack::Builder& builder,
  arangodb::iresearch::IResearchViewMeta::CommitBaseMeta const& meta
) {
  if (!builder.isOpenObject()) {
    return false;
  }

  builder.add("cleanupIntervalStep", arangodb::velocypack::Value(meta._cleanupIntervalStep));

  typedef arangodb::iresearch::IResearchViewMeta::CommitBaseMeta::ConsolidationPolicy ConsolidationPolicy;
  struct ConsolidationPolicyHash { size_t operator()(ConsolidationPolicy::Type const& value) const { return size_t(value); } }; // for GCC compatibility
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
      if (!policy.intervalStep()) {
        continue; // do not output disabled consolidation policies
      }

      auto itr = policies.find(policy.type());

      if (itr != policies.end()) {
        arangodb::velocypack::Builder policyBuilder;

        {
          arangodb::velocypack::ObjectBuilder policyBuilderWrapper(&policyBuilder);

          policyBuilderWrapper->add("intervalStep", arangodb::velocypack::Value(policy.intervalStep()));
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

size_t IResearchViewMeta::CommitBaseMeta::ConsolidationPolicy::Hash::operator()(
    IResearchViewMeta::CommitBaseMeta::ConsolidationPolicy const& value
) const {
  auto step = value.intervalStep();
  auto threshold = value.threshold();
  auto type = value.type();

  return std::hash<decltype(step)>{}(step)
    ^ std::hash<decltype(threshold)>{}(threshold)
    ^ std::hash<size_t>{}(size_t(type))
    ;
}

IResearchViewMeta::CommitBaseMeta::ConsolidationPolicy::ConsolidationPolicy(
    IResearchViewMeta::CommitBaseMeta::ConsolidationPolicy::Type type,
    size_t intervalStep,
    float threshold
): _intervalStep(intervalStep), _threshold(threshold), _type(type) {
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

IResearchViewMeta::CommitBaseMeta::ConsolidationPolicy::ConsolidationPolicy(
    IResearchViewMeta::CommitBaseMeta::ConsolidationPolicy const& other
) {
  *this = other;
}

IResearchViewMeta::CommitBaseMeta::ConsolidationPolicy::ConsolidationPolicy(
    IResearchViewMeta::CommitBaseMeta::ConsolidationPolicy&& other
) noexcept {
  *this = std::move(other);
}

IResearchViewMeta::CommitBaseMeta::ConsolidationPolicy& IResearchViewMeta::CommitBaseMeta::ConsolidationPolicy::operator=(
    IResearchViewMeta::CommitBaseMeta::ConsolidationPolicy const& other
) {
  if (this != &other) {
    _intervalStep = other._intervalStep;
    _policy = other._policy;
    _threshold = other._threshold;
    _type = other._type;
  }

  return *this;
}

IResearchViewMeta::CommitBaseMeta::ConsolidationPolicy& IResearchViewMeta::CommitBaseMeta::ConsolidationPolicy::operator=(
    IResearchViewMeta::CommitBaseMeta::ConsolidationPolicy&& other
) noexcept {
  if (this != &other) {
    _intervalStep = std::move(other._intervalStep);
    _policy = std::move(other._policy);
    _threshold = std::move(other._threshold);
    _type = std::move(other._type);
  }

  return *this;
}

bool IResearchViewMeta::CommitBaseMeta::ConsolidationPolicy::operator==(
    IResearchViewMeta::CommitBaseMeta::ConsolidationPolicy const& other
) const noexcept {
  return _type == other._type
    && _intervalStep == other._intervalStep
    && _threshold == other._threshold
    ;
}

/*static*/ const IResearchViewMeta::CommitBaseMeta::ConsolidationPolicy& IResearchViewMeta::CommitBaseMeta::ConsolidationPolicy::DEFAULT(
    IResearchViewMeta::CommitBaseMeta::ConsolidationPolicy::Type type
) {
  switch (type) {
    case Type::BYTES:
    {
      static const ConsolidationPolicy policy(type, 10, 0.85f);
      return policy;
    }
  case Type::BYTES_ACCUM:
    {
      static const ConsolidationPolicy policy(type, 10, 0.85f);
      return policy;
    }
  case Type::COUNT:
    {
      static const ConsolidationPolicy policy(type, 10, 0.85f);
      return policy;
    }
  case Type::FILL:
    {
      static const ConsolidationPolicy policy(type, 10, 0.85f);
      return policy;
    }
  default:
    // internal logic error here!!! do not know how to initialize policy
    // should have a case for every declared type
    throw std::runtime_error(std::string("internal error, unsupported consolidation type '") + arangodb::basics::StringUtils::itoa(size_t(type)) + "'");
  }
}

size_t IResearchViewMeta::CommitBaseMeta::ConsolidationPolicy::intervalStep() const noexcept {
  return _intervalStep;
}

irs::index_writer::consolidation_policy_t const& IResearchViewMeta::CommitBaseMeta::ConsolidationPolicy::policy() const noexcept {
  return _policy;
}

float IResearchViewMeta::CommitBaseMeta::ConsolidationPolicy::threshold() const noexcept {
  return _threshold;
}

IResearchViewMeta::CommitBaseMeta::ConsolidationPolicy::Type IResearchViewMeta::CommitBaseMeta::ConsolidationPolicy::type() const noexcept {
  return _type;
}

bool IResearchViewMeta::CommitBaseMeta::operator==(
  CommitBaseMeta const& other
) const noexcept {
  if (_cleanupIntervalStep != other._cleanupIntervalStep) {
    return false; // values do not match
  }

  if (!equalConsolidationPolicies(_consolidationPolicies, other._consolidationPolicies)) {
    return false; // values do not match
  }

  return true;
}

bool IResearchViewMeta::CommitBulkMeta::operator==(
  CommitBulkMeta const& other
) const noexcept {
  return _commitIntervalBatchSize == other._commitIntervalBatchSize
    && *static_cast<CommitBaseMeta const*>(this) == other;
}

bool IResearchViewMeta::CommitBulkMeta::operator!=(
  CommitBulkMeta const& other
) const noexcept {
  return !(*this == other);
}

bool IResearchViewMeta::CommitItemMeta::operator==(
  CommitItemMeta const& other
) const noexcept {
  return _commitIntervalMsec == other._commitIntervalMsec
    && _commitTimeoutMsec == other._commitTimeoutMsec
    && *static_cast<CommitBaseMeta const*>(this) == other;
}

bool IResearchViewMeta::CommitItemMeta::operator!=(
  CommitItemMeta const& other
  ) const noexcept {
  return !(*this == other);
}

IResearchViewMeta::Mask::Mask(bool mask /*=false*/) noexcept
  : _collections(mask),
    _commitBulk(mask),
    _commitItem(mask),
    _dataPath(mask),
    _locale(mask),
    _scorers(mask),
    _threadsMaxIdle(mask),
    _threadsMaxTotal(mask) {
}

IResearchViewMeta::IResearchViewMeta()
  : _dataPath(""),
    _locale(std::locale::classic()),
    _threadsMaxIdle(5),
    _threadsMaxTotal(5) {
  _commitBulk._cleanupIntervalStep = 10;
  _commitBulk._commitIntervalBatchSize = 10000;
  _commitBulk._consolidationPolicies.emplace_back(CommitBaseMeta::ConsolidationPolicy::DEFAULT(CommitBaseMeta::ConsolidationPolicy::Type::BYTES));
  _commitBulk._consolidationPolicies.emplace_back(CommitBaseMeta::ConsolidationPolicy::DEFAULT(CommitBaseMeta::ConsolidationPolicy::Type::BYTES_ACCUM));
  _commitBulk._consolidationPolicies.emplace_back(CommitBaseMeta::ConsolidationPolicy::DEFAULT(CommitBaseMeta::ConsolidationPolicy::Type::COUNT));
  _commitBulk._consolidationPolicies.emplace_back(CommitBaseMeta::ConsolidationPolicy::DEFAULT(CommitBaseMeta::ConsolidationPolicy::Type::FILL));
  _commitItem._cleanupIntervalStep = 10;
  _commitItem._commitIntervalMsec = 60 * 1000;
  _commitItem._commitTimeoutMsec = 5 * 1000;
  _commitItem._consolidationPolicies.emplace_back(CommitBaseMeta::ConsolidationPolicy::DEFAULT(CommitBaseMeta::ConsolidationPolicy::Type::BYTES));
  _commitItem._consolidationPolicies.emplace_back(CommitBaseMeta::ConsolidationPolicy::DEFAULT(CommitBaseMeta::ConsolidationPolicy::Type::BYTES_ACCUM));
  _commitItem._consolidationPolicies.emplace_back(CommitBaseMeta::ConsolidationPolicy::DEFAULT(CommitBaseMeta::ConsolidationPolicy::Type::COUNT));
  _commitItem._consolidationPolicies.emplace_back(CommitBaseMeta::ConsolidationPolicy::DEFAULT(CommitBaseMeta::ConsolidationPolicy::Type::FILL));

  for (auto& scorer: allKnownScorers()) {
    if (scorer.second._isDefault) {
      _features |= scorer.second._features;
      _scorers.emplace(scorer.first, scorer.second._scorer);
    }
  }
}

IResearchViewMeta::IResearchViewMeta(IResearchViewMeta const& defaults) {
  *this = defaults;
}

IResearchViewMeta::IResearchViewMeta(IResearchViewMeta&& other) noexcept {
  *this = std::move(other);
}

IResearchViewMeta& IResearchViewMeta::operator=(IResearchViewMeta&& other) noexcept {
  if (this != &other) {
    _collections = std::move(other._collections);
    _commitBulk = std::move(other._commitBulk);
    _commitItem = std::move(other._commitItem);
    _dataPath = std::move(other._dataPath);
    _features = std::move(other._features);
    _locale = std::move(other._locale);
    _scorers = std::move(other._scorers);
    _threadsMaxIdle = std::move(other._threadsMaxIdle);
    _threadsMaxTotal = std::move(other._threadsMaxTotal);
  }

  return *this;
}

IResearchViewMeta& IResearchViewMeta::operator=(IResearchViewMeta const& other) {
  if (this != &other) {
    _collections = other._collections;
    _commitBulk = other._commitBulk;
    _commitItem = other._commitItem;
    _dataPath = other._dataPath;
    _features = other._features;
    _locale = other._locale;
    _scorers = other._scorers;
    _threadsMaxIdle = other._threadsMaxIdle;
    _threadsMaxTotal = other._threadsMaxTotal;
  }

  return *this;
}

bool IResearchViewMeta::operator==(IResearchViewMeta const& other) const noexcept {
  if (_collections != other._collections) {
    return false; // values do not match
  }

  if (_commitBulk != other._commitBulk) {
    return false; // values do not match
  }

  if (_commitItem != other._commitItem) {
    return false; // values do not match
  }

  if (_dataPath != other._dataPath) {
    return false; // values do not match
  }

  if (_features != other._features) {
    return false; // values do not match
  }

  if (irs::locale_utils::language(_locale) != irs::locale_utils::language(other._locale)
      || irs::locale_utils::country(_locale) != irs::locale_utils::country(other._locale)
      || irs::locale_utils::encoding(_locale) != irs::locale_utils::encoding(other._locale)) {
    return false; // values do not match
  }

  if (_scorers != other._scorers) {
    return false; // values do not match
  }

  if (_threadsMaxIdle != other._threadsMaxIdle) {
    return false; // values do not match
  }

  if (_threadsMaxTotal != other._threadsMaxTotal) {
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

  {
    // optional jSON object
    static const std::string fieldName("commitBulk");

    mask->_commitBulk = slice.hasKey(fieldName);

    if (!mask->_commitBulk) {
      _commitBulk = defaults._commitBulk;
    } else {
      auto field = slice.get(fieldName);

      if (!field.isObject()) {
        errorField = fieldName;

        return false;
      }

      {
        // optional size_t
        static const std::string subFieldName("commitIntervalBatchSize");
        bool tmpBool;

        if (!getNumber(_commitBulk._commitIntervalBatchSize, field, subFieldName, tmpBool, defaults._commitBulk._commitIntervalBatchSize)) {
          errorField = fieldName + "=>" + subFieldName;

          return false;
        }
      }

      std::string errorSubField;

      if (!initCommitBaseMeta(_commitBulk, field, errorSubField, defaults._commitBulk)) {
        errorField = fieldName + "=>" + errorSubField;

        return false;
      }
    }
  }

  {
    // optional jSON object
    static const std::string fieldName("commitItem");

    mask->_commitItem = slice.hasKey(fieldName);

    if (!mask->_commitItem) {
      _commitItem = defaults._commitItem;
    } else {
      auto field = slice.get(fieldName);

      if (!field.isObject()) {
        errorField = fieldName;

        return false;
      }

      {
        // optional size_t
        static const std::string subFieldName("commitIntervalMsec");
        bool tmpBool;

        if (!getNumber(_commitItem._commitIntervalMsec, field, subFieldName, tmpBool, defaults._commitItem._commitIntervalMsec)) {
          errorField = fieldName + "=>" + subFieldName;

          return false;
        }
      }

      {
        // optional size_t
        static const std::string subFieldName("commitTimeoutMsec");
        bool tmpBool;

        if (!getNumber(_commitItem._commitTimeoutMsec, field, subFieldName, tmpBool, defaults._commitItem._commitTimeoutMsec)) {
          errorField = fieldName + "=>" + subFieldName;

          return false;
        }
      }

      std::string errorSubField;

      if (!initCommitBaseMeta(_commitItem, field, errorSubField, defaults._commitItem)) {
        errorField = fieldName + "=>" + errorSubField;

        return false;
      }
    }
  }

  {
    // optional string
    static const std::string fieldName("dataPath");

    if (!getString(_dataPath, slice, fieldName, mask->_dataPath, defaults._dataPath)) {
      errorField = fieldName;

      return false;
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

      // use UTF-8 encoding since that is what JSON objects use
      _locale = std::locale::classic().name() == locale
        ? std::locale::classic()
        : irs::locale_utils::locale(locale, true);
    }
  }

  {
    // optional string list
    static const std::string fieldName("scorers");

    mask->_scorers = slice.hasKey(fieldName);

    if (!mask->_scorers) {
      _features = defaults._features; // add features from default scorers
      _scorers = defaults._scorers; // always add default scorers
    } else {
      auto field = slice.get(fieldName);

      if (!field.isArray()) { // [ <scorerName 1> ... <scorerName N> ]
        errorField = fieldName;

        return false;
      }

      for (arangodb::velocypack::ArrayIterator itr(field); itr.valid(); ++itr) {
        auto entry = itr.value();

        if (!entry.isString()) {
          errorField = fieldName + "=>[" + arangodb::basics::StringUtils::itoa(itr.index()) + "]";

          return false;
        }

        auto name = entry.copyString();

        if (_scorers.find(name) != _scorers.end()) {
          continue; // do not insert duplicates
        }

        auto& knownScorers = allKnownScorers();
        auto knownScorersItr = knownScorers.equal_range(name);

        if (knownScorersItr.first == knownScorersItr.second) {
          errorField = fieldName + "=>" + name;

          return false; // unknown scorer
        }

        _features.clear(); // reset to match read values exactly
        _scorers.clear(); // reset to match read values exactly

        // ensure default scorers and their features are always present
        for (auto& scorer: knownScorers) {
          if (scorer.second._isDefault) {
            _features |= scorer.second._features;
            _scorers.emplace(scorer.first, scorer.second._scorer);
          }
        }

        for (auto scorerItr = knownScorersItr.first; scorerItr != knownScorersItr.second; ++scorerItr) {
          _features |= scorerItr->second._features;
          _scorers.emplace(scorerItr->first, scorerItr->second._scorer);
        }
      }
    }
  }

  {
    // optional size_t
    static const std::string fieldName("threadsMaxIdle");

    if (!getNumber(_threadsMaxIdle, slice, fieldName, mask->_threadsMaxIdle, defaults._threadsMaxIdle)) {
      errorField = fieldName;

      return false;
    }
  }

  {
    // optional size_t
    static const std::string fieldName("threadsMaxTotal");

    if (!getNumber(_threadsMaxTotal, slice, fieldName, mask->_threadsMaxTotal, defaults._threadsMaxTotal) || !_threadsMaxTotal) {
      errorField = fieldName;

      return false;
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

  if ((!ignoreEqual || _commitBulk != ignoreEqual->_commitBulk) && (!mask || mask->_commitBulk)) {
    arangodb::velocypack::Builder subBuilder;

    {
      arangodb::velocypack::ObjectBuilder subBuilderWrapper(&subBuilder);

      subBuilderWrapper->add("commitIntervalBatchSize", arangodb::velocypack::Value(_commitBulk._commitIntervalBatchSize));

      if (!jsonCommitBaseMeta(*(subBuilderWrapper.builder), _commitBulk)) {
        return false;
      }
    }

    builder.add("commitBulk", subBuilder.slice());
  }

  if ((!ignoreEqual || _commitItem != ignoreEqual->_commitItem) && (!mask || mask->_commitItem)) {
    arangodb::velocypack::Builder subBuilder;

    {
      arangodb::velocypack::ObjectBuilder subBuilderWrapper(&subBuilder);

      subBuilderWrapper->add("commitIntervalMsec", arangodb::velocypack::Value(_commitItem._commitIntervalMsec));
      subBuilderWrapper->add("commitTimeoutMsec", arangodb::velocypack::Value(_commitItem._commitTimeoutMsec));

      if (!jsonCommitBaseMeta(*(subBuilderWrapper.builder), _commitItem)) {
        return false;
      }
    }

    builder.add("commitItem", subBuilder.slice());
  }

  if ((!ignoreEqual || _dataPath != ignoreEqual->_dataPath) && (!mask || mask->_dataPath) && !_dataPath.empty()) {
    builder.add("dataPath", arangodb::velocypack::Value(_dataPath));
  }

  if ((!ignoreEqual || _locale != ignoreEqual->_locale) && (!mask || mask->_locale)) {
    builder.add("locale", arangodb::velocypack::Value(irs::locale_utils::name(_locale)));
  }

  if ((!ignoreEqual || _scorers != ignoreEqual->_scorers) && (!mask || mask->_scorers)) {
    arangodb::velocypack::Builder subBuilder;

    {
      arangodb::velocypack::ArrayBuilder subBuilderWrapper(&subBuilder);

      for (auto& scorer: _scorers) {
        subBuilderWrapper->add(arangodb::velocypack::Value(scorer.first));
      }
    }

    builder.add("scorers", subBuilder.slice());
  }

  if ((!ignoreEqual || _threadsMaxIdle != ignoreEqual->_threadsMaxIdle) && (!mask || mask->_threadsMaxIdle)) {
    builder.add("threadsMaxIdle", arangodb::velocypack::Value(_threadsMaxIdle));
  }

  if ((!ignoreEqual || _threadsMaxTotal != ignoreEqual->_threadsMaxTotal) && (!mask || mask->_threadsMaxTotal)) {
    builder.add("threadsMaxTotal", arangodb::velocypack::Value(_threadsMaxTotal));
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

  size += sizeof(TRI_voc_cid_t) * _collections.size();
  size += _dataPath.size();
  size += sizeof(irs::flags::type_map::key_type) * _features.size();

  for (auto& scorer: _scorers) {
    size += scorer.first.length() + sizeof(scorer.second);
  }

  return size;
}

NS_END // iresearch
NS_END // arangodb

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------