/*global require, exports, Buffer */
'use strict';

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

////////////////////////////////////////////////////////////////////////////////
/// @fn JSF_foxx_BaseMiddleware_initializer
/// @brief The Base Middleware
///
/// The `BaseMiddleware` manipulates the request and response
/// objects to give you a nicer API.
////////////////////////////////////////////////////////////////////////////////

function BaseMiddleware() {
  var middleware = function (request, response, options, next) {
    var responseFunctions,
      requestFunctions,
      trace,
      _ = require("underscore"),
      console = require("console"),
      crypto = require("org/arangodb/crypto"),
      actions = require("org/arangodb/actions"),
      internal = require("internal"),
      fs = require("fs"),
      arangodb = require("org/arangodb");

    requestFunctions = {

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_foxx_BaseMiddleware_request_cookie
///
/// `request.cookie(name, cfg)`
///
/// Read a cookie from the request. Optionally the cookie's signature can be verified.
///
/// *Parameter*
///
/// * *name*: the name of the cookie to read from the request.
/// * *cfg* (optional): an object with any of the following properties:
///   * *signed* (optional): an object with any of the following properties:
///   * *secret*: a secret string that was used to sign the cookie.
///   * *algorithm*: hashing algorithm that was used to sign the cookie. Default: *"sha256"*.
///
/// If *signed* is a string, it will be used as the *secret* instead.
///
/// If a *secret* is provided, a second cookie with the name *name + ".sig"* will
/// be read and its value will be verified as the cookie value's signature.
///
/// If the cookie is not set or its signature is invalid, "undefined" will be returned instead.
///
/// @EXAMPLES
///
/// ```
/// var sid = request.cookie("sid", {signed: "keyboardcat"});
/// ```
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

      cookie: function (name, cfg) {
        if (!cfg || typeof cfg !== 'object') {
          cfg = {};
        }
        var value = this.cookies[name] || undefined;
        if (value && cfg.signed) {
          if (typeof cfg.signed === 'string') {
            cfg.signed = {secret: cfg.signed};
          }
          var valid = crypto.constantEquals(
            this.cookies[name + '.sig'] || '',
            crypto.hmac(cfg.signed.secret, value, cfg.signed.algorithm)
          );
          if (!valid) {
            value = undefined;
          }
        }
        return value;
      },

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_foxx_BaseMiddleware_request_body
///
/// `request.body()`
///
/// Get the JSON parsed body of the request. If you need the raw version, please
/// refer to the *rawBody* function.
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

      body: function () {
        var requestBody = this.requestBody || '{}';
        return JSON.parse(requestBody);
      },

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_foxx_BaseMiddleware_request_rawBody
///
/// `request.rawBody()`
///
/// The raw request body, not parsed. The body is returned as a UTF-8 string.
/// Note that this can only be used sensibly if the request body contains
/// valid UTF-8. If the request body is known to contain non-UTF-8 data, the
/// request body can be accessed by using `request.rawBodyBuffer`.
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

      rawBody: function () {
        return this.requestBody;
      },

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_foxx_BaseMiddleware_request_rawBodyBuffer
///
/// `request.rawBodyBuffer()`
///
/// The raw request body, returned as a Buffer object.
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

      rawBodyBuffer: function () {
        return internal.rawRequestBody(this);
      },

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_foxx_BaseMiddleware_request_requestParts
///
/// `request.requestParts()`
///
/// Returns an array containing the individual parts of a multi-part request.
/// Each part contains a `headers` attribute with all headers of the part,
/// and a `data` attribute with the content of the part in a Buffer object.
/// If the request is not a multi-part request, this function will throw an
/// error.
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

      requestParts: function () {
        return internal.requestParts(this);
      },

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_foxx_BaseMiddleware_request_params
///
/// `request.params(key)`
///
/// Get the parameters of the request. This process is two-fold:
///
/// * If you have defined an URL like */test/:id* and the user requested
///   */test/1*, the call *params("id")* will return *1*.
/// * If you have defined an URL like */test* and the user gives a query
///   component, the query parameters will also be returned.  So for example if
///   the user requested */test?a=2*, the call *params("a")* will return *2*.
/// @endDocuBlock
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
/// @startDocuBlock JSF_foxx_BaseMiddleware_response_cookie
///
/// `response.cookie(name, value, cfg)`
///
/// Add a cookie to the response. Optionally the cookie can be signed.
///
/// *Parameter*
///
/// * *name*: the name of the cookie to add to the response.
/// * *value*: the value of the cookie to add to the response.
/// * *cfg* (optional): an object with any of the following properties:
///   * *ttl* (optional): the number of seconds until this cookie expires.
///   * *path* (optional): the cookie path.
///   * *domain* (optional): the cookie domain.
///   * *secure* (optional): mark the cookie as safe transport (HTTPS) only.
///   * *httpOnly* (optional): mark the cookie as HTTP(S) only.
///   * *signed* (optional): an object with any of the following properties:
///     * *secret*: a secret string to sign the cookie with.
///     * *algorithm*: hashing algorithm to sign the cookie with. Default: *"sha256"*.
///
/// If *signed* is a string, it will be used as the *secret* instead.
///
/// If a *secret* is provided, a second cookie with the name *name + ".sig"* will
/// be added to the response, containing the cookie's HMAC signature.
///
/// @EXAMPLES
///
/// ```
/// response.cookie("sid", "abcdef", {signed: "keyboardcat"});
/// ```
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

      cookie: function (name, value, cfg) {
        if (!cfg || typeof cfg !== 'object') {
          cfg = {ttl: cfg};
        }
        var ttl = (typeof cfg.ttl === 'number' && cfg.ttl !== Infinity) ? cfg.ttl : undefined;
        actions.addCookie(this, name, value, ttl, cfg.path, cfg.domain, cfg.secure, cfg.httpOnly);
        if (cfg.signed) {
          if (typeof cfg.signed === 'string') {
            cfg.signed = {secret: cfg.signed};
          }
          var sig = crypto.hmac(cfg.signed.secret, value, cfg.signed.algorithm);
          actions.addCookie(this, name + '.sig', sig, ttl, cfg.path, cfg.domain, cfg.secure, cfg.httpOnly);
        }
      },

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_foxx_BaseMiddleware_response_status
///
/// `response.status(code)`
///
/// Set the status *code* of your response, for example:
///
/// @EXAMPLES
///
/// ```
/// response.status(404);
/// ```
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

      status: function (code) {
        this.responseCode = code;
      },

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_foxx_BaseMiddleware_response_set
///
/// `response.set(key, value)`
///
/// Set a header attribute, for example:
///
/// @EXAMPLES
///
/// ```js
/// response.set("Content-Length", 123);
/// response.set("Content-Type", "text/plain");
/// ```
///
/// or alternatively:
///
/// ```js
/// response.set({
///   "Content-Length": "123",
///   "Content-Type": "text/plain"
/// });
/// ```
/// @endDocuBlock
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
/// @startDocuBlock JSF_foxx_BaseMiddleware_response_json
///
/// `response.json(object)`
///
/// Set the content type to JSON and the body to the JSON encoded *object*
/// you provided.
///
/// @EXAMPLES
///
/// ```js
/// response.json({'born': 'December 12, 1915'});
/// ```
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

      json: function (obj) {
        this.contentType = "application/json";
        this.body = JSON.stringify(obj);
      },

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_foxx_BaseMiddleware_response_send
///
/// `response.send(value)`
///
/// Sets the response body to the specified *value*. If *value* is a Buffer
/// object, the content type will be set to `application/octet-stream` if not
/// yet set. If *value* is a string, the content type will be set to `text/html`
/// if not yet set. If *value* is an object, it will be treated as in `res.json`.
///
/// @EXAMPLES
///
/// ```js
/// response.send({"born": "December 12, 1915"});
/// response.send(new Buffer("some binary data"));
/// response.send("<html><head><title>Hello World</title></head><body></body></html>");
/// ```
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

      send: function (obj) {
        if (obj instanceof Buffer) {
          // Buffer
          if (! this.contentType) {
            this.contentType = "application/octet-stream";
          }
          // fallthrough
        }
        else if (obj !== null && typeof obj === 'object' && ! Array.isArray(obj)) {
          // JSON body, treat regularly
          this.json(obj);
          return;
        }
        else if (typeof obj === 'string') {
          // string
          if (! this.contentType) {
            this.contentType = "text/html";
          }
        }

        this.body = obj;
      },

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_foxx_BaseMiddleware_response_sendFile
///
/// `response.sendFile(filename, options)`
///
/// Sets the content of the specified file as the response body. The filename
/// must be absolute. If no content type is yet set for the response, the
/// response's content type will be determined automatically based
/// on the filename extension. If no content type is known for the extension,
/// the content type will default to `application/octet-stream`.
///
/// The `options` array can be used to control the behavior of sendFile.
/// Currently only the following option exists:
/// - `lastModified`: if set to true, the last modification date and time
///   of the file will be returned in the `Last-Modified` HTTP header
///
/// @EXAMPLES
///
/// ```js
/// response.sendFile('/tmp/results.json');
/// response.sendFile(applicationContext.foxxFilename('image.png'), { lastModified: true });
/// ```
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

      sendFile: function (filename, options) {
        options = options || { };

        this.body = fs.readBuffer(filename);
        if (options.lastModified) {
          this.set("Last-Modified", new Date(fs.mtime(filename) * 1000).toUTCString());
        }

        if (! this.contentType) {
          this.contentType = arangodb.guessContentType(filename, 'application/octet-stream');
        }
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
        var bodyLength = 0;
        if (response.body !== undefined) {
          if (typeof response.body === "string") {
            bodyLength = parseInt(response.body.length, 10);
          } else {
            try {
              bodyLength = String(response.body).length;
            } catch (err) {
              // ignore if this fails
            }
          }
        }
        console.log("%s, outgoing response with status %s of type %s, body length: %d",
                    options.mount,
                    response.responseCode || 200,
                    response.contentType,
                    bodyLength);
      } else if (response.hasOwnProperty("bodyFromFile")) {
        console.log("%s, outgoing response with status %s of type %s, body file: %s",
                    options.mount,
                    response.responseCode || 200,
                    response.contentType,
                    response.bodyFromFile);
      } else {
        console.log("%s, outgoing response with status %s of type %s, no body",
                    options.mount,
                    response.responseCode || 200,
                    response.contentType);
      }
    }
  };

  return {
    stringRepresentation: String(middleware),
    functionRepresentation: middleware
  };
}

exports.BaseMiddleware = BaseMiddleware;

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

/// Local Variables:
/// mode: outline-minor
/// outline-regexp: "/// @brief\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}\\|/\\*jslint"
/// End:
