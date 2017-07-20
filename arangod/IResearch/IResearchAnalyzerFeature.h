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

#include "analysis/analyzers.hpp"
#include "utils/async_utils.hpp"
#include "utils/hash_utils.hpp"
#include "utils/object_pool.hpp"

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
  class AnalyzerPool {
   public:
    explicit operator bool() const noexcept;
    irs::flags const& features() const noexcept;
    irs::analysis::analyzer::ptr get() const noexcept; // nullptr == error creating analyzer

   private:
    friend IResearchAnalyzerFeature;

    // 'make(...)' method wrapper for irs::analysis::analyzer types
    struct Builder {
      typedef irs::analysis::analyzer::ptr ptr;
      DECLARE_FACTORY_DEFAULT(irs::string_ref const& type, irs::string_ref const& properties);
    };
    struct Meta {
      irs::flags _features; // cached analyzer features
      std::string _properties;
      std::string _type;
    };
    typedef irs::unbounded_object_pool<Builder> Cache;

    Meta const& _meta;
    mutable std::shared_ptr<Cache> _pool; // cache of irs::analysis::analyzer (constructed via AnalyzerBuilder::make(...))

    AnalyzerPool();
    AnalyzerPool(Meta const& meta, std::shared_ptr<Cache> const& pool);
    static Meta const& emptyMeta() noexcept;
  };

  IResearchAnalyzerFeature(application_features::ApplicationServer* server);

  AnalyzerPool emplace(
    irs::string_ref const& name,
    irs::string_ref const& type,
    irs::string_ref const& properties
  ) noexcept;
  AnalyzerPool get(irs::string_ref const& name) const noexcept;
  void prepare() override;
  size_t remove(irs::string_ref const& name) noexcept;
  void start() override;
  void stop() override;
  bool visit(std::function<bool(irs::string_ref const& name, irs::string_ref const& type, irs::string_ref const& properties)> const& visitor);

 private:
  struct AnalyzerEntry {
    AnalyzerPool::Meta _meta;
    mutable std::mutex _mutex;
    mutable std::weak_ptr<AnalyzerPool::Cache> _pool;
  };

  // map of caches of irs::analysis::analyzer pools indexed by analyzer name and their associated metas
  std::unordered_map<irs::hashed_string_ref, AnalyzerEntry> _analyzers;
  mutable irs::async_utils::read_write_mutex _mutex;
  bool _started;

  std::shared_ptr<AnalyzerPool::Cache> get(AnalyzerEntry const& entry) const;
};

NS_END // iresearch
NS_END // arangodb

#endif