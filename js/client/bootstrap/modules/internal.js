/* jshint -W051:true */
/* eslint-disable */
;(function () {
  'use strict'
  /* eslint-enable */

  // //////////////////////////////////////////////////////////////////////////////
  // / @brief module "internal"
  // /
  // / @file
  // /
  // / DISCLAIMER
  // /
  // / Copyright 2004-2013 triAGENS GmbH, Cologne, Germany
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
  // / @author Dr. Frank Celler
  // / @author Copyright 2010-2013, triAGENS GmbH, Cologne, Germany
  // //////////////////////////////////////////////////////////////////////////////

  var exports = require('internal');

  // //////////////////////////////////////////////////////////////////////////////
  // / @brief hide global variables
  // //////////////////////////////////////////////////////////////////////////////

  if (global.ArangoConnection) {
    exports.ArangoConnection = global.ArangoConnection;
  }

  if (global.SYS_ARANGO) {
    exports.arango = global.SYS_ARANGO;
    delete global.SYS_ARANGO;
  }

  // //////////////////////////////////////////////////////////////////////////////
  // / @brief write-ahead log functionality
  // //////////////////////////////////////////////////////////////////////////////

  exports.wal = {
    flush: function (waitForSync, waitForCollector) {
      if (exports.arango) {
        var wfs = waitForSync ? 'true' : 'false';
        var wfc = waitForCollector ? 'true' : 'false';
        exports.arango.PUT('/_admin/wal/flush?waitForSync=' + wfs + '&waitForCollector=' + wfc, '');
        return;
      }

      throw 'not connected';
    },

    properties: function (value) {
      if (exports.arango) {
        if (value !== undefined) {
          return exports.arango.PUT('/_admin/wal/properties', JSON.stringify(value));
        }

        return exports.arango.GET('/_admin/wal/properties', '');
      }

      throw 'not connected';
    },

    transactions: function () {
      if (exports.arango) {
        return exports.arango.GET('/_admin/wal/transactions', '');
      }

      throw 'not connected';
    }
  };

  // //////////////////////////////////////////////////////////////////////////////
  // / @brief reloads the AQL user functions
  // //////////////////////////////////////////////////////////////////////////////

  exports.reloadAqlFunctions = function () {
    if (exports.arango) {
      exports.arango.POST('/_admin/aql/reload', '');
      return;
    }

    throw 'not connected';
  };

  // //////////////////////////////////////////////////////////////////////////////
  // / @brief rebuilds the routing cache
  // //////////////////////////////////////////////////////////////////////////////

  exports.reloadRouting = function () {
    if (exports.arango) {
      exports.arango.POST('/_admin/routing/reload', '');
      return;
    }

    throw 'not connected';
  };

  // //////////////////////////////////////////////////////////////////////////////
  // / @brief rebuilds the routing cache
  // //////////////////////////////////////////////////////////////////////////////

  exports.routingCache = function () {
    if (exports.arango) {
      return exports.arango.GET('/_admin/routing/routes', '');
    }

    throw 'not connected';
  };

  // //////////////////////////////////////////////////////////////////////////////
  // / @brief rebuilds the authentication cache
  // //////////////////////////////////////////////////////////////////////////////

  exports.reloadAuth = function () {
    if (exports.arango) {
      exports.arango.POST('/_admin/auth/reload', '');
      return;
    }

    throw 'not connected';
  };

  // //////////////////////////////////////////////////////////////////////////////
  // / @brief logs a request in curl format
  // //////////////////////////////////////////////////////////////////////////////

  exports.appendCurlRequest = function (shellAppender, jsonAppender, rawAppender) {
    return function (method, url, body, headers) {
      var response;
      var curl;
      var i;
      var jsonBody = false;

      if ((typeof body !== 'string') && (body !== undefined)) {
        jsonBody = true;
        body = exports.inspect(body);
      }

      curl = 'shell> curl ';

      if (method === 'POST') {
        response = exports.arango.POST_RAW(url, body, headers);
        curl += '-X ' + method + ' ';
      } else if (method === 'PUT') {
        response = exports.arango.PUT_RAW(url, body, headers);
        curl += '-X ' + method + ' ';
      } else if (method === 'GET') {
        response = exports.arango.GET_RAW(url, headers);
      } else if (method === 'DELETE') {
        response = exports.arango.DELETE_RAW(url, headers);
        curl += '-X ' + method + ' ';
      } else if (method === 'PATCH') {
        response = exports.arango.PATCH_RAW(url, body, headers);
        curl += '-X ' + method + ' ';
      } else if (method === 'HEAD') {
        response = exports.arango.HEAD_RAW(url, headers);
        curl += '-X ' + method + ' ';
      } else if (method === 'OPTION') {
        response = exports.arango.OPTION_RAW(url, body, headers);
        curl += '-X ' + method + ' ';
      }
      if (headers !== undefined && headers !== '') {
        for (i in headers) {
          if (headers.hasOwnProperty(i)) {
            curl += "--header '" + i + ': ' + headers[i] + "' ";
          }
        }
      }

      if (body !== undefined && body !== '') {
        curl += '--data-binary @- ';
      }

      curl += '--dump - http://localhost:8529' + url;

      shellAppender(curl);

      if (body !== undefined && body !== '' && body) {
        rawAppender(' &lt;&lt;EOF\n');
        if (jsonBody) {
          jsonAppender(body);
        } else {
          rawAppender(body);
        }
        rawAppender('\nEOF');
      }
      rawAppender('\n\n');
      return response;
    };
  };

  // //////////////////////////////////////////////////////////////////////////////
  // / @brief logs a raw response
  // //////////////////////////////////////////////////////////////////////////////

  exports.appendRawResponse = function (appender, syntaxAppender) {
    return function (response) {
      var key;
      var headers = response.headers;

      // generate header
      appender('HTTP/1.1 ' + headers['http/1.1'] + '\n');

      for (key in headers) {
        if (headers.hasOwnProperty(key)) {
          if (key !== 'http/1.1' && key !== 'server' && key !== 'connection'
            && key !== 'content-length') {
            appender(key + ': ' + headers[key] + '\n');
          }
        }
      }

      appender('\n');

      // append body
      if (response.body !== undefined) {
        syntaxAppender(exports.inspect(response.body));
        appender('\n');
      }
    };
  };

  // //////////////////////////////////////////////////////////////////////////////
  // / @brief logs a response in JSON
  // //////////////////////////////////////////////////////////////////////////////

  exports.appendJsonResponse = function (appender, syntaxAppender) {
    return function (response) {
      var syntaxAppend = exports.appendRawResponse(syntaxAppender, syntaxAppender);

      // copy original body (this is necessary because 'response' is passed by reference)
      var copy = response.body;
      // overwrite body with parsed JSON && append
      response.body = JSON.parse(response.body);
      syntaxAppend(response);
      // restore original body
      response.body = copy;
    };
  };

  // //////////////////////////////////////////////////////////////////////////////
  // / @brief log function
  // //////////////////////////////////////////////////////////////////////////////

  exports.log = function (level, msg) {
    exports.output(level, ': ', msg, '\n');
  };

  // //////////////////////////////////////////////////////////////////////////////
  // / @brief sprintf wrapper
  // //////////////////////////////////////////////////////////////////////////////

  try {
    if (typeof window !== 'undefined') {
      exports.sprintf = function (format) {
        var n = arguments.length;
        if (n === 0) {
          return '';
        }
        if (n <= 1) {
          return String(format);
        }

        var i;
        var args = [];
        for (i = 1; i < arguments.length; ++i) {
          args.push(arguments[i]);
        }
        i = 0;

        return format.replace(/%[dfs]/, function () {
          return String(args[i++]);
        });
      };
    }
  } catch (e) {
    // noop
  }
}());
