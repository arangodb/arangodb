////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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
#include <velocypack/velocypack-aliases.h>

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/StringUtils.h"
#include "GeneralServer/ServerSecurityFeature.h"
#include "Logger/LogBufferFeature.h"
#include "Logger/Logger.h"
#include "Logger/LoggerFeature.h"
#include "Utils/ExecContext.h"

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::rest;

RestAdminLogHandler::RestAdminLogHandler(arangodb::application_features::ApplicationServer& server,
                                         GeneralRequest* request, GeneralResponse* response)
    : RestBaseHandler(server, request, response) {}

arangodb::Result RestAdminLogHandler::verifyPermitted() {
  auto& loggerFeature = server().getFeature<arangodb::LoggerFeature>();

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

  size_t const len = _request->suffixes().size();
  if (len == 0) {
    reportLogs();
  } else {
    setLogLevel();
  }

  return RestStatus::DONE;
}

void RestAdminLogHandler::reportLogs() {
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
    if (logLevel == "fatal" || logLevel == "0") {
      ul = LogLevel::FATAL;
    } else if (logLevel == "error" || logLevel == "1") {
      ul = LogLevel::ERR;
    } else if (logLevel == "warning" || logLevel == "2") {
      ul = LogLevel::WARN;
    } else if (logLevel == "info" || logLevel == "3") {
      ul = LogLevel::INFO;
    } else if (logLevel == "debug" || logLevel == "4") {
      ul = LogLevel::DEBUG;
    } else if (logLevel == "trace" || logLevel == "5") {
      ul = LogLevel::TRACE;
    } else {
      generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                    std::string("unknown '") + (found2 ? "level" : "upto") +
                        "' log level: '" + logLevel + "'");
      return;
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

  // check the sort direction
  bool sortAscending = true;
  std::string sortdir = StringUtils::tolower(_request->value("sort", found));

  if (found) {
    if (sortdir == "desc") {
      sortAscending = false;
    }
  }

  // check the search criteria
  bool search = false;
  std::string searchString = StringUtils::tolower(_request->value("search", search));

  // generate result
  std::vector<LogBuffer> entries = server().getFeature<LogBufferFeature>().entries(ul, start, useUpto);
  std::vector<LogBuffer> clean;

  if (search) {
    for (auto const& entry : entries) {
      std::string text = StringUtils::tolower(entry._message);

      if (text.find(searchString) == std::string::npos) {
        continue;
      }

      clean.emplace_back(entry);
    }
  } else {
    clean.swap(entries);
  }

  if (!sortAscending) {
    std::reverse(clean.begin(), clean.end());
  }

  VPackBuilder result;
  result.add(VPackValue(VPackValueType::Object));
  size_t length = clean.size();

  try {
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

    // For now we build the arrays one ofter the other first id
    result.add("lid", VPackValue(VPackValueType::Array));

    for (size_t i = 0; i < length; ++i) {
      try {
        auto& buf = clean.at(i + static_cast<size_t>(offset));
        result.add(VPackValue(buf._id));
      } catch (...) {
      }
    }

    result.close();

    result.add("topic", VPackValue(VPackValueType::Array));

    for (size_t i = 0; i < length; ++i) {
      try {
        auto& buf = clean.at(i + static_cast<size_t>(offset));
        result.add(VPackValue(LogTopic::lookup(buf._topicId)));
      } catch (...) {
      }
    }
    result.close();

    // second level
    result.add("level", VPackValue(VPackValueType::Array));

    for (size_t i = 0; i < length; ++i) {
      try {
        auto& buf = clean.at(i + static_cast<size_t>(offset));
        uint32_t l = 0;

        switch (buf._level) {
          case LogLevel::FATAL:
            l = 0;
            break;
          case LogLevel::ERR:
            l = 1;
            break;
          case LogLevel::WARN:
            l = 2;
            break;
          case LogLevel::DEFAULT:
          case LogLevel::INFO:
            l = 3;
            break;
          case LogLevel::DEBUG:
            l = 4;
            break;
          case LogLevel::TRACE:
            l = 5;
            break;
        }

        result.add(VPackValue(l));
      } catch (...) {
      }
    }

    result.close();

    // third timestamp
    result.add("timestamp", VPackValue(VPackValueType::Array));

    for (size_t i = 0; i < length; ++i) {
      try {
        auto& buf = clean.at(i + static_cast<size_t>(offset));
        result.add(VPackValue(static_cast<size_t>(buf._timestamp)));
      } catch (...) {
      }
    }

    result.close();

    // fourth text
    result.add("text", VPackValue(VPackValueType::Array));

    for (size_t i = 0; i < length; ++i) {
      try {
        auto& buf = clean.at(i + static_cast<size_t>(offset));
        result.add(VPackValue(buf._message));
      } catch (...) {
      }
    }

    result.close();

    result.close();  // Close the result object

    generateResult(rest::ResponseCode::OK, result.slice());
  } catch (...) {
    // Not Enough memory to build everything up
    // Has been ignored thus far
    // So ignore again
  }
}

void RestAdminLogHandler::setLogLevel() {
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
