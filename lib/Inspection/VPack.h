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

#include "Inspection/VPackLoadInspector.h"
#include "Inspection/VPackSaveInspector.h"

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
  if (auto res = inspector.apply(result); !res.ok()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_INTERNAL, std::string{"Error while parsing VelocyPack: "} +
                                res.error() + "\nPath: " + res.path());
  }
}

template<class Inspector, class T>
void deserialize(Slice slice, T& result, inspection::ParseOptions options) {
  Inspector inspector(slice, options);
  deserialize(inspector, result);
}

template<class Inspector, class T, class Context>
void deserialize(Slice slice, T& result, inspection::ParseOptions options,
                 Context const& context) {
  Inspector inspector(slice, options, context);
  deserialize(inspector, result);
}

template<class Inspector, class T>
T deserialize(Slice slice, inspection::ParseOptions options) {
  T result;
  detail::deserialize<Inspector, T>(slice, result, options);
  return result;
}

template<class Inspector, class T, class Context>
T deserialize(Slice slice, inspection::ParseOptions options,
              Context const& context) {
  T result;
  detail::deserialize<Inspector, T>(slice, result, options, context);
  return result;
}
}  // namespace detail

template<class T>
void deserialize(Slice slice, T& result,
                 inspection::ParseOptions options = {}) {
  detail::deserialize<inspection::VPackLoadInspector<>>(slice, result, options);
}

template<class T, class Context>
void deserialize(Slice slice, T& result, inspection::ParseOptions options,
                 Context const& context) {
  detail::deserialize<inspection::VPackLoadInspector<Context>>(
      slice, result, options, context);
}

template<class T>
void deserializeUnsafe(Slice slice, T& result,
                       inspection::ParseOptions options = {}) {
  detail::deserialize<inspection::VPackUnsafeLoadInspector<>>(slice, result,
                                                              options);
}

template<class T, class Context>
void deserializeUnsafe(Slice slice, T& result, inspection::ParseOptions options,
                       Context const& context) {
  detail::deserialize<inspection::VPackUnsafeLoadInspector<Context>>(
      slice, result, options, context);
}

template<class T>
T deserialize(Slice slice, inspection::ParseOptions options = {}) {
  return detail::deserialize<inspection::VPackLoadInspector<>, T>(slice,
                                                                  options);
}

template<class T, class Context>
T deserialize(Slice slice, inspection::ParseOptions options,
              Context const& context) {
  return detail::deserialize<inspection::VPackLoadInspector<Context>, T>(
      slice, options, context);
}

template<class T>
T deserializeUnsafe(Slice slice, inspection::ParseOptions options = {}) {
  return detail::deserialize<inspection::VPackUnsafeLoadInspector<>, T>(
      slice, options);
}

template<class T, class Context>
T deserializeUnsafe(Slice slice, inspection::ParseOptions options,
                    Context const& context) {
  return detail::deserialize<inspection::VPackUnsafeLoadInspector<Context>, T>(
      slice, options, context);
}

}  // namespace arangodb::velocypack
