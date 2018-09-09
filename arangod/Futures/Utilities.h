////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2018 ArangoDB GmbH, Cologne, Germany
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

#include "Future.h"

#ifndef ARANGOD_FUTURES_UTILITIES_H
#define ARANGOD_FUTURES_UTILITIES_H 1

namespace arangodb {
namespace futures {
    
template <class T>
Future<T> makeFuture(Try<T>&& t) {
  return Future<T>(detail::SharedState<T>::make(std::move(t)));
}

Future<void> makeFuture() {
  return Future<void>(detail::SharedState<void>::make());
}

template <class T>
Future<typename std::decay<T>::type> makeFuture(T&& t) {
  return makeFuture(Try<typename std::decay<T>::type>(std::forward<T>(t)));
}
  
/// Make a failed Future from an std::exception_ptr.
template <class T>
Future<T> makeFuture(std::exception_ptr const& e) {
  return makeFuture(Try<T>(e));
}

/// Make a Future from an exception type E that can be passed to
/// std::make_exception_ptr().
template <class T, class E>
typename std::enable_if<std::is_base_of<std::exception, E>::value, Future<T>>::type
makeFuture(E const& e) {
  return makeFuture(Try<T>(std::make_exception_ptr<E>(e)));
}

template <class F>
Future<std::result_of<F>> makeFutureWith(F&& f) {
  return makeFuture(std::move(makeTry(std::forward(f))));
}
    
}}
#endif // ARANGOD_FUTURES_UTILITIES_H
