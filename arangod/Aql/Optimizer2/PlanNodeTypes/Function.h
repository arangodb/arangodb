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
/// @author Heiko Kernbach
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include "Aql/Optimizer2/Types/Types.h"

#include "velocypack/Builder.h"

namespace arangodb::aql::optimizer2::types {

struct Function {
  // always set
  AttributeTypes::String name;
  bool isDeterministic;
  bool canRunOnDBServer;  // deprecated
  bool usesV8;

  // only set in case we have a function of type "NODE_TYPE_FCALL"
  std::optional<bool> canAccessDocuments;
  std::optional<bool> canRunOnDBServerCluster;
  std::optional<bool> canRunOnDBServerOneShard;
  std::optional<bool> cacheable;

  bool operator==(Function const& other) const = default;
};

template<typename Inspector>
auto inspect(Inspector& f, Function& x) {
  return f.object(x).fields(
      f.field("name", x.name), f.field("isDeterministic", x.isDeterministic),
      f.field("canRunOnDBServer", x.canRunOnDBServer),
      f.field("usesV8", x.usesV8),
      // optionals
      f.field("canAccessDocuments", x.canAccessDocuments),
      f.field("canRunOnDBServerCluster", x.canRunOnDBServerCluster),
      f.field("canRunOnDBServerOneShard", x.canRunOnDBServerOneShard),
      f.field("cacheable", x.cacheable)

  );
}

}  // namespace arangodb::aql::optimizer2::types
