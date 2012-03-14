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
/// @addtogroup AvocadoAPI
/// @{
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                  global variables
// -----------------------------------------------------------------------------

var actions = require("actions");
var simple = require("simple-query");

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

function buildDocumentFromReq(req) {
  
    //   Example requests:
    //   Header:
    //     POST /_api/key/example_collection/example_key1
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
      "key" : key,
      "value" : req.requestBody      
    }
    
    if (req.headers["x-voc-expires"] != undefined) {
      //  TODO check value
      doc["x-voc-expires"] = req.headers["x-voc-expires"];      
    }
    
    if (req.headers["x-voc-extended"] != undefined) {
      var json = JSON.parse(req.headers["x-voc-extended"]);
      if (json != undefined) {
        doc["x-voc-extended"] = json;
      }
    }
    
    doc["x-voc-created"] = internal.time();
    
    return doc;
}


////////////////////////////////////////////////////////////////////////////////
/// @brief create a key value pair
////////////////////////////////////////////////////////////////////////////////

function postKeyValue(req, res) {
  if (req.suffix.length < 2) {
    actions.actionResultError (req, res, 404, actions.keyValueNotModified, "Key value pair not created. Missing key."); 
    return;
  }

  try {
    var collection = req.suffix[0];
    
    var doc = buildDocumentFromReq(req);

    var s = db[collection].select({ "key" : doc.key });
    s.execute();
        
    if (s._countTotal != 0) {
      actions.actionResultError (req, res, 404, actions.keyValueNotModified, "Use PUT to change value");      
    }
    else {
      var id = db[collection].save(doc);    
      var result = {
        "saved" : true,
        "_id" : id
      }
      actions.actionResultOK(req, res, 201, result);      
    }
  }
  catch (e) {
    actions.actionResultError (req, res, 404, actions.keyValueNotModified, "Key value pair not created.");
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief update the value of a key value pair
////////////////////////////////////////////////////////////////////////////////

function putKeyValue(req, res) {
  if (req.suffix.length < 2) {
    actions.actionResultError (req, res, 404, actions.keyValueNotFound, "Key value pair not found");
    return;
  }

  try {
    var collection = req.suffix[0];
    
    var doc = buildDocumentFromReq(req);

    var s = db[collection].select({ "key" : doc.key });
    s.execute();
        
    if (s._countTotal < 1) {
      actions.actionResultError (req, res, 404, actions.keyValueNotFound, "Key value pair not found");      
    }
    else if (s._countTotal > 1) {
      actions.actionResultError (req, res, 404, actions.keyValueNotFound, "Key value pair not found. Wrong key?");
    }
    else {
      var id = s._execution._documents[0]._id;
      if (db[collection].replace(id, doc)) {            
        actions.actionResultOK(req, res, 202, { "changed" : true });                              
      }
      else {
        actions.actionResultError(req, res, 404, actions.keyValueNotFound, "Value not changed");        
      }
    }
  }
  catch (e) {
    actions.actionResultError (req, res, 404, actions.keyValueNotFound, "Key value pair not found");
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief delete an existing key value pair
////////////////////////////////////////////////////////////////////////////////

function deleteKeyValue(req, res) {
  if (req.suffix.length < 2) {
    actions.actionResultError (req, res, 404, actions.keyValueNotFound, "Key value pair not found");
    return;
  }

  try {
    var collection = req.suffix[0];
    
    var key = req.suffix[1];
    
    for (var i = 2; i < req.suffix.length; ++i) {
      key += "/" + req.suffix[i];
    }

    var s = db[collection].select({ "key" : key });
    s.execute();
    
    if (s._countTotal < 1) {
      actions.actionResultError (req, res, 404, actions.keyValueNotFound, "Key value pair not found");      
    }
    else if (s._countTotal > 1) {
      actions.actionResultError (req, res, 404, actions.keyValueNotFound, "Key value pair not found. Wrong key?");
    }
    else {
      var id = s._execution._documents[0]._id;
      if (db[collection].delete(id)) {            
        actions.actionResultOK(req, res, 202, { "removed" : true });                              
      }
      else {
        actions.actionResultError(req, res, 404, actions.keyValueNotFound, "Value not removed");        
      }
    }
  }
  catch (e) {
    actions.actionResultError (req, res, 404, actions.keyValueNotFound, "Key value pair not found");
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return an existing key value pair
////////////////////////////////////////////////////////////////////////////////

function getKeyValue(req, res) {
  if (req.suffix.length < 2) {
    actions.actionResultError (req, res, 404, actions.keyValueNotFound, "Key value pair not found");
    return;
  }

  try {
    var collection = req.suffix[0];
    
    var key = req.suffix[1];
    
    for (var i = 2; i < req.suffix.length; ++i) {
      key += "/" + req.suffix[i];
    }
        
    var s = db[collection].select({ "key" : key });
    s.execute();
    
    if (s._countTotal < 1) {
      actions.actionResultError (req, res, 404, actions.keyValueNotFound, "Key value pair not found");      
    }
    else if (s._countTotal > 1) {
      actions.actionResultError (req, res, 404, actions.keyValueNotFound, "Key value pair not found. Wrong key?");
    }
    else {
      var headers = {};
      
      if (s._execution._documents[0]["x-voc-expires"] != undefined) {
        headers["x-voc-expires"] = s._execution._documents[0]["x-voc-expires"];
      }
      if (s._execution._documents[0]["x-voc-extended"] != undefined) {
        headers["x-voc-extended"] = JSON.stringify(s._execution._documents[0]["x-voc-extended"]);
      }
      if (s._execution._documents[0]["x-voc-created"] != undefined) {
        headers["x-voc-created"] = s._execution._documents[0]["x-voc-created"];
      }
      
      actions.actionResultOK(req, res, 200, s._execution._documents[0].value, headers);                      
    }
  }
  catch (e) {
    actions.actionResultError (req, res, 404, actions.keyValueNotFound, "Key value pair not found");
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       initialiser
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief cursor actions gateway 
////////////////////////////////////////////////////////////////////////////////

actions.defineHttp({
  url : "_api/key",
  context : "api",

  callback : function (req, res) {
    switch (req.requestType) {
      case ("POST") : 
        postKeyValue(req, res); 
        break;

      case ("GET") : 
        getKeyValue(req, res); 
        break;

      case ("PUT") :  
        putKeyValue(req, res); 
        break;

      case ("DELETE") :  
        deleteKeyValue(req, res); 
        break;

      default:
        actions.actionResultUnsupported(req, res);
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
