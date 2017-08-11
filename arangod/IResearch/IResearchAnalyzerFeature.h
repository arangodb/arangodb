////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2017 ArangoDB GmbH, Cologne, Germany
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

#ifndef ARANGOD_IRESEARCH__IRESEARCH_ANALYZER_FEATURE_H
#define ARANGOD_IRESEARCH__IRESEARCH_ANALYZER_FEATURE_H 1

#include "analysis/analyzer.hpp"
#include "utils/async_utils.hpp"
#include "utils/hash_utils.hpp"
#include "utils/object_pool.hpp"

#include "VocBase/voc-types.h"

#include "ApplicationFeatures/ApplicationFeature.h"

NS_BEGIN(arangodb)
NS_BEGIN(transaction)

class Methods; // forward declaration

NS_END // transaction
NS_END // arangodb

NS_BEGIN(arangodb)
NS_BEGIN(iresearch)

////////////////////////////////////////////////////////////////////////////////
/// @brief a cache of IResearch analyzer instances
///        and a provider of AQL TOKENS(<data>, <analyzer>) function
///        NOTE: deallocation of an IResearchAnalyzerFeature instance
///              invalidates all AnalyzerPool instances previously provided by
///              the deallocated feature instance
////////////////////////////////////////////////////////////////////////////////
class IResearchAnalyzerFeature final: public arangodb::application_features::ApplicationFeature {
 public:
  // thread-safe analyzer pool
  class AnalyzerPool: private irs::util::noncopyable {
   public:
    DECLARE_SPTR(AnalyzerPool);
    irs::flags const& features() const noexcept;
    irs::analysis::analyzer::ptr get() const noexcept; // nullptr == error creating analyzer
    std::string const& name() const noexcept;

   private:
    friend IResearchAnalyzerFeature; // required for calling AnalyzerPool::init()

    // 'make(...)' method wrapper for irs::analysis::analyzer types
    struct Builder {
      typedef irs::analysis::analyzer::ptr ptr;
      DECLARE_FACTORY_DEFAULT(irs::string_ref const& type, irs::string_ref const& properties);
    };

    mutable irs::unbounded_object_pool<Builder> _cache; // cache of irs::analysis::analyzer (constructed via AnalyzerBuilder::make(...))
    std::string _config; // non-null type + non-null properties + key
    irs::flags _features; // cached analyzer features
    irs::string_ref _key; // the key of the persisted configuration for this pool, null == not persisted
    std::string _name; // ArangoDB alias for an IResearch analyzer configuration
    irs::string_ref _properties; // IResearch analyzer configuration
    uint64_t _refCount; // number of references held to this pool across reboots
    irs::string_ref _type; // IResearch analyzer name

    AnalyzerPool(irs::string_ref const& name);
    bool init(irs::string_ref const& type, irs::string_ref const& properties);
    void setKey(irs::string_ref const& type);
  };

  IResearchAnalyzerFeature(application_features::ApplicationServer* server);

  std::pair<AnalyzerPool::ptr, bool> emplace(
    irs::string_ref const& name,
    irs::string_ref const& type,
    irs::string_ref const& properties
  ) noexcept;
  AnalyzerPool::ptr ensure(irs::string_ref const& name); // before start() returns pool placeholder, during start() all placeholders are initialized, after start() returns same as get(...)
  size_t erase(irs::string_ref const& name, bool force = false) noexcept;
  AnalyzerPool::ptr get(irs::string_ref const& name) const noexcept;
  static AnalyzerPool::ptr identity() noexcept; // the identity analyzer
  static std::string const& name() noexcept;
  void prepare() override;
  bool release(irs::string_ref const& name); // release a persistent registration for a specific pool
  bool reserve(irs::string_ref const& name); // register a persistent user for a specific pool
  void start() override;
  void stop() override;
  bool visit(std::function<bool(irs::string_ref const& name, irs::string_ref const& type, irs::string_ref const& properties)> const& visitor);

 private:
  // map of caches of irs::analysis::analyzer pools indexed by analyzer name and their associated metas
  std::unordered_map<irs::hashed_string_ref, AnalyzerPool::ptr> _analyzers;
  mutable irs::async_utils::read_write_mutex _mutex;
  bool _started;

  std::pair<AnalyzerPool::ptr, bool> emplace(
    irs::string_ref const& name,
    irs::string_ref const& type,
    irs::string_ref const& properties,
    bool initAndPersist
  ) noexcept;
  void loadConfiguration(
    std::unordered_set<irs::string_ref> const& preinitialized
  );
  bool storeConfiguration(AnalyzerPool& pool);
  bool updateConfiguration(AnalyzerPool& pool, int64_t delta);
  bool updateConfiguration(
    arangodb::transaction::Methods& trx,
    AnalyzerPool& pool,
    int64_t delta
  );
};

NS_END // iresearch
NS_END // arangodb

#endif