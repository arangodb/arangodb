////////////////////////////////////////////////////////////////////////////////
/// @brief querying and managing collections
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
var API = "_api/collection";

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoAPI
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief collection representation
////////////////////////////////////////////////////////////////////////////////

function CollectionRepresentation (collection, showProperties, showCount, showFigures) {
  var result = {};

  result.id = collection._id;
  result.name = collection.name();

  if (showProperties) {
    var properties = collection.properties();

    result.waitForSync = properties.waitForSync;
    result.journalSize = properties.journalSize;
  }

  if (showCount) {
    result.count = collection.count();
  }

  if (showFigures) {
    var figures = collection.figures();

    if (figures) {
      result.figures = figures;
    }
  }

  result.status = collection.status();

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoAPI
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a collection
///
/// @RESTHEADER{POST /_api/collection,creates a collection}
///
/// @REST{POST /_api/collection}
///
/// Creates an new collection with a given name. The request must contain an
/// object with the following attributes.
///
/// - @LIT{name}: The name of the collection.
///
/// - @LIT{waitForSync} (optional, default: false): If @LIT{true} then
///   the data is synchronised to disk before returning from a create or
///   update of an document.
///
/// - @LIT{journalSize} (optional, default is a @ref
///   CommandLineArango "configuration parameter"): The maximal size of
///   a journal or datafile.  Note that this also limits the maximal
///   size of a single object. Must be at least 1MB.
///
/// - @LIT{isSystem} (optional, default is @LIT{false}): If true, create a
///   system collection. In this case @FA{collection-name} should start with
///   an underscore.
///
/// @EXAMPLES
///
/// @verbinclude api-collection-create-collection
////////////////////////////////////////////////////////////////////////////////

function POST_api_collection (req, res) {
  var body = actions.getJsonBody(req, res);

  if (body === undefined) {
    return;
  }

  if (! body.hasOwnProperty("name")) {
    actions.resultBad(req, res, actions.ERROR_ARANGO_ILLEGAL_NAME,
                      "name must be non-empty");
    return;
  }

  var name = body.name;
  var parameter = { waitForSync : false };
  var cid = undefined;

  if (body.hasOwnProperty("waitForSync")) {
    parameter.waitForSync = body.waitForSync;
  }

  if (body.hasOwnProperty("journalSize")) {
    parameter.journalSize = body.journalSize;
  }

  if (body.hasOwnProperty("isSystem")) {
    parameter.isSystem = body.isSystem;
  }

  if (body.hasOwnProperty("_id")) {
    cid = body._id;
  }

  try {
    var collection = internal.db._create(name, parameter, cid);

    var result = {};
    var headers = {};

    result.id = collection._id;
    result.name = collection.name();
    result.waitForSync = parameter.waitForSync;
    result.status = collection.status();

    headers.location = "/" + API + "/" + collection._id;
      
    actions.resultOk(req, res, actions.HTTP_OK, result, headers);
  }
  catch (err) {
    actions.resultException(req, res, err);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns all collections
///
/// @RESTHEADER{GET /_api/collection,reads all collections}
///
/// @REST{GET /_api/collection}
///
/// Returns an object with an attribute @LIT{collections} containing a 
/// list of all collection descriptions. The same information is also
/// available in the @LIT{names} as hash map with the collection names
/// as keys.
///
/// @EXAMPLES
///
/// Return information about all collections:
///
/// @verbinclude api-collection-all-collections
////////////////////////////////////////////////////////////////////////////////

function GET_api_collections (req, res) {
  var list = [];
  var names = {};
  var collections = internal.db._collections();

  for (var i = 0;  i < collections.length;  ++i) {
    var collection = collections[i];
    var rep = CollectionRepresentation(collection);
    
    list.push(rep);
    names[rep.name] = rep;
  }

  var result = { collections : list, names : names };
  
  actions.resultOk(req, res, actions.HTTP_OK, result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns a collection
///
/// @RESTHEADER{GET /_api/collection,reads a collection}
///
/// @REST{GET /_api/collection/@FA{collection-identifier}}
//////////////////////////////////////////////////////////
///
/// The result is an objects describing the collection with the following
/// attributes:
///
/// - @LIT{id}: The identifier of the collection.
///
/// - @LIT{name}: The name of the collection.
///
/// - @LIT{status}: The status of the collection as number.
///  - 1: new born collection
///  - 2: unloaded
///  - 3: loaded
///  - 4: in the process of being unloaded
///  - 5: deleted
///
/// Every other status indicates a corrupted collection.
///
/// If the @FA{collection-identifier} is unknown, then a @LIT{HTTP 404} is
/// returned.
///
/// It is possible to specify a name instead of an identifier.  In this case the
/// response will contain a field "Location" which contains the correct
/// location.
///
/// @REST{GET /_api/collection/@FA{collection-identifier}/properties}
////////////////////////////////////////////////////////////////////
///
/// In addition to the above, the result will always contain the
/// @LIT{waitForSync} and the @LIT{journalSize} properties. This is
/// achieved by forcing a load of the underlying collection.
///
/// - @LIT{waitForSync}: If @LIT{true} then creating or changing a
///   document will wait until the data has been synchronised to disk.
///
/// - @LIT{journalSize}: The maximal size of a journal / datafile.
///
/// @REST{GET /_api/collection/@FA{collection-identifier}/count}
////////////////////////////////////////////////////////////////
///
/// In addition to the above, the result also contains the number of documents.
/// Note that this will always load the collection into memory.
///
/// - @LIT{count}: The number of documents inside the collection.
///
/// @REST{GET /_api/collection/@FA{collection-identifier}/figures}
//////////////////////////////////////////////////////////////////
///
/// In addition to the above, the result also contains the number of documents
/// and additional statistical information about the collection.  Note that this
/// will always load the collection into memory.
///
/// - @LIT{count}: The number of documents inside the collection.
///
/// - @LIT{figures.alive.count}: The number of living documents.
///
/// - @LIT{figures.alive.size}: The total size in bytes used by all
///   living documents.
///
/// - @LIT{figures.dead.count}: The number of dead documents.
///
/// - @LIT{figures.dead.size}: The total size in bytes used by all
///   dead documents.
///
/// - @LIT{figures.dead.deletion}: The total number of deletion markers.
///
/// - @LIT{figures.datafiles.count}: The number of active datafiles.
/// - @LIT{figures.datafiles.fileSize}: The total filesize of datafiles.
///
/// - @LIT{figures.journals.count}: The number of journal files.
/// - @LIT{figures.journals.fileSize}: The total filesize of journal files.
///
/// - @LIT{journalSize}: The maximal size of the journal in bytes.
///
/// Note: the filesizes of shapes and compactor files are not reported.
///
/// @EXAMPLES
/////////////
///
/// Using an identifier:
///
/// @verbinclude api-collection-get-collection-identifier
///
/// Using a name:
///
/// @verbinclude api-collection-get-collection-name
///
/// Using an identifier and requesting the number of documents:
///
/// @verbinclude api-collection-get-collection-count
///
/// Using an identifier and requesting the figures of the collection:
///
/// @verbinclude api-collection-get-collection-figures
////////////////////////////////////////////////////////////////////////////////

function GET_api_collection (req, res) {

  // .............................................................................
  // /_api/collection
  // .............................................................................

  if (req.suffix.length === 0) {
    GET_api_collections(req, res);
    return;
  }

  var name = decodeURIComponent(req.suffix[0]);
  var id = parseInt(name) || name;
  var collection = internal.db._collection(id);

  if (collection === null) {
    actions.collectionNotFound(req, res, name);
    return;
  }

  // .............................................................................
  // /_api/collection/<identifier>
  // .............................................................................

  if (req.suffix.length === 1) {
    var result = CollectionRepresentation(collection, false, false, false);
    var headers = { location : "/" + API + "/" + collection._id };

    actions.resultOk(req, res, actions.HTTP_OK, result, headers);
  }

  else if (req.suffix.length === 2) {
    var sub = decodeURIComponent(req.suffix[1]);

    // .............................................................................
    // /_api/collection/<identifier>/figures
    // .............................................................................

    if (sub === "figures") {
      var result = CollectionRepresentation(collection, true, true, true);
      var headers = { location : "/" + API + "/" + collection._id + "/figures" };

      actions.resultOk(req, res, actions.HTTP_OK, result, headers);
    }

    // .............................................................................
    // /_api/collection/<identifier>/count
    // .............................................................................

    else if (sub === "count") {
      var result = CollectionRepresentation(collection, true, true, false);
      var headers = { location : "/" + API + "/" + collection._id + "/count" };

      actions.resultOk(req, res, actions.HTTP_OK, result, headers);
    }

    // .............................................................................
    // /_api/collection/<identifier>/properties
    // .............................................................................

    else if (sub === "properties") {
      var result = CollectionRepresentation(collection, true, false, false);
      var headers = { location : "/" + API + "/" + collection._id + "/properties" };

      actions.resultOk(req, res, actions.HTTP_OK, result, headers);
    }

    // .............................................................................
    // /_api/collection/<identifier>/parameter (DEPRECATED)
    // .............................................................................

    else if (sub === "parameter") {
      var result = CollectionRepresentation(collection, true, false, false);
      var headers = { location : "/" + API + "/" + collection._id + "/parameter" };

      actions.resultOk(req, res, actions.HTTP_OK, result, headers);
    }

    else {
      actions.resultNotFound(req, res, "expecting one of the resources 'count', 'figures', 'properties', 'parameter'");
    }
  }
  else {
    actions.resultBad(req, res, actions.ERROR_HTTP_BAD_PARAMETER,
                      "expect GET /" + API + "/<collection-identifer>/<method>");
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief loads a collection
///
/// @RESTHEADER{PUT /_api/collection/.../load,loads a collection}
///
/// @REST{PUT /_api/collection/@FA{collection-identifier}/load}
///
/// Loads a collection into memory.  On success an object with the following
///
/// - @LIT{id}: The identifier of the collection.
///
/// - @LIT{name}: The name of the collection.
///
/// - @LIT{count}: The number of documents inside the collection.
///
/// - @LIT{status}: The status of the collection as number.
///
/// If the @FA{collection-identifier} is missing, then a @LIT{HTTP 400} is
/// returned.  If the @FA{collection-identifier} is unknown, then a @LIT{HTTP
/// 404} is returned.
///
/// It is possible to specify a name instead of an identifier.
///
/// @EXAMPLES
///
/// @verbinclude api-collection-identifier-load
////////////////////////////////////////////////////////////////////////////////

function PUT_api_collection_load (req, res, collection) {
  try {
    collection.load();

    var result = CollectionRepresentation(collection, false, true, false);
    
    actions.resultOk(req, res, actions.HTTP_OK, result);
  }
  catch (err) {
    actions.resultException(req, res, err);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief unloads a collection
///
/// @RESTHEADER{PUT /_api/collection/.../unload,unloads a collection}
///
/// @REST{PUT /_api/collection/@FA{collection-identifier}/unload}
///
/// Removes a collection from memory. This call does not delete any documents.
/// You can use the collection afterwards; in which case it will be loaded into
/// memory, again. On success an object with the following
///
/// - @LIT{id}: The identifier of the collection.
///
/// - @LIT{name}: The name of the collection.
///
/// - @LIT{status}: The status of the collection as number.
///
/// If the @FA{collection-identifier} is missing, then a @LIT{HTTP 400} is
/// returned.  If the @FA{collection-identifier} is unknown, then a @LIT{HTTP
/// 404} is returned.
///
/// @EXAMPLES
///
/// @verbinclude api-collection-identifier-unload
////////////////////////////////////////////////////////////////////////////////

function PUT_api_collection_unload (req, res, collection) {
  try {
    collection.unload();

    var result = CollectionRepresentation(collection);
    
    actions.resultOk(req, res, actions.HTTP_OK, result);
  }
  catch (err) {
    actions.resultException(req, res, err);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief truncates a collection
///
/// @RESTHEADER{PUT /_api/collection/.../truncate,truncates a collection}
///
/// @REST{PUT /_api/collection/@FA{collection-identifier}/truncate}
///
/// Removes all documents from the collection, but leaves the indexes intact.
///
/// @EXAMPLES
///
/// @verbinclude api-collection-identifier-truncate
////////////////////////////////////////////////////////////////////////////////

function PUT_api_collection_truncate (req, res, collection) {
  try {
    collection.truncate();

    var result = CollectionRepresentation(collection);
    
    actions.resultOk(req, res, actions.HTTP_OK, result);
  }
  catch (err) {
    actions.resultException(req, res, err);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief changes a collection
///
/// @RESTHEADER{PUT /_api/collection/.../properties,changes the properties of a collection}
///
/// @REST{PUT /_api/collection/@FA{collection-identifier}/properties}
///
/// Changes the properties of a collection. Expects an object with the
/// attribute(s)
///
/// - @LIT{waitForSync}: If @LIT{true} then creating or changing a
///   document will wait until the data has been synchronised to disk.
///
/// If returns an object with the attributes
///
/// - @LIT{id}: The identifier of the collection.
///
/// - @LIT{name}: The name of the collection.
///
/// - @LIT{waitForSync}: The new value.
///
/// @EXAMPLES
///
/// @verbinclude api-collection-identifier-properties-sync
////////////////////////////////////////////////////////////////////////////////

function PUT_api_collection_properties (req, res, collection) {
  var body = actions.getJsonBody(req, res);

  if (body === undefined) {
    return;
  }

  try {
    collection.properties(body);

    var result = CollectionRepresentation(collection, true);
    
    actions.resultOk(req, res, actions.HTTP_OK, result);
  }
  catch (err) {
    actions.resultException(req, res, err);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief renames a collection
///
/// @RESTHEADER{PUT /_api/collection/.../rename,renames a collection}
///
/// @REST{PUT /_api/collection/@FA{collection-identifier}/rename}
///
/// Renames a collection. Expects an object with the attribute(s)
///
/// - @LIT{name}: The new name.
///
/// If returns an object with the attributes
///
/// - @LIT{id}: The identifier of the collection.
///
/// - @LIT{name}: The new name of the collection.
///
/// @EXAMPLES
///
/// @verbinclude api-collection-identifier-rename
////////////////////////////////////////////////////////////////////////////////

function PUT_api_collection_rename (req, res, collection) {
  var body = actions.getJsonBody(req, res);

  if (body === undefined) {
    return;
  }

  if (! body.hasOwnProperty("name")) {
    actions.resultBad(req, res, actions.ERROR_ARANGO_ILLEGAL_NAME,
                      "name must be non-empty");
    return;
  }

  var name = body.name;

  try {
    collection.rename(name);

    var result = CollectionRepresentation(collection);
    
    actions.resultOk(req, res, actions.HTTP_OK, result);
  }
  catch (err) {
    actions.resultException(req, res, err);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief changes a collection
////////////////////////////////////////////////////////////////////////////////

function PUT_api_collection (req, res) {

  if (req.suffix.length != 2) {
    actions.resultBad(req, res, actions.ERROR_HTTP_BAD_PARAMETER,
                      "expected PUT /" + API + "/<collection-identifer>/<action>");
    return;
  }

  var name = decodeURIComponent(req.suffix[0]);
  var id = parseInt(name) || name;
  var collection = internal.db._collection(id);
    
  if (collection === null) {
    actions.collectionNotFound(req, res, name);
    return;
  }

  var sub = decodeURIComponent(req.suffix[1]);

  if (sub === "load") {
    PUT_api_collection_load(req, res, collection);
  }
  else if (sub === "unload") {
    PUT_api_collection_unload(req, res, collection);
  }
  else if (sub === "truncate") {
    PUT_api_collection_truncate(req, res, collection);
  }
  else if (sub === "properties") {
    PUT_api_collection_properties(req, res, collection);
  }
  else if (sub === "rename") {
    PUT_api_collection_rename(req, res, collection);
  }
  else {
    actions.resultNotFound(req, res, "expecting one of the actions 'load', 'unload', 'truncate', 'properties', 'rename'");
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief deletes a collection
///
/// @RESTHEADER{DELETE /_api/collection,deletes a collection}
///
/// @REST{DELETE /_api/collection/@FA{collection-identifier}}
///
/// Deletes a collection identified by @FA{collection-identified}.
///
/// If the collection was successfully deleted then, an object is returned with
/// the following attributes:
///
/// - @LIT{error}: @LIT{false}
///
/// - @LIT{id}: The identifier of the deleted collection.
///
/// If the @FA{collection-identifier} is missing, then a @LIT{HTTP 400} is
/// returned.  If the @FA{collection-identifier} is unknown, then a @LIT{HTTP
/// 404} is returned.
///
/// It is possible to specify a name instead of an identifier. 
///
/// @EXAMPLES
///
/// Using an identifier:
///
/// @verbinclude api-collection-delete-collection-identifier
///
/// Using a name:
///
/// @verbinclude api-collection-delete-collection-name
////////////////////////////////////////////////////////////////////////////////

function DELETE_api_collection (req, res) {
  if (req.suffix.length != 1) {
    actions.resultBad(req, res, actions.ERROR_HTTP_BAD_PARAMETER,
                      "expected DELETE /" + API + "/<collection-identifer>");
  }
  else {
    var name = decodeURIComponent(req.suffix[0]);
    var id = parseInt(name) || name;
    var collection = internal.db._collection(id);
    
    if (collection === null) {
      actions.collectionNotFound(req, res, name);
    }
    else {
      try {
        var result = {
          id : collection._id
        };

        collection.drop();

        actions.resultOk(req, res, actions.HTTP_OK, result);
      }
      catch (err) {
        actions.resultException(req, res, err);
      }
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief reads or creates a collection
////////////////////////////////////////////////////////////////////////////////

actions.defineHttp({
  url : API,
  context : "api",

  callback : function (req, res) {
    if (req.requestType === actions.GET) {
      GET_api_collection(req, res);
    }
    else if (req.requestType === actions.DELETE) {
      DELETE_api_collection(req, res);
    }
    else if (req.requestType === actions.POST) {
      POST_api_collection(req, res);
    }
    else if (req.requestType === actions.PUT) {
      PUT_api_collection(req, res);
    }
    else {
      actions.resultUnsupported(req, res);
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
