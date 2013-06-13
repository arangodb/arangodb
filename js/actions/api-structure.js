/*jslint indent: 2,
         nomen: true,
         maxlen: 100,
         sloppy: true,
         vars: true,
         white: true,
         plusplus: true,
         stupid: true */
/*global require */

////////////////////////////////////////////////////////////////////////////////
/// @brief querying and managing structures
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2013 triagens GmbH, Cologne, Germany
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
/// @author Dr. Frank Celler
/// @author Copyright 2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var arangodb = require("org/arangodb");
var console = require("console");
var actions = require("org/arangodb/actions");
var arangodb = require("org/arangodb");
var db = arangodb.db;
var ERRORS = arangodb.errors;

var DEFAULT_KEY = "default";
var API = "_api/structures";

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoAPI
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief returns a (OK) result 
////////////////////////////////////////////////////////////////////////////////

function resultOk (req, res, httpReturnCode, keyvals, headers) {  
  'use strict';

  res.responseCode = httpReturnCode;
  res.contentType = "application/json; charset=utf-8";
  res.body = JSON.stringify(keyvals);

  if (headers !== undefined && headers !== null) {
    res.headers = headers;    
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns a (error) result 
////////////////////////////////////////////////////////////////////////////////

function resultError (req, res, httpReturnCode, errorNum, errorMessage, keyvals, headers) {  
  'use strict';

  var i;

  res.responseCode = httpReturnCode;
  res.contentType = "application/json; charset=utf-8";
  
  var result = {};

  if (keyvals !== undefined) {
    for (i in keyvals) {
       if (keyvals.hasOwnProperty(i)) {
        result[i] = keyvals[i];
      }
    }
  }

  result.error        = true;
  result.code         = httpReturnCode;
  result.errorNum     = errorNum;
  result.errorMessage = errorMessage;
  
  res.body = JSON.stringify(result);

  if (headers !== undefined && headers !== null) {
    res.headers = headers;    
  }
}


////////////////////////////////////////////////////////////////////////////////
/// @brief returns true if a "if-match" or "if-none-match" errer happens
////////////////////////////////////////////////////////////////////////////////

function matchError (req, res, doc) {  

  if (req.headers["if-none-match"] !== undefined) {
    if (doc._rev === req.headers["if-none-match"]) {
      // error      
      res.responseCode = actions.HTTP_NOT_MODIFIED;
      res.contentType = "application/json; charset=utf-8";
      res.body = '';
      res.headers = {};      
      return true;
    }
  }  

  if (req.headers["if-match"] !== undefined) {
    if (doc._rev !== req.headers["if-match"]) {
      // error
      resultError(req, res, actions.HTTP_PRECONDITION_FAILED, 
          arangodb.ERROR_ARANGO_CONFLICT, "wrong revision", 
          {'_id': doc._id, '_rev': doc._rev, '_key': doc._key});
      return true;
    }
  }  

  var rev = req.parameters.rev;
  if (rev !== undefined) {
    if (doc._rev !== rev) {
      // error
      resultError(req, res, actions.HTTP_PRECONDITION_FAILED, 
          arangodb.ERROR_ARANGO_CONFLICT, "wrong revision", 
          {'_id': doc._id, '_rev': doc._rev, '_key': doc._key});
      return true;
    }
  }  

  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the collection
////////////////////////////////////////////////////////////////////////////////

function getCollectionByRequest(req, res)  {
  
  if (req.suffix.length === 0) {
    // GET /_api/structure (missing collection)
    resultError(req, res, actions.HTTP_BAD, 
        arangodb.ERROR_ARANGO_COLLECTION_NOT_FOUND, "collection not found");
    return;
  }

  // TODO: check parameter createCollection=true and create collection

  return db._collection(req.suffix[0]);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief save a document
////////////////////////////////////////////////////////////////////////////////

function saveDocument(req, res, collection, document)  {
  var doc;
  var waitForSync = collection.properties().waitForSync;
  
  if (req.parameters.waitForSync) {
    waitForSync = true;
  }
    
  try {
    doc = collection.save(document, waitForSync);
  }
  catch(err) {
    resultError(req, res, actions.HTTP_BAD, 
        arangodb.ERROR_FAILED, 
        err);
    return;
  }

  var headers = {
    "Etag" :  doc._rev,
    "location" : "/_api/structures/" + doc._id
  };

  var returnCode = waitForSync ? actions.HTTP_CREATED : actions.HTTP_ACCEPTED;
  
  resultOk(req, res, returnCode, doc, headers);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the document
////////////////////////////////////////////////////////////////////////////////

function getDocumentByRequest(req, res, collection)  {
  
  if (req.suffix.length < 2) {
    resultError(req, res, actions.HTTP_BAD, 
        arangodb.ERROR_ARANGO_DOCUMENT_HANDLE_BAD, 
        "expecting GET /_api/structure/<document-handle>");
    return;
  }

  try {
    return collection.document(req.suffix[1]);
  }
  catch (err) {
    resultError(req, res, actions.HTTP_NOT_FOUND, 
        arangodb.ERROR_ARANGO_DOCUMENT_NOT_FOUND, 
        "document /_api/structure/" + req.suffix[0] + "/" + req.suffix[1] + 
        " not found"); 
  }    
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the types
////////////////////////////////////////////////////////////////////////////////

function getTypes (structure) {
  var predefinedTypes = {
    "boolean" : {
    },
    "string" : {
    },
    "number" : {
    },
    "mixed" : {
    }
  };

  var types = {
    "predefinedTypes" : predefinedTypes,
    "arrayTypes" : {},
    "objectTypes" : {}
  };
  
  if (undefined !== structure.arrayTypes) {
    types.arrayTypes = structure.arrayTypes;
  }
  
  if (undefined !== structure.objectTypes) {
    types.objectTypes = structure.objectTypes;
  }
  return types;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the request language
////////////////////////////////////////////////////////////////////////////////

function getLang (req) {
  if (undefined !== req.parameters.lang) {
    return req.parameters.lang;
  }
  
  return null;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns formatter
////////////////////////////////////////////////////////////////////////////////

function selectFormatter (formatter1, formatter2, lang) {  
   var formatter = formatter1;
   if (undefined === formatter1 || JSON.stringify(formatter1) === "{}") {
     formatter = formatter2;
   }

   if (undefined !== formatter) {
     if (undefined === lang) {
       return formatter[DEFAULT_KEY];
     }
     
     if (undefined === formatter[lang]) {
       return formatter[DEFAULT_KEY];
     }     
     return formatter[lang];
   }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the parser
////////////////////////////////////////////////////////////////////////////////

function selectParser (parser1, parser2, lang) {  
 var parser = parser1;
 if (undefined === parser1 || JSON.stringify(parser1) === "{}") {
   parser = parser2;
 }

 if (undefined !== parser) {
   if (undefined === lang) {
     return parser[DEFAULT_KEY];
   }
     
   if (undefined === parser[lang]) {
     return parser[DEFAULT_KEY];
   }     
   return parser[lang];
 }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief call a module function
////////////////////////////////////////////////////////////////////////////////

function callModuleFunction(value, moduleName, functionName, functionArgs) {  
  if (undefined === moduleName) {
    return value;
  }
  
  try {
    var formatModule = require(moduleName);
    if (formatModule.hasOwnProperty(functionName)) {
      // call the function
      return formatModule[functionName].call(null, value, functionArgs);      
    }
  }  
  catch (err) {
    // could not load module
    console.warn("module error for module: " + moduleName + " error: " + err);
    return value;
  }

  // function not found
  console.warn("module function '" + functionName + "' of module '" + moduleName 
          + "' not found.");
  
  return value;        
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the formatted value
////////////////////////////////////////////////////////////////////////////////

function formatValue (value, structure, types, lang) {
  var result;
  var key;
  var section;

  try {
    var type = types.predefinedTypes[structure.type];
    if (type) {
      console.warn("predefined type found: " + structure.type);    
      
      // TODO check type of value
      
      section = selectFormatter(structure.formatter, 
          types.predefinedTypes[structure.type].formatter, lang);
        
      if (undefined === section) {
        return value;
      }
      
      return callModuleFunction(value, 
              section.module, 
              section['do'], 
              section.args);
    }
    
    // array types
    type = types.arrayTypes[structure.type];
    if (type) {
      console.warn("array type found: " + structure.type);
      
      // TODO check type of value
      
      // check for array formatter
      section = selectFormatter(structure.formatter, undefined, lang);        
      if (undefined !== section) {
        return callModuleFunction(value, 
              section.module, 
              section['do'], 
              section.args);
      }
      
      // format each element
      result = [];
      for (key = 0; key < value.length; ++key) {
        result[key] = formatValue(value[key], type, types, lang);
      }
      return result;
    }
    
    // object types    
    type = types.objectTypes[structure.type];
    if (type) {
      console.warn("object type found: " + structure.type);

      // TODO check type of value

      // check for object formatter
      section = selectFormatter(structure.formatter, undefined, lang);        
      if (undefined !== section) {
        return callModuleFunction(value, 
              section.module, 
              section['do'], 
              section.args);
      }

      var attributes = type.attributes;
      if (undefined === attributes) {
        // no attributes
        return null;
      }
      
      // TODO check type of attribute
      
      // format each property
      result = {};
      for (key in attributes) {
        if (attributes.hasOwnProperty(key)) {
          if (value.hasOwnProperty(key)) {                    
            var subStructure = attributes[key];
            if (undefined === subStructure) {
              result[key] = value[key];
            }
            else {
              result[key] = formatValue(value[key], subStructure, types, lang);
            }
          }
          else {
            result[key] = null;
          }
        }
      }
      return result;
    }
  }
  catch (err) {
    console.warn("error = " + err);
  }

  return value;  
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the parsed value
////////////////////////////////////////////////////////////////////////////////

function parseValue (value, structure, types, lang) {
  var result;
  var key;
  var section;

  // console.warn("in parseValue");    

  try {
    var type = types.predefinedTypes[structure.type];
    if (type) {
      // console.warn("predefined type found: " + structure.type);    
      
      // TODO check type of value
      
      section = selectParser(structure.parser, 
          types.predefinedTypes[structure.type].parser, lang);

      if (undefined === section) {
        console.warn("section is undefined");
        return value;
      }

      var x = callModuleFunction(value, 
              section.module, 
              section['do'], 
              section.args);
              
      // console.warn("parsing " + value + " to " + x );
              
      return x;
    }
    
    // array types
    type = types.arrayTypes[structure.type];
    if (type) {
      console.warn("array type found: " + structure.type);
      
      // TODO check type of value
      
      // check for array formatter
      section = selectParser(structure.parser, undefined, lang);        
      if (undefined !== section) {
        return callModuleFunction(value, 
              section.module, 
              section['do'], 
              section.args);
      }
      
      // parse each element
      result = [];
      for (key = 0; key < value.length; ++key) {
        result[key] = parseValue(value[key], type, types, lang);
      }
      return result;
    }
    
    // object types    
    type = types.objectTypes[structure.type];
    if (type) {
      console.warn("object type found: " + structure.type);

      // TODO check type of value

      // check for object parser
      section = selectParser(structure.parser, undefined, lang);        
      if (undefined !== section) {
        return callModuleFunction(value, 
              section.module, 
              section['do'], 
              section.args);
      }

      var attributes = type.attributes;
      if (undefined === attributes) {
        // no attributes
        return null;
      }
      
      // TODO check type of attribute
      
      // parse each property
      result = {};
      for (key in attributes) {
        if (attributes.hasOwnProperty(key)) {
          if (value.hasOwnProperty(key)) {
          
            var subStructure = attributes[key];
            if (undefined === subStructure) {
             result[key] = value[key];
            }
            else {
              result[key] = parseValue(value[key], subStructure, types, lang);
            }

          }
          else {
            result[key] = null;
          }
        }
      }
      return result;
    }
  }
  catch (err) {
    console.warn("error = " + err);
  }

  return value;  
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns true if the value is valid
////////////////////////////////////////////////////////////////////////////////

function validateValue (value, structure, types, lang) {
  var result;
  var key;
  var validators;
  var v;

  try {
    var type = types.predefinedTypes[structure.type];
    if (type) {
      console.warn("predefined type found: " + structure.type);    
      
      // TODO check type of value
      
      validators = structure.validators;        
      if (undefined !== validators) {        
        for (key = 0; key < validators.length; ++key) {

          console.warn("call function: " + validators[key]['do']);    
          
          result = callModuleFunction(value, 
              validators[key].module, 
              validators[key]['do'], 
              validators[key].args);
              
          if (!result) {
            return false;
          }
        }        
      }

      validators = types.predefinedTypes[structure.type].validators;
      if (undefined !== validators) {        
        for (key = 0; key < validators.length; ++key) {

          console.warn("call function: " + validators[key]['do']);    
          
          result = callModuleFunction(value, 
              validators[key].module, 
              validators[key]['do'], 
              validators[key].args);
              
          if (!result) {
            return false;
          }
        }        
      }

      return true;      
    }
    
    // array types
    type = types.arrayTypes[structure.type];
    if (type) {
      console.warn("array type found: " + structure.type);
      
      // TODO check type of value
      
      // check for array validator
      validators = structure.validators;        
      if (undefined !== validators) {        
        for (key = 0; key < validators.length; ++key) {
          
          result = callModuleFunction(value, 
              validators[key].module, 
              validators[key]['do'], 
              validators[key].args);
              
          if (!result) {
            return false;
          }
        }
        return true;
      }
      
      // validate each element
      for (key = 0; key < value.length; ++key) {
        var valid = validateValue(value[key], type, types);
        if (!valid) {
          return false;
        }
      }
      return true;
    }
    
    // object types    
    type = types.objectTypes[structure.type];
    if (type) {
      console.warn("object type found: " + structure.type);

      // TODO check type of value

      // check for object validator
      validators = structure.validators;        
      if (undefined !== validators) {        
        for (key = 0; key < validators.length; ++key) {
          
          result = callModuleFunction(value, 
              validators[key].module, 
              validators[key]['do'], 
              validators[key].args);
              
          if (!result) {
            return false;
          }
        }
      }

      var attributes = type.attributes;
      if (undefined === attributes) {
        // no attributes
        return true;
      }
      
      // TODO check type of attribute
      
      // validate each property
      for (key in attributes) {
        if (attributes.hasOwnProperty(key)) {
          
          if (value.hasOwnProperty(key)) {
            v = value[key];
          }
          else {
            v = null;
          }
        
          var subStructure = attributes[key];
        
          if (undefined !== subStructure) {
            if (!parseValue(v, subStructure, types, lang)) {
              return false;
            }
          }
        }
      }
      
      return true;
    }
  }
  catch (err) {
    console.warn("error = " + err);
  }

  return false;  
}

////////////////////////////////////////////////////////////////////////////////
/// @brief convert a string to boolean
////////////////////////////////////////////////////////////////////////////////

function stringToBoolean(string){
	switch(string.toLowerCase()){
		case "true": case "yes": case "1": return true;
		case "false": case "no": case "0": case null: return false;
		default: return Boolean(string);
	}
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the structured document
////////////////////////////////////////////////////////////////////////////////

function resultStructure (req, res, doc, structure, headers) {
  var result = {};

  var key;
  var types = getTypes(structure);
  var lang = getLang(req);
  var format = true;

  if (undefined !== req.parameters.format) {
    format = stringToBoolean(req.parameters.format);
  }
  
  if (structure.attributes !== undefined) {
    for (key in structure.attributes) {
      if (structure.attributes.hasOwnProperty(key)) {
        var value = doc[key];
        
        // format value
        if (format) {
          result[key] = formatValue(value, structure.attributes[key], types, lang);
        }
        else {
          result[key] = value;
        }
      }      
    }
  }

  result._id = doc._id;
  result._rev = doc._rev;
  result._key = doc._key;
  
  resultOk(req, res, actions.HTTP_OK, result, headers);    
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the document
////////////////////////////////////////////////////////////////////////////////

function saveDocumentByStructure(req, res, collection, structure, body) {
  var document = {};
  
  var key;
  var value;
  var types = getTypes(structure);
  var lang = getLang(req);
  var format = true;

  if (undefined !== req.parameters.format) {
    format = stringToBoolean(req.parameters.format);
  }
  
  // TODO check type of body
  
  try {
    for (key in structure.attributes) {
      if (structure.attributes.hasOwnProperty(key)) {
        value = body[key];
        
        if (format) {
          value = parseValue(value, structure.attributes[key], types, lang);
        }
          
        console.warn("validate key: " + key);    
        if (validateValue(value, structure.attributes[key], types)) {
          document[key] = value;
        }
        else {
          throw("value of attribute '" + key + "' is not valid.");
        }
      }      
    }
  }
  catch(err) {
    resultError(req, res, actions.HTTP_BAD, 
        arangodb.ERROR_FAILED, 
        err);
    return;
  }
  
  saveDocument(req, res, collection, document);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the document
////////////////////////////////////////////////////////////////////////////////

function get_api_structure(req, res)  {
  var structure;
  
  var collection = getCollectionByRequest(req, res);
  if (undefined === collection) {
    return;
  }

  var doc = getDocumentByRequest(req, res, collection);
  if (undefined === doc) {
    return;
  }

  if (matchError(req, res, doc)) {
    return;
  }

  var headers = {
    "Etag" :  doc._rev
  };

  try {
    structure = db._collection("_structures").document(collection.name());
  }
  catch (err) {
    // return the doc
    console.warn("error = " + err);
    resultOk(req, res, actions.HTTP_OK, doc, headers);
    return;
  }

  resultStructure(req, res,  doc,  structure, headers);
}


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
/// @RESTHEADER{POST /_api/structure,creates a structured document}
///
/// @RESTQUERYPARAMETERS
///
/// @RESTQUERYPARAM{collection,string,required}
/// Creates a new document in the collection `collection`.
///
/// @RESTBODYPARAM{structure,json,required}
/// The structure definition.
///
/// @RESTDESCRIPTION
///
/// Creates a new document in the collection identified by the
/// `collection-identifier`.  A JSON representation of the document must be
/// passed as the body of the POST request. The document must fullfill the
/// requirements of the structure definition, see @ref ArangoStructures.
///
/// In all other respects the function is identical to the 
/// @ref triagens::arango::RestDocumentHandler::createDocument "POST /_api/structure".
////////////////////////////////////////////////////////////////////////////////

function post_api_structure (req, res) {
  // POST /_api/structure
  var body;
  var structure;

  var collection = getCollectionByRequest(req, res);
  if (undefined === collection) {
    return;
  }

  body = actions.getJsonBody(req, res);

  if (body === undefined) {
    resultError(req, res, actions.HTTP_BAD, 
        arangodb.ERROR_FAILED, "no body data");
    return;
  }

  try {
    structure = db._collection("_structures").document(collection.name());
    saveDocumentByStructure(req, res, collection, structure, body);
  }
  catch (err) {
    saveDocument(req, res, collection, body);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief handles a structure request
////////////////////////////////////////////////////////////////////////////////

actions.defineHttp({
  url : API,
  context : "api",

  callback : function (req, res) {
    try {
      if (req.requestType === actions.GET) {
        get_api_structure(req, res);
      }
      else if (req.requestType === actions.POST) {
        post_api_structure(req, res);
      }
/*      
      else if (req.requestType === actions.DELETE) {
        delete_api_structure(req, res);
      }
      else if (req.requestType === actions.PUT) {
        put_api_structure(req, res);
      }
*/      
      else {
        actions.resultUnsupported(req, res);
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

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @\\}\\)"
// End:
