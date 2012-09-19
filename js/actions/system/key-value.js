////////////////////////////////////////////////////////////////////////////////
/// @brief query results key value actions
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
/// @author Jan Steemann
/// @author Copyright 2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoAPI
/// @{
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                  global variables
// -----------------------------------------------------------------------------

var actions = require("org/arangodb/actions");
var simple = require("simple-query");

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

function formatTimeStamp (timestamp) {  
    var d = new Date(timestamp * 1000);
    
    var year = d.getUTCFullYear();
    var month = d.getUTCMonth() + 1;
    var date =  d.getUTCDate();

    if (month < 10) month = "0" + month;
    if (date < 10) date = "0" + date;

    var hour = d.getUTCHours();
    var minutes = d.getUTCMinutes();
    var seconds = d.getUTCSeconds();

    if (hour < 10) hour = "0" + hour;
    if (minutes < 10) minutes = "0" + minutes;
    if (seconds < 10) seconds = "0" + seconds;
    
    return year + "-" + month + "-" + date + "T" + hour + ":" + minutes + ":" + seconds + "Z";    
}

function buildDocumentFromReq(req) {
  
    //   Example requests:
    //   Header:
    //     POST /_api/key/example_collection/example_key1 HTTP/1.1
    //     Host: localhost:9000
    //     x-voc-expires: 2011-09-29T08:00:00Z
    //     x-voc-extended: {"option1":35,"option2":"x"}
    //   Body:
    //     12
    //
    //  
  
    var key = req.suffix[1];
    
    for (var i = 2; i < req.suffix.length; ++i) {
      key += "/" + req.suffix[i];
    }

    var doc = {
      "$key" : key,
      "$value" : req.requestBody      
    }
    
    if (req.headers["x-voc-expires"] != undefined) {
      var d = new Date(req.headers["x-voc-expires"]);
      // store time stamp as double
      doc["$expires"] = d.getTime() / 1000;      
    }
    
    if (req.headers["x-voc-extended"] != undefined) {
      var json = JSON.parse(req.headers["x-voc-extended"]);
      if (json != undefined) {
        doc["$extended"] = json;
      }
    }
    
    // store time stamp as double
    doc["$created"] = internal.time();
    
    return doc;
}


////////////////////////////////////////////////////////////////////////////////
/// @brief create a key value pair
///
/// @REST{POST /_api/key/@FA{collection-name}/@FA{key}}
/// {data}
///
/// headers "x-voc-expires" and "x-voc-extended"
////////////////////////////////////////////////////////////////////////////////

function postKeyValue(req, res) {
  if (req.suffix.length < 2) {
    actions.resultBad(req, res, actions.ERROR_KEYVALUE_INVALID_KEY, actions.getErrorMessage(actions.ERROR_KEYVALUE_INVALID_KEY));
    return;
  }

  try {
    var collection = req.suffix[0];
    
    if (internal.db._collection(collection) === null) {
      actions.collectionNotFound(req, res, collection);
      return;      
    }

    if (req.requestBody == "") {
      actions.resultError(req, res, actions.HTTP_BAD, actions.ERROR_KEYVALUE_NO_VALUE, actions.getErrorMessage(actions.ERROR_KEYVALUE_NO_VALUE));
      return;
    }
  
    var doc = buildDocumentFromReq(req);

    var oldDoc = internal.db._collection(collection).firstExample("$key", doc["$key"]);
        
    if (oldDoc != undefined) {
      actions.resultError(req, res, actions.HTTP_BAD, actions.ERROR_KEYVALUE_KEY_EXISTS, actions.getErrorMessage(actions.ERROR_KEYVALUE_KEY_EXISTS));
    }
    else {
      var id = db[collection].save(doc);    
      var result = {
        "saved" : true,
        "_id" : id
      }
      actions.resultOk(req, res, actions.HTTP_CREATED, result);      
    }
  }
  catch (err) {
    actions.resultException(req, res, err);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief update the value of a key value pair
///
/// @REST{PUT /_api/key/@FA{collection-name}/@FA{key}}
/// {data}
///
////////////////////////////////////////////////////////////////////////////////

function putKeyValue(req, res) {
  if (req.suffix.length < 2) {
    actions.resultBad(req, res, actions.ERROR_KEYVALUE_INVALID_KEY, actions.getErrorMessage(actions.ERROR_KEYVALUE_INVALID_KEY));
    return;
  }

  try {
    var collection = req.suffix[0];
    
    if (internal.db._collection(collection) === null) {
      actions.collectionNotFound(req, res);
      return;      
    }
    
    var doc = buildDocumentFromReq(req);

    var oldDoc = internal.db._collection(collection).firstExample("$key", doc["$key"]);
        
    if (oldDoc == undefined) {
      if (req.parameters["create"] == 1) {
        var id = db[collection].save(doc);    
        var result = {
          "saved" : true,
          "_id" : id
        }
        actions.resultOk(req, res, actions.HTTP_CREATED, result);      
        return;
      }
      actions.resultError(req, res, actions.HTTP_NOT_FOUND, actions.ERROR_KEYVALUE_KEY_NOT_FOUND, actions.getErrorMessage(actions.ERROR_KEYVALUE_KEY_NOT_FOUND));
    }
    else {
      // get _id
      var id = oldDoc._id;
      
      // save x-voc-created
      var created = oldDoc["$created"];      
      if (created != undefined) {
        doc["x-voc-created"] = created;        
      }
      
      // replace the document
      if (db[collection].replace(id, doc)) {            
        actions.resultOk(req, res, actions.HTTP_ACCEPTED, {"changed" : true});                              
      }
      else {
        actions.resultError(req, res, actions.HTTP_BAD, actions.ERROR_KEYVALUE_KEY_NOT_CHANGED, actions.getErrorMessage(actions.ERROR_KEYVALUE_KEY_NOT_CHANGED));
      }
    }
  }
  catch (err) {
    actions.resultException(req, res, err);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief delete an existing key value pair
///
/// @REST{DELETE /_api/key/@FA{collection-name}/@FA{key}}
///
////////////////////////////////////////////////////////////////////////////////

function deleteKeyValue(req, res) {
  if (req.suffix.length < 2) {
    actions.resultBad(req, res, actions.ERROR_KEYVALUE_INVALID_KEY, actions.getErrorMessage(actions.ERROR_KEYVALUE_INVALID_KEY));
    return;
  }

  try {
    var collection = req.suffix[0];
    
    if (internal.db._collection(collection) === null) {
      actions.collectionNotFound(req, res);
      return;      
    }
    
    var key = req.suffix[1];
    
    for (var i = 2; i < req.suffix.length; ++i) {
      key += "/" + req.suffix[i];
    }

    var doc = internal.db._collection(collection).firstExample("$key", key);
    
    if (doc == undefined) {
      actions.resultError(req, res, actions.HTTP_NOT_FOUND, actions.ERROR_KEYVALUE_KEY_NOT_FOUND, actions.getErrorMessage(actions.ERROR_KEYVALUE_KEY_NOT_FOUND));
    }
    else {
      var id = doc._id;
      if (db[collection].remove(id)) {            
        actions.resultOk(req, res, actions.HTTP_ACCEPTED, {"removed" : true});                              
      }
      else {
        actions.resultError(req, res, actions.HTTP_BAD, actions.ERROR_KEYVALUE_KEY_NOT_REMOVED, actions.getErrorMessage(actions.ERROR_KEYVALUE_KEY_NOT_REMOVED));
      }
    }
  }
  catch (err) {
    actions.resultException(req, res, err);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return an existing key value pair
///
/// @REST{GET /_api/key/@FA{collection-name}/@FA{key}}
///
/// returned headers: "x-voc-expires", "x-voc-extended" and "x-voc-created"
/// 
////////////////////////////////////////////////////////////////////////////////

function getKeyValue(req, res) {
  if (req.suffix.length < 2) {
    actions.resultBad(req, res, actions.ERROR_KEYVALUE_INVALID_KEY, actions.getErrorMessage(actions.ERROR_KEYVALUE_INVALID_KEY));
    return;
  }

  try {
    var collection = req.suffix[0];
    
    if (internal.db._collection(collection) === null) {
      actions.collectionNotFound(req, res);
      return;      
    }
    
    var key = req.suffix[1];
    
    for (var i = 2; i < req.suffix.length; ++i) {
      key += "/" + req.suffix[i];
    }
        
    var doc = internal.db._collection(collection).firstExample("$key", key); 
        
    if (doc == undefined) {
      actions.resultError(req, res, actions.HTTP_NOT_FOUND, actions.ERROR_KEYVALUE_KEY_NOT_FOUND, actions.getErrorMessage(actions.ERROR_KEYVALUE_KEY_NOT_FOUND));
    }
    else {
      var headers = {};
      
      if (doc["$expires"] != undefined) {
        // format timestamp
        headers["x-voc-expires"] = formatTimeStamp(doc["$expires"]);
      }
      if (doc["$extended"] != undefined) {
        // serialize header value
        headers["x-voc-extended"] = JSON.stringify(doc["$extended"]);
      }
      if (doc["$created"] != undefined) {
        // format timestamp
        headers["x-voc-created"] = formatTimeStamp(doc["$created"]);
      }
      
      actions.resultOk(req, res, actions.HTTP_OK, doc["$value"], headers);                      
    }
  }
  catch (err) {
    actions.resultException(req, res, err);
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       initialiser
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief key value pair actions gateway 
////////////////////////////////////////////////////////////////////////////////

actions.defineHttp({
  url : "_api/key",
  context : "api",

  callback : function (req, res) {
    switch (req.requestType) {
      case (actions.POST) :
        postKeyValue(req, res); 
        break;

      case (actions.GET) :
        getKeyValue(req, res); 
        break;

      case (actions.PUT) :
        putKeyValue(req, res); 
        break;

      case (actions.DELETE) :
        deleteKeyValue(req, res); 
        break;

      default:
        actions.resultUnsupported(req, res);
    }
  }
});


////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoAPI
/// @{
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief key value pair search
///
/// @REST{GET /_api/keys/@FA{collection-name}/@FA{key-prefix}}
///
////////////////////////////////////////////////////////////////////////////////

function searchKeyValue(req, res) {
  if (req.suffix.length < 2) {
    actions.resultBad(req, res, actions.ERROR_KEYVALUE_INVALID_KEY, actions.getErrorMessage(actions.ERROR_KEYVALUE_INVALID_KEY));
    return;
  }

  try {
    var collection = req.suffix[0];
    
    if (internal.db._collection(collection) === null) {
      actions.collectionNotFound(req, res);
      return;      
    }
    
    var prefix = req.suffix[1];
    
    for (var i = 2; i < req.suffix.length; ++i) {
      prefix += "/" + req.suffix[i];
    }
    
    //
    // TODO: build a query which selects the keys
    //
    
    var cursor = internal.db._collection(collection).all();
    
    result = [];
    
    while (cursor.hasNext() ) {
      var doc = cursor.next();
      if (doc["$key"] != undefined && doc["$key"].indexOf(prefix) === 0) {
        result.push(doc["$key"]);
      }
    }

    actions.resultOk(req, res, actions.HTTP_OK, result);
  }
  catch (err) {
    actions.resultException(req, res, err);
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       initialiser
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief key value pair actions gateway 
////////////////////////////////////////////////////////////////////////////////

actions.defineHttp({
  url : "_api/keys",
  context : "api",

  callback : function (req, res) {
    switch (req.requestType) {
      case (actions.GET) :
        searchKeyValue(req, res); 
        break;
        
      default:
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
