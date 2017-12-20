////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2017 ArangoDB GmbH, Cologne, Germany
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
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_IRESEARCH__APPLICATION_SERVER_HELPER_H
#define ARANGOD_IRESEARCH__APPLICATION_SERVER_HELPER_H 1

#include "ApplicationFeatures/ApplicationServer.h"

namespace arangodb {

namespace aql {

struct Function;
class AqlFunctionFeature;

} // aql

namespace iresearch {

bool addFunction(
  arangodb::aql::AqlFunctionFeature& functions,
  arangodb::aql::Function const& function
);

template<typename T>
T* getFeature(std::string const& name) {
  auto* feature = arangodb::application_features::ApplicationServer::lookupFeature(name);

  #ifdef ARANGODB_ENABLE_MAINTAINER_MODE
    return dynamic_cast<T*>(feature);
  #else
    return static_cast<T*>(feature);
  #endif
}

template<typename T>
T* getFeature() {
  return getFeature<T>(T::name());
}

arangodb::aql::Function const* getFunction(
  arangodb::aql::AqlFunctionFeature& functions,
  std::string const& name
);

} // iresearch
} // arangodb

#endif
