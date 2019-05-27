/* jshint strict: false, unused: false */
/* global AQL_EXECUTE, ArangoServerState, ArangoClusterComm, ArangoClusterInfo, ArangoAgency */

// //////////////////////////////////////////////////////////////////////////////
// / @brief cluster actions
// /
// / @file
// /
// / DISCLAIMER
// /
// / Copyright 2014 ArangoDB GmbH, Cologne, Germany
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
// / Copyright holder is ArangoDB GmbH, Cologne, Germany
// /
// / @author Max Neunhoeffer
// / @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
// / @author Copyright 2013-2014, triAGENS GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

var actions = require('@arangodb/actions');
var cluster = require('@arangodb/cluster');
var wait = require("internal").wait;

// var internal = require('internal');
var _ = require('lodash');

actions.defineHttp({
  url: '_admin/cluster/removeServer',
  allowUseDatabase: true,
  prefix: false,

  callback: function (req, res) {
    if (req.requestType !== actions.POST ||
      !require('@arangodb/cluster').isCoordinator()) {
      actions.resultError(req, res, actions.HTTP_FORBIDDEN, 0,
        'only DELETE requests are allowed and only to coordinators');
      return;
    }

    let serverId;
    try {
      serverId = JSON.parse(req.requestBody);
    } catch (e) {
    }

    if (typeof serverId !== 'string') {
      actions.resultError(req, res, actions.HTTP_BAD,
        'required parameter ServerID was not given');
      return;
    }

    let agency = ArangoAgency.get('', false, true).arango;

    let node = agency.Supervision.Health[serverId];
    if (node === undefined) {
      actions.resultError(req, res, actions.HTTP_NOT_FOUND,
        'unknown server id');
      return;
    }

    if (serverId.substr(0, 4) !== 'CRDN' && serverId.substr(0, 4) !== 'PRMR') {
      actions.resultError(req, res, actions.HTTP_BAD,
        'couldn\'t determine role for serverid ' + serverId);
      return;
    }

    let count = 0;    // Try for 60s if server still in use or not failed
    let msg = "";
    let used = [];
    while (++count <= 60) {
      // need to make sure it is not responsible for anything
      used = [];
      let preconditions = reducePlanServers(function (data, agencyKey, servers) {
        data[agencyKey] = {'old': servers};
        if (servers.indexOf(serverId) !== -1) {
          used.push(agencyKey);
        }
        return data;
      }, {});
      preconditions = reduceCurrentServers(function (data, agencyKey, servers) {
        data[agencyKey] = {'old': servers};
        if (servers.indexOf(serverId) !== -1) {
          used.push(agencyKey);
        }
        return data;
      }, preconditions);

      preconditions['/arango/Supervision/Health/' + serverId + '/Status'] = {'old': 'FAILED'};
      preconditions["/arango/Supervision/DBServers/" + serverId]
        = { "oldEmpty": true };

      if (!checkServerLocked(serverId) && used.length === 0) {
        let operations = {};
        operations['/arango/Plan/Coordinators/' + serverId] = {'op': 'delete'};
        operations['/arango/Plan/DBServers/' + serverId] = {'op': 'delete'};
        operations['/arango/Current/ServersRegistered/' + serverId] = {'op': 'delete'};
        operations['/arango/Current/DBServers/' + serverId] = {'op': 'delete'};
        operations['/arango/Supervision/Health/' + serverId] = {'op': 'delete'};
        operations['/arango/Target/MapUniqueToShortID/' + serverId] = {'op': 'delete'};

        try {
          global.ArangoAgency.write([[operations, preconditions]]);
          actions.resultOk(req, res, actions.HTTP_OK, true);
          return;
        } catch (e) {
          if (e.code === 412) {
            console.log("removeServer: got precondition failed, retrying...");
          } else {
            console.warn("removeServer: could not talk to agency, retrying...");
          }
        }
      } else {
        if (used.length > 0) {
          console.log("removeServer: server", serverId, "still in use in",
                      used.length, "locations.");
        } else {
          console.log("removeServer: server", serverId, "locked in agency.");
        }
      }
      wait(1.0);
    }  // while count
    actions.resultError(req, res, actions.HTTP_PRECONDITION_FAILED,
      'the server not failed, locked or is still in use at the following '
      + 'locations: ' + JSON.stringify(used));
  }
});

// //////////////////////////////////////////////////////////////////////////////
// / @brief was docuBlock JSF_cluster_node_version_GET
// //////////////////////////////////////////////////////////////////////////////

actions.defineHttp({
  url: '_admin/cluster/maintenance',
  allowUseDatabase: true,
  prefix: false,

  callback: function (req, res) {
    if (req.requestType !== actions.PUT) {
      actions.resultError(req, res, actions.HTTP_FORBIDDEN, 0,
        'only GET and PUT requests are allowed');
      return;
    }

    let role = global.ArangoServerState.role();
    if (role !== 'COORDINATOR' && role !== 'SINGLE') {
      actions.resultError(req, res, actions.HTTP_FORBIDDEN, 0,
        'only GET requests are allowed and only to coordinators or singles');
      return;
    }

    var body = JSON.parse(req.requestBody);
    if (body === undefined) {
      res.responseCode = actions.HTTP_BAD;
      res.body = JSON.stringify({
        'error': true,
        'errorMessage': 'empty body'
      });
      return;
    }

    let operations = {};
    if (body === "on") {
      operations['/arango/Supervision/Maintenance'] =
        {"op":"set","new":true,"ttl":3600};
    } else if (body === "off") {
      operations['/arango/Supervision/Maintenance'] = {"op":"delete"};
    } else {
      res.responseCode = actions.HTTP_BAD;
      res.body = JSON.stringify({
        'error': true,
        'errorMessage': 'state string must be "on" or "off"'
      });
      return;
    }
    let preconditions = {};
    try {
      global.ArangoAgency.write([[operations, preconditions]]);
    } catch (e) {
      throw e;
    }

    // Wait 2 min for supervision to go to maintenance mode
    var waitUntil = new Date().getTime() + 120.0*1000;
    while (true) {
      var mode = global.ArangoAgency.read([["/arango/Supervision/State/Mode"]])[0].
          arango.Supervision.State.Mode;

      if (body === "on" && mode === "Maintenance") {
        res.body = JSON.stringify({
          error: false,
          warning: 'Cluster supervision deactivated. It will be reactivated automatically in 60 minutes unless this call is repeated until then.'});
        break;
      } else if (body === "off" && mode === "Normal") {
        res.body = JSON.stringify({
          error: false,
          warning: 'Cluster supervision reactivated.'});
        break;
      }

      wait(0.1);

      if (new Date().getTime() > waitUntil) {
        res.responseCode = actions.HTTP_GATEWAY_TIMEOUT;
        res.body = JSON.stringify({
          'error': true,
          'errorMessage':
          'timed out while waiting for supervision to go into maintenance mode'
        });
        return;
      }

    }

    return ;

  }});
  // //////////////////////////////////////////////////////////////////////////////
// / @brief was docuBlock JSF_cluster_node_version_GET
// //////////////////////////////////////////////////////////////////////////////

actions.defineHttp({
  url: '_admin/clusterNodeVersion',
  prefix: false,

  callback: function (req, res) {
    if (req.requestType !== actions.GET ||
      !require('@arangodb/cluster').isCoordinator()) {
      actions.resultError(req, res, actions.HTTP_FORBIDDEN, 0,
        'only GET requests are allowed and only on coordinator');
      return;
    }

    let serverId;
    try {
      if (req.parameters.ServerID) {
        serverId = req.parameters.ServerID;
      }
    } catch (e) {
    }

    if (typeof serverId !== 'string' || serverId.length === 0) {
      actions.resultError(req, res, actions.HTTP_BAD,
        'required parameter ServerID was not given');
      return;
    }

    var options = { timeout: 10 };
    var op = ArangoClusterComm.asyncRequest('GET', 'server:' + serverId, req.database,
      '/_api/version', '', {}, options);
    var r = ArangoClusterComm.wait(op);
    res.contentType = 'application/json; charset=utf-8';
    if (r.status === 'RECEIVED') {
      res.responseCode = actions.HTTP_OK;
      res.body = r.body;
    } else if (r.status === 'TIMEOUT') {
      res.responseCode = actions.HTTP_BAD;
      res.body = JSON.stringify({
        'error': true,
        'errorMessage': 'operation timed out'
      });
    } else {
      res.responseCode = actions.HTTP_BAD;
      var bodyobj;
      try {
        bodyobj = JSON.parse(r.body);
      } catch (err) {}
      res.body = JSON.stringify({
        'error': true,
        'errorMessage': 'error from Server, possibly Server unknown',
        'body': bodyobj
      });
    }
  }
});

// //////////////////////////////////////////////////////////////////////////////
// / @brief was docuBlock JSF_cluster_node_stats_GET
// //////////////////////////////////////////////////////////////////////////////

actions.defineHttp({
  url: '_admin/clusterNodeStats',
  prefix: false,

  callback: function (req, res) {
    if (req.requestType !== actions.GET ||
      !require('@arangodb/cluster').isCoordinator()) {
      actions.resultError(req, res, actions.HTTP_FORBIDDEN, 0,
        'only GET requests are allowed and only on coordinator');
      return;
    }

    let serverId;
    try {
      if (req.parameters.ServerID) {
        serverId = req.parameters.ServerID;
      }
    } catch (e) {
    }

    if (typeof serverId !== 'string') {
      actions.resultError(req, res, actions.HTTP_BAD,
        'required parameter ServerID was not given');
      return;
    }

    var options = { timeout: 10 };
    var op = ArangoClusterComm.asyncRequest('GET', 'server:' + serverId, req.database,
      '/_admin/statistics', '', {}, options);
    var r = ArangoClusterComm.wait(op);
    res.contentType = 'application/json; charset=utf-8';
    if (r.status === 'RECEIVED') {
      res.responseCode = actions.HTTP_OK;
      res.body = r.body;
    } else if (r.status === 'TIMEOUT') {
      res.responseCode = actions.HTTP_BAD;
      res.body = JSON.stringify({
        'error': true,
        'errorMessage': 'operation timed out'
      });
    } else {
      res.responseCode = actions.HTTP_BAD;
      var bodyobj;
      try {
        bodyobj = JSON.parse(r.body);
      } catch (err) {}
      res.body = JSON.stringify({
        'error': true,
        'errorMessage': 'error from Server, possibly Server unknown',
        'body': bodyobj
      });
    }
  }
});

// //////////////////////////////////////////////////////////////////////////////
// / @brief was docuBlock JSF_cluster_node_engine_GET
// //////////////////////////////////////////////////////////////////////////////

actions.defineHttp({
  url: '_admin/clusterNodeEngine',
  prefix: false,

  callback: function (req, res) {
    if (req.requestType !== actions.GET ||
      !require('@arangodb/cluster').isCoordinator()) {
      actions.resultError(req, res, actions.HTTP_FORBIDDEN, 0,
        'only GET requests are allowed and only on coordinator');
      return;
    }

    let serverId;
    try {
      if (req.parameters.ServerID) {
        serverId = req.parameters.ServerID;
      }
    } catch (e) {
    }

    if (typeof serverId !== 'string') {
      actions.resultError(req, res, actions.HTTP_BAD,
        'required parameter ServerID was not given');
      return;
    }

    var options = { timeout: 10 };
    var op = ArangoClusterComm.asyncRequest('GET', 'server:' + serverId, req.database,
      '/_api/engine', '', {}, options);
    var r = ArangoClusterComm.wait(op);
    res.contentType = 'application/json; charset=utf-8';
    if (r.status === 'RECEIVED') {
      res.responseCode = actions.HTTP_OK;
      res.body = r.body;
    } else if (r.status === 'TIMEOUT') {
      res.responseCode = actions.HTTP_BAD;
      res.body = JSON.stringify({
        'error': true,
        'errorMessage': 'operation timed out'
      });
    } else {
      res.responseCode = actions.HTTP_BAD;
      var bodyobj;
      try {
        bodyobj = JSON.parse(r.body);
      } catch (err) {}
      res.body = JSON.stringify({
        'error': true,
        'errorMessage': 'error from Server, possibly Server unknown',
        'body': bodyobj
      });
    }
  }
});

// //////////////////////////////////////////////////////////////////////////////
// / @brief was docuBlock JSF_cluster_statistics_GET
// //////////////////////////////////////////////////////////////////////////////

actions.defineHttp({
  url: '_admin/clusterStatistics',
  prefix: false,

  callback: function (req, res) {
    if (req.requestType !== actions.GET) {
      actions.resultError(req, res, actions.HTTP_FORBIDDEN, 0,
        'only GET requests are allowed');
      return;
    }
    if (!require('@arangodb/cluster').isCoordinator()) {
      actions.resultError(req, res, actions.HTTP_FORBIDDEN, 0,
        'only allowed on coordinator');
      return;
    }
    if (!req.parameters.hasOwnProperty('DBserver')) {
      actions.resultError(req, res, actions.HTTP_BAD,
        'required parameter DBserver was not given');
      return;
    }
    var DBserver = req.parameters.DBserver;
    var options = { timeout: 10 };
    var op = ArangoClusterComm.asyncRequest('GET', 'server:' + DBserver, req.database,
      '/_admin/statistics', '', {}, options);
    var r = ArangoClusterComm.wait(op);
    res.contentType = 'application/json; charset=utf-8';
    if (r.status === 'RECEIVED') {
      res.responseCode = actions.HTTP_OK;
      res.body = r.body;
    } else if (r.status === 'TIMEOUT') {
      res.responseCode = actions.HTTP_BAD;
      res.body = JSON.stringify({
        'error': true,
        'errorMessage': 'operation timed out'
      });
    } else {
      res.responseCode = actions.HTTP_BAD;
      var bodyobj;
      try {
        bodyobj = JSON.parse(r.body);
      } catch (err) {}
      res.body = JSON.stringify({
        'error': true,
        'errorMessage': 'error from DBserver, possibly DBserver unknown',
        'body': bodyobj
      });
    }
  }
});

// //////////////////////////////////////////////////////////////////////////////
// / @brief was docuBlock JSF_cluster_statistics_GET
// //////////////////////////////////////////////////////////////////////////////

actions.defineHttp({
  url: '_admin/cluster/health',
  allowUseDatabase: true,
  prefix: false,

  callback: function (req, res) {
    let role = global.ArangoServerState.role();
    if (req.requestType !== actions.GET ||
        (role !== 'COORDINATOR' && role !== 'SINGLE')) {
      actions.resultError(req, res, actions.HTTP_FORBIDDEN, 0,
        'only GET requests are allowed and only to coordinators or singles');
      return;
    }

    var timeout = 60.0;
    try {
      if (req.parameters.hasOwnProperty('timeout')) {
        timeout = Number(req.parameters.timeout);
      }
    } catch (e) {}

    var clusterId;
    try {
      clusterId = ArangoAgency.get('Cluster', false, true).arango.Cluster;
    } catch (e1) {
      actions.resultError(req, res, actions.HTTP_NOT_FOUND, 0,
        'Failed to retrieve clusterId node from agency!');
      return;
    }

    let agency = ArangoAgency.agency();

    var Health = {};
    var startTime = new Date();
    while (true) {
      try {
        Health = ArangoAgency.get('Supervision/Health', false, true).arango.Supervision.Health;
      } catch (e1) {
        actions.resultError(req, res, actions.HTTP_NOT_FOUND, 0,
          'Failed to retrieve supervision node from agency!');
        return;
      }
      if (Object.keys(Health).length !== 0) {
        break;
      }
      if (new Date() - startTime > timeout * 1000) {   // milliseconds
        actions.resultError(req, res, actions.HTTP_NOT_FOUND, 0,
          'Failed to get health status from agency in ' + timeout + ' seconds.');
        return;
      }
      console.warn("/_api/cluster/health not ready yet, retrying...");
      require("internal").wait(0.5);
    }

    Health = Object.entries(Health).reduce((Health, [serverId, struct]) => {
      let canBeDeleted = false;
      if (serverId.startsWith('PRMR')) {
        Health[serverId].Role = 'DBServer';
      } else if (serverId.startsWith('CRDN')) {
        Health[serverId].Role = 'Coordinator';
      }
      if (struct.Role === 'Coordinator') {
        canBeDeleted = struct.Status === 'FAILED';
      } else if (struct.Role === 'DBServer') {
        if (struct.Status === 'FAILED') {
          let numUsed = reducePlanServers(function (numUsed, agencyKey, servers) {
            if (servers.indexOf(serverId) !== -1) {
              numUsed++;
            }
            return numUsed;
          }, 0);
          if (numUsed === 0) {
            numUsed = reduceCurrentServers(function (numUsed, agencyKey, servers) {
              if (servers.indexOf(serverId) !== -1) {
                numUsed++;
              }
              return numUsed;
            }, 0);
          }
          canBeDeleted = numUsed === 0;
        }
      }
      // the structure is all uppercase for whatever reason so make it uppercase as well
      Health[serverId].CanBeDeleted = canBeDeleted;
      return Health;
    }, Health);

    Object.entries(agency.configuration.pool).forEach(([key, value]) => {

      if (Health.hasOwnProperty(key)) {
        Health[key].Endpoint = value;
        Health[key].Role = 'Agent';
        Health[key].CanBeDeleted = false;
      } else {
        Health[key] = {Endpoint: value, Role: 'Agent', CanBeDeleted: false};
      }

      var options = { timeout: 5 };
      var op = ArangoClusterComm.asyncRequest(
        'GET', value, '_system', '/_api/agency/config', '', {}, options);
      var r = ArangoClusterComm.wait(op);

      if (r.status === 'RECEIVED') {
        var record = JSON.parse(r.body);
        Health[key].Version = record.version;
        Health[key].Engine = record.engine;
        Health[key].Leader = record.leaderId;
        if (record.hasOwnProperty("lastAcked")) {
          Health[key].Leading = true;
          Object.entries(record.lastAcked).forEach(([k,v]) => {
            if (Health.hasOwnProperty(k)) {
              Health[k].LastAckedTime = v.lastAckedTime;
            } else {
              Health[k] = {LastAckedTime: v.lastAckedTime};
            }
          });
        }
        Health[key].Status = "GOOD";
      } else {
        Health[key].Status = "BAD";
        if (r.status === 'TIMEOUT') {
          Health[key].Error = "TIMEOUT";
        } else {
          try {
            Health[key].Error = JSON.parse(r.body);
          } catch (err) {
            Health[key].Error = "UNKNOWN";
          }
        }
      }

    });

    actions.resultOk(req, res, actions.HTTP_OK, {Health, ClusterId: clusterId});
  }
});

function reducePlanServers (reducer, data) {
  var databases = ArangoAgency.get('Plan/Collections');
  databases = databases.arango.Plan.Collections;

  return Object.keys(databases).reduce(function (data, databaseName) {
    var collections = databases[databaseName];

    return Object.keys(collections).reduce(function (data, collectionKey) {
      var collection = collections[collectionKey];

      return Object.keys(collection.shards).reduce(function (data, shardKey) {
        var servers = collection.shards[shardKey];

        let key = '/arango/Plan/Collections/' + databaseName + '/' + collectionKey + '/shards/' + shardKey;
        return reducer(data, key, servers);
      }, data);
    }, data);
  }, data);
}

function reduceCurrentServers (reducer, data) {
  var databases = ArangoAgency.get('Current/Collections');
  databases = databases.arango.Current.Collections;

  return Object.keys(databases).reduce(function (data, databaseName) {
    var collections = databases[databaseName];

    return Object.keys(collections).reduce(function (data, collectionKey) {
      var collection = collections[collectionKey];

      return Object.keys(collection).reduce(function (data, shardKey) {
        var servers = collection[shardKey].servers;

        let key = '/arango/Current/Collections/' + databaseName + '/' + collectionKey + '/' + shardKey + '/servers';
        return reducer(data, key, servers);
      }, data);
    }, data);
  }, data);
}

function checkServerLocked (server) {
  var locks = ArangoAgency.get('Supervision/DBServers');
  try {
    if (locks.arango.Supervision.DBServers.hasOwnProperty(server)) {
      return true;
    }
  } catch (e) {
  }
  return false;
}

// //////////////////////////////////////////////////////////////////////////////
// / @start Docu Block JSF_getNumberOfServers
// / (intentionally not in manual)
// / @brief gets the number of coordinators desired, which are stored in
// / /Target/NumberOfDBServers in the agency.
// /
// / @ RESTHEADER{GET /_admin/cluster/numberOfServers, Get desired number of coordinators and DBServers.}
// /
// / @ RESTQUERYPARAMETERS
// /
// / @ RESTDESCRIPTION gets the number of coordinators and DBServers desired,
// / which are stored in `/Target` in the agency. A body of the form
// /     { "numberOfCoordinators": 12, "numberOfDBServers": 12 }
// / is returned. Note that both value can be `null` indicating that the
// / cluster cannot be scaled.
// /
// / @ RESTRETURNCODES
// /
// / @ RESTRETURNCODE{200} is returned when everything went well.
// /
// / @ RESTRETURNCODE{403} server is not a coordinator or method was not GET
// / or PUT.
// /
// / @ RESTRETURNCODE{503} the get operation did not work.
// /
// / @end Docu Block
// //////////////////////////////////////////////////////////////////////////////

// //////////////////////////////////////////////////////////////////////////////
// / @start Docu Block JSF_putNumberOfServers
// / (intentionally not in manual)
// / @brief sets the number of coordinators and or DBServers desired, which
// / are stored in /Target in the agency.
// /
// / @ RESTHEADER{PUT /_admin/cluster/numberOfServers, Set desired number of coordinators and or DBServers.}
// /
// / @ RESTQUERYPARAMETERS
// /
// / @ RESTDESCRIPTION sets the number of coordinators and DBServers desired,
// / which are stored in `/Target` in the agency. A body of the form
// /     { "numberOfCoordinators": 12, "numberOfDBServers": 12,
// /       "cleanedServers": [] }
// / must be supplied. Either one of the values can be left out and will
// / then not be changed. Either numeric value can be `null` to indicate
// / that the cluster cannot be scaled.
// /
// / @ RESTRETURNCODES
// /
// / @ RESTRETURNCODE{200} is returned when everything went well.
// /
// / @ RESTRETURNCODE{400} body is not valid JSON.
// /
// / @ RESTRETURNCODE{403} server is not a coordinator or method was not GET
// / or PUT.
// /
// / @ RESTRETURNCODE{503} the agency operation did not work.
// /
// / @end Docu Block
// //////////////////////////////////////////////////////////////////////////////

actions.defineHttp({
  url: '_admin/cluster/numberOfServers',
  allowUseDatabase: false,
  prefix: false,

  callback: function (req, res) {
    if (!require('@arangodb/cluster').isCoordinator()) {
      actions.resultError(req, res, actions.HTTP_FORBIDDEN, 0,
        'only coordinators can serve this request');
      return;
    }
    if (req.requestType !== actions.GET &&
      req.requestType !== actions.PUT) {
      actions.resultError(req, res, actions.HTTP_FORBIDDEN, 0,
        'only GET and PUT methods are allowed');
      return;
    }

    // Now get to work:
    if (req.requestType === actions.GET) {
      var nrCoordinators;
      var nrDBServers;
      var cleanedServers;
      try {
        nrCoordinators = ArangoAgency.get('Target/NumberOfCoordinators');
        nrCoordinators = nrCoordinators.arango.Target.NumberOfCoordinators;
        nrDBServers = ArangoAgency.get('Target/NumberOfDBServers');
        nrDBServers = nrDBServers.arango.Target.NumberOfDBServers;
        cleanedServers = ArangoAgency.get('Target/CleanedServers');
        cleanedServers = cleanedServers.arango.Target.CleanedServers;
      } catch (e1) {
        actions.resultError(req, res, actions.HTTP_SERVICE_UNAVAILABLE,
          'Cannot read from agency.');
        return;
      }
      actions.resultOk(req, res, actions.HTTP_OK, {
        numberOfCoordinators: nrCoordinators,
        numberOfDBServers: nrDBServers,
        cleanedServers
      });
    } else { // PUT
      var body = actions.getJsonBody(req, res);
      if (body === undefined) {
        return;
      }
      if (typeof body !== 'object') {
        actions.resultError(req, res, actions.HTTP_BAD,
          'body must be an object');
        return;
      }
      var ok = true;
      try {
        if (body.hasOwnProperty('numberOfCoordinators') &&
          (typeof body.numberOfCoordinators === 'number' ||
          body.numberOfCoordinators === null)) {
          ArangoAgency.set('Target/NumberOfCoordinators',
            body.numberOfCoordinators);
        }
      } catch (e1) {
        ok = false;
      }
      try {
        if (body.hasOwnProperty('numberOfDBServers') &&
          (typeof body.numberOfDBServers === 'number' ||
          body.numberOfDBServers === null)) {
          ArangoAgency.set('Target/NumberOfDBServers',
            body.numberOfDBServers);
        }
      } catch (e2) {
        ok = false;
      }
      try {
        if (body.hasOwnProperty('cleanedServers') &&
          typeof body.cleanedServers === 'object' &&
          Array.isArray(body.cleanedServers)) {
          ArangoAgency.set('Target/CleanedServers',
            body.cleanedServers);
        }
      } catch (e3) {
        ok = false;
      }
      if (!ok) {
        actions.resultError(req, res, actions.HTTP_SERVICE_UNAVAILABLE,
          'Cannot write to agency.');
        return;
      }
      actions.resultOk(req, res, actions.HTTP_OK, true);
    }
  }
});

// //////////////////////////////////////////////////////////////////////////////
// / @start Docu Block JSF_postCleanOutServer
// / (intentionally not in manual)
// / @brief triggers activities to clean out a DBServer
// /
// / @ RESTHEADER{POST /_admin/cluster/cleanOutServer, Trigger activities to clean out a DBServers.}
// /
// / @ RESTQUERYPARAMETERS
// /
// / @ RESTDESCRIPTION Triggers activities to clean out a DBServer.
// / The body must be a JSON object with attribute "server" that is a string
// / with the ID of the server to be cleaned out.
// /
// / @ RESTRETURNCODES
// /
// / @ RESTRETURNCODE{202} is returned when everything went well and the
// / job is scheduled.
// /
// / @ RESTRETURNCODE{400} body is not valid JSON.
// /
// / @ RESTRETURNCODE{403} server is not a coordinator or method was not POST.
// /
// / @ RESTRETURNCODE{503} the agency operation did not work.
// /
// / @end Docu Block
// //////////////////////////////////////////////////////////////////////////////

actions.defineHttp({
  url: '_admin/cluster/cleanOutServer',
  allowUseDatabase: false,
  prefix: false,

  callback: function (req, res) {
    if (!require('@arangodb/cluster').isCoordinator()) {
      actions.resultError(req, res, actions.HTTP_FORBIDDEN, 0,
        'only coordinators can serve this request');
      return;
    }
    if (req.requestType !== actions.POST) {
      actions.resultError(req, res, actions.HTTP_FORBIDDEN, 0,
        'only the POST method is allowed');
      return;
    }

    // Now get to work:
    var body = actions.getJsonBody(req, res);
    if (body === undefined) {
      return;
    }
    if (typeof body !== 'object' ||
      !body.hasOwnProperty('server') ||
      typeof body.server !== 'string') {
      actions.resultError(req, res, actions.HTTP_BAD,
        "body must be an object with a string attribute 'server'");
      return;
    }

    // First translate the server name from short name to long name:
    var server = body.server;
    var servers = global.ArangoClusterInfo.getDBServers();
    for (let i = 0; i < servers.length; i++) {
      if (servers[i].serverId !== server) {
        if (servers[i].serverName === server) {
          server = servers[i].serverId;
          break;
        }
      }
    }

    var ok = true;
    var id;
    try {
      id = ArangoClusterInfo.uniqid();
      var todo = {
        'type': 'cleanOutServer',
        'server': server,
        'jobId': id,
        'timeCreated': (new Date()).toISOString(),
        'creator': ArangoServerState.id()
      };
      ArangoAgency.set('Target/ToDo/' + id, todo);
    } catch (e1) {
      ok = false;
    }
    if (!ok) {
      actions.resultError(req, res, actions.HTTP_SERVICE_UNAVAILABLE,
        {error: true, errorMsg: 'Cannot write to agency.'});
      return;
    }
    actions.resultOk(req, res, actions.HTTP_ACCEPTED, {error: false, id: id});
  }
});

// //////////////////////////////////////////////////////////////////////////////
// / @start Docu Block JSF_getqueryAgencyJob
// / (intentionally not in manual)
// / @brief asks about progress on an agency job by id
// /
// / @ RESTHEADER{GET /_admin/cluster/queryAgencyJob, Ask about an agency job by its id.}
// /
// / @ RESTQUERYPARAMETERS `id` must be a string with the ID of the agency
// / job being queried.
// /
// / @ RESTDESCRIPTION Returns information (if known) about the job with ID
// / `id`. This can either be a cleanOurServer or a moveShard job at this
// / stage.
// /
// / @ RESTRETURNCODES
// /
// / @ RESTRETURNCODE{200} is returned when everything went well and the
// / information about the job is returned. It might be that the job is
// / not found.
// /
// / @ RESTRETURNCODE{400} id parameter is not given or not a string.
// /
// / @ RESTRETURNCODE{403} server is not a coordinator or method was not GET.
// /
// / @ RESTRETURNCODE{503} the agency operation did not work.
// /
// / @end Docu Block
// //////////////////////////////////////////////////////////////////////////////

actions.defineHttp({
  url: '_admin/cluster/queryAgencyJob',
  allowUseDatabase: false,
  prefix: false,

  callback: function (req, res) {
    if (!require('@arangodb/cluster').isCoordinator()) {
      actions.resultError(req, res, actions.HTTP_FORBIDDEN, 0,
        'only coordinators can serve this request');
      return;
    }
    if (req.requestType !== actions.GET) {
      actions.resultError(req, res, actions.HTTP_FORBIDDEN, 0,
        'only the GET method is allowed');
      return;
    }

    // Now get to work:
    let id;
    try {
      if (req.parameters.id) {
        id = req.parameters.id;
      }
    } catch(e) {
    }

    if (typeof id !== 'string' || id.length === 0) {
      actions.resultError(req, res, actions.HTTP_BAD,
        'required parameter id was not given');
      return;
    }

    var ok = true;
    var job;
    try {
      job = require('@arangodb/cluster').queryAgencyJob(id);
    } catch (e1) {
      ok = false;
    }
    if (!ok) {
      actions.resultError(req, res, actions.HTTP_SERVICE_UNAVAILABLE,
        {error: true, errorMsg: 'Cannot read from agency.'});
      return;
    }
    actions.resultOk(req, res, actions.HTTP_OK, job);
  }
});

// //////////////////////////////////////////////////////////////////////////////
// / @start Docu Block JSF_postMoveShard
// / (intentionally not in manual)
// / @brief triggers activities to move a shard
// /
// / @ RESTHEADER{POST /_admin/cluster/moveShard, Trigger activities to move a shard.}
// /
// / @ RESTQUERYPARAMETERS
// /
// / @ RESTDESCRIPTION Triggers activities to move a shard.
// / The body must be a JSON document with the following attributes:
// /   - `"database"`: a string with the name of the database
// /   - `"collection"`: a string with the name of the collection
// /   - `"shard"`: a string with the name of the shard to move
// /   - `"fromServer"`: a string with the ID of a server that is currently
// /     the leader or a follower for this shard
// /   - `"toServer"`: a string with the ID of a server that is currently
// /     not the leader and not a follower for this shard
// /
// / @ RESTRETURNCODES
// /
// / @ RESTRETURNCODE{202} is returned when everything went well and the
// / job is scheduled.
// /
// / @ RESTRETURNCODE{400} body is not valid JSON.
// /
// / @ RESTRETURNCODE{403} server is not a coordinator or method was not POST.
// /
// / @ RESTRETURNCODE{503} the agency operation did not work.
// /
// / @end Docu Block
// //////////////////////////////////////////////////////////////////////////////

actions.defineHttp({
  url: '_admin/cluster/moveShard',
  allowUseDatabase: false,
  prefix: false,

  callback: function (req, res) {
    if (!require('@arangodb/cluster').isCoordinator()) {
      actions.resultError(req, res, actions.HTTP_FORBIDDEN, 0,
        'only coordinators can serve this request');
      return;
    }
    if (req.requestType !== actions.POST) {
      actions.resultError(req, res, actions.HTTP_FORBIDDEN, 0,
        'only the POST method is allowed');
      return;
    }

    // Now get to work:
    var body = actions.getJsonBody(req, res);
    if (body === undefined) {
      return;
    }
    if (typeof body !== 'object' ||
      !body.hasOwnProperty('database') ||
      typeof body.database !== 'string' ||
      !body.hasOwnProperty('collection') ||
      typeof body.collection !== 'string' ||
      !body.hasOwnProperty('shard') ||
      typeof body.shard !== 'string' ||
      !body.hasOwnProperty('fromServer') ||
      typeof body.fromServer !== 'string' ||
      !body.hasOwnProperty('toServer') ||
      typeof body.toServer !== 'string') {
      actions.resultError(req, res, actions.HTTP_BAD,
        "body must be an object with string attributes 'database', 'collection', 'shard', 'fromServer' and 'toServer'");
      return;
    }
    body.shards = [body.shard];
    body.collections = [body.collection];
    var r = require('@arangodb/cluster').moveShard(body);
    if (r.error) {
      actions.resultError(req, res, actions.HTTP_SERVICE_UNAVAILABLE, r);
      return;
    }
    actions.resultOk(req, res, actions.HTTP_ACCEPTED, r);
  }
});

// //////////////////////////////////////////////////////////////////////////////
// / @start Docu Block JSF_collectionShardDistribution
// / (intentionally not in manual)
// / @brief returns information about all collections and their shard
// / distribution
// /
// / @ RESTHEADER{PUT /_admin/cluster/collectionShardDistribution,
// / Get shard distribution for a specific collections.}
// /
// / @ RESTDESCRIPTION Returns an object with an attribute for a specific collection.
// / The attribute name is the collection name. Each value is an object
// / of the following form:
// /
// /     { "collection1": { "Plan": { "s100001": ["DBServer001", "DBServer002"],
// /                                  "s100002": ["DBServer003", "DBServer004"] },
// /                        "Current": { "s100001": ["DBServer001", "DBServer002"],
// /                                     "s100002": ["DBServer003"] } },
// /       "collection2": ...
// /     }
//
// / The body must be a JSON document with the following attributes:
// /   - `"collection"`: a string with the name of the collection
// /
// / @ RESTRETURNCODES
// /
// / @ RESTRETURNCODE{200} is returned when everything went well and the
// / job is scheduled.
// /
// / @ RESTRETURNCODE{403} server is not a coordinator or method was not GET.
// /
// / @end Docu Block
// //////////////////////////////////////////////////////////////////////////////

actions.defineHttp({
  url: '_admin/cluster/collectionShardDistribution',
  allowUseDatabase: false,
  prefix: false,

  callback: function (req, res) {
    if (!require('@arangodb/cluster').isCoordinator()) {
      actions.resultError(req, res, actions.HTTP_FORBIDDEN, 0,
        'only coordinators can serve this request');
      return;
    }
    if (req.requestType !== actions.PUT) {
      actions.resultError(req, res, actions.HTTP_FORBIDDEN, 0,
        'only the PUT method is allowed');
      return;
    }

    var body = actions.getJsonBody(req, res);
    if (typeof body !== 'object') {
      actions.resultError(req, res, actions.HTTP_BAD,
        'body must be an object.');
      return;
    }
    if (!body.collection) {
      actions.resultError(req, res, actions.HTTP_BAD,
        'body missing. expected collection name.');
      return;
    }
    if (typeof body.collection !== 'string') {
      actions.resultError(req, res, actions.HTTP_BAD,
        'collection name must be a string.');
      return;
    }

    var result = require('@arangodb/cluster').collectionShardDistribution(body.collection);
    actions.resultOk(req, res, actions.HTTP_OK, result);
  }
});

// //////////////////////////////////////////////////////////////////////////////
// / @start Docu Block JSF_getShardDistribution
// / (intentionally not in manual)
// / @brief returns information about all collections and their shard
// / distribution
// /
// / @ RESTHEADER{GET /_admin/cluster/shardDistribution, Get shard distribution for all collections.}
// /
// / @ RESTDESCRIPTION Returns an object with an attribute for each collection.
// / The attribute name is the collection name. Each value is an object
// / of the following form:
// /
// /     { "collection1": { "Plan": { "s100001": ["DBServer001", "DBServer002"],
// /                                  "s100002": ["DBServer003", "DBServer004"] },
// /                        "Current": { "s100001": ["DBServer001", "DBServer002"],
// /                                     "s100002": ["DBServer003"] } },
// /       "collection2": ...
// /     }
// /
// / @ RESTRETURNCODES
// /
// / @ RESTRETURNCODE{200} is returned when everything went well and the
// / job is scheduled.
// /
// / @ RESTRETURNCODE{403} server is not a coordinator or method was not GET.
// /
// / @end Docu Block
// //////////////////////////////////////////////////////////////////////////////

actions.defineHttp({
  url: '_admin/cluster/shardDistribution',
  allowUseDatabase: false,
  prefix: false,

  callback: function (req, res) {
    if (!require('@arangodb/cluster').isCoordinator()) {
      actions.resultError(req, res, actions.HTTP_FORBIDDEN, 0,
        'only coordinators can serve this request');
      return;
    }
    if (req.requestType !== actions.GET) {
      actions.resultError(req, res, actions.HTTP_FORBIDDEN, 0,
        'only the GET method is allowed');
      return;
    }

    var result = require('@arangodb/cluster').shardDistribution();
    actions.resultOk(req, res, actions.HTTP_OK, result);
  }
});

// //////////////////////////////////////////////////////////////////////////////
// / @start Docu Block JSF_postRebalanceShards
// / (intentionally not in manual)
// / @brief triggers activities to rebalance shards
// /
// / @ RESTHEADER{POST /_admin/cluster/rebalanceShards, Trigger activities to rebalance shards.}
// /
// / @ RESTDESCRIPTION Triggers activities to rebalance shards.
// / The body must be an empty JSON object.
// /
// / @ RESTRETURNCODES
// /
// / @ RESTRETURNCODE{202} is returned when everything went well.
// /
// / @ RESTRETURNCODE{400} body is not valid JSON.
// /
// / @ RESTRETURNCODE{403} server is not a coordinator or method was not POST.
// /
// / @ RESTRETURNCODE{503} the agency operation did not work.
// /
// / @end Docu Block
// //////////////////////////////////////////////////////////////////////////////

actions.defineHttp({
  url: '_admin/cluster/rebalanceShards',
  allowUseDatabase: true,
  prefix: false,

  callback: function (req, res) {
    if (!require('@arangodb/cluster').isCoordinator()) {
      actions.resultError(req, res, actions.HTTP_FORBIDDEN, 0,
        'only coordinators can serve this request');
      return;
    }
    if (req.requestType !== actions.POST) {
      actions.resultError(req, res, actions.HTTP_FORBIDDEN, 0,
        'only the POST method is allowed');
      return;
    }

    // Now get to work:
    var body = actions.getJsonBody(req, res);
    if (body === undefined) {
      return;
    }
    if (typeof body !== 'object') {
      actions.resultError(req, res, actions.HTTP_BAD,
        'body must be an object.');
      return;
    }
    var ok = require('@arangodb/cluster').rebalanceShards();
    if (!ok) {
      actions.resultError(req, res, actions.HTTP_SERVICE_UNAVAILABLE,
        'Cannot write to agency.');
      return;
    }
    actions.resultOk(req, res, actions.HTTP_ACCEPTED, true);
  }
});

// //////////////////////////////////////////////////////////////////////////////
// / @start Docu Block JSF_getSupervisionState
// / (intentionally not in manual)
// / @brief returns information about the state of Supervision jobs
// /
// / @ RESTHEADER{GET /_admin/cluster/supervisionState, Get information
// / about the state of Supervision jobs.
// /
// / @ RESTDESCRIPTION Returns an object with attributes `ToDo`, `Pending`,
// / `Failed` and `Finished` mirroring the state of Supervision jobs in
// / the agency.
// /
// / @ RESTRETURNCODES
// /
// / @ RESTRETURNCODE{200} is returned when everything went well and the
// / job is scheduled.
// /
// / @ RESTRETURNCODE{403} server is not a coordinator or method was not GET.
// /
// / @end Docu Block
// //////////////////////////////////////////////////////////////////////////////

actions.defineHttp({
  url: '_admin/cluster/supervisionState',
  allowUseDatabase: false,
  prefix: false,

  callback: function (req, res) {
    if (!require('@arangodb/cluster').isCoordinator()) {
      actions.resultError(req, res, actions.HTTP_FORBIDDEN, 0,
        'only coordinators can serve this request');
      return;
    }
    if (req.requestType !== actions.GET) {
      actions.resultError(req, res, actions.HTTP_FORBIDDEN, 0,
        'only the GET method is allowed');
      return;
    }

    var result = require('@arangodb/cluster').supervisionState();
    if (result.error) {
      actions.resultError(req, res, actions.HTTP_BAD, result);
      return;
    }
    actions.resultOk(req, res, actions.HTTP_OK, result);
  }
});
