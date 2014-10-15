/*jshint strict: false, unused: false */
/*global require, SYS_CLUSTER_TEST, UPGRADE_ARGS: true,
  ArangoServerState, ArangoClusterComm, ArangoClusterInfo */

////////////////////////////////////////////////////////////////////////////////
/// @brief cluster actions
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
/// @author Max Neunhoeffer
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2013-2014, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var actions = require("org/arangodb/actions");
var cluster = require("org/arangodb/cluster");
var internal = require("internal");

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_cluster_test_GET
/// @brief executes a cluster roundtrip for sharding
///
/// @RESTHEADER{GET /_admin/cluster-test, Execute cluster roundtrip}
///
/// @RESTDESCRIPTION
///
/// Executes a cluster roundtrip from a coordinator to a DB server and
/// back. This call only works in a coordinator node in a cluster.
/// One can and should append an arbitrary path to the URL and the
/// part after */_admin/cluster-test* is used as the path of the HTTP
/// request which is sent from the coordinator to a DB node. Likewise,
/// any form data appended to the URL is forwarded in the request to the
/// DB node. This handler takes care of all request types (see below)
/// and uses the same request type in its request to the DB node.
///
/// The following HTTP headers are interpreted in a special way:
///
///   - *X-Shard-ID*: This specifies the ID of the shard to which the
///     cluster request is sent and thus tells the system to which DB server
///     to send the cluster request. Note that the mapping from the
///     shard ID to the responsible server has to be defined in the
///     agency under *Current/ShardLocation/<shardID>*. One has to give
///     this header, otherwise the system does not know where to send
///     the request.
///   - *X-Client-Transaction-ID*: the value of this header is taken
///     as the client transaction ID for the request
///   - *X-Timeout*: specifies a timeout in seconds for the cluster
///     operation. If the answer does not arrive within the specified
///     timeout, an corresponding error is returned and any subsequent
///     real answer is ignored. The default if not given is 24 hours.
///   - *X-Synchronous-Mode*: If set to *true* the test function uses
///     synchronous mode, otherwise the default asynchronous operation
///     mode is used. This is mainly for debugging purposes.
///   - *Host*: This header is ignored and not forwarded to the DB server.
///   - *User-Agent*: This header is ignored and not forwarded to the DB
///     server.
///
/// All other HTTP headers and the body of the request (if present, see
/// other HTTP methods below) are forwarded as given in the original request.
///
/// In asynchronous mode the DB server answers with an HTTP request of its
/// own, in synchronous mode it sends a HTTP response. In both cases the
/// headers and the body are used to produce the HTTP response of this
/// API call.
///
/// @RESTRETURNCODES
///
/// The return code can be anything the cluster request returns, as well as:
///
/// @RESTRETURNCODE{200}
/// is returned when everything went well, or if a timeout occurred. In the
/// latter case a body of type application/json indicating the timeout
/// is returned.
///
/// @RESTRETURNCODE{403}
/// is returned if ArangoDB is not running in cluster mode.
///
/// @RESTRETURNCODE{404}
/// is returned if ArangoDB was not compiled for cluster operation.
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_cluster_test_POST
/// @brief executes a cluster roundtrip for sharding
///
/// @RESTHEADER{POST /_admin/cluster-test, Execute cluster roundtrip}
///
/// @RESTBODYPARAM{body,anything,required}
///
/// @RESTDESCRIPTION
/// See GET method. The body can be any type and is simply forwarded.
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_cluster_test_PUT
/// @brief executes a cluster roundtrip for sharding
///
/// @RESTHEADER{PUT /_admin/cluster-test, Execute cluster roundtrip}
///
/// @RESTBODYPARAM{body,anything,required}
///
/// @RESTDESCRIPTION
/// See GET method. The body can be any type and is simply forwarded.
// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_cluster_test_DELETE
/// @brief executes a cluster roundtrip for sharding
///
/// @RESTHEADER{DELETE /_admin/cluster-test, Delete cluster roundtrip}
///
/// @RESTDESCRIPTION
/// See GET method. The body can be any type and is simply forwarded.
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_cluster_test_PATCH
/// @brief executes a cluster roundtrip for sharding
///
/// @RESTHEADER{PATCH /_admin/cluster-test, Update cluster roundtrip}
///
/// @RESTBODYPARAM{body,anything,required}
///
/// @RESTDESCRIPTION
/// See GET method. The body can be any type and is simply forwarded.
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_cluster_test_HEAD
/// @brief executes a cluster roundtrip for sharding
///
/// @RESTHEADER{HEAD /_admin/cluster-test, Execute cluster roundtrip}
///
/// @RESTDESCRIPTION
/// See GET method. The body can be any type and is simply forwarded.
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

actions.defineHttp({
  url: "_admin/cluster-test",
  prefix: true,

  callback : function (req, res) {
    var path;
    if (req.hasOwnProperty('suffix') && req.suffix.length !== 0) {
      path = "/"+req.suffix.join("/");
    }
    else {
      path = "/_admin/version";
    }
    var params = "";
    var shard = "";
    var p;

    for (p in req.parameters) {
      if (req.parameters.hasOwnProperty(p)) {
        if (params === "") {
          params = "?";
        }
        else {
          params += "&";
        }
        params += p+"="+ encodeURIComponent(String(req.parameters[p]));
      }
    }
    if (params !== "") {
      path += params;
    }
    var headers = {};
    var transID = "";
    var timeout = 24*3600.0;
    var asyncMode = true;

    for (p in req.headers) {
      if (req.headers.hasOwnProperty(p)) {
        if (p === "x-client-transaction-id") {
          transID = req.headers[p];
        }
        else if (p === "x-timeout") {
          timeout = parseFloat(req.headers[p]);
          if (isNaN(timeout)) {
            timeout = 24*3600.0;
          }
        }
        else if (p === "x-synchronous-mode") {
          asyncMode = false;
        }
        else if (p === "x-shard-id") {
          shard = req.headers[p];
        }
        else {
          headers[p] = req.headers[p];
        }
      }
    }

    var body;
    if (req.requestBody === undefined || typeof req.requestBody !== "string") {
      body = "";
    }
    else {
      body = req.requestBody;
    }

    var r;
    if (typeof SYS_CLUSTER_TEST === "undefined") {
      actions.resultError(req, res, actions.HTTP_NOT_FOUND,
                          "Not compiled for cluster operation");
    }
    else {
      try {
        r = SYS_CLUSTER_TEST(req, res, shard, path, transID,
                              headers, body, timeout, asyncMode);
        if (r.timeout || typeof r.errorMessage === 'string') {
          res.responseCode = actions.HTTP_OK;
          res.contentType = "application/json; charset=utf-8";
          var s = JSON.stringify(r);
          res.body = s;
        }
        else {
          res.responseCode = actions.HTTP_OK;
          res.contentType = r.headers.contentType;
          res.headers = r.headers;
          res.body = r.body;
        }
      }
      catch(err) {
        actions.resultError(req, res, actions.HTTP_FORBIDDEN, String(err));
      }
    }
  }
});

////////////////////////////////////////////////////////////////////////////////
/// @brief function to parse an authorization header
////////////////////////////////////////////////////////////////////////////////

function parseAuthorization (authorization) {
  var auth = require("internal").base64Decode(authorization.substr(6));
  var pos = auth.indexOf(":");
  if (pos === -1) {
    return {username:"root", passwd:""};
  }
  return { username: auth.substr(0, pos),
           passwd: auth.substr(pos+1) || "" };
}


////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_cluster_planner_POST
/// @brief exposes the cluster planning functionality
///
/// @RESTHEADER{POST /_admin/clusterPlanner, Produce cluster startup plan}
///
/// @RESTBODYPARAM{body,json,required}
///
/// @RESTDESCRIPTION Given a description of a cluster, this plans the details
/// of a cluster and returns a JSON description of a plan to start up this
/// cluster.
///
/// @RESTRETURNCODES
///
/// @RESTRETURNCODE{200} is returned when everything went well.
///
/// @RESTRETURNCODE{400} the posted body was not valid JSON.
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

actions.defineHttp({
  url: "_admin/clusterPlanner",
  prefix: false,

  callback : function (req, res) {
    if (ArangoServerState.disableDispatcherKickstarter() === true) {
      actions.resultError(req, res, actions.HTTP_FORBIDDEN);
      return;
    }
    if (req.requestType !== actions.POST) {
      actions.resultError(req, res, actions.HTTP_FORBIDDEN);
      return;
    }
    var userconfig;
    try {
      userconfig = JSON.parse(req.requestBody);
    }
    catch (err) {
      actions.resultError(req, res, actions.HTTP_BAD,
                          "Posted body was not valid JSON.");
      return;
    }
    var Planner = require("org/arangodb/cluster/planner").Planner;
    try {
      var p = new Planner(userconfig);
      res.responseCode = actions.HTTP_OK;
      res.contentType = "application/json; charset=utf-8";
      res.body = JSON.stringify({"clusterPlan": p.getPlan(),
                                 "config": p.config,
                                 "error": false});
    }
    catch (error) {
      actions.resultException(req, res, error, undefined, false);
    }
  }
});

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_cluster_dispatcher_POST
/// @brief exposes the dispatcher functionality to start up, shutdown,
/// relaunch, upgrade or cleanup a cluster according to a cluster plan
/// as for example provided by the kickstarter.
///
/// @RESTHEADER{POST /_admin/clusterDispatch,execute startup commands}
///
/// @RESTQUERYPARAMETERS
///
/// @RESTBODYPARAM{body,json,required}
///
/// @RESTDESCRIPTION The body must be an object with the following properties:
///
///   - *clusterPlan*: is a cluster plan (see JSF_cluster_planner_POST),
///   - *myname*: is the ID of this dispatcher, this is used to decide
///     which commands are executed locally and which are forwarded
///     to other dispatchers
///   - *action*: can be one of the following:
///
///       - "launch": the cluster is launched for the first time, all
///         data directories and log files are cleaned and created
///       - "shutdown": the cluster is shut down, the additional property
///         *runInfo* (see below) must be bound as well
///       - "relaunch": the cluster is launched again, all data directories
///         and log files are untouched and need to be there already
///       - "cleanup": use this after a shutdown to remove all data in the
///         data directories and all log files, use with caution
///       - "isHealthy": checks whether or not the processes involved
///         in the cluster are running or not. The additional property
///         *runInfo* (see above) must be bound as well
///       - "upgrade": performs an upgrade of a cluster, to this end,
///         the agency is started, and then every server is once started
///         with the "--upgrade" option, and then normally. Finally,
///         the script "verion-check.js" is run on one of the coordinators
///         for the cluster.
///
///   - *runInfo": this is needed for the "shutdown" and "isHealthy" actions
///     only and should be the structure that "launch", "relaunch" or
///     "upgrade" returned. It contains runtime information like process
///     IDs.
///
/// This call executes the plan by either doing the work personally
/// or by delegating to other dispatchers.
///
/// @RESTRETURNCODES
///
/// @RESTRETURNCODE{200} is returned when everything went well.
///
/// @RESTRETURNCODE{400} the posted body was not valid JSON, or something
/// went wrong with the startup.
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

actions.defineHttp({
  url: "_admin/clusterDispatch",
  prefix: false,

  callback : function (req, res) {
    if (ArangoServerState.disableDispatcherKickstarter() === true) {
      actions.resultError(req, res, actions.HTTP_FORBIDDEN);
      return;
    }
    if (req.requestType !== actions.POST) {
      actions.resultError(req, res, actions.HTTP_FORBIDDEN);
      return;
    }
    var input;
    try {
      input = JSON.parse(req.requestBody);
    }
    catch (error) {
      actions.resultError(req, res, actions.HTTP_BAD,
                          "Posted body was not valid JSON.");
      return;
    }
    if (!input.hasOwnProperty("clusterPlan")) {
      actions.resultError(req, res, actions.HTTP_BAD,
                          'Posted body needs a "clusterPlan" property.');
      return;
    }
    if (!input.hasOwnProperty("myname")) {
      actions.resultError(req, res, actions.HTTP_BAD,
                          'Posted body needs a "myname" property.');
      return;
    }
    var action = input.action;
    var Kickstarter, k, r;
    if (action === "launch") {
      Kickstarter = require("org/arangodb/cluster/kickstarter").Kickstarter;
      try {
        k = new Kickstarter(input.clusterPlan, input.myname);
        r = k.launch();
        res.responseCode = actions.HTTP_OK;
        res.contentType = "application/json; charset=utf-8";
        res.body = JSON.stringify(r);
      }
      catch (error2) {
        actions.resultException(req, res, error2, undefined, false);
      }
    }
    else if (action === "relaunch") {
      Kickstarter = require("org/arangodb/cluster/kickstarter").Kickstarter;
      try {
        k = new Kickstarter(input.clusterPlan, input.myname);
        r = k.relaunch();
        res.responseCode = actions.HTTP_OK;
        res.contentType = "application/json; charset=utf-8";
        res.body = JSON.stringify(r);
      }
      catch (error3) {
        actions.resultException(req, res, error3, undefined, false);
      }
    }
    else if (action === "upgrade") {
      Kickstarter = require("org/arangodb/cluster/kickstarter").Kickstarter;
      try {
        k = new Kickstarter(input.clusterPlan, input.myname);
        r = k.upgrade(input.username, input.password);
        res.responseCode = actions.HTTP_OK;
        res.contentType = "application/json; charset=utf-8";
        res.body = JSON.stringify(r);
      }
      catch (error3a) {
        actions.resultException(req, res, error3a, undefined, false);
      }
    }
    else if (action === "shutdown") {
      if (!input.hasOwnProperty("runInfo")) {
        actions.resultError(req, res, actions.HTTP_BAD,
                            'Posted body needs a "runInfo" property.');
        return;
      }
      Kickstarter = require("org/arangodb/cluster/kickstarter").Kickstarter;
      try {
        k = new Kickstarter(input.clusterPlan, input.myname);
        k.runInfo = input.runInfo;
        r = k.shutdown();
        res.responseCode = actions.HTTP_OK;
        res.contentType = "application/json; charset=utf-8";
        res.body = JSON.stringify(r);
      }
      catch (error4) {
        actions.resultException(req, res, error4, undefined, false);
      }
    }
    else if (action === "cleanup") {
      Kickstarter = require("org/arangodb/cluster/kickstarter").Kickstarter;
      try {
        k = new Kickstarter(input.clusterPlan, input.myname);
        r = k.cleanup();
        res.responseCode = actions.HTTP_OK;
        res.contentType = "application/json; charset=utf-8";
        res.body = JSON.stringify(r);
      }
      catch (error5) {
        actions.resultException(req, res, error5, undefined, false);
      }
    }
    else if (action === "isHealthy") {
      if (!input.hasOwnProperty("runInfo")) {
        actions.resultError(req, res, actions.HTTP_BAD,
                            'Posted body needs a "runInfo" property.');
        return;
      }
      Kickstarter = require("org/arangodb/cluster/kickstarter").Kickstarter;
      try {
        k = new Kickstarter(input.clusterPlan, input.myname);
        k.runInfo = input.runInfo;
        r = k.isHealthy();
        res.responseCode = actions.HTTP_OK;
        res.contentType = "application/json; charset=utf-8";
        res.body = JSON.stringify(r);
      }
      catch (error6) {
        actions.resultException(req, res, error6, undefined, false);
      }
    }
    else {
      actions.resultError(req, res, actions.HTTP_BAD,
                          'Action '+action+' not yet implemented.');
    }
  }
});

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_cluster_check_port_GET
/// @brief allows to check whether a given port is usable
///
/// @RESTHEADER{GET /_admin/clusterCheckPort, Check port}
///
/// @RESTQUERYPARAMETERS
///
/// @RESTQUERYPARAM{port,integer,required}
///
/// @RESTDESCRIPTION Checks whether the requested port is usable.
///
/// @RESTRETURNCODES
///
/// @RESTRETURNCODE{200} is returned when everything went well.
///
/// @RESTRETURNCODE{400} the parameter port was not given or is no integer.
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

actions.defineHttp({
  url: "_admin/clusterCheckPort",
  prefix: false,

  callback : function (req, res) {
    if (ArangoServerState.disableDispatcherKickstarter() === true) {
      actions.resultError(req, res, actions.HTTP_FORBIDDEN);
      return;
    }
    if (req.requestType !== actions.GET) {
      actions.resultError(req, res, actions.HTTP_FORBIDDEN);
      return;
    }
    var port;
    if (!req.parameters.hasOwnProperty("port")) {
      actions.resultError(req, res, actions.HTTP_BAD,
                          "required parameter port was not given");
      return;
    }
    try {
      port = parseInt(req.parameters.port, 10);
      if (port < 1 || port > 65535) {
        throw "banana";
      }
    }
    catch (err) {
      actions.resultError(req, res, actions.HTTP_BAD,
                          "given port was not a proper integer");
      return;
    }
    try {
      var r = internal.testPort("tcp://0.0.0.0:" + port);
      res.responseCode = actions.HTTP_OK;
      res.contentType = "application/json; charset=utf-8";
      res.body = JSON.stringify(r);
    }
    catch (err2) {
      actions.resultError(req, res, actions.HTTP_BAD,
                          "exception in port test");
    }
  }
});

////////////////////////////////////////////////////////////////////////////////
/// @brief allows to query the statistics of a DBserver in the cluster
///
/// @RESTHEADER{GET /_admin/clusterStatistics, Queries statistics of DBserver}
///
/// @RESTQUERYPARAMETERS
///
/// @RESTQUERYPARAM{DBserver,string,required}
///
/// @RESTDESCRIPTION Queries the statistics of the given DBserver
///
/// @RESTRETURNCODES
///
/// @RESTRETURNCODE{200} is returned when everything went well.
///
/// @RESTRETURNCODE{400} the parameter DBserver was not given or is not the
/// ID of a DBserver
///
/// @RESTRETURNCODE{403} server is not a coordinator.
////////////////////////////////////////////////////////////////////////////////

actions.defineHttp({
  url: "_admin/clusterStatistics",
  prefix: false,

  callback : function (req, res) {
    if (req.requestType !== actions.GET) {
      actions.resultError(req, res, actions.HTTP_FORBIDDEN, 0,
                          "only GET requests are allowed");
      return;
    }
    if (!require("org/arangodb/cluster").isCoordinator()) {
      actions.resultError(req, res, actions.HTTP_FORBIDDEN, 0,
                          "only allowed on coordinator");
      return;
    }
    if (!req.parameters.hasOwnProperty("DBserver")) {
      actions.resultError(req, res, actions.HTTP_BAD,
                          "required parameter DBserver was not given");
      return;
    }
    var DBserver = req.parameters.DBserver;
    var coord = { coordTransactionID: ArangoClusterInfo.uniqid() };
    var options = { coordTransactionID: coord.coordTransactionID, timeout:10 };
    var op = ArangoClusterComm.asyncRequest("GET","server:"+DBserver,"_system",
                                            "/_admin/statistics","",{},options);
    var r = ArangoClusterComm.wait(op);
    res.contentType = "application/json; charset=utf-8";
    if (r.status === "RECEIVED") {
      res.responseCode = actions.HTTP_OK;
      res.body = r.body;
    }
    else if (r.status === "TIMEOUT") {
      res.responseCode = actions.HTTP_BAD;
      res.body = JSON.stringify( {"error":true,
                                  "errorMessage": "operation timed out"});
    }
    else {
      res.responseCode = actions.HTTP_BAD;
      var bodyobj;
      try {
        bodyobj = JSON.parse(r.body);
      }
      catch (err) {
      }
      res.body = JSON.stringify( {"error":true,
        "errorMessage": "error from DBserver, possibly DBserver unknown",
        "body": bodyobj} );
    }
  }
});

////////////////////////////////////////////////////////////////////////////////
/// @brief allows to query the historic statistics of a DBserver in the cluster
////////////////////////////////////////////////////////////////////////////////

actions.defineHttp({
  url: "_admin/history",
  prefix: false,

  callback : function (req, res) {
    if (req.requestType !== actions.POST) {
      actions.resultError(req, res, actions.HTTP_FORBIDDEN, 0,
                          "only POST requests are allowed");
      return;

    }
    var body = actions.getJsonBody(req, res);
    if (body === undefined) {
        return;
    }
    var DBserver = req.parameters.DBserver;

    //build query
    var figures = body.figures;
    var filterString = " filter u.time > @startDate";
    var bind = {
      startDate: (new Date().getTime() / 1000) - 20 * 60
    };

    if (cluster.isCoordinator() && !req.parameters.hasOwnProperty("DBserver")) {
      filterString += " filter u.clusterId == @serverId";
      bind.serverId = cluster.coordinatorId();
    }

    var returnValue = " return u";
    if (figures) {
      returnValue = " return { time : u.time, server : {uptime : u.server.uptime} ";

      var groups = {};
      figures.forEach(function(f) {
          var g = f.split(".")[0];
          if (!groups[g]) {
              groups[g] = [];
          }
          groups[g].push(f.split(".")[1] + " : u." + f);
      });
      Object.keys(groups).forEach(function(key) {
          returnValue +=  ", " + key + " : {" + groups[key]  +"}";
      });
      returnValue += "}";
    }
    // allow at most ((60 / 10) * 20) * 2 documents to prevent total chaos
    var myQueryVal = "FOR u in _statistics " + filterString + " LIMIT 240 SORT u.time" + returnValue;

    if (!req.parameters.hasOwnProperty("DBserver")) {
        // query the local statistics collection
        var cursor = internal.AQL_QUERY(myQueryVal, bind);
        res.contentType = "application/json; charset=utf-8";
        if (cursor instanceof Error) {
            res.responseCode = actions.HTTP_BAD;
            res.body = JSON.stringify( {"error":true,
                "errorMessage": "an error occured"});
        }
        res.responseCode = actions.HTTP_OK;
        res.body = JSON.stringify({result : cursor.docs});
    }
    else {
        // query a remote statistics collection
        var coord = { coordTransactionID: ArangoClusterInfo.uniqid() };
        var options = { coordTransactionID: coord.coordTransactionID, timeout:10 };
        var op = ArangoClusterComm.asyncRequest("POST","server:"+DBserver,"_system",
            "/_api/cursor",JSON.stringify({query: myQueryVal, bindVars: bind}),{},options);
        var r = ArangoClusterComm.wait(op);
        res.contentType = "application/json; charset=utf-8";
        if (r.status === "RECEIVED") {
            res.responseCode = actions.HTTP_OK;
            res.body = r.body;
        }
        else if (r.status === "TIMEOUT") {
            res.responseCode = actions.HTTP_BAD;
            res.body = JSON.stringify( {"error":true,
                "errorMessage": "operation timed out"});
        }
        else {
            res.responseCode = actions.HTTP_BAD;
            var bodyobj;
            try {
                bodyobj = JSON.parse(r.body);
            }
            catch (err) {
            }
            res.body = JSON.stringify( {"error":true,
                "errorMessage": "error from DBserver, possibly DBserver unknown",
                "body": bodyobj} );
        }
    }
  }
});

////////////////////////////////////////////////////////////////////////////////
/// @brief bootstraps the all db servers
////////////////////////////////////////////////////////////////////////////////

actions.defineHttp({
  url: "_admin/cluster/bootstrapDbServers",
  prefix: false,

  callback: function (req, res) {
    var body = actions.getJsonBody(req, res);

    try {
      var result = cluster.bootstrapDbServers(body.isRelaunch);

      if (result) {
        actions.resultOk(req, res, actions.HTTP_OK);
      }
      else {
        actions.resultBad(req, res);
      }
    }
    catch(err) {
      actions.resultException(req, res, err);
    }
  }
});

////////////////////////////////////////////////////////////////////////////////
/// @brief bootstraps one db server
////////////////////////////////////////////////////////////////////////////////

actions.defineHttp({
  url: "_admin/cluster/bootstrapDbServer",
  prefix: false,

  callback: function (req, res) {
    var body = actions.getJsonBody(req, res);

    UPGRADE_ARGS = {
      isCluster: true,
      isDbServer: true,
      isRelaunch: body.isRelaunch
    };

    try {
      var func = internal.loadStartup("server/bootstrap/db-server.js");
      var result = func && func();

      if (result) {
        actions.resultOk(req, res, actions.HTTP_OK);
      }
      else {
        actions.resultBad(req, res);
      }
    }
    catch(err) {
      actions.resultException(req, res, err);
    }
  }
});

////////////////////////////////////////////////////////////////////////////////
/// @brief upgrade cluster database
////////////////////////////////////////////////////////////////////////////////

actions.defineHttp({
  url: "_admin/cluster/upgradeClusterDatabase",
  prefix: false,

  callback: function (req, res) {
    var body = actions.getJsonBody(req, res);

    UPGRADE_ARGS = {
      isCluster: true,
      isCoordinator: true,
      isRelaunch: body.isRelaunch || false,
      upgrade: body.upgrade || false
    };

    try {
      var result = internal.loadStartup("server/upgrade-database.js");

      if (result) {
        actions.resultOk(req, res, actions.HTTP_OK);
      }
      else {
        actions.resultBad(req, res);
      }
    }
    catch(err) {
      actions.resultException(req, res, err);
    }
  }
});

////////////////////////////////////////////////////////////////////////////////
/// @brief bootstraps the coordinator
////////////////////////////////////////////////////////////////////////////////

actions.defineHttp({
  url: "_admin/cluster/bootstrapCoordinator",
  allowUseDatabase: true,
  prefix: false,

  callback: function (req, res) {
    try {
      var func = internal.loadStartup("server/bootstrap/coordinator.js");
      var result = func && func();

      if (result) {
        actions.resultOk(req, res, actions.HTTP_OK);
      }
      else {
        actions.resultBad(req, res);
      }
    }
    catch(err) {
      actions.resultException(req, res, err);
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
