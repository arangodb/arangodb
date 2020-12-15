////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2019 ArangoDB GmbH, Cologne, Germany
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

#include "Agency/AsyncAgencyComm.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/StaticStrings.h"
#include "Basics/StringUtils.h"
#include "Cluster/ClusterInfo.h"
#include "Futures/Utilities.h"
#include "Logger/LogMacros.h"
#include "Scheduler/SchedulerFeature.h"

#include <chrono>

#include <velocypack/Buffer.h>
#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

namespace {
using namespace arangodb::fuerte;
using namespace arangodb;

using namespace std::chrono_literals;

using clock = std::chrono::steady_clock;
using RequestType = arangodb::AsyncAgencyComm::RequestType;

struct RequestMeta {
  network::Timeout timeout;
  fuerte::RestVerb method;
  RequestType type;
  std::string url;
  std::vector<std::string> clientIds;
  network::Headers headers;
  clock::time_point startTime;
  uint64_t requestId;
  bool skipScheduler;
  unsigned tries;

  [[nodiscard]] bool isRetryOnNoResponse() const {
    return type == RequestType::READ;
  }

  [[nodiscard]] bool isInquiryOnNoResponse() const {
    return type == RequestType::WRITE && !clientIds.empty();
  }
};

bool agencyAsyncShouldCancel(AsyncAgencyCommManager& manager, RequestMeta& meta) {
  if (meta.tries++ > 20) {
    return true;
  }

  return manager.isStopping();
}

bool agencyAsyncShouldTimeout(RequestMeta const& meta) {
  auto now = clock::now();
  return (now > (meta.startTime + meta.timeout));
}

auto agencyAsyncWaitTime(RequestMeta const& meta) {
  clock::duration timeout = 0ms;
  if (meta.tries > 0) {
    timeout = 1ms * clock::duration::rep(static_cast<uint64_t>(1) << meta.tries);
  }
  // only wait as long as the timeout allows
  return std::min(timeout, (meta.startTime +
                            std::chrono::duration_cast<clock::duration>(meta.timeout)) -
                               clock::now());
}

std::string extractEndpointFromUrl(std::string const& location) {
  std::string specification;
  size_t delim = std::string::npos;

  if (location.compare(0, 7, "http://", 7) == 0) {
    specification = "http+tcp://" + location.substr(7);
    delim = specification.find_first_of('/', 12);
  } else if (location.compare(0, 8, "https://", 8) == 0) {
    specification = "http+ssl://" + location.substr(8);
    delim = specification.find_first_of('/', 13);
  }

  // invalid location header
  if (delim == std::string::npos) {
    return {};
  }

  return Endpoint::unifiedForm(specification.substr(0, delim));
}

void redirectOrError(AsyncAgencyCommManager& man, std::string const& endpoint,
                     std::string const& location) {
  std::string newEndpoint = extractEndpointFromUrl(location);

  if (newEndpoint.empty()) {
    man.reportError(endpoint);
  } else {
    man.reportRedirect(endpoint, newEndpoint);
  }
}

auto agencyAsyncWait(RequestMeta const& meta, clock::duration d)
    -> futures::Future<futures::Unit> {
  if (d == clock::duration::zero()) {
    return futures::makeFuture();
  }

  if (!meta.skipScheduler) {
    return SchedulerFeature::SCHEDULER->delay(d);
  }

  std::this_thread::sleep_for(d);
  return futures::makeFuture();
}

arangodb::AsyncAgencyComm::FutureResult agencyAsyncSend(AsyncAgencyCommManager& man,
                                                        RequestMeta&& meta,
                                                        VPackBuffer<uint8_t>&& body);

arangodb::AsyncAgencyComm::FutureResult agencyAsyncInquiry(AsyncAgencyCommManager& man,
                                                           RequestMeta&& meta,
                                                           VPackBuffer<uint8_t>&& body) {
  using namespace arangodb;

  // check for conditions to abort
  auto waitTime = agencyAsyncWaitTime(meta);
  if (agencyAsyncShouldCancel(man, meta)) {
    LOG_TOPIC("aac82", TRACE, Logger::AGENCYCOMM)
        << "agencyAsyncSend [" << meta.requestId << "] "
        << "abort because cancelled";
    return futures::makeFuture(AsyncAgencyCommResult{Error::Canceled, nullptr});
  } else if (agencyAsyncShouldTimeout(meta)) {
    LOG_TOPIC("aac81", TRACE, Logger::AGENCYCOMM)
        << "agencyAsyncSend [" << meta.requestId << "] "
        << "abort because timeout";
    return futures::makeFuture(AsyncAgencyCommResult{Error::Timeout, nullptr});
  }

  LOG_TOPIC("aac79", TRACE, Logger::AGENCYCOMM)
      << "agencyAsyncInquiry [" << meta.requestId << "] " << to_string(meta.method)
      << " " << meta.url << " body: " << VPackSlice(body.data()).toJson() << " delay: "
      << std::chrono::duration_cast<std::chrono::milliseconds>(waitTime).count() << "ms"
      << " timeout: " << meta.timeout.count() << "s";

  auto future = agencyAsyncWait(meta, waitTime);
  return std::move(future).thenValue([meta = std::move(meta), &man,
                                      body = std::move(body)](auto) mutable {
    // build inquire request
    VPackBuffer<uint8_t> query;
    {
      VPackBuilder b(query);
      {
        VPackArrayBuilder ab(&b);
        for (auto const& i : meta.clientIds) {
          b.add(VPackValue(i));
        }
      }
    }

    std::string endpoint = man.getCurrentEndpoint();
    network::RequestOptions opts;
    opts.timeout = meta.timeout;
    opts.skipScheduler = meta.skipScheduler;

    auto future = network::sendRequest(man.pool(), endpoint, meta.method,
                                       "/_api/agency/inquire", std::move(query),
                                       opts, meta.headers);
    return std::move(future).thenValue(
        [meta = std::move(meta), endpoint = std::move(endpoint), &man,
         body = std::move(body)](network::Response&& result) mutable {
          auto& resp = result.response;

          switch (result.error) {
            case Error::NoError:
              // handle inquiry response
              if (resp->statusCode() == fuerte::StatusNotFound) {
                return ::agencyAsyncSend(man, std::move(meta), std::move(body));
              }

              if (resp->statusCode() == fuerte::StatusTemporaryRedirect) {
                // get the Location header
                std::string const& location =
                    resp->header.metaByKey(arangodb::StaticStrings::Location);
                redirectOrError(man, endpoint, location);
                return ::agencyAsyncInquiry(man, std::move(meta), std::move(body));
              }
              return futures::makeFuture(AsyncAgencyCommResult{result.error,
                                                               std::move(resp)});  // otherwise return error as is
            case Error::Canceled:
              if (man.server().isStopping()) {
                return futures::makeFuture(
                  AsyncAgencyCommResult{result.error, std::move(resp)});
              }
              [[fallthrough]];
            case Error::CouldNotConnect:
            case Error::ConnectionClosed:
            case Error::Timeout:
            case Error::ReadError:
            case Error::WriteError:
            case Error::ProtocolError:
            case Error::CloseRequested:
              // retry the request at different endpoint
              man.reportError(endpoint);

              [[fallthrough]];
              /* fallthrough */
            case Error::QueueCapacityExceeded:
              // retry the request after a timeout
              return agencyAsyncInquiry(man, std::move(meta), std::move(body));

            case Error::VstUnauthorized:
              return futures::makeFuture(AsyncAgencyCommResult{result.error,
                                                               std::move(resp)});  // unrecoverable error
          }

          ADB_UNREACHABLE;
        });
  });
}

arangodb::AsyncAgencyComm::FutureResult agencyAsyncSend(AsyncAgencyCommManager& man,
                                                        RequestMeta&& meta,
                                                        VPackBuffer<uint8_t>&& body) {
  using namespace arangodb;

  // check for conditions to abort
  auto waitTime = agencyAsyncWaitTime(meta);
  if (agencyAsyncShouldCancel(man, meta)) {
    LOG_TOPIC("aac84", TRACE, Logger::AGENCYCOMM)
        << "agencyAsyncSend [" << meta.requestId << "] "
        << "abort because cancelled";
    return futures::makeFuture(AsyncAgencyCommResult{Error::Canceled, nullptr});
  } else if (agencyAsyncShouldTimeout(meta)) {
    LOG_TOPIC("aac85", TRACE, Logger::AGENCYCOMM)
        << "agencyAsyncSend [" << meta.requestId << "] "
        << "abort because timeout";
    return futures::makeFuture(AsyncAgencyCommResult{Error::Timeout, nullptr});
  }

  LOG_TOPIC("aac80", TRACE, Logger::AGENCYCOMM)
      << "agencyAsyncSend [" << meta.requestId << "] " << to_string(meta.method)
      << " " << meta.url << " body: " << VPackSlice(body.data()).toJson() << " delay: "
      << std::chrono::duration_cast<std::chrono::milliseconds>(waitTime).count() << "ms"
      << " timeout: " << meta.timeout.count() << "s";

  // after a possible small delay (if required)
  auto future = agencyAsyncWait(meta, waitTime);
  return std::move(future).thenValue([meta = std::move(meta), &man,
                                      body = std::move(body)](auto) mutable {
    // acquire the current endpoint
    std::string endpoint = man.getCurrentEndpoint();
    network::RequestOptions opts;
    opts.timeout = meta.timeout;
    opts.skipScheduler = meta.skipScheduler;

    auto requestId = meta.requestId;

    LOG_TOPIC("aac89", DEBUG, Logger::AGENCYCOMM)
        << "agencyAsyncSend [" << requestId << "] delay done, sending request";

    // and fire off the request
    auto future = network::sendRequest(man.pool(), endpoint, meta.method, meta.url,
                                       std::move(body), opts, meta.headers);
    return std::move(future)
        .thenValue([meta = std::move(meta), endpoint = std::move(endpoint),
                    &man](network::Response&& result) mutable {
          LOG_TOPIC("aac83", TRACE, Logger::AGENCYCOMM)
              << "agencyAsyncSend [" << meta.requestId
              << "] request done, result: " << to_string(result.error);

          auto& req = result.request;
          auto& resp = result.response;

          switch (result.error) {
            case Error::NoError: {
              TRI_ASSERT(req != nullptr);

              LOG_TOPIC("aac88", TRACE, Logger::AGENCYCOMM)
                  << "agencyAsyncSend [" << meta.requestId
                  << "] communication successful, statusCode: " << resp->statusCode();

              // success
              if ((resp->statusCode() >= 200 && resp->statusCode() <= 299)) {
                return futures::makeFuture(
                    AsyncAgencyCommResult{result.error, std::move(resp)});
              }
              // user error
              if ((400 <= resp->statusCode() && resp->statusCode() <= 499)) {
                return futures::makeFuture(
                    AsyncAgencyCommResult{result.error, std::move(resp)});
              }

              // 503 redirect
              if (resp->statusCode() == StatusTemporaryRedirect) {
                // get the Location header
                std::string const& location =
                    resp->header.metaByKey(arangodb::StaticStrings::Location);
                redirectOrError(man, endpoint, location);

                // send again
                auto& body = *req;
                return ::agencyAsyncSend(man, std::move(meta), std::move(body).moveBuffer());
              }

              // if we only did reads return here
              if (meta.type == RequestType::READ) {
                return futures::makeFuture(
                    AsyncAgencyCommResult{result.error, std::move(resp)});
              }
            }

            [[fallthrough]];
            
            case Error::Canceled: {
              if (man.server().isStopping() || req == nullptr) {
                return futures::makeFuture(
                  AsyncAgencyCommResult{result.error, std::move(resp)});
              }
            }
            [[fallthrough]];

            case Error::Timeout:
            case Error::ConnectionClosed:
            case Error::ProtocolError:
            case Error::WriteError:
            case Error::ReadError:
            case Error::CloseRequested: {
              TRI_ASSERT(req != nullptr); 

              // inquire the request
              man.reportError(endpoint);
              // in case of a write transaction we have to do inquiry
              if (meta.isInquiryOnNoResponse()) {
                auto& body = *req;
                return ::agencyAsyncInquiry(man, std::move(meta),
                                            std::move(body).moveBuffer());
              } else if (!meta.isRetryOnNoResponse()) {
                // return error
                return futures::makeFuture(
                    AsyncAgencyCommResult{result.error, std::move(resp)});
              }
            }
            // otherwise just send again
            [[fallthrough]];

            case Error::CouldNotConnect: {
              LOG_TOPIC("aac90", TRACE, Logger::AGENCYCOMM)
                  << "agencyAsyncSend [" << meta.requestId << "] retry request soon";
              // retry to send the request
              man.reportError(endpoint);
              [[fallthrough]];
            }

            case Error::QueueCapacityExceeded: {
              TRI_ASSERT(req != nullptr);
              auto& body = *req;
              return ::agencyAsyncSend(man, std::move(meta),
                                       std::move(body).moveBuffer());  // retry always
            }

            case Error::VstUnauthorized: {
              return futures::makeFuture(
                AsyncAgencyCommResult{result.error,
                                      std::move(resp)});  // unrecoverable error
            }
          }

          ADB_UNREACHABLE;
        })
        .thenValue([requestId](AsyncAgencyCommResult&& result) {
          // return the result as is
          LOG_TOPIC("aac8a", DEBUG, Logger::AGENCYCOMM)
              << "agencyAsyncSend [" << requestId << "] finished agency request";
          return std::move(result);
        });
  });
}

}  // namespace

namespace arangodb {

AsyncAgencyComm::FutureResult AsyncAgencyComm::sendWithFailover(
    arangodb::fuerte::RestVerb method, std::string const& url, network::Timeout timeout,
    RequestType type, uint64_t index) const {
  std::vector<ClientId> clientIds;
  VPackBuffer<uint8_t> body;
  return sendWithFailover(method, url + "?index=" + std::to_string(index), timeout, type, clientIds, std::move(body));
}

AsyncAgencyComm::FutureResult AsyncAgencyComm::sendWithFailover(
    arangodb::fuerte::RestVerb method, std::string const& url, network::Timeout timeout,
    RequestType type, velocypack::Buffer<uint8_t>&& body) const {
  std::vector<ClientId> clientIds;
  return sendWithFailover(method, url, timeout, type, clientIds, std::move(body));
}

AsyncAgencyComm::FutureResult AsyncAgencyComm::sendWithFailover(
    arangodb::fuerte::RestVerb method, std::string const& url,
    network::Timeout timeout, RequestType type, std::vector<ClientId> clientIds,
    velocypack::Buffer<uint8_t>&& body) const {
  network::Headers headers;
  uint64_t requestId = _manager.nextRequestId();

  return agencyAsyncSend(_manager,
                         RequestMeta({timeout, method, type, url, std::move(clientIds),
                                      std::move(headers), clock::now(), requestId,
                                      _skipScheduler || _manager.getSkipScheduler(), 0}),
                         std::move(body))
      .then([requestId](futures::Try<AsyncAgencyCommResult>&& e) {
        if (e.hasException()) {
          LOG_TOPIC("aac8b", DEBUG, Logger::AGENCYCOMM)
              << "sendWithFailover [" << requestId << "] had exception during request";
          try {
            e.throwIfFailed();
          } catch (const std::exception& e) {
            LOG_TOPIC("aac8c", DEBUG, Logger::AGENCYCOMM)
                << "sendWithFailover [" << requestId << "] message: " << e.what();
            throw e;
          }
        }

        return std::move(e).get();
      });
}

AsyncAgencyComm::FutureResult AsyncAgencyComm::sendWithFailover(
    arangodb::fuerte::RestVerb method, std::string const& url,
    network::Timeout timeout, RequestType type, std::vector<ClientId> clientIds,
    AgencyTransaction const& trx) const {
  VPackBuffer<uint8_t> body;
  {
    VPackBuilder builder(body);
    VPackArrayBuilder arr(&builder);
    trx.toVelocyPack(builder);
  }

  return sendWithFailover(method, url, timeout, type, std::move(clientIds),
                          std::move(body));
}

void AsyncAgencyCommManager::addEndpoint(std::string const& endpoint) {
  std::unique_lock<std::mutex> guard(_lock);
  _endpoints.push_back(endpoint);
}

void AsyncAgencyCommManager::updateEndpoints(std::vector<std::string> const& endpoints) {
  std::unique_lock<std::mutex> guard(_lock);
  _endpoints.assign(endpoints.begin(), endpoints.end());
}

std::string AsyncAgencyCommManager::getCurrentEndpoint() {
  std::unique_lock<std::mutex> guard(_lock);
  TRI_ASSERT(!_endpoints.empty());
  return _endpoints.front();
}

void AsyncAgencyCommManager::reportError(std::string const& endpoint) {
  std::unique_lock<std::mutex> guard(_lock);
  TRI_ASSERT(!_endpoints.empty());
  LOG_TOPIC("aac42", TRACE, Logger::AGENCYCOMM)
      << "reportError(" << endpoint << "), endpoints = " << _endpoints;
  if (endpoint == _endpoints.front()) {
    _endpoints.pop_front();
    LOG_TOPIC("aac43", DEBUG, Logger::AGENCYCOMM)
        << "Error using endpoint " << endpoint << ", switching to "
        << _endpoints.front();
    _endpoints.push_back(endpoint);
  }
}

void AsyncAgencyCommManager::reportRedirect(std::string const& endpoint,
                                            std::string const& redirectTo) {
  std::unique_lock<std::mutex> guard(_lock);
  TRI_ASSERT(!_endpoints.empty());

  LOG_TOPIC("aac45", TRACE, Logger::AGENCYCOMM)
      << "reportRedirect(" << endpoint << ", " << redirectTo
      << "), endpoints = " << _endpoints;
  if (endpoint == _endpoints.front()) {
    LOG_TOPIC("aac46", DEBUG, Logger::AGENCYCOMM)
        << "Redirect using endpoint " << endpoint << ", switching to " << redirectTo;
    _endpoints.pop_front();
    _endpoints.erase(std::remove(_endpoints.begin(), _endpoints.end(), redirectTo),
                     _endpoints.end());
    _endpoints.push_back(endpoint);
    _endpoints.push_front(redirectTo);
  }
}

application_features::ApplicationServer& AsyncAgencyCommManager::server() {
  return _server;
}

AsyncAgencyCommManager::AsyncAgencyCommManager(application_features::ApplicationServer& server)
    : _server(server) {}

const char* AGENCY_URL_READ = "/_api/agency/read";
const char* AGENCY_URL_WRITE = "/_api/agency/write";
const char* AGENCY_URL_POLL = "/_api/agency/poll";

AsyncAgencyComm::FutureResult AsyncAgencyComm::getValues(std::string const& path) const {
  return sendTransaction(120s, AgencyReadTransaction(path));
}

AsyncAgencyComm::FutureResult AsyncAgencyComm::poll(
  network::Timeout timeout, uint64_t index) const {
  return sendPollTransaction(timeout, index);
}

AsyncAgencyComm::FutureReadResult AsyncAgencyComm::getValues(
    std::shared_ptr<arangodb::cluster::paths::Path const> const& path) const {
  return sendTransaction(120s, AgencyReadTransaction(path->str())).thenValue([path = path](AsyncAgencyCommResult&& result) mutable {
    if (result.ok() && result.statusCode() == fuerte::StatusOK) {
      return futures::makeFuture(AgencyReadResult{std::move(result), std::move(path)});
    }

    return futures::makeFuture(AgencyReadResult{std::move(result), nullptr});
  });
}

AsyncAgencyComm::FutureResult AsyncAgencyComm::sendTransaction(
    network::Timeout timeout, AgencyReadTransaction const& trx) const {
  std::vector<ClientId> clientIds;
  return sendWithFailover(fuerte::RestVerb::Post, AGENCY_URL_READ, timeout,
                          RequestType::READ, clientIds, trx);
}

AsyncAgencyComm::FutureResult AsyncAgencyComm::sendTransaction(
    network::Timeout timeout, AgencyWriteTransaction const& trx) const {
  std::vector<ClientId> clientIds;
  if (!trx.clientId.empty()) {
    clientIds.push_back(trx.clientId);
  }
  return sendWithFailover(fuerte::RestVerb::Post, AGENCY_URL_WRITE, timeout,
                          RequestType::WRITE, std::move(clientIds), trx);
}

AsyncAgencyComm::FutureResult AsyncAgencyComm::sendWriteTransaction(
    network::Timeout timeout, velocypack::Buffer<uint8_t>&& body) const {
  std::vector<std::string> clientIds;
  VPackSlice bodySlice(body.data());
  if (bodySlice.isArray()) {
    // In the writing case we want to find all transactions with client IDs
    // and remember these IDs:
    for (auto const query : VPackArrayIterator(bodySlice)) {
      if (query.isArray() && query.length() == 3 && query[0].isObject() &&
          query[2].isString()) {
        clientIds.push_back(query[2].copyString());
      }
    }
  }
  return sendWithFailover(fuerte::RestVerb::Post, AGENCY_URL_WRITE, timeout,
                          RequestType::WRITE, std::move(clientIds), std::move(body));
}

AsyncAgencyComm::FutureResult AsyncAgencyComm::sendReadTransaction(
    network::Timeout timeout, velocypack::Buffer<uint8_t>&& body) const {
  std::vector<ClientId> clientIds;
  return sendWithFailover(fuerte::RestVerb::Post, AGENCY_URL_READ, timeout,
                          RequestType::READ, clientIds, std::move(body));
}

AsyncAgencyComm::FutureResult AsyncAgencyComm::sendPollTransaction(
  network::Timeout timeout, uint64_t index) const {

  return sendWithFailover(
    fuerte::RestVerb::Get, AGENCY_URL_POLL, timeout, RequestType::READ, index);
}

AsyncAgencyComm::FutureResult AsyncAgencyComm::deleteKey(
    network::Timeout timeout,
    std::shared_ptr<arangodb::cluster::paths::Path const> const& path) const {
  return deleteKey(timeout, path->str());
}

AsyncAgencyComm::FutureResult AsyncAgencyComm::deleteKey(network::Timeout timeout,
                                                         std::string const& path) const {
  VPackBuffer<uint8_t> transaction;
  {
    VPackBuilder trxBuilder(transaction);
    VPackArrayBuilder env(&trxBuilder);
    {
      VPackArrayBuilder trx(&trxBuilder);
      {
        VPackObjectBuilder ops(&trxBuilder);
        {
          VPackObjectBuilder op(&trxBuilder, path);
          trxBuilder.add("op", VPackValue("delete"));
        }
      }
    }
  }

  return sendWriteTransaction(timeout, std::move(transaction));
}

std::unique_ptr<AsyncAgencyCommManager> AsyncAgencyCommManager::INSTANCE = nullptr;
AsyncAgencyCommManager& AsyncAgencyCommManager::getInstance() {
  if (INSTANCE) {
    return *INSTANCE;
  }

  THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_DISABLED,
                                 "Agency Comm Manager not initialized");
}

std::string AsyncAgencyCommManager::endpointsString() const {
  std::unique_lock<std::mutex> guard(_lock);
  return basics::StringUtils::join(_endpoints);
}

}  // namespace arangodb
