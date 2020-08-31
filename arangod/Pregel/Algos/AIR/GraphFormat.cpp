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

#include <velocypack/Collection.h>
#include <velocypack/velocypack-aliases.h>

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

void filterDocumentData(VPackBuilder& finalBuilder, VPackSlice const& arraySlice,
                        arangodb::velocypack::Slice& document) {
  for (auto&& key : VPackArrayIterator(arraySlice)) {
    if (key.isString()) {
      VPackBuilder tmp;
      VPackBuilder innerBuilder;
      innerBuilder.openObject();
      innerBuilder.add(key.copyString(), document.get(key.stringRef()));
      innerBuilder.close();

      if (!finalBuilder.slice().isObject()) {
        finalBuilder.openObject();
        finalBuilder.close();
      }
      VPackCollection::merge(tmp, finalBuilder.slice(), innerBuilder.slice(), true, false);
      finalBuilder.clear();
      finalBuilder.add(tmp.slice());
    } else if (key.isArray()) {
      std::vector<VPackStringRef> path;
      size_t pathLength = key.length();
      size_t iterationStep = 0;

      VPackBuilder innerArrayBuilder;
      innerArrayBuilder.openObject();  // open outer object

      for (auto&& innerKey : VPackArrayIterator(key)) {
        // TODO: optimize in the future - right now complicated to build a fresh object via key-path strings

        if (!innerKey.isString()) {
          TRI_ASSERT(false);  // TODO: Add type checking in deserializer
        }
        // build up path - will change in every iteration step
        path.emplace_back(innerKey.stringRef());

        if (iterationStep < (pathLength - 1)) {
          innerArrayBuilder.add(innerKey);  //, VPackValue(VPackValueType::Object));
          innerArrayBuilder.openObject();
        } else {
          innerArrayBuilder.add(innerKey.copyString(), document.get(path));
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
    } else {
      // TODO: this cannot happen anymore if we introduce type checking within the deserializer
      LOG_DEVEL << "filterDocumentData: Expecting key to be a string nor an array, got: " << key.toJson();
      TRI_ASSERT(false);
    }
  }
}

// Extract vertex data from vertex document into target
void GraphFormat::copyVertexData(std::string const& documentId,
                                 arangodb::velocypack::Slice vertexDocument,
                                 vertex_type& targetPtr) {
  // TODO: change GraphFormat interface here. Work with builder instead of Slice
  if (_dataAccess.readVertex->slice().isArray()) {
    // copy only specified keys/key-paths to document
    VPackBuilder tmpBuilder;
    filterDocumentData(tmpBuilder, _dataAccess.readVertex->slice(), vertexDocument);
    targetPtr.reset(_vertexAccumulatorDeclarations, _customDefinitions,
                    _dataAccess, documentId, tmpBuilder.slice(), _vertexIdRange++);
  } else {
    // copy all
    targetPtr.reset(_vertexAccumulatorDeclarations, _customDefinitions,
                    _dataAccess, documentId, vertexDocument, _vertexIdRange++);
  }
}

void GraphFormat::copyEdgeData(arangodb::velocypack::Slice edgeDocument, edge_type& targetPtr) {
  // TODO: change GraphFormat interface here. Work with builder instead of Slice
  if (_dataAccess.readEdge->slice().isArray()) {
    // copy only specified keys/key-paths to document
    VPackBuilder tmpBuilder;
    filterDocumentData(tmpBuilder, _dataAccess.readEdge->slice(), edgeDocument);
    targetPtr.reset(tmpBuilder.slice());
  } else {
    // copy all
    targetPtr.reset(edgeDocument);
  }
}

bool GraphFormat::buildVertexDocument(arangodb::velocypack::Builder& b,
                                      const vertex_type* ptr, size_t size) const {
  if (ptr->_dataAccess.writeVertex->slice().isArray()) {
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
    auto res = Evaluate(m, ptr->_dataAccess.writeVertex->slice(), tmpBuilder);
    if (res.fail()) {
      LOG_DEVEL << "finalize program failed: " << res.error().toString();
      THROW_ARANGO_EXCEPTION(res);
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
      // (copy all) this is the default if no writeVertex program is set
      b.add(VPackValue(acc.first));
      if (auto res = acc.second->finalizeIntoBuilder(b); res.fail()) {
        LOG_DEVEL << "finalize program failed: " << res.error().toString();
        THROW_ARANGO_EXCEPTION(res);
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
