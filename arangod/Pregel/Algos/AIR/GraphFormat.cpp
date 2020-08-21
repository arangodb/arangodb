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
#include "Pregel/Algos/AIR/Greenspun/Extractor.h"
#include "Pregel/Algos/AIR/Greenspun/Interpreter.h"

namespace arangodb {
namespace pregel {
namespace algos {
namespace accumulators {

// Graph Format
GraphFormat::GraphFormat(application_features::ApplicationServer& server,
                         std::string const& resultField,
                         AccumulatorsDeclaration const& globalAccumulatorDeclarations,
                         AccumulatorsDeclaration const& vertexAccumulatorDeclarations,
                         CustomAccumulatorDefinitions customDefinitions,
                         DataAccessDefinition const& dataAccess)
    : graph_format(server),
      _resultField(resultField),
      _globalAccumulatorDeclarations(globalAccumulatorDeclarations),
      _vertexAccumulatorDeclarations(vertexAccumulatorDeclarations),
      _customDefinitions(std::move(customDefinitions)),
      _dataAccess(dataAccess) {}

size_t GraphFormat::estimatedVertexSize() const { return sizeof(vertex_type); }
size_t GraphFormat::estimatedEdgeSize() const { return sizeof(edge_type); }

void filterDocumentData(VPackBuilder& tmpBuilder, VPackSlice const& arraySlice,
                        arangodb::velocypack::Slice& document) {
  for (auto&& key : VPackArrayIterator(arraySlice)) {
    if (key.isString()) {
      {
        VPackObjectBuilder ob(&tmpBuilder);
        tmpBuilder.add(key.copyString(), document.get(key.stringRef()));
      }
    } else if (key.isArray()) {
      std::vector<VPackStringRef> path;
      size_t pathLength = key.length();
      size_t iterationStep = 0;

      tmpBuilder.openObject();
      for (auto&& pathStep : VPackArrayIterator(key)) {
        if (!pathStep.isString()) {
          TRI_ASSERT(false);  // TODO: Add type checking in deserializer
        }
        // build up path - will change in every iteration step
        path.emplace_back(pathStep.stringRef());

        if (iterationStep < (pathLength - 1)) {
          tmpBuilder.add(pathStep.copyString(), VPackValue(VPackValueType::Object));
        } else {
          tmpBuilder.add(pathStep.copyString(), document.get(path));
        }

        // get slice of each document depth
        path.emplace_back(pathStep.stringRef());
        iterationStep++;
      }

      // now close all opened objects
      for (size_t step = 0; step < (pathLength - 1); step++) {
        tmpBuilder.close();
      }
    } else {
      TRI_ASSERT(false);
    }
  }
}

// Extract vertex data from vertex document into target
void GraphFormat::copyVertexData(std::string const& documentId,
                                 arangodb::velocypack::Slice vertexDocument,
                                 vertex_type& targetPtr) {
  if (targetPtr._dataAccess.readVertex->slice().isArray()) {
    // copy only specified keys/key-paths to document
    VPackBuilder tmpBuilder;
    filterDocumentData(tmpBuilder, targetPtr._dataAccess.readVertex->slice(), vertexDocument);
    targetPtr.reset(_globalAccumulatorDeclarations, _vertexAccumulatorDeclarations,
                    _customDefinitions, _dataAccess, documentId,
                    tmpBuilder.slice(), _vertexIdRange++);
  } else {
    // copy all
    targetPtr.reset(_globalAccumulatorDeclarations,
                    _vertexAccumulatorDeclarations, _customDefinitions,
                    _dataAccess, documentId, vertexDocument, _vertexIdRange++);
  }
}

void GraphFormat::copyEdgeData(arangodb::velocypack::Slice edgeDocument, edge_type& targetPtr) {
  // TODO: implement filtering
  targetPtr.reset(edgeDocument);
}

bool GraphFormat::buildVertexDocument(arangodb::velocypack::Builder& b,
                                      const vertex_type* ptr, size_t size) const {
  if (ptr->_dataAccess.writeVertex->slice().isArray()) {
    greenspun::Machine m;
    InitMachine(m);

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
                      return iter->second->getIntoBuilderWithResult(tmpBuilder);
                    }
                    return greenspun::EvalError("vertex accumulator `" + std::string{accumId} +
                                                "` not found");
                  });

    VPackBuilder tmpBuilder;
    auto res = Evaluate(m, ptr->_dataAccess.writeVertex->slice(), tmpBuilder);
    if (res.fail()) {
      LOG_DEVEL << "finalize program failed: " << res.error().toString();
      TRI_ASSERT(false);
    }

    if (tmpBuilder.slice().isObject()) {
      for (auto&& entry : VPackObjectIterator(tmpBuilder.slice())) {
        b.add(entry.key);
        b.add(entry.value);
      }
    } else {
      return false;  // will not write as tmpBuilder is not a valid (object) result
    }
  } else {
    VPackObjectBuilder guard(&b, _resultField);
    for (auto&& acc : ptr->_vertexAccumulators) {
      // this will be obsolete soon
      b.add(VPackValue(acc.first));
      if (auto res = acc.second->finalizeIntoBuilder(b); res.fail()) {
        LOG_DEVEL << "finalize program failed: " << res.error().toString();
        TRI_ASSERT(false);
      }
    }
  }

  return true;
}

bool GraphFormat::buildEdgeDocument(arangodb::velocypack::Builder& b,
                                    const edge_type* ptr, size_t size) const {
  // FIXME
  // std::abort();
  return false;
}

}  // namespace accumulators
}  // namespace algos
}  // namespace pregel
}  // namespace arangodb
