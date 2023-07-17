////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
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

#include "NodeLoadInspector.h"
#include "Basics/Exceptions.h"

namespace arangodb::consensus {

namespace detail {

template<class Inspector, class T>
[[nodiscard]] inspection::Status deserializeWithStatus(Inspector& inspector,
                                                       T& result) {
  return inspector.apply(result);
}

template<class Inspector, class T>
void deserialize(Inspector& inspector, T& result) {
  if (auto res = deserializeWithStatus(inspector, result); !res.ok()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_DESERIALIZE, std::string{"Error while parsing VelocyPack: "} +
                                   res.error() + "\nPath: " + res.path());
  }
}

template<class Inspector, class T>
void deserialize(Node const& node, T& result,
                 inspection::ParseOptions options) {
  Inspector inspector(&node, options);
  deserialize(inspector, result);
}

template<class Inspector, class T, class Context>
void deserialize(Node const& node, T& result, inspection::ParseOptions options,
                 Context const& context) {
  Inspector inspector(&node, options, context);
  deserialize(inspector, result);
}

template<class Inspector, class T>
T deserialize(Node const& node, inspection::ParseOptions options) {
  T result;
  detail::deserialize<Inspector, T>(node, result, options);
  return result;
}

template<class Inspector, class T, class Context>
T deserialize(Node const& node, inspection::ParseOptions options,
              Context const& context) {
  T result;
  detail::deserialize<Inspector, T>(node, result, options, context);
  return result;
}

template<class Inspector, class T>
[[nodiscard]] inspection::Status deserializeWithStatus(
    Node const& node, T& result, inspection::ParseOptions options) {
  Inspector inspector(&node, options);
  return deserializeWithStatus(inspector, result);
}

template<class Inspector, class T, class Context>
[[nodiscard]] inspection::Status deserializeWithStatus(
    Node const& node, T& result, inspection::ParseOptions options,
    Context const& context) {
  Inspector inspector(&node, options, context);
  return deserializeWithStatus(inspector, result);
}

}  // namespace detail

template<class T>
void deserialize(Node const& node, T& result,
                 inspection::ParseOptions options = {}) {
  detail::deserialize<inspection::NodeLoadInspector<>>(node, result, options);
}

template<class T, class Context>
void deserialize(Node const& node, T& result, inspection::ParseOptions options,
                 Context const& context) {
  detail::deserialize<inspection::NodeLoadInspector<Context>>(node, result,
                                                              options, context);
}

template<class T>
[[nodiscard]] inspection::Status deserializeWithStatus(
    Node const& node, T& result, inspection::ParseOptions options = {}) {
  return detail::deserializeWithStatus<inspection::NodeLoadInspector<>>(
      node, result, options);
}

template<class T, class Context>
[[nodiscard]] inspection::Status deserializeWithStatus(
    Node const& node, T& result, inspection::ParseOptions options,
    Context const& context) {
  return detail::deserializeWithStatus<inspection::NodeLoadInspector<Context>>(
      node, result, options, context);
}

template<class T>
void deserializeUnsafe(Node const& node, T& result,
                       inspection::ParseOptions options = {}) {
  detail::deserialize<inspection::NodeUnsafeLoadInspector<>>(node, result,
                                                             options);
}

template<class T, class Context>
void deserializeUnsafe(Node const& node, T& result,
                       inspection::ParseOptions options,
                       Context const& context) {
  detail::deserialize<inspection::NodeUnsafeLoadInspector<Context>>(
      node, result, options, context);
}

template<class T>
T deserialize(Node const& node, inspection::ParseOptions options = {}) {
  return detail::deserialize<inspection::NodeLoadInspector<>, T>(node, options);
}

template<class T, class Context>
T deserialize(Node const& node, inspection::ParseOptions options,
              Context const& context) {
  return detail::deserialize<inspection::NodeLoadInspector<Context>, T>(
      node, options, context);
}

template<class T>
T deserializeUnsafe(Node const& node, inspection::ParseOptions options = {}) {
  return detail::deserialize<inspection::NodeUnsafeLoadInspector<>, T>(node,
                                                                       options);
}

template<class T, class Context>
T deserializeUnsafe(Node const& node, inspection::ParseOptions options,
                    Context const& context) {
  return detail::deserialize<inspection::NodeUnsafeLoadInspector<Context>, T>(
      node, options, context);
}

}  // namespace arangodb::consensus
