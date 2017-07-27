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
    irs::flags _features; // cached analyzer features
    std::string _name;
    std::string _properties;
    uint64_t _refCount; // number of references held to this pool across reboots
    TRI_voc_rid_t _rid; // the revision id of the persisted configuration for this pool
    std::string _type;

    AnalyzerPool(irs::string_ref const& name);
    bool init(irs::string_ref const& type, irs::string_ref const& properties) noexcept;
  };

  IResearchAnalyzerFeature(application_features::ApplicationServer* server);

  std::pair<AnalyzerPool::ptr, bool> emplace(
    irs::string_ref const& name,
    irs::string_ref const& type,
    irs::string_ref const& properties
  ) noexcept;
  size_t erase(irs::string_ref const& name, bool force = false) noexcept;
  AnalyzerPool::ptr get(irs::string_ref const& name) const noexcept;
  static AnalyzerPool::ptr identity() noexcept; // the identity analyzer
  static std::string const& name();
  void prepare() override;
  bool release(AnalyzerPool::ptr const& pool) noexcept; // release a persistent registration for a specific pool
  bool reserve(AnalyzerPool::ptr const& pool) noexcept; // register a persistent user for a specific pool
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
    bool persist
  ) noexcept;
  void loadConfiguration();
  bool storeConfiguration(AnalyzerPool const& pool);
};

NS_END // iresearch
NS_END // arangodb

#endif