/*jslint indent: 2, nomen: true, maxlen: 120, sloppy: true, vars: true, white: true, plusplus: true, nonpropdel: true */
/*global require, ArangoConnection, print, SYS_ARANGO */

////////////////////////////////////////////////////////////////////////////////
/// @brief module "internal"
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
/// @author Copyright 2010-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                 Module "internal"
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoShell
/// @{
////////////////////////////////////////////////////////////////////////////////

(function () {
  var internal = require("internal");

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoShell
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief hide global variables
////////////////////////////////////////////////////////////////////////////////

    if (typeof ArangoConnection !== "undefined") {
      internal.ArangoConnection = ArangoConnection;
      delete ArangoConnection;
    }

    if (typeof SYS_ARANGO !== "undefined") {
      internal.arango = SYS_ARANGO;
      delete SYS_ARANGO;
    }

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoShell
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief flushes the module cache of the server
////////////////////////////////////////////////////////////////////////////////

  internal.flushServerModules = function () {
    if (typeof internal.arango !== 'undefined') {
      internal.arango.POST("/_admin/modules/flush", "");
      return;
    }

    throw "not connected";
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief rebuilds the routing cache
////////////////////////////////////////////////////////////////////////////////

  internal.reloadRouting = function () {
    if (typeof internal.arango !== 'undefined') {
      internal.arango.POST("/_admin/routing/reload", "");
      return;
    }

    throw "not connected";
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief rebuilds the routing cache
////////////////////////////////////////////////////////////////////////////////

  internal.routingCache = function () {
    if (typeof internal.arango !== 'undefined') {
      return internal.arango.GET("/_admin/routing/routes", "");
      
    }

    throw "not connected";
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief rebuilds the authentication cache
////////////////////////////////////////////////////////////////////////////////

  internal.reloadAuth = function () {
    if (typeof internal.arango !== 'undefined') {
      internal.arango.POST("/_admin/auth/reload", "");
      return;
    }

    throw "not connected";
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief execute javascript file on the server
////////////////////////////////////////////////////////////////////////////////

  internal.executeServer = function (body) {
    if (typeof internal.arango !== 'undefined') {
      return internal.arango.POST("/_admin/execute", body);
    }

    throw "not connected";
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief logs a request in curl format
////////////////////////////////////////////////////////////////////////////////

  internal.appendCurlRequest = function (appender) {
    return function (method, url, body) {
      var response;
      var curl;

      if (typeof body !== 'string') {
        body = JSON.stringify(body);
      }

      curl = "unix> curl ";

      if (method === 'POST') {
        response = internal.arango.POST_RAW(url, body);
        curl += "-X " + method + " ";
      }
      else if (method === 'PUT') {
        response = internal.arango.PUT_RAW(url, body);
        curl += "-X " + method + " ";
      }
      else if (method === 'GET') {
        response = internal.arango.GET_RAW(url, body);
      }
      else if (method === 'DELETE') {
        response = internal.arango.DELETE_RAW(url, body);
        curl += "-X " + method + " ";
      }
      else if (method === 'PATCH') {
        response = internal.arango.PATCH_RAW(url, body);
        curl += "-X " + method + " ";
      }
      else if (method === 'OPTION') {
        response = internal.arango.OPTION_RAW(url, body);
        curl += "-X " + method + " ";
      }

      if (body !== undefined && body !== "") {
        curl += "--data @- ";
      }

      curl += "--dump - http://localhost:8529" + url;

      appender(curl + "\n");

      if (body !== undefined && body !== "") {
        appender(body += "\n");
      }

      appender("\n");

      return response;
    };
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief logs a response in JSON
////////////////////////////////////////////////////////////////////////////////

  internal.appendJsonResponse = function (appender) {
    return function (response) {
      var key;
      var headers = response.headers;
      var output;

      // generate header
      appender("HTTP/1.1 " + headers['http/1.1'] + "\n");

      for (key in headers) {
        if (headers.hasOwnProperty(key)) {
          if (key !== 'http/1.1' && key !== 'server' && key !== 'connection'
              && key !== 'content-length') {
            appender(key + ": " + headers[key] + "\n");
          }
        }
      }

      appender("\n");

      // pretty print body
      internal.startCaptureMode();
      print(JSON.parse(response.body));
      output = internal.stopCaptureMode();
      appender(output);
      appender("\n");
    };
  };

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoShell
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief log function
////////////////////////////////////////////////////////////////////////////////

  internal.log = function (level, msg) {
    internal.output(level, ": ", msg, "\n");
  };

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

}());

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}\\|/\\*jslint"
// End:
