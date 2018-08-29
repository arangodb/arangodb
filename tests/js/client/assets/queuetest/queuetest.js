const db = require('internal').db;

db.foxxqueuetest.replace('test', {'date': Date.now(), 'server': ArangoServerState.id()});
module.exports = true;
