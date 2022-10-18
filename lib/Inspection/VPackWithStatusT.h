////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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

#include <Inspection/VPackLoadInspector.h>
#include <Inspection/VPackSaveInspector.h>

#include "StatusT.h"

namespace arangodb::velocypack {

template<typename T>
[[nodiscard]] auto serializeWithStatusT(T& value)
    -> inspection::StatusT<SharedSlice> {
  auto builder = Builder();
  inspection::VPackSaveInspector<> inspector(builder);
  auto res = inspector.apply(value);

  if (res.ok()) {
    return inspection::StatusT<SharedSlice>::ok(
        std::move(builder).sharedSlice());
  } else {
    return inspection::StatusT<SharedSlice>::error(std::move(res));
  }
}

template<typename T>
[[nodiscard]] auto deserializeWithStatusT(SharedSlice slice)
    -> inspection::StatusT<T> {
  inspection::VPackLoadInspector<> inspector(slice.slice(),
                                             inspection::ParseOptions{});
  T data{};

  auto res = inspector.apply(data);
  if (res.ok()) {
    return inspection::StatusT<T>::ok(std::move(data));
  } else {
    return inspection::StatusT<T>::error(std::move(res));
  }
}

}  // namespace arangodb::velocypack
