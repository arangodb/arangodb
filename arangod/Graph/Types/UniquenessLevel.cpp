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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#include "UniquenessLevel.h"

#include <iostream>

using namespace arangodb;
using namespace arangodb::graph;

std::ostream& arangodb::graph::operator<<(std::ostream& stream,
                                          VertexUniquenessLevel const& level) {
  switch (level) {
    case VertexUniquenessLevel::NONE:
      stream << "NONE";
      break;
    case VertexUniquenessLevel::PATH:
      stream << "PATH";
      break;
    case VertexUniquenessLevel::GLOBAL:
      stream << "GLOBAL";
      break;
  }
  return stream;
}

std::ostream& arangodb::graph::operator<<(std::ostream& stream,
                                          EdgeUniquenessLevel const& level) {
  switch (level) {
    case EdgeUniquenessLevel::NONE:
      stream << "NONE";
      break;
    case EdgeUniquenessLevel::PATH:
      stream << "PATH";
      break;
    case EdgeUniquenessLevel::GLOBAL:
      stream << "GLOBAL";
      break;
  }
  return stream;
}