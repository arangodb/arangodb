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
/// @author Tobias Gödderz
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_CLUSTER_CLUSTERTYPES_H
#define ARANGOD_CLUSTER_CLUSTERTYPES_H

#include <limits>
#include <string>
#include <iosfwd>
#include <memory>
#include "velocypack/Builder.h"
#include "velocypack/velocypack-aliases.h"

#include "Basics/Result.h"

namespace arangodb {

typedef std::string ServerID;         // ID of a server
typedef std::string DatabaseID;       // ID/name of a database
typedef std::string CollectionID;     // ID of a collection
typedef std::string ViewID;           // ID of a view
typedef std::string ShardID;          // ID of a shard
typedef uint32_t ServerShortID;       // Short ID of a server
typedef std::string ServerShortName;  // Short name of a server

class RebootId {
 public:
  explicit constexpr RebootId(uint64_t rebootId) noexcept : _value(rebootId) {}
  uint64_t value() const noexcept { return _value; }

  bool initialized() const noexcept { return value() != 0; }

  bool operator==(RebootId other) const noexcept {
    return value() == other.value();
  }
  bool operator!=(RebootId other) const noexcept {
    return value() != other.value();
  }
  bool operator<(RebootId other) const noexcept {
    return value() < other.value();
  }
  bool operator>(RebootId other) const noexcept {
    return value() > other.value();
  }
  bool operator<=(RebootId other) const noexcept {
    return value() <= other.value();
  }
  bool operator>=(RebootId other) const noexcept {
    return value() >= other.value();
  }

  static constexpr RebootId max() noexcept {
    return RebootId{std::numeric_limits<decltype(_value)>::max()};
  }
  
  std::ostream& print(std::ostream& o) const;

 private:
  uint64_t _value;
};

namespace velocypack {
class Builder;
class Slice;
}

struct AnalyzersRevision {
 public:
  using Revision = uint64_t;
  using Ptr = std::shared_ptr<AnalyzersRevision const>;


  static constexpr Revision LATEST = std::numeric_limits<uint64_t>::max();
  static constexpr Revision MIN = 0;
  
  AnalyzersRevision(AnalyzersRevision const&) = delete;
  AnalyzersRevision& operator=(AnalyzersRevision const&) = delete;

  Revision getRevision() const noexcept {
    return _revision;
  }

  Revision getBuildingRevision() const noexcept {
    return _buildingRevision;
  }

  ServerID const& getServerID() const noexcept {
    return _serverID;
  }

  RebootId const& getRebootID() const noexcept {
    return _rebootID;
  }

  void toVelocyPack(VPackBuilder& builder) const;

  static Ptr fromVelocyPack(VPackSlice const& slice, std::string& error);

  static Ptr getEmptyRevision();

 private:
  AnalyzersRevision(Revision revision, Revision buildingRevision,
    ServerID&& serverID, uint64_t rebootID) noexcept
    : _revision(revision), _buildingRevision(buildingRevision),
    _serverID(std::move(serverID)), _rebootID(rebootID) {}

  Revision _revision;
  Revision _buildingRevision;
  ServerID _serverID;
  RebootId _rebootID;
};

/// @brief Analyzers revisions used in query.
/// Stores current database revision
/// and _system database revision (analyzers from _system are accessible from other databases)
/// If at some point we will decide to allow cross-database anayzer usage this could
/// became more complicated. But for now  we keep it simple - store just two members
struct QueryAnalyzerRevisions {
  constexpr QueryAnalyzerRevisions(AnalyzersRevision::Revision current, 
                                   AnalyzersRevision::Revision system)
    : currentDbRevision(current), systemDbRevision(system) {}

  QueryAnalyzerRevisions() = default;
  QueryAnalyzerRevisions(QueryAnalyzerRevisions const&) = default;
  QueryAnalyzerRevisions& operator=(QueryAnalyzerRevisions const&) = default;

  void toVelocyPack(VPackBuilder& builder) const;
  Result fromVelocyPack(velocypack::Slice slice);

  bool isDefault() const noexcept {
    return currentDbRevision == AnalyzersRevision::MIN &&
      systemDbRevision == AnalyzersRevision::MIN;
  }

  bool operator==(QueryAnalyzerRevisions const& other) const noexcept {
    return currentDbRevision == other.currentDbRevision &&
      systemDbRevision == other.systemDbRevision;
  }

  std::ostream& print(std::ostream& o) const;

  bool operator!=(QueryAnalyzerRevisions const& other) const noexcept {
    return !(*this == other);
  }

  /// @brief Gets analyzers revision to be used with specified database
  /// @param vocbase database name
  /// @return analyzers revision
  AnalyzersRevision::Revision getVocbaseRevision(std::string_view vocbase) const noexcept;

  static QueryAnalyzerRevisions QUERY_LATEST;

 private:
  AnalyzersRevision::Revision currentDbRevision{ AnalyzersRevision::MIN};
  AnalyzersRevision::Revision systemDbRevision{ AnalyzersRevision::MIN};
};

}  // namespace arangodb

std::ostream& operator<<(std::ostream& o, arangodb::RebootId const& r);
std::ostream& operator<<(std::ostream& o, arangodb::QueryAnalyzerRevisions const& r);

#endif  // ARANGOD_CLUSTER_CLUSTERTYPES_H
