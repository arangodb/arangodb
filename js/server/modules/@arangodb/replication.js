'use strict';

// //////////////////////////////////////////////////////////////////////////////
// / @brief Replication management
// /
// / @file
// /
// / DISCLAIMER
// /
// / Copyright 2012 triagens GmbH, Cologne, Germany
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
// / Copyright holder is triAGENS GmbH, Cologne, Germany
// /
// / @author Jan Steemann
// / @author Copyright 2013, triAGENS GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

var internal = require('internal');
var endpointToURL = require('@arangodb/cluster').endpointToURL;
var request = require('@arangodb/request').request;

var logger = { };
var applier = { };

// //////////////////////////////////////////////////////////////////////////////
// / @brief return the replication logger state
// //////////////////////////////////////////////////////////////////////////////

logger.state = function () {
  return internal.getStateReplicationLogger();
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief return the tick ranges provided by the replication logger 
// //////////////////////////////////////////////////////////////////////////////

logger.tickRanges = function () {
  return internal.tickRangesReplicationLogger();
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief return the first tick that can be provided by the replication logger
// //////////////////////////////////////////////////////////////////////////////

logger.firstTick = function () {
  return internal.firstTickReplicationLogger();
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief starts the replication applier
// //////////////////////////////////////////////////////////////////////////////

applier.start = function (initialTick, barrierId) {
  if (initialTick === undefined) {
    return internal.startReplicationApplier();
  }

  return internal.startReplicationApplier(initialTick, barrierId);
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief shuts down the replication applier
// //////////////////////////////////////////////////////////////////////////////

applier.shutdown = applier.stop = function () {
  return internal.shutdownReplicationApplier();
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief return the replication applier state
// //////////////////////////////////////////////////////////////////////////////

applier.state = function () {
  return internal.getStateReplicationApplier();
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief stop the applier and "forget" all configuration
// //////////////////////////////////////////////////////////////////////////////

applier.forget = function () {
  return internal.forgetStateReplicationApplier();
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief returns the configuration of the replication applier
// //////////////////////////////////////////////////////////////////////////////

applier.properties = function (config) {
  if (config === undefined) {
    return internal.configureReplicationApplier();
  }

  return internal.configureReplicationApplier(config);
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief performs a one-time synchronization with a remote endpoint
// //////////////////////////////////////////////////////////////////////////////

function sync (config) {
  return internal.synchronizeReplication(config);
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief performs a one-time synchronization with a remote endpoint
// //////////////////////////////////////////////////////////////////////////////

function syncCollection (collection, config) {
  config = config || { };
  config.restrictType = 'include';
  config.restrictCollections = [ collection ];
  config.includeSystem = true;
  if (!config.hasOwnProperty('verbose')) {
    config.verbose = false;
  }

  return internal.synchronizeReplication(config);
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief sets up the replication (all-in-one function for initial
// / synchronization and continuous replication)
// //////////////////////////////////////////////////////////////////////////////

function setupReplication (config) {
  config = config || { };
  if (!config.hasOwnProperty('autoStart')) {
    config.autoStart = true;
  }
  if (!config.hasOwnProperty('includeSystem')) {
    config.includeSystem = true;
  }
  if (!config.hasOwnProperty('verbose')) {
    config.verbose = false;
  }
  config.keepBarrier = true;

  try {
    // stop previous instance
    applier.stop();
  } catch (err) {}
  // remove existing configuration
  applier.forget();

  // run initial sync
  var result = internal.synchronizeReplication(config);

  // store applier configuration
  applier.properties(config);

  applier.start(result.lastLogTick, result.barrierId);
  return applier.state();
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief returns the server's id
// //////////////////////////////////////////////////////////////////////////////

function serverId () {
  return internal.serverId();
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief finishes off a synchronization of a single collection
// / `collection` is the name of a collection on the other server that should
// / have been synchronized using syncCollection (incremental or not) 
// / recently. `from` is the tick as a string of that synchronization.
// / `config` can contain the following attributes:
// /   - "endpoint": endpoint of the other server
// / If one ensures that there is a read transaction ongoing on that collection
// / on the other server, then this function is guaranteed to bring the two
// / collections in perfect sync, until that read transaction is aborted.
// //////////////////////////////////////////////////////////////////////////////

var mType = {
  REPLICATION_COLLECTION_CREATE: 2000,
  REPLICATION_COLLECTION_DROP: 2001,
  REPLICATION_COLLECTION_RENAME: 2002,
  REPLICATION_COLLECTION_CHANGE: 2003,

  REPLICATION_INDEX_CREATE: 2100,
  REPLICATION_INDEX_DROP: 2101,

  REPLICATION_TRANSACTION_START: 2200,
  REPLICATION_TRANSACTION_COMMIT: 2201,
  REPLICATION_TRANSACTION_ABORT: 2202,

  REPLICATION_MARKER_DOCUMENT: 2300,
  REPLICATION_MARKER_EDGE: 2301,
  REPLICATION_MARKER_REMOVE: 2302
};

function syncCollectionFinalize (database, collname, from, config) {
  var url = endpointToURL(config.endpoint) + '/_db/' + database +
    '/_api/replication/logger-follow?collection=' + collname + '&from=';

  var coll = require('internal').db[collname];

  var transactions = {};

  function apply (entry) {
    var todo;

    function tryPostpone (entry) {
      if (entry.tid !== '0') {
        todo = transactions[entry.tid];
        if (Array.isArray(todo)) {
          entry.tid = '0';
          todo.push(entry);
          return true;
        }
      }
      return false;
    }

    if (entry.type === mType.REPLICATION_MARKER_DOCUMENT) {
      if (tryPostpone(entry)) {
        return;
      }
      try {
        coll.insert(entry.data, {isRestore: true});
        return;
      } catch (err) {
        console.debug('syncCollectionFinalize: insert1', entry, JSON.stringify(err));
      }
      try {
        coll.replace(entry.data._key, entry.data, {isRestore: true});
      } catch (errx) {
        console.error('syncCollectionFinalize: replace1', entry, JSON.stringify(errx));
        throw errx;
      }
    } else if (entry.type === mType.REPLICATION_MARKER_EDGE) {
      if (tryPostpone(entry)) {
        return;
      }
      try {
        coll.insert(entry.data, {isRestore: true});
        return;
      } catch (err) {
        console.debug('syncCollectionFinalize: insert2', entry, JSON.stringify(err));
      }
      try {
        coll.replace(entry.data._key, entry.data, {isRestore: true});
      } catch (errx) {
        console.error('syncCollectionFinalize: replace2', entry, JSON.stringify(errx));
        throw errx;
      }
    } else if (entry.type === mType.REPLICATION_MARKER_REMOVE) {
      if (tryPostpone(entry)) {
        return;
      }
      try {
        coll.remove(entry.data._key);
      } catch (errx) {
        console.error('syncCollectionFinalize: remove', entry, JSON.stringify(errx));
        throw errx;
      }
    } else if (entry.type === mType.REPLICATION_TRANSACTION_START) {
      transactions[entry.tid] = [];
    } else if (entry.type === mType.REPLICATION_TRANSACTION_COMMIT) {
      todo = transactions[entry.tid];
      if (Array.isArray(todo)) {
        for (var i = 0; i < todo.length; i++) {
          apply(todo[i]);
        }
      }
      delete transactions[entry.tid];
    } else if (entry.type === mType.REPLICATION_TRANSACTION_ABORT) {
      delete transactions[entry.tid];
    } else if (entry.type === mType.REPLICATION_INDEX_CREATE) {
      try {
        coll.ensureIndex(entry.data);
      } catch(errx) {
        console.error('syncCollectionFinalize: ensureIndex', entry, JSON.stringify(errx));
        throw errx;
      }
    } else if (entry.type === mType.REPLICATION_INDEX_DROP) {
      try {
        coll.dropIndex(entry.data.id);
      } catch(errx) {
        console.error('syncCollectionFinalize: dropIndex', entry, JSON.stringify(errx));
        throw errx;
      }
    } else if (entry.type === mType.REPLICATION_COLLECTION_CHANGE) {
      try {
        coll.properties(entry.data);
      } catch(errx) {
        console.error('syncCollectionFinalize: properties', entry, JSON.stringify(errx));
        throw errx;
      }
    } else {
      // all else, including dropping and creating the collection
      throw 'Found collection drop, create or rename marker.';
    }
  }

  while (true) {
    var chunk = request(url + from);
    if (chunk.statusCode !== 200 &&
      chunk.statusCode !== 204) {
      return {error: true, errorMessage: 'could not contact leader',
      response: chunk};
    }
    if (chunk.statusCode === 204 || chunk.body.length === 0) {
      break; // all done
    }

    var l = chunk.body.split('\n');
    l.pop();
    try {
      l = l.map(JSON.parse);
    } catch (err) {
      return {error: true, errorMessage: 'could not parse body',
      response: chunk, exception: err};
    }

    console.trace('Applying chunk:', l);
    try {
      for (var i = 0; i < l.length; i++) {
        apply(l[i]);
      }
    } catch (err2) {
      return {error: true, errorMessage: 'could not replicate body ops',
      response: chunk, exception: err2};
    }
    if (chunk.headers['x-arango-replication-checkmore'] !== 'true') {
      break; // all done
    }

    from = chunk.headers['x-arango-replication-lastincluded'];
  }
  return {error: false};
}

exports.logger = logger;
exports.applier = applier;
exports.sync = sync;
exports.syncCollection = syncCollection;
exports.setupReplication = setupReplication;
exports.serverId = serverId;
exports.syncCollectionFinalize = syncCollectionFinalize;
