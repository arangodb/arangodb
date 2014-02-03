/*jslint indent: 2, nomen: true, maxlen: 100, sloppy: true, vars: true, white: true, plusplus: true, evil: true */
/*global require, exports, module, SYS_CLUSTER_TEST */

////////////////////////////////////////////////////////////////////////////////
/// @brief cluster actions
///
/// @file js/api-cluster.js
///
/// DISCLAIMER
///
/// Copyright 2014-2014 triagens GmbH, Cologne, Germany
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
/// @author Max Neunhoeffer
/// @author Copyright 2014, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var actions = require("org/arangodb/actions");

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------


// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @fn JSF_cluster_test_GET
/// @brief executes a cluster roundtrip for sharding
///
/// @RESTHEADER{GET /_admin/cluster-test,executes a cluster roundtrip}
///
/// @RESTDESCRIPTION
///
/// Executes a cluster roundtrip from a coordinator to a DB server and
/// back. This call only works in a coordinator node in a cluster.
/// One can and should append an arbitrary path to the URL and the
/// part after `/_admin/cluster-test` is used as the path of the HTTP
/// request which is sent from the coordinator to a DB node. Likewise,
/// any form data appended to the URL is forwarded in the request to the
/// DB node. This handler takes care of all request types (see below)
/// and uses the same request type in its request to the DB node.
///
/// The following HTTP headers are interpreted in a special way:
///
///   - `X-Shard-ID`: This specifies the ID of the shard to which the
///     cluster request is sent and thus tells the system to which DB server
///     to send the cluster request. Note that the mapping from the 
///     shard ID to the responsible server has to be defined in the 
///     agency under `Current/ShardLocation/<shardID>`. One has to give
///     this header, otherwise the system does not know where to send
///     the request.
///   - `X-Client-Transaction-ID`: the value of this header is taken
///     as the client transaction ID for the request
///   - `X-Timeout`: specifies a timeout in seconds for the cluster
///     operation. If the answer does not arrive within the specified
///     timeout, an corresponding error is returned and any subsequent
///     real answer is ignored. The default if not given is 24 hours.
///   - `X-Synchronous-Mode`: If set to `true` the test function uses
///     synchronous mode, otherwise the default asynchronous operation
///     mode is used. This is mainly for debugging purposes.
///   - `Host`: This header is ignored and not forwarded to the DB server.
///   - `User-Agent`: This header is ignored and not forwarded to the DB 
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
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @fn JSF_cluster_test_POST
/// @brief executes a cluster roundtrip for sharding
///
/// @RESTHEADER{POST /_admin/cluster-test,executes a cluster roundtrip}
///
/// @RESTBODYPARAM{body,anything,required}
///
/// @RESTDESCRIPTION
/// See GET method. The body can be any type and is simply forwarded.
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @fn JSF_cluster_test_PUT
/// @brief executes a cluster roundtrip for sharding
///
/// @RESTHEADER{PUT /_admin/cluster-test,executes a cluster roundtrip}
///
/// @RESTBODYPARAM{body,anything,required}
///
/// @RESTDESCRIPTION
/// See GET method. The body can be any type and is simply forwarded.
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @fn JSF_cluster_test_DELETE
/// @brief executes a cluster roundtrip for sharding
///
/// @RESTHEADER{DELETE /_admin/cluster-test,executes a cluster roundtrip}
///
/// @RESTDESCRIPTION
/// See GET method. The body can be any type and is simply forwarded.
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @fn JSF_cluster_test_PATCH
/// @brief executes a cluster roundtrip for sharding
///
/// @RESTHEADER{PATCH /_admin/cluster-test,executes a cluster roundtrip}
///
/// @RESTBODYPARAM{body,anything,required}
///
/// @RESTDESCRIPTION
/// See GET method. The body can be any type and is simply forwarded.
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @fn JSF_cluster_test_HEAD
/// @brief executes a cluster roundtrip for sharding
///
/// @RESTHEADER{HEAD /_admin/cluster-test,executes a cluster roundtrip}
///
/// @RESTDESCRIPTION
/// See GET method. The body can be any type and is simply forwarded.
////////////////////////////////////////////////////////////////////////////////

actions.defineHttp({
  url : "_admin/cluster-test",
  context : "admin",
  prefix : true,

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
/// @fn JSF_cluster_kickstarter_POST
/// @brief exposes the kickstarter planning function
///
/// @RESTHEADER{POST /_admin/kickstarter,produce a cluster startup plan}
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
////////////////////////////////////////////////////////////////////////////////

actions.defineHttp({
  url : "_admin/kickstarter",
  context : "admin",
  prefix : "false",
  callback : function (req, res) {
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
    var Kickstarter = require("org/arangodb/cluster/kickstarter").Kickstarter;
    try {
      var k = new Kickstarter(userconfig);
      res.responseCode = actions.HTTP_OK;
      res.contentType = "application/json; charset=utf-8";
      res.body = JSON.stringify({"dispatchers": k.dispatchers,
                                 "commands": k.commands,
                                 "config": k.config,
                                 "agencyData": k.agencyData});
    }
    catch (error) {
      actions.resultException(req, res, error, undefined, false);
    }
  }
});

////////////////////////////////////////////////////////////////////////////////
/// @fn JSF_cluster_dispatcher_POST
/// @brief exposes the dispatcher functionality to start up a cluster 
/// according to a startup plan as for example provided by the kickstarter.
///
/// @RESTHEADER{POST /_admin/dispatch,execute startup commands}
///
/// @RESTQUERYPARAMETERS
///
/// @RESTBODYPARAM{body,json,required}
///
/// @RESTDESCRIPTION Given a list of cluster startup commands, this
/// call executes the plan by either starting up processes personally
/// or by delegating to other dispatchers.
/// 
/// @RESTRETURNCODES
///
/// @RESTRETURNCODE{200} is returned when everything went well.
///
/// @RESTRETURNCODE{400} the posted body was not valid JSON, or something
/// went wrong with the startup.
////////////////////////////////////////////////////////////////////////////////

actions.defineHttp({
  url : "_admin/dispatch",
  context : "admin",
  prefix : "false",
  callback : function (req, res) {
    if (req.requestType !== actions.POST) {
      actions.resultError(req, res, actions.HTTP_FORBIDDEN);
      return;
    }
    var startupPlan;
    try {
      startupPlan = JSON.parse(req.requestBody);
    }
    catch (error) {
      actions.resultError(req, res, actions.HTTP_BAD,
                          "Posted body was not valid JSON.");
      return;
    }
    var dispatch = require("org/arangodb/cluster/dispatcher").dispatch;
    var r;
    try {
      r = dispatch(startupPlan);
      res.responseCode = actions.HTTP_OK;
      res.contentType = "application/json; charset=utf-8";
      res.body = JSON.stringify(r);
    }
    catch (error2) {
      actions.resultException(req, res, error2, undefined, false);
    }
  }
});
      


////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @\\}"
// End:
