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
/// @author Markus Pfeiffer
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include "Inspection/VPack.h"
#include "Pregel/SenderMessage.h"
#include "Pregel/SenderMessageFormat.h"

namespace arangodb::pregel {

using CollectionIdType = uint16_t;
using ColorType = uint16_t;
using PropagatedColor = uint16_t;
using VectorOfColors = std::vector<PropagatedColor>;

struct ColorPropagationValue {
  CollectionIdType equivalenceClass;
  std::vector<bool> colors;

  static CollectionIdType none() {
    return std::numeric_limits<CollectionIdType>::max();
  }

  [[nodiscard]] bool contains(ColorType color) const {
    // todo assert that color < numColors
    return colors[color];
  }

  void add(ColorType color) {
    // todo assert that color < numColors
    colors[color] = true;
  }

  [[nodiscard]] VectorOfColors getColors() const {
    VectorOfColors result;
    auto const size = colors.size();
    result.reserve(size);
    for (size_t color = 0; color < size; ++color) {
      if (contains(static_cast<ColorType>(color))) {
        result.push_back(static_cast<decltype(result)::value_type>(color));
      }
    }
    result.shrink_to_fit();
    return result;
  }
};

struct ColorPropagationMessageValue {
  CollectionIdType equivalenceClass = 0;
  std::vector<PropagatedColor> colors;
};

template<typename Inspector>
auto inspect(Inspector& f, ColorPropagationMessageValue& x) {
  return f.object(x).fields(
      f.field(Utils::equivalenceClass, x.equivalenceClass),
      f.field(Utils::colors, x.colors));
}

struct ColorPropagationUserParameters {
  uint64_t maxGss;
  uint16_t numColors;
  std::string inputColorsFieldName;
  std::string outputColorsFieldName;
  std::string equivalenceClassFieldName;
};

template<typename Inspector>
auto inspect(Inspector& f, ColorPropagationUserParameters& x) {
  return f.object(x).fields(
      f.field(Utils::maxGSS, x.maxGss), f.field(Utils::numColors, x.numColors),
      f.field(Utils::inputColorsFieldName, x.inputColorsFieldName),
      f.field(Utils::outputColorsFieldName, x.outputColorsFieldName),
      f.field(Utils::equivalenceClass, x.equivalenceClassFieldName));
}

struct ColorPropagationValueMessageFormat
    : public MessageFormat<ColorPropagationMessageValue> {
  ColorPropagationValueMessageFormat() = default;
  void unwrapValue(VPackSlice s,
                   ColorPropagationMessageValue& value) const override {
    value = arangodb::velocypack::deserialize<ColorPropagationMessageValue>(s);
  }
  void addValue(VPackBuilder& arrayBuilder,
                ColorPropagationMessageValue const& value) const override {
    arangodb::velocypack::serialize(arrayBuilder, value);
  }
};
}  // namespace arangodb::pregel
