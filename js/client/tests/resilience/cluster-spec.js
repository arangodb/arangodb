/*global print, arango, describe, it */
'use strict';

const InstanceManager = require('@arangodb/testing/InstanceManager.js');
const wait = require('internal').wait;
const expect = require('chai').expect;

describe('ClusterResilience', function() {
  let instanceManager;
  before(function() {
    instanceManager = new InstanceManager('cluster_resilience');
    let endpoint = instanceManager.startCluster(3, 2, 2);
    arango.reconnect(endpoint, '_system');
  })

  afterEach(function() {
    instanceManager.check();
  })

  after(function() {
    instanceManager.cleanup();
  })

  it('should setup and teardown the cluster properly', function() {
  })

  /*
  it('should report the same number of documents after a server restart', function() {
    db._create('testcollection', { shards: 4});
    db.testcollection.insert({'testung': Date.now()});
    db.testcollection.insert({'testung': Date.now()});
    db.testcollection.insert({'testung': Date.now()});
    db.testcollection.insert({'testung': Date.now()});
    db.testcollection.insert({'testung': Date.now()});
    db.testcollection.insert({'testung': Date.now()});
    db.testcollection.insert({'testung': Date.now()});

    let count = 7;
    expect(db.testcollection.count()).to.equal(count);

    let dbServer = instanceManager.dbServers()[0];
    instanceManager.kill(dbServer);
    wait(0.5);
    instanceManager.restart(dbServer);
    instanceManager.waitForAllInstances();

    expect(count).to.equal(db.testcollection.count());
    })
  */
});
