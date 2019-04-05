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

#include "ApplicationFeatures/ApplicationFeature.h"
#include "Auth/Common.h"
#include "VocBase/voc-types.h"

struct TRI_vocbase_t; // forward declaration

namespace arangodb {
namespace transaction {

class Methods;  // forward declaration

}  // namespace transaction
}  // namespace arangodb

namespace arangodb {
namespace iresearch {

////////////////////////////////////////////////////////////////////////////////
/// @brief a cache of IResearch analyzer instances
///        and a provider of AQL TOKENS(<data>, <analyzer>) function
///        NOTE: deallocation of an IResearchAnalyzerFeature instance
///              invalidates all AnalyzerPool instances previously provided
///              by the deallocated feature instance
////////////////////////////////////////////////////////////////////////////////
class IResearchAnalyzerFeature final : public arangodb::application_features::ApplicationFeature {
 public:
  // thread-safe analyzer pool
  class AnalyzerPool : private irs::util::noncopyable {
   public:
    typedef std::shared_ptr<AnalyzerPool> ptr;
    explicit AnalyzerPool(irs::string_ref const& name);
    irs::flags const& features() const noexcept { return _features; }
    irs::analysis::analyzer::ptr get() const noexcept;  // nullptr == error creating analyzer
    std::string const& name() const noexcept { return _name; }
    irs::string_ref const& properties() const noexcept { return _properties; }
    irs::string_ref const& type() const noexcept { return _type; }

   private:
    friend class IResearchAnalyzerFeature; // required for calling AnalyzerPool::init(...) and AnalyzerPool::setKey(...)

    // 'make(...)' method wrapper for irs::analysis::analyzer types
    struct Builder {
      typedef irs::analysis::analyzer::ptr ptr;
      DECLARE_FACTORY(irs::string_ref const& type, irs::string_ref const& properties);
    };

    mutable irs::unbounded_object_pool<Builder> _cache;  // cache of irs::analysis::analyzer (constructed via
                                                         // AnalyzerBuilder::make(...))
    std::string _config;   // non-null type + non-null properties + key
    irs::flags _features;  // cached analyzer features
    irs::string_ref _key; // the key of the persisted configuration for this pool, null == static analyzer
    std::string _name;  // ArangoDB alias for an IResearch analyzer configuration
    irs::string_ref _properties;  // IResearch analyzer configuration
    irs::string_ref _type;        // IResearch analyzer name

    bool init(irs::string_ref const& type, irs::string_ref const& properties,
              irs::flags const& features = irs::flags::empty_instance());
    void setKey(irs::string_ref const& type);
  };

  explicit IResearchAnalyzerFeature(arangodb::application_features::ApplicationServer& server);

  //////////////////////////////////////////////////////////////////////////////
  /// @return analyzers in the specified vocbase are granted 'level' access
  //////////////////////////////////////////////////////////////////////////////
  static bool canUse( // check permissions
    TRI_vocbase_t const& vocbase, // analyzer vocbase
    arangodb::auth::Level const& level // access level
  );

  //////////////////////////////////////////////////////////////////////////////
  /// @return analyzer with the given prefixed name (or unprefixed and resides
  ///         in defaultVocbase) is granted 'level' access
  //////////////////////////////////////////////////////////////////////////////
  static bool canUse( // check permissions
    irs::string_ref const& name, // analyzer name (already normalized)
    arangodb::auth::Level const& level // access level
  );

  //////////////////////////////////////////////////////////////////////////////
  /// @brief emplace an analyzer as per the specified parameters
  /// @param result the result of the successful emplacement (out-param)
  ///               first - the emplaced pool
  ///               second - if an insertion of an new analyzer occured
  /// @param name analyzer name (already normalized)
  /// @param type the underlying IResearch analyzer type
  /// @param properties the configuration for the underlying IResearch type
  /// @param features the expected features the analyzer should produce
  /// @param implicitCreation false == treat as error if creation is required
  /// @return success
  /// @note emplacement while inRecovery() will not allow adding new analyzers
  ///       valid because for existing links the analyzer definition should
  ///       already have been persisted and feature administration is not
  ///       allowed during recovery
  //////////////////////////////////////////////////////////////////////////////
  typedef std::pair<AnalyzerPool::ptr, bool> EmplaceResult;
  arangodb::Result emplace( // emplace an analyzer
    EmplaceResult& result, // emplacement result on success (out-param)
    irs::string_ref const& name, // analyzer name
    irs::string_ref const& type, // analyzer type
    irs::string_ref const& properties, // analyzer properties
    irs::flags const& features = irs::flags::empty_instance() // analyzer features
  );

  //////////////////////////////////////////////////////////////////////////////
  /// @brief get analyzer or placeholder
  ///        before start() returns pool placeholder,
  ///        during start() all placeholders are initialized,
  ///        after start() returns same as get(...)
  /// @param name analyzer name (used verbatim)
  //////////////////////////////////////////////////////////////////////////////
  // FIXME TODO remove
  AnalyzerPool::ptr ensure(irs::string_ref const& name);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief find analyzer
  /// @param name analyzer name (already normalized)
  /// @return analyzer with the specified name or nullptr
  //////////////////////////////////////////////////////////////////////////////
  AnalyzerPool::ptr get(irs::string_ref const& name) const noexcept;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief find analyzer
  /// @param name analyzer name (already normalized)
  /// @param type the underlying IResearch analyzer type
  /// @param properties the configuration for the underlying IResearch type
  /// @param features the expected features the analyzer should produce
  /// @return analyzer matching the specified parameters or nullptr
  /// @note will construct and cache the analyzer if missing only on db-server
  ///       persistence in the cases of inRecovery or !storage engine will fail
  //////////////////////////////////////////////////////////////////////////////
  AnalyzerPool::ptr get( // find analyzer
    irs::string_ref const& name, // analyzer name
    irs::string_ref const& type, // analyzer type
    irs::string_ref const& properties, // analyzer properties
    irs::flags const& features // analyzer features
  );

  static AnalyzerPool::ptr identity() noexcept;  // the identity analyzer
  static std::string const& name() noexcept;

  //////////////////////////////////////////////////////////////////////////////
  /// @return normalized analyzer name, i.e. with vocbase prefix
  //////////////////////////////////////////////////////////////////////////////
  static std::string normalize( // normalize name
    irs::string_ref const& name, // analyzer name
    TRI_vocbase_t const& activeVocbase, // fallback vocbase if not part of name
    TRI_vocbase_t const& systemVocbase, // the system vocbase for use with empty prefix
    bool expandVocbasePrefix = true // use full vocbase name as prefix for active/system v.s. EMPTY/'::'
  );

  void prepare() override;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief remove the specified analyzer
  /// @param name analyzer name (already normalized)
  /// @param force remove even if the analyzer is actively referenced
  //////////////////////////////////////////////////////////////////////////////
  arangodb::Result remove(irs::string_ref const& name, bool force = false);

  void start() override;
  void stop() override;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief visit all analyzers for the specified vocbase
  /// @param vocbase only visit analysers for this vocbase (nullptr == all)
  /// @return visitation compleated fully
  //////////////////////////////////////////////////////////////////////////////
  bool visit( // visit analyzers
    std::function<bool(AnalyzerPool::ptr const& analyzer)> const& visitor, // visitor
    TRI_vocbase_t const* vocbase = nullptr // analyzers for vocbase
  ) const;

 private:
  // map of caches of irs::analysis::analyzer pools indexed by analyzer name and
  // their associated metas
  typedef std::unordered_map<irs::hashed_string_ref, AnalyzerPool::ptr> Analyzers;

  Analyzers _analyzers; // all analyzers known to this feature (including static) (names are stored with expanded vocbase prefixes)
  Analyzers _customAnalyzers;  // user defined analyzers managed by this feature, a
                               // subset of '_analyzers' (used for removals)
  std::unordered_map<std::string, std::chrono::system_clock::time_point> _lastLoad; // last time a database was loaded
  mutable irs::async_utils::read_write_mutex _mutex;
  bool _started;

  // FIXME TODO remove
  std::pair<AnalyzerPool::ptr, bool> emplace(
      irs::string_ref const& name, irs::string_ref const& type,
      irs::string_ref const& properties, bool initAndPersist,
      irs::flags const& features = irs::flags::empty_instance()) noexcept;

  static Analyzers const& getStaticAnalyzers();

  //////////////////////////////////////////////////////////////////////////////
  /// @brief validate analyzer parameters and emplace into map
  //////////////////////////////////////////////////////////////////////////////
  typedef std::pair<Analyzers::iterator, bool> EmplaceAnalyzerResult;
  arangodb::Result emplaceAnalyzer( // emplace
    EmplaceAnalyzerResult& result, // emplacement result on success (out-param)
    arangodb::iresearch::IResearchAnalyzerFeature::Analyzers& analyzers, // analyzers
    irs::string_ref const& name, // analyzer name
    irs::string_ref const& type, // analyzer type
    irs::string_ref const& properties, // analyzer properties
    irs::flags const& features // analyzer features
  );

  //////////////////////////////////////////////////////////////////////////////
  /// @brief ensure an analyzer as per the specified parameters exists if
  ///        possible
  /// @param result the result of the successful emplacement (out-param)
  ///               first - the emplaced pool
  ///               second - if an insertion of an new analyzer occured
  /// @param name analyzer name (already normalized)
  /// @param type the underlying IResearch analyzer type
  /// @param properties the configuration for the underlying IResearch type
  /// @param features the expected features the analyzer should produce
  /// @param allowCreation false == treat as an error if creation is required
  /// @return success
  /// @note ensure while inRecovery() will not allow new analyzer persistance
  ///       valid because for existing links the analyzer definition should
  ///       already have been persisted and feature administration is not
  ///       allowed during recovery
  /// @note for db-server analyzers are not persisted
  ///       valid because the authoritative analyzer source is from coordinators
  //////////////////////////////////////////////////////////////////////////////
  arangodb::Result ensure( // ensure analyzer existence if possible
    EmplaceResult& result, // emplacement result on success (out-param)
    irs::string_ref const& name, // analyzer name
    irs::string_ref const& type, // analyzer type
    irs::string_ref const& properties, // analyzer properties
    irs::flags const& features, // analyzer features
    bool allowCreation
  );

  //////////////////////////////////////////////////////////////////////////////
  /// @brief load the analyzers for the specific database, analyzers read from
  ///        the corresponding collection if they have not been loaded yet
  /// @param database the database to load analizers for (nullptr == all)
  /// @note on coordinator and db-server reload is also done if the database has
  ///       not been reloaded in 'timeout' seconds
  //////////////////////////////////////////////////////////////////////////////
  arangodb::Result loadAnalyzers( // load analyzers
      irs::string_ref const& database = irs::string_ref::NIL // database to load
  );

  //////////////////////////////////////////////////////////////////////////////
  /// @brief store the definition for the speicifed pool in the corresponding
  ///        vocbase
  /// @note on success will modify the '_key' of the pool
  //////////////////////////////////////////////////////////////////////////////
  arangodb::Result storeAnalyzer(AnalyzerPool& pool);
};

}  // namespace iresearch
}  // namespace arangodb

#endif