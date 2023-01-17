////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
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
/// @author Valery Mironov
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <s2/s2shape.h>

namespace arangodb::geo {

class LaxShapeContainer final {
 public:
  enum class Type : uint8_t {
    Empty = 0,
    S2Point = 1,
    S2Polyline = 2,
    S2Polygon = 4,
    S2MultiPoint = 5,
    S2MultiPolyline = 6,
  };

  LaxShapeContainer(LaxShapeContainer const& other) = delete;
  LaxShapeContainer& operator=(LaxShapeContainer const& other) = delete;

  LaxShapeContainer() noexcept = default;
  LaxShapeContainer(LaxShapeContainer&& other) noexcept
      : _data{std::move(other._data)},
        _type{std::exchange(other._type, Type::Empty)} {}
  LaxShapeContainer& operator=(LaxShapeContainer&& other) noexcept {
    std::swap(_data, other._data);
    std::swap(_type, other._type);
    return *this;
  }

  bool empty() const noexcept { return _type == Type::Empty; }

  S2Point centroid() const noexcept;

  S2Shape const* shape() const noexcept { return _data.get(); }
  Type type() const noexcept { return _type; }

  // Using s2 Encode/Decode
  bool Decode(Decoder& decoder);

 private:
  std::unique_ptr<S2Shape> _data{nullptr};
  Type _type{Type::Empty};
};

}  // namespace arangodb::geo
