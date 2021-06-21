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
/// @author Andrey Abramov
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_TESTS_IRESEARCH__IRESEARCH_FIELDS_H
#define ARANGOD_TESTS_IRESEARCH__IRESEARCH_FIELDS_H 1

#include "analysis/token_streams.hpp"
#include "store/store_utils.hpp"

#include "Geo/GeoJson.h"
#include "IResearch/GeoAnalyzer.h"
#include "IResearch/VelocyPackHelper.h"

#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

namespace arangodb {
namespace tests {

struct StringField final {
  irs::string_ref name() const {
    return fieldName;
  }

  irs::token_stream& get_tokens() const {
    stream.reset(value);
    return stream;
  }

  bool write(irs::data_output& out) const {
    irs::write_string(out, value);
    return true;
  }

  irs::flags const& features() const {
    return irs::flags::empty_instance();
  }

  mutable irs::string_token_stream stream;
  irs::string_ref value;
  irs::string_ref fieldName;
};

struct GeoField final {
  irs::string_ref name() const {
    return fieldName;
  }

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

  irs::flags const& features() const {
    return irs::flags::empty_instance();
  }

  mutable iresearch::GeoJSONAnalyzer stream{{}};
  VPackSlice shapeSlice;
  irs::string_ref fieldName;
};

} // arangodb
} // tests

#endif // ARANGOD_TESTS_IRESEARCH__IRESEARCH_FIELDS_H
