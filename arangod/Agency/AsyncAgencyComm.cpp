#include "Agency/AsyncAgencyComm.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Futures/Utilities.h"
#include "Basics/StaticStrings.h"

#include <velocypack/Iterator.h>
#include <velocypack/Buffer.h>
#include <velocypack/velocypack-aliases.h>

namespace {
using namespace arangodb::fuerte;
using namespace arangodb;

struct RequestMeta {
  network::Timeout timeout;
  fuerte::RestVerb method;
  std::string url;
  std::vector<std::string> clientIds;
  network::Headers headers;
};


arangodb::AsyncAgencyComm::FutureResult agencyAsyncInquiry(
  AsyncAgencyCommManager *man,
  RequestMeta&& meta,
  VPackBuffer<uint8_t>&& body
) {

  using namespace arangodb;

  if (application_features::ApplicationServer::server().isStopping()) {
    return futures::makeFuture(AsyncAgencyCommResult{fuerte::Error::Canceled, nullptr});
  }

  std::string endpoint = man->getCurrentEndpoint();
  auto *pool = man->pool();

  // build inquiry request
  VPackBuilder b;
  {
    VPackArrayBuilder ab(&b);
    for (auto const& i : meta.clientIds) {
      b.add(VPackValue(i));
    }
  }

  return network::sendRequest(pool, endpoint, meta.method, meta.url, std::move(body), meta.timeout, meta.headers).thenValue(
    [meta = std::move(meta), ep = std::move(endpoint), man]
      (network::Response &&result) mutable {

    auto &req = result.request;
    auto &resp = result.response;

    switch (result.error) {

      case fuerte::Error::Timeout:
      case fuerte::Error::CouldNotConnect:
        // retry to send the request
        man->reportError(ep);
        return agencyAsyncInquiry (man, std::move(meta), std::move(*req.get()).moveBuffer());

      case fuerte::Error::NoError:
        // handle inquiry response
        TRI_ASSERT(false);

      case fuerte::Error::Canceled:
      case fuerte::Error::VstUnauthorized:
      case fuerte::Error::ProtocolError:
      case fuerte::Error::QueueCapacityExceeded:
      case fuerte::Error::CloseRequested:
      case fuerte::Error::ConnectionClosed:
      case fuerte::Error::ReadError:
      case fuerte::Error::WriteError:
      default:
        // return the result as is
        return futures::makeFuture(
          AsyncAgencyCommResult{result.error, std::move(resp)});
    }
  });
}

arangodb::AsyncAgencyComm::FutureResult agencyAsyncSend(
  AsyncAgencyCommManager *man,
  RequestMeta&& meta,
  VPackBuffer<uint8_t>&& body
) {

  using namespace arangodb;

  if (application_features::ApplicationServer::server().isStopping()) {
    return futures::makeFuture(AsyncAgencyCommResult{fuerte::Error::Canceled, nullptr});
  }

  std::string endpoint = man->getCurrentEndpoint();
  auto *pool = man->pool();
  return network::sendRequest(pool, endpoint, meta.method, meta.url, std::move(body), meta.timeout, meta.headers).thenValue(
    [meta = std::move(meta), ep = std::move(endpoint), man]
    (network::Response &&result) mutable {

    auto &req = result.request;
    auto &resp = result.response;

    switch (result.error) {
      case fuerte::Error::CouldNotConnect:
        // retry to send the request
        man->reportError(ep);
        return agencyAsyncSend (man, std::move(meta), std::move(*req.get()).moveBuffer());

      case fuerte::Error::NoError:
        if ((400 <= resp->statusCode() && resp->statusCode() <= 499)) {
          return futures::makeFuture(
            AsyncAgencyCommResult{result.error, std::move(resp)});
        }

        if (resp->statusCode() == StatusServiceUnavailable) {
          std::string const& location = resp->header.metaByKey(arangodb::StaticStrings::Location);
          if (location.empty()) {
            man->reportError(ep);
          } else {
            man->reportRedirect(ep, location);
          }
          return agencyAsyncSend (man, std::move(meta), std::move(*req.get()).moveBuffer());
        }

        if (meta.clientIds.size() == 0) {
          return futures::makeFuture(
            AsyncAgencyCommResult{result.error, std::move(resp)});
        }

        /* fallthrough */
      case fuerte::Error::Timeout:
        // inquiry the request
        man->reportError(ep);
        return agencyAsyncInquiry(man, std::move(meta), std::move(*req.get()).moveBuffer());

      case fuerte::Error::Canceled:
      case fuerte::Error::VstUnauthorized:
      case fuerte::Error::ProtocolError:
      case fuerte::Error::QueueCapacityExceeded:
      case fuerte::Error::CloseRequested:
      case fuerte::Error::ConnectionClosed:
      case fuerte::Error::ReadError:
      case fuerte::Error::WriteError:
      default:
        // return the result as is
        return futures::makeFuture(
          AsyncAgencyCommResult{result.error, std::move(resp)});
    }
  });
}

}


namespace arangodb {

AsyncAgencyComm::FutureResult AsyncAgencyComm::sendWithFailover(
  fuerte::RestVerb method, std::string url,
  arangodb::network::Timeout timeout, VPackBuffer<uint8_t> &&body) const {

  std::vector<std::string> clientIds;
  VPackSlice bodySlice(body.data());
  if (bodySlice.isArray()) {
    // In the writing case we want to find all transactions with client IDs
    // and remember these IDs:
    for (auto const& query : VPackArrayIterator(bodySlice)) {
      if (query.isArray() && query.length() == 3 && query[0].isObject() &&
          query[2].isString()) {
        clientIds.push_back(query[2].copyString());
      }
    }
  }

  network::Headers headers;
  return agencyAsyncSend(_manager, RequestMeta({timeout, method,
    url, std::move(clientIds), std::move(headers)}), std::move(body));
}

void AsyncAgencyCommManager::addEndpoint(std::string const& endpoint) {
  {
    std::unique_lock<std::mutex> guard(_lock);
    _endpoints.push_back(endpoint);
  }
}

void AsyncAgencyCommManager::updateEndpoints(std::vector<std::string> const& endpoints) {
  {
    std::unique_lock<std::mutex> guard(_lock);
    _endpoints.clear();
    std::copy(endpoints.begin(), endpoints.end(),
              std::back_inserter(_endpoints));
  }
}

std::string AsyncAgencyCommManager::getCurrentEndpoint() {
  {
    std::unique_lock<std::mutex> guard(_lock);
    TRI_ASSERT(_endpoints.size() > 0);
    return _endpoints.front();
  }
};

void AsyncAgencyCommManager::reportError(std::string const& endpoint) {
  {
    std::unique_lock<std::mutex> guard(_lock);
    if (endpoint == _endpoints.front()) {
      _endpoints.pop_front();
      _endpoints.push_back(endpoint);
    }
  }
}

void AsyncAgencyCommManager::reportRedirect(std::string const& endpoint, std::string const& redirectTo) {
  {
    std::unique_lock<std::mutex> guard(_lock);
    if (endpoint == _endpoints.front()) {
      _endpoints.pop_front();
      _endpoints.erase(std::remove(_endpoints.begin(), _endpoints.end(), redirectTo),
                      _endpoints.end());
      _endpoints.push_back(endpoint);
      _endpoints.push_front(redirectTo);
    }
  }
}

std::unique_ptr<AsyncAgencyCommManager> AsyncAgencyCommManager::INSTANCE = nullptr;

}
