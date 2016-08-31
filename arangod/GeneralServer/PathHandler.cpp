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
#include "Basics/StringBuffer.h"
#include "Basics/StringUtils.h"
#include "Basics/mimetypes.h"
#include "Logger/Logger.h"
#include "Rest/HttpRequest.h"
#include "Rest/HttpResponse.h"

using namespace arangodb::basics;

static std::string const AllowedChars =
    "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ01234567890. +-_=";

namespace arangodb {
namespace rest {

// -----------------------------------------------------------------------------
// constructors and destructors
// -----------------------------------------------------------------------------

PathHandler::PathHandler(GeneralRequest* request, GeneralResponse* response,
                         Options const* options)
    : RestHandler(request, response),
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

RestHandler::status PathHandler::execute() {
  // TODO needs to handle VPP
  auto response = dynamic_cast<HttpResponse*>(_response.get());

  if (response == nullptr) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_INTERNAL);
  }

  std::vector<std::string> const& names = _request->suffix();
  std::string name = path;
  std::string last;

  if (names.empty() && !defaultFile.empty()) {
    std::string url = _request->requestPath();

    if (!url.empty() && url[url.size() - 1] != '/') {
      url += '/';
    }
    url += defaultFile;

    resetResponse(rest::ResponseCode::MOVED_PERMANENTLY);

    response->setHeaderNC(StaticStrings::Location, url);
    response->setContentType(rest::ContentType::HTML);

    response->body().appendText(
        "<html><head><title>Moved</title></head><body><h1>Moved</h1><p>This "
        "page has moved to <a href=\"");
    response->body().appendText(url);
    response->body().appendText(">");
    response->body().appendText(url);
    response->body().appendText("</a>.</p></body></html>");

    return status::DONE;
  }

  for (std::vector<std::string>::const_iterator j = names.begin();
       j != names.end(); ++j) {
    std::string const& next = *j;

    if (next == ".") {
      LOG(WARN) << "file '" << name << "' contains '.'";

      resetResponse(rest::ResponseCode::FORBIDDEN);
      response->body().appendText("path contains '.'");
      return status::DONE;
    }

    if (next == "..") {
      LOG(WARN) << "file '" << name << "' contains '..'";

      resetResponse(rest::ResponseCode::FORBIDDEN);
      response->body().appendText("path contains '..'");
      return status::DONE;
    }

    std::string::size_type sc = next.find_first_not_of(AllowedChars);

    if (sc != std::string::npos) {
      LOG(WARN) << "file '" << name << "' contains illegal character";

      resetResponse(rest::ResponseCode::FORBIDDEN);
      response->body().appendText("path contains illegal character '" +
                                  std::string(1, next[sc]) + "'");
      return status::DONE;
    }

    if (!path.empty()) {
      if (!FileUtils::isDirectory(path)) {
        LOG(WARN) << "file '" << name << "' not found";

        resetResponse(rest::ResponseCode::NOT_FOUND);
        response->body().appendText("file not found");
        return status::DONE;
      }
    }

    name += "/" + next;
    last = next;

    if (!allowSymbolicLink && FileUtils::isSymbolicLink(name)) {
      LOG(WARN) << "file '" << name << "' contains symbolic link";

      resetResponse(rest::ResponseCode::FORBIDDEN);
      response->body().appendText("symbolic links are not allowed");
      return status::DONE;
    }
  }

  if (!FileUtils::isRegularFile(name)) {
    LOG(WARN) << "file '" << name << "' not found";

    resetResponse(rest::ResponseCode::NOT_FOUND);
    response->body().appendText("file not found");
    return status::DONE;
  }

  resetResponse(rest::ResponseCode::OK);

  try {
    FileUtils::slurp(name, response->body());
  } catch (...) {
    LOG(WARN) << "file '" << name << "' not readable";

    resetResponse(rest::ResponseCode::NOT_FOUND);
    response->body().appendText("file not readable");
    return status::DONE;
  }

  // check if we should use caching and this is an HTTP GET request
  if (cacheMaxAge > 0 &&
      _request->requestType() == rest::RequestType::GET) {
    // yes, then set a pro-caching header
    response->setHeaderNC(StaticStrings::CacheControl, maxAgeHeader);
  }

  std::string::size_type d = last.find_last_of('.');

  if (d != std::string::npos) {
    std::string suffix = last.substr(d + 1);

    if (suffix.size() > 0) {
      // look up the mimetype
      char const* mimetype = TRI_GetMimetype(suffix.c_str());

      if (mimetype != nullptr) {
        response->setContentType(mimetype);

        return status::DONE;
      }
    } else {
      // note: changed the log level to debug. an unknown content-type does not
      // justify a warning
      LOG(TRACE) << "unknown suffix '" << suffix << "'";
    }
  }

  response->setContentType(contentType);

  return status::DONE;
}

void PathHandler::handleError(Exception const&) {
  resetResponse(rest::ResponseCode::SERVER_ERROR);
}
}
}
