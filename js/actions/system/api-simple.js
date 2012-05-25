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

var actions = require("actions");
var simple = require("simple-query");
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
/// Returns all documents of a collections. The call expects an JSON object
/// as body with the following attributes:
///
/// - @LIT{collection}: The identifier or name of the collection to query.
///
/// - @LIT{skip}: The documents to skip in the query. (optional)
///
/// - @LIT{limit}: The maximal amount of documents to return. The @LIT{skip}
///   is applied before the @LIT{limit} restriction. (optional)
///
/// Returns a cursor containing the result, see @ref HttpCursor for details.
///
/// @EXAMPLES
///
/// Limit the amount of documents using
///
/// @verbinclude api-simple-all-skip-limit
////////////////////////////////////////////////////////////////////////////////

actions.defineHttp({
  url : API + "all",
  context : "api",

  callback : function (req, res) {
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
      var id = parseInt(name) || name;
      var collection = internal.db._collection(id);

      if (collection === null) {
        actions.collectionNotFound(req, res, name);
      }
      else {
        try {
          var result = collection.all();

          if (skip !== null && skip !== undefined) {
            result = result.skip(skip);
          }

          if (limit !== null && limit !== undefined) {
            result = result.limit(limit);
          }

          actions.resultCursor(req, res, CREATE_CURSOR(result.toArray(), true));
        }
        catch (err) {
          actions.resultException(req, res, err);
        }
      }
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
/// - @LIT{collection}: The identifier or name of the collection to query.
///
/// - @LIT{latitude}: The latitude of the coordinate.
///
/// - @LIT{longitude}: The longitude of the coordinate.
///
/// - @LIT{distance}: If given, the attribute key used to store the
///   distance. (optional)
///
/// - @LIT{skip}: The documents to skip in the query. (optional)
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

      var name = body.collection;
      var id = parseInt(name) || name;
      var collection = internal.db._collection(id);

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
        try {
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

          actions.resultCursor(req, res, CREATE_CURSOR(result.toArray(), true));
        }
        catch (err) {
          actions.resultException(req, res, err);
        }
      }
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
/// - @LIT{collection}: The identifier or name of the collection to query.
///
/// - @LIT{latitude}: The latitude of the coordinate.
///
/// - @LIT{longitude}: The longitude of the coordinate.
///
/// - @LIT{radius}: The maximal radius.
///
/// - @LIT{distance}: If given, the attribute key used to store the
///   distance. (optional)
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
      var id = parseInt(name) || name;
      var collection = internal.db._collection(id);

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
        try {
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
          
          actions.resultCursor(req, res, CREATE_CURSOR(result.toArray(), true));
        }
        catch (err) {
          actions.resultException(req, res, err);
        }
      }
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
/// - @LIT{collection}: The identifier or name of the collection to query.
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

      var name = body.collection;
      var id = parseInt(name) || name;
      var collection = internal.db._collection(id);

      if (collection === null) {
        actions.collectionNotFound(req, res, name);
      }
      else if (typeof example !== "object") {
        actions.badParameter(req, res, "example");
      }
      else {
        try {
          var result = collection.byExample(example);

          if (skip !== null && skip !== undefined) {
            result = result.skip(skip);
          }

          if (limit !== null && limit !== undefined) {
            result = result.limit(limit);
          }

          actions.resultCursor(req, res, CREATE_CURSOR(result.toArray(), true));
        }
        catch (err) {
          actions.resultException(req, res, err);
        }
      }
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
/// - @LIT{collection}: The identifier or name of the collection to query.
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
      var id = parseInt(name) || name;
      var collection = internal.db._collection(id);

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
          actions.resultNotFound(req, res, "no match");
        }
      }
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

      var name = body.collection;
      var id = parseInt(name) || name;
      var collection = internal.db._collection(id);

      if (collection === null) {
        actions.collectionNotFound(req, res, name);
      }
      else if (typeof example !== "object") {
        actions.badParameter(req, res, "example");
      }
      else {
        try {
          var result = collection.BY_EXAMPLE_HASH(index, example, skip, limit);

          actions.resultOk(req, res, actions.HTTP_OK, result);
        }
        catch (err) {
          actions.resultException(req, res, err);
        }
      }
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
/// - @LIT{collection}: The identifier or name of the collection to query.
///
/// - @LIT{attribute}: The attribute path to check.
///
/// - @LIT{left}: The lower bound.
///
/// - @LIT{right}: The upper bound.
///
/// - @LIT{closed}: If true, use intervall including @LIT{left} and @LIT{right},
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

      var name = body.collection;
      var id = parseInt(name) || name;
      var collection = internal.db._collection(id);

      if (collection === null) {
        actions.collectionNotFound(req, res, name);
      }
      else {
        try {
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

          actions.resultCursor(req, res, CREATE_CURSOR(result.toArray(), true));
        }
        catch (err) {
          actions.resultException(req, res, err);
        }
      }
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
