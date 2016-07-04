/* jshint strict: false */
/* global CREATE_CURSOR */

// //////////////////////////////////////////////////////////////////////////////
// / @brief simple queries
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

var actions = require('@arangodb/actions');
var db = require('@arangodb').db;
var ERRORS = require('internal').errors;

var API = '_api/simple/';

// //////////////////////////////////////////////////////////////////////////////
// / @brief checks the number of shards
// //////////////////////////////////////////////////////////////////////////////

var checkShards = function (req, collection) {
  if (collection === null) {
    throw 'unexpected collection value';
  }

  var cluster = require('@arangodb/cluster');
  if (!cluster.isCoordinator()) {
    // no cluster
    return true;
  }

  // check number of shards
  var shards = cluster.shardList(req.database, collection.name());
  if (shards.length <= 1) {
    return true;
  }

  return false;
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief returns all documents of a collection matching a given example,
// / using a specific hash index
// /
// / RESTHEADER{PUT /_api/simple/by-example-hash, Hash index}
// /
// / **Note**: This is only used internally and should not be accessible by the user
// /           and thus this documentation is private.
// /
// / RESTBODYPARAM{query,string,required,string}
// / Contains the query specification.
// /
// / RESTDESCRIPTION
// /
// / This will find all documents matching a given example, using the specified
// / hash index.
// /
// / The call expects a JSON object as body with the following attributes:
// /
// / - *collection*: The name of the collection to query.
// /
// / - *index*: The id of the index to be used for the query. The index must exist
// /   and must be of type *hash*.
// /
// / - *example*: an example document. The example must contain a value for each
// /   attribute in the index.
// /
// / - *skip*: The number of documents to skip in the query. (optional)
// /
// / - *limit*: The maximal number of documents to return. (optional)
// /
// / Returns a cursor containing the result, see [Http Cursor](../HttpAqlQueryCursor/README.md) for details.
// /
// / RESTRETURNCODES
// /
// / RESTRETURNCODE{201}
// / is returned if the query was executed successfully.
// /
// / RESTRETURNCODE{400}
// / is returned if the body does not contain a valid JSON representation of a
// / query. The response body contains an error document in this case.
// /
// / RESTRETURNCODE{404}
// / is returned if the collection specified by *collection* is unknown.  The
// / response body contains an error document in this case.
// / The same error code is also returned if an invalid index id or type is used.
// //////////////////////////////////////////////////////////////////////////////

// //////////////////////////////////////////////////////////////////////////////
// / @brief returns all documents of a collection matching a given example,
// / using a specific skiplist index
// /
// / RESTHEADER{PUT /_api/simple/by-example-skiplist, Skiplist index}
// /
// / **Note**: This is only used internally and should not be accesible by the user.
// /           and thus this documentation is private.
// /
// / RESTBODYPARAM{query,string,required,string}
// / Contains the query specification.
// /
// / RESTDESCRIPTION
// /
// / This will find all documents matching a given example, using the specified
// / skiplist index.
// /
// / The call expects a JSON object as body with the following attributes:
// /
// / - *collection*: The name of the collection to query.
// /
// / - *index*: The id of the index to be used for the query. The index must
// /   exist and must be of type *skiplist*.
// /
// / - *example*: an example document. The example must contain a value for each
// /   attribute in the index.
// /
// / - *skip*: The number of documents to skip in the query. (optional)
// /
// / - *limit*: The maximal number of documents to return. (optional)
// /
// / Returns a cursor containing the result, see [Http Cursor](../HttpAqlQueryCursor/README.md) for details.
// /
// / RESTRETURNCODES
// /
// / RESTRETURNCODE{201}
// / is returned if the query was executed successfully.
// /
// / RESTRETURNCODE{400}
// / is returned if the body does not contain a valid JSON representation of a
// / query. The response body contains an error document in this case.
// /
// / RESTRETURNCODE{404}
// / is returned if the collection specified by *collection* is unknown.  The
// / response body contains an error document in this case.
// / The same error code is also returned if an invalid index id or type is used.
// //////////////////////////////////////////////////////////////////////////////

// //////////////////////////////////////////////////////////////////////////////
// / @brief returns all documents of a collection matching a given condition,
// / using a specific skiplist index
// /
// / RESTHEADER{PUT /_api/simple/by-condition-skiplist,Query by-condition using Skiplist index}
// /
// / **Note**: This is only used internally and should not be accesible by the user.
// /           and thus this documentation is private.
// /
// / RESTBODYPARAM{query,string,required,string}
// / Contains the query specification.
// /
// / RESTDESCRIPTION
// /
// / This will find all documents matching a given condition, using the specified
// / skiplist index.
// /
// / The call expects a JSON object as body with the following attributes:
// /
// / - *collection*: The name of the collection to query.
// /
// / - *index*: The id of the index to be used for the query. The index must
// /   exist and must be of type *skiplist*.
// /
// / - *condition*: the condition which all returned documents shall satisfy.
// /   Conditions must be specified for all indexed attributes.
// /
// / - *skip*: The number of documents to skip in the query. (optional)
// /
// / - *limit*: The maximal number of documents to return. (optional)
// /
// / Returns a cursor containing the result, see [Http Cursor](../HttpAqlQueryCursor/README.md) for details.
// /
// / RESTRETURNCODES
// /
// / RESTRETURNCODE{201}
// / is returned if the query was executed successfully.
// /
// / RESTRETURNCODE{400}
// / is returned if the body does not contain a valid JSON representation of a
// / query. The response body contains an error document in this case.
// /
// / RESTRETURNCODE{404}
// / is returned if the collection specified by *collection* is unknown.  The
// / response body contains an error document in this case.
// / The same error code is also returned if an invalid index id or type is used.
// //////////////////////////////////////////////////////////////////////////////

// //////////////////////////////////////////////////////////////////////////////
// / @brief create a cursor response
// //////////////////////////////////////////////////////////////////////////////

function createCursorResponse (req, res, cursor) {
  actions.resultCursor(req, res, cursor, undefined, { countRequested: true });
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief was docuBlock JSA_put_api_simple_any
// //////////////////////////////////////////////////////////////////////////////

actions.defineHttp({
  url: API + 'any',

  callback: function (req, res) {
    try {
      var body = actions.getJsonBody(req, res);

      if (body === undefined) {
        return;
      }

      if (req.requestType !== actions.PUT) {
        actions.resultUnsupported(req, res);
      } else {
        var name = body.collection;
        var id = parseInt(name, 10) || name;
        var collection = db._collection(id);

        if (collection === null) {
          actions.collectionNotFound(req, res, name);
        } else {
          var result = { document: collection.any() };

          actions.resultOk(req, res, actions.HTTP_OK, result);
        }
      }
    } catch (err) {
      actions.resultException(req, res, err, undefined, false);
    }
  }
});

// //////////////////////////////////////////////////////////////////////////////
// / @brief was docuBlock JSA_put_api_simple_near
// //////////////////////////////////////////////////////////////////////////////

actions.defineHttp({
  url: API + 'near',

  callback: function (req, res) {
    try {
      var body = actions.getJsonBody(req, res);

      if (body === undefined) {
        return;
      }

      if (req.requestType !== actions.PUT) {
        actions.resultUnsupported(req, res);
      } else {
        var limit = body.limit;
        var skip = body.skip;
        var latitude = body.latitude;
        var longitude = body.longitude;
        var distance = body.distance;
        var name = body.collection;
        var geo = body.geo;

        var collection = db._collection(name);

        if (collection === null) {
          actions.collectionNotFound(req, res, name);
        } else if (latitude === null || latitude === undefined) {
          actions.badParameter(req, res, 'latitude');
        } else if (longitude === null || longitude === undefined) {
          actions.badParameter(req, res, 'longitude');
        } else {
          var result;

          if (geo === null || geo === undefined) {
            result = collection.near(latitude, longitude);
          } else {
            result = collection.geo({ id: geo }).near(latitude, longitude);
          }

          if (skip !== null && skip !== undefined) {
            result = result.skip(skip);
          }

          if (limit !== null && limit !== undefined) {
            result = result.limit(limit);
          }

          if (distance !== null && distance !== undefined) {
            result = result.distance(distance);
          }

          createCursorResponse(req, res, CREATE_CURSOR(result.toArray(), body.batchSize, body.ttl));
        }
      }
    } catch (err) {
      actions.resultException(req, res, err, undefined, false);
    }
  }
});

// //////////////////////////////////////////////////////////////////////////////
// / @brief was docuBlock JSA_put_api_simple_within
// //////////////////////////////////////////////////////////////////////////////

actions.defineHttp({
  url: API + 'within',

  callback: function (req, res) {
    try {
      var body = actions.getJsonBody(req, res);

      if (body === undefined) {
        return;
      }

      if (req.requestType !== actions.PUT) {
        actions.resultUnsupported(req, res);
      } else {
        var limit = body.limit;
        var skip = body.skip;
        var latitude = body.latitude;
        var longitude = body.longitude;
        var distance = body.distance;
        var radius = body.radius;
        var geo = body.geo;
        var name = body.collection;
        var collection = db._collection(name);

        if (collection === null) {
          actions.collectionNotFound(req, res, name);
        } else if (latitude === null || latitude === undefined) {
          actions.badParameter(req, res, 'latitude');
        } else if (longitude === null || longitude === undefined) {
          actions.badParameter(req, res, 'longitude');
        } else {
          var result;

          if (geo === null || geo === undefined) {
            result = collection.within(latitude, longitude, radius);
          } else {
            result = collection.geo({ id: geo }).within(latitude, longitude, radius);
          }

          if (skip !== null && skip !== undefined) {
            result = result.skip(skip);
          }

          if (limit !== null && limit !== undefined) {
            result = result.limit(limit);
          }

          if (distance !== null && distance !== undefined) {
            result = result.distance(distance);
          }

          createCursorResponse(req, res, CREATE_CURSOR(result.toArray(), body.batchSize, body.ttl));
        }
      }
    } catch (err) {
      actions.resultException(req, res, err, undefined, false);
    }
  }
});

// //////////////////////////////////////////////////////////////////////////////
// / @brief was docuBlock JSA_put_api_simple_within_rectangle
// //////////////////////////////////////////////////////////////////////////////

actions.defineHttp({
  url: API + 'within-rectangle',

  callback: function (req, res) {
    try {
      var body = actions.getJsonBody(req, res);

      if (body === undefined) {
        return;
      }

      if (req.requestType !== actions.PUT) {
        actions.resultUnsupported(req, res);
      } else {
        var limit = body.limit;
        var skip = body.skip;
        var latitude1 = body.latitude1;
        var longitude1 = body.longitude1;
        var latitude2 = body.latitude2;
        var longitude2 = body.longitude2;
        var geo = body.geo;
        var name = body.collection;
        var collection = db._collection(name);

        if (collection === null) {
          actions.collectionNotFound(req, res, name);
        } else if (latitude1 === null || latitude1 === undefined) {
          actions.badParameter(req, res, 'latitude1');
        } else if (longitude1 === null || longitude1 === undefined) {
          actions.badParameter(req, res, 'longitude1');
        } else if (latitude2 === null || latitude2 === undefined) {
          actions.badParameter(req, res, 'latitude2');
        } else if (longitude2 === null || longitude2 === undefined) {
          actions.badParameter(req, res, 'longitude2');
        } else {
          var result;

          if (geo === null || geo === undefined) {
            result = collection.withinRectangle(latitude1, longitude1, latitude2, longitude2);
          } else {
            result = collection.geo({ id: geo }).withinRectangle(latitude1, longitude1, latitude2, longitude2);
          }

          if (skip !== null && skip !== undefined) {
            result = result.skip(skip);
          }

          if (limit !== null && limit !== undefined) {
            result = result.limit(limit);
          }

          createCursorResponse(req, res, CREATE_CURSOR(result.toArray(), body.batchSize, body.ttl));
        }
      }
    } catch (err) {
      actions.resultException(req, res, err, undefined, false);
    }
  }
});

// //////////////////////////////////////////////////////////////////////////////
// / @brief was docuBlock JSA_put_api_simple_fulltext
// //////////////////////////////////////////////////////////////////////////////

actions.defineHttp({
  url: API + 'fulltext',

  callback: function (req, res) {
    try {
      var body = actions.getJsonBody(req, res);

      if (body === undefined) {
        return;
      }

      if (req.requestType !== actions.PUT) {
        actions.resultUnsupported(req, res);
      } else {
        var limit = body.limit;
        var skip = body.skip;
        var attribute = body.attribute;
        var query = body.query;
        var iid = body.index || undefined;
        var name = body.collection;
        var collection = db._collection(name);

        if (collection === null) {
          actions.collectionNotFound(req, res, name);
        } else if (attribute === null || attribute === undefined) {
          actions.badParameter(req, res, 'attribute');
        } else if (query === null || query === undefined) {
          actions.badParameter(req, res, 'query');
        } else {
          var result = collection.fulltext(attribute, query, iid);

          if (skip !== null && skip !== undefined) {
            result = result.skip(skip);
          }

          if (limit !== null && limit !== undefined) {
            result = result.limit(limit);
          }

          createCursorResponse(req, res, CREATE_CURSOR(result.toArray(), body.batchSize, body.ttl));
        }
      }
    } catch (err) {
      actions.resultException(req, res, err, undefined, false);
    }
  }
});

// //////////////////////////////////////////////////////////////////////////////
// / @brief was docuBlock JSA_put_api_simple_by_example
// //////////////////////////////////////////////////////////////////////////////

actions.defineHttp({
  url: API + 'by-example',

  callback: function (req, res) {
    try {
      var body = actions.getJsonBody(req, res);

      if (body === undefined) {
        return;
      }

      if (req.requestType !== actions.PUT) {
        actions.resultUnsupported(req, res);
      } else {
        var limit = body.limit;
        var skip = body.skip;
        var example = body.example;
        var name = body.collection;
        var collection = db._collection(name);

        if (collection === null) {
          actions.collectionNotFound(req, res, name);
        } else if (typeof example !== 'object') {
          actions.badParameter(req, res, 'example');
        } else {
          var result = collection.byExample(example);

          if (skip !== null && skip !== undefined) {
            result = result.skip(skip);
          }

          if (limit !== null && limit !== undefined) {
            result = result.limit(limit);
          }

          createCursorResponse(req, res, CREATE_CURSOR(result.toArray(), body.batchSize, body.ttl));
        }
      }
    } catch (err) {
      actions.resultException(req, res, err, undefined, false);
    }
  }
});

// //////////////////////////////////////////////////////////////////////////////
// / @brief was docuBlock JSA_put_api_simple_first_example
// //////////////////////////////////////////////////////////////////////////////

actions.defineHttp({
  url: API + 'first-example',

  callback: function (req, res) {
    try {
      var body = actions.getJsonBody(req, res);

      if (body === undefined) {
        return;
      }

      if (req.requestType !== actions.PUT) {
        actions.resultUnsupported(req, res);
      } else {
        var example = body.example;
        var name = body.collection;
        var collection = db._collection(name);

        if (collection === null) {
          actions.collectionNotFound(req, res, name);
        } else if (typeof example !== 'object') {
          actions.badParameter(req, res, 'example');
        } else {
          var result = collection.byExample(example).limit(1);

          if (result.hasNext()) {
            actions.resultOk(req, res, actions.HTTP_OK, { document: result.next() });
          } else {
            actions.resultNotFound(req, res, ERRORS.ERROR_HTTP_NOT_FOUND.code, 'no match');
          }
        }
      }
    } catch (err) {
      actions.resultException(req, res, err, undefined, false);
    }
  }
});

// //////////////////////////////////////////////////////////////////////////////
// / @brief was docuBlock JSA_put_api_simple_first
// //////////////////////////////////////////////////////////////////////////////

actions.defineHttp({
  url: API + 'first',

  callback: function (req, res) {
    try {
      if (req.requestType !== actions.PUT) {
        actions.resultUnsupported(req, res);
      } else {
        var body = actions.getJsonBody(req, res);
        var name = body.collection;
        var count = body.count;
        var collection = db._collection(name);

        if (collection === null) {
          actions.collectionNotFound(req, res, name);
        }

        if (count) {
          actions.resultOk(req, res, actions.HTTP_OK, { result: collection.first(count) });
        } else {
          actions.resultOk(req, res, actions.HTTP_OK, { result: collection.first() });
        }
      }
    } catch (err) {
      actions.resultException(req, res, err, undefined, false);
    }
  }
});

// //////////////////////////////////////////////////////////////////////////////
// / @brief was docuBlock JSA_put_api_simple_last
// //////////////////////////////////////////////////////////////////////////////

actions.defineHttp({
  url: API + 'last',

  callback: function (req, res) {
    try {
      if (req.requestType !== actions.PUT) {
        actions.resultUnsupported(req, res);
      } else {
        var body = actions.getJsonBody(req, res);
        var name = body.collection;
        var count = body.count;
        var collection = db._collection(name);

        if (collection === null) {
          actions.collectionNotFound(req, res, name);
        }

        if (count) {
          actions.resultOk(req, res, actions.HTTP_OK, { result: collection.last(count) });
        } else {
          actions.resultOk(req, res, actions.HTTP_OK, { result: collection.last() });
        }
      }
    } catch (err) {
      actions.resultException(req, res, err, undefined, false);
    }
  }
});

// //////////////////////////////////////////////////////////////////////////////
// / @brief was docuBlock JSA_put_api_simple_range
// //////////////////////////////////////////////////////////////////////////////

actions.defineHttp({
  url: API + 'range',

  callback: function (req, res) {
    try {
      var body = actions.getJsonBody(req, res);

      if (body === undefined) {
        return;
      }

      if (req.requestType !== actions.PUT) {
        actions.resultUnsupported(req, res);
      } else {
        var limit = body.limit;
        var skip = body.skip;
        var name = body.collection;
        var attribute = body.attribute;
        var left = body.left;
        var right = body.right;
        var closed = body.closed;
        var collection = db._collection(name);

        if (collection === null) {
          actions.collectionNotFound(req, res, name);
        } else {
          var result;

          if (closed) {
            result = collection.closedRange(attribute, left, right);
          } else {
            result = collection.range(attribute, left, right);
          }

          if (skip !== null && skip !== undefined) {
            result = result.skip(skip);
          }

          if (limit !== null && limit !== undefined) {
            result = result.limit(limit);
          }

          createCursorResponse(req, res, CREATE_CURSOR(result.toArray(), body.batchSize, body.ttl));
        }
      }
    } catch (err) {
      actions.resultException(req, res, err, undefined, false);
    }
  }
});

// //////////////////////////////////////////////////////////////////////////////
// / @brief was docuBlock JSA_put_api_simple_remove_by_example
// //////////////////////////////////////////////////////////////////////////////

actions.defineHttp({
  url: API + 'remove-by-example',

  callback: function (req, res) {
    try {
      var body = actions.getJsonBody(req, res);

      if (body === undefined) {
        return;
      }

      if (req.requestType !== actions.PUT) {
        actions.resultUnsupported(req, res);
      } else {
        var example = body.example;
        var name = body.collection;
        var collection = db._collection(name);

        if (collection === null) {
          actions.collectionNotFound(req, res, name);
        } else if (typeof example !== 'object' || Array.isArray(example)) {
          actions.badParameter(req, res, 'example');
        } else {
          var options = body.options;
          if (typeof options === 'undefined') {
            var waitForSync = body.waitForSync || undefined;
            var limit = body.limit || null;
            options = {waitForSync: waitForSync, limit: limit};
          }

          if (options.limit > 0 && !checkShards(req, collection)) {
            actions.resultError(req, res, actions.HTTP_NOT_IMPLEMENTED, ERRORS.ERROR_CLUSTER_UNSUPPORTED.code);
          } else {
            var result = collection.removeByExample(example, options);
            actions.resultOk(req, res, actions.HTTP_OK, { deleted: result });
          }
        }
      }
    } catch (err) {
      actions.resultException(req, res, err, undefined, false);
    }
  }
});

// //////////////////////////////////////////////////////////////////////////////
// / @brief was docuBlock JSA_put_api_simple_replace_by_example
// //////////////////////////////////////////////////////////////////////////////

actions.defineHttp({
  url: API + 'replace-by-example',

  callback: function (req, res) {
    try {
      var body = actions.getJsonBody(req, res);

      if (body === undefined) {
        return;
      }

      if (req.requestType !== actions.PUT) {
        actions.resultUnsupported(req, res);
      } else {
        var example = body.example;
        var newValue = body.newValue;
        var name = body.collection;
        var collection = db._collection(name);

        if (collection === null) {
          actions.collectionNotFound(req, res, name);
        } else if (typeof example !== 'object' || Array.isArray(example)) {
          actions.badParameter(req, res, 'example');
        } else if (typeof newValue !== 'object' || Array.isArray(newValue)) {
          actions.badParameter(req, res, 'newValue');
        } else {
          var options = body.options;
          if (typeof options === 'undefined') {
            var waitForSync = body.waitForSync || undefined;
            var limit = body.limit || null;
            options = {waitForSync: waitForSync, limit: limit};
          }

          if (options.limit > 0 && !checkShards(req, collection)) {
            actions.resultError(req, res, actions.HTTP_NOT_IMPLEMENTED, ERRORS.ERROR_CLUSTER_UNSUPPORTED.code);
          } else {
            var result = collection.replaceByExample(example, newValue, options);
            actions.resultOk(req, res, actions.HTTP_OK, { replaced: result });
          }
        }
      }
    } catch (err) {
      actions.resultException(req, res, err, undefined, false);
    }
  }
});

// //////////////////////////////////////////////////////////////////////////////
// / @brief was docuBlock JSA_put_api_simple_update_by_example
// //////////////////////////////////////////////////////////////////////////////

actions.defineHttp({
  url: API + 'update-by-example',

  callback: function (req, res) {
    try {
      var body = actions.getJsonBody(req, res);

      if (body === undefined) {
        return;
      }

      if (req.requestType !== actions.PUT) {
        actions.resultUnsupported(req, res);
      } else {
        var example = body.example;
        var newValue = body.newValue;
        var name = body.collection;
        var collection = db._collection(name);
        var limit = body.limit || null;
        var keepNull = body.keepNull || null;
        var mergeObjects = true;
        if (body.hasOwnProperty('mergeObjects')) {
          mergeObjects = body.mergeObjects || false;
        }

        if (collection === null) {
          actions.collectionNotFound(req, res, name);
        } else if (typeof example !== 'object' || Array.isArray(example)) {
          actions.badParameter(req, res, 'example');
        } else if (typeof newValue !== 'object' || Array.isArray(newValue)) {
          actions.badParameter(req, res, 'newValue');
        } else {
          var options = body.options;
          if (typeof options === 'undefined') {
            var waitForSync = body.waitForSync || undefined;
            limit = body.limit || undefined;
            options = {waitForSync: waitForSync, keepNull: keepNull, limit: limit, mergeObjects: mergeObjects};
          }

          if (options.limit > 0 && !checkShards(req, collection)) {
            actions.resultError(req, res, actions.HTTP_NOT_IMPLEMENTED, ERRORS.ERROR_CLUSTER_UNSUPPORTED.code);
          } else {
            var result = collection.updateByExample(example, newValue, options);
            actions.resultOk(req, res, actions.HTTP_OK, { updated: result });
          }
        }
      }
    } catch (err) {
      actions.resultException(req, res, err, undefined, false);
    }
  }
});
