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
/// @author Julia Volmer
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include "Futures/Utilities.h"
#include "Pregel/Connection/Connection.h"
#include "Pregel/ExecutionNumber.h"
#include "Pregel/Messaging/Aggregate.h"
#include "Pregel/Messaging/Message.h"
#include "Pregel/Messaging/MessageQueue.h"

namespace arangodb::pregel::conductor {

template<Addable T>
struct WorkerApi {
  WorkerApi(ExecutionNumber executionNumber, std::vector<ServerID> servers,
            std::unique_ptr<Connection> connection)
      : _executionNumber{std::move(executionNumber)},
        _servers{std::move(servers)},
        _connection{std::move(connection)} {}
  template<Addable S>
  WorkerApi(WorkerApi<S>&& api)
      : _executionNumber{std::move(api._executionNumber)},
        _servers{std::move(api._servers)},
        _connection{std::move(api._connection)} {}
  WorkerApi() = default;
  template<Addable S>
  auto operator=(WorkerApi<S>&& other) -> WorkerApi<T>& {
    _executionNumber = std::move(other._executionNumber);
    _servers = std::move(other._servers);
    _connection = std::move(other._connection);
    return *this;
  }

  static auto create(ExecutionNumber executionNumber,
                     std::unique_ptr<Connection> connection) -> WorkerApi<T> {
    return WorkerApi<T>(std::move(executionNumber), {}, std::move(connection));
  }

  template<typename In>
  auto sendToAll(In const& in) -> Result {
    auto requests = std::vector<futures::Future<Result>>{};
    for (auto&& server : _servers) {
      requests.emplace_back(_connection->post(
          Destination{Destination::Type::server, server},
          ModernMessage{.executionNumber = _executionNumber, .payload = {in}}));
    }
    auto responses = futures::collectAll(requests).get();
    for (auto const& response : responses) {
      if (response.get().fail()) {
        return Result{response.get().errorNumber(),
                      fmt::format("Got unsuccessful response from worker "
                                  "after sending message {}: {} ",
                                  velocypack::serialize(in).toJson(),
                                  response.get().errorMessage())};
      }
    }
    _aggregate = Aggregate<T>::withComponentsCount(responses.size());
    return Result{};
  }

  template<typename In>
  auto send(std::unordered_map<ServerID, In> const& in) -> Result {
    auto requests = std::vector<futures::Future<Result>>{};
    for (auto&& [server, message] : in) {
      // TODO check that server is inside _servers
      requests.emplace_back(
          _connection->post(Destination{Destination::Type::server, server},
                            ModernMessage{.executionNumber = _executionNumber,
                                          .payload = {message}}));
    }
    auto responses = futures::collectAll(requests).get();
    for (auto const& response : responses) {
      if (response.get().fail()) {
        return Result{response.get().errorNumber(),
                      fmt::format("Got unsuccessful response from worker "
                                  "after sending messages {}: {} ",
                                  velocypack::serialize(in).toJson(),
                                  response.get().errorMessage())};
      }
    }
    _aggregate = Aggregate<T>::withComponentsCount(responses.size());
    return Result{};
  }

  template<Addable Out, typename In>
  auto requestFromAll(In const& in) -> futures::Future<ResultT<Out>> {
    auto results = std::vector<futures::Future<ResultT<Out>>>{};
    for (auto&& server : _servers) {
      results.emplace_back(
          _connection
              ->send(Destination{Destination::Type::server, server},
                     ModernMessage{.executionNumber = _executionNumber,
                                   .payload = {in}})
              .thenValue([](auto&& response) -> futures::Future<ResultT<Out>> {
                if (response.fail()) {
                  return Result{response.errorNumber(),
                                response.errorMessage()};
                }
                if (!std::holds_alternative<ResultT<Out>>(
                        response.get().payload)) {
                  return Result{TRI_ERROR_INTERNAL,
                                "Message from worker does not include the "
                                "expecte type"};
                }
                return std::get<ResultT<Out>>(response.get().payload);
              }));
    }
    return futures::collectAll(results).thenValue(
        [in = std::move(in)](auto responses) -> ResultT<Out> {
          auto out = Out{};
          for (auto const& response : responses) {
            if (response.get().fail()) {
              return Result{response.get().errorNumber(),
                            fmt::format("Got unsuccessful response from worker "
                                        "after sending message {}: {}",
                                        velocypack::serialize(in).toJson(),
                                        response.get().errorMessage())};
            }
            out.add(response.get().get());
          }
          return out;
        });
  }

  auto queue(MessagePayload message) -> Result {
    if (!std::holds_alternative<ResultT<T>>(message)) {
      return Result{TRI_ERROR_INTERNAL, "Received unexpected message type"};
    }
    _queue.push(std::get<ResultT<T>>(message));
    return Result{};
  }

  auto aggregateAllResponses() -> ResultT<T> {
    if (_servers.size() == 0) {
      return T{};
    }
    auto message = _queue.pop();
    if (message.fail()) {
      return Result{
          message.errorNumber(),
          fmt::format("Got unsuccessful message: {}", message.errorMessage())};
    }
    auto aggregate = _aggregate.aggregate(message.get());
    if (aggregate.has_value()) {
      return aggregate.value();
    }
    return aggregateAllResponses();
  }

  ExecutionNumber _executionNumber;
  std::vector<ServerID> _servers = {};
  std::unique_ptr<Connection> _connection;

 private:
  Aggregate<T> _aggregate;
  MessageQueue<ResultT<T>> _queue;
};

struct VoidMessage {
  auto add(VoidMessage const& other) -> void{};
};

}  // namespace arangodb::pregel::conductor
