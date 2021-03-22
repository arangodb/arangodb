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

#include "ClusterTypes.h"
#include "Basics/debugging.h"
#include "Basics/StaticStrings.h"
#include "Basics/voc-errors.h"

#include <iostream>

std::ostream& operator<<(std::ostream& o, arangodb::RebootId const& r) {
  return r.print(o);
}

std::ostream& operator<<(std::ostream& o, 
  arangodb::QueryAnalyzerRevisions const& r) {
  return r.print(o);
}

namespace arangodb {

void QueryAnalyzerRevisions::toVelocyPack(VPackBuilder& builder) const {
  VPackObjectBuilder scope(&builder, StaticStrings::ArangoSearchAnalyzersRevision);
  if (currentDbRevision != AnalyzersRevision::MIN) {
    scope->add(StaticStrings::ArangoSearchCurrentAnalyzersRevision,
               VPackValue(currentDbRevision));
  }
  if (systemDbRevision != AnalyzersRevision::MIN) {
    scope->add(StaticStrings::ArangoSearchSystemAnalyzersRevision,
               VPackValue(systemDbRevision));
  }
}

Result QueryAnalyzerRevisions::fromVelocyPack(velocypack::Slice slice) {
  auto revisions = slice.get(StaticStrings::ArangoSearchAnalyzersRevision);
  if (revisions.isObject()) {
    auto current = revisions.get(StaticStrings::ArangoSearchCurrentAnalyzersRevision);
    if (!current.isNone()) {
      if (current.isNumber()) {
        currentDbRevision = current.getNumber<AnalyzersRevision::Revision>();
      } else {
        std::string error{ "Invalid " };
        error.append(StaticStrings::ArangoSearchAnalyzersRevision);
        error += ".";
        error += StaticStrings::ArangoSearchCurrentAnalyzersRevision;
        error += " attribute value. Number expected got ";
        error += current.typeName();
        return Result(TRI_ERROR_INTERNAL, error);
      }
    } else {
      currentDbRevision = AnalyzersRevision::MIN;
    }
    auto sys = revisions.get(StaticStrings::ArangoSearchSystemAnalyzersRevision);
    if (!sys.isNone()) {
      if (sys.isNumber()) {
        systemDbRevision = sys.getNumber<AnalyzersRevision::Revision>();
      } else {
        std::string error{ "Invalid " };
        error.append(StaticStrings::ArangoSearchAnalyzersRevision);
        error += ".";
        error += StaticStrings::ArangoSearchSystemAnalyzersRevision;
        error += " attribute value. Number expected got ";
        error += sys.typeName();
        return Result(TRI_ERROR_INTERNAL, error);
      }
    } else {
      systemDbRevision = AnalyzersRevision::MIN;
    }
  } else if (revisions.isNone()) {
    // query without analyzers revision
    currentDbRevision = AnalyzersRevision::MIN;
    systemDbRevision = AnalyzersRevision::MIN;
  } else {
    std::string error;
    error = "Invalid ";
    error.append(StaticStrings::ArangoSearchAnalyzersRevision);
    error += " attribute value. Object expected got ";
    error += revisions.typeName();
    return Result(TRI_ERROR_INTERNAL, error);
  }
  return {};
}

AnalyzersRevision::Revision QueryAnalyzerRevisions::getVocbaseRevision(
  std::string_view vocbase) const noexcept {
  return vocbase == StaticStrings::SystemDatabase ? systemDbRevision : currentDbRevision;
}

std::ostream& QueryAnalyzerRevisions::print(std::ostream& o) const {
  o << "[Current:" << currentDbRevision << " System:" << systemDbRevision << "]";
  return o;
}

QueryAnalyzerRevisions QueryAnalyzerRevisions::QUERY_LATEST(AnalyzersRevision::LATEST,
                                                            AnalyzersRevision::LATEST);

std::ostream& RebootId::print(std::ostream& o) const {
  o << _value;
  return o;
}

AnalyzersRevision::Ptr AnalyzersRevision::getEmptyRevision() {
  static auto ptr =  std::shared_ptr<AnalyzersRevision::Ptr::element_type>(
      new AnalyzersRevision(AnalyzersRevision::MIN, AnalyzersRevision::MIN, "", 0));
  return ptr;
}

void AnalyzersRevision::toVelocyPack(VPackBuilder& builder) const {
  VPackObjectBuilder guard(&builder);
  builder.add(StaticStrings::AnalyzersRevision, VPackValue(_revision));
  builder.add(StaticStrings::AnalyzersBuildingRevision, VPackValue(_buildingRevision));
  TRI_ASSERT((_serverID.empty() && !_rebootID.initialized()) ||
             (!_serverID.empty() && _rebootID.initialized()));

  if (!_serverID.empty()) {
    builder.add(StaticStrings::AttrCoordinator, VPackValue(_serverID));
  }
  if (_rebootID.initialized()) {
    builder.add(StaticStrings::AttrCoordinatorRebootId, VPackValue(_rebootID.value()));
  }
}

AnalyzersRevision::Ptr AnalyzersRevision::fromVelocyPack(VPackSlice const& slice, std::string& error) {
  if (!slice.isObject()) {
    error = "Analyzers in the plan is not a valid json object.";
    return nullptr;
  }

  auto const revisionSlice = slice.get(StaticStrings::AnalyzersRevision);
  if (!revisionSlice.isNumber()) {
    error = StaticStrings::AnalyzersRevision + " key is missing or not a number";
    return nullptr;
  }

  auto const buildingRevisionSlice = slice.get(StaticStrings::AnalyzersBuildingRevision);
  if (!buildingRevisionSlice.isNumber()) {
    error = StaticStrings::AnalyzersBuildingRevision + " key is missing or not a number";
    return nullptr;
  }
  ServerID coordinatorID;
  if (slice.hasKey(StaticStrings::AttrCoordinator)) {
    auto const coordinatorSlice = slice.get(StaticStrings::AttrCoordinator);
    if (!coordinatorSlice.isString()) {
      error = StaticStrings::AttrCoordinator + " is not a string";
      return nullptr;
    }
    velocypack::ValueLength length;
    auto const* cID = coordinatorSlice.getString(length);
    coordinatorID = ServerID(cID, length);
  }

  uint64_t rebootID = 0;
  if (slice.hasKey(StaticStrings::AttrCoordinatorRebootId)) {
    auto const rebootIDSlice = slice.get(StaticStrings::AttrCoordinatorRebootId);
    if (!rebootIDSlice.isNumber()) {
      error = StaticStrings::AttrCoordinatorRebootId + " key is not a number";
      return nullptr;
    }
    rebootID = rebootIDSlice.getNumber<uint64_t>();
  }
  return std::shared_ptr<AnalyzersRevision::Ptr::element_type>(
      new AnalyzersRevision(revisionSlice.getNumber<AnalyzersRevision::Revision>(),
                            buildingRevisionSlice.getNumber<AnalyzersRevision::Revision>(),
                            std::move(coordinatorID), rebootID));
}
}  // namespace arangodb
