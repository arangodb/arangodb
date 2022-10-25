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
/// @author Heiko  Kernbach
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Inspection/Types.h"
namespace arangodb::aql::optimizer2 {

struct AttributeTypes {
 public:
  // General types
  typedef std::uint64_t Numeric;
  typedef double Double;
  typedef std::string String;

  // Specific types BaseNode
  typedef std::uint64_t NodeId;
  typedef std::string NodeType;  // TODO: Let's use a "numeric" type 'later'.
  typedef std::vector<Numeric> Dependencies;

  // Specific types Index
  typedef std::string IndexType;  // TODO: Let's use a "numeric" type 'later'.
};

struct ProjectionType {
  typedef std::vector<AttributeTypes::String> ProjectionArray;
  typedef AttributeTypes::String ProjectionString;
  // Specific types Projection(s)
  std::variant<ProjectionArray, ProjectionString> projection;

  bool operator==(ProjectionType const&) const = default;
};

template<class Inspector>
auto inspect(Inspector& f, ProjectionType& x) {
  namespace insp = arangodb::inspection;
  return f.variant(x.projection)
      .unqualified()
      .alternatives(insp::inlineType<ProjectionType::ProjectionArray>(),
                    insp::inlineType<ProjectionType::ProjectionString>());
}

}  // namespace arangodb::aql::optimizer2