////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
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
/// @author Koichi Nakata
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Metrics/CounterBuilder.h"
#include "Metrics/FixScale.h"
#include "Metrics/HistogramBuilder.h"

#include <initializer_list>

namespace arangodb {

struct RequestStatisticsBytesReceivedScale {
  static metrics::FixScale<double> scale() {
    static std::initializer_list<double> const cuts{250, 1000, 2000, 5000,
                                                    10000};
    return {250, 10000, cuts};
  }
};

struct RequestStatisticsBytesSentScale {
  static metrics::FixScale<double> scale() {
    static std::initializer_list<double> const cuts{250, 1000, 2000, 5000,
                                                    10000};
    return {250, 10000, cuts};
  }
};

struct RequestStatisticsConnectionTimeScale {
  static metrics::FixScale<double> scale() {
    static std::initializer_list<double> const cuts{0.1, 1.0, 60.0};
    return {0.1, 60.0, cuts};
  }
};

struct RequestStatisticsRequestTimeScale {
  static metrics::FixScale<double> scale() {
    static std::initializer_list<double> const cuts{0.01, 0.05, 0.1,  0.2, 0.5,
                                                    1.0,  5.0,  15.0, 30.0};
    return {0.01, 30.0, cuts};
  }
};

DECLARE_HISTOGRAM(arangodb_client_connection_statistics_bytes_received,
                  RequestStatisticsBytesReceivedScale,
                  "Bytes received for requests");
DECLARE_HISTOGRAM(arangodb_client_connection_statistics_bytes_sent,
                  RequestStatisticsBytesSentScale, "Bytes sent for responses");
DECLARE_HISTOGRAM(arangodb_client_user_connection_statistics_bytes_received,
                  RequestStatisticsBytesReceivedScale,
                  "Bytes received for requests, only user traffic");
DECLARE_HISTOGRAM(arangodb_client_user_connection_statistics_bytes_sent,
                  RequestStatisticsBytesSentScale,
                  "Bytes sent for responses, only user traffic");
DECLARE_HISTOGRAM(arangodb_client_connection_statistics_connection_time,
                  RequestStatisticsConnectionTimeScale,
                  "Total connection time of a client");
DECLARE_HISTOGRAM(arangodb_client_connection_statistics_total_time,
                  RequestStatisticsRequestTimeScale,
                  "Total time needed to answer a request");
DECLARE_HISTOGRAM(arangodb_client_connection_statistics_request_time,
                  RequestStatisticsRequestTimeScale,
                  "Request time needed to answer a request");
DECLARE_HISTOGRAM(arangodb_client_connection_statistics_queue_time,
                  RequestStatisticsRequestTimeScale,
                  "Queue time needed to answer a request");
DECLARE_HISTOGRAM(arangodb_client_connection_statistics_io_time,
                  RequestStatisticsRequestTimeScale,
                  "IO time needed to answer a request");

DECLARE_COUNTER(arangodb_http_request_statistics_total_requests_total,
                "Total number of HTTP requests");
DECLARE_COUNTER(arangodb_http_request_statistics_superuser_requests_total,
                "Total number of HTTP requests executed by superuser/JWT");
DECLARE_COUNTER(arangodb_http_request_statistics_user_requests_total,
                "Total number of HTTP requests executed by clients");
DECLARE_COUNTER(arangodb_http_request_statistics_async_requests_total,
                "Number of asynchronously executed HTTP requests");
DECLARE_COUNTER(arangodb_http_request_statistics_http_delete_requests_total,
                "Number of HTTP DELETE requests");
DECLARE_COUNTER(arangodb_http_request_statistics_http_get_requests_total,
                "Number of HTTP GET requests");
DECLARE_COUNTER(arangodb_http_request_statistics_http_head_requests_total,
                "Number of HTTP HEAD requests");
DECLARE_COUNTER(arangodb_http_request_statistics_http_options_requests_total,
                "Number of HTTP OPTIONS requests");
DECLARE_COUNTER(arangodb_http_request_statistics_http_patch_requests_total,
                "Number of HTTP PATCH requests");
DECLARE_COUNTER(arangodb_http_request_statistics_http_post_requests_total,
                "Number of HTTP POST requests");
DECLARE_COUNTER(arangodb_http_request_statistics_http_put_requests_total,
                "Number of HTTP PUT requests");
DECLARE_COUNTER(arangodb_http_request_statistics_other_http_requests_total,
                "Number of other HTTP requests");

}  // namespace arangodb
