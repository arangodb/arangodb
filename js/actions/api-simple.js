/*jshint strict: false */
/*global require, CREATE_CURSOR */

////////////////////////////////////////////////////////////////////////////////
/// @brief simple queries
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2014 ArangoDB GmbH, Cologne, Germany
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
/// @author Achim Brandt
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var actions = require("org/arangodb/actions");
var db = require("org/arangodb").db;
var ERRORS = require("internal").errors;

var API = "_api/simple/";

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief returns all documents of a collection matching a given example,
/// using a specific hash index
///
/// RESTHEADER{PUT /_api/simple/by-example-hash, Hash index}
///
/// **Note**: This is only used internally and should not be accesible by the user.
///
/// RESTBODYPARAM{query,string,required}
/// Contains the query specification.
///
/// RESTDESCRIPTION
///
/// This will find all documents matching a given example, using the specified
/// hash index.
///
/// The call expects a JSON object as body with the following attributes:
///
/// - *collection*: The name of the collection to query.
///
/// - *index*: The id of the index to be used for the query. The index must exist
///   and must be of type *hash*.
///
/// - *example*: an example document. The example must contain a value for each
///   attribute in the index.
///
/// - *skip*: The number of documents to skip in the query. (optional)
///
/// - *limit*: The maximal number of documents to return. (optional)
///
/// Returns a cursor containing the result, see [Http Cursor](../HttpAqlQueryCursor/README.md) for details.
///
/// RESTRETURNCODES
///
/// RESTRETURNCODE{201}
/// is returned if the query was executed successfully.
///
/// RESTRETURNCODE{400}
/// is returned if the body does not contain a valid JSON representation of a
/// query. The response body contains an error document in this case.
///
/// RESTRETURNCODE{404}
/// is returned if the collection specified by *collection* is unknown.  The
/// response body contains an error document in this case.
/// The same error code is also returned if an invalid index id or type is used.
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSA_put_api_simple_by_example_skiplist
/// @brief returns all documents of a collection matching a given example,
/// using a specific skiplist index
///
/// @RESTHEADER{PUT /_api/simple/by-example-skiplist, Skiplist index}
///
/// **Note**: This is only used internally and should not be accesible by the user.
///
/// @RESTBODYPARAM{query,string,required}
/// Contains the query specification.
///
/// @RESTDESCRIPTION
///
/// This will find all documents matching a given example, using the specified
/// skiplist index.
///
/// The call expects a JSON object as body with the following attributes:
///
/// - *collection*: The name of the collection to query.
///
/// - *index*: The id of the index to be used for the query. The index must
///   exist and must be of type *skiplist*.
///
/// - *example*: an example document. The example must contain a value for each
///   attribute in the index.
///
/// - *skip*: The number of documents to skip in the query. (optional)
///
/// - *limit*: The maximal number of documents to return. (optional)
///
/// Returns a cursor containing the result, see [Http Cursor](../HttpAqlQueryCursor/README.md) for details.
///
/// @RESTRETURNCODES
///
/// @RESTRETURNCODE{201}
/// is returned if the query was executed successfully.
///
/// @RESTRETURNCODE{400}
/// is returned if the body does not contain a valid JSON representation of a
/// query. The response body contains an error document in this case.
///
/// @RESTRETURNCODE{404}
/// is returned if the collection specified by *collection* is unknown.  The
/// response body contains an error document in this case.
/// The same error code is also returned if an invalid index id or type is used.
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSA_put_api_simple_by_condition_skiplist
/// @brief returns all documents of a collection matching a given condition,
/// using a specific skiplist index
///
/// @RESTHEADER{PUT /_api/simple/by-condition-skiplist,Query by-condition using Skiplist index}
///
/// @RESTBODYPARAM{query,string,required}
/// Contains the query specification.
///
/// @RESTDESCRIPTION
///
/// This will find all documents matching a given condition, using the specified
/// skiplist index.
///
/// The call expects a JSON object as body with the following attributes:
///
/// - *collection*: The name of the collection to query.
///
/// - *index*: The id of the index to be used for the query. The index must
///   exist and must be of type *skiplist*.
///
/// - *condition*: the condition which all returned documents shall satisfy.
///   Conditions must be specified for all indexed attributes.
///
/// - *skip*: The number of documents to skip in the query. (optional)
///
/// - *limit*: The maximal number of documents to return. (optional)
///
/// Returns a cursor containing the result, see [Http Cursor](../HttpAqlQueryCursor/README.md) for details.
///
/// @RESTRETURNCODES
///
/// @RESTRETURNCODE{201}
/// is returned if the query was executed successfully.
///
/// @RESTRETURNCODE{400}
/// is returned if the body does not contain a valid JSON representation of a
/// query. The response body contains an error document in this case.
///
/// @RESTRETURNCODE{404}
/// is returned if the collection specified by *collection* is unknown.  The
/// response body contains an error document in this case.
/// The same error code is also returned if an invalid index id or type is used.
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief create a cursor response
////////////////////////////////////////////////////////////////////////////////

function createCursorResponse (req, res, cursor) {
  actions.resultCursor(req, res, cursor, undefined, { countRequested: true });
}

////////////////////////////////////////////////////////////////////////////////
/// @brief setup an index query
////////////////////////////////////////////////////////////////////////////////

function setupIndexQuery (name, func, isExampleQuery) {
  actions.defineHttp({
    url: API + name,

    callback: function (req, res) {
      try {
        var body = actions.getJsonBody(req, res);

        if (body === undefined) {
          return;
        }

        if (req.requestType !== actions.PUT) {
          actions.resultUnsupported(req, res);
        }
        else {
          var limit = body.limit;
          var skip = body.skip || 0;
          var name = body.collection;
          var index = body.index;
          var collection = db._collection(name);

          if (collection === null) {
            actions.collectionNotFound(req, res, name);
            return;
          }

          var result;

          if (isExampleQuery) {
            if (typeof body.example !== "object" || Array.isArray(body.example)) {
              actions.badParameter(req, res, "example");
              return;
            }

            result = collection[func](index, body.example);
          }
          else {
            if (typeof body.condition !== "object" || Array.isArray(body.condition)) {
              actions.badParameter(req, res, "condition");
              return;
            }

            result = collection[func](index, body.condition);
          }

          if (skip > 0) {
            result.skip(skip);
          }
          if (limit !== undefined && limit !== null) {
            result.limit(limit);
          }

          createCursorResponse(req, res, CREATE_CURSOR(result.toArray(), true, body.batchSize, body.ttl));
        }
      }
      catch (err) {
        actions.resultException(req, res, err, undefined, false);
      }
    }
  });
}

////////////////////////////////////////////////////////////////////////////////
/// @brief register all these functions
////////////////////////////////////////////////////////////////////////////////

function setupIndexQueries () {
  var map = {
    "by-example-hash"       : [ "byExampleHash", true ],
    "by-example-skiplist"   : [ "byExampleSkiplist", true ],
    "by-condition-skiplist" : [ "byConditionSkiplist", false ]
  };

  var m;
  for (m in map) {
    if (map.hasOwnProperty(m)) {
      setupIndexQuery(m, map[m][0], map[m][1]);
    }
  }
}

setupIndexQueries();

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSA_put_api_simple_all
/// @brief returns all documents of a collection
///
/// @RESTHEADER{PUT /_api/simple/all, Return all}
///
/// @RESTBODYPARAM{query,string,required}
/// Contains the query.
///
/// @RESTDESCRIPTION
///
/// Returns all documents of a collections. The call expects a JSON object
/// as body with the following attributes:
///
/// - *collection*: The name of the collection to query.
///
/// - *skip*: The number of documents to skip in the query (optional).
///
/// - *limit*: The maximal amount of documents to return. The *skip*
///   is applied before the *limit* restriction. (optional)
///
/// Returns a cursor containing the result, see [Http Cursor](../HttpAqlQueryCursor/README.md) for details.
///
/// @RESTRETURNCODES
///
/// @RESTRETURNCODE{201}
/// is returned if the query was executed successfully.
///
/// @RESTRETURNCODE{400}
/// is returned if the body does not contain a valid JSON representation of a
/// query. The response body contains an error document in this case.
///
/// @RESTRETURNCODE{404}
/// is returned if the collection specified by *collection* is unknown.  The
/// response body contains an error document in this case.
///
/// @EXAMPLES
///
/// Limit the amount of documents using *limit*
///
/// @EXAMPLE_ARANGOSH_RUN{RestSimpleAllSkipLimit}
///     var cn = "products";
///     db._drop(cn);
///     var collection = db._create(cn, { waitForSync: true });
///     collection.save({"Hello1" : "World1" });
///     collection.save({"Hello2" : "World2" });
///     collection.save({"Hello3" : "World3" });
///     collection.save({"Hello4" : "World4" });
///     collection.save({"Hello5" : "World5" });
///
///     var url = "/_api/simple/all";
///     var body = '{ "collection": "products", "skip": 2, "limit" : 2 }';
///
///     var response = logCurlRequest('PUT', url, body);
///
///     assert(response.code === 201);
///
///     logJsonResponse(response);
///     db._drop(cn);
/// @END_EXAMPLE_ARANGOSH_RUN
///
/// Using a *batchSize* value
///
/// @EXAMPLE_ARANGOSH_RUN{RestSimpleAllBatch}
///     var cn = "products";
///     db._drop(cn);
///     var collection = db._create(cn);
///     collection.save({"Hello1" : "World1" });
///     collection.save({"Hello2" : "World2" });
///     collection.save({"Hello3" : "World3" });
///     collection.save({"Hello4" : "World4" });
///     collection.save({"Hello5" : "World5" });
///
///     var url = "/_api/simple/all";
///     var body = '{ "collection": "products", "batchSize" : 3 }';
///
///     var response = logCurlRequest('PUT', url, body);
///
///     assert(response.code === 201);
///
///     logJsonResponse(response);
///     db._drop(cn);
/// @END_EXAMPLE_ARANGOSH_RUN
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

actions.defineHttp({
  url: API + "all",

  callback : function (req, res) {
    try {
      var body = actions.getJsonBody(req, res);

      if (body === undefined) {
        return;
      }

      if (req.requestType !== actions.PUT) {
        actions.resultUnsupported(req, res);
      }
      else {
        var limit = body.limit;
        var skip = body.skip;
        var name = body.collection;
        var collection = db._collection(name);

        if (collection === null) {
          actions.collectionNotFound(req, res, name);
        }
        else {
          var result = collection.all();

          if (skip !== null && skip !== undefined) {
            result = result.skip(skip);
          }

          if (limit !== null && limit !== undefined) {
            result = result.limit(limit);
          }

          createCursorResponse(req, res, CREATE_CURSOR(result.toArray(), true, body.batchSize, body.ttl));
        }
      }
    }
    catch (err) {
      actions.resultException(req, res, err, undefined, false);
    }
  }
});

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSA_put_api_simple_any
/// @brief returns a random document from a collection
///
/// @RESTHEADER{PUT /_api/simple/any, Random document}
///
/// @RESTBODYPARAM{query,string,required}
/// Contains the query.
///
/// @RESTDESCRIPTION
///
/// Returns a random document from a collection. The call expects a JSON object
/// as body with the following attributes:
///
/// - *collection*: The identifier or name of the collection to query.
///
/// Returns a JSON object with the document stored in the attribute
/// *document* if the collection contains at least one document. If
/// the collection is empty, the *document* attrbute contains null.
///
/// @RESTRETURNCODES
///
/// @RESTRETURNCODE{200}
/// is returned if the query was executed successfully.
///
/// @RESTRETURNCODE{400}
/// is returned if the body does not contain a valid JSON representation of a
/// query. The response body contains an error document in this case.
///
/// @RESTRETURNCODE{404}
/// is returned if the collection specified by *collection* is unknown.  The
/// response body contains an error document in this case.
///
/// @EXAMPLES
///
/// @EXAMPLE_ARANGOSH_RUN{RestSimpleAny}
///     var cn = "products";
///     db._drop(cn);
///     var collection = db._create(cn);
///     collection.save({"Hello1" : "World1" });
///     collection.save({"Hello2" : "World2" });
///     collection.save({"Hello3" : "World3" });
///     collection.save({"Hello4" : "World4" });
///     collection.save({"Hello5" : "World5" });
///
///     var url = "/_api/simple/any";
///     var body = '{ "collection": "products" }';
///
///     var response = logCurlRequest('PUT', url, body);
///
///     assert(response.code === 200);
///
///     logJsonResponse(response);
///     db._drop(cn);
/// @END_EXAMPLE_ARANGOSH_RUN
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

actions.defineHttp({
  url: API + "any",

  callback: function (req, res) {
    try {
      var body = actions.getJsonBody(req, res);

      if (body === undefined) {
        return;
      }

      if (req.requestType !== actions.PUT) {
        actions.resultUnsupported(req, res);
      }
      else {
        var name = body.collection;
        var id = parseInt(name, 10) || name;
        var collection = db._collection(id);

        if (collection === null) {
          actions.collectionNotFound(req, res, name);
        }
        else {
          var result = { document: collection.any() };

          actions.resultOk(req, res, actions.HTTP_OK, result);
        }
      }
    }
    catch (err) {
      actions.resultException(req, res, err, undefined, false);
    }
  }
});

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSA_put_api_simple_near
/// @brief returns all documents of a collection near a given location
///
/// @RESTHEADER{PUT /_api/simple/near, Near query}
///
/// @RESTBODYPARAM{query,string,required}
/// Contains the query.
///
/// @RESTDESCRIPTION
///
/// The default will find at most 100 documents near the given coordinate.  The
/// returned list is sorted according to the distance, with the nearest document
/// being first in the list. If there are near documents of equal distance, documents
/// are chosen randomly from this set until the limit is reached.
///
/// In order to use the *near* operator, a geo index must be defined for the
/// collection. This index also defines which attribute holds the coordinates
/// for the document.  If you have more then one geo-spatial index, you can use
/// the *geo* field to select a particular index.
///
/// The call expects a JSON object as body with the following attributes:
///
/// - *collection*: The name of the collection to query.
///
/// - *latitude*: The latitude of the coordinate.
///
/// - *longitude*: The longitude of the coordinate.
///
/// - *distance*: If given, the attribute key used to return the distance to
///   the given coordinate. (optional). If specified, distances are returned in meters.
///
/// - *skip*: The number of documents to skip in the query. (optional)
///
/// - *limit*: The maximal amount of documents to return. The *skip* is
///   applied before the *limit* restriction. The default is 100. (optional)
///
/// - *geo*: If given, the identifier of the geo-index to use. (optional)
///
/// Returns a cursor containing the result, see [Http Cursor](../HttpAqlQueryCursor/README.md) for details.
///
/// @RESTRETURNCODES
///
/// @RESTRETURNCODE{201}
/// is returned if the query was executed successfully.
///
/// @RESTRETURNCODE{400}
/// is returned if the body does not contain a valid JSON representation of a
/// query. The response body contains an error document in this case.
///
/// @RESTRETURNCODE{404}
/// is returned if the collection specified by *collection* is unknown.  The
/// response body contains an error document in this case.
///
/// @EXAMPLES
///
/// Without distance:
///
/// @EXAMPLE_ARANGOSH_RUN{RestSimpleNear}
///     var cn = "products";
///     db._drop(cn);
///     var products = db._create(cn, { waitForSync: true });
///     var loc = products.ensureGeoIndex("loc");
///     var i;
///     for (i = -0.01;  i <= 0.01;  i += 0.002) {
///       products.save({ name : "Name/" + i + "/",loc: [ i, 0 ] });
///     }
///     var url = "/_api/simple/near";
///     var body = '{ ' +
///       '"collection": "products", ' +
///       '"latitude" : 0, ' +
///       '"longitude" : 0, ' +
///       '"skip" : 1, ' +
///       '"limit" : 2 ' +
///     '}';
///
///     var response = logCurlRequest('PUT', url, body);
///
///     assert(response.code === 201);
///
///     logJsonResponse(response);
///     db._drop(cn);
/// @END_EXAMPLE_ARANGOSH_RUN
///
/// With distance:
///
/// @EXAMPLE_ARANGOSH_RUN{RestSimpleNearDistance}
///     var cn = "products";
///     db._drop(cn);
///     var products = db._create(cn, { waitForSync: true });
///     var loc = products.ensureGeoIndex("loc");
///     var i;
///     for (i = -0.01;  i <= 0.01;  i += 0.002) {
///       products.save({ name : "Name/" + i + "/",loc: [ i, 0 ] });
///     }
///     var url = "/_api/simple/near";
///     var body = '{ ' +
///       '"collection": "products", ' +
///       '"latitude" : 0, ' +
///       '"longitude" : 0, ' +
///       '"skip" : 1, ' +
///       '"limit" : 3, ' +
///       '"distance" : "distance" ' +
///     '}';
///
///     var response = logCurlRequest('PUT', url, body);
///
///     assert(response.code === 201);
///
///     logJsonResponse(response);
///     db._drop(cn);
/// @END_EXAMPLE_ARANGOSH_RUN
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

actions.defineHttp({
  url: API + "near",

  callback : function (req, res) {
    try {
      var body = actions.getJsonBody(req, res);

      if (body === undefined) {
        return;
      }

      if (req.requestType !== actions.PUT) {
        actions.resultUnsupported(req, res);
      }
      else {
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
        }
        else if (latitude === null || latitude === undefined) {
          actions.badParameter(req, res, "latitude");
        }
        else if (longitude === null || longitude === undefined) {
          actions.badParameter(req, res, "longitude");
        }
        else {
          var result;

          if (geo === null || geo === undefined) {
            result = collection.near(latitude, longitude);
          }
          else {
            result = collection.geo({ id : geo }).near(latitude, longitude);
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

          createCursorResponse(req, res, CREATE_CURSOR(result.toArray(), true, body.batchSize, body.ttl));
        }
      }
    }
    catch (err) {
      actions.resultException(req, res, err, undefined, false);
    }
  }
});

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSA_put_api_simple_within
/// @brief returns all documents of a collection within a given radius
///
/// @RESTHEADER{PUT /_api/simple/within, Within query}
///
/// @RESTBODYPARAM{query,string,required}
/// Contains the query.
///
/// @RESTDESCRIPTION
///
/// This will find all documents within a given radius around the coordinate
/// (*latitude*, *longitude*). The returned list is sorted by distance.
///
/// In order to use the *within* operator, a geo index must be defined for
/// the collection. This index also defines which attribute holds the
/// coordinates for the document.  If you have more then one geo-spatial index,
/// you can use the *geo* field to select a particular index.
///
/// The call expects a JSON object as body with the following attributes:
///
/// - *collection*: The name of the collection to query.
///
/// - *latitude*: The latitude of the coordinate.
///
/// - *longitude*: The longitude of the coordinate.
///
/// - *radius*: The maximal radius (in meters).
///
/// - *distance*: If given, the attribute key used to return the distance to
///   the given coordinate. (optional). If specified, distances are returned in meters.
///
/// - *skip*: The number of documents to skip in the query. (optional)
///
/// - *limit*: The maximal amount of documents to return. The *skip* is
///   applied before the *limit* restriction. The default is 100. (optional)
///
/// - *geo*: If given, the identifier of the geo-index to use. (optional)
///
/// Returns a cursor containing the result, see [Http Cursor](../HttpAqlQueryCursor/README.md) for details.
///
/// @RESTRETURNCODES
///
/// @RESTRETURNCODE{201}
/// is returned if the query was executed successfully.
///
/// @RESTRETURNCODE{400}
/// is returned if the body does not contain a valid JSON representation of a
/// query. The response body contains an error document in this case.
///
/// @RESTRETURNCODE{404}
/// is returned if the collection specified by *collection* is unknown.  The
/// response body contains an error document in this case.
///
/// @EXAMPLES
///
/// Without distance:
///
/// @EXAMPLE_ARANGOSH_RUN{RestSimpleWithin}
///     var cn = "products";
///     db._drop(cn);
///     var products = db._create(cn, { waitForSync: true });
///     var loc = products.ensureGeoIndex("loc");
///     var i;
///     for (i = -0.01;  i <= 0.01;  i += 0.002) {
///       products.save({ name : "Name/" + i + "/",loc: [ i, 0 ] });
///     }
///     var url = "/_api/simple/near";
///     var body = '{ ' +
///       '"collection": "products", ' +
///       '"latitude" : 0, ' +
///       '"longitude" : 0, ' +
///       '"skip" : 1, ' +
///       '"limit" : 2, ' +
///       '"radius" : 500 ' +
///     '}';
///
///     var response = logCurlRequest('PUT', url, body);
///
///     assert(response.code === 201);
///
///     logJsonResponse(response);
///     db._drop(cn);
/// @END_EXAMPLE_ARANGOSH_RUN
///
/// With distance:
///
/// @EXAMPLE_ARANGOSH_RUN{RestSimpleWithinDistance}
///     var cn = "products";
///     db._drop(cn);
///     var products = db._create(cn, { waitForSync: true });
///     var loc = products.ensureGeoIndex("loc");
///     var i;
///     for (i = -0.01;  i <= 0.01;  i += 0.002) {
///       products.save({ name : "Name/" + i + "/",loc: [ i, 0 ] });
///     }
///     var url = "/_api/simple/near";
///     var body = '{ "collection": "products", "latitude" : 0, "longitude" : 0, ' +
///                '"skip" : 1, "limit" : 3, "distance" : "distance", "radius" : 300 }';
///
///     var response = logCurlRequest('PUT', url, body);
///
///     assert(response.code === 201);
///
///     logJsonResponse(response);
///     db._drop(cn);
/// @END_EXAMPLE_ARANGOSH_RUN
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

actions.defineHttp({
  url: API + "within",

  callback : function (req, res) {
    try {
      var body = actions.getJsonBody(req, res);

      if (body === undefined) {
        return;
      }

      if (req.requestType !== actions.PUT) {
        actions.resultUnsupported(req, res);
      }
      else {
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
        }
        else if (latitude === null || latitude === undefined) {
          actions.badParameter(req, res, "latitude");
        }
        else if (longitude === null || longitude === undefined) {
          actions.badParameter(req, res, "longitude");
        }
        else {
          var result;

          if (geo === null || geo === undefined) {
            result = collection.within(latitude, longitude, radius);
          }
          else {
            result = collection.geo({ id : geo }).within(latitude, longitude, radius);
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

          createCursorResponse(req, res, CREATE_CURSOR(result.toArray(), true, body.batchSize, body.ttl));
        }
      }
    }
    catch (err) {
      actions.resultException(req, res, err, undefined, false);
    }
  }
});

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSA_put_api_simple_fulltext
/// @brief returns documents of a collection as a result of a fulltext query
///
/// @RESTHEADER{PUT /_api/simple/fulltext, Fulltext index query}
///
/// @RESTBODYPARAM{query,string,required}
/// Contains the query.
///
/// @RESTDESCRIPTION
///
/// This will find all documents from the collection that match the fulltext
/// query specified in *query*.
///
/// In order to use the *fulltext* operator, a fulltext index must be defined
/// for the collection and the specified attribute.
///
/// The call expects a JSON object as body with the following attributes:
///
/// - *collection*: The name of the collection to query.
///
/// - *attribute*: The attribute that contains the texts.
///
/// - *query*: The fulltext query.
///
/// - *skip*: The number of documents to skip in the query (optional).
///
/// - *limit*: The maximal amount of documents to return. The *skip*
///   is applied before the *limit* restriction. (optional)
///
/// - *index*: The identifier of the fulltext-index to use.
///
/// Returns a cursor containing the result, see [Http Cursor](../HttpAqlQueryCursor/README.md) for details.
///
/// @RESTRETURNCODES
///
/// @RESTRETURNCODE{201}
/// is returned if the query was executed successfully.
///
/// @RESTRETURNCODE{400}
/// is returned if the body does not contain a valid JSON representation of a
/// query. The response body contains an error document in this case.
///
/// @RESTRETURNCODE{404}
/// is returned if the collection specified by *collection* is unknown.  The
/// response body contains an error document in this case.
///
/// @EXAMPLES
///
/// @EXAMPLE_ARANGOSH_RUN{RestSimpleFulltext}
///     var cn = "products";
///     db._drop(cn);
///     var products = db._create(cn, { waitForSync: true });
///     products.save({"text" : "this text contains word" });
///     products.save({"text" : "this text also has a word" });
///     products.save({"text" : "this is nothing" });
///     var loc = products.ensureFulltextIndex("text");
///     var url = "/_api/simple/fulltext";
///     var body = '{ "collection": "products", "attribute" : "text", "query" : "word" }';
///
///     var response = logCurlRequest('PUT', url, body);
///
///     assert(response.code === 201);
///
///     logJsonResponse(response);
///     db._drop(cn);
/// @END_EXAMPLE_ARANGOSH_RUN
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

actions.defineHttp({
  url: API + "fulltext",

  callback : function (req, res) {
    try {
      var body = actions.getJsonBody(req, res);

      if (body === undefined) {
        return;
      }

      if (req.requestType !== actions.PUT) {
        actions.resultUnsupported(req, res);
      }
      else {
        var limit = body.limit;
        var skip = body.skip;
        var attribute = body.attribute;
        var query = body.query;
        var iid = body.index || undefined;
        var name = body.collection;
        var collection = db._collection(name);

        if (collection === null) {
          actions.collectionNotFound(req, res, name);
        }
        else if (attribute === null || attribute === undefined) {
          actions.badParameter(req, res, "attribute");
        }
        else if (query === null || query === undefined) {
          actions.badParameter(req, res, "query");
        }
        else {
          var result = collection.fulltext(attribute, query, iid);

          if (skip !== null && skip !== undefined) {
            result = result.skip(skip);
          }

          if (limit !== null && limit !== undefined) {
            result = result.limit(limit);
          }

          createCursorResponse(req, res, CREATE_CURSOR(result.toArray(), true, body.batchSize, body.ttl));
        }
      }
    }
    catch (err) {
      actions.resultException(req, res, err, undefined, false);
    }
  }
});

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSA_put_api_simple_by_example
/// @brief returns all documents of a collection matching a given example
///
/// @RESTHEADER{PUT /_api/simple/by-example, Simple query by-example}
///
/// @RESTBODYPARAM{query,string,required}
/// Contains the query.
///
/// @RESTDESCRIPTION
///
/// This will find all documents matching a given example.
///
/// The call expects a JSON object as body with the following attributes:
///
/// - *collection*: The name of the collection to query.
///
/// - *example*: The example document.
///
/// - *skip*: The number of documents to skip in the query (optional).
///
/// - *limit*: The maximal amount of documents to return. The *skip*
///   is applied before the *limit* restriction. (optional)
///
/// Returns a cursor containing the result, see [Http Cursor](../HttpAqlQueryCursor/README.md) for details.
///
/// @RESTRETURNCODES
///
/// @RESTRETURNCODE{201}
/// is returned if the query was executed successfully.
///
/// @RESTRETURNCODE{400}
/// is returned if the body does not contain a valid JSON representation of a
/// query. The response body contains an error document in this case.
///
/// @RESTRETURNCODE{404}
/// is returned if the collection specified by *collection* is unknown.  The
/// response body contains an error document in this case.
///
/// @EXAMPLES
///
/// Matching an attribute:
///
/// @EXAMPLE_ARANGOSH_RUN{RestSimpleByExample}
///     var cn = "products";
///     db._drop(cn);
///     var products = db._create(cn, { waitForSync: true });
///     products.save({ "a": { "k": 1, "j": 1 }, "i": 1});
///     products.save({ "a": { "j": 1 }, "i": 1});
///     products.save({ "i": 1});
///     products.save({ "a": { "k": 2, "j": 2 }, "i": 1});
///     var url = "/_api/simple/by-example";
///     var body = '{ "collection": "products", "example" :  { "i" : 1 }  }';
///
///     var response = logCurlRequest('PUT', url, body);
///
///     assert(response.code === 201);
///
///     logJsonResponse(response);
///     db._drop(cn);
/// @END_EXAMPLE_ARANGOSH_RUN
///
/// Matching an attribute which is a sub-document:
///
/// @EXAMPLE_ARANGOSH_RUN{RestSimpleByExample2}
///     var cn = "products";
///     db._drop(cn);
///     var products = db._create(cn, { waitForSync: true });
///     products.save({ "a": { "k": 1, "j": 1 }, "i": 1});
///     products.save({ "a": { "j": 1 }, "i": 1});
///     products.save({ "i": 1});
///     products.save({ "a": { "k": 2, "j": 2 }, "i": 1});
///     var url = "/_api/simple/by-example";
///     var body = '{ "collection": "products", "example" : { "a.j" : 1 } }';
///
///     var response = logCurlRequest('PUT', url, body);
///
///     assert(response.code === 201);
///
///     logJsonResponse(response);
///     db._drop(cn);
/// @END_EXAMPLE_ARANGOSH_RUN
///
/// Matching an attribute within a sub-document:
///
/// @EXAMPLE_ARANGOSH_RUN{RestSimpleByExample3}
///     var cn = "products";
///     db._drop(cn);
///     var products = db._create(cn, { waitForSync: true });
///     products.save({ "a": { "k": 1, "j": 1 }, "i": 1});
///     products.save({ "a": { "j": 1 }, "i": 1});
///     products.save({ "i": 1});
///     products.save({ "a": { "k": 2, "j": 2 }, "i": 1});
///     var url = "/_api/simple/by-example";
///     var body = '{ "collection": "products", "example" : { "a" : { "j" : 1 } } }';
///
///     var response = logCurlRequest('PUT', url, body);
///
///     assert(response.code === 201);
///
///     logJsonResponse(response);
///     db._drop(cn);
/// @END_EXAMPLE_ARANGOSH_RUN
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

actions.defineHttp({
  url: API + "by-example",

  callback : function (req, res) {
    try {
      var body = actions.getJsonBody(req, res);

      if (body === undefined) {
        return;
      }

      if (req.requestType !== actions.PUT) {
        actions.resultUnsupported(req, res);
      }
      else {
        var limit = body.limit;
        var skip = body.skip;
        var example = body.example;
        var name = body.collection;
        var collection = db._collection(name);

        if (collection === null) {
          actions.collectionNotFound(req, res, name);
        }
        else if (typeof example !== "object") {
          actions.badParameter(req, res, "example");
        }
        else {
          var result = collection.byExample(example);

          if (skip !== null && skip !== undefined) {
            result = result.skip(skip);
          }

          if (limit !== null && limit !== undefined) {
            result = result.limit(limit);
          }

          createCursorResponse(req, res, CREATE_CURSOR(result.toArray(), true, body.batchSize, body.ttl));
        }
      }
    }
    catch (err) {
      actions.resultException(req, res, err, undefined, false);
    }
  }
});

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSA_put_api_simple_first_example
/// @brief returns one document of a collection matching a given example
///
/// @RESTHEADER{PUT /_api/simple/first-example, Document matching an example}
///
/// @RESTBODYPARAM{query,json,required}
/// Contains the query.
///
/// @RESTDESCRIPTION
///
/// This will return the first document matching a given example.
///
/// The call expects a JSON object as body with the following attributes:
///
/// - *collection*: The name of the collection to query.
///
/// - *example*: The example document.
///
/// Returns a result containing the document or *HTTP 404* if no
/// document matched the example.
///
/// If more than one document in the collection matches the specified example, only
/// one of these documents will be returned, and it is undefined which of the matching
/// documents is returned.
///
/// @RESTRETURNCODES
///
/// @RESTRETURNCODE{200}
/// is returned when the query was successfully executed.
///
/// @RESTRETURNCODE{400}
/// is returned if the body does not contain a valid JSON representation of a
/// query. The response body contains an error document in this case.
///
/// @RESTRETURNCODE{404}
/// is returned if the collection specified by *collection* is unknown.  The
/// response body contains an error document in this case.
///
/// @EXAMPLES
///
/// If a matching document was found:
///
/// @EXAMPLE_ARANGOSH_RUN{RestSimpleFirstExample}
///     var cn = "products";
///     db._drop(cn);
///     var products = db._create(cn, { waitForSync: true });
///     products.save({ "a": { "k": 1, "j": 1 }, "i": 1});
///     products.save({ "a": { "j": 1 }, "i": 1});
///     products.save({ "i": 1});
///     products.save({ "a": { "k": 2, "j": 2 }, "i": 1});
///     var url = "/_api/simple/first-example";
///     var body = '{ "collection": "products", "example" :  { "i" : 1 }  }';
///
///     var response = logCurlRequest('PUT', url, body);
///
///     assert(response.code === 200);
///
///     logJsonResponse(response);
///     db._drop(cn);
/// @END_EXAMPLE_ARANGOSH_RUN
///
/// If no document was found:
///
/// @EXAMPLE_ARANGOSH_RUN{RestSimpleFirstExampleNotFound}
///     var cn = "products";
///     db._drop(cn);
///     var products = db._create(cn, { waitForSync: true });
///     products.save({ "a": { "k": 1, "j": 1 }, "i": 1});
///     products.save({ "a": { "j": 1 }, "i": 1});
///     products.save({ "i": 1});
///     products.save({ "a": { "k": 2, "j": 2 }, "i": 1});
///     var url = "/_api/simple/first-example";
///     var body = '{ "collection": "products", "example" :  { "l" : 1 }  }';
///
///     var response = logCurlRequest('PUT', url, body);
///
///     assert(response.code === 404);
///
///     logJsonResponse(response);
///     db._drop(cn);
/// @END_EXAMPLE_ARANGOSH_RUN
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

actions.defineHttp({
  url: API + "first-example",

  callback : function (req, res) {
    try {
      var body = actions.getJsonBody(req, res);

      if (body === undefined) {
        return;
      }

      if (req.requestType !== actions.PUT) {
        actions.resultUnsupported(req, res);
      }
      else {
        var example = body.example;
        var name = body.collection;
        var collection = db._collection(name);

        if (collection === null) {
          actions.collectionNotFound(req, res, name);
        }
        else if (typeof example !== "object") {
          actions.badParameter(req, res, "example");
        }
        else {
          var result = collection.byExample(example).limit(1);

          if (result.hasNext()) {
            actions.resultOk(req, res, actions.HTTP_OK, { document : result.next() });
          }
          else {
            actions.resultNotFound(req, res, ERRORS.ERROR_HTTP_NOT_FOUND.code, "no match");
          }
        }
      }
    }
    catch (err) {
      actions.resultException(req, res, err, undefined, false);
    }
  }
});

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSA_put_api_simple_first
/// @brief returns the first document(s) of a collection
///
/// @RESTHEADER{PUT /_api/simple/first, First document of a collection}
///
/// @RESTBODYPARAM{query,json,required}
/// Contains the query.
///
/// @RESTDESCRIPTION
///
/// This will return the first document(s) from the collection, in the order of
/// insertion/update time. When the *count* argument is supplied, the result
/// will be a list of documents, with the "oldest" document being first in the
/// result list.
/// If the *count* argument is not supplied, the result is the "oldest" document
/// of the collection, or *null* if the collection is empty.
///
/// The request body must be a JSON object with the following attributes:
/// - *collection*: the name of the collection
///
/// - *count*: the number of documents to return at most. Specifiying count is
///   optional. If it is not specified, it defaults to 1.
///
/// Note: this method is not supported for sharded collections with more than
/// one shard.
///
/// @RESTRETURNCODES
///
/// @RESTRETURNCODE{200}
/// is returned when the query was successfully executed.
///
/// @RESTRETURNCODE{400}
/// is returned if the body does not contain a valid JSON representation of a
/// query. The response body contains an error document in this case.
///
/// @RESTRETURNCODE{404}
/// is returned if the collection specified by *collection* is unknown.  The
/// response body contains an error document in this case.
///
/// @EXAMPLES
///
/// Retrieving the first n documents:
///
/// @EXAMPLE_ARANGOSH_RUN{RestSimpleFirst}
///     var cn = "products";
///     db._drop(cn);
///     var products = db._create(cn, { waitForSync: false });
///     products.save({ "a": { "k": 1, "j": 1 }, "i": 1});
///     products.save({ "a": { "j": 1 }, "i": 1});
///     products.save({ "i": 1});
///     products.save({ "a": { "k": 2, "j": 2 }, "i": 1});
///     var url = "/_api/simple/first";
///     var body = '{ "collection": "products", "count" : 2 }';
///
///     var response = logCurlRequest('PUT', url, body);
///
///     assert(response.code === 200);
///
///     logJsonResponse(response);
///     db._drop(cn);
/// @END_EXAMPLE_ARANGOSH_RUN
///
/// Retrieving the first document:
///
/// @EXAMPLE_ARANGOSH_RUN{RestSimpleFirstSingle}
///     var cn = "products";
///     db._drop(cn);
///     var products = db._create(cn, { waitForSync: false });
///     products.save({ "a": { "k": 1, "j": 1 }, "i": 1});
///     products.save({ "a": { "j": 1 }, "i": 1});
///     products.save({ "i": 1});
///     products.save({ "a": { "k": 2, "j": 2 }, "i": 1});
///     var url = "/_api/simple/first";
///     var body = '{ "collection": "products" }';
///
///     var response = logCurlRequest('PUT', url, body);
///
///     assert(response.code === 200);
///
///     logJsonResponse(response);
///     db._drop(cn);
/// @END_EXAMPLE_ARANGOSH_RUN
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

actions.defineHttp({
  url: API + "first",

  callback : function (req, res) {
    try {
      if (req.requestType !== actions.PUT) {
        actions.resultUnsupported(req, res);
      }
      else {
        var body = actions.getJsonBody(req, res);
        var name = body.collection;
        var count = body.count;
        var collection = db._collection(name);

        if (collection === null) {
          actions.collectionNotFound(req, res, name);
        }

        if (count) {
          actions.resultOk(req, res, actions.HTTP_OK, { result : collection.first(count) });
        }
        else {
          actions.resultOk(req, res, actions.HTTP_OK, { result : collection.first() });
        }
      }
    }
    catch (err) {
      actions.resultException(req, res, err, undefined, false);
    }
  }
});

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSA_put_api_simple_last
/// @brief returns the last document(s) of a collection
///
/// @RESTHEADER{PUT /_api/simple/last, Last document of a collection}
///
/// @RESTBODYPARAM{query,json,required}
/// Contains the query.
///
/// @RESTDESCRIPTION
///
/// This will return the last documents from the collection, in the order of
/// insertion/update time. When the *count* argument is supplied, the result
/// will be a list of documents, with the "latest" document being first in the
/// result list.
///
/// The request body must be a JSON object with the following attributes:
/// - *collection*: the name of the collection
///
/// - *count*: the number of documents to return at most. Specifiying count is
///   optional. If it is not specified, it defaults to 1.
///
/// If the *count* argument is not supplied, the result is the "latest" document
/// of the collection, or *null* if the collection is empty.
///
/// Note: this method is not supported for sharded collections with more than
/// one shard.
///
/// @RESTRETURNCODES
///
/// @RESTRETURNCODE{200}
/// is returned when the query was successfully executed.
///
/// @RESTRETURNCODE{400}
/// is returned if the body does not contain a valid JSON representation of a
/// query. The response body contains an error document in this case.
///
/// @RESTRETURNCODE{404}
/// is returned if the collection specified by *collection* is unknown.  The
/// response body contains an error document in this case.
///
/// @EXAMPLES
///
/// Retrieving the last n documents:
///
/// @EXAMPLE_ARANGOSH_RUN{RestSimpleLast}
///     var cn = "products";
///     db._drop(cn);
///     var products = db._create(cn, { waitForSync: false });
///     products.save({ "a": { "k": 1, "j": 1 }, "i": 1});
///     products.save({ "a": { "j": 1 }, "i": 1});
///     products.save({ "i": 1});
///     products.save({ "a": { "k": 2, "j": 2 }, "i": 1});
///     var url = "/_api/simple/last";
///     var body = '{ "collection": "products", "count" : 2 }';
///
///     var response = logCurlRequest('PUT', url, body);
///
///     assert(response.code === 200);
///
///     logJsonResponse(response);
///     db._drop(cn);
/// @END_EXAMPLE_ARANGOSH_RUN
///
/// Retrieving the first document:
///
/// @EXAMPLE_ARANGOSH_RUN{RestSimpleLastSingle}
///     var cn = "products";
///     db._drop(cn);
///     var products = db._create(cn, { waitForSync: false });
///     products.save({ "a": { "k": 1, "j": 1 }, "i": 1});
///     products.save({ "a": { "j": 1 }, "i": 1});
///     products.save({ "i": 1});
///     products.save({ "a": { "k": 2, "j": 2 }, "i": 1});
///     var url = "/_api/simple/last";
///     var body = '{ "collection": "products" }';
///
///     var response = logCurlRequest('PUT', url, body);
///
///     assert(response.code === 200);
///
///     logJsonResponse(response);
///     db._drop(cn);
/// @END_EXAMPLE_ARANGOSH_RUN
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

actions.defineHttp({
  url: API + "last",

  callback : function (req, res) {
    try {
      if (req.requestType !== actions.PUT) {
        actions.resultUnsupported(req, res);
      }
      else {
        var body = actions.getJsonBody(req, res);
        var name = body.collection;
        var count = body.count;
        var collection = db._collection(name);

        if (collection === null) {
          actions.collectionNotFound(req, res, name);
        }

        if (count) {
          actions.resultOk(req, res, actions.HTTP_OK, { result : collection.last(count) });
        }
        else {
          actions.resultOk(req, res, actions.HTTP_OK, { result : collection.last() });
        }
      }
    }
    catch (err) {
      actions.resultException(req, res, err, undefined, false);
    }
  }
});

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSA_put_api_simple_range
/// @brief returns all documents of a collection within a range
///
/// @RESTHEADER{PUT /_api/simple/range, Simple range query}
///
/// @RESTBODYPARAM{query,json,required}
/// Contains the query.
///
/// @RESTDESCRIPTION
///
/// This will find all documents within a given range. In order to execute a
/// range query, a skip-list index on the queried attribute must be present.
///
/// The call expects a JSON object as body with the following attributes:
///
/// - *collection*: The name of the collection to query.
///
/// - *attribute*: The attribute path to check.
///
/// - *left*: The lower bound.
///
/// - *right*: The upper bound.
///
/// - *closed*: If *true*, use interval including *left* and *right*,
///   otherwise exclude *right*, but include *left*.
///
/// - *skip*: The number of documents to skip in the query (optional).
///
/// - *limit*: The maximal amount of documents to return. The *skip*
///   is applied before the *limit* restriction. (optional)
///
/// Returns a cursor containing the result, see [Http Cursor](../HttpAqlQueryCursor/README.md) for details.
///
/// @RESTRETURNCODES
///
/// @RESTRETURNCODE{201}
/// is returned if the query was executed successfully.
///
/// @RESTRETURNCODE{400}
/// is returned if the body does not contain a valid JSON representation of a
/// query. The response body contains an error document in this case.
///
/// @RESTRETURNCODE{404}
/// is returned if the collection specified by *collection* is unknown.  The
/// response body contains an error document in this case.
///
/// @EXAMPLES
///
/// @EXAMPLE_ARANGOSH_RUN{RestSimpleRange}
///     var cn = "products";
///     db._drop(cn);
///     var products = db._create(cn, { waitForSync: true });
///     products.ensureUniqueSkiplist("i");
///     products.save({ "i": 1});
///     products.save({ "i": 2});
///     products.save({ "i": 3});
///     products.save({ "i": 4});
///     var url = "/_api/simple/range";
///     var body = '{ "collection": "products", "attribute" : "i", "left" : 2, "right" : 4 }';
///
///     var response = logCurlRequest('PUT', url, body);
///
///     assert(response.code === 201);
///
///     logJsonResponse(response);
///     db._drop(cn);
/// @END_EXAMPLE_ARANGOSH_RUN
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

actions.defineHttp({
  url: API + "range",

  callback : function (req, res) {
    try {
      var body = actions.getJsonBody(req, res);

      if (body === undefined) {
        return;
      }

      if (req.requestType !== actions.PUT) {
        actions.resultUnsupported(req, res);
      }
      else {
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
        }
        else {
          var result;

          if (closed) {
            result = collection.closedRange(attribute, left, right);
          }
          else {
            result = collection.range(attribute, left, right);
          }

          if (skip !== null && skip !== undefined) {
            result = result.skip(skip);
          }

          if (limit !== null && limit !== undefined) {
            result = result.limit(limit);
          }

          createCursorResponse(req, res, CREATE_CURSOR(result.toArray(), true, body.batchSize, body.ttl));
        }
      }
    }
    catch (err) {
      actions.resultException(req, res, err, undefined, false);
    }
  }
});

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSA_put_api_simple_remove_by_example
/// @brief removes all documents of a collection that match an example
///
/// @RESTHEADER{PUT /_api/simple/remove-by-example, Remove documents by example}
///
/// @RESTBODYPARAM{query,json,required}
/// Contains the query.
///
/// @RESTDESCRIPTION
///
/// This will find all documents in the collection that match the specified
/// example object.
///
/// The call expects a JSON object as body with the following attributes:
///
/// - *collection*: The name of the collection to remove from.
///
/// - *example*: An example document that all collection documents are compared
///   against.
///
/// - options: an json object which can contains following attributes:
///
/// - *waitForSync*: if set to true, then all removal operations will
///   instantly be synchronised to disk. If this is not specified, then the
///   collection's default sync behavior will be applied.
///
/// - *limit*: an optional value that determines how many documents to
///   delete at most. If *limit* is specified but is less than the number
///   of documents in the collection, it is undefined which of the documents
///   will be deleted.
///
/// Note: the *limit* attribute is not supported on sharded collections.
/// Using it will result in an error.
/// The options attributes waitForSync and limit can given yet without
/// an ecapsulation into a json object. but this may be deprecated in future
/// versions of arango
///
/// Returns the number of documents that were deleted.
///
/// @RESTRETURNCODES
///
/// @RESTRETURNCODE{200}
/// is returned if the query was executed successfully.
///
/// @RESTRETURNCODE{400}
/// is returned if the body does not contain a valid JSON representation of a
/// query. The response body contains an error document in this case.
///
/// @RESTRETURNCODE{404}
/// is returned if the collection specified by *collection* is unknown.  The
/// response body contains an error document in this case.
///
/// @EXAMPLES
///
/// @EXAMPLE_ARANGOSH_RUN{RestSimpleRemoveByExample}
///     var cn = "products";
///     db._drop(cn);
///     var products = db._create(cn, { waitForSync: true });
///     products.save({ "a": { "k": 1, "j": 1 }, "i": 1});
///     products.save({ "a": { "j": 1 }, "i": 1});
///     products.save({ "i": 1});
///     products.save({ "a": { "k": 2, "j": 2 }, "i": 1});
///     var url = "/_api/simple/remove-by-example";
///     var body = '{ "collection": "products", "example" : { "a" : { "j" : 1 } } }';
///
///     var response = logCurlRequest('PUT', url, body);
///
///     assert(response.code === 200);
///
///     logJsonResponse(response);
///     db._drop(cn);
/// @END_EXAMPLE_ARANGOSH_RUN
/// Using Parameter: waitForSync and limit
/// @EXAMPLE_ARANGOSH_RUN{RestSimpleRemoveByExample_1}
///     var cn = "products";
///     db._drop(cn);
///     var products = db._create(cn, { waitForSync: true });
///     products.save({ "a": { "k": 1, "j": 1 }, "i": 1});
///     products.save({ "a": { "j": 1 }, "i": 1});
///     products.save({ "i": 1});
///     products.save({ "a": { "k": 2, "j": 2 }, "i": 1});
///     var url = "/_api/simple/remove-by-example";
///     var body = '{ "collection": "products", "example" : { "a" : { "j" : 1 } },' +
///                 '"waitForSync": true, "limit": 2 }';
///
///     var response = logCurlRequest('PUT', url, body);
///
///     assert(response.code === 200);
///
///     logJsonResponse(response);
///     db._drop(cn);
/// @END_EXAMPLE_ARANGOSH_RUN
/// Using Parameter: waitForSync and limit with new signature
/// @EXAMPLE_ARANGOSH_RUN{RestSimpleRemoveByExample_2}
///     var cn = "products";
///     db._drop(cn);
///     var products = db._create(cn, { waitForSync: true });
///     products.save({ "a": { "k": 1, "j": 1 }, "i": 1});
///     products.save({ "a": { "j": 1 }, "i": 1});
///     products.save({ "i": 1});
///     products.save({ "a": { "k": 2, "j": 2 }, "i": 1});
///     var url = "/_api/simple/remove-by-example";
///     var body = '{'+
///                '"collection": "products",' +
///                '"example" : { "a" : { "j" : 1 } },' +
///                '"options": {"waitForSync": true, "limit": 2} ' +
///                '}';
///
///     var response = logCurlRequest('PUT', url, body);
///
///     assert(response.code === 200);
///
///     logJsonResponse(response);
///     db._drop(cn);
/// @END_EXAMPLE_ARANGOSH_RUN
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

actions.defineHttp({
  url: API + "remove-by-example",

  callback : function (req, res) {
    try {
      var body = actions.getJsonBody(req, res);

      if (body === undefined) {
        return;
      }

      if (req.requestType !== actions.PUT) {
        actions.resultUnsupported(req, res);
      }
      else {
        var example = body.example;
        var name = body.collection;
        var collection = db._collection(name);

        if (collection === null) {
          actions.collectionNotFound(req, res, name);
        }
        else if (typeof example !== "object" || Array.isArray(example)) {
          actions.badParameter(req, res, "example");
        }
        else {
          var options = body.options;
          if (typeof options === "undefined") {
            var waitForSync = body.waitForSync || undefined;
            var limit = body.limit || null;
            options = {waitForSync: waitForSync, limit: limit};
          }
          var result = collection.removeByExample(example, options);
          actions.resultOk(req, res, actions.HTTP_OK, { deleted: result });
        }
      }
    }
    catch (err) {
      actions.resultException(req, res, err, undefined, false);
    }
  }
});

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSA_put_api_simple_replace_by_example
/// @brief replaces the body of all documents of a collection that match an
/// example
///
/// @RESTHEADER{PUT /_api/simple/replace-by-example, Replace documents by example}
///
/// @RESTBODYPARAM{query,json,required}
/// Contains the query.
///
/// @RESTDESCRIPTION
///
/// This will find all documents in the collection that match the specified
/// example object, and replace the entire document body with the new value
/// specified. Note that document meta-attributes such as *_id*, *_key*,
/// *_from*, *_to* etc. cannot be replaced.
///
/// The call expects a JSON object as body with the following attributes:
///
/// - *collection*: The name of the collection to replace within.
///
/// - *example*: An example document that all collection documents are compared
///   against.
///
/// - *newValue*: The replacement document that will get inserted in place
///   of the "old" documents.
///
/// - *options*: an json object which can contain following attributes
///
/// - *waitForSync*: if set to true, then all removal operations will
///   instantly be synchronised to disk. If this is not specified, then the
///   collection's default sync behavior will be applied.
///
/// - *limit*: an optional value that determines how many documents to
///   replace at most. If *limit* is specified but is less than the number
///   of documents in the collection, it is undefined which of the documents
///   will be replaced.
///
/// Note: the *limit* attribute is not supported on sharded collections.
/// Using it will result in an error.
/// The options attributes waitForSync and limit can given yet without
/// an ecapsulation into a json object. but this may be deprecated in future
/// versions of arango
///
/// Returns the number of documents that were replaced.
///
/// @RESTRETURNCODES
///
/// @RESTRETURNCODE{200}
/// is returned if the query was executed successfully.
///
/// @RESTRETURNCODE{400}
/// is returned if the body does not contain a valid JSON representation of a
/// query. The response body contains an error document in this case.
///
/// @RESTRETURNCODE{404}
/// is returned if the collection specified by *collection* is unknown.  The
/// response body contains an error document in this case.
///
/// @EXAMPLES
///
/// @EXAMPLE_ARANGOSH_RUN{RestSimpleReplaceByExample}
///     var cn = "products";
///     db._drop(cn);
///     var products = db._create(cn, { waitForSync: true });
///     products.save({ "a": { "k": 1, "j": 1 }, "i": 1});
///     products.save({ "a": { "j": 1 }, "i": 1});
///     products.save({ "i": 1});
///     products.save({ "a": { "k": 2, "j": 2 }, "i": 1});
///     var url = "/_api/simple/replace-by-example";
///     var body = '{ ' +
///       '"collection": "products", ' +
///       '"example" : { "a" : { "j" : 1 } }, ' +
///       '"newValue" : {"foo" : "bar"}, ' +
///       '"limit" : 3 ' +
///     '}';
///
///     var response = logCurlRequest('PUT', url, body);
///
///     assert(response.code === 200);
///
///     logJsonResponse(response);
///     db._drop(cn);
/// @END_EXAMPLE_ARANGOSH_RUN
/// Using new Signature for attributes WaitForSync and limit
/// @EXAMPLE_ARANGOSH_RUN{RestSimpleReplaceByExampleWaitForSync}
///     var cn = "products";
///     db._drop(cn);
///     var products = db._create(cn, { waitForSync: true });
///     products.save({ "a": { "k": 1, "j": 1 }, "i": 1});
///     products.save({ "a": { "j": 1 }, "i": 1});
///     products.save({ "i": 1});
///     products.save({ "a": { "k": 2, "j": 2 }, "i": 1});
///     var url = "/_api/simple/replace-by-example";
///     var body = '{ ' +
///       '"collection": "products", ' +
///       '"example" : { "a" : { "j" : 1 } }, ' +
///       '"newValue" : {"foo" : "bar"}, ' +
///       '"options": {"limit" : 3,  "waitForSync": true  }'+
///     '}';
///
///     var response = logCurlRequest('PUT', url, body);
///
///     assert(response.code === 200);
///
///     logJsonResponse(response);
///     db._drop(cn);
/// @END_EXAMPLE_ARANGOSH_RUN
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

actions.defineHttp({
  url: API + "replace-by-example",

  callback : function (req, res) {
    try {
      var body = actions.getJsonBody(req, res);

      if (body === undefined) {
        return;
      }

      if (req.requestType !== actions.PUT) {
        actions.resultUnsupported(req, res);
      }
      else {
        var example = body.example;
        var newValue = body.newValue;
        var name = body.collection;
        var collection = db._collection(name);

        if (collection === null) {
          actions.collectionNotFound(req, res, name);
        }
        else if (typeof example !== "object" || Array.isArray(example)) {
          actions.badParameter(req, res, "example");
        }
        else if (typeof newValue !== "object" || Array.isArray(newValue)) {
          actions.badParameter(req, res, "newValue");
        }
        else {
          var options = body.options;
          if (typeof options === "undefined") {
            var waitForSync = body.waitForSync || undefined;
            var limit = body.limit || null;
            options = {waitForSync: waitForSync, limit: limit};
          }
          var result = collection.replaceByExample(example, newValue, options);
          actions.resultOk(req, res, actions.HTTP_OK, { replaced: result });
        }
      }
    }
    catch (err) {
      actions.resultException(req, res, err, undefined, false);
    }
  }
});

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSA_put_api_simple_update_by_example
/// @brief partially updates the body of all documents of a collection that
/// match an example
///
/// @RESTHEADER{PUT /_api/simple/update-by-example, Update documents by example}
///
/// @RESTBODYPARAM{query,json,required}
/// Contains the query.
///
/// @RESTDESCRIPTION
///
/// This will find all documents in the collection that match the specified
/// example object, and partially update the document body with the new value
/// specified. Note that document meta-attributes such as *_id*, *_key*,
/// *_from*, *_to* etc. cannot be replaced.
///
/// The call expects a JSON object as body with the following attributes:
///
/// - *collection*: The name of the collection to update within.
///
/// - *example*: An example document that all collection documents are compared
///   against.
///
/// - *newValue*: A document containing all the attributes to update in the
///   found documents.
///
/// - *options*: a json object wich can contains following attributes:
///
/// - *keepNull*: This parameter can be used to modify the behavior when
///   handling *null* values. Normally, *null* values are stored in the
///   database. By setting the *keepNull* parameter to *false*, this
///   behavior can be changed so that all attributes in *data* with *null*
///   values will be removed from the updated document.
///
/// - *waitForSync*: if set to true, then all removal operations will
///   instantly be synchronised to disk. If this is not specified, then the
///   collection's default sync behavior will be applied.
///
/// - *limit*: an optional value that determines how many documents to
///   update at most. If *limit* is specified but is less than the number
///   of documents in the collection, it is undefined which of the documents
///   will be updated.
///
/// Note: the *limit* attribute is not supported on sharded collections.
/// Using it will result in an error.
///
/// Returns the number of documents that were updated.
///
///
/// @RESTRETURNCODES
///
/// @RESTRETURNCODE{200}
/// is returned if the collection was updated successfully and *waitForSync* was
/// *true*.
///
/// @RESTRETURNCODE{400}
/// is returned if the body does not contain a valid JSON representation of a
/// query. The response body contains an error document in this case.
///
/// @RESTRETURNCODE{404}
/// is returned if the collection specified by *collection* is unknown.  The
/// response body contains an error document in this case.
///
/// @EXAMPLES
/// using old syntax for options
/// @EXAMPLE_ARANGOSH_RUN{RestSimpleUpdateByExample}
///     var cn = "products";
///     db._drop(cn);
///     var products = db._create(cn, { waitForSync: true });
///     products.save({ "a": { "k": 1, "j": 1 }, "i": 1});
///     products.save({ "a": { "j": 1 }, "i": 1});
///     products.save({ "i": 1});
///     products.save({ "a": { "k": 2, "j": 2 }, "i": 1});
///     var url = "/_api/simple/update-by-example";
///     var body = '{ ' +
///       '"collection": "products", ' +
///       '"example" : { "a" : { "j" : 1 } }, ' +
///       '"newValue" : { "a" : { "j" : 22 } }, ' +
///       '"limit" : 3 ' +
///     '}';
///
///     var response = logCurlRequest('PUT', url, body);
///
///     assert(response.code === 200);
///
///     logJsonResponse(response);
///     db._drop(cn);
/// @END_EXAMPLE_ARANGOSH_RUN
/// using new signature for options
/// @EXAMPLE_ARANGOSH_RUN{RestSimpleUpdateByExample_1}
///     var cn = "products";
///     db._drop(cn);
///     var products = db._create(cn, { waitForSync: true });
///     products.save({ "a": { "k": 1, "j": 1 }, "i": 1});
///     products.save({ "a": { "j": 1 }, "i": 1});
///     products.save({ "i": 1});
///     products.save({ "a": { "k": 2, "j": 2 }, "i": 1});
///     var url = "/_api/simple/update-by-example";
///     var body = '{ ' +
///       '"collection": "products", ' +
///       '"example" : { "a" : { "j" : 1 } }, ' +
///       '"newValue" : { "a" : { "j" : 22 } }, ' +
///       '"options" :  { "limit" : 3, "waitForSync": true }  ' +
///     '}';
///
///     var response = logCurlRequest('PUT', url, body);
///
///     assert(response.code === 200);
///
///     logJsonResponse(response);
///     db._drop(cn);
/// @END_EXAMPLE_ARANGOSH_RUN
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

actions.defineHttp({
  url: API + "update-by-example",

  callback : function (req, res) {
    try {
      var body = actions.getJsonBody(req, res);

      if (body === undefined) {
        return;
      }

      if (req.requestType !== actions.PUT) {
        actions.resultUnsupported(req, res);
      }
      else {
        var example = body.example;
        var newValue = body.newValue;
        var name = body.collection;
        var collection = db._collection(name);
        var limit = body.limit || null;
        var keepNull = body.keepNull || null;

        if (collection === null) {
          actions.collectionNotFound(req, res, name);
        }
        else if (typeof example !== "object" || Array.isArray(example)) {
          actions.badParameter(req, res, "example");
        }
        else if (typeof newValue !== "object" || Array.isArray(newValue)) {
          actions.badParameter(req, res, "newValue");
        }
        else {
          var options = body.options;
          if (typeof options === "undefined") {
            var waitForSync = body.waitForSync || undefined;
            limit = body.limit || undefined;
            options = {waitForSync: waitForSync, keepNull: keepNull, limit: limit};
          }
          var result = collection.updateByExample(example,
                                                  newValue,
                                                  options);
          actions.resultOk(req, res, actions.HTTP_OK, { updated: result });
        }
      }
    }
    catch (err) {
      actions.resultException(req, res, err, undefined, false);
    }
  }
});

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
