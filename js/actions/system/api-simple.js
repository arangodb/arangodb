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
/// @RESTHEADER{PUT /_api/simple/all,executes simple query "all"}
///
/// @REST{PUT /_api/simple/all}
///
/// Returns all documents of a collections. The call expects a JSON object
/// as body with the following attributes:
///
/// - @LIT{collection}: The name of the collection to query.
///
/// - @LIT{skip}: The number of documents to skip in the query (optional).
///
/// - @LIT{limit}: The maximal amount of documents to return. The @LIT{skip}
///   is applied before the @LIT{limit} restriction. (optional)
///
/// Returns a cursor containing the result, see @ref HttpCursor for details.
///
/// @EXAMPLES
///
/// Limit the amount of documents using @LIT{limit}
///
/// @verbinclude api-simple-all-skip-limit
/// 
/// Using a @LIT{batchSize} value
///
/// @verbinclude api-simple-all-batch
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
      actions.resultException(req, res, err);
    }
  }
});

////////////////////////////////////////////////////////////////////////////////
/// @fn JSA_PUT_api_simple_any
/// @brief returns a random document of a collection
///
/// @RESTHEADER{PUT /_api/simple/any,executes simple query "any"}
///
/// @REST{PUT /_api/simple/any}
///
/// Returns a random document of a collection. The call expects a JSON object
/// as body with the following attributes:
///
/// - @LIT{collection}: The identifier or name of the collection to query.
///
/// Returns a JSON object with the document stored in the attribute
/// @LIT{document} if the collection contains at least one document. If
/// the collection is empty, the attrbute contains null.
///
/// @EXAMPLES
///
/// @code
/// > curl --data @- -X PUT --dump - http://localhost:8529/_api/simple/any
/// { "collection" : 222186062247 }
///
/// HTTP/1.1 200 OK
/// content-type: application/json; charset=utf-8
/// content-length: 114
/// 
/// {
///   "document": {
///     "_id": "222186062247/223172116903",
///     "_rev": 223172116903,
///     "Hello": "World"
///   },
///   "error": false,
///   "code":200
/// }
/// @endcode
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
      actions.resultException(req, res, err);
    }
  }
});

////////////////////////////////////////////////////////////////////////////////
/// @fn JSA_PUT_api_simple_near
/// @brief returns all documents of a collection near a given location
///
/// @RESTHEADER{PUT /_api/simple/near,executes simple query "near"}
///
/// @REST{PUT /_api/simple/near}
///
/// The default will find at most 100 documents near a given coordinate.  The
/// returned list is sorted according to the distance, with the nearest document
/// coming first. If there are near documents of equal distance, documents are
/// chosen randomly from this set until the limit is reached.
///
/// In order to use the @FN{near} operator, a geo index must be defined for the
/// collection. This index also defines which attribute holds the coordinates
/// for the document.  If you have more then one geo-spatial index, you can use
/// the @LIT{geo} field to select a particular index.
///
/// The call expects a JSON hash array as body with the following attributes:
///
/// - @LIT{collection}: The name of the collection to query.
///
/// - @LIT{latitude}: The latitude of the coordinate.
///
/// - @LIT{longitude}: The longitude of the coordinate.
///
/// - @LIT{distance}: If given, the attribute key used to store the
///   distance. (optional)
///
/// - @LIT{skip}: The number of documents to skip in the query. (optional)
///
/// - @LIT{limit}: The maximal amount of documents to return. The @LIT{skip} is
///   applied before the @LIT{limit} restriction. The default is 100. (optional)
///
/// - @LIT{geo}: If given, the identifier of the geo-index to use. (optional)
///
/// Returns a cursor containing the result, see @ref HttpCursor for details.
///
/// @EXAMPLES
///
/// Without distance:
///
/// @verbinclude api-simple-near
///
/// With distance:
///
/// @verbinclude api-simple-near-distance
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
      actions.resultException(req, res, err);
    }
  }
});

////////////////////////////////////////////////////////////////////////////////
/// @fn JSA_PUT_api_simple_within
/// @brief returns all documents of a collection within a given radius
///
/// @RESTHEADER{PUT /_api/simple/within,executes simple query "within"}
///
/// @REST{PUT /_api/simple/within}
///
/// This will find all documents with in a given radius around the coordinate
/// (@FA{latitude}, @FA{longitude}). The returned list is sorted by distance.
///
/// In order to use the @FN{within} operator, a geo index must be defined for
/// the collection. This index also defines which attribute holds the
/// coordinates for the document.  If you have more then one geo-spatial index,
/// you can use the @LIT{geo} field to select a particular index.
///
/// The call expects a JSON hash array as body with the following attributes:
///
/// - @LIT{collection}: The name of the collection to query.
///
/// - @LIT{latitude}: The latitude of the coordinate.
///
/// - @LIT{longitude}: The longitude of the coordinate.
///
/// - @LIT{radius}: The maximal radius (in meters).
///
/// - @LIT{distance}: If given, the result attribute key used to store the
///   distance values (optional). If specified, distances are returned in meters.
///
/// - @LIT{skip}: The documents to skip in the query. (optional)
///
/// - @LIT{limit}: The maximal amount of documents to return. (optional)
///
/// - @LIT{geo}: If given, the identifier of the geo-index to use. (optional)
///
/// Returns a cursor containing the result, see @ref HttpCursor for details.
///
/// @EXAMPLES
///
/// Without distance:
///
/// @verbinclude api-simple-within
///
/// With distance:
///
/// @verbinclude api-simple-within-distance
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
      actions.resultException(req, res, err);
    }
  }
});

////////////////////////////////////////////////////////////////////////////////
/// @fn JSA_PUT_api_simple_fulltext
/// @brief returns documents of a collection as a result of a fulltext query
///
/// @RESTHEADER{PUT /_api/simple/fulltext,executes simple query "fulltext"}
///
/// @REST{PUT /_api/simple/fulltext}
///
/// This will find all documents from the collection that match the fulltext
/// query specified in @FA{query}.
///
/// In order to use the @FN{fulltext} operator, a fulltext index must be defined 
/// for the collection and the specified attribute.
///
/// The call expects a JSON hash array as body with the following attributes:
///
/// - @LIT{collection}: The name of the collection to query.
///
/// - @LIT{attribute}: The attribute that contains the texts.
///
/// - @LIT{query}: The fulltext query.
///
/// - @LIT{skip}: The documents to skip in the query. (optional)
///
/// - @LIT{limit}: The maximal amount of documents to return. (optional)
///
/// - @LIT{index}: If given, the identifier of the fulltext-index to use. (optional)
///
/// Returns a cursor containing the result, see @ref HttpCursor for details.
///
/// @EXAMPLES
///
/// @verbinclude api-simple-fulltext
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
      actions.resultException(req, res, err);
    }
  }
});

////////////////////////////////////////////////////////////////////////////////
/// @fn JSA_PUT_api_simple_by_example
/// @brief returns all documents of a collection matching a given example
///
/// @RESTHEADER{PUT /_api/simple/by-example,executes simple query "by-example"}
///
/// @REST{PUT /_api/simple/by-example}
///
/// This will find all documents matching a given example.
///
/// The call expects a JSON hash array as body with the following attributes:
///
/// - @LIT{collection}: The name of the collection to query.
///
/// - @LIT{example}: The example.
///
/// - @LIT{skip}: The documents to skip in the query. (optional)
///
/// - @LIT{limit}: The maximal amount of documents to return. (optional)
///
/// Returns a cursor containing the result, see @ref HttpCursor for details.
///
/// @EXAMPLES
///
/// Matching an attribute:
///
/// @verbinclude api-simple-by-example1
///
/// Matching an attribute which is a sub-document:
///
/// @verbinclude api-simple-by-example2
///
/// Matching an attribute within a sub-document:
///
/// @verbinclude api-simple-by-example3
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
      actions.resultException(req, res, err);
    }
  }
});

////////////////////////////////////////////////////////////////////////////////
/// @fn JSA_PUT_api_simple_first_example
/// @brief returns one document of a collection matching a given example
///
/// @RESTHEADER{PUT /_api/simple/first-example,executes simple query "first-example"}
///
/// @REST{PUT /_api/simple/first-example}
///
/// This will return the first document matching a given example.
///
/// The call expects a JSON hash array as body with the following attributes:
///
/// - @LIT{collection}: The name of the collection to query.
///
/// - @LIT{example}: The example.
///
/// Returns a result containing the document or @LIT{HTTP 404} if no
/// document matched the example.
///
/// @EXAMPLES
///
/// If a matching document was found:
///
/// @verbinclude api-simple-first-example
///
/// If no document was found:
///
/// @verbinclude api-simple-first-example-not-found
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
      actions.resultException(req, res, err);
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
      actions.resultException(req, res, err);
    }
  }
});

////////////////////////////////////////////////////////////////////////////////
/// @fn JSA_PUT_api_simple_range
/// @brief returns all documents of a collection within a range
///
/// @RESTHEADER{PUT /_api/simple/range,executes simple range query}
///
/// @REST{PUT /_api/simple/range}
///
/// This will find all documents within a given range. You must declare a
/// skip-list index on the attribute in order to be able to use a range query.
///
/// The call expects a JSON hash array as body with the following attributes:
///
/// - @LIT{collection}: The name of the collection to query.
///
/// - @LIT{attribute}: The attribute path to check.
///
/// - @LIT{left}: The lower bound.
///
/// - @LIT{right}: The upper bound.
///
/// - @LIT{closed}: If true, use interval including @LIT{left} and @LIT{right},
///   otherwise exclude @LIT{right}, but include @LIT{left}.
///
/// - @LIT{skip}: The documents to skip in the query. (optional)
///
/// - @LIT{limit}: The maximal amount of documents to return. (optional)
///
/// Returns a cursor containing the result, see @ref HttpCursor for details.
///
/// @EXAMPLES
///
/// @verbinclude api-simple-range
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
      actions.resultException(req, res, err);
    }
  }
});

////////////////////////////////////////////////////////////////////////////////
/// @fn JSA_PUT_api_simple_remove_by_example
/// @brief removes all documents of a collection that match an example
///
/// @RESTHEADER{PUT /_api/simple/remove-by-example,removes documents by example}
///
/// @REST{PUT /_api/simple/remove-by-example}
///
/// This will find all documents in the collection that match the specified 
/// example object. 
///
/// The call expects a JSON hash array as body with the following attributes:
///
/// - @LIT{collection}: The name of the collection to remove from.
///
/// - @LIT{example}: An example object that all collection objects are compared
///   against.
///
/// - @LIT{waitForSync}: if set to true, then all removal operations will 
///   instantly be synchronised to disk. If this is not specified, then the
///   collection's default sync behavior will be applied.
///
/// - @LIT{limit}: an optional value that determines how many documents to 
///   delete at most. If @LIT{limit} is specified but is less than the number
///   of documents in the collection, it is undefined which of the documents 
///   will be deleted.
///
/// Returns the number of documents that were deleted
///
/// @EXAMPLES
///
/// @verbinclude api-simple-remove-by-example
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
        else if (typeof example !== "object") {
          actions.badParameter(req, res, "example");
        }
        else {
          var result = collection.removeByExample(example, body.waitForSync, limit);
          actions.resultOk(req, res, actions.HTTP_OK, { deleted: result });
        }
      }
    }
    catch (err) {
      actions.resultException(req, res, err);
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
/// @REST{PUT /_api/simple/replace-by-example}
///
/// This will find all documents in the collection that match the specified 
/// example object, and replace the entire document body with the new value
/// specified. Note that document meta-attributes such as @LIT{_id}, @LIT{_key},
/// @LIT{_from}, @LIT{_to} etc. cannot be replaced. 
///
/// The call expects a JSON hash array as body with the following attributes:
///
/// - @LIT{collection}: The name of the collection to replace within.
///
/// - @LIT{example}: An example object that all collection objects are compared
///   against.
///
/// - @LIT{newValue}: The replacement document that will get inserted in place
///   of the "old" documents.
///
/// - @LIT{waitForSync}: if set to true, then all removal operations will 
///   instantly be synchronised to disk. If this is not specified, then the
///   collection's default sync behavior will be applied.
///
/// - @LIT{limit}: an optional value that determines how many documents to 
///   replace at most. If @LIT{limit} is specified but is less than the number
///   of documents in the collection, it is undefined which of the documents 
///   will be replaced.
///
/// Returns the number of documents that were replaced
///
/// @EXAMPLES
///
/// @verbinclude api-simple-replace-by-example
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
        else if (typeof example !== "object") {
          actions.badParameter(req, res, "example");
        }
        else if (typeof newValue !== "object") {
          actions.badParameter(req, res, "newValue");
        }
        else {
          var result = collection.replaceByExample(example, newValue, body.waitForSync, limit);
          actions.resultOk(req, res, actions.HTTP_OK, { replaced: result });
        }
      }
    }
    catch (err) {
      actions.resultException(req, res, err);
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
/// @REST{PUT /_api/simple/update-by-example}
///
/// This will find all documents in the collection that match the specified 
/// example object, and partially update the document body with the new value
/// specified. Note that document meta-attributes such as @LIT{_id}, @LIT{_key},
/// @LIT{_from}, @LIT{_to} etc. cannot be replaced. 
///
/// The call expects a JSON hash array as body with the following attributes:
///
/// - @LIT{collection}: The name of the collection to update within.
///
/// - @LIT{example}: An example object that all collection objects are compared
///   against.
///
/// - @LIT{newValue}: The update value that will get inserted in place
///   of the "old" version of the found documents.
///
/// - @LIT{keepNull}: This parameter can be used to modify the behavior when
///   handling @LIT{null} values. Normally, @LIT{null} values are stored in the
///   database. By setting the @FA{keepNull} parameter to @LIT{false}, this 
///   behavior can be changed so that all attributes in @FA{data} with @LIT{null} 
///   values will be removed from the updated document.
///
/// - @LIT{waitForSync}: if set to true, then all removal operations will 
///   instantly be synchronised to disk. If this is not specified, then the
///   collection's default sync behavior will be applied.
///
/// - @LIT{limit}: an optional value that determines how many documents to 
///   update at most. If @LIT{limit} is specified but is less than the number
///   of documents in the collection, it is undefined which of the documents 
///   will be updated.
///
/// Returns the number of documents that were updated
///
/// @EXAMPLES
///
/// @verbinclude api-simple-update-by-example
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
        else if (typeof example !== "object") {
          actions.badParameter(req, res, "example");
        }
        else if (typeof newValue !== "object") {
          actions.badParameter(req, res, "newValue");
        }
        else {
          var result = collection.updateByExample(example, newValue, keepNull, body.waitForSync, limit);
          actions.resultOk(req, res, actions.HTTP_OK, { updated: result });
        }
      }
    }
    catch (err) {
      actions.resultException(req, res, err);
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
