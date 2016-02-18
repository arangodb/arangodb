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
/// @author Dr. Frank Celler
////////////////////////////////////////////////////////////////////////////////

#include "PathHandler.h"

#include "Basics/FileUtils.h"
#include "Basics/Logger.h"
#include "Basics/mimetypes.h"
#include "Basics/StringBuffer.h"
#include "Basics/StringUtils.h"
#include "Rest/HttpRequest.h"
#include "Rest/HttpResponse.h"

using namespace arangodb::basics;

namespace arangodb {
namespace rest {

// -----------------------------------------------------------------------------
// constructors and destructors
// -----------------------------------------------------------------------------

PathHandler::PathHandler(HttpRequest* request, Options const* options)
    : HttpHandler(request),
      path(options->path),
      contentType(options->contentType),
      allowSymbolicLink(options->allowSymbolicLink),
      defaultFile(options->defaultFile),
      cacheMaxAge(options->cacheMaxAge),
      maxAgeHeader("max-age=") {
  std::string::size_type pos = path.size();

  while (1 < pos && path[pos - 1] == '/') {
    path.erase(--pos);
  }

  maxAgeHeader += StringUtils::itoa(cacheMaxAge);
}

// -----------------------------------------------------------------------------
// Handler methods
// -----------------------------------------------------------------------------

HttpHandler::status_t PathHandler::execute() {
  static std::string const allowed =
      "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ01234567890. +-_=";

  std::vector<std::string> names = _request->suffix();
  std::string name = path;
  std::string last = "";

  if (names.empty() && !defaultFile.empty()) {
    std::string url = _request->requestPath();

    if (!url.empty() && url[url.size() - 1] != '/') {
      url += '/';
    }
    url += defaultFile;

    createResponse(HttpResponse::MOVED_PERMANENTLY);

    _response->setHeader(TRI_CHAR_LENGTH_PAIR("location"), url);
    _response->setContentType("text/html");

    _response->body().appendText(
        "<html><head><title>Moved</title></head><body><h1>Moved</h1><p>This "
        "page has moved to <a href=\"");
    _response->body().appendText(url);
    _response->body().appendText(">");
    _response->body().appendText(url);
    _response->body().appendText("</a>.</p></body></html>");

    return status_t(HANDLER_DONE);
  }

  for (std::vector<std::string>::const_iterator j = names.begin();
       j != names.end(); ++j) {
    std::string const& next = *j;

    if (next == ".") {
      LOG(WARN) << "file '" << name.c_str() << "' contains '.'";

      createResponse(HttpResponse::FORBIDDEN);
      _response->body().appendText("path contains '.'");
      return status_t(HANDLER_DONE);
    }

    if (next == "..") {
      LOG(WARN) << "file '" << name.c_str() << "' contains '..'";

      createResponse(HttpResponse::FORBIDDEN);
      _response->body().appendText("path contains '..'");
      return status_t(HANDLER_DONE);
    }

    std::string::size_type sc = next.find_first_not_of(allowed);

    if (sc != std::string::npos) {
      LOG(WARN) << "file '" << name.c_str() << "' contains illegal character";

      createResponse(HttpResponse::FORBIDDEN);
      _response->body().appendText("path contains illegal character '" +
                                   std::string(1, next[sc]) + "'");
      return status_t(HANDLER_DONE);
    }

    if (!path.empty()) {
      if (!FileUtils::isDirectory(path)) {
        LOG(WARN) << "file '" << name.c_str() << "' not found";

        createResponse(HttpResponse::NOT_FOUND);
        _response->body().appendText("file not found");
        return status_t(HANDLER_DONE);
      }
    }

    name += "/" + next;
    last = next;

    if (!allowSymbolicLink && FileUtils::isSymbolicLink(name)) {
      LOG(WARN) << "file '" << name.c_str() << "' contains symbolic link";

      createResponse(HttpResponse::FORBIDDEN);
      _response->body().appendText("symbolic links are not allowed");
      return status_t(HANDLER_DONE);
    }
  }

  if (!FileUtils::isRegularFile(name)) {
    LOG(WARN) << "file '" << name.c_str() << "' not found";

    createResponse(HttpResponse::NOT_FOUND);
    _response->body().appendText("file not found");
    return status_t(HANDLER_DONE);
  }

  createResponse(HttpResponse::OK);

  try {
    FileUtils::slurp(name, _response->body());
  } catch (...) {
    LOG(WARN) << "file '" << name.c_str() << "' not readable";

    createResponse(HttpResponse::NOT_FOUND);
    _response->body().appendText("file not readable");
    return status_t(HANDLER_DONE);
  }

  // check if we should use caching and this is an HTTP GET request
  if (cacheMaxAge > 0 &&
      _request->requestType() == HttpRequest::HTTP_REQUEST_GET) {
    // yes, then set a pro-caching header
    _response->setHeader(TRI_CHAR_LENGTH_PAIR("cache-control"), maxAgeHeader);
  }

  std::string::size_type d = last.find_last_of('.');

  if (d != std::string::npos) {
    std::string suffix = last.substr(d + 1);

    if (suffix.size() > 0) {
      // look up the mimetype
      char const* mimetype = TRI_GetMimetype(suffix.c_str());

      if (mimetype != 0) {
        _response->setContentType(mimetype);

        return status_t(HANDLER_DONE);
      }
    } else {
      // note: changed the log level to debug. an unknown content-type does not
      // justify a warning
      LOG(TRACE) << "unknown suffix '" << suffix.c_str() << "'";
    }
  }

  _response->setContentType(contentType);

  return status_t(HANDLER_DONE);
}

void PathHandler::handleError(const Exception&) {
  createResponse(HttpResponse::SERVER_ERROR);
}
}
}
