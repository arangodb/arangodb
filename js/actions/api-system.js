/*jshint strict: false */
/*global require, ArangoServerState */

////////////////////////////////////////////////////////////////////////////////
/// @brief administration actions
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2014 ArangoDB GmbH, Cologne, Germany
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
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var arangodb = require("org/arangodb");
var actions = require("org/arangodb/actions");
var internal = require("internal");
var console = require("console");
var users = require("org/arangodb/users");

var targetDatabaseVersion = require("org/arangodb/database-version").CURRENT_VERSION;

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief main routing action
////////////////////////////////////////////////////////////////////////////////

actions.defineHttp({
  url : "",
  prefix : true,

  callback : function (req, res) {
    try {
      actions.routeRequest(req, res);
    }
    catch (err) {
      if (err instanceof internal.SleepAndRequeue) {
        throw err;
      }

      var msg = 'A runtime error occurred while executing an action: '
                + String(err) + " " + String(err.stack);

      if (err.hasOwnProperty("route")) {
        actions.errorFunction(err.route, msg)(req, res);
      }
      else {
        actions.resultError(req, res, actions.HTTP_SERVER_ERROR, actions.HTTP_SERVER_ERROR, msg);
      }
    }
  }
});

////////////////////////////////////////////////////////////////////////////////
/// @brief _admin/database/version
/// @startDocuBlock JSF_get_admin_database_version
///
/// @RESTHEADER{GET /_admin/database/target-version, Return the required version of the database}
///
/// @RESTDESCRIPTION
///
/// Returns the database-version that this server requires.
/// The version is returned in the *version* attribute of the result.
///
/// @RESTRETURNCODES
///
/// @RESTRETURNCODE{200}
/// Is returned in all cases.
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

actions.defineHttp({
  url : "_admin/database/target-version",
  prefix : false,

  callback : function (req, res) {
    actions.resultOk(req, res, actions.HTTP_OK, { version: String(targetDatabaseVersion) });
  }
});

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the role of a server in a cluster
/// @startDocuBlock JSF_get_admin_server_role
///
/// @RESTHEADER{GET /_admin/server/role, Return role of a server in a cluster}
///
/// @RESTDESCRIPTION
///
/// Returns the role of a server in a cluster.
/// The role is returned in the *role* attribute of the result.
/// Possible return values for *role* are:
/// - *COORDINATOR*: the server is a coordinator in a cluster
/// - *PRIMARY*: the server is a primary database server in a cluster
/// - *SECONDARY*: the server is a secondary database server in a cluster
/// - *UNDEFINED*: in a cluster, *UNDEFINED* is returned if the server role cannot be
///    determined. On a single server, *UNDEFINED* is the only possible return
///    value.
///
/// @RESTRETURNCODES
///
/// @RESTRETURNCODE{200}
/// Is returned in all cases.
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

actions.defineHttp({
  url : "_admin/server/role",
  prefix : false,

  callback : function (req, res) {
    actions.resultOk(req, res, actions.HTTP_OK, { role: ArangoServerState.role() });
  }
});

////////////////////////////////////////////////////////////////////////////////
/// @brief flushes the write-ahead log
/// @startDocuBlock JSF_put_admin_wal_flush
///
/// @RESTHEADER{PUT /_admin/wal/flush, Flushes the write-ahead log}
///
/// @RESTURLPARAMETERS
///
/// @RESTURLPARAM{waitForSync,boolean,optional}
/// Whether or not the operation should block until the not-yet synchronized
/// data in the write-ahead log was synchronized to disk.
///
/// @RESTURLPARAM{waitForCollector,boolean,optional}
/// Whether or not the operation should block until the data in the flushed
/// log has been collected by the write-ahead log garbage collector. Note that
/// setting this option to *true* might block for a long time if there are
/// long-running transactions and the write-ahead log garbage collector cannot
/// finish garbage collection.
///
/// @RESTDESCRIPTION
///
/// Flushes the write-ahead log. By flushing the currently active write-ahead
/// logfile, the data in it can be transferred to collection journals and
/// datafiles. This is useful to ensure that all data for a collection is
/// present in the collection journals and datafiles, for example, when dumping
/// the data of a collection.
///
/// @RESTRETURNCODES
///
/// @RESTRETURNCODE{200}
/// Is returned if the operation succeeds.
///
/// @RESTRETURNCODE{405}
/// is returned when an invalid HTTP method is used.
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

actions.defineHttp({
  url : "_admin/wal/flush",
  prefix : false,

  callback : function (req, res) {
    if (req.requestType !== actions.PUT) {
      actions.resultUnsupported(req, res);
      return;
    }

    internal.wal.flush(req.parameters.waitForSync === "true",
                       req.parameters.waitForCollector === "true");
    actions.resultOk(req, res, actions.HTTP_OK);
  }
});

////////////////////////////////////////////////////////////////////////////////
/// @brief configures the write-ahead log
/// @startDocuBlock JSF_put_admin_wal_properties
///
/// @RESTHEADER{PUT /_admin/wal/properties, Configures the write-ahead log}
///
/// @RESTDESCRIPTION
///
/// Configures the behavior of the write-ahead log. The body of the request
/// must be a JSON object with the following attributes:
/// - *allowOversizeEntries*: whether or not operations that are bigger than a
///   single logfile can be executed and stored
/// - *logfileSize*: the size of each write-ahead logfile
/// - *historicLogfiles*: the maximum number of historic logfiles to keep
/// - *reserveLogfiles*: the maximum number of reserve logfiles that ArangoDB
///   allocates in the background
/// - *throttleWait*: the maximum wait time that operations will wait before
///   they get aborted if case of write-throttling (in milliseconds)
/// - *throttleWhenPending*: the number of unprocessed garbage-collection
///   operations that, when reached, will activate write-throttling. A value of
///   *0* means that write-throttling will not be triggered.
///
/// Specifying any of the above attributes is optional. Not specified attributes
/// will be ignored and the configuration for them will not be modified.
///
/// @RESTRETURNCODES
///
/// @RESTRETURNCODE{200}
/// Is returned if the operation succeeds.
///
/// @RESTRETURNCODE{405}
/// is returned when an invalid HTTP method is used.
/// @endDocuBlock
///
/// @EXAMPLES
///
/// @EXAMPLE_ARANGOSH_RUN{RestWalPropertiesPut}
///     var url = "/_admin/wal/properties";
///     var body = {
///       logfileSize: 32 * 1024 * 1024,
///       allowOversizeEntries: true
///     };
///     var response = logCurlRequest('PUT', url, JSON.stringify(body));
///
///     assert(response.code === 200);
///
///     logJsonResponse(response);
/// @END_EXAMPLE_ARANGOSH_RUN
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief retrieves the configuration of the write-ahead log
/// @startDocuBlock JSF_get_admin_wal_properties
///
/// @RESTHEADER{GET /_admin/wal/properties, Retrieves the configuration of the write-ahead log}
///
/// @RESTDESCRIPTION
///
/// Retrieves the configuration of the write-ahead log. The result is a JSON
/// array with the following attributes:
/// - *allowOversizeEntries*: whether or not operations that are bigger than a
///   single logfile can be executed and stored
/// - *logfileSize*: the size of each write-ahead logfile
/// - *historicLogfiles*: the maximum number of historic logfiles to keep
/// - *reserveLogfiles*: the maximum number of reserve logfiles that ArangoDB
///   allocates in the background
/// - *syncInterval*: the interval for automatic synchronization of not-yet
///   synchronized write-ahead log data (in milliseconds)
/// - *throttleWait*: the maximum wait time that operations will wait before
///   they get aborted if case of write-throttling (in milliseconds)
/// - *throttleWhenPending*: the number of unprocessed garbage-collection
///   operations that, when reached, will activate write-throttling. A value of
///   *0* means that write-throttling will not be triggered.
///
/// @RESTRETURNCODES
///
/// @RESTRETURNCODE{200}
/// Is returned if the operation succeeds.
///
/// @RESTRETURNCODE{405}
/// is returned when an invalid HTTP method is used.
/// @endDocuBlock
///
/// @EXAMPLES
///
/// @EXAMPLE_ARANGOSH_RUN{RestWalPropertiesGet}
///     var url = "/_admin/wal/properties";
///     var response = logCurlRequest('GET', url);
///
///     assert(response.code === 200);
///
///     logJsonResponse(response);
/// @END_EXAMPLE_ARANGOSH_RUN
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

actions.defineHttp({
  url : "_admin/wal/properties",
  prefix : false,

  callback : function (req, res) {
    var result;

    if (req.requestType === actions.PUT) {
      var body = actions.getJsonBody(req, res);
      if (body === undefined) {
        return;
      }

      result = internal.wal.properties(body);
      actions.resultOk(req, res, actions.HTTP_OK, result);
    }
    else if (req.requestType === actions.GET) {
      result = internal.wal.properties();
      actions.resultOk(req, res, actions.HTTP_OK, result);
    }
    else {
      actions.resultUnsupported(req, res);
    }
  }
});

////////////////////////////////////////////////////////////////////////////////
/// @brief reloads the server authentication information
////////////////////////////////////////////////////////////////////////////////

actions.defineHttp({
  url : "_admin/auth/reload",
  prefix : false,

  callback : function (req, res) {
    users.reload();
    actions.resultOk(req, res, actions.HTTP_OK);
  }
});

////////////////////////////////////////////////////////////////////////////////
/// @brief reloads the AQL user functions
////////////////////////////////////////////////////////////////////////////////

actions.defineHttp({
  url : "_admin/aql/reload",
  prefix : false,

  callback : function (req, res) {
    internal.reloadAqlFunctions();
    actions.resultOk(req, res, actions.HTTP_OK);
  }
});

////////////////////////////////////////////////////////////////////////////////
/// @brief reloads the routing information
/// @startDocuBlock JSF_get_admin_routing_reloads
///
/// @RESTHEADER{POST /_admin/routing/reload, Reloads the routing information}
///
/// @RESTDESCRIPTION
///
/// Reloads the routing information from the collection *routing*.
///
/// @RESTRETURNCODES
///
/// @RESTRETURNCODE{200}
/// Routing information was reloaded successfully.
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

actions.defineHttp({
  url : "_admin/routing/reload",
  prefix : false,

  callback : function (req, res) {
    internal.executeGlobalContextFunction("reloadRouting");
    console.debug("about to flush the routing cache");
    actions.resultOk(req, res, actions.HTTP_OK);
  }
});

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the current routing information
////////////////////////////////////////////////////////////////////////////////

actions.defineHttp({
  url : "_admin/routing/routes",
  prefix : false,

  callback : function (req, res) {
    actions.resultOk(req, res, actions.HTTP_OK, actions.routingCache());
  }
});

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the system time
/// @startDocuBlock JSF_get_admin_time
///
/// @RESTHEADER{GET /_admin/time, Return system time}
///
/// @RESTDESCRIPTION
///
/// The call returns an object with the attribute *time*. This contains the
/// current system time as a Unix timestamp with microsecond precision.
///
/// @RESTRETURNCODES
///
/// @RESTRETURNCODE{200}
/// Time was returned successfully.
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

actions.defineHttp({
  url : "_admin/time",
  prefix : false,

  callback : function (req, res) {
    actions.resultOk(req, res, actions.HTTP_OK, { time : internal.time() });
  }
});

////////////////////////////////////////////////////////////////////////////////
/// @brief sleeps, this is useful for timeout tests
/// @startDocuBlock JSF_get_admin_sleep
///
/// @RESTHEADER{GET /_admin/sleep?duration=5, Sleep for 5 seconds}
///
/// @RESTDESCRIPTION
///
/// The call returns an object with the attribute *duration*. This takes
/// as many seconds as the duration argument says.
///
/// @RESTRETURNCODES
///
/// @RESTRETURNCODE{200}
/// Sleep was conducted successfully.
/// @DendocuBlock
////////////////////////////////////////////////////////////////////////////////

actions.defineHttp({
  url : "_admin/sleep",
  prefix : false,

  callback : function (req, res) {
    var time = parseFloat(req.parameters.duration);
    if (isNaN(time)) {
      time = 3.0;
    }
    internal.wait(time);
    actions.resultOk(req, res, actions.HTTP_OK, { duration : time });
  }
});

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the request
/// @startDocuBlock JSF_get_admin_echo
///
/// @RESTHEADER{GET /_admin/echo, Return current request}
///
/// @RESTDESCRIPTION
///
/// The call returns an object with the following attributes:
///
/// - *headers*: a list of HTTP headers received
///
/// - *requestType*: the HTTP request method (e.g. GET)
///
/// - *parameters*: list of URL parameters received
///
/// @RESTRETURNCODES
///
/// @RESTRETURNCODE{200}
/// Echo was returned successfully.
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

actions.defineHttp({
  url : "_admin/echo",
  prefix : true,

  callback : function (req, res) {
    res.responseCode = actions.HTTP_OK;
    res.contentType = "application/json; charset=utf-8";
    res.body = JSON.stringify(req);
  }
});

////////////////////////////////////////////////////////////////////////////////
/// @brief returns system status information for the server
/// @startDocuBlock JSF_get_admin_statistics
///
/// @RESTHEADER{GET /_admin/statistics, Read the statistics}
///
/// @RESTDESCRIPTION
///
/// Returns the statistics information. The returned object contains the
/// statistics figures grouped together according to the description returned by
/// *_admin/statistics-description*. For instance, to access a figure *userTime*
/// from the group *system*, you first select the sub-object describing the
/// group stored in *system* and in that sub-object the value for *userTime* is
/// stored in the attribute of the same name.
///
/// In case of a distribution, the returned object contains the total count in
/// *count* and the distribution list in *counts*. The sum (or total) of the
/// individual values is returned in *sum*.
///
/// @RESTRETURNCODES
///
/// @RESTRETURNCODE{200}
/// Statistics were returned successfully.
///
/// @EXAMPLES
///
/// @EXAMPLE_ARANGOSH_RUN{RestAdminStatistics1}
///     var url = "/_admin/statistics";
///     var response = logCurlRequest('GET', url);
///
///     assert(response.code === 200);
///
///     logJsonResponse(response);
/// @END_EXAMPLE_ARANGOSH_RUN
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

actions.defineHttp({
  url : "_admin/statistics",
  prefix : false,

  callback : function (req, res) {
    var result;

    try {
      result = {};
      result.time = internal.time();
      result.system = internal.processStatistics();
      result.client = internal.clientStatistics();
      result.http = internal.httpStatistics();
      result.server = internal.serverStatistics();

      actions.resultOk(req, res, actions.HTTP_OK, result);
    }
    catch (err) {
      actions.resultException(req, res, err, undefined, false);
    }
  }
});

////////////////////////////////////////////////////////////////////////////////
/// @brief returns statistics description
/// @startDocuBlock JSF_get_admin_statistics_description
///
/// @RESTHEADER{GET /_admin/statistics-description, Statistics description}
///
/// @RESTDESCRIPTION
///
/// Returns a description of the statistics returned by */_admin/statistics*.
/// The returned objects contains a list of statistics groups in the attribute
/// *groups* and a list of statistics figures in the attribute *figures*.
///
/// A statistics group is described by
///
/// - *group*: The identifier of the group.
/// - *name*: The name of the group.
/// - *description*: A description of the group.
///
/// A statistics figure is described by
///
/// - *group*: The identifier of the group to which this figure belongs.
/// - *identifier*: The identifier of the figure. It is unique within the group.
/// - *name*: The name of the figure.
/// - *description*: A description of the figure.
/// - *type*: Either *current*, *accumulated*, or *distribution*.
/// - *cuts*: The distribution vector.
/// - *units*: Units in which the figure is measured.
///
/// @RESTRETURNCODES
///
/// @RESTRETURNCODE{200}
/// Description was returned successfully.
///
/// @EXAMPLES
///
/// @EXAMPLE_ARANGOSH_RUN{RestAdminStatisticsDescription1}
///     var url = "/_admin/statistics-description";
///     var response = logCurlRequest('GET', url);
///
///     assert(response.code === 200);
///
///     logJsonResponse(response);
/// @END_EXAMPLE_ARANGOSH_RUN
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

actions.defineHttp({
  url : "_admin/statistics-description",
  prefix : false,

  callback : function (req, res) {
    var result;

    try {
      result = {
        groups: [
          {
            group: "system",
            name: "Process Statistics",
            description: "Statistics about the ArangoDB process"
          },

          {
            group: "client",
            name: "Client Connection Statistics",
            description: "Statistics about the connections."
          },

          {
            group: "http",
            name: "HTTP Request Statistics",
            description: "Statistics about the HTTP requests."
          },

          {
            group: "server",
            name: "Server Statistics",
            description: "Statistics about the ArangoDB server"
          }

        ],

        figures: [

          // .............................................................................
          // system statistics
          // .............................................................................

          {
            group: "system",
            identifier: "userTime",
            name: "User Time",
            description: "Amount of time that this process has been scheduled in user mode, " +
                         "measured in seconds.",
            type: "accumulated",
            units: "seconds"
          },

          {
            group: "system",
            identifier: "systemTime",
            name: "System Time",
            description: "Amount of time that this process has been scheduled in kernel mode, " +
                         "measured in seconds.",
            type: "accumulated",
            units: "seconds"
          },

          {
            group: "system",
            identifier: "numberOfThreads",
            name: "Number of Threads",
            description: "Number of threads in the arangod process.",
            type: "current",
            units: "number"
          },

          {
            group: "system",
            identifier: "residentSize",
            name: "Resident Set Size",
            description: "The total size of the number of pages the process has in real memory. " +
                         "This is just the pages which count toward text, data, or stack space. " +
                         "This does not include pages which have not been demand-loaded in, " +
                         "or which are swapped out. The resident set size is reported in bytes.",
            type: "current",
            units: "bytes"
          },

          {
            group: "system",
            identifier: "residentSizePercent",
            name: "Resident Set Size",
            description: "The percentage of physical memory used by the process as resident " +
                         "set size.",
            type: "current",
            units: "percent"
          },

          {
            group: "system",
            identifier: "virtualSize",
            name: "Virtual Memory Size",
            description: "On Windows, this figure contains the total amount of memory that the " +
                         "memory manager has committed for the arangod process. On other " +
                         "systems, this figure contains The size of the virtual memory the " +
                         "process is using.",
            type: "current",
            units: "bytes"
          },

          {
            group: "system",
            identifier: "minorPageFaults",
            name: "Minor Page Faults",
            description: "The number of minor faults the process has made which have " +
                         "not required loading a memory page from disk. This figure is " +
                         "not reported on Windows.",
            type: "accumulated",
            units: "number"
          },

          {
            group: "system",
            identifier: "majorPageFaults",
            name: "Major Page Faults",
            description: "On Windows, this figure contains the total number of page faults. " +
                         "On other system, this figure contains the number of major faults the " +
                         "process has made which have required loading a memory page from disk.",
            type: "accumulated",
            units: "number"
          },

          // .............................................................................
          // client statistics
          // .............................................................................

          {
            group: "client",
            identifier: "httpConnections",
            name: "Client Connections",
            description: "The number of connections that are currently open.",
            type: "current",
            units: "number"
          },

          {
            group: "client",
            identifier: "totalTime",
            name: "Total Time",
            description: "Total time needed to answer a request.",
            type: "distribution",
            cuts: internal.requestTimeDistribution,
            units: "seconds"
          },

          {
            group: "client",
            identifier: "requestTime",
            name: "Request Time",
            description: "Request time needed to answer a request.",
            type: "distribution",
            cuts: internal.requestTimeDistribution,
            units: "seconds"
          },

          {
            group: "client",
            identifier: "queueTime",
            name: "Queue Time",
            description: "Queue time needed to answer a request.",
            type: "distribution",
            cuts: internal.requestTimeDistribution,
            units: "seconds"
          },

          {
            group: "client",
            identifier: "bytesSent",
            name: "Bytes Sent",
            description: "Bytes sents for a request.",
            type: "distribution",
            cuts: internal.bytesSentDistribution,
            units: "bytes"
          },

          {
            group: "client",
            identifier: "bytesReceived",
            name: "Bytes Received",
            description: "Bytes receiveds for a request.",
            type: "distribution",
            cuts: internal.bytesReceivedDistribution,
            units: "bytes"
          },

          {
            group: "client",
            identifier: "connectionTime",
            name: "Connection Time",
            description: "Total connection time of a client.",
            type: "distribution",
            cuts: internal.connectionTimeDistribution,
            units: "seconds"
          },

          {
            group: "http",
            identifier: "requestsTotal",
            name: "Total requests",
            description: "Total number of HTTP requests.",
            type: "accumulated",
            units: "number"
          },

          {
            group: "http",
            identifier: "requestsAsync",
            name: "Async requests",
            description: "Number of asynchronously executed HTTP requests.",
            type: "accumulated",
            units: "number"
          },

          {
            group: "http",
            identifier: "requestsGet",
            name: "HTTP GET requests",
            description: "Number of HTTP GET requests.",
            type: "accumulated",
            units: "number"
          },

          {
            group: "http",
            identifier: "requestsHead",
            name: "HTTP HEAD requests",
            description: "Number of HTTP HEAD requests.",
            type: "accumulated",
            units: "number"
          },

          {
            group: "http",
            identifier: "requestsPost",
            name: "HTTP POST requests",
            description: "Number of HTTP POST requests.",
            type: "accumulated",
            units: "number"
          },

          {
            group: "http",
            identifier: "requestsPut",
            name: "HTTP PUT requests",
            description: "Number of HTTP PUT requests.",
            type: "accumulated",
            units: "number"
          },

          {
            group: "http",
            identifier: "requestsPatch",
            name: "HTTP PATCH requests",
            description: "Number of HTTP PATCH requests.",
            type: "accumulated",
            units: "number"
          },

          {
            group: "http",
            identifier: "requestsDelete",
            name: "HTTP DELETE requests",
            description: "Number of HTTP DELETE requests.",
            type: "accumulated",
            units: "number"
          },

          {
            group: "http",
            identifier: "requestsOptions",
            name: "HTTP OPTIONS requests",
            description: "Number of HTTP OPTIONS requests.",
            type: "accumulated",
            units: "number"
          },

          {
            group: "http",
            identifier: "requestsOther",
            name: "other HTTP requests",
            description: "Number of other HTTP requests.",
            type: "accumulated",
            units: "number"
          },


          // .............................................................................
          // server statistics
          // .............................................................................

          {
            group: "server",
            identifier: "uptime",
            name: "Server Uptime",
            description: "Number of seconds elapsed since server start.",
            type: "current",
            units: "seconds"
          },

          {
            group: "server",
            identifier: "physicalMemory",
            name: "Physical Memory",
            description: "Physical memory in bytes.",
            type: "current",
            units: "bytes"
          }

        ]
      };

      actions.resultOk(req, res, actions.HTTP_OK, result);
    }
    catch (err) {
      actions.resultException(req, res, err, undefined, false);
    }
  }
});

////////////////////////////////////////////////////////////////////////////////
/// @brief executes one or multiple tests on the server
/// @startDocuBlock JSF_post_admin_test
///
/// @RESTHEADER{POST /_admin/test, Runs tests on server}
///
/// @RESTBODYPARAM{body,javascript,required}
/// A JSON body containing an attribute "tests" which lists the files
/// containing the test suites.
///
/// @RESTDESCRIPTION
///
/// Executes the specified tests on the server and returns an object with the
/// test results. The object has an attribute "error" which states whether
/// any error occurred. The object also has an attribute "passed" which
/// indicates which tests passed and which did not.
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

actions.defineHttp({
  url : "_admin/test",
  prefix : false,

  callback : function (req, res) {
    var body = actions.getJsonBody(req, res);

    if (body === undefined) {
      return;
    }

    var tests = body.tests;
    if (! Array.isArray(tests)) {
      actions.resultError(req, res,
                          actions.HTTP_BAD, arangodb.ERROR_HTTP_BAD_PARAMETER,
                          "expected attribute 'tests' is missing");
      return;
    }

    var jsUnity = require("jsunity");
    var testResults = { passed: { }, error: false };

    tests.forEach (function (test) {
      var result = false;
      try {
        result = jsUnity.runTest(test);
      }
      catch (err) {
      }
      testResults.passed[test] = result;
      if (! result) {
        testResults.error = true;
      }
    });

    actions.resultOk(req, res, actions.HTTP_OK, testResults);
  }
});

////////////////////////////////////////////////////////////////////////////////
/// @brief executes a JavaScript program on the server
/// @startDocuBlock JSF_post_admin_execute
///
/// @RESTHEADER{POST /_admin/execute, Execute program}
///
/// @RESTBODYPARAM{body,javascript,required}
/// The body to be executed.
///
/// @RESTDESCRIPTION
///
/// Executes the javascript code in the body on the server as the body
/// of a function with no arguments. If you have a *return* statement
/// then the return value you produce will be returned as content type
/// *application/json*. If the parameter *returnAsJSON* is set to
/// *true*, the result will be a JSON object describing the return value
/// directly, otherwise a string produced by JSON.stringify will be
/// returned.
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

actions.defineHttp({
  url : "_admin/execute",
  prefix : false,

  callback : function (req, res) {
    /*jshint evil: true */
    var body = req.requestBody;
    var result;

    console.warn("about to execute: '%s'", body);

    if (body !== "") {
      result = eval("(function() {" + body + "}());");
    }

    if (req.parameters.hasOwnProperty("returnAsJSON") &&
        req.parameters.returnAsJSON === "true") {
      actions.resultOk(req, res, actions.HTTP_OK, result);
    }
    else {
      actions.resultOk(req, res, actions.HTTP_OK, JSON.stringify(result));
    }
  }
});

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
