////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#include "ColorPropagation.h"
#include <atomic>
#include <climits>
#include <utility>
#include "Cluster/ClusterInfo.h"
#include "Cluster/ServerState.h"
#include "Pregel/Aggregator.h"
#include "Pregel/Algorithm.h"
#include "Pregel/IncomingCache.h"

using namespace arangodb;
using namespace arangodb::pregel;
using namespace arangodb::pregel::algos;

namespace {
/**
 * Add new colors to currentColors and return new colors (those colors from
 * messages that were not in currentColors). Consider only messages from
 * vertices of other equivalence classes.
 * @param currentColors the current colors to be updated
 * @param messages iterator over obtained colors
 */
VectorOfColors updateColors(
    ColorPropagationValue* vertexState,
    MessageIterator<ColorPropagationMessageValue> const& messages) {
  VectorOfColors newColors;
  for (auto const& colorPropagationMessageValue : messages) {
    if (colorPropagationMessageValue->equivalenceClass ==
        vertexState->equivalenceClass) {
      continue;
    }
    for (auto const& color : colorPropagationMessageValue->colors) {
      if (not vertexState->contains(color)) {
        newColors.push_back(color);
        vertexState->add(color);
      }
    }
  }
  return newColors;
}

}  // namespace

void ColorPropagationWorkerContext::postGlobalSuperstep(uint64_t gss) {
  // We have State::SendInitialColors only when GSS == 0.
  state = State::PropagateColors;
}

void ColorPropagationComputation::compute(
    MessageIterator<ColorPropagationMessageValue> const& messages) {
  auto const* ctx =
      dynamic_cast<ColorPropagationWorkerContext const*>(context());
  auto* vertexState = mutableVertexData();

  switch (ctx->state) {
    case State::SendInitialColors: {
      sendMessageToAllNeighbours(
          {vertexState->equivalenceClass, vertexState->getColors()});
    } break;

    case State::PropagateColors: {
      auto newColors = updateColors(vertexState, messages);
      if (newColors.empty()) {
        voteHalt();
      }
      sendMessageToAllNeighbours({vertexState->equivalenceClass, newColors});
    } break;
  }
}

VertexComputation<ColorPropagationValue, int8_t, ColorPropagationMessageValue>*
ColorPropagation::createComputation(WorkerConfig const* config) const {
  return new ColorPropagationComputation();
}

WorkerContext* ColorPropagation::workerContext(VPackSlice userParams) const {
  return new ColorPropagationWorkerContext(_maxGss, _numColors);
}

ColorPropagationWorkerContext::ColorPropagationWorkerContext(uint64_t maxGss,
                                                             uint16_t numColors)
    : numColors{numColors}, maxGss{maxGss} {}

CollectionIdType getEquivalenceClass(
    VPackSlice vertexDocument, std::string_view equivalenceClassFieldName) {
  auto eqClass = vertexDocument.get(equivalenceClassFieldName);
  if (eqClass.isNone() or not eqClass.isNumber()) {
    // will be ignored
    return ColorPropagationValue::none();
  }
  return eqClass.getNumber<CollectionIdType>();
}

/**
 * Get colors (natural numbers from 0 to numColors) from vertexDocument and
 * write them into senders.
 * @param senders the data of the vertex
 * @param vertexDocument the source of data
 * @param documentId the document id (needed for error messages)
 * @param numColors the total number of colors in the graph
 * @return
 */
Result getInitialColors(ColorPropagationValue& senders,
                        VPackSlice vertexDocument,
                        std::string_view colorsFieldName,
                        std::string_view documentId, uint16_t numColors) {
  senders.colors.resize(numColors);

  auto colorsDocument = vertexDocument.get(colorsFieldName);
  if (colorsDocument.isNone()) {
    // not all vertices must have an initial color
    return {TRI_ERROR_NO_ERROR};
  }

  if (colorsDocument.isNumber()) {
    auto color = colorsDocument.getNumber<PropagatedColor>();
    senders.add(color);
    return {TRI_ERROR_NO_ERROR};
  }
  if (not colorsDocument.isArray()) {
    return {
        TRI_ERROR_BAD_PARAMETER,
        fmt::format(
            "The colors field of the document with Id {} should be a list of "
            "numbers. It is {}",
            documentId, colorsDocument.toJson())};
  }

  for (auto const& colorDocument : velocypack::ArrayIterator(colorsDocument)) {
    auto const color = colorDocument.getNumber<uint16_t>();
    if (color >= numColors) {
      return {TRI_ERROR_BAD_PARAMETER,
              fmt::format("The colors field of the document with Id "
                          "{} should contain numbers in range [0,{}) where {} "
                          "is the total number of colors in the graph.",
                          documentId, numColors, numColors)};
    }
    senders.add(color);
  }
  return {TRI_ERROR_NO_ERROR};
}

void ColorPropagationGraphFormat::copyVertexData(
    arangodb::velocypack::Options const&, std::string const& documentId,
    arangodb::velocypack::Slice document, ColorPropagationValue& senders,
    uint64_t& vertexIdRange) {
  senders.equivalenceClass =
      getEquivalenceClass(document, equivalenceClassFieldName);

  auto result = getInitialColors(senders, document, inputColorsFieldName,
                                 documentId, numColors);
  if (result.fail()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
                                   result.errorMessage());
  }
}

#include "Logger/LogLevel.h"

bool ColorPropagationGraphFormat::buildVertexDocument(
    VPackBuilder& b, ColorPropagationValue const* ptr) const {
  auto result = ptr->getColors();
  // todo: Change the interface to serialize with the key
  b.add(VPackValue(outputColorsFieldName));
  serialize(b, result);
  return true;
}

GraphFormat<ColorPropagationValue, int8_t>* ColorPropagation::inputFormat()
    const {
  return new ColorPropagationGraphFormat(
      _server, _inputColorsFieldName, _outputColorsFieldName,
      _equivalenceClassFieldName, _numColors);
}

IAggregator* ColorPropagation::aggregator(std::string const& name) const {
  return new OverwriteAggregator<uint8_t>(
      static_cast<uint8_t>(State::SendInitialColors), true);
}
