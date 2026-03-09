////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2026 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Tobias Gödderz
////////////////////////////////////////////////////////////////////////////////

#include "BackendImpl.h"

#include "Basics/Result.h"
#include "Basics/StaticStrings.h"
#include "Basics/voc-errors.h"
#include "Inspection/JsonPrintInspector.h"
#include "Inspection/VPackLoadInspector.h"

#include <fuerte/types.h>
#include <velocypack/Buffer.h>
#include <velocypack/Exception.h>
#include <velocypack/Parser.h>

#include <sstream>

namespace arangodb::rbac {

BackendImpl::BackendImpl(network::ConnectionPool& pool,
                         std::string authorizationEndpoint)
    : _sendRequest([&pool](network::DestinationId const& dest,
                           fuerte::RestVerb verb, std::string const& path,
                           velocypack::Buffer<uint8_t> payload,
                           network::RequestOptions const& opts,
                           network::Headers headers) {
        return network::sendRequest(&pool, network::DestinationId{dest}, verb,
                                    std::string{path}, std::move(payload), opts,
                                    std::move(headers));
      }),
      _authorizationEndpoint(std::move(authorizationEndpoint)) {}

BackendImpl::BackendImpl(network::Sender sendRequest,
                         std::string authorizationEndpoint)
    : _sendRequest(std::move(sendRequest)),
      _authorizationEndpoint(std::move(authorizationEndpoint)) {}

namespace {

template<typename T>
auto buildJsonBody(T& value) -> ResultT<std::string> {
  std::ostringstream stream;
  // TODO The JsonPrintInspector does not (yet) escape strings!
  //      This needs to be resolved before merging, either by implementing
  //      escaping in the JsonPrintInspector, or by serializing into VelocyPack
  //      first, and then converting the VelocyPack to JSON.
  inspection::JsonPrintInspector<> inspector{
      stream, inspection::JsonPrintFormat::kMinimal};
  auto res = inspector.apply(value);
  if (!res.ok()) {
    // TODO Choose an appropriate error code, and possibly extend the message
    //      to clarify.
    return Result{TRI_ERROR_INTERNAL, res.error()};
  }
  return std::move(stream).str();
}

auto parseEvaluateResponseMany(std::string_view jsonBody)
    -> ResultT<Backend::EvaluateResponseMany> {
  // TODO We parse the JSON response into a VelocyPack first, and then
  //      use the VPackLoadInspector to convert it into a EvaluateResponseMany.
  //      This is not optimal, but we currently don't have an
  //      inspection::JsonLoadInspector.
  std::shared_ptr<velocypack::Builder> builder;
  try {
    builder = velocypack::Parser::fromJson(jsonBody);
  } catch (velocypack::Exception const& e) {
    return Result{
        // TODO Choose an appropriate error code, and possibly extend the
        //      message to clarify.
        TRI_ERROR_INTERNAL,
        std::string{"Failed to parse authorization response: "} + e.what()};
  }
  inspection::VPackLoadInspector<> inspector(builder->slice(),
                                             inspection::ParseOptions{});
  Backend::EvaluateResponseMany result{};
  auto res = inspector.apply(result);
  if (!res.ok()) {
    // TODO Choose an appropriate error code, and possibly extend the message
    //      to clarify.
    return Result{TRI_ERROR_INTERNAL, res.error()};
  }
  return result;
}

auto makeJsonRequestOptions(transaction::MethodsApi api) -> network::RequestOptions {
  return network::RequestOptions{
      .contentType = StaticStrings::MimeTypeJson,
      .acceptType = StaticStrings::MimeTypeJson,
      .skipScheduler = api == transaction::MethodsApi::Synchronous,
      .sendHLCHeader = false,
  };
}

auto jsonToPayload(std::string_view json) -> velocypack::Buffer<uint8_t> {
  velocypack::Buffer<uint8_t> payload;
  payload.append(reinterpret_cast<uint8_t const*>(json.data()), json.size());
  return payload;
}

}  // namespace

auto BackendImpl::evaluateTokenManyImpl(JwtToken const& token,
                                        RequestItems const& items,
                                        transaction::MethodsApi api)
    -> futures::Future<ResultT<EvaluateResponseMany>> {
  auto requestBody = EvaluateTokenManyRequest{
      .token = token.jwtToken,
      .items = items.items,
  };

  auto bodyResult = buildJsonBody(requestBody);
  if (!bodyResult.ok()) {
    co_return bodyResult.result();
  }

  auto response = co_await sendRequest(
      fuerte::RestVerb::Post,
      "/_integration/authorization/v1/evaluate-token-many", bodyResult.get(), api);

  if (auto result = response.combinedResult(); result.fail()) {
    co_return result;
  }

  co_return parseEvaluateResponseMany(
      response.response().payloadAsStringView());
}

auto BackendImpl::evaluateManyImpl(PlainUser const& user,
                                   RequestItems const& items,
                                   transaction::MethodsApi api)
    -> futures::Future<ResultT<EvaluateResponseMany>> {
  auto requestBody = EvaluateManyRequest{
      .user = user.username,
      .roles = user.roles,
      .items = items.items,
  };

  auto bodyResult = buildJsonBody(requestBody);
  if (!bodyResult.ok()) {
    co_return bodyResult.result();
  }

  auto response = co_await sendRequest(
      fuerte::RestVerb::Post, "/_integration/authorization/v1/evaluate-many",
      bodyResult.get(), api);

  if (auto result = response.combinedResult(); result.fail()) {
    co_return result;
  }

  co_return parseEvaluateResponseMany(
      response.response().payloadAsStringView());
}

auto BackendImpl::sendRequest(arangodb::fuerte::RestVerb verb, std::string path,
                              std::string_view payload, transaction::MethodsApi api)
    -> futures::Future<network::Response> {
  // TODO It's currently necessary to copy the data once, because the inspector
  //      can only write into an stream, but the network Methods only take a
  //      velocypack::Buffer as payload.
  auto fut = _sendRequest(_authorizationEndpoint, verb, std::move(path),
                      jsonToPayload(payload), makeJsonRequestOptions(api), {});
  if (api == transaction::MethodsApi::Synchronous) {
    // Wait here if the caller is synchronous, because in this case,
    // skipScheduler is set, and we must not execute arbitrary code on the
    // network thread. Waiting here will ensure that the current thread
    // continues execution after the following co_await, and not the network
    // thread resolving the promise.
    fut.wait();
  }
  return fut;
}

}  // namespace arangodb::rbac
