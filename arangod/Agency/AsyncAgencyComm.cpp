#include "Agency/AsyncAgencyComm.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Futures/Utilities.h"

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
  RequestMeta&& meta,
  VPackBuffer<uint8_t>&& body
) {

  using namespace arangodb;

  if (application_features::ApplicationServer::server().isStopping()) {
    return futures::makeFuture(AgencyAsyncResult{fuerte::Error::Canceled, nullptr});
  }

  std::string endpoint;
  AgencyCommManager::MANAGER->acquire(endpoint);
  auto *pool = AgencyCommManager::MANAGER->pool();

  // build inquiry request
  VPackBuilder b;
  {
    VPackArrayBuilder ab(&b);
    for (auto const& i : meta.clientIds) {
      b.add(VPackValue(i));
    }
  }

  return network::sendRequest(pool, endpoint, meta.method, meta.url, std::move(body), meta.timeout, meta.headers).thenValue(
    [meta = std::move(meta), ep = std::move(endpoint)]
      (network::Response &&result) mutable {

    auto &req = result.request;
    auto &resp = result.response;

    switch (result.error) {

      case fuerte::Error::Timeout:
      case fuerte::Error::CouldNotConnect:
        // retry to send the request
        AgencyCommManager::MANAGER->failed(ep);
        return agencyAsyncInquiry (std::move(meta), std::move(*req.get()).moveBuffer());

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
          AgencyAsyncResult{result.error, std::move(resp)});
    }
  });
}

arangodb::AsyncAgencyComm::FutureResult agencyAsyncSend(
  RequestMeta&& meta,
  VPackBuffer<uint8_t>&& body
) {

  using namespace arangodb;

  if (application_features::ApplicationServer::server().isStopping()) {
    return futures::makeFuture(AgencyAsyncResult{fuerte::Error::Canceled, nullptr});
  }

  std::string endpoint;
  AgencyCommManager::MANAGER->acquire(endpoint);
  auto *pool = AgencyCommManager::MANAGER->pool();
  return network::sendRequest(pool, endpoint, meta.method, meta.url, std::move(body), meta.timeout, meta.headers).thenValue(
    [meta = std::move(meta), ep = std::move(endpoint)]
    (network::Response &&result) mutable {

    auto &req = result.request;
    auto &resp = result.response;

    switch (result.error) {
      case fuerte::Error::CouldNotConnect:
        // retry to send the request
        AgencyCommManager::MANAGER->failed(ep);
        return agencyAsyncSend (std::move(meta), std::move(*req.get()).moveBuffer());

      case fuerte::Error::NoError:
        if ((400 <= resp->statusCode() && resp->statusCode() <= 499) || resp->statusCode() == 503) {
          return futures::makeFuture(
            AgencyAsyncResult{result.error, std::move(resp)});
        }

        if (meta.clientIds.size() == 0) {
          return futures::makeFuture(
            AgencyAsyncResult{result.error, std::move(resp)});
        }

        /* fallthrough */
      case fuerte::Error::Timeout:
        // inquiry the request
        AgencyCommManager::MANAGER->failed(ep);
        return agencyAsyncInquiry(std::move(meta), std::move(*req.get()).moveBuffer());

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
          AgencyAsyncResult{result.error, std::move(resp)});
    }
  });
}

}


namespace arangodb {

AsyncAgencyComm::FutureResult AsyncAgencyComm::sendWithFailover(fuerte::RestVerb method, std::string url, arangodb::network::Timeout timeout, VPackBuffer<uint8_t> &&body) const {

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
  return agencyAsyncSend(RequestMeta({timeout, method, url, std::move(clientIds), std::move(headers)}), std::move(body));
}

}
