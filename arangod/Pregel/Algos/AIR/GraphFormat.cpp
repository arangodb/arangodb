////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2020 ArangoDB GmbH, Cologne, Germany
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

#include "Greenspun/Extractor.h"
#include "Greenspun/Interpreter.h"

#include "Basics/overload.h"

#include <velocypack/Collection.h>
#include <velocypack/velocypack-aliases.h>

#include <utility>

namespace arangodb {
namespace pregel {
namespace algos {
namespace accumulators {

// Graph Format
GraphFormat::GraphFormat(application_features::ApplicationServer& server,
                         std::string  resultField,
                         AccumulatorsDeclaration  globalAccumulatorDeclarations,
                         AccumulatorsDeclaration  vertexAccumulatorDeclarations,
                         CustomAccumulatorDefinitions customDefinitions,
                         DataAccessDefinition  dataAccess)
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
  for (auto&& path : paths) {
    std::visit(overload{[&](std::string const& key) {
                          VPackBuilder tmp;
                          VPackBuilder innerBuilder;
                          innerBuilder.openObject();
                          innerBuilder.add(key, document.get(key));
                          innerBuilder.close();

                          if (!finalBuilder.slice().isObject()) {
                            finalBuilder.openObject();
                            finalBuilder.close();
                          }
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

// Extract vertex data from vertex document into target
void GraphFormat::copyVertexData(std::string const& documentId,
                                 arangodb::velocypack::Slice vertexDocument,
                                 vertex_type& targetPtr, uint64_t& vertexIdRange) {
  // HACK HACK HACK - we want to eliminate all custom types and work with
  // the actual json document
  VPackBuilder doc;
  {
    VPackParser parser(doc);
    parser.parse(vertexDocument.toJson());
  }

  // TODO: change GraphFormat interface here. Work with builder instead of Slice
  if (_dataAccess.readVertex) {
    // copy only specified keys/key-paths to document
    VPackBuilder tmpBuilder;
    filterDocumentData(tmpBuilder, *_dataAccess.readVertex, doc.slice());
    targetPtr.reset(_vertexAccumulatorDeclarations, _customDefinitions,
                    _dataAccess, documentId, tmpBuilder.slice(), _vertexIdRange++);
  } else {
    // copy all
    targetPtr.reset(_vertexAccumulatorDeclarations, _customDefinitions,
                    _dataAccess, documentId, doc.slice(), _vertexIdRange++);
  }
}

void GraphFormat::copyEdgeData(arangodb::velocypack::Slice edgeDocument, edge_type& targetPtr) {
  // TODO: change GraphFormat interface here. Work with builder instead of Slice
  if (_dataAccess.readEdge) {
    // copy only specified keys/key-paths to document
    VPackBuilder tmpBuilder;
    filterDocumentData(tmpBuilder, *_dataAccess.readEdge, edgeDocument);
    targetPtr.reset(tmpBuilder.slice());
  } else {
    // copy all
    targetPtr.reset(edgeDocument);
  }
}

greenspun::EvalResult GraphFormat::buildVertexDocumentWithResult(
    arangodb::velocypack::Builder& b, const vertex_type* ptr) const {
  if (_dataAccess.writeVertex && _dataAccess.writeVertex->slice().isArray()) {
    greenspun::Machine m;
    InitMachine(m);

    // TODO: Try to use "air_accumRef" here instead (Source: VertexData)
    m.setFunction("accum-ref",
                  [ptr](greenspun::Machine& ctx, VPackSlice const params,
                        VPackBuilder& tmpBuilder) -> greenspun::EvalResult {
                    auto res = greenspun::extract<std::string>(params);
                    if (res.fail()) {
                      return std::move(res).error();
                    }

                    auto&& [accumId] = res.value();

                    if (auto iter = ptr->_vertexAccumulators.find(accumId);
                        iter != std::end(ptr->_vertexAccumulators)) {
                      return iter->second->getIntoBuilder(tmpBuilder);
                    }
                    return greenspun::EvalError("vertex accumulator `" + std::string{accumId} +
                                                "` not found");
                  });

    VPackBuilder tmpBuilder;
    auto res = Evaluate(m, _dataAccess.writeVertex->slice(), tmpBuilder);
    if (res.fail()) {
      THROW_ARANGO_EXCEPTION(res);
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
    VPackObjectBuilder guard(&b, _resultField);
    for (auto&& acc : ptr->_vertexAccumulators) {
      // (copy all) this is the default if no writeVertex program is set
      b.add(VPackValue(acc.first));
      if (auto res = acc.second->finalizeIntoBuilder(b); res.fail()) {
        return res.mapError([&](greenspun::EvalError &err) {
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
