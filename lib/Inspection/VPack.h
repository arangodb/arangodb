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
/// @author Manuel Pöter
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include "Basics/Exceptions.h"
#include "Basics/voc-errors.h"

#include "Inspection/VPackNoThrow.h"

namespace arangodb::velocypack {

template<class T>
void serialize(Builder& builder, T& value) {
  inspection::VPackSaveInspector<> inspector(builder);
  if (auto res = inspector.apply(value); !res.ok()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_INTERNAL,
        std::string{"Error while serializing to VelocyPack: "} + res.error() +
            "\nPath: " + res.path());
  }
}

namespace detail {

template<class Inspector, class T>
void deserialize(Inspector& inspector, T& result) {
  if (auto res = deserializeWithStatus(inspector, result); !res.ok()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_INTERNAL, std::string{"Error while parsing VelocyPack: "} +
                                res.error() + "\nPath: " + res.path());
  }
}

}  // namespace detail

}  // namespace arangodb::velocypack
