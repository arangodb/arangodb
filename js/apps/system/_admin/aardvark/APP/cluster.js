'use strict';

////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2010-2013 triAGENS GmbH, Cologne, Germany
/// Copyright 2016 ArangoDB GmbH, Cologne, Germany
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
/// @author Michael Hackstein
/// @author Alan Plum
////////////////////////////////////////////////////////////////////////////////

const _ = require('lodash');
const dd = require('dedent');
const cluster = require('@arangodb/cluster');
const createRouter = require('@arangodb/foxx/router');
const internal = require('internal');

const router = createRouter();
module.exports = router;


router.use((req, res, next) => {
  if (!internal.options()['server.disable-authentication'] && !req.session.uid) {
    res.throw('unauthorized');
  }
  next();
});


router.get('/amICoordinator', function(req, res) {
  res.json(cluster.isCoordinator());
})
.summary('Plan and start a new cluster')
.description('This will plan a new cluster with the information given in the body');


if (cluster.isCluster()) {
  // only make these functions available in cluster mode!
  const Communication = require('@arangodb/cluster/agency-communication');
  const comm = new Communication.Communication();
  const beats = comm.sync.Heartbeats();
  const diff = comm.diff.current;
  const servers = comm.current.DBServers();
  const dbs = comm.current.Databases();
  const coords = comm.current.Coordinators();

  router.get('/ClusterType', function(req, res) {
    // Not yet implemented
    res.json({type: 'symmetricSetup'});
  })
  .summary('Get the type of the cluster')
  .description(dd`
     Returns a string containing the cluster type
     Possible anwers:
     - testSetup
     - symmetricalSetup
     - asymmetricalSetup
  `);

  router.get('/DBServers', function(req, res) {
    const resList = [];
    const list = servers.getList();
    const diffList = diff.DBServers();
    const didBeat = beats.didBeat();
    const serving = beats.getServing();

    _.each(list, function(v, k) {
      v.name = k;
      resList.push(v);
      if (!_.contains(didBeat, k)) {
        v.status = 'critical';
        return;
      }
      if (v.role === 'primary' && !_.contains(serving, k)) {
        v.status = 'warning';
        return;
      }
      v.status = 'ok';
    });

    _.each(diffList.missing, function(v) {
      v.status = 'missing';
      resList.push(v);
    });

    res.json(resList);
  })
  .summary('Get all DBServers')
  .description('Get a list of all running and expected DBServers within the cluster');

  router.get('/Coordinators', function(req, res) {
    const resList = [];
    const list = coords.getList();
    const diffList = diff.Coordinators();
    const didBeat = beats.didBeat();

    _.each(list, (v, k) => {
      v.name = k;
      resList.push(v);
      if (!_.contains(didBeat, k)) {
        v.status = 'critical';
        return;
      }
      v.status = 'ok';
    });

    _.each(diffList.missing, (v) => {
      v.status = 'missing';
      resList.push(v);
    });

    res.json(resList);
  });

  router.get('/Databases', function(req, res) {
    res.json(
      dbs.getList()
      .map((name) => ({name}))
    );
  });

  router.get('/:dbname/Collections', function(req, res) {
    try {
      res.json(
        dbs
        .select(req.pathParams.dbname)
        .getCollections()
        .map((name) => ({name}))
      );
    } catch (e) {
      res.json([]);
    }
  });

  router.get('/:dbname/:colname/Shards', function(req, res) {
    res.json(
      dbs
      .select(req.pathParams.dbname)
      .collection(req.pathParams.colname)
      .getShardsByServers()
    );
  });

  router.get('/:dbname/:colname/Shards/:servername', function(req, res) {
    res.json(
      dbs
      .select(req.pathParams.dbname)
      .collection(req.pathParams.colname)
      .getShardsForServer(req.pathParams.servername)
      .map((c) => ({id: c}))
    );
  });
}
