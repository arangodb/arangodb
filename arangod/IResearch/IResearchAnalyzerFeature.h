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
/// @author Andrey Abramov
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <chrono>
#include <functional>
#include <memory>
#include <ostream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include <analysis/analyzer.hpp>
#include <analysis/analyzers.hpp>
#include "analysis/token_streams.hpp"
#include <index/index_features.hpp>
#include <index/field_meta.hpp>
#include <utils/async_utils.hpp>
#include <utils/attributes.hpp>
#include <utils/bit_utils.hpp>
#include <utils/hash_utils.hpp>
#include <utils/memory.hpp>
#include <utils/noncopyable.hpp>
#include <utils/object_pool.hpp>
#include <utils/string.hpp>

#include <velocypack/Builder.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

#include "ApplicationFeatures/ApplicationFeature.h"
#include "Auth/Common.h"
#include "Basics/ReadWriteLock.h"
#include "Basics/Result.h"
#include "Basics/Identifier.h"  // this include only need to make clang see << operator for Identifier
#include "Cluster/ClusterTypes.h"
#include "IResearch/IResearchAnalyzerValueTypeAttribute.h"
#include "IResearch/IResearchCommon.h"
#include "Scheduler/SchedulerFeature.h"

struct TRI_vocbase_t;  // forward declaration

namespace arangodb {
namespace application_features {
class ApplicationServer;
}
}  // namespace arangodb

namespace arangodb {
namespace iresearch {

////////////////////////////////////////////////////////////////////////////////
/// @enum FieldFeatures
/// @brief supported field features
////////////////////////////////////////////////////////////////////////////////
enum class FieldFeatures : uint32_t { NONE = 0, NORM = 1 };

ENABLE_BITMASK_ENUM(FieldFeatures);

////////////////////////////////////////////////////////////////////////////////
/// @class Features
/// @brief a representation of supported IResearch features
////////////////////////////////////////////////////////////////////////////////
class Features {
 public:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief ctor
  //////////////////////////////////////////////////////////////////////////////
  constexpr Features() = default;

  constexpr Features(irs::IndexFeatures indexFeatures) noexcept
      : Features{FieldFeatures::NONE, indexFeatures} {}

  constexpr Features(FieldFeatures fieldFeatures,
                     irs::IndexFeatures indexFeatures) noexcept
      : _fieldFeatures{fieldFeatures}, _indexFeatures{indexFeatures} {}

  //////////////////////////////////////////////////////////////////////////////
  /// @brief adds feature by name. Properly resolves field/index features
  /// @param featureName feature name
  /// @return true if feature found, false otherwise
  //////////////////////////////////////////////////////////////////////////////
  bool add(irs::string_ref featureName);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Set to default state with no features
  //////////////////////////////////////////////////////////////////////////////
  void clear() noexcept {
    _indexFeatures = irs::IndexFeatures::NONE;
    _fieldFeatures = FieldFeatures::NONE;
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief custom field features
  //////////////////////////////////////////////////////////////////////////////
  std::vector<irs::type_info::type_id> fieldFeatures(LinkVersion version) const;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief index features
  //////////////////////////////////////////////////////////////////////////////
  constexpr irs::IndexFeatures indexFeatures() const noexcept {
    return _indexFeatures;
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief validate that features are supported by arangod an ensure that
  /// their
  ///        dependencies are met
  /// @return Result containing error description if any
  //////////////////////////////////////////////////////////////////////////////
  Result validate() const;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief visit feature names
  //////////////////////////////////////////////////////////////////////////////
  void visit(std::function<void(std::string_view)> visitor) const;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief compare features for equality
  //////////////////////////////////////////////////////////////////////////////
  constexpr bool operator==(Features const& rhs) const noexcept {
    return _indexFeatures == rhs._indexFeatures &&
           _fieldFeatures == rhs._fieldFeatures;
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief compare features for inequality
  //////////////////////////////////////////////////////////////////////////////
  constexpr bool operator!=(Features const& rhs) const noexcept {
    return !(*this == rhs);
  }

 private:
  FieldFeatures _fieldFeatures{FieldFeatures::NONE};
  irs::IndexFeatures _indexFeatures{irs::IndexFeatures::NONE};
};  // Features

////////////////////////////////////////////////////////////////////////////////
/// @class AnalyzerPool
/// @brief thread-safe analyzer pool
////////////////////////////////////////////////////////////////////////////////
class AnalyzerPool : private irs::util::noncopyable {
 public:
  using ptr = std::shared_ptr<AnalyzerPool>;

  using StoreFunc = VPackSlice (*)(irs::token_stream const* ctx,
                                   VPackSlice slice, VPackBuffer<uint8_t>& buf);

  // type tags for primitive token streams
  struct NullStreamTag {};
  struct BooleanStreamTag {};
  struct NumericStreamTag {};
  struct StringStreamTag {};

  // 'make(...)' method wrapper for irs::analysis::analyzer types
  struct Builder {
    using ptr = irs::analysis::analyzer::ptr;
    static ptr make(irs::string_ref const& type, VPackSlice properties);

    static ptr make(NullStreamTag) {
      return std::make_unique<irs::null_token_stream>();
    }

    static ptr make(BooleanStreamTag) {
      return std::make_unique<irs::boolean_token_stream>();
    }

    static ptr make(NumericStreamTag) {
      return std::make_unique<irs::numeric_token_stream>();
    }

    static ptr make(StringStreamTag) {
      return std::make_unique<irs::string_token_stream>();
    }
  };

  using CacheType = irs::unbounded_object_pool<Builder>;

  explicit AnalyzerPool(irs::string_ref const& name);

  // nullptr == error creating analyzer
  CacheType::ptr get() const noexcept;

  Features features() const noexcept { return _features; }
  irs::features_t fieldFeatures() const noexcept {
    return {_fieldFeatures.data(), _fieldFeatures.size()};
  }
  irs::IndexFeatures indexFeatures() const noexcept {
    return features().indexFeatures();
  }
  std::string const& name() const noexcept { return _name; }
  VPackSlice properties() const noexcept { return _properties; }
  irs::string_ref const& type() const noexcept { return _type; }
  AnalyzersRevision::Revision revision() const noexcept { return _revision; }
  AnalyzerValueType inputType() const noexcept { return _inputType; }
  AnalyzerValueType returnType() const noexcept { return _returnType; }
  StoreFunc storeFunc() const noexcept { return _storeFunc; }
  bool accepts(AnalyzerValueType types) const noexcept {
    return (_inputType & types) != AnalyzerValueType::Undefined;
  }

  bool requireMangled() const noexcept { return _requireMangling; }

  bool returns(AnalyzerValueType types) const noexcept {
    return (_returnType & types) != AnalyzerValueType::Undefined;
  }

  bool operator==(AnalyzerPool const& rhs) const;
  bool operator!=(AnalyzerPool const& rhs) const { return !(*this == rhs); }

  // definition to be stored in _analyzers collection or shown to the end user
  void toVelocyPack(velocypack::Builder& builder, bool forPersistence = false);

  // definition to be stored/shown in a link definition
  void toVelocyPack(velocypack::Builder& builder,
                    TRI_vocbase_t const* vocbase = nullptr);

 private:
  // required for calling AnalyzerPool::init(...) and AnalyzerPool::setKey(...)
  friend class IResearchAnalyzerFeature;

  void toVelocyPack(velocypack::Builder& builder, irs::string_ref const& name);

  bool init(irs::string_ref const& type, VPackSlice const properties,
            AnalyzersRevision::Revision revision, Features features,
            LinkVersion version);
  void setKey(irs::string_ref const& type);

  mutable CacheType _cache;  // cache of irs::analysis::analyzer
  std::vector<irs::type_info::type_id>
      _fieldFeatures;    // cached iresearch field features
  std::string _config;   // non-null type + non-null properties + key
  irs::string_ref _key;  // the key of the persisted configuration for this
                         // pool, null == static analyzer
  std::string _name;  // ArangoDB alias for an IResearch analyzer configuration.
                      // Should be normalized name or static analyzer name see
                      // assertion in ctor
  VPackSlice _properties;  // IResearch analyzer configuration
  irs::string_ref _type;   // IResearch analyzer name
  Features _features;      // analyzer features
  StoreFunc _storeFunc{};
  AnalyzerValueType _inputType{AnalyzerValueType::Undefined};
  AnalyzerValueType _returnType{AnalyzerValueType::Undefined};
  AnalyzersRevision::Revision _revision{AnalyzersRevision::MIN};
  bool _requireMangling{false};  // analyzer pool requires field name mangling
                                 // even in non-link usage
};                               // AnalyzerPool

////////////////////////////////////////////////////////////////////////////////
/// @brief a cache of IResearch analyzer instances
///        and a provider of AQL TOKENS(<data>, <analyzer>) function
///        NOTE: deallocation of an IResearchAnalyzerFeature instance
///              invalidates all AnalyzerPool instances previously provided
///              by the deallocated feature instance
////////////////////////////////////////////////////////////////////////////////
class IResearchAnalyzerFeature final
    : public application_features::ApplicationFeature {
 public:
  /// first == vocbase name, second == analyzer name
  /// EMPTY == system vocbase
  /// NIL == unprefixed analyzer name, i.e. active vocbase
  using AnalyzerName = std::pair<irs::string_ref, irs::string_ref>;

  explicit IResearchAnalyzerFeature(
      application_features::ApplicationServer& server);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief check permissions
  /// @param vocbase analyzer vocbase
  /// @param level access level
  /// @return analyzers in the specified vocbase are granted 'level' access
  //////////////////////////////////////////////////////////////////////////////
  static bool canUse(TRI_vocbase_t const& vocbase, auth::Level const& level);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief check permissions for analyzer usage from vocbase by name
  /// @param vocbaseName  vocbase name to check
  /// @param level access level
  /// @return analyzers in the specified vocbase are granted 'level' access
  //////////////////////////////////////////////////////////////////////////////
  static bool canUseVocbase(irs::string_ref const& vocbaseName,
                            auth::Level const& level);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief check permissions
  /// @param name analyzer name (already normalized)
  /// @param level access level
  /// @return analyzer with the given prefixed name (or unprefixed and resides
  ///         in defaultVocbase) is granted 'level' access
  //////////////////////////////////////////////////////////////////////////////
  static bool canUse(irs::string_ref const& name, auth::Level const& level);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief create new analyzer pool
  /// @param analyzer created analyzer
  /// @param name analyzer name (already normalized)
  /// @param type the underlying IResearch analyzer type
  /// @param properties the configuration for the underlying IResearch type
  /// @param revision the revision number for analyzer
  /// @param features the expected features the analyzer should produce
  /// @param version the target link version
  /// @param extendedNames whether or not extended analyzer names are allowed
  /// @return success
  //////////////////////////////////////////////////////////////////////////////
  static Result createAnalyzerPool(AnalyzerPool::ptr& analyzer,
                                   irs::string_ref const& name,
                                   irs::string_ref const& type,
                                   VPackSlice const properties,
                                   AnalyzersRevision::Revision revision,
                                   Features features, LinkVersion version,
                                   bool extendedNames);

  static AnalyzerPool::ptr identity() noexcept;  // the identity analyzer
  static std::string const& name() noexcept;

  //////////////////////////////////////////////////////////////////////////////
  /// @param name analyzer name
  /// @param activeVocbase fallback vocbase if not part of name
  /// @param expandVocbasePrefix use full vocbase name as prefix for
  ///                            active/system v.s. EMPTY/'::'
  /// @return normalized analyzer name, i.e. with vocbase prefix
  //////////////////////////////////////////////////////////////////////////////
  static std::string normalize(irs::string_ref const& name,
                               irs::string_ref const& activeVocbase,
                               bool expandVocbasePrefix = true);

  //////////////////////////////////////////////////////////////////////////////
  /// @param name analyzer name (normalized)
  /// @return vocbase prefix extracted from normalized analyzer name
  ///         EMPTY == system vocbase
  ///         NIL == analyzer name have had no db name prefix
  /// @see analyzerReachableFromDb
  //////////////////////////////////////////////////////////////////////////////
  static irs::string_ref extractVocbaseName(
      irs::string_ref const& name) noexcept {
    return splitAnalyzerName(name).first;
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Loads (or reloads if necessary) analyzers available for use in
  /// context of specified database.
  /// @param dbName database name
  /// @return overall load result
  //////////////////////////////////////////////////////////////////////////////
  Result loadAvailableAnalyzers(irs::string_ref const& dbName);

  //////////////////////////////////////////////////////////////////////////////
  /// Checks if analyzer db (identified by db name prefix extracted from
  /// analyzer name) could be reached from specified db. Properly handles
  /// special cases (e.g. NIL and EMPTY)
  /// @param dbNameFromAnalyzer database name extracted from analyzer name
  /// @param currentDbName database name to check against (should not be empty!)
  /// @param forGetters check special case for getting analyzer (not
  /// creating/removing)
  /// @return true if analyzer is reachable
  static bool analyzerReachableFromDb(irs::string_ref const& dbNameFromAnalyzer,
                                      irs::string_ref const& currentDbName,
                                      bool forGetters = false) noexcept;

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief split the analyzer name into the vocbase part and analyzer part
  /// @param name analyzer name
  /// @return pair of first == vocbase name, second == analyzer name
  ///         EMPTY == system vocbase
  ///         NIL == unprefixed analyzer name, i.e. active vocbase
  ////////////////////////////////////////////////////////////////////////////////
  static AnalyzerName splitAnalyzerName(
      irs::string_ref const& analyzer) noexcept;

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
  Result emplace(EmplaceResult& result, irs::string_ref const& name,
                 irs::string_ref const& type, VPackSlice const properties,
                 Features features = {});

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Emplaces batch of analyzers within single analyzers revision.
  /// Intended for use
  ///        with restore/replication. Tries to load as many as possible
  ///        analyzers skipping unparsable ones.
  /// @param vocbase target vocbase
  /// @param dumpedAnalyzers VPack array of dumped data
  /// @return OK or first failure
  /// @note should not be used while inRecovery()
  //////////////////////////////////////////////////////////////////////////////
  Result bulkEmplace(TRI_vocbase_t& vocbase, VPackSlice const dumpedAnalyzers);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief removes all analyzers from database in single revision
  /// @param vocbase target vocbase
  /// @return operation result
  /// @note should not be used while inRecovery()
  //////////////////////////////////////////////////////////////////////////////
  Result removeAllAnalyzers(TRI_vocbase_t& vocbase);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief find analyzer
  /// @param name analyzer name (already normalized)
  /// @param onlyCached check only locally cached analyzers
  /// @return analyzer with the specified name or nullptr
  //////////////////////////////////////////////////////////////////////////////
  AnalyzerPool::ptr get(irs::string_ref const& name,
                        QueryAnalyzerRevisions const& revision,
                        bool onlyCached = false) const noexcept {
    auto splittedName = splitAnalyzerName(name);
    TRI_ASSERT(splittedName.first.null() || !splittedName.first.empty());
    return get(name, splittedName,
               splittedName.first.null()
                   ? AnalyzersRevision::MIN  // built-in analyzers always has
                                             // MIN revision
                   : revision.getVocbaseRevision(splittedName.first),
               onlyCached);
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief find analyzer
  /// @param name analyzer name
  /// @param activeVocbase fallback vocbase if not part of name
  /// @param onlyCached check only locally cached analyzers
  /// @return analyzer with the specified name or nullptr
  //////////////////////////////////////////////////////////////////////////////
  AnalyzerPool::ptr get(irs::string_ref const& name,
                        TRI_vocbase_t const& activeVocbase,
                        QueryAnalyzerRevisions const& revision,
                        bool onlyCached = false) const;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief remove the specified analyzer
  /// @param name analyzer name (already normalized)
  /// @param force remove even if the analyzer is actively referenced
  //////////////////////////////////////////////////////////////////////////////
  Result remove(irs::string_ref const& name, bool force = false);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief visit all analyzers for the specified vocbase
  /// @param vocbase only visit analysers for this vocbase (nullptr == static)
  /// @return visitation compleated fully
  //////////////////////////////////////////////////////////////////////////////
  bool visit(
      std::function<bool(AnalyzerPool::ptr const&)> const& visitor) const;
  bool visit(std::function<bool(AnalyzerPool::ptr const&)> const& visitor,
             TRI_vocbase_t const* vocbase) const;

  ///////////////////////////////////////////////////////////////////////////////
  /// @brief removes analyzers for specified database from cache
  /// @param vocbase  database to invalidate analyzers
  ///////////////////////////////////////////////////////////////////////////////
  void invalidate(const TRI_vocbase_t& vocbase);

  ///////////////////////////////////////////////////////////////////////////////
  /// @brief return current known analyzers revision
  /// @param vocbase to get revision for
  /// @param forceLoadPlan force request to get latest available revision
  /// @return revision number. always 0 for single server and before plan is
  /// loaded
  ///////////////////////////////////////////////////////////////////////////////
  AnalyzersRevision::Ptr getAnalyzersRevision(const TRI_vocbase_t& vocbase,
                                              bool forceLoadPlan = false) const;

  ///////////////////////////////////////////////////////////////////////////////
  /// @brief return current known analyzers revision
  /// @param vocbase name to get revision for
  /// @param forceLoadPlan force request to get latest available revision
  /// @return revision number. always 0 for single server and before plan is
  /// loaded
  ///////////////////////////////////////////////////////////////////////////////
  AnalyzersRevision::Ptr getAnalyzersRevision(
      const irs::string_ref& vocbaseName, bool forceLoadPlan = false) const;

  virtual void prepare() override;
  virtual void start() override;
  virtual void beginShutdown() override;
  virtual void stop() override;

 private:
  // map of caches of irs::analysis::analyzer pools indexed by analyzer name and
  // their associated metas
  using Analyzers =
      std::unordered_map<irs::hashed_string_ref, AnalyzerPool::ptr>;
  using EmplaceAnalyzerResult = std::pair<Analyzers::iterator, bool>;

  Analyzers
      _analyzers;  // all analyzers known to this feature (including static)
                   // (names are stored with expanded vocbase prefixes)
  std::unordered_map<std::string, AnalyzersRevision::Revision>
      _lastLoad;  // last revision for database was loaded
  mutable basics::ReadWriteLock
      _mutex;  // for use with member '_analyzers', '_lastLoad'

  static Analyzers const& getStaticAnalyzers();

  Result removeFromCollection(irs::string_ref const& name,
                              irs::string_ref const& vocbase);
  Result cleanupAnalyzersCollection(
      irs::string_ref const& database,
      AnalyzersRevision::Revision buildingRevision);
  Result finalizeRemove(irs::string_ref const& name,
                        irs::string_ref const& vocbase);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief validate analyzer parameters and emplace into map
  //////////////////////////////////////////////////////////////////////////////
  Result emplaceAnalyzer(
      EmplaceAnalyzerResult& result,
      iresearch::IResearchAnalyzerFeature::Analyzers& analyzers,
      irs::string_ref const name, irs::string_ref const type,
      VPackSlice const properties, Features const& features,
      AnalyzersRevision::Revision revision);

  AnalyzerPool::ptr get(irs::string_ref const& normalizedName,
                        AnalyzerName const& name,
                        AnalyzersRevision::Revision const revision,
                        bool onlyCached) const noexcept;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief load the analyzers for the specific database, analyzers read from
  ///        the corresponding collection if they have not been loaded yet
  /// @param database the database to load analizers for (nullptr == all)
  /// @note on coordinator and db-server reload is also done if the database has
  ///       changed analyzers revision in agency
  //////////////////////////////////////////////////////////////////////////////
  Result loadAnalyzers(irs::string_ref const& database = irs::string_ref::NIL);

  ////////////////////////////////////////////////////////////////////////////////
  /// removes analyzers for database from feature cache
  /// Write lock must be acquired by caller
  /// @param database the database to cleanup analyzers for
  void cleanupAnalyzers(irs::string_ref const& database);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief store the definition for the speicifed pool in the corresponding
  ///        vocbase
  /// @note on success will modify the '_key' of the pool
  //////////////////////////////////////////////////////////////////////////////
  Result storeAnalyzer(AnalyzerPool& pool);

  /// @brief dangling analyzer revisions collector
  std::function<void(bool)> _gcfunc;
  std::mutex _workItemMutex;
  Scheduler::WorkHandle _workItem;
};

}  // namespace iresearch
}  // namespace arangodb
