////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Andrey Abramov
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "analysis/token_streams.hpp"
#include "store/store_utils.hpp"

#include "Geo/GeoJson.h"
#include "IResearch/GeoAnalyzer.h"
#include "IResearch/VelocyPackHelper.h"

#include <velocypack/Slice.h>

namespace arangodb {
namespace tests {

struct StringField final {
  std::string_view name() const { return fieldName; }

  irs::token_stream& get_tokens() const {
    stream.reset(value);
    return stream;
  }

  bool write(irs::data_output& out) const {
    irs::write_string(out, value);
    return true;
  }

  const irs::features_t& features() const noexcept { return _featuresRange; }

  irs::IndexFeatures index_features() const noexcept {
    return irs::IndexFeatures::NONE;
  }

  mutable irs::string_token_stream stream;
  std::string_view value;
  std::string_view fieldName;
  irs::features_t _featuresRange;
};

struct GeoField final {
  std::string_view name() const { return fieldName; }

  irs::token_stream& get_tokens() const {
    if (!shapeSlice.isNone()) {
      stream.reset(iresearch::ref<char>(shapeSlice));
    }
    return stream;
  }

  bool write(irs::data_output& out) const {
    if (!shapeSlice.isNone()) {
      out.write_bytes(shapeSlice.start(), shapeSlice.byteSize());
    }
    return true;
  }

  const irs::features_t& features() const noexcept { return _featuresRange; }

  irs::IndexFeatures index_features() const noexcept {
    return irs::IndexFeatures::NONE;
  }

  mutable iresearch::GeoVPackAnalyzer stream{{}};
  VPackSlice shapeSlice;
  std::string_view fieldName;
  irs::features_t _featuresRange;
};

}  // namespace tests
}  // namespace arangodb
