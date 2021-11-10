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

#include "GraphFormat.h"

#include "Greenspun/Interpreter.h"
#include "Pregel/Algos/AIR/VertexComputation.h"

#include "Basics/VelocyPackHelper.h"
#include "Basics/overload.h"

#include <velocypack/Collection.h>
#include <velocypack/velocypack-aliases.h>

#include <utility>

namespace arangodb {
namespace pregel {
namespace algos {
namespace accumulators {

// Graph Format
GraphFormat::GraphFormat(application_features::ApplicationServer& server, std::string resultField,
                         AccumulatorsDeclaration globalAccumulatorDeclarations,
                         AccumulatorsDeclaration vertexAccumulatorDeclarations,
                         CustomAccumulatorDefinitions customDefinitions,
                         DataAccessDefinition dataAccess)
    : graph_format(server),
      _resultField(std::move(resultField)),
      _globalAccumulatorDeclarations(std::move(globalAccumulatorDeclarations)),
      _vertexAccumulatorDeclarations(std::move(vertexAccumulatorDeclarations)),
      _customDefinitions(std::move(customDefinitions)),
      _dataAccess(std::move(dataAccess)) {}

size_t GraphFormat::estimatedVertexSize() const { return sizeof(vertex_type); }
size_t GraphFormat::estimatedEdgeSize() const { return sizeof(edge_type); }

void filterDocumentData(VPackBuilder& finalBuilder, PathList paths,
                        arangodb::velocypack::Slice const& document) {
  // necessary to be able to be merged later.
  finalBuilder.openObject();
  finalBuilder.close();

  for (auto&& path : paths) {
    std::visit(overload{[&](std::string const& key) {
                          VPackBuilder tmp;
                          VPackBuilder innerBuilder;
                          innerBuilder.openObject();
                          innerBuilder.add(key, document.get(key));
                          innerBuilder.close();

                          VPackCollection::merge(tmp, finalBuilder.slice(),
                                                 innerBuilder.slice(), true, false);
                          finalBuilder.clear();
                          finalBuilder.add(tmp.slice());
                        },
                        [&](KeyPath const& path) {
                          TRI_ASSERT(!path.empty());  // deserializer ensures this
                          size_t pathLength = path.size();
                          size_t iterationStep = 0;

                          std::vector<VPackStringRef> pathInner;

                          VPackBuilder innerArrayBuilder;
                          innerArrayBuilder.openObject();  // open outer object

                          for (auto&& innerKey : path) {
                            // build up path - will change in every iteration step
                            pathInner.emplace_back(innerKey);

                            if (iterationStep < (pathLength - 1)) {
                              innerArrayBuilder.add(VPackValue(innerKey));
                              innerArrayBuilder.openObject();
                            } else {
                              innerArrayBuilder.add(innerKey, document.get(path));
                            }

                            // get slice of each document depth
                            iterationStep++;
                          }

                          // now close all inner opened objects
                          for (size_t step = 0; step < (pathLength - 1); step++) {
                            innerArrayBuilder.close();
                          }

                          innerArrayBuilder.close();  // close outer object
                          VPackBuilder tmp;
                          VPackCollection::merge(tmp, finalBuilder.slice(),
                                                 innerArrayBuilder.slice(), true, false);
                          finalBuilder.clear();
                          finalBuilder.add(tmp.slice());
                        }},
               path);
  }
}

// Extract vertex data from vertex rawDocument into target
void GraphFormat::copyVertexData(arangodb::velocypack::Options const& vpackOptions,
                                 std::string const& documentId,
                                 arangodb::velocypack::Slice rawDocument,
                                 ProgrammablePregelAlgorithm::vertex_type& targetPtr,
                                 uint64_t& vertexIdRange) {
  // Eliminate all custom types
  VPackBuilder sanitizedDocument;
  basics::VelocyPackHelper::sanitizeNonClientTypes(rawDocument, rawDocument, sanitizedDocument,
                                                   &vpackOptions, false, true);

  if (_dataAccess.readVertex) {
    // copy only specified keys/key-paths to rawDocument
    VPackBuilder tmpBuilder;
    filterDocumentData(tmpBuilder, *_dataAccess.readVertex, sanitizedDocument.slice());
    targetPtr.reset(_vertexAccumulatorDeclarations, _customDefinitions,
                    documentId, tmpBuilder.slice(), _vertexIdRange++);
  } else {
    // copy all
    targetPtr.reset(_vertexAccumulatorDeclarations, _customDefinitions,
                    documentId, sanitizedDocument.slice(), _vertexIdRange++);
  }
}

void GraphFormat::copyEdgeData(arangodb::velocypack::Options const& vpackOptions,
                               arangodb::velocypack::Slice rawDocument,
                               ProgrammablePregelAlgorithm::edge_type& targetPtr) {
  // Eliminate all custom types
  VPackBuilder sanitizedDocument;
  basics::VelocyPackHelper::sanitizeNonClientTypes(rawDocument, rawDocument, sanitizedDocument,
                                                   &vpackOptions, false, true);

  if (_dataAccess.readEdge) {
    // copy only specified keys/key-paths to rawDocument
    VPackBuilder tmpBuilder;
    filterDocumentData(tmpBuilder, *_dataAccess.readEdge, sanitizedDocument.slice());
    targetPtr.reset(tmpBuilder.slice());
  } else {
    // copy all
    targetPtr.reset(sanitizedDocument.slice());
  }
}

greenspun::EvalResult GraphFormat::buildVertexDocumentWithResult(
    arangodb::velocypack::Builder& b, vertex_type const* ptr) const {
  if (_dataAccess.writeVertex) {
    if (_dataAccess.writeVertex->slice().isArray()) {
      greenspun::Machine m;
      InitMachine(m);

      m.setFunction("accum-ref",
                    [ptr](greenspun::Machine& ctx, VPackSlice const params,
                          VPackBuilder& tmpBuilder) -> greenspun::EvalResult {
                      return VertexComputation::air_accumRef_helper(params, tmpBuilder, ptr);
                    });

      VPackBuilder tmpBuilder;
      auto res = Evaluate(m, _dataAccess.writeVertex->slice(), tmpBuilder);
      if (res.fail()) {
        THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_AIR_EXECUTION_ERROR,
                                       res.error().toString());
      }

      if (tmpBuilder.slice().isObject()) {
        for (auto&& entry : VPackObjectIterator(tmpBuilder.slice())) {
          b.add(entry.key);
          b.add(entry.value);
        }
      } else {
        return {};  // will not write as tmpBuilder is not a valid (object) result
      }
    } else {
      return greenspun::EvalError(
          "writeVertex needs to be an array, but found: " +
          std::string{_dataAccess.writeVertex->slice().typeName()} +
          " instead.");
    }
  } else {
    VPackObjectBuilder guard(&b, _resultField);
    for (auto&& acc : ptr->_vertexAccumulators) {
      // (copy all) this is the default if no writeVertex program is set
      b.add(VPackValue(acc.first));
      if (auto res = acc.second->finalizeIntoBuilder(b); res.fail()) {
        return res.mapError([&](greenspun::EvalError& err) {
          err.wrapMessage("when finalizing accumulator " + acc.first);
        });
      }
    }
  }

  return {};
}

}  // namespace accumulators
}  // namespace algos
}  // namespace pregel
}  // namespace arangodb
