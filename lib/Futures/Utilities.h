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
/// @author Lars Maier
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_FUTURES_UTILITIES_H
#define ARANGOD_FUTURES_UTILITIES_H 1
#include <mellon/utilities.h>
#include "Future.h"

namespace arangodb::futures {

template <typename T>
auto collectAll(std::vector<Future<T>>& v) {
  return mellon::collect(v.begin(), v.end()).and_then([](auto&& r) mutable noexcept {
    // and now wrap this into a expected (this is cheap because of temporaries)
    return Try<std::vector<Try<T>>>(std::forward<decltype(r)>(r));
  });
}

template <typename... Fs, typename R = std::tuple<Try<Fs>...>>
auto collect(Future<Fs>&&... fs) {
  // TODO maybe we want to extend this function to allow to accept temporary objects
  return mellon::collect(std::move(fs)...).and_then([](auto&& r) mutable noexcept {
    // and now wrap this into a expected (this is cheap because of temporaries)
    return Try<R>(std::forward<decltype(r)>(r));
  });
}

template <typename T>
auto collectAll(std::vector<Future<T>>&& v) {
  return mellon::collect(v.begin(), v.end()).and_then([](auto&& r) mutable noexcept {
    // and now wrap this into a expected (this is cheap because of temporaries)
    return Try<std::vector<Try<T>>>(std::forward<decltype(r)>(r));
  });
}

}  // namespace arangodb::futures

#endif
