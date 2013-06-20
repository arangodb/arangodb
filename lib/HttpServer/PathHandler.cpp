////////////////////////////////////////////////////////////////////////////////
/// @brief path handler
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2004-2013 triAGENS GmbH, Cologne, Germany
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
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Dr. Frank Celler
/// @author Copyright 2008-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "PathHandler.h"

#include <fstream>

#include "Basics/FileUtils.h"
#include "Logger/Logger.h"
#include "Basics/StringBuffer.h"
#include "Basics/StringUtils.h"
#include "BasicsC/mimetypes.h"
#include "Rest/HttpRequest.h"
#include "Rest/HttpResponse.h"

using namespace triagens::basics;

namespace triagens {
  namespace rest {

// -----------------------------------------------------------------------------
// constructors and destructors
// -----------------------------------------------------------------------------

    PathHandler::PathHandler (HttpRequest* request, Options const* options)
      : HttpHandler(request),
        path(options->path),
        contentType(options->contentType),
        allowSymbolicLink(options->allowSymbolicLink),
        defaultFile(options->defaultFile),
        cacheMaxAge(options->cacheMaxAge),
        maxAgeHeader("max-age=") {

      string::size_type pos = path.size();

      while (1 < pos && path[pos - 1] == '/') {
        path.erase(--pos);
      }

      maxAgeHeader += StringUtils::itoa(cacheMaxAge);
    }

// -----------------------------------------------------------------------------
    // Handler methods
// -----------------------------------------------------------------------------

    HttpHandler::status_e PathHandler::execute () {
      static string const allowed = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ01234567890. +-_=";

      vector<string> names = _request->suffix();
      string name = path;
      string last = "";

      if (names.empty() && ! defaultFile.empty()) {
        string url = _request->requestPath();

        if (! url.empty() && url[url.size() - 1] != '/') {
          url += '/';
        }
        url += defaultFile;

        _response = createResponse(HttpResponse::MOVED_PERMANENTLY);

        _response->setHeader("location", url);
        _response->setContentType("text/html");

        _response->body().appendText("<html><head><title>Moved</title></head><body><h1>Moved</h1><p>This page has moved to <a href=\"");
        _response->body().appendText(url);
        _response->body().appendText(">");
        _response->body().appendText(url);
        _response->body().appendText("</a>.</p></body></html>");

        return HANDLER_DONE;
      }

      for (vector<string>::const_iterator j = names.begin();  j != names.end();  ++j) {
        string const& next = *j;

        if (next == ".") {
          LOGGER_WARNING("file '" << name << "' contains '.'");

          _response = createResponse(HttpResponse::FORBIDDEN);
          _response->body().appendText("path contains '.'");
          return HANDLER_DONE;
        }

        if (next == "..") {
          LOGGER_WARNING("file '" << name << "' contains '..'");

          _response = createResponse(HttpResponse::FORBIDDEN);
          _response->body().appendText("path contains '..'");
          return HANDLER_DONE;
        }

        string::size_type sc = next.find_first_not_of(allowed);

        if (sc != string::npos) {
          LOGGER_WARNING("file '" << name << "' contains illegal character");

          _response = createResponse(HttpResponse::FORBIDDEN);
          _response->body().appendText("path contains illegal character '" + string(1, next[sc]) + "'");
          return HANDLER_DONE;
        }

        if (! path.empty()) {
          if (! FileUtils::isDirectory(path)) {
            LOGGER_WARNING("file '" << name << "' not found");

            _response = createResponse(HttpResponse::NOT_FOUND);
            _response->body().appendText("file not found");
            return HANDLER_DONE;
          }
        }

        name += "/" + next;
        last = next;

        if (! allowSymbolicLink && FileUtils::isSymbolicLink(name)) {
          LOGGER_WARNING("file '" << name << "' contains symbolic link");

          _response = createResponse(HttpResponse::FORBIDDEN);
          _response->body().appendText("symbolic links are not allowed");
          return HANDLER_DONE;
        }
      }

      if (! FileUtils::isRegularFile(name)) {
        LOGGER_WARNING("file '" << name << "' not found");

        _response = createResponse(HttpResponse::NOT_FOUND);
        _response->body().appendText("file not found");
        return HANDLER_DONE;
      }

      _response = createResponse(HttpResponse::OK);

      try {
        FileUtils::slurp(name, _response->body());
      }
      catch (...) {
        LOGGER_WARNING("file '" << name << "' not readable");

        _response = createResponse(HttpResponse::NOT_FOUND);
        _response->body().appendText("file not readable");
        return HANDLER_DONE;
      }

      // check if we should use caching and this is an HTTP GET request
      if (cacheMaxAge > 0 && 
          _request->requestType() == HttpRequest::HTTP_REQUEST_GET) {
        // yes, then set a pro-caching header
        _response->setHeader("cache-control", strlen("cache-control"), maxAgeHeader);
      }

      string::size_type d = last.find_last_of('.');

      if (d != string::npos) {
        string suffix = last.substr(d + 1);

        if (suffix.size() > 0) {
          // look up the mimetype
          const char* mimetype = TRI_GetMimetype(suffix.c_str());

          if (mimetype != 0) {
            _response->setContentType(mimetype);

            return HANDLER_DONE;
          }
        }
        else {
          // note: changed the log level to debug. an unknown content-type does not justify a warning
          LOGGER_TRACE("unknown suffix = " << suffix);
        }
      }
      
      _response->setContentType(contentType);

      return HANDLER_DONE;
    }



    void PathHandler::handleError (TriagensError const&) {
      _response = createResponse(HttpResponse::SERVER_ERROR);
    }
  }
}
