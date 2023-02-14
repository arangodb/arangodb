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

#pragma once

#include "Pregel/Algorithm.h"
#include "Pregel/CommonFormats.h"
#include "Pregel/VertexComputation.h"
#include "Pregel/Algos/ColorPropagation/ColorPropagationValue.h"

namespace arangodb::pregel::algos {

/**
 * Propagates colors to the successors in other collections until a fixed point
 * is reached.
 *
 * Requirements
 * -1. The vertex knows its successors from other collections.
 *    TODO: discuss how to assure that
 * 0. We know beforehand how many colors we have. The colors are natural numbers
 *    from a range [0, n-1] for some n > 0.
 * 1. The expected number of colors is probably not bigger than 50.
 *    TODO: clarify this
 *    However, we want to be able to work with a greater number of colors,
 *    up to 2^16.
 * 2. Memory efficiency. Rationale: each vertex has possibly several colors
 *    stored.
 * 3. Small amount of data transferred between vertices. Rationale: the number
 *    of communication rounds is probably the same for any sensible
 *    implementation up to a small additive constant. The next bottleneck
 *    is probably the amount of transferred data.
 *    TODO: test this
 * 4. Possibly fast local computations.
 *
 * Implementation
 * 1. We send only new colors, i.e. colors we obtained in the previous round
 *    (requirements 3, 4). We expect that the number of sent colors in an GSS
 *    is sufficiently small and thus send the colors as a vector of colors.
 *    Sending them in a bitvector (see Item 2) is not supported by VPack.
 * 2. Every vertex stores its current colors as a bit vector, we use
 *    std::vector<bool> for a concise representation.
 *    The alternatives could be:
 *    - An (ordered) vector of colors.
 *      - Disadvantage: This is not so space efficient if the
 *        number of colors to be stored is sufficiently big.
 *        TODO: compute the border
 *        We aim a possibly general solution.
 *      - Disadvantage: The update takes O(n * numColors * log numColors) where
 *        n is the sum of lengths of obtained vectors and numColors is the total
 *        number of colors instead of O(n) for bitvectors.
 *        (O(n * numColors * log numColors): for each color in each obtained
 *        vector, find the correct place on the local vector and shift all
 *        greater elements to teh right.)
 *      - Advantage: no need to convert the colors into a initial segment of
 *        natural numbers at the beginning.
 *    - A hash set:
 *      - Disadvantage: too space consuming.
 *      - Advantage: an easier implementation.
 *    - An ordered set (as a tree):
 *      - Disadvantage: too space consuming.
 *      - Disadvantage: The update takes O(n * log numColors).
 *      - Advantage: an easier implementation.
 * 3. For a normal GSS, we assume that
 *    - we have sent our current colors in the previous GSSs (not necessarily
 *      all current colors in the last one);
 *    - the colors are in the range [0, n - 1] for some positive n that is known
 *      to the vertices (n can be stored in the WorkerContext).
 *    To assure this, we make some preparations in the first GSS.
 *    - Send all initial colors to the conductor and to the successors from
 *      other collections.
 *    - After the first GSS, the conductor computes the total number of colors
 *      (say, n) and a mapping [0, n - 1] -> actual colors.
 *    - This mapping is sent to all Workers.
 * 4. In a normal GSS, we
 *    - update our current colors by adding new obtained colors to the local
 *      store;
 *    - send the new colors to all successors outside our own collection.
 */

struct ColorPropagation : public Algorithm<ColorPropagationValue, int8_t,
                                           ColorPropagationMessageValue> {
 public:
  explicit ColorPropagation(application_features::ApplicationServer& server,
                            VPackSlice userParams)
      : Algorithm<ColorPropagationValue, int8_t, ColorPropagationMessageValue>(
            server, "colorpropagation"),
        _numColors{getNumColors(userParams)},
        _inputColorsFieldName(getInputColorsFieldName(userParams)),
        _outputColorsFieldName(getOutputColorsFieldName(userParams)),
        _equivalenceClassFieldName(getEquivalenceClassFieldName(userParams)),
        _maxGss{getMaxGss(userParams)} {}

  [[nodiscard]] GraphFormat<ColorPropagationValue, int8_t>* inputFormat()
      const override;
  [[nodiscard]] ColorPropagationValueMessageFormat* messageFormat()
      const override {
    return new ColorPropagationValueMessageFormat();
  }

  VertexComputation<ColorPropagationValue, int8_t,
                    ColorPropagationMessageValue>*
  createComputation(WorkerConfig const*) const override;

  [[nodiscard]] WorkerContext* workerContext(
      VPackSlice userParams) const override;

  [[nodiscard]] IAggregator* aggregator(std::string const& name) const override;

 private:
  uint16_t const _numColors;
  std::string _inputColorsFieldName;
  std::string _outputColorsFieldName;
  std::string _equivalenceClassFieldName;
  uint64_t const _maxGss;

  static uint64_t getMaxGss(VPackSlice userParams) {
    return deserialize<ColorPropagationUserParameters>(userParams).maxGss;
  }
  static uint16_t getNumColors(VPackSlice userParams) {
    return deserialize<ColorPropagationUserParameters>(userParams).numColors;
  };

  static std::string getInputColorsFieldName(VPackSlice userParams) {
    try {
      return deserialize<ColorPropagationUserParameters>(userParams)
          .inputColorsFieldName;
    } catch (std::exception const& ex) {
      LOG_TOPIC("42cd7", ERR, Logger::PREGEL)
          << "Input colors field name is missing" << ex.what();
    } catch (...) {
      LOG_TOPIC("4cb72", ERR, Logger::PREGEL)
          << "Unknown error while parsing input color field name";
    }
    return "";
  }

  static std::string getOutputColorsFieldName(VPackSlice userParams) {
    try {
      return deserialize<ColorPropagationUserParameters>(userParams)
          .outputColorsFieldName;
    } catch (std::exception const& ex) {
      LOG_TOPIC("8faa4", ERR, Logger::PREGEL)
          << "Output colors field name is missing" << ex.what();
    } catch (...) {
      LOG_TOPIC("83eb8", ERR, Logger::PREGEL)
          << "Unknown error while parsing output color field name";
    }
    return "";
  }

  static std::string getEquivalenceClassFieldName(VPackSlice userParams) {
    return deserialize<ColorPropagationUserParameters>(userParams)
        .equivalenceClassFieldName;
  }
};

enum class State { SendInitialColors, PropagateColors };

struct ColorPropagationWorkerContext : public WorkerContext {
  ColorPropagationWorkerContext(uint64_t maxGss, uint16_t numColors);
  void postGlobalSuperstep(uint64_t gss) override;

  State state = State::SendInitialColors;
  uint16_t numColors;
  uint64_t maxGss;
};

struct ColorPropagationComputation
    : public VertexComputation<ColorPropagationValue, int8_t,
                               ColorPropagationMessageValue> {
  ColorPropagationComputation() = default;
  void compute(
      MessageIterator<ColorPropagationMessageValue> const& messages) override;
};

struct ColorPropagationGraphFormat
    : public GraphFormat<ColorPropagationValue, int8_t> {
  const std::string inputColorsFieldName;
  const std::string outputColorsFieldName;
  const std::string equivalenceClassFieldName;
  const uint16_t numColors;

  explicit ColorPropagationGraphFormat(
      application_features::ApplicationServer& server,
      std::string inputColorsFieldName, std::string outputColorsFieldName,
      std::string equivalenceClassFieldName, uint16_t numColors)
      : GraphFormat<ColorPropagationValue, int8_t>(server),
        inputColorsFieldName(std::move(inputColorsFieldName)),
        outputColorsFieldName(outputColorsFieldName),
        equivalenceClassFieldName(std::move(equivalenceClassFieldName)),
        numColors(numColors) {}

  [[nodiscard]] size_t estimatedEdgeSize() const override { return 0; }

  void copyVertexData(arangodb::velocypack::Options const&,
                      std::string const& documentId,
                      arangodb::velocypack::Slice document,
                      ColorPropagationValue& senders,
                      uint64_t& vertexIdRange) override;

  bool buildVertexDocument(arangodb::velocypack::Builder& b,
                           ColorPropagationValue const* ptr) const override;
};
}  // namespace arangodb::pregel::algos
