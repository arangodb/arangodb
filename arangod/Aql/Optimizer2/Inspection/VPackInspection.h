/* TODO: This is my space to prototype the necessary (de)serialisation
 * primitives using inspectors; This is also used in the Pregel refactor branch
 * and should be turned into a bona-fide PR for arangodb proper as soon as it is
 * more mature
 *
 * One goal of this code is to avoid using ArangoDB exceptions and macros such
 * as THROW because these tend to pull in the entirety of the ArangoDB codebase
 * into an executable.
 *
 * Of course the underlying situation should be addressed, but here we are. */

#pragma once

#include <Inspection/VPackLoadInspector.h>
#include <Inspection/VPackSaveInspector.h>

#include "StatusT.h"

namespace arangodb::velocypack {

template<typename T>
[[nodiscard]] auto serializeWithStatus(T& value)
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
[[nodiscard]] auto deserializeWithStatus(SharedSlice slice)
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
