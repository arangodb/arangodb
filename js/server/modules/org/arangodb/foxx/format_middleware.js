/*jslint indent: 2, nomen: true, maxlen: 120 */
/*global module, require, exports */

////////////////////////////////////////////////////////////////////////////////
/// @brief Foxx Format Middleware
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2013 triagens GmbH, Cologne, Germany
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
/// @author Lucas Dohmen
/// @author Copyright 2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var FormatMiddleware;

////////////////////////////////////////////////////////////////////////////////
/// @brief FormatMiddleware
////////////////////////////////////////////////////////////////////////////////

FormatMiddleware = function (allowedFormats, defaultFormat) {
  'use strict';
  var stringRepresentation, middleware = function (request, response, options, next) {
    var parsed, determinePathAndFormat;

    determinePathAndFormat = function (path, headers) {
      var mimeTypes = require("org/arangodb/mimetypes").mimeTypes,
        extensions = require("org/arangodb/mimetypes").extensions,
        urlFormatToMime = function (urlFormat) {
          var mimeType;

          if (mimeTypes[urlFormat]) {
            mimeType = mimeTypes[urlFormat][0];
          } else {
            mimeType = undefined;
          }

          return mimeType;
        },
        mimeToUrlFormat = function (mimeType) {
          var urlFormat;

          if (extensions[mimeType]) {
            urlFormat = extensions[mimeType][0];
          } else {
            urlFormat = undefined;
          }

          return urlFormat;
        },
        parsed = {
          contentType: headers.accept
        };

      path = path.split('.');

      if (path.length === 1) {
        parsed.path = path.join();
      } else {
        parsed.format = path.pop();
        parsed.path = path.join('.');
      }

      if (parsed.contentType === undefined && parsed.format === undefined) {
        parsed.format = defaultFormat;
        parsed.contentType = urlFormatToMime(defaultFormat);
      } else if (parsed.contentType === undefined) {
        parsed.contentType = urlFormatToMime(parsed.format);
      } else if (parsed.format === undefined) {
        parsed.format = mimeToUrlFormat(parsed.contentType);
      }

      if (parsed.format !== mimeToUrlFormat(parsed.contentType)) {
        throw new Error("Contradiction between Accept Header and URL.");
      }

      if (allowedFormats.indexOf(parsed.format) < 0) {
        throw new Error("Format '" + parsed.format + "' is not allowed.");
      }

      return parsed;
    };

    try {
      parsed = determinePathAndFormat(request.path, request.headers);
      request.path = parsed.path;
      request.format = parsed.format;
      response.contentType = parsed.contentType;
      next();
    } catch (e) {
      response.responseCode = 406;
      response.body = e;
    }
  };

  stringRepresentation = String(middleware)
    .replace("allowedFormats", JSON.stringify(allowedFormats))
    .replace("defaultFormat", JSON.stringify(defaultFormat));

  return middleware;
};

exports.FormatMiddleware = FormatMiddleware;

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

/// Local Variables:
/// mode: outline-minor
/// outline-regexp: "/// @brief\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}\\|/\\*jslint"
/// End:
