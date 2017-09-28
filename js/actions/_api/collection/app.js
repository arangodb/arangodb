/* jshint strict: false */
/*global ArangoClusterInfo */

// //////////////////////////////////////////////////////////////////////////////
// / @brief querying and managing collections
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
// / @author Achim Brandt
// / @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
// / @author Copyright 2012, triAGENS GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

var arangodb = require('@arangodb');
var actions = require('@arangodb/actions');
var cluster = require('@arangodb/cluster');
var errors = require('internal').errors;

// //////////////////////////////////////////////////////////////////////////////
// / @brief return a prefixed URL
// //////////////////////////////////////////////////////////////////////////////

function databasePrefix (req, url) {
  // location response (e.g. /_db/dbname/_api/collection/xyz)
  return '/_db/' + encodeURIComponent(arangodb.db._name()) + url;
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief collection representation
// //////////////////////////////////////////////////////////////////////////////

function collectionRepresentation(collection, showProperties, showCount, showFigures) {
  var result = {};

  result.id = collection._id;
  result.name = collection.name();
  result.isSystem = (result.name.charAt(0) === '_');

  if (showProperties) {
    var properties = collection.properties();

    result.doCompact = properties.doCompact;
    result.isVolatile = properties.isVolatile;
    result.journalSize = properties.journalSize;
    result.keyOptions = properties.keyOptions;
    result.waitForSync = properties.waitForSync;
    result.indexBuckets = properties.indexBuckets;
    if (properties.cacheEnabled) {
      result.cacheEnabled = properties.cacheEnabled;
    }

    if (cluster.isCoordinator()) {
      result.avoidServers = properties.avoidServers;
      result.numberOfShards = properties.numberOfShards;
      result.replicationFactor = properties.replicationFactor;
      result.avoidServers = properties.avoidServers;
      result.distributeShardsLike = properties.distributeShardsLike;
      result.shardKeys = properties.shardKeys;
    }
  }

  if (showCount) {
    // show either the count value as a number or the detailed shard counts
    result.count = collection.count(showCount === 'details');
  }

  if (showFigures) {
    var figures = collection.figures();

    if (figures) {
      result.figures = figures;
    }
  }

  result.status = collection.status();
  result.type = collection.type();

  return result;
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief helper to parse arguments for creating collections
// //////////////////////////////////////////////////////////////////////////////

function parseBodyForCreateCollection (req, res) {

  var body = actions.getJsonBody(req, res);

  if (body === undefined) {
    return { bodyIsEmpty: true };
  }

  var r = {};

  if (!body.hasOwnProperty('name')) {
    r.name = '';
  } else {
    r.name = body.name;
  }
  r.parameters = { waitForSync: require('internal').options()['database.wait-for-sync']};
  r.type = arangodb.ArangoCollection.TYPE_DOCUMENT;

  if (body.hasOwnProperty('doCompact')) {
    r.parameters.doCompact = body.doCompact;
  }

  if (body.hasOwnProperty('isSystem')) {
    r.parameters.isSystem = (body.isSystem && r.name[0] === '_');
  }

  if (body.hasOwnProperty('id')) {
    r.parameters.id = body.id;
  }

  if (body.hasOwnProperty('isVolatile')) {
    r.parameters.isVolatile = body.isVolatile;
  }

  if (body.hasOwnProperty('journalSize')) {
    r.parameters.journalSize = body.journalSize;
  }

  if (body.hasOwnProperty('indexBuckets')) {
    r.parameters.indexBuckets = body.indexBuckets;
  }

  if (body.hasOwnProperty('keyOptions')) {
    r.parameters.keyOptions = body.keyOptions;
  }

  if (body.hasOwnProperty('type')) {
    r.type = body.type;
  }

  if (body.hasOwnProperty('waitForSync')) {
    r.parameters.waitForSync = body.waitForSync;
  }

  if (body.hasOwnProperty('cacheEnabled')) {
    r.parameters.cacheEnabled = body.cacheEnabled;
  }

  if (cluster.isCoordinator()) {
    if (body.hasOwnProperty('shardKeys')) {
      r.parameters.shardKeys = body.shardKeys || { };
    }

    if (body.hasOwnProperty('numberOfShards')) {
      r.parameters.numberOfShards = body.numberOfShards || 0;
    }

    if (body.hasOwnProperty('distributeShardsLike')) {
      r.parameters.distributeShardsLike = body.distributeShardsLike || '';
    }

    if (body.hasOwnProperty('avoidServers')) {
      r.parameters.avoidServers = body.avoidServers || [];
    }

    if (body.hasOwnProperty('isSmart')) {
      r.parameters.isSmart = body.isSmart || '';
    }

    if (body.hasOwnProperty('smartGraphAttribute')) {
      r.parameters.smartGraphAttribute = body.smartGraphAttribute || '';
    }

    if (body.hasOwnProperty('replicationFactor')) {
      r.parameters.replicationFactor = body.replicationFactor || '';
    }

    if (body.hasOwnProperty('servers')) {
      r.parameters.servers = body.servers || '';
    }
  }

  return r;
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief was docuBlock JSF_post_api_collection
// //////////////////////////////////////////////////////////////////////////////

function post_api_collection (req, res) {
  var r = parseBodyForCreateCollection(req, res);

  if (r.bodyIsEmpty) {
    return; // error in JSON, is already reported
  }

  if (r.name === '') {
    actions.resultBad(req, res, arangodb.ERROR_ARANGO_ILLEGAL_NAME,
      'name must be non-empty');
    return;
  }

  try {
    var options = {};
    if (req.parameters.hasOwnProperty('waitForSyncReplication')) {
      var value = req.parameters.waitForSyncReplication.toLowerCase();
      if (value === 'true' || value === 'yes' || value === 'on' || value === 'y' || value === '1') {
        options.waitForSyncReplication = true;
      } else {
        options.waitForSyncReplication = false;
      }
    }
    var collection;
    if (typeof (r.type) === 'string') {
      if (r.type.toLowerCase() === 'edge' || r.type === '3') {
        r.type = arangodb.ArangoCollection.TYPE_EDGE;
      }
    }

    if (r.type === arangodb.ArangoCollection.TYPE_EDGE) {
      collection = arangodb.db._createEdgeCollection(r.name, r.parameters, options);
    } else {
      collection = arangodb.db._createDocumentCollection(r.name, r.parameters, options);
    }

    var result = {};

    result.id = collection._id;
    result.name = collection.name();
    result.waitForSync = r.parameters.waitForSync || false;
    result.isVolatile = r.parameters.isVolatile || false;
    result.isSystem = r.parameters.isSystem || false;
    result.status = collection.status();
    result.type = collection.type();
    result.keyOptions = collection.keyOptions;
    if (r.parameters.cacheEnabled !== undefined) {
      result.cacheEnabled = r.parameters.cacheEnabled;
    }

    if (cluster.isCoordinator()) {
      result.shardKeys = collection.shardKeys;
      result.numberOfShards = collection.numberOfShards;
      result.distributeShardsLike = collection.distributeShardsLike || '';
    }

    var headers = {
      location: databasePrefix(req, '/_api/collection/' + result.name)
    };

    actions.resultOk(req, res, actions.HTTP_OK, result, headers);
  } catch (err) {
    actions.resultException(req, res, err, undefined, false);
  }
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief was docuBlock JSF_get_api_collections
// //////////////////////////////////////////////////////////////////////////////

function get_api_collections (req, res) {
  var excludeSystem;
  var collections = arangodb.db._collections();

  excludeSystem = false;
  if (req.parameters.hasOwnProperty('excludeSystem')) {
    var value = req.parameters.excludeSystem.toLowerCase();
    if (value === 'true' || value === 'yes' || value === 'on' || value === 'y' || value === '1') {
      excludeSystem = true;
    }
  }

  var list = [];
  for (var i = 0;  i < collections.length;  ++i) {
    var collection = collections[i];
    var rep = collectionRepresentation(collection);

    // include system collections or exclude them?
    if (!excludeSystem || rep.name.substr(0, 1) !== '_') {
      list.push(rep);
    }
  }

  actions.resultOk(req, res, actions.HTTP_OK, { result: list });
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief was docuBlock JSA_get_api_collection_name
// //////////////////////////////////////////////////////////////////////////////

// //////////////////////////////////////////////////////////////////////////////
// / @brief was docuBlock JSA_get_api_collection_properties
// //////////////////////////////////////////////////////////////////////////////

// //////////////////////////////////////////////////////////////////////////////
// / @brief was docuBlock JSA_get_api_collection_count
// //////////////////////////////////////////////////////////////////////////////

// //////////////////////////////////////////////////////////////////////////////
// / @brief was docuBlock JSA_get_api_collection_figures
// //////////////////////////////////////////////////////////////////////////////

// //////////////////////////////////////////////////////////////////////////////
// / @brief was docuBlock JSA_get_api_collection_revision
// //////////////////////////////////////////////////////////////////////////////

// //////////////////////////////////////////////////////////////////////////////
// / @brief was docuBlock JSA_get_api_collection_checksum
// //////////////////////////////////////////////////////////////////////////////

function get_api_collection (req, res) {
  var name;
  var result;
  var sub;

  // .............................................................................
  // /_api/collection
  // .............................................................................

  if (req.suffix.length === 0 && req.parameters.id === undefined) {
    get_api_collections(req, res);
    return;
  }

  // .............................................................................
  // /_api/collection/<name>
  // .............................................................................

  name = req.suffix[0];

  var collection = arangodb.db._collection(name);

  if (collection === null) {
    actions.collectionNotFound(req, res, name);
    return;
  }

  var headers;

  // .............................................................................
  // /_api/collection/<name>
  // .............................................................................

  if (req.suffix.length === 1) {
    result = collectionRepresentation(collection, false, false, false);
    headers = {
      location: databasePrefix(req, '/_api/collection/' + collection.name())
    };
    actions.resultOk(req, res, actions.HTTP_OK, result, headers);
    return;
  }

  if (req.suffix.length === 2) {
    sub = req.suffix[1];

    // .............................................................................
    // /_api/collection/<identifier>/checksum
    // .............................................................................

    if (sub === 'checksum') {
      var withRevisions = false;
      var withData = false;
      var value;
      if (req.parameters.hasOwnProperty('withRevisions')) {
        value = req.parameters.withRevisions.toLowerCase();
        if (value === 'true' || value === 'yes' || value === 'on' || value === 'y' || value === '1') {
          withRevisions = true;
        }
      }

      if (req.parameters.hasOwnProperty('withData')) {
        value = req.parameters.withData.toLowerCase();
        if (value === 'true' || value === 'yes' || value === 'on' || value === 'y' || value === '1') {
          withData = true;
        }
      }

      result = collectionRepresentation(collection, false, false, false);
      var checksum = collection.checksum(withRevisions, withData);
      result.checksum = checksum.checksum;
      result.revision = checksum.revision;
      actions.resultOk(req, res, actions.HTTP_OK, result);
    }

    // .............................................................................
    // /_api/collection/<identifier>/figures
    // .............................................................................
    else if (sub === 'figures') {
      result = collectionRepresentation(collection, true, true, true);
      headers = {
        location: databasePrefix(req, '/_api/collection/' + collection.name() + '/figures')
      };
      actions.resultOk(req, res, actions.HTTP_OK, result, headers);
    }

    // .............................................................................
    // /_api/collection/<identifier>/count
    // .............................................................................
    else if (sub === 'count') {
      // show either the count value as a number or the detailed shard counts
      if (req.parameters.details === 'true') {
        result = collectionRepresentation(collection, true, 'details', false);
      } else {
        result = collectionRepresentation(collection, true, true, false);
      }
      headers = {
        location: databasePrefix(req, '/_api/collection/' + collection.name() + '/count')
      };
      actions.resultOk(req, res, actions.HTTP_OK, result, headers);
    }

    // .............................................................................
    // /_api/collection/<identifier>/properties
    // .............................................................................
    else if (sub === 'properties') {
      result = collectionRepresentation(collection, true, false, false);
      headers = {
        location: databasePrefix(req, '/_api/collection/' + collection.name() + '/properties')
      };
      actions.resultOk(req, res, actions.HTTP_OK, result, headers);
    }

    // .............................................................................
    // /_api/collection/<identifier>/revision
    // .............................................................................
    else if (sub === 'revision') {
      result = collectionRepresentation(collection, false, false, false);
      result.revision = collection.revision();
      actions.resultOk(req, res, actions.HTTP_OK, result);
    }
    
    else if (sub === 'shards') {
      result = collectionRepresentation(collection, false, false, false);
      result.shards = Object.keys(ArangoClusterInfo.getCollectionInfo(arangodb.db._name(), collection.name()).shardShorts);
      actions.resultOk(req, res, actions.HTTP_OK, result);

    } else {
      actions.resultNotFound(req, res, arangodb.ERROR_HTTP_NOT_FOUND,
        "expecting one of the resources 'checksum', 'count',"
        + " 'figures', 'properties', 'revision', 'shards'");
    }
  } else {
    actions.resultBad(req, res, arangodb.ERROR_HTTP_BAD_PARAMETER,
      'expect GET /_api/collection/<collection-name>/<method>');
  }
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief was docuBlock JSF_put_api_collection_load
// //////////////////////////////////////////////////////////////////////////////

function put_api_collection_load (req, res, collection) {
  try {
    collection.load();

    var showCount = true;
    var body = actions.getJsonBody(req, res);

    if (body && body.hasOwnProperty('count')) {
      showCount = body.count;
    }

    var result = collectionRepresentation(collection, false, showCount, false);

    actions.resultOk(req, res, actions.HTTP_OK, result);
  } catch (err) {
    actions.resultException(req, res, err, undefined, false);
  }
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief was docuBlock JSF_put_api_collection_loadIndexesIntoMemory
// //////////////////////////////////////////////////////////////////////////////

function put_api_collection_load_indexes_in_memory (req, res, collection) {
  try {
    // Load all index values into Memory
    collection.loadIndexesIntoMemory();

    actions.resultOk(req, res, actions.HTTP_OK, { result: true });
  } catch (err) {
    actions.resultException(req, res, err, undefined, false);
  }
}



// //////////////////////////////////////////////////////////////////////////////
// / @brief was docuBlock JSF_put_api_collection_unload
// //////////////////////////////////////////////////////////////////////////////

function put_api_collection_unload (req, res, collection) {
  try {
    if (req.parameters.hasOwnProperty('flush')) {
      var value = req.parameters.flush.toLowerCase();
      if (value === 'true' || value === 'yes' || value === 'on' || value === 'y' || value === '1') {
        if (collection.status() === 3 /* loaded */ &&
          collection.figures().uncollectedLogfileEntries > 0) {
          // flush WAL so uncollected logfile entries can get collected
          require('internal').wal.flush();
        }
      }
    }

    // then unload
    collection.unload();

    var result = collectionRepresentation(collection);

    actions.resultOk(req, res, actions.HTTP_OK, result);
  } catch (err) {
    actions.resultException(req, res, err, undefined, false);
  }
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief was docuBlock JSF_put_api_collection_truncate
// //////////////////////////////////////////////////////////////////////////////

function put_api_collection_truncate (req, res, collection) {
  let waitForSync = false;
  if (req.parameters.hasOwnProperty('waitForSync')) {
    let value = req.parameters.waitForSync.toLowerCase();
    if (value === 'true' || value === 'yes' ||
        value === 'on' || value === 'y' || value === '1') {
      waitForSync = true;
    }
  }
  let isSynchronousReplicationFrom = "";
  if (req.parameters.hasOwnProperty('isSynchronousReplication')) {
    isSynchronousReplicationFrom = req.parameters.isSynchronousReplication;
  }
  try {
    collection.truncate(waitForSync, isSynchronousReplicationFrom);

    var result = collectionRepresentation(collection);

    actions.resultOk(req, res, actions.HTTP_OK, result);
  } catch (err) {
    actions.resultException(req, res, err, undefined, false);
  }
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief was docuBlock JSF_put_api_collection_properties
// //////////////////////////////////////////////////////////////////////////////

function put_api_collection_properties (req, res, collection) {
  var body = actions.getJsonBody(req, res);

  if (body === undefined) {
    return;
  }

  try {
    collection.properties(body);

    var result = collectionRepresentation(collection, true);

    actions.resultOk(req, res, actions.HTTP_OK, result);
  } catch (err) {
    actions.resultException(req, res, err, undefined, false);
  }
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief was docuBlock JSF_put_api_collection_rename
// //////////////////////////////////////////////////////////////////////////////

function put_api_collection_rename (req, res, collection) {
  var body = actions.getJsonBody(req, res);

  if (body === undefined) {
    return;
  }

  if (!body.hasOwnProperty('name')) {
    actions.resultBad(req, res, arangodb.ERROR_ARANGO_ILLEGAL_NAME,
      'name must be non-empty');
    return;
  }

  var name = body.name;

  try {
    collection.rename(name);

    var result = collectionRepresentation(collection);

    actions.resultOk(req, res, actions.HTTP_OK, result);
  } catch (err) {
    actions.resultException(req, res, err, undefined, false);
  }
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief was docuBlock JSF_put_api_collection_rotate
// //////////////////////////////////////////////////////////////////////////////

function put_api_collection_rotate (req, res, collection) {
  try {
    collection.rotate();

    actions.resultOk(req, res, actions.HTTP_OK, { result: true });
  } catch (err) {
    actions.resultException(req, res, err, undefined, false);
  }
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief changes a collection
// //////////////////////////////////////////////////////////////////////////////

function put_api_collection (req, res) {
  if (req.suffix.length !== 2) {
    actions.resultBad(req, res, arangodb.ERROR_HTTP_BAD_PARAMETER,
      'expected PUT /_api/collection/<collection-name>/<action>');
    return;
  }

  var name = req.suffix[0];
  var collection = arangodb.db._collection(name);

  if (collection === null) {
    actions.collectionNotFound(req, res, name);
    return;
  }

  var sub = req.suffix[1];

  if (sub === 'load') {
    put_api_collection_load(req, res, collection);
  } else if (sub === 'unload') {
    put_api_collection_unload(req, res, collection);
    collection = null;
    // run garbage collection once in all threads
    require('internal').executeGlobalContextFunction('collectGarbage');
  } else if (sub === 'truncate') {
    put_api_collection_truncate(req, res, collection);
  } else if (sub === 'properties') {
    put_api_collection_properties(req, res, collection);
  } else if (sub === 'rename') {
    put_api_collection_rename(req, res, collection);
  } else if (sub === 'rotate') {
    put_api_collection_rotate(req, res, collection);
  } else if (sub === 'loadIndexesIntoMemory') {
    put_api_collection_load_indexes_in_memory(req, res, collection);
  } else {
    actions.resultNotFound(req, res, arangodb.ERROR_HTTP_NOT_FOUND,
      "expecting one of the actions 'load', 'unload',"
      + " 'truncate', 'properties', 'rename', 'loadIndexesIntoMemory'");
  }
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief was docuBlock JSF_delete_api_collection
// //////////////////////////////////////////////////////////////////////////////

function delete_api_collection (req, res) {
  if (req.suffix.length !== 1) {
    actions.resultBad(req, res, arangodb.ERROR_HTTP_BAD_PARAMETER,
      'expected DELETE /_api/collection/<collection-name>');
  } else {
    var name = req.suffix[0];
    var collection = arangodb.db._collection(name);

    if (collection === null) {
      actions.collectionNotFound(req, res, name);
    } else {
      try {
        var result = {
          id: collection._id
        };

        var options = {};
        if (req.parameters.hasOwnProperty('isSystem')) {
          // are we allowed to drop system collections?
          var value = req.parameters.isSystem.toLowerCase();
          if (value === 'true' || value === 'yes' || value === 'on' || value === 'y' || value === '1') {
            options.isSystem = true;
          }
        }
        collection.drop(options);

        actions.resultOk(req, res, actions.HTTP_OK, result);
      } catch (err) {
        actions.resultException(req, res, err, undefined, false);
      }
    }
  }
}



// //////////////////////////////////////////////////////////////////////////////
// / @brief handles a collection request
// //////////////////////////////////////////////////////////////////////////////

actions.defineHttp({
  url: '_api/collection',

  callback: function (req, res) {
    try {
      if (req.requestType === actions.GET) {
        get_api_collection(req, res);
      } else if (req.requestType === actions.DELETE) {
        delete_api_collection(req, res);
      } else if (req.requestType === actions.POST) {
        post_api_collection(req, res);
      } else if (req.requestType === actions.PUT) {
        put_api_collection(req, res);
      } else {
        actions.resultUnsupported(req, res);
      }
    } catch (err) {
      actions.resultException(req, res, err, undefined, false);
    }
  }
});
