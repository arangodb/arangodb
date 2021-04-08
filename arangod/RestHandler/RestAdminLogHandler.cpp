////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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
/// @author Achim Brandt
////////////////////////////////////////////////////////////////////////////////

#include "RestAdminLogHandler.h"

#include <velocypack/Builder.h>
#include <velocypack/Iterator.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/StringUtils.h"
#include "Basics/conversions.h"
#include "Cluster/ClusterFeature.h"
#include "Cluster/ServerState.h"
#include "GeneralServer/AuthenticationFeature.h"
#include "GeneralServer/ServerSecurityFeature.h"
#include "Logger/LogBufferFeature.h"
#include "Logger/Logger.h"
#include "Logger/LoggerFeature.h"
#include "Network/Methods.h"
#include "Network/NetworkFeature.h"
#include "Network/Utils.h"
#include "Utils/ExecContext.h"

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::rest;

namespace {
network::Headers buildHeaders(std::unordered_map<std::string, std::string> const& originalHeaders) {
  auto auth = AuthenticationFeature::instance();

  network::Headers headers;
  if (auth != nullptr && auth->isActive()) {
    headers.try_emplace(StaticStrings::Authorization,
                        "bearer " + auth->tokenCache().jwtToken());
  }
  for (auto& header : originalHeaders) {
    headers.try_emplace(header.first, header.second);
  }
  return headers;
}
}  // namespace

RestAdminLogHandler::RestAdminLogHandler(arangodb::application_features::ApplicationServer& server,
                                         GeneralRequest* request, GeneralResponse* response)
    : RestBaseHandler(server, request, response) {}

arangodb::Result RestAdminLogHandler::verifyPermitted() {
  auto& loggerFeature = server().getFeature<arangodb::LoggerFeature>();

  if (!loggerFeature.isAPIEnabled()) {
    return arangodb::Result(TRI_ERROR_HTTP_FORBIDDEN, "log API is disabled");
  }

  // do we have admin rights (if rights are active)
  if (loggerFeature.onlySuperUser()) {
    if (!ExecContext::current().isSuperuser()) {
      return arangodb::Result(
          TRI_ERROR_HTTP_FORBIDDEN, "you need super user rights for log operations");
    } // if
  } else {
    if (!ExecContext::current().isAdminUser()) {
      return arangodb::Result(
          TRI_ERROR_HTTP_FORBIDDEN, "you need admin rights for log operations");
    } // if
  }

  return arangodb::Result();
}

RestStatus RestAdminLogHandler::execute() {
  auto result = verifyPermitted();
  if (!result.ok()) {
    generateError(
        rest::ResponseCode::FORBIDDEN, result.errorNumber(), result.errorMessage());
    return RestStatus::DONE;
  }

  auto const& suffixes = _request->suffixes();
  auto const type = _request->requestType();

  if (type == rest::RequestType::DELETE_REQ) {
    clearLogs();
  } else if (type == rest::RequestType::GET) {
    if (suffixes.empty()) {
      return reportLogs(/*newFormat*/ false);
    } else if (suffixes.size() == 1 && suffixes[0] == "entries") {
      return reportLogs(/*newFormat*/ true);
    } else if (suffixes.size() == 1 && suffixes[0] == "level") {
      handleLogLevel();
    } else {
      generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_SUPERFLUOUS_SUFFICES,
                    "superfluous suffix, expecting /_admin/log/entries");
    }
  } else {
    handleLogLevel();
  }

  return RestStatus::DONE;
}

void RestAdminLogHandler::clearLogs() {
  server().getFeature<LogBufferFeature>().clear();
  generateOk(rest::ResponseCode::OK, VPackSlice::emptyObjectSlice());
}

RestStatus RestAdminLogHandler::reportLogs(bool newFormat) {
  bool foundServerIdParameter;
  std::string const& serverId = _request->value("serverId", foundServerIdParameter);

  if (ServerState::instance()->isCoordinator() && foundServerIdParameter) {
    if (serverId != ServerState::instance()->getId()) {
      // not ourselves! - need to pass through the request
      auto& ci = server().getFeature<ClusterFeature>().clusterInfo();

      bool found = false;
      for (auto const& srv : ci.getServers()) {
        // validate if server id exists
        if (srv.first == serverId) {
          found = true;
          break;
        }
      }

      if (!found) {
        generateError(rest::ResponseCode::NOT_FOUND, TRI_ERROR_HTTP_BAD_PARAMETER,
                      std::string("unknown serverId supplied."));
        return RestStatus::DONE;
      }

      NetworkFeature const& nf = server().getFeature<NetworkFeature>();
      network::ConnectionPool* pool = nf.pool();
      if (pool == nullptr) {
        THROW_ARANGO_EXCEPTION(TRI_ERROR_SHUTTING_DOWN);
      }

      network::RequestOptions options;
      options.timeout = network::Timeout(30.0);
      options.database = _request->databaseName();
      options.parameters = _request->parameters();

      auto f = network::sendRequest(pool, "server:" + serverId, fuerte::RestVerb::Get,
                                    _request->requestPath(), VPackBuffer<uint8_t>{},
                                    options, buildHeaders(_request->headers()));
      return waitForFuture(std::move(f).thenValue(
          [self = std::dynamic_pointer_cast<RestAdminLogHandler>(
               shared_from_this())](network::Response const& r) {
            if (r.fail()) {
              self->generateError(r.combinedResult());
            } else {
              self->generateResult(rest::ResponseCode::OK, r.slice());
            }
            return RestStatus::DONE;
          }));
    }
  }

  // check the maximal log level to report
  bool found1;
  std::string const& upto = StringUtils::tolower(_request->value("upto", found1));

  bool found2;
  std::string const& lvl = StringUtils::tolower(_request->value("level", found2));

  LogLevel ul = LogLevel::INFO;
  bool useUpto = true;
  std::string logLevel;

  // prefer level over upto
  if (found2) {
    logLevel = lvl;
    useUpto = false;
  } else if (found1) {
    logLevel = upto;
    useUpto = true;
  }

  if (found1 || found2) {
    // translate from number into log level
    if (logLevel.size() == 1 && logLevel[0] >= '0' && logLevel[0] <= '5') {
      static_assert(static_cast<int>(LogLevel::FATAL) == 1);
      static_assert(static_cast<int>(LogLevel::ERR) == 2);
      static_assert(static_cast<int>(LogLevel::WARN) == 3);
      static_assert(static_cast<int>(LogLevel::INFO) == 4);
      static_assert(static_cast<int>(LogLevel::DEBUG) == 5);
      static_assert(static_cast<int>(LogLevel::TRACE) == 6);
      ul = static_cast<LogLevel>((logLevel[0] - '0') + 1);
    } else {
      bool isValid = Logger::translateLogLevel(logLevel, true, ul);
      if (!isValid) {
        generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                      std::string("unknown '") + (found2 ? "level" : "upto") +
                          "' log level: '" + logLevel + "'");
        return RestStatus::DONE;
      }
    }
  }

  // check the starting position
  uint64_t start = 0;

  bool found;
  std::string const& s = _request->value("start", found);

  if (found) {
    start = StringUtils::uint64(s);
  }

  // check the offset
  int64_t offset = 0;
  std::string const& o = _request->value("offset", found);

  if (found) {
    offset = StringUtils::int64(o);
  }

  // check the size
  uint64_t size = (uint64_t)-1;
  std::string const& si = _request->value("size", found);

  if (found) {
    size = StringUtils::uint64(si);
  }

  // check the search criteria
  std::string const& searchString = _request->value("search");
  // generate result
  std::vector<LogBuffer> entries = server().getFeature<LogBufferFeature>().entries(ul, start, useUpto, searchString);

  // check the sort direction
  std::string const& sortdir = StringUtils::tolower(_request->value("sort"));
  if (sortdir == "desc") {
    std::reverse(entries.begin(), entries.end());
  }

  VPackBuilder result;

  if (newFormat) {
    // new log format - introduced in 3.8.0.
    // this format is more intuitive and useful than the old format.
    // the new format because it groups all attributes of a message together 
    // in an object, whereas in the old format, the attributes of a message
    // were split into multiple top-level arrays (one array per attribute).
    size_t start = 0;
    if (offset > 0) {
      start += static_cast<uint64_t>(offset);
      start = std::min<size_t>(start, entries.size());
    }

    result.openObject();
    result.add("total", VPackValue(entries.size()));

    result.add("messages", VPackValue(VPackValueType::Array));
    for (size_t i = start; i < entries.size(); ++i) {
      if (i - start >= size) {
        // produced enough results
        break;
      }
      auto& buf = entries[i];

      result.openObject();
      result.add("id", VPackValue(buf._id));
      result.add("topic", VPackValue(LogTopic::lookup(buf._topicId)));
      LogLevel lvl = (buf._level == LogLevel::DEFAULT ? LogLevel::INFO : buf._level);
      result.add("level", VPackValue(Logger::translateLogLevel(lvl)));
      result.add("date", VPackValue(TRI_StringTimeStamp(buf._timestamp, Logger::getUseLocalTime())));
      result.add("message", VPackValue(buf._message));
      result.close();
    }

    result.close(); // messages
    result.close();
  } else {
    // old log format
    size_t length = entries.size();

    result.openObject();
    result.add("totalAmount", VPackValue(length));

    if (offset < 0) {
      offset = 0;
    } else if (offset >= static_cast<int64_t>(length)) {
      length = 0;
      offset = 0;
    } else if (offset > 0) {
      length -= static_cast<size_t>(offset);
    }

    // restrict to at most <size> elements
    if (length > size) {
      length = static_cast<size_t>(size);
    }

    // For now we build the arrays one after the other first id
    result.add("lid", VPackValue(VPackValueType::Array));

    for (size_t i = 0; i < length; ++i) {
      try {
        auto& buf = entries.at(i + static_cast<size_t>(offset));
        result.add(VPackValue(buf._id));
      } catch (...) {
      }
    }

    result.close();

    result.add("topic", VPackValue(VPackValueType::Array));

    for (size_t i = 0; i < length; ++i) {
      try {
        auto& buf = entries.at(i + static_cast<size_t>(offset));
        result.add(VPackValue(LogTopic::lookup(buf._topicId)));
      } catch (...) {
      }
    }
    result.close();

    // second level
    result.add("level", VPackValue(VPackValueType::Array));

    for (size_t i = 0; i < length; ++i) {
      try {
        auto& buf = entries.at(i + static_cast<size_t>(offset));

        if (buf._level == LogLevel::DEFAULT) {
          result.add(VPackValue(3)); // INFO
        } else {
          TRI_ASSERT(static_cast<uint32_t>(buf._level) > 0);
          result.add(VPackValue(static_cast<uint32_t>(buf._level) - 1)); 
        }
      } catch (...) {
      }
    }

    result.close();

    // third timestamp
    result.add("timestamp", VPackValue(VPackValueType::Array));

    for (size_t i = 0; i < length; ++i) {
      try {
        auto& buf = entries.at(i + static_cast<size_t>(offset));
        result.add(VPackValue(static_cast<size_t>(buf._timestamp)));
      } catch (...) {
      }
    }

    result.close();

    // fourth text
    result.add("text", VPackValue(VPackValueType::Array));

    for (size_t i = 0; i < length; ++i) {
      try {
        auto& buf = entries.at(i + static_cast<size_t>(offset));
        result.add(VPackValue(buf._message));
      } catch (...) {
      }
    }

    result.close();
  
    result.close();  // Close the result object
  } // format end

  generateResult(rest::ResponseCode::OK, result.slice());
  return RestStatus::DONE;
}

void RestAdminLogHandler::handleLogLevel() {
  std::vector<std::string> const& suffixes = _request->suffixes();

  // was validated earlier
  TRI_ASSERT(!suffixes.empty());

  if (suffixes[0] != "level") {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_SUPERFLUOUS_SUFFICES,
                  "superfluous suffix, expecting /_admin/log/level");
    return;
  }

  auto const type = _request->requestType();

  if (type == rest::RequestType::GET) {
    // report log level
    VPackBuilder builder;
    builder.openObject();
    auto const& levels = Logger::logLevelTopics();
    for (auto const& level : levels) {
      builder.add(level.first, VPackValue(Logger::translateLogLevel(level.second)));
    }
    builder.close();

    generateResult(rest::ResponseCode::OK, builder.slice());
  } else if (type == rest::RequestType::PUT) {
    // set log level
    bool parseSuccess = false;
    VPackSlice slice = this->parseVPackBody(parseSuccess);
    if (!parseSuccess) {
      return;  // error message generated in parseVPackBody
    }

    if (slice.isString()) {
      Logger::setLogLevel(slice.copyString());
    } else if (slice.isObject()) {
      for (auto it : VPackObjectIterator(slice)) {
        if (it.value.isString()) {
          std::string const l = it.key.copyString() + "=" + it.value.copyString();
          Logger::setLogLevel(l);
        }
      }
    }

    // now report current log level
    VPackBuilder builder;
    builder.openObject();
    auto const& levels = Logger::logLevelTopics();
    for (auto const& level : levels) {
      builder.add(level.first, VPackValue(Logger::translateLogLevel(level.second)));
    }
    builder.close();

    generateResult(rest::ResponseCode::OK, builder.slice());
  } else {
    // invalid method
    generateError(rest::ResponseCode::METHOD_NOT_ALLOWED, TRI_ERROR_HTTP_METHOD_NOT_ALLOWED);
  }
}
