////////////////////////////////////////////////////////////////////////////////
/// @brief simple queries
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2012 triagens GmbH, Cologne, Germany
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
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Achim Brandt
/// @author Copyright 2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var actions = require("org/arangodb/actions");
var simple = require("org/arangodb/simple-query");
var db = require("org/arangodb").db;
var ERRORS = require("internal").errors;

var API = "_api/simple/";

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoAPI
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @fn JSA_PUT_api_simple_all
/// @brief returns all documents of a collection
///
/// @RESTHEADER{PUT /_api/simple/all,executes simple query ALL}
///
/// @RESTBODYPARAM{query,string,required}
/// Contains the query.
///
/// @RESTDESCRIPTION
///
/// Returns all documents of a collections. The call expects a JSON object
/// as body with the following attributes:
///
/// - `collection`: The name of the collection to query.
///
/// - `skip`: The number of documents to skip in the query (optional).
///
/// - `limit`: The maximal amount of documents to return. The `skip`
///   is applied before the `limit` restriction. (optional)
///
/// Returns a cursor containing the result, see @ref HttpCursor for details.
///
/// @RESTRETURNCODES
///
/// @RESTRETURNCODE{201}
/// is returned if the return set contains at least one document and `waitForSync` was
/// `true`.
///
/// @RESTRETURNCODE{400}
/// is returned if the body does not contain a valid JSON representation of a
/// document. The response body contains an error document in this case.
///
/// @RESTRETURNCODE{404}
/// is returned if the collection specified by `collection` is unknown.  The
/// response body contains an error document in this case.
///
/// @EXAMPLES
///
/// Limit the amount of documents using `limit`
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
/// Using a `batchSize` value
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
////////////////////////////////////////////////////////////////////////////////

actions.defineHttp({
  url : API + "all",
  context : "api",

  callback : function (req, res) {
    try {
      var body = actions.getJsonBody(req, res);

      if (body === undefined) {
        return;
      }

      if (req.requestType != actions.PUT) {
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

          actions.resultCursor(req, res, CREATE_CURSOR(result.toArray(), true, body.batchSize));
        }
      }
    }
    catch (err) {
      actions.resultException(req, res, err, undefined, false);
    }
  }
});

////////////////////////////////////////////////////////////////////////////////
/// @fn JSA_PUT_api_simple_any
/// @brief returns a random document of a collection
///
/// @RESTHEADER{PUT /_api/simple/any,executes simple query ANY}
///
/// @RESTBODYPARAM{query,string,required}
/// Contains the query.
///
/// @RESTDESCRIPTION
///
/// Returns a random document of a collection. The call expects a JSON object
/// as body with the following attributes:
///
/// - `collection`: The identifier or name of the collection to query.
///
/// Returns a JSON object with the document stored in the attribute
/// `document` if the collection contains at least one document. If
/// the collection is empty, the attrbute contains null.
///
/// @RESTRETURNCODES
///
/// @RESTRETURNCODE{200}
/// is returned if the return set contains at least one document and `waitForSync` was
/// `true`.
///
/// @RESTRETURNCODE{400}
/// is returned if the body does not contain a valid JSON representation of a
/// document. The response body contains an error document in this case.
///
/// @RESTRETURNCODE{404}
/// is returned if the collection specified by `collection` is unknown.  The
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
////////////////////////////////////////////////////////////////////////////////

actions.defineHttp({
  url : API + "any",
  context : "api",

  callback : function (req, res) {
    try {
      var body = actions.getJsonBody(req, res);

      if (body === undefined) {
        return;
      }

      if (req.requestType != actions.PUT) {
        actions.resultUnsupported(req, res);
      }
      else {
        var name = body.collection;
        var id = parseInt(name) || name;
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
/// @fn JSA_PUT_api_simple_near
/// @brief returns all documents of a collection near a given location
///
/// @RESTHEADER{PUT /_api/simple/near,executes simple query NEAR}
///
/// @RESTBODYPARAM{query,string,required}
/// Contains the query.
///
/// @RESTDESCRIPTION
///
/// The default will find at most 100 documents near a given coordinate.  The
/// returned list is sorted according to the distance, with the nearest document
/// coming first. If there are near documents of equal distance, documents are
/// chosen randomly from this set until the limit is reached.
///
/// In order to use the `near` operator, a geo index must be defined for the
/// collection. This index also defines which attribute holds the coordinates
/// for the document.  If you have more then one geo-spatial index, you can use
/// the `geo` field to select a particular index.
///
/// The call expects a JSON hash array as body with the following attributes:
///
/// - `collection`: The name of the collection to query.
///
/// - `latitude`: The latitude of the coordinate.
///
/// - `longitude`: The longitude of the coordinate.
///
/// - `distance`: If given, the attribute key used to store the
///   distance. (optional)
///
/// - `skip`: The number of documents to skip in the query. (optional)
///
/// - `limit`: The maximal amount of documents to return. The `skip` is
///   applied before the `limit` restriction. The default is 100. (optional)
///
/// - `geo`: If given, the identifier of the geo-index to use. (optional)
///
/// Returns a cursor containing the result, see @ref HttpCursor for details.
///
/// @RESTRETURNCODES
///
/// @RESTRETURNCODE{201}
/// is returned if the return set contains at least one document and `waitForSync` was
/// `true`.
///
/// @RESTRETURNCODE{400}
/// is returned if the body does not contain a valid JSON representation of a
/// document. The response body contains an error document in this case.
///
/// @RESTRETURNCODE{404}
/// is returned if the collection specified by `collection` is unknown.  The
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
///     var body = '{ "collection": "products", "latitude" : 0, "longitude" : 0, "skip" : 1, "limit" : 2 }';
///
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
///     var body = '{ "collection": "products", "latitude" : 0, "longitude" : 0, "skip" : 1, "limit" : 3, "distance" : "distance" }';
///
///     var response = logCurlRequest('PUT', url, body);
///
///     assert(response.code === 201);
///
///     logJsonResponse(response);
///     db._drop(cn);
/// @END_EXAMPLE_ARANGOSH_RUN
////////////////////////////////////////////////////////////////////////////////

actions.defineHttp({
  url : API + "near",
  context : "api",

  callback : function (req, res) {
    try {
      var body = actions.getJsonBody(req, res);

      if (body === undefined) {
        return;
      }

      if (req.requestType != actions.PUT) {
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

          if (skip !== null && skip != undefined) {
            result = result.skip(skip);
          }

          if (limit !== null && limit != undefined) {
            result = result.limit(limit);
          }

          if (distance !== null && distance !== undefined) {
            result = result.distance(distance);
          }

          actions.resultCursor(req, res, CREATE_CURSOR(result.toArray(), true, body.batchSize));
        }
      }
    }
    catch (err) {
      actions.resultException(req, res, err, undefined, false);
    }
  }
});

////////////////////////////////////////////////////////////////////////////////
/// @fn JSA_PUT_api_simple_within
/// @brief returns all documents of a collection within a given radius
///
/// @RESTHEADER{PUT /_api/simple/within,executes simple query WITHIN}
///
/// @RESTBODYPARAM{query,string,required}
/// Contains the query.
///
/// @RESTDESCRIPTION
///
/// This will find all documents with in a given radius around the coordinate
/// (`latitude`, `longitude`). The returned list is sorted by distance.
///
/// In order to use the `within` operator, a geo index must be defined for
/// the collection. This index also defines which attribute holds the
/// coordinates for the document.  If you have more then one geo-spatial index,
/// you can use the `geo` field to select a particular index.
///
/// The call expects a JSON hash array as body with the following attributes:
///
/// - `collection`: The name of the collection to query.
///
/// - `latitude`: The latitude of the coordinate.
///
/// - `longitude`: The longitude of the coordinate.
///
/// - `radius`: The maximal radius (in meters).
///
/// - `distance`: If given, the result attribute key used to store the
///   distance values (optional). If specified, distances are returned in meters.
///
/// - `skip`: The documents to skip in the query. (optional)
///
/// - `limit`: The maximal amount of documents to return. (optional)
///
/// - `geo`: If given, the identifier of the geo-index to use. (optional)
///
/// Returns a cursor containing the result, see @ref HttpCursor for details.
///
/// @RESTRETURNCODES
///
/// @RESTRETURNCODE{201}
/// is returned if the return set contains at least one document and `waitForSync` was
/// `true`.
///
/// @RESTRETURNCODE{400}
/// is returned if the body does not contain a valid JSON representation of a
/// document. The response body contains an error document in this case.
///
/// @RESTRETURNCODE{404}
/// is returned if the collection specified by `collection` is unknown.  The
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
///     var body = '{ "collection": "products", "latitude" : 0, "longitude" : 0, "skip" : 1, "limit" : 2, "radius" : 500 }';
///
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
////////////////////////////////////////////////////////////////////////////////

actions.defineHttp({
  url : API + "within",
  context : "api",

  callback : function (req, res) {
    try {
      var body = actions.getJsonBody(req, res);

      if (body === undefined) {
        return;
      }

      if (req.requestType != actions.PUT) {
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
          
          actions.resultCursor(req, res, CREATE_CURSOR(result.toArray(), true, body.batchSize));
        }
      }
    }
    catch (err) {
      actions.resultException(req, res, err, undefined, false);
    }
  }
});

////////////////////////////////////////////////////////////////////////////////
/// @fn JSA_PUT_api_simple_fulltext
/// @brief returns documents of a collection as a result of a fulltext query
///
/// @RESTHEADER{PUT /_api/simple/fulltext,executes simple query FULLTEXT}
///
/// @RESTBODYPARAM{query,string,required}
/// Contains the query.
///
/// @RESTDESCRIPTION
///
/// This will find all documents from the collection that match the fulltext
/// query specified in `query`.
///
/// In order to use the `fulltext` operator, a fulltext index must be defined 
/// for the collection and the specified attribute.
///
/// The call expects a JSON hash array as body with the following attributes:
///
/// - `collection`: The name of the collection to query.
///
/// - `attribute`: The attribute that contains the texts.
///
/// - `query`: The fulltext query.
///
/// - `skip`: The documents to skip in the query. (optional)
///
/// - `limit`: The maximal amount of documents to return. (optional)
///
/// - `index`: If given, the identifier of the fulltext-index to use. (optional)
///
/// Returns a cursor containing the result, see @ref HttpCursor for details.
///
/// @RESTRETURNCODES
///
/// @RESTRETURNCODE{201}
/// is returned if the return set contains at least one document and `waitForSync` was
/// `true`.
///
/// @RESTRETURNCODE{400}
/// is returned if the body does not contain a valid JSON representation of a
/// document. The response body contains an error document in this case.
///
/// @RESTRETURNCODE{404}
/// is returned if the collection specified by `collection` is unknown.  The
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
////////////////////////////////////////////////////////////////////////////////

actions.defineHttp({
  url : API + "fulltext",
  context : "api",

  callback : function (req, res) {
    try {
      var body = actions.getJsonBody(req, res);

      if (body === undefined) {
        return;
      }

      if (req.requestType != actions.PUT) {
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
          
          actions.resultCursor(req, res, CREATE_CURSOR(result.toArray(), true, body.batchSize));
        }
      }
    }
    catch (err) {
      actions.resultException(req, res, err, undefined, false);
    }
  }
});

////////////////////////////////////////////////////////////////////////////////
/// @fn JSA_PUT_api_simple_by_example
/// @brief returns all documents of a collection matching a given example
///
/// @RESTHEADER{PUT /_api/simple/by-example,executes simple query by-example}
///
/// @RESTBODYPARAM{query,string,required}
/// Contains the query.
///
/// @RESTDESCRIPTION
///
/// This will find all documents matching a given example.
///
/// The call expects a JSON hash array as body with the following attributes:
///
/// - `collection`: The name of the collection to query.
///
/// - `example`: The example.
///
/// - `skip`: The documents to skip in the query. (optional)
///
/// - `limit`: The maximal amount of documents to return. (optional)
///
/// Returns a cursor containing the result, see @ref HttpCursor for details.
///
/// @RESTRETURNCODES
///
/// @RESTRETURNCODE{201}
/// is returned if the return set contains at least one document and `waitForSync` was
/// `true`.
///
/// @RESTRETURNCODE{400}
/// is returned if the body does not contain a valid JSON representation of a
/// document. The response body contains an error document in this case.
///
/// @RESTRETURNCODE{404}
/// is returned if the collection specified by `collection` is unknown.  The
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
////////////////////////////////////////////////////////////////////////////////

actions.defineHttp({
  url : API + "by-example",
  context : "api",

  callback : function (req, res) {
    try {
      var body = actions.getJsonBody(req, res);

      if (body === undefined) {
        return;
      }

      if (req.requestType != actions.PUT) {
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

          actions.resultCursor(req, res, CREATE_CURSOR(result.toArray(), true, body.batchSize));
        }
      }
    }
    catch (err) {
      actions.resultException(req, res, err, undefined, false);
    }
  }
});

////////////////////////////////////////////////////////////////////////////////
/// @fn JSA_PUT_api_simple_first_example
/// @brief returns one document of a collection matching a given example
///
/// @RESTHEADER{PUT /_api/simple/first-example,executes simple query first-example}
///
/// @RESTBODYPARAM{query,string,required}
/// Contains the query.
//
/// @RESTDESCRIPTION
///
/// This will return the first document matching a given example.
///
/// The call expects a JSON hash array as body with the following attributes:
///
/// - `collection`: The name of the collection to query.
///
/// - `example`: The example.
///
/// Returns a result containing the document or `HTTP 404` if no
/// document matched the example.
///
/// @RESTRETURNCODES
///
/// @RESTRETURNCODE{201}
/// is returned if the return set contains at least one document and `waitForSync` was
/// `true`.
///
/// @RESTRETURNCODE{400}
/// is returned if the body does not contain a valid JSON representation of a
/// document. The response body contains an error document in this case.
///
/// @RESTRETURNCODE{404}
/// is returned if the collection specified by `collection` is unknown.  The
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
////////////////////////////////////////////////////////////////////////////////

actions.defineHttp({
  url : API + "first-example",
  context : "api",

  callback : function (req, res) {
    try {
      var body = actions.getJsonBody(req, res);
  
      if (body === undefined) {
        return;
      }

      if (req.requestType != actions.PUT) {
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
/// @brief returns all documents of a collection matching a given example
////////////////////////////////////////////////////////////////////////////////

actions.defineHttp({
  url : API + "BY-EXAMPLE-HASH",
  context : "api",

  callback : function (req, res) {
    try {
      var body = actions.getJsonBody(req, res);

      if (body === undefined) {
        return;
      }

      if (req.requestType != actions.PUT) {
        actions.resultUnsupported(req, res);
      }
      else {
        var limit = body.limit;
        var skip = body.skip;
        var name = body.collection;
        var example = body.example;
        var index = body.index;
        var collection = db._collection(name);

        if (collection === null) {
          actions.collectionNotFound(req, res, name);
        }
        else if (typeof example !== "object") {
          actions.badParameter(req, res, "example");
        }
        else {
          var result = collection.BY_EXAMPLE_HASH(index, example, skip, limit);

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
/// @fn JSA_PUT_api_simple_range
/// @brief returns all documents of a collection within a range
///
/// @RESTHEADER{PUT /_api/simple/range,executes simple range query}
///
/// @RESTBODYPARAM{query,string,required}
/// Contains the query.
//
/// @RESTDESCRIPTION
///
/// This will find all documents within a given range. You must declare a
/// skip-list index on the attribute in order to be able to use a range query.
///
/// The call expects a JSON hash array as body with the following attributes:
///
/// - `collection`: The name of the collection to query.
///
/// - `attribute`: The attribute path to check.
///
/// - `left`: The lower bound.
///
/// - `right`: The upper bound.
///
/// - `closed`: If true, use interval including `left` and `right`,
///   otherwise exclude `right`, but include `left`.
///
/// - `skip`: The documents to skip in the query. (optional)
///
/// - `limit`: The maximal amount of documents to return. (optional)
///
/// Returns a cursor containing the result, see @ref HttpCursor for details.
///
/// @RESTRETURNCODES
///
/// @RESTRETURNCODE{201}
/// is returned if there are at least one document in the return range and `waitForSync` was
/// `true`.
///
/// @RESTRETURNCODE{400}
/// is returned if the body does not contain a valid JSON representation of a
/// document. The response body contains an error document in this case.
///
/// @RESTRETURNCODE{404}
/// is returned if the collection specified by `collection` is unknown.  The
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
////////////////////////////////////////////////////////////////////////////////

actions.defineHttp({
  url : API + "range",
  context : "api",

  callback : function (req, res) {
    try {
      var body = actions.getJsonBody(req, res);

      if (body === undefined) {
        return;
      }

      if (req.requestType != actions.PUT) {
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

          actions.resultCursor(req, res, CREATE_CURSOR(result.toArray(), true, body.batchSize));
        }
      }
    }
    catch (err) {
      actions.resultException(req, res, err, undefined, false);
    }
  }
});

////////////////////////////////////////////////////////////////////////////////
/// @fn JSA_PUT_api_simple_remove_by_example
/// @brief removes all documents of a collection that match an example
///
/// @RESTHEADER{PUT /_api/simple/remove-by-example,removes documents by example}
///
/// @RESTBODYPARAM{query,string,required}
/// Contains the query.
//
/// @RESTDESCRIPTION
///
/// This will find all documents in the collection that match the specified 
/// example object. 
///
/// The call expects a JSON hash array as body with the following attributes:
///
/// - `collection`: The name of the collection to remove from.
///
/// - `example`: An example object that all collection objects are compared
///   against.
///
/// - `waitForSync`: if set to true, then all removal operations will 
///   instantly be synchronised to disk. If this is not specified, then the
///   collection's default sync behavior will be applied.
///
/// - `limit`: an optional value that determines how many documents to 
///   delete at most. If `limit` is specified but is less than the number
///   of documents in the collection, it is undefined which of the documents 
///   will be deleted.
///
/// Returns the number of documents that were deleted
///
/// @RESTRETURNCODES
///
/// @RESTRETURNCODE{200}
/// is returned if there where removed at least one document and `waitForSync` was
/// `true`.
///
/// @RESTRETURNCODE{400}
/// is returned if the body does not contain a valid JSON representation of a
/// document. The response body contains an error document in this case.
///
/// @RESTRETURNCODE{404}
/// is returned if the collection specified by `collection` is unknown.  The
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
////////////////////////////////////////////////////////////////////////////////

actions.defineHttp({
  url : API + "remove-by-example",
  context : "api",

  callback : function (req, res) {
    try {
      var body = actions.getJsonBody(req, res);
  
      if (body === undefined) {
        return;
      }

      if (req.requestType != actions.PUT) {
        actions.resultUnsupported(req, res);
      }
      else {
        var example = body.example;
        var name = body.collection;
        var collection = db._collection(name);
        var limit = body.limit || null;

        if (collection === null) {
          actions.collectionNotFound(req, res, name);
        }
        else if (typeof example !== "object" || Array.isArray(example)) {
          actions.badParameter(req, res, "example");
        }
        else {
          var result = collection.removeByExample(example, body.waitForSync, limit);
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
/// @fn JSA_PUT_api_simple_replace_by_example
/// @brief replaces the body of all documents of a collection that match an 
/// example
///
/// @RESTHEADER{PUT /_api/simple/replace-by-example,replaces documents by example}
///
/// @RESTBODYPARAM{query,string,required}
/// Contains the query.
//
/// @RESTDESCRIPTION
///
/// This will find all documents in the collection that match the specified 
/// example object, and replace the entire document body with the new value
/// specified. Note that document meta-attributes such as `_id`, `_key`,
/// `_from`, `_to` etc. cannot be replaced. 
///
/// The call expects a JSON hash array as body with the following attributes:
///
/// - `collection`: The name of the collection to replace within.
///
/// - `example`: An example object that all collection objects are compared
///   against.
///
/// - `newValue`: The replacement document that will get inserted in place
///   of the "old" documents.
///
/// - `waitForSync`: if set to true, then all removal operations will 
///   instantly be synchronised to disk. If this is not specified, then the
///   collection's default sync behavior will be applied.
///
/// - `limit`: an optional value that determines how many documents to 
///   replace at most. If `limit` is specified but is less than the number
///   of documents in the collection, it is undefined which of the documents 
///   will be replaced.
///
/// Returns the number of documents that were replaced
///
/// @RESTRETURNCODES
///
/// @RESTRETURNCODE{200}
/// is returned if the documents in the collection where replaced successfully and `waitForSync` was
/// `true`.
///
/// @RESTRETURNCODE{400}
/// is returned if the body does not contain a valid JSON representation of a
/// document. The response body contains an error document in this case.
///
/// @RESTRETURNCODE{404}
/// is returned if the collection specified by `collection` is unknown.  The
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
///     var body = '{ "collection": "products", "example" : { "a" : { "j" : 1 } }, "newValue" : {"foo" : "bar"}, "limit" : 3 }';
///
///     var response = logCurlRequest('PUT', url, body);
///
///     assert(response.code === 200);
///
///     logJsonResponse(response);
///     db._drop(cn);
/// @END_EXAMPLE_ARANGOSH_RUN
////////////////////////////////////////////////////////////////////////////////

actions.defineHttp({
  url : API + "replace-by-example",
  context : "api",

  callback : function (req, res) {
    try {
      var body = actions.getJsonBody(req, res);
  
      if (body === undefined) {
        return;
      }

      if (req.requestType != actions.PUT) {
        actions.resultUnsupported(req, res);
      }
      else {
        var example = body.example;
        var newValue = body.newValue;
        var name = body.collection;
        var collection = db._collection(name);
        var limit = body.limit || null;

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
          var result = collection.replaceByExample(example, newValue, body.waitForSync, limit);
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
/// @fn JSA_PUT_api_simple_update_by_example
/// @brief partially updates the body of all documents of a collection that
/// match an example
///
/// @RESTHEADER{PUT /_api/simple/update-by-example,updates documents by example}
///
/// @RESTBODYPARAM{query,string,required}
/// Contains the query.
//
/// @RESTDESCRIPTION
///
/// This will find all documents in the collection that match the specified 
/// example object, and partially update the document body with the new value
/// specified. Note that document meta-attributes such as `_id`, `_key`,
/// `_from`, `_to` etc. cannot be replaced. 
///
/// The call expects a JSON hash array as body with the following attributes:
///
/// - `collection`: The name of the collection to update within.
///
/// - `example`: An example object that all collection objects are compared
///   against.
///
/// - `newValue`: The update value that will get inserted in place
///   of the "old" version of the found documents.
///
/// - `keepNull`: This parameter can be used to modify the behavior when
///   handling `null` values. Normally, `null` values are stored in the
///   database. By setting the `keepNull` parameter to `false`, this 
///   behavior can be changed so that all attributes in `data` with `null` 
///   values will be removed from the updated document.
///
/// - `waitForSync`: if set to true, then all removal operations will 
///   instantly be synchronised to disk. If this is not specified, then the
///   collection's default sync behavior will be applied.
///
/// - `limit`: an optional value that determines how many documents to 
///   update at most. If `limit` is specified but is less than the number
///   of documents in the collection, it is undefined which of the documents 
///   will be updated.
///
/// Returns the number of documents that were updated
///
/// @RESTRETURNCODES
///
/// @RESTRETURNCODE{200}
/// is returned if the collection was updated successfully and `waitForSync` was
/// `true`.
///
/// @RESTRETURNCODE{400}
/// is returned if the body does not contain a valid JSON representation of a
/// document. The response body contains an error document in this case.
///
/// @RESTRETURNCODE{404}
/// is returned if the collection specified by `collection` is unknown.  The
/// response body contains an error document in this case.
///
/// @EXAMPLES
///
/// @EXAMPLE_ARANGOSH_RUN{RestSimpleUpdateByExample}
///     var cn = "products";
///     db._drop(cn);
///     var products = db._create(cn, { waitForSync: true });
///     products.save({ "a": { "k": 1, "j": 1 }, "i": 1});
///     products.save({ "a": { "j": 1 }, "i": 1});
///     products.save({ "i": 1});
///     products.save({ "a": { "k": 2, "j": 2 }, "i": 1});
///     var url = "/_api/simple/update-by-example";
///     var body = '{ "collection": "products", "example" : { "a" : { "j" : 1 } }, "newValue" : { "a" : { "j" : 22 } }, "limit" : 3 }';
///
///     var response = logCurlRequest('PUT', url, body);
///
///     assert(response.code === 200);
///
///     logJsonResponse(response);
///     db._drop(cn);
/// @END_EXAMPLE_ARANGOSH_RUN
////////////////////////////////////////////////////////////////////////////////

actions.defineHttp({
  url : API + "update-by-example",
  context : "api",

  callback : function (req, res) {
    try {
      var body = actions.getJsonBody(req, res);
  
      if (body === undefined) {
        return;
      }

      if (req.requestType != actions.PUT) {
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
          var result = collection.updateByExample(example, newValue, keepNull, body.waitForSync, limit);
          actions.resultOk(req, res, actions.HTTP_OK, { updated: result });
        }
      }
    }
    catch (err) {
      actions.resultException(req, res, err, undefined, false);
    }
  }
});

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @}\\)"
// End:
