'use strict';

// //////////////////////////////////////////////////////////////////////////////
// / @brief Foxx BaseMiddleware
// /
// / @file
// /
// / DISCLAIMER
// /
// / Copyright 2013 triagens GmbH, Cologne, Germany
// /
// / Licensed under the Apache License, Version 2.0 (the "License")
// / you may not use this file except in compliance with the License.
// / You may obtain a copy of the License at
// /
// /     http://www.apache.org/licenses/LICENSE-2.0
// /
// / Unless required by applicable law or agreed to in writing, software
// / distributed under the License is distributed on an "AS IS" BASIS,
// / WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// / See the License for the specific language governing permissions and
// / limitations under the License.
// /
// / Copyright holder is triAGENS GmbH, Cologne, Germany
// /
// / @author Lucas Dohmen
// / @author Copyright 2013, triAGENS GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

const mimeTypes = require('mime-types');

// //////////////////////////////////////////////////////////////////////////////
// / JSF_foxx_BaseMiddleware_initializer
// / @brief The Base Middleware
// /
// / The `BaseMiddleware` manipulates the request and response
// / objects to give you a nicer API.
// //////////////////////////////////////////////////////////////////////////////

function BaseMiddleware () {
  var middleware = function (request, response, options, next) {
    var responseFunctions,
      requestFunctions,
      trace,
      _ = require('lodash'),
      console = require('console'),
      crypto = require('@arangodb/crypto'),
      actions = require('@arangodb/actions'),
      internal = require('internal'),
      fs = require('fs');

    requestFunctions = {

      // //////////////////////////////////////////////////////////////////////////////
      // / @brief was docuBlock JSF_foxx_BaseMiddleware_request_cookie
      // //////////////////////////////////////////////////////////////////////////////

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

      // //////////////////////////////////////////////////////////////////////////////
      // / @brief was docuBlock JSF_foxx_BaseMiddleware_request_body
      // //////////////////////////////////////////////////////////////////////////////

      body: function () {
        var requestBody = this.requestBody || '{}';
        return JSON.parse(requestBody);
      },

      // //////////////////////////////////////////////////////////////////////////////
      // / @brief was docuBlock JSF_foxx_BaseMiddleware_request_rawBody
      // //////////////////////////////////////////////////////////////////////////////

      rawBody: function () {
        return this.requestBody;
      },

      // //////////////////////////////////////////////////////////////////////////////
      // / @brief was docuBlock JSF_foxx_BaseMiddleware_request_rawBodyBuffer
      // //////////////////////////////////////////////////////////////////////////////

      rawBodyBuffer: function () {
        return internal.rawRequestBody(this);
      },

      // //////////////////////////////////////////////////////////////////////////////
      // / @brief was docuBlock JSF_foxx_BaseMiddleware_request_requestParts
      // //////////////////////////////////////////////////////////////////////////////

      requestParts: function () {
        return internal.requestParts(this);
      },

      // //////////////////////////////////////////////////////////////////////////////
      // / @brief was docuBlock JSF_foxx_BaseMiddleware_request_params
      // //////////////////////////////////////////////////////////////////////////////

      params: function (key) {
        if (this.parameters && this.parameters.hasOwnProperty(key)) {
          return this.parameters[key];
        }
        return this.urlParameters && this.urlParameters[key];
      }
    };

    responseFunctions = {

      // //////////////////////////////////////////////////////////////////////////////
      // / @brief was docuBlock JSF_foxx_BaseMiddleware_response_cookie
      // //////////////////////////////////////////////////////////////////////////////

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

      // //////////////////////////////////////////////////////////////////////////////
      // / @brief was docuBlock JSF_foxx_BaseMiddleware_response_status
      // //////////////////////////////////////////////////////////////////////////////

      status: function (code) {
        this.responseCode = code;
      },

      // //////////////////////////////////////////////////////////////////////////////
      // / @brief was docuBlock JSF_foxx_BaseMiddleware_response_set
      // //////////////////////////////////////////////////////////////////////////////

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

          if (key === 'content-type') {
            this.contentType = value;
          }
        }.bind(this));
      },

      // //////////////////////////////////////////////////////////////////////////////
      // / @brief was docuBlock JSF_foxx_BaseMiddleware_response_json
      // //////////////////////////////////////////////////////////////////////////////

      json: function (obj) {
        this.contentType = 'application/json';
        this.body = JSON.stringify(obj);
      },

      // //////////////////////////////////////////////////////////////////////////////
      // / @brief was docuBlock JSF_foxx_BaseMiddleware_response_send
      // //////////////////////////////////////////////////////////////////////////////

      send: function (obj) {
        if (obj instanceof Buffer) {
          // Buffer
          if (!this.contentType) {
            this.contentType = 'application/octet-stream';
          }
        // fallthrough
        } else if (obj !== null && typeof obj === 'object' && !Array.isArray(obj)) {
          // JSON body, treat regularly
          this.json(obj);
          return;
        } else if (typeof obj === 'string') {
          // string
          if (!this.contentType) {
            this.contentType = 'text/html';
          }
        }

        this.body = obj;
      },

      // //////////////////////////////////////////////////////////////////////////////
      // / @brief was docuBlock JSF_foxx_BaseMiddleware_response_sendFile
      // //////////////////////////////////////////////////////////////////////////////

      sendFile: function (filename, options) {
        options = options || { };

        this.body = fs.readBuffer(filename);
        if (options.lastModified) {
          this.set('Last-Modified', new Date(fs.mtime(filename) * 1000).toUTCString());
        }

        if (!this.contentType) {
          this.contentType = mimeTypes.lookup(filename) || 'application/octet-stream';
        }
      }
    };

    // //////////////////////////////////////////////////////////////////////////////
    // / JSF_foxx_BaseMiddleware_response_trace
    // / @brief trace
    // //////////////////////////////////////////////////////////////////////////////

    trace = options.isDevelopment;
    if (!trace && options.options) {
      trace = options.options.trace;
    }

    if (trace) {
      console.log('%s, incoming request from %s: %s',
        options.mount,
        request.client.address,
        actions.stringifyRequest(request));
    }

    Object.assign(request, requestFunctions);
    Object.assign(response, responseFunctions);

    next();

    if (trace) {
      if (response.hasOwnProperty('body')) {
        var bodyLength = 0;
        if (response.body !== undefined) {
          if (typeof response.body === 'string') {
            bodyLength = parseInt(response.body.length, 10);
          } else {
            try {
              bodyLength = String(response.body).length;
            } catch (err) {
              // ignore if this fails
            }
          }
        }
        console.log('%s, outgoing response with status %s of type %s, body length: %d',
          options.mount,
          response.responseCode || 200,
          response.contentType,
          bodyLength);
      } else if (response.hasOwnProperty('bodyFromFile')) {
        console.log('%s, outgoing response with status %s of type %s, body file: %s',
          options.mount,
          response.responseCode || 200,
          response.contentType,
          response.bodyFromFile);
      } else {
        console.log('%s, outgoing response with status %s, no body',
          options.mount,
          response.responseCode || 200);
      }
    }
  };

  return {
    functionRepresentation: middleware
  };
}

exports.BaseMiddleware = BaseMiddleware;
