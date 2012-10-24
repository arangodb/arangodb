/*jslint indent: 2,
         nomen: true,
         maxlen: 100,
         sloppy: true,
         vars: true,
         white: true,
         plusplus: true */
/*global require, exports */

////////////////////////////////////////////////////////////////////////////////
/// @brief administration actions
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2010-2012 triagens GmbH, Cologne, Germany
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
/// @author Copyright 2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

(function() {
  var actions = require("org/arangodb/actions");
  var internal = require("internal");
  var console = require("internal");

// -----------------------------------------------------------------------------
// --SECTION--                                                  standard routing
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoAPI
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief routing function
////////////////////////////////////////////////////////////////////////////////

  function Routing (req, res) {
    var action;
    var execute;
    var next;
    var path = req.suffix.join("/");

    action = actions.firstRouting(req.requestType, req.suffix);

    execute = function () {
      if (action.route === undefined) {
	actions.resultNotImplemented(req, res, "unknown path '" + path + "'");
	return;
      }

      if (action.route.path !== undefined) {
	req.path = action.route.path;
      }
      else {
	delete req.path;
      }

      if (action.prefix !== undefined) {
	req.prefix = action.prefix;
      }
      else {
	delete req.prefix;
      }

      if (action.suffix !== undefined) {
	req.suffix = action.suffix;
      }
      else {
	delete req.suffix;
      }

      if (action.urlParameters !== undefined) {
	req.urlParameters = action.urlParameters;
      }
      else {
	req.urlParameters = {};
      }

      var func = action.route.callback.controller;
      if (func === null || typeof func !== 'function') {
        func = actions.errorFunction(action.route, 'Invalid callback definition found for route ' + JSON.stringify(action.route));
      }

      try {
        func(req, res, action.route.callback.options, next);
      }
      catch (err) {
        actions.errorFunction(action.route, 'A runtime error occurred while executing an action: ' + String(err))(req, res, action.route.callback.options, next);
      }
    };

    next = function () {
      action = actions.nextRouting(action);
      execute();
    };

    execute();
  }

  actions.defineHttp({
    url : "",
    prefix : true,
    context : "admin",
    callback : Routing
  });

////////////////////////////////////////////////////////////////////////////////
/// @brief returns system status information for the server
////////////////////////////////////////////////////////////////////////////////

  actions.defineHttp({
    url : "_admin/routing/reload",
    context : "admin",
    prefix : false,
    callback : function (req, res) {
      internal.executeGlobalContextFunction("require(\"org/arangodb/actions\").reloadRouting()");
      actions.resultOk(req, res, actions.HTTP_OK);
    }
  });

////////////////////////////////////////////////////////////////////////////////
/// @brief returns system status information for the server
////////////////////////////////////////////////////////////////////////////////

  actions.defineHttp({
    url : "_admin/routing/routes",
    context : "admin",
    prefix : false,
    callback : function (req, res) {
      actions.resultOk(req, res, actions.HTTP_OK, actions.routingCache());
    }
  });

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                            administration actions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoAPI
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @fn JSF_GET_admin_time
/// @brief returns the system time
///
/// @RESTHEADER{GET /_admin/time,returns the system time}
///
/// @REST{GET /_admin/time}
///
/// The call returns an object with the attribute @LIT{time}. This contains the
/// current system time as a Unix timestamp with microsecond precision.
////////////////////////////////////////////////////////////////////////////////

  actions.defineHttp({
    url : "_admin/time",
    context : "admin",
    prefix : false,
    callback : function (req, res) {
      actions.resultOk(req, res, actions.HTTP_OK, { time : internal.time() });
    }
  });

////////////////////////////////////////////////////////////////////////////////
/// @fn JSF_GET_admin_echo
/// @brief returns the request
///
/// @RESTHEADER{GET /_admin/echo,returns the current request}
///
/// @REST{GET /_admin/echo}
///
/// The call returns an object with the following attributes:
///
/// - @LIT{headers}: a list of HTTP headers received
///
/// - @LIT{requestType}: the HTTP request method (e.g. GET)
///
/// - @LIT{parameters}: list of URL parameters received
////////////////////////////////////////////////////////////////////////////////

  actions.defineHttp({
    url : "_admin/echo",
    context : "admin",
    prefix : true,
    callback : function (req, res) {
      res.responseCode = actions.HTTP_OK;
      res.contentType = "application/json; charset=utf-8";
      res.body = JSON.stringify(req);
    }
  });

////////////////////////////////////////////////////////////////////////////////
/// @fn JSF_GET_admin_status
/// @brief returns system status information for the server
///
/// @RESTHEADER{GET /_admin/status,reads the system status}
///
/// @REST{GET /_admin/status}
///
/// The call returns an object with the following attributes:
///
/// - @LIT{system.userTime}: Amount of time that this process has been
///   scheduled in user mode, measured in clock ticks divided by
///   sysconf(_SC_CLK_TCK) aka seconds.
///
/// - @LIT{system.systemTime}: mount of time that this process has
///   been scheduled in kernel mode, measured in clock ticks divided by
///   sysconf(_SC_CLK_TCK) aka seconds.
///
/// - @LIT{system.numberOfThreads}: Number of threads in this process.
///
/// - @LIT{system.residentSize}: Resident Set Size: number of pages
///   the process has in real memory.  This is just the pages which
///   count toward text, data, or stack space.  This does not include
///   pages which have not been demand-loaded in, or which are swapped
///   out.
///
/// - @LIT{system.virtualSize}: Virtual memory size in bytes.
///
/// - @LIT{system.minorPageFaults}: The number of minor faults the process has
///   made which have not required loading a memory page from disk.
///
/// - @LIT{system.majorPageFaults}: The number of major faults the process has
///   made which have required loading a memory page from disk.
////////////////////////////////////////////////////////////////////////////////

  actions.defineHttp({
    url : "_admin/status",
    context : "admin",

    callback : function (req, res) {
      var result;

      try {
        result = {};
        result.system = SYS_PROCESS_STAT();

        actions.resultOk(req, res, actions.HTTP_OK, result);
      }
      catch (err) {
        actions.resultException(req, res, err);
      }
    }
  });

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                   session actions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoAPI
/// @{
////////////////////////////////////////////////////////////////////////////////

    function GET_admin_session (req, res) {
      var result;
      var realm;

      if (req.user === null) {
        realm = "basic realm=\"arangodb\"";

        res.responseCode = actions.HTTP_UNAUTHORIZED;
        res.headers = { "www-authenticate" : realm };
      }
      else {
        var user = internal.db._collection("_users").firstExample({ user : req.user });

        if (user === null) {
          actions.resultNotFound(req, res, "unknown user '" + req.user + "'");
        }
        else {
          result = {
            user : user.user,
            permissions : user.permissions || []
          };

          actions.resultOk(req, res, actions.HTTP_OK, result);
        }
      }
    }

    function POST_admin_session (req, res) {
      actions.resultUnsupported(req, res);
    }

    function PUT_admin_session (req, res) {
      actions.resultUnsupported(req, res);
    }

    function DELETE_admin_session (req, res) {
      actions.resultUnsupported(req, res);
    }

////////////////////////////////////////////////////////////////////////////////
/// @brief session call dispatcher
////////////////////////////////////////////////////////////////////////////////

  actions.defineHttp({
    url : "_admin/session",
    context : "admin",
    prefix : false,
    callback : function (req, res) {
      try {
        if (req.requestType === actions.GET) {
          GET_admin_session(req, res);
        }
        else if (req.requestType === actions.DELETE) {
          DELETE_admin_session(req, res);
        }
        else if (req.requestType === actions.POST) {
          POST_admin_session(req, res);
        }
        else if (req.requestType === actions.PUT) {
          PUT_admin_session(req, res);
        }
        else {
          actions.resultUnsupported(req, res);
        }
      }
      catch (err) {
        actions.resultException(req, res, err);
      }
    }
  });

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

})();

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @\\}\\)"
// End:

