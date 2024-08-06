/* global ArangoServerState */
const internal = require('internal');
const db = internal.db;

db.foxxqueuetest.replace('test', {'date': Date.now(), 'server': ArangoServerState.id()});

if (internal.debugCanUseFailAt()
  && internal.debugShouldFailAt("foxxmaster::queuetest")) {
  internal.suspendExternal(internal.getPid());
}

module.exports = true;
