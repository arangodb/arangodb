/* global ArangoServerState */
const internal = require('internal');
const db = internal.db;
let IM = global.instanceManager;

db.foxxqueuetest.replace('test', {'date': Date.now(), 'server': ArangoServerState.id()});

if (IM.debugCanUseFailAt()
  && IM.debugShouldFailAt("foxxmaster::queuetest")) {
  internal.suspendExternal(internal.getPid());
}

module.exports = true;
