/*jslint indent: 2, nomen: true, maxlen: 100, sloppy: true, vars: true, white: true, regexp: true plusplus: true */
/*global require, exports, module */

////////////////////////////////////////////////////////////////////////////////
/// @brief JavaScript actions module
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
/// @author Dr. Frank Celler
/// @author Copyright 2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var internal = require("internal");

var arangodb = require("org/arangodb");

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief prints the flatten routing cache for a method
////////////////////////////////////////////////////////////////////////////////

function printFlatRoutingMethod (indent, flat) {
  var i;

  for (i = 0;  i < flat.length;  ++i) {
    var f = flat[i];
    var c = "";

    if (f.prefix) {
      c += "prefix ";
    }

    if (c !== "") {
      c = "[" + c.substr(0, c.length - 1) + "]";
    }

    arangodb.printf("%s%d: %s %s\n", indent, i, f.path, c);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief prints the routing cache for a method
////////////////////////////////////////////////////////////////////////////////

function printRoutingMethod (indent, routes) {
  var k;
  var i;

  if (routes.hasOwnProperty('exact')) {
    for (k in routes.exact) {
      arangodb.printf("%sEXACT '%s'\n", indent, k);

      if (routes.exact.hasOwnProperty(k)) {
        printRoutingMethod(indent + " ", routes.exact[k]);
      }
    }
  }

  if (routes.hasOwnProperty('parameters')) {
    for (i = 0;  i < routes.parameters.length;  ++i) {
      var parameter = routes.parameters[i];

      if (parameter.hasOwnProperty('constraint')) {
        arangodb.printf("%PARAMETER '%s'\n", indent, parameter.constraint);
      }
      else {
        arangodb.printf("%PARAMETER\n", indent);
      }

      printRoutingMethod(indent + " ", parameter.match);
    }
  }

  if (routes.hasOwnProperty('callback')) {
    arangodb.printf("%sCALLBACK\n", indent);
  }

  if (routes.hasOwnProperty('prefix')) {
    arangodb.printf("%sPREFIX\n", indent);
    printRoutingMethod(indent + " ", routes.prefix);
  }  
}

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the routing cache
////////////////////////////////////////////////////////////////////////////////

exports.routingCache = internal.routingCache;

////////////////////////////////////////////////////////////////////////////////
/// @brief prints the flatten routing cache
////////////////////////////////////////////////////////////////////////////////

function printFlatRouting (method) {
  var cache = exports.routingCache();

  if (method === undefined) {
    method = "GET";
  }
  else {
    method = method.toUpperCase();
  }

  arangodb.printf("METHOD %s\n", method);
  printFlatRoutingMethod(" ", cache.flat[method]);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief prints the routing cache
////////////////////////////////////////////////////////////////////////////////

function printRouting (method) {
  var cache = exports.routingCache();

  if (method === undefined) {
    method = "GET";
  }
  else {
    method = method.toUpperCase();
  }

  arangodb.printf("METHOD %s\n", method);
  arangodb.printf(" ROUTES\n");
  printRoutingMethod("  ", cache.routes[method]);

  arangodb.printf("METHOD %s\n", method);
  arangodb.printf(" MIDDLEWARE\n");
  printRoutingMethod("  ", cache.middleware[method]);
}

// -----------------------------------------------------------------------------
// --SECTION--                                                    MODULE EXPORTS
// -----------------------------------------------------------------------------

exports.printFlatRouting = printFlatRouting;
exports.printRouting = printRouting;

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @\\}"
// End:
