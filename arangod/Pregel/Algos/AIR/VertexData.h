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
/// @author Heiko Kernbach
/// @author Lars Maier
/// @author Markus Pfeiffer
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_PREGEL_ALGOS_ACCUMULATORS_VERTEXDATA_H
#define ARANGODB_PREGEL_ALGOS_ACCUMULATORS_VERTEXDATA_H 1

#include "Pregel/Algos/AIR/AbstractAccumulator.h"
#include "Pregel/Algos/AIR/AccumulatorOptions.h"
#include "Pregel/Algos/AIR/Accumulators.h"

#include <velocypack/Builder.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

namespace arangodb {
namespace pregel {
namespace algos {
namespace accumulators {

// Vertex data has to be default constructible m(
class VertexData {
 public:
  void reset(AccumulatorsDeclaration const& vertexAccumulatorsDeclaration,
             CustomAccumulatorDefinitions const& customDefinitions,
             std::string documentId, VPackSlice const& doc, std::size_t vertexId);

  std::unique_ptr<AccumulatorBase> const& accumulatorByName(std::string_view name) const;

  // The vertex accumulators are *not* reset automatically
  std::map<std::string, std::unique_ptr<AccumulatorBase>, std::less<>> _vertexAccumulators;

  std::string _documentId;
  // FIXME: YOLO. we copy the whole document, which is
  //        probably super expensive.
  VPackBuilder _document;
  std::size_t _vertexId;
};

std::ostream& operator<<(std::ostream&, VertexData const&);

}  // namespace accumulators
}  // namespace algos
}  // namespace pregel
}  // namespace arangodb
#endif
