#include "Agency/AsyncAgencyComm.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Futures/Utilities.h"
#include "Basics/StaticStrings.h"
#include "Scheduler/SchedulerFeature.h"
#include "Logger/LogMacros.h"

#include <chrono>

#include <velocypack/Iterator.h>
#include <velocypack/Buffer.h>
#include <velocypack/velocypack-aliases.h>

namespace {
using namespace arangodb::fuerte;
using namespace arangodb;

using namespace std::chrono_literals;

using clock = std::chrono::steady_clock;

struct RequestMeta {
  network::Timeout timeout;
  fuerte::RestVerb method;
  std::string url;
  std::vector<std::string> clientIds;
  network::Headers headers;
  clock::time_point startTime;
  unsigned tries;
};

bool agencyAsyncShouldCancel(RequestMeta &meta) {
  if (meta.tries++ > 20) {
    return true;
  }

  if (application_features::ApplicationServer::server().isStopping()) {
    return true;
  }

  return false;
}

bool agencyAsyncShouldTimeout(RequestMeta const& meta) {
  auto now = clock::now();
  return (now > meta.startTime + meta.timeout);
}

auto agencyAsyncWaitTime(RequestMeta const& meta) {
  if (meta.tries > 0) {
    return 250ms * (1l << meta.tries);
  }
  return 0ms;
}

std::string extractEndpointFromUrl(std::string const& location) {

  std::string specification;
  size_t delim = std::string::npos;

  if (location.substr(0, 7) == "http://") {
    specification = "http+tcp://" + location.substr(7);
    delim = specification.find_first_of('/', 12);
  } else if (location.substr(0, 8) == "https://") {
    specification = "http+ssl://" + location.substr(8);
    delim = specification.find_first_of('/', 13);
  }

  // invalid location header
  if (delim == std::string::npos) {
    return "";
  }

  return Endpoint::unifiedForm(specification.substr(0, delim));
}

void redirectOrError(AsyncAgencyCommManager& man, std::string const& endpoint, std::string const& location) {

  std::string newEndpoint = extractEndpointFromUrl(location);

  if (newEndpoint.empty()) {
    man.reportError(endpoint);
  } else {
    man.reportRedirect(endpoint, newEndpoint);
  }
}

arangodb::AsyncAgencyComm::FutureResult agencyAsyncSend(AsyncAgencyCommManager& man, RequestMeta&& meta, VPackBuffer<uint8_t>&& body);

arangodb::AsyncAgencyComm::FutureResult agencyAsyncInquiry(
  AsyncAgencyCommManager& man,
  RequestMeta&& meta,
  VPackBuffer<uint8_t>&& body
) {
  using namespace arangodb;

  // check for conditions to abort
  if (agencyAsyncShouldCancel(meta)) {
    return futures::makeFuture(AsyncAgencyCommResult{fuerte::Error::Canceled, nullptr});
  } else if (agencyAsyncShouldTimeout(meta)) {
    return futures::makeFuture(AsyncAgencyCommResult{fuerte::Error::Timeout, nullptr});
  }

  auto waitTime = agencyAsyncWaitTime(meta);
  return SchedulerFeature::SCHEDULER->delay(waitTime).thenValue(
    [meta = std::move(meta), &man, body = std::move(body)](auto){


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
    return network::sendRequest(man.pool(), endpoint, meta.method, "/_api/agency/inquire", std::move(query), opts, meta.headers).thenValue(
      [meta = std::move(meta), endpoint = std::move(endpoint), &man, body = std::move(body)]
        (network::Response &&result) mutable {

      auto &resp = result.response;

      switch (result.error) {
        case fuerte::Error::NoError:
          // handle inquiry response
          if (resp->statusCode() == fuerte::StatusNotFound) {
            return ::agencyAsyncSend (man, std::move(meta), std::move(body));
          }

          if (resp->statusCode() == fuerte::StatusTemporaryRedirect) {
            // get the Location header
            std::string const& location = resp->header.metaByKey(arangodb::StaticStrings::Location);
            redirectOrError (man, endpoint, location);
            return ::agencyAsyncInquiry (man, std::move(meta), std::move(body));
          }

          if (resp->statusCode() == fuerte::StatusOK) {
            break;
          }


          [[fallthrough]]
          /* fallthrough */
        case fuerte::Error::Timeout:
        case fuerte::Error::CouldNotConnect:
          // retry to send the request again
          man.reportError(endpoint);
          return agencyAsyncInquiry (man, std::move(meta), std::move(body));

        default:
          // return the result as is
          break;
      }

      return futures::makeFuture(
            AsyncAgencyCommResult{result.error, std::move(resp)});
    });
  });
}


arangodb::AsyncAgencyComm::FutureResult agencyAsyncSend(
  AsyncAgencyCommManager& man,
  RequestMeta&& meta,
  VPackBuffer<uint8_t>&& body
) {
  using namespace arangodb;

  // check for conditions to abort
  if (agencyAsyncShouldCancel(meta)) {
    return futures::makeFuture(AsyncAgencyCommResult{fuerte::Error::Canceled, nullptr});
  } else if (agencyAsyncShouldTimeout(meta)) {
    return futures::makeFuture(AsyncAgencyCommResult{fuerte::Error::Timeout, nullptr});
  }

  // after a possible small delay (if required)
  auto waitTime = agencyAsyncWaitTime(meta);
  return SchedulerFeature::SCHEDULER->delay(waitTime).thenValue(
    [meta = std::move(meta), &man, body = std::move(body)](auto) {

    // aquire the current endpoint
    std::string endpoint = man.getCurrentEndpoint();
    network::RequestOptions opts;
    opts.timeout = meta.timeout;

    // and fire off the request
    return network::sendRequest(man.pool(), endpoint, meta.method, meta.url, std::move(body), opts, meta.headers)
      .thenValue(
        [meta = std::move(meta), endpoint = std::move(endpoint), &man] (network::Response &&result) mutable {

      auto &req = result.request;
      auto &resp = result.response;
      auto &body = *req;

      switch (result.error) {
        case fuerte::Error::NoError:
          // success
          if ((resp->statusCode() >= 200 && resp->statusCode() <= 299)) {
            break;
          }
          // user error
          if ((400 <= resp->statusCode() && resp->statusCode() <= 499)) {
            break;
          }

          // 503 redirect
          if (resp->statusCode() == StatusTemporaryRedirect) {
            // get the Location header
            std::string const& location = resp->header.metaByKey(arangodb::StaticStrings::Location);
            redirectOrError (man, endpoint, location);

            // send again
            return ::agencyAsyncSend (man, std::move(meta), std::move(body).moveBuffer());
          }

          // if we only did reads return here
          if (meta.clientIds.size() == 0) {
            break;
          }

          [[fallthrough]]
          /* fallthrough */
        case fuerte::Error::Timeout:
          // inquiry the request
          man.reportError(endpoint);
          return ::agencyAsyncInquiry(man, std::move(meta), std::move(body).moveBuffer());

        case fuerte::Error::CouldNotConnect:
          // retry to send the request
          man.reportError(endpoint);
          return ::agencyAsyncSend (man, std::move(meta), std::move(body).moveBuffer());

        default:
          break;
      }

      // return the result as is
      return futures::makeFuture(
        AsyncAgencyCommResult{result.error, std::move(resp)});
    });
  });
}

}

namespace arangodb {

AsyncAgencyComm::FutureResult AsyncAgencyComm::sendWithFailover(
  fuerte::RestVerb method, std::string const& url,
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
    url, std::move(clientIds), std::move(headers), clock::now(), 0}), std::move(body));
}


AsyncAgencyComm::FutureResult AsyncAgencyComm::sendWithFailover(fuerte::RestVerb method, std::string const& url, network::Timeout timeout, AgencyTransaction const& trx) const {

  VPackBuffer<uint8_t> body;
  {
    VPackBuilder builder(body);
    VPackArrayBuilder arr(&builder);
    trx.toVelocyPack(builder);
  }

  return sendWithFailover(method, url, timeout, std::move(body));
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

const char * AGENCY_URL_READ = "/_api/agency/read";
const char * AGENCY_URL_WRITE = "/_api/agency/write";

AsyncAgencyComm::FutureResult AsyncAgencyComm::getValues(std::string const& path) const {
  return sendWithFailover(fuerte::RestVerb::Post, AGENCY_URL_READ, 120s, AgencyReadTransaction(path));
}

AsyncAgencyComm::FutureReadResult AsyncAgencyComm::getValues(std::shared_ptr<arangodb::cluster::paths::Path const> const& path) const {

  return sendWithFailover(fuerte::RestVerb::Post, AGENCY_URL_READ, 120s, AgencyReadTransaction(path->str()))
    .thenValue([path](AsyncAgencyCommResult &&result) {
      if (result.ok() && result.statusCode() == fuerte::StatusOK) {
        return futures::makeFuture(
          AgencyReadResult{std::move(result), std::move(path)});
      }

      return futures::makeFuture(
        AgencyReadResult{std::move(result), nullptr});
    });
}

AsyncAgencyComm::FutureResult AsyncAgencyComm::sendTransaction(network::Timeout timeout, AgencyReadTransaction const& trx) const {
  return sendWithFailover(fuerte::RestVerb::Post, AGENCY_URL_READ, timeout, trx);
}

AsyncAgencyComm::FutureResult AsyncAgencyComm::sendTransaction(network::Timeout timeout, AgencyWriteTransaction const& trx) const {
  return sendWithFailover(fuerte::RestVerb::Post, AGENCY_URL_WRITE, timeout, trx);
}


AsyncAgencyComm::FutureResult AsyncAgencyComm::sendWriteTransaction(network::Timeout timeout, velocypack::Buffer<uint8_t>&& body) const {
  return sendWithFailover(fuerte::RestVerb::Post, AGENCY_URL_WRITE, timeout, std::move(body));
}

AsyncAgencyComm::FutureResult AsyncAgencyComm::sendReadTransaction(network::Timeout timeout, velocypack::Buffer<uint8_t>&& body) const {
  return sendWithFailover(fuerte::RestVerb::Post, AGENCY_URL_READ, timeout, std::move(body));
}

AsyncAgencyComm::FutureResult AsyncAgencyComm::deleteKey(network::Timeout timeout, std::shared_ptr<arangodb::cluster::paths::Path const> const& path) const {
  return deleteKey(timeout, path->str());
}

AsyncAgencyComm::FutureResult AsyncAgencyComm::deleteKey(network::Timeout timeout, std::string const& path) const {
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

}
