/* jshint strict: false, unused: false */
/* global AQL_EXECUTE, SYS_CLUSTER_TEST
  ArangoServerState, ArangoClusterComm, ArangoClusterInfo, ArangoAgency */

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
// / @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
// / @author Copyright 2013-2014, triAGENS GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

var actions = require('@arangodb/actions');
var cluster = require('@arangodb/cluster');
//var internal = require('internal');
var _ = require('lodash');

// //////////////////////////////////////////////////////////////////////////////
// / @brief was docuBlock JSF_cluster_test_GET
// //////////////////////////////////////////////////////////////////////////////

// //////////////////////////////////////////////////////////////////////////////
// / @brief was docuBlock JSF_cluster_test_POST
// //////////////////////////////////////////////////////////////////////////////

// //////////////////////////////////////////////////////////////////////////////
// / @brief was docuBlock JSF_cluster_test_PUT
// //////////////////////////////////////////////////////////////////////////////

// //////////////////////////////////////////////////////////////////////////////
// / @brief was docuBlock JSF_cluster_test_DELETE
// //////////////////////////////////////////////////////////////////////////////

// //////////////////////////////////////////////////////////////////////////////
// / @brief was docuBlock JSF_cluster_test_PATCH
// //////////////////////////////////////////////////////////////////////////////

// //////////////////////////////////////////////////////////////////////////////
// / @brief was docuBlock JSF_cluster_test_HEAD
// //////////////////////////////////////////////////////////////////////////////

actions.defineHttp({
  url: '_admin/cluster-test',
  prefix: true,

  callback: function (req, res) {
    var path;
    if (req.hasOwnProperty('suffix') && req.suffix.length !== 0) {
      path = '/' + req.suffix.join('/');
    } else {
      path = '/_admin/version';
    }
    var params = '';
    var shard = '';
    var p;

    for (p in req.parameters) {
      if (req.parameters.hasOwnProperty(p)) {
        if (params === '') {
          params = '?';
        } else {
          params += '&';
        }
        params += p + '=' + encodeURIComponent(String(req.parameters[p]));
      }
    }
    if (params !== '') {
      path += params;
    }
    var headers = {};
    var transID = '';
    var timeout = 24 * 3600.0;
    var asyncMode = true;

    for (p in req.headers) {
      if (req.headers.hasOwnProperty(p)) {
        if (p === 'x-client-transaction-id') {
          transID = req.headers[p];
        } else if (p === 'x-timeout') {
          timeout = parseFloat(req.headers[p]);
          if (isNaN(timeout)) {
            timeout = 24 * 3600.0;
          }
        } else if (p === 'x-synchronous-mode') {
          asyncMode = false;
        } else if (p === 'x-shard-id') {
          shard = req.headers[p];
        } else {
          headers[p] = req.headers[p];
        }
      }
    }

    var body;
    if (req.requestBody === undefined || typeof req.requestBody !== 'string') {
      body = '';
    } else {
      body = req.requestBody;
    }

    var r;
    if (typeof SYS_CLUSTER_TEST === 'undefined') {
      actions.resultError(req, res, actions.HTTP_NOT_FOUND,
        'Not compiled for cluster operation');
    } else {
      try {
        r = SYS_CLUSTER_TEST(req, res, shard, path, transID,
          headers, body, timeout, asyncMode);
        if (r.timeout || typeof r.errorMessage === 'string') {
          res.responseCode = actions.HTTP_OK;
          res.contentType = 'application/json; charset=utf-8';
          var s = JSON.stringify(r);
          res.body = s;
        } else {
          res.responseCode = actions.HTTP_OK;
          res.contentType = r.headers.contentType;
          res.headers = r.headers;
          res.body = r.body;
        }
      } catch (err) {
        actions.resultError(req, res, actions.HTTP_FORBIDDEN, String(err));
      }
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
    var op = ArangoClusterComm.asyncRequest('GET', 'server:' + DBserver, '_system',
      '/_admin/statistics', '', {}, options);
    var r = ArangoClusterComm.wait(op);
    res.contentType = 'application/json; charset=utf-8';
    if (r.status === 'RECEIVED') {
      res.responseCode = actions.HTTP_OK;
      res.body = r.body;
    } else if (r.status === 'TIMEOUT') {
      res.responseCode = actions.HTTP_BAD;
      res.body = JSON.stringify({'error': true,
      'errorMessage': 'operation timed out'});
    } else {
      res.responseCode = actions.HTTP_BAD;
      var bodyobj;
      try {
        bodyobj = JSON.parse(r.body);
      } catch (err) {}
      res.body = JSON.stringify({'error': true,
        'errorMessage': 'error from DBserver, possibly DBserver unknown',
      'body': bodyobj});
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
    if (req.requestType !== actions.GET ||
      !require('@arangodb/cluster').isCoordinator()) {
      actions.resultError(req, res, actions.HTTP_FORBIDDEN, 0,
        'only GET requests are allowed and only to coordinators');
      return;
    }

    var timeout = 60.0;

    try {
      if (req.parameters.hasOwnProperty('timeout')) {
        timeout = Number(req.parameters.timeout);
      }
    } catch (e) {}

    // Now get to work, first get the write lock on the Plan in the Agency:
    var success = ArangoAgency.lockRead('Supervision', timeout);
    if (!success) {
      actions.resultError(req, res, actions.HTTP_REQUEST_TIMEOUT, 0,
        'could not get a read lock on Plan in Agency');
      return;
    }

    try {
      var Health;
      try {
        Health = ArangoAgency.get('Supervision/Health', false, true).arango.Supervision.Health;
      } catch (e1) {
        actions.resultError(req, res, actions.HTTP_NOT_FOUND, 0,
          'Failed to retrieve supervision node from agency!');
        return;
      }

      actions.resultOk(req, res, actions.HTTP_OK, {Health});
    } finally {
      ArangoAgency.unlockRead('Supervision', timeout);
    }
  }
});

// //////////////////////////////////////////////////////////////////////////////
// / @brief allows to query the historic statistics of a DBserver in the cluster
// //////////////////////////////////////////////////////////////////////////////

actions.defineHttp({
  url: '_admin/history',
  prefix: false,

  callback: function (req, res) {
    if (req.requestType !== actions.POST) {
      actions.resultError(req, res, actions.HTTP_FORBIDDEN, 0,
        'only POST requests are allowed');
      return;
    }
    var body = actions.getJsonBody(req, res);
    if (body === undefined) {
      return;
    }
    var DBserver = req.parameters.DBserver;

    // build query
    var figures = body.figures;
    var filterString = ' filter u.time > @startDate';
    var bind = {
      startDate: (new Date().getTime() / 1000) - 20 * 60
    };

    if (cluster.isCoordinator() && !req.parameters.hasOwnProperty('DBserver')) {
      filterString += ' filter u.clusterId == @serverId';
      bind.serverId = cluster.coordinatorId();
    }

    var returnValue = ' return u';
    if (figures) {
      returnValue = ' return { time : u.time, server : {uptime : u.server.uptime} ';

      var groups = {};
      figures.forEach(function (f) {
        var g = f.split('.')[0];
        if (!groups[g]) {
          groups[g] = [];
        }
        groups[g].push(f.split('.')[1] + ' : u.' + f);
      });
      Object.keys(groups).forEach(function (key) {
        returnValue += ', ' + key + ' : {' + groups[key] + '}';
      });
      returnValue += '}';
    }
    // allow at most ((60 / 10) * 20) * 2 documents to prevent total chaos
    var myQueryVal = 'FOR u in _statistics ' + filterString + ' LIMIT 240 SORT u.time' + returnValue;

    if (!req.parameters.hasOwnProperty('DBserver')) {
      // query the local statistics collection
      var cursor = AQL_EXECUTE(myQueryVal, bind);
      res.contentType = 'application/json; charset=utf-8';
      if (cursor instanceof Error) {
        res.responseCode = actions.HTTP_BAD;
        res.body = JSON.stringify({'error': true,
        'errorMessage': 'an error occurred'});
      }
      res.responseCode = actions.HTTP_OK;
      res.body = JSON.stringify({result: cursor.docs});
    } else {
      // query a remote statistics collection
      var options = { timeout: 10 };
      var op = ArangoClusterComm.asyncRequest('POST', 'server:' + DBserver, '_system',
        '/_api/cursor', JSON.stringify({query: myQueryVal, bindVars: bind}), {}, options);
      var r = ArangoClusterComm.wait(op);
      res.contentType = 'application/json; charset=utf-8';
      if (r.status === 'RECEIVED') {
        res.responseCode = actions.HTTP_OK;
        res.body = r.body;
      } else if (r.status === 'TIMEOUT') {
        res.responseCode = actions.HTTP_BAD;
        res.body = JSON.stringify({'error': true,
        'errorMessage': 'operation timed out'});
      } else {
        res.responseCode = actions.HTTP_BAD;
        var bodyobj;
        try {
          bodyobj = JSON.parse(r.body);
        } catch (err) {}
        res.body = JSON.stringify({'error': true,
          'errorMessage': 'error from DBserver, possibly DBserver unknown',
        'body': bodyobj});
      }
    }
  }
});

// //////////////////////////////////////////////////////////////////////////////
// / @start Docu Block JSF_getSecondary
// / (intentionally not in manual)
// / @brief gets the secondary of a primary DBserver
// /
// / @ RESTHEADER{GET /_admin/cluster/getSecondary, Get secondary of a primary DBServer}
// /
// / @ RESTQUERYPARAMETERS
// /
// / @ RESTDESCRIPTION Gets the configuration in the agency of the secondary
// / replicating a primary.
// /
// / @ RESTQUERYPARAMETERS
// /
// / @ RESTQUERYPARAM{primary,string,required}
// / is the ID of the primary whose secondary we would like to get.
// /
// / @ RESTQUERYPARAM{timeout,number,optional}
// / the timeout to use in HTTP requests to the agency, default is 60.
// /
// / @ RESTRETURNCODES
// /
// / @ RESTRETURNCODE{200} is returned when everything went well.
// /
// / @ RESTRETURNCODE{400} the primary was not given as query parameter.
// /
// / @ RESTRETURNCODE{403} server is not a coordinator or method was not GET.
// /
// / @ RESTRETURNCODE{404} the given primary name is not configured in Agency.
// /
// / @ RESTRETURNCODE{408} there was a timeout in the Agency communication.
// /
// / @ RESTRETURNCODE{500} the get operation did not work.
// /
// / @end Docu Block
// //////////////////////////////////////////////////////////////////////////////

actions.defineHttp({
  url: '_admin/cluster/getSecondary',
  allowUseDatabase: true,
  prefix: false,

  callback: function (req, res) {
    if (req.requestType !== actions.GET ||
      !require('@arangodb/cluster').isCoordinator()) {
      actions.resultError(req, res, actions.HTTP_FORBIDDEN, 0,
        'only GET requests are allowed and only to coordinators');
      return;
    }
    if (!req.parameters.hasOwnProperty('primary')) {
      actions.resultError(req, res, actions.HTTP_BAD, 0,
        '"primary" is not given as parameter');
      return;
    }
    var primary = req.parameters.primary;

    var timeout = 60.0;

    try {
      if (req.parameters.hasOwnProperty('timeout')) {
        timeout = Number(req.parameters.timeout);
      }
    } catch (e) {}

    // Now get to work, first get the write lock on the Plan in the Agency:
    var success = ArangoAgency.lockRead('Plan', timeout);
    if (!success) {
      actions.resultError(req, res, actions.HTTP_REQUEST_TIMEOUT, 0,
        'could not get a read lock on Plan in Agency');
      return;
    }

    try {
      var oldValue;
      try {
        oldValue = ArangoAgency.get('Plan/DBServers/' + primary, false, false);
        oldValue = oldValue.arango.Plan.DBServers[primary];
      } catch (e1) {
        actions.resultError(req, res, actions.HTTP_NOT_FOUND, 0,
          'Primary with the given ID is not configured in Agency.');
        return;
      }

      oldValue = oldValue['Plan/DBServers/' + primary];

      actions.resultOk(req, res, actions.HTTP_OK, { primary: primary,
      secondary: oldValue });
    } finally {
      ArangoAgency.unlockRead('Plan', timeout);
    }
  }
});

// //////////////////////////////////////////////////////////////////////////////
// / @start Docu Block JSF_replaceSecondary
// / (intentionally not in manual)
// / @brief exchanges the secondary of a primary DBserver
// /
// / @ RESTHEADER{PUT /_admin/cluster/replaceSecondary, Replace secondary of a primary DBServer}
// /
// / @ RESTDESCRIPTION Replaces the configuration in the agency of the secondary
// / replicating a primary. Use with care, because the old secondary will
// / relatively quickly delete its data. For security reasons and to avoid
// / races, the ID of the old secondary must be given as well.
// /
// / @ RESTBODYPARAM{primary,string,required,string}
// / is the ID of the primary whose secondary is to be changed.
// /
// / @ RESTBODYPARAM{oldSecondary,string,required,string}
// / is the old ID of the secondary.
// /
// / @ RESTBODYPARAM{newSecondary,string,required,string}
// / is the new ID of the secondary.
// /
// / @ RESTBODYPARAM{ttl,number,optional,number}
// / the time to live in seconds for the write lock, default is 60.
// /
// / @ RESTBODYPARAM{timeout,number,optional,number}
// / the timeout to use in HTTP requests to the agency, default is 60.
// /
// / @ RESTRETURNCODES
// /
// / @ RESTRETURNCODE{200} is returned when everything went well.
// /
// / @ RESTRETURNCODE{400} either one of the required body parameters was
// / not given or no server with this ID exists.
// /
// / @ RESTRETURNCODE{403} server is not a coordinator or method was not PUT.
// /
// / @ RESTRETURNCODE{404} the given primary name is not configured in Agency.
// /
// / @ RESTRETURNCODE{408} there was a timeout in the Agency communication.
// /
// / @ RESTRETURNCODE{412} the given oldSecondary was not the current secondary
// / of the given primary.
// /
// / @ RESTRETURNCODE{500} the change operation did not work.
// /
// / @end Docu Block
// //////////////////////////////////////////////////////////////////////////////

actions.defineHttp({
  url: '_admin/cluster/replaceSecondary',
  allowUseDatabase: true,
  prefix: false,

  callback: function (req, res) {

    if (req.requestType !== actions.PUT ||
      !require('@arangodb/cluster').isCoordinator()) {
      actions.resultError(req, res, actions.HTTP_FORBIDDEN, 0,
        'only PUT requests are allowed and only to coordinators');
      return;
    }
    var body = actions.getJsonBody(req, res);
    if (body === undefined) {
      return;
    }
    if (!body.hasOwnProperty('primary') ||
      typeof (body.primary) !== 'string' ||
      !body.hasOwnProperty('oldSecondary') ||
      typeof (body.oldSecondary) !== 'string' ||
      !body.hasOwnProperty('newSecondary') ||
      typeof (body.newSecondary) !== 'string') {
      actions.resultError(req, res, actions.HTTP_BAD, 0,
        'not all three of "primary", "oldSecondary" and ' +
        '"newSecondary" are given in body and are strings');
      return;
    }

    var ttl = 60.0;
    var timeout = 60.0;

    if (body.hasOwnProperty('ttl') && typeof body.ttl === 'number') {
      ttl = body.ttl;
    }
    if (body.hasOwnProperty('timeout') && typeof body.timeout === 'number') {
      timeout = body.timeout;
    }

    // Now get to work, first get the write lock on the Plan in the Agency:
    var success = ArangoAgency.lockWrite('Plan', ttl, timeout);
    if (!success) {
      actions.resultError(req, res, actions.HTTP_REQUEST_TIMEOUT, 0,
        'could not get a write lock on Plan in Agency');
      return;
    }

    try {
      var oldValue;
      try {
        oldValue = ArangoAgency.get('Plan/DBServers/' + body.primary, false,
                                    false);
        if (oldValue.arango.Plan.DBServers.hasOwnProperty(body.primary)) {
          oldValue = oldValue.arango.Plan.DBServers[body.primary];
        } else {
          throw true;
        }
      } catch (e1) {
        actions.resultError(req, res, actions.HTTP_NOT_FOUND, 0,
          'Primary with the given ID is not configured in Agency.');
        return;
      }

      if (oldValue !== body.oldSecondary) {
        actions.resultError(req, res, actions.HTTP_PRECONDITION_FAILED, 0,
          'Primary does not have the given oldSecondary as ' +
          'its secondary, current value: ' + oldValue);
        return;
      }
      try {
        ArangoAgency.set('Plan/DBServers/' + body.primary, body.newSecondary,
          0);
      } catch (e2) {
        actions.resultError(req, res, actions.HTTP_SERVER_ERROR, 0,
          'Cannot change secondary of given primary.');
        return;
      }

      try {
        ArangoAgency.increaseVersion('Plan/Version');
      } catch (e3) {
        actions.resultError(req, res, actions.HTTP_SERVER_ERROR, 0,
          'Cannot increase Plan/Version.');
        return;
      }

      actions.resultOk(req, res, actions.HTTP_OK, body);
    } finally {
      ArangoAgency.unlockWrite('Plan', timeout);
    }
  }
});

// //////////////////////////////////////////////////////////////////////////////
// / @brief changes responsibility for all shards from oldServer to newServer.
// / This needs to be done atomically!
// //////////////////////////////////////////////////////////////////////////////

function changeAllShardReponsibilities (oldServer, newServer) {
  // This is only called when we have the write lock and we "only" have to
  // make sure that either all or none of the shards are moved.
  var collections = ArangoAgency.get('Plan/Collections', true, false);
  collections = collections.arango.Plan.Collections;
  var done = {};
  try {
    Object.keys(collections).forEach(function (collectionKey) {
      var collection = collections[collectionKey];
      var old = _.cloneDeep(collection);

      Object.keys(collection.shards).forEach(function (shardKey) {
        var servers = collection.shards[shardKey];
        collection.shards[shardKey] = servers.map(function (server) {
          if (server === oldServer) {
            return newServer;
          } else {
            return server;
          }
        });
      });
      ArangoAgency.set(collectionKey, collection, 0);
      done[collectionKey] = old;
    });
  } catch (e) {
    // mop: rollback
    try {
      Object.keys(done).forEach(function (collectionKey) {
        ArangoAgency.set(collectionKey, done[collectionKey], 0);
      });
    } catch (e2) {
      console.error('Got error during rollback', e2);
    }
    throw e;
  }
}

// //////////////////////////////////////////////////////////////////////////////
// / @start Docu Block JSF_swapPrimaryAndSecondary
// / (intentionally not in manual)
// / @brief swaps the roles of a primary and secondary pair
// /
// / @ RESTHEADER{PUT /_admin/cluster/swapPrimaryAndSecondary, Swaps the roles of a primary and secondary pair.}
// /
// / @RESTDESCRIPTION Swaps the roles of a primary and replicating secondary
// / pair. This includes changing the entry for all shards for which the
// / primary was responsible to the name of the secondary. All changes happen
// / in a single write transaction (using a write lock) and the Plan/Version
// / is increased. Use with care, because currently replication in the cluster
// / is asynchronous and the old secondary might not yet have all the data.
// / For security reasons and to avoid races, the ID of the old secondary
// / must be given as well.
// /
// / @ RESTBODYPARAM{primary,string,required,string}
// / is the ID of the primary whose secondary is to be changed.
// /
// / @ RESTBODYPARAM{secondary,string,required,string}
// / is the ID of the secondary, which must be the secondary of this primay.
// /
// / @ RESTBODYPARAM{ttl,number,optional,number}
// / the time to live in seconds for the write lock, default is 60.
// /
// / @ RESTBODYPARAM{timeout,number,optional,number}
// / the timeout to use in HTTP requests to the agency, default is 60.
// /
// / @ RESTRETURNCODES
// /
// / @ RESTRETURNCODE{200} is returned when everything went well.
// /
// / @ RESTRETURNCODE{400} either one of the required body parameters was
// / not given or no server with this ID exists.
// /
// / @ RESTRETURNCODE{403} server is not a coordinator or method was not PUT.
// /
// / @ RESTRETURNCODE{404} the given primary name is not configured in Agency.
// /
// / @ RESTRETURNCODE{408} there was a timeout in the Agency communication.
// /
// / @ RESTRETURNCODE{412} the given secondary was not the current secondary
// / of the given primary.
// /
// / @ RESTRETURNCODE{500} the change operation did not work.
// /
// / @end Docu Block
// //////////////////////////////////////////////////////////////////////////////

actions.defineHttp({
  url: '_admin/cluster/swapPrimaryAndSecondary',
  allowUseDatabase: true,
  prefix: false,

  callback: function (req, res) {
    if (req.requestType !== actions.PUT ||
      !require('@arangodb/cluster').isCoordinator()) {
      actions.resultError(req, res, actions.HTTP_FORBIDDEN, 0,
        'only PUT requests are allowed and only to coordinators');
      return;
    }
    var body = actions.getJsonBody(req, res);
    if (body === undefined) {
      return;
    }
    if (!body.hasOwnProperty('primary') ||
      typeof (body.primary) !== 'string' ||
      !body.hasOwnProperty('secondary') ||
      typeof (body.secondary) !== 'string') {
      actions.resultError(req, res, actions.HTTP_BAD, 0,
        'not both "primary" and "secondary" ' +
        'are given in body and are strings');
      return;
    }

    var ttl = 60.0;
    var timeout = 60.0;

    if (body.hasOwnProperty('ttl') && typeof body.ttl === 'number') {
      ttl = body.ttl;
    }
    if (body.hasOwnProperty('timeout') && typeof body.timeout === 'number') {
      timeout = body.timeout;
    }

    // Now get to work, first get the write lock on the Plan in the Agency:
    var success = ArangoAgency.lockWrite('Plan', ttl, timeout);
    if (!success) {
      actions.resultError(req, res, actions.HTTP_REQUEST_TIMEOUT, 0,
        'could not get a write lock on Plan in Agency');
      return;
    }

    try {
      var oldValue;
      try {
        oldValue = ArangoAgency.get('Plan/DBServers/' + body.primary, false,
          false);
        oldValue = oldValue.arango.Plan.DBservers[body.primary];
      } catch (e1) {
        actions.resultError(req, res, actions.HTTP_NOT_FOUND, 0,
          'Primary with the given ID is not configured in Agency.');
        return;
      }
      oldValue = oldValue['Plan/DBServers/' + body.primary];
      if (oldValue !== body.secondary) {
        actions.resultError(req, res, actions.HTTP_PRECONDITION_FAILED, 0,
          'Primary does not have the given secondary as ' +
          'its secondary, current value: ' + oldValue);
        return;
      }
      try {
        ArangoAgency.remove('Plan/DBServers/' + body.primary, false);
      } catch (e2) {
        actions.resultError(req, res, actions.HTTP_SERVER_ERROR, 0,
          'Cannot remove old primary entry.');
        return;
      }
      try {
        ArangoAgency.set('Plan/DBServers/' + body.secondary,
          body.primary, 0);
      } catch (e3) {
        actions.resultError(req, res, actions.HTTP_SERVER_ERROR, 0,
          'Cannot set secondary as primary.');
        // Try to reset the old primary:
        try {
          ArangoAgency.set('Plan/DBServers/' + body.primary,
            body.secondary, 0);
        } catch (e4) {
          actions.resultError(req, res, actions.HTTP_SERVER_ERROR, 0,
            'Cannot set secondary as primary, could not ' +
            'even reset the old value!');
        }
        return;
      }

      try {
        // Now change all responsibilities for shards to the "new" primary
        // body.secondary:
        changeAllShardReponsibilities(body.primary, body.secondary);
      } catch (e5) {
        actions.resultError(req, res, actions.HTTP_SERVER_ERROR, 0,
          'Could not change responsibilities for shards.');
        // Try to reset the old primary:
        try {
          ArangoAgency.set('Plan/DBServers/' + body.primary,
            body.secondary, 0);
          ArangoAgency.remove('Plan/DBServers/' + body.secondary);
        } catch (e4) {
          actions.resultError(req, res, actions.HTTP_SERVER_ERROR, 0,
            'Cannot change responsibility for shards and ' +
            'could not even reset the old value!');
        }
        return;
      }

      try {
        ArangoAgency.increaseVersion('Plan/Version');
      } catch (e3) {
        actions.resultError(req, res, actions.HTTP_SERVER_ERROR, 0,
          'Cannot increase Plan/Version.');
        return;
      }

      actions.resultOk(req, res, actions.HTTP_OK, {primary: body.secondary,
      secondary: body.primary});
    } finally {
      ArangoAgency.unlockWrite('Plan', timeout);
    }
  }
});

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
      actions.resultOk(req, res, actions.HTTP_OK,
        {numberOfCoordinators: nrCoordinators,
          numberOfDBServers: nrDBServers,
        cleanedServers});
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
    var ok = true;
    var id;
    try {
      id = ArangoClusterInfo.uniqid();
      var todo = { 'type': 'cleanOutServer',
        'server': body.server,
        'jobId': id,
        'timeCreated': (new Date()).toISOString(),
      'creator': ArangoServerState.id() };
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
    body.shards=[body.shard];
    body.collections=[body.collection];
    var r = require('@arangodb/cluster').moveShard(body);
    if (r.error) {
      actions.resultError(req, res, actions.HTTP_SERVICE_UNAVAILABLE, r);
      return;
    }
    actions.resultOk(req, res, actions.HTTP_ACCEPTED, r);
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
    var dbsToCheck = []; var diff;

    var getDifference = function (a, b) {
      return a.filter(function (i) {
        return b.indexOf(i) < 0;
      });
    };

    _.each(result.results, function (info, collection) {
      _.each(info.Plan, function (shard, shardkey) {
        // check if shard is out of sync
        if (!_.isEqual(shard.followers, info.Current[shardkey].followers)) {
          // if not in sync, get document counts of leader and compare with follower
          diff = getDifference(shard.followers, info.Current[shardkey].followers);

          dbsToCheck.push({
            shard: shardkey,
            toCheck: diff,
            leader: info.Plan[shardkey].leader,
            collection: collection
          });
        }
      });
    });

    var leaderOP, followerOP, leaderR, followerR, leaderBody, followerBody;
    var options = { timeout: 10 };

    _.each(dbsToCheck, function (shard) {
      if (shard.leader.charAt(0) === '_') {
        shard.leader = shard.leader.substr(1, shard.leader.length - 1);
      }
      if (typeof shard.toCheck === 'object') {
        if (shard.toCheck.length === 0) {
          return;
        }
      }

      // get counts of leader and follower shard
      leaderOP = null;
      try {
        leaderOP = ArangoClusterComm.asyncRequest('GET', 'server:' + shard.leader, '_system',
        '/_api/collection/' + shard.shard + '/count', '', {}, options);
      } catch (e) {
      }

      followerOP = null;
      try {
        followerOP = ArangoClusterComm.asyncRequest('GET', 'server:' + shard.toCheck, '_system',
        '/_api/collection/' + shard.shard + '/count', '', {}, options);
      } catch (e) {
      }
      let leaderCount = null;
      if (leaderOP) {
        leaderR = ArangoClusterComm.wait(leaderOP);
        leaderBody = JSON.parse(leaderR.body);
        leaderCount = leaderBody.count;
      }
      let followerCount = null;
      if (followerOP) {
        followerR = ArangoClusterComm.wait(followerOP);
        followerBody = JSON.parse(followerR.body);
        followerCount = followerBody.count;
      }

      result.results[shard.collection].Plan[shard.shard].progress = {
        total: leaderCount,
        current: followerCount
      };
    });

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
