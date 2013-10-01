/*jslint indent: 2, nomen: true, maxlen: 120 */
/*global module, require, exports */

////////////////////////////////////////////////////////////////////////////////
/// @brief Foxx BaseMiddleware
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

var BaseMiddleware;

////////////////////////////////////////////////////////////////////////////////
/// @fn JSF_foxx_BaseMiddleware_initializer
/// @brief The Base Middleware
///
/// The `BaseMiddleware` manipulates the request and response
/// objects to give you a nicer API.
////////////////////////////////////////////////////////////////////////////////

BaseMiddleware = function () {
  'use strict';
  var middleware = function (request, response, options, next) {
    var responseFunctions,
      requestFunctions,
      trace,
      _ = require("underscore"),
      console = require("console"),
      actions = require("org/arangodb/actions");

    requestFunctions = {

////////////////////////////////////////////////////////////////////////////////
/// @fn JSF_foxx_BaseMiddleware_request_body
/// @brief Get the JSON parsed body of the request
///
/// @FUN{request.body()}
///
/// Get the JSON parsed body of the request. If you need the raw version, please
/// refer to the `rawBody` function.
////////////////////////////////////////////////////////////////////////////////

      body: function () {
        return JSON.parse(this.requestBody);
      },

////////////////////////////////////////////////////////////////////////////////
/// @fn JSF_foxx_BaseMiddleware_request_rawBody
/// @brief Get the raw body of the request
///
/// @FUN{request.rawBody()}
///
/// The raw request body, not parsed. Just a String.
////////////////////////////////////////////////////////////////////////////////

      rawBody: function () {
        return this.requestBody;
      },

////////////////////////////////////////////////////////////////////////////////
/// @fn JSF_foxx_BaseMiddleware_request_params
/// @brief The jinxed `params` function
///
/// @FUN{request.params(@FA{key})}
///
/// Get the parameters of the request. This process is two-fold:
///
/// - If you have defined an URL like `/test/:id` and the user requested
///   `/test/1`, the call `params("id")` will return `1`.
/// - If you have defined an URL like `/test` and the user gives a query
///   component, the query parameters will also be returned.  So for example if
///   the user requested `/test?a=2`, the call `params("a")` will return `2`.
////////////////////////////////////////////////////////////////////////////////

      params: function (key) {
        var ps = {};
        _.extend(ps, this.urlParameters);
        _.extend(ps, this.parameters);
        return ps[key];
      }
    };

    responseFunctions = {

////////////////////////////////////////////////////////////////////////////////
/// @fn JSF_foxx_BaseMiddleware_response_status
/// @brief The straightforward `status` function
///
/// @FUN{response.status(@FA{code})}
///
/// Set the status @FA{code} of your response, for example:
///
/// @EXAMPLES
///
/// @code
///    response.status(404);
/// @endcode
////////////////////////////////////////////////////////////////////////////////

      status: function (code) {
        this.responseCode = code;
      },

////////////////////////////////////////////////////////////////////////////////
/// @fn JSF_foxx_BaseMiddleware_response_set
/// @brief The radical `set` function
///
/// @FUN{response.set(@FA{key}, @FA{value})}
///
/// Set a header attribute, for example:
///
/// @EXAMPLES
///
/// @code
///     response.set("Content-Length", 123);
///     response.set("Content-Type", "text/plain");
///
///     // or alternatively:
///
///     response.set({
///       "Content-Length": "123",
///       "Content-Type": "text/plain"
///     });
/// @endcode
////////////////////////////////////////////////////////////////////////////////

      set: function (key, value) {
        var attributes = {};
        if (_.isUndefined(value)) {
          attributes = key;
        } else {
          attributes[key] = value;
        }

        _.each(attributes, function (value, key) {
          key = key.toLowerCase();
          this.headers = this.headers || {};
          this.headers[key] = value;

          if (key === "content-type") {
            this.contentType = value;
          }
        }, this);
      },

////////////////////////////////////////////////////////////////////////////////
/// @fn JSF_foxx_BaseMiddleware_response_json
/// @brief The magical `json` function
///
/// @FUN{response.json(@FA{object})}
///
/// Set the content type to JSON and the body to the JSON encoded @FA{object}
/// you provided.
///
/// @EXAMPLES
///
/// @code
///     response.json({'born': 'December 12, 1915'});
/// @endcode
////////////////////////////////////////////////////////////////////////////////

      json: function (obj) {
        this.contentType = "application/json";
        this.body = JSON.stringify(obj);
      }
    };

////////////////////////////////////////////////////////////////////////////////
/// @fn JSF_foxx_BaseMiddleware_response_trace
/// @brief trace
////////////////////////////////////////////////////////////////////////////////

    trace = options.isDevelopment;
    if (!trace && options.hasOwnProperty("options")) {
      trace = options.options.trace;
    }

    if (trace) {
      console.log("%s, incoming request from %s: %s",
                  options.mount,
                  request.client.address,
                  actions.stringifyRequestAddress(request));
    }

    _.extend(request, requestFunctions);
    _.extend(response, responseFunctions);

    next();

    if (trace) {
      if (response.hasOwnProperty("body")) {
        console.log("%s, outgoing response with status %s of type %s, body length: %d",
                    options.mount,
                    response.responseCode,
                    response.contentType,
                    parseInt(response.body.length, 10));
      } else if (response.hasOwnProperty("bodyFromFile")) {
        console.log("%s, outgoing response with status %s of type %s, body file: %s",
                    options.mount,
                    response.responseCode,
                    response.contentType,
                    response.bodyFromFile);
      } else {
        console.log("%s, outgoing response with status %s of type %s, no body",
                    options.mount,
                    response.responseCode,
                    response.contentType);
      }
    }
  };

  return {
    stringRepresentation: String(middleware),
    functionRepresentation: middleware
  };
};

exports.BaseMiddleware = BaseMiddleware;

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

/// Local Variables:
/// mode: outline-minor
/// outline-regexp: "/// @brief\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}\\|/\\*jslint"
/// End:
