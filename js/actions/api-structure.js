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
/*

Configuration example document:

{
 "_key" : "a_collection_name",      <- name of the collection with structure

 "attributes": {                    <- List of all attributes
  "number": {                       <- Name of the attribute
   "type": "number",                <- Type of the attribute
   "formatter": {                   <- Output formatter configuration
    "default": {
     "args": {
      "decPlaces": 4,
      "decSeparator": ".",
      "thouSeparator": ","
     },
     "module": "org/arangodb/formatter",
     "do": "formatFloat"
    },
    "de": {
     "args": {
      "decPlaces": 4,
      "decSeparator": ",",
      "thouSeparator": "."
     },
     "module": "org/arangodb/formatter",
     "do": "formatFloat"
    }
   },
   "parser": {                      <- Input parser configuration
    "default": {
     "args": {
      "decPlaces": 4,
      "decSeparator": ".",
      "thouSeparator": ","
     },
     "module": "org/arangodb/formatter",
     "do": "parseFloat"
    },
    "de": {
     "args": {
      "decPlaces": 4,
      "decSeparator": ",",
      "thouSeparator": "."
     },
     "module": "org/arangodb/formatter",
     "do": "parseFloat"
    }
   },
   "validators":                    <- List of input validators
      [ { "module": "org/arangodb/formatter", "do": "validateNotNull" } ]
  },
  "string": {                       <- Name of the attribute
   "type": "string",                <- Type of the attribute
   "formatter": {},
   "validators": []
  },
  "zahlen": {                       <- Name of the attribute
   "type": "number_list_type"       <- Type of the attribute
  },
  "number2": {                      <- Name of the attribute
   "type": "number",                <- Type of the attribute
   "formatter": {
    "default": {
     "args": {
      "decPlaces": 0,
      "decSeparator": ".",
      "thouSeparator": ","
     },
     "module": "org/arangodb/formatter",
     "do": "formatFloat"
    }
   },
   "validators": []
  },
  "no_structure": {                    <- Name of the attribute
   "type": "mixed"                     <- Type of the attribute
  },
  "timestamp": {
   "type": "number",
   "formatter": {
    "default": {
     "module": "org/arangodb/formatter",
     "do": "formatDatetime",
     "args": {
      "lang": "en",
      "timezone": "GMT",
      "pattern": "yyyy-MM-dd'T'HH:mm:ssZ"
     }
    },
    "de": {
     "module": "org/arangodb/formatter",
     "do": "formatDatetime",
     "args": {
      "lang": "de",
      "timezone": "Europe/Berlin",
      "pattern": "yyyy.MM.dd HH:mm:ss zzz"
     }
    }
   },
   "parser": {
    "default": {
     "module": "org/arangodb/formatter",
     "do": "parseDatetime",
     "args": {
      "lang": "en",
      "timezone": "GMT",
      "pattern": "yyyy-MM-dd'T'HH:mm:ssZ"
     }
    },
    "de": {
     "module": "org/arangodb/formatter",
     "do": "parseDatetime",
     "args": {
      "lang": "de",
      "timezone": "Europe/Berlin",
      "pattern": "yyyy.MM.dd HH:mm:ss zzz"
     }
    }
   },
   "validators": []
  },
  "object1": {                         <- Name of the attribute
   "type": "complex_type1"             <- Type of the attribute
  }
 },


 "arrayTypes": {                       <- Array type definitions
  "number_list_type": {                <- Name of type
   "type": "number",                  
   "formatter": {
    "default": {
     "args": {
      "decPlaces": 2,
      "decSeparator": ".",
      "thouSeparator": ","
     },
     "module": "org/arangodb/formatter",
     "do": "formatFloat"
    },
    "de": {
     "args": {
      "decPlaces": 2,
      "decSeparator": ",",
      "thouSeparator": "."
     },
     "module": "org/arangodb/formatter",
     "do": "formatFloat"
    }
   },
   "parser": {
    "default": {
     "args": {
      "decPlaces": 2,
      "decSeparator": ".",
      "thouSeparator": ","
     },
     "module": "org/arangodb/formatter",
     "do": "parseFloat"
    },
    "de": {
     "args": {
      "decPlaces": 2,
      "decSeparator": ",",
      "thouSeparator": "."
     },
     "module": "org/arangodb/formatter",
     "do": "parseFloat"
    }
   }
  }
 },


 "objectTypes": {                       <- Object type definitions
  "complex_type1": {                    <- Name of type
   "attributes": {                      <- Attributes of the object type
    "aNumber": {                        
     "type": "number"
    },
    "aList": {
     "type": "number_list_type"         <- reference to array type
    }
   }
  }
 }
}

*/
////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief convert a string to boolean
////////////////////////////////////////////////////////////////////////////////

function stringToBoolean(string){
  if (undefined === string) {
    return false;
  }
  
	switch(string.toLowerCase()){
		case "true": case "yes": case "1": return true;
		case "false": case "no": case "0": case null: return false;
		default: return Boolean(string);
	}
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns a (OK) result 
////////////////////////////////////////////////////////////////////////////////

function resultOk (req, res, httpReturnCode, keyvals, headers) {  
  'use strict';

  res.responseCode = httpReturnCode;
  res.contentType = "application/json; charset=utf-8";
  
  if (undefined !== keyvals) {
    res.body = JSON.stringify(keyvals);
  }

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
  if (undefined !== errorMessage.errorNum) {
    result.errorNum = errorMessage.errorNum;
  }
  else {
    result.errorNum     = errorNum;    
  }
  if (undefined !== errorMessage.errorMessage) {
    result.errorMessage = errorMessage.errorMessage;
  }
  else {
    result.errorMessage = errorMessage;    
  }
  
  
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
/// @brief returns the overwite policy
////////////////////////////////////////////////////////////////////////////////

function getOverwritePolicy(req)  {
  
  var policy = req.parameters.policy;
  
  if (undefined !== policy && "error" === policy) {
    return false;
  }
  
  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the overwite policy
////////////////////////////////////////////////////////////////////////////////

function getKeepNull(req)  {  
  return stringToBoolean(req.parameters.keepNull);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns wait for sync
////////////////////////////////////////////////////////////////////////////////

function getWaitForSync(req, collection)  {  
  if (collection.properties().waitForSync) {
    return true;
  }
  
  return stringToBoolean(req.parameters.waitForSync);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief save a document
////////////////////////////////////////////////////////////////////////////////

function saveDocument(req, res, collection, document)  {
  var doc;
  var waitForSync = getWaitForSync(req, collection);
    
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
  
  doc.error = false;
  
  resultOk(req, res, returnCode, doc, headers);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief replace a document
////////////////////////////////////////////////////////////////////////////////

function replaceDocument(req, res, collection, oldDocument, newDocument)  {
  var doc;
  var waitForSync = getWaitForSync(req, collection);
  var overwrite = getOverwritePolicy(req);
  
  if (!overwrite && 
      undefined !== newDocument._rev && 
      oldDocument._rev !== newDocument._rev) {
    resultError(req, res, actions.HTTP_BAD, 
        arangodb.ERROR_FAILED, 
        "wrong version");
    return;    
  }

  try {
    doc = collection.replace(oldDocument, newDocument, true, waitForSync);
  }
  catch(err) {
    resultError(req, res, actions.HTTP_BAD, 
        arangodb.ERROR_FAILED, 
        err);
    return;
  }

  var headers = {
    "Etag" :  doc._rev
  };

  var returnCode = waitForSync ? actions.HTTP_CREATED : actions.HTTP_ACCEPTED;
  
  resultOk(req, res, returnCode, doc, headers);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief update a document
////////////////////////////////////////////////////////////////////////////////

function patchDocument(req, res, collection, oldDocument, newDocument)  {
  var doc;
  var waitForSync = getWaitForSync(req, collection);
  var overwrite = getOverwritePolicy(req);
  var keepNull = getKeepNull(req);
  
  if (!overwrite && 
      undefined !== newDocument._rev && 
      oldDocument._rev !== newDocument._rev) {
    resultError(req, res, actions.HTTP_BAD, 
        arangodb.ERROR_FAILED, 
        "wrong version");
    return;    
  }
  
  try {
    doc = collection.update(oldDocument, newDocument, true, keepNull, waitForSync);
  }
  catch(err) {
    resultError(req, res, actions.HTTP_BAD, 
        arangodb.ERROR_FAILED, 
        err);
    return;
  }

  var headers = {
    "Etag" :  doc._rev
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
      //console.warn("predefined type found: " + structure.type);    
      
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
      //console.warn("array type found: " + structure.type);
      
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
      
      if(value instanceof Array) {
        for (key = 0; key < value.length; ++key) {
          result[key] = formatValue(value[key], type, types, lang);
        }
      }
      return result;
    }
    
    // object types    
    type = types.objectTypes[structure.type];
    if (type) {
      //console.warn("object type found: " + structure.type);

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
    //console.warn("error = " + err);
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
        //console.warn("section is undefined");
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
      //console.warn("array type found: " + structure.type);
      
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
      if(value instanceof Array) {
        for (key = 0; key < value.length; ++key) {
          result[key] = parseValue(value[key], type, types, lang);
        }
      }
      return result;
    }
    
    // object types    
    type = types.objectTypes[structure.type];
    if (type) {
      //console.warn("object type found: " + structure.type);

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
    //console.warn("error = " + err);
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
  
  //console.warn("in validateValue(): " + structure.type);    

  try {
    var type = types.predefinedTypes[structure.type];
    if (type) {
      //console.warn("predefined type found: " + structure.type);    
      
      // TODO check type of value
      
      validators = structure.validators;        
      if (undefined !== validators) {        
        for (key = 0; key < validators.length; ++key) {

          //console.warn("call function: " + validators[key]['do']);    
          
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

          //console.warn("call function: " + validators[key]['do']);    
          
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
      //console.warn("array type found: " + structure.type);
      
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
      //console.warn("object type found: " + structure.type);

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
            if (!validateValue(v, subStructure, types, lang)) {
              return false;
            }
          }
        }
      }
      
      return true;
    }
  }
  catch (err) {
    //console.warn("error = " + err);
  }

  return false;  
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
/// @brief parse body and return the parsed document
/// @throws exception
////////////////////////////////////////////////////////////////////////////////

function parseDocumentByStructure(req, res, structure, body, isPatch) {
  var document = {};

  var key;
  var value;
  var types = getTypes(structure);
  var lang = getLang(req);
  var format = true;

  if (undefined !== req.parameters.format) {
    format = stringToBoolean(req.parameters.format);
  }
    
  for (key in structure.attributes) {
    if (structure.attributes.hasOwnProperty(key)) {
      value = body[key];

      if (!isPatch || undefined !== value) {
        if (format) {
          value = parseValue(value, structure.attributes[key], types, lang);
        }

        //console.warn("validate key: " + key);
        if (validateValue(value, structure.attributes[key], types)) {
          document[key] = value;
        }
        else {
          throw("value of attribute '" + key + "' is not valid.");
        }
      }
    }
  }

  if (undefined !== body._id) {
    document._id = body._id;
  }
  if (undefined !== body._rev) {
    document._rev = body._rev;
  }
  if (undefined !== body._key) {
    document._key = body._key;
  }  
  
  return document;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the parsed document
////////////////////////////////////////////////////////////////////////////////

function saveDocumentByStructure(req, res, collection, structure, body) {
  try {
    var document = parseDocumentByStructure(req, res, structure, body, false);
    saveDocument(req, res, collection, document);
  }
  catch(err) {
    resultError(req, res, actions.HTTP_BAD, 
        arangodb.ERROR_FAILED, 
        err);
    return;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief replace the parsed document
////////////////////////////////////////////////////////////////////////////////

function replaceDocumentByStructure(req, res, collection, structure, oldDocument, body) {
  try {
    var document = parseDocumentByStructure(req, res, structure, body, false);
    replaceDocument(req, res, collection, oldDocument, document);
  }
  catch(err) {
    resultError(req, res, actions.HTTP_BAD, 
        arangodb.ERROR_FAILED, 
        err);
    return;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief patch the parsed document
////////////////////////////////////////////////////////////////////////////////

function patchDocumentByStructure(req, res, collection, structure, oldDocument, body) {
  try {
    var document = parseDocumentByStructure(req, res, structure, body, true);
    patchDocument(req, res, collection, oldDocument, document);
  }
  catch(err) {
    resultError(req, res, actions.HTTP_BAD, 
        arangodb.ERROR_FAILED, 
        err);
    return;
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoAPI
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief reads a single document
///
/// @RESTHEADER{GET /_api/structures/`document-handle`,reads a document}
///
/// @RESTURLPARAMETERS
///
/// @RESTURLPARAM{document-handle,string,required}
/// The Handle of the Document.
///
/// @RESTQUERYPARAM{rev,string,optional}
/// You can conditionally select a document based on a target revision id by
/// using the `rev` URL parameter.
/// 
/// @RESTQUERYPARAM{lang,string,optional}
/// Language of the data.
///
/// @RESTQUERYPARAM{format,boolean,optional}
/// False for unformated values (default: true).
///
/// @RESTHEADERPARAMETERS
///
/// @RESTHEADERPARAM{If-None-Match,string,optional}
/// If the "If-None-Match" header is given, then it must contain exactly one
/// etag. The document is returned, if it has a different revision than the
/// given etag. Otherwise a `HTTP 304` is returned.
///
/// @RESTHEADERPARAM{If-Match,string,optional}
/// If the "If-Match" header is given, then it must contain exactly one
/// etag. The document is returned, if it has the same revision ad the
/// given etag. Otherwise a `HTTP 412` is returned. As an alternative
/// you can supply the etag in an attribute `rev` in the URL.
///
/// @RESTDESCRIPTION
/// Returns the document identified by `document-handle`. The returned
/// document contains two special attributes: `_id` containing the document
/// handle and `_rev` containing the revision.
///
/// @RESTRETURNCODES
/// 
/// @RESTRETURNCODE{200}
/// is returned if the document was found
///
/// @RESTRETURNCODE{404}
/// is returned if the document or collection was not found
///
/// @RESTRETURNCODE{304}
/// is returned if the "If-None-Match" header is given and the document has
/// same version
///
/// @RESTRETURNCODE{412}
/// is returned if a "If-Match" header or `rev` is given and the found
/// document has a different version
///
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
    resultOk(req, res, actions.HTTP_OK, doc, headers);
    return;
  }

  resultStructure(req, res,  doc,  structure, headers);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief reads a single document head
///
/// @RESTHEADER{HEAD /_api/structures/`document-handle`,reads a document header}
///
/// @RESTURLPARAMETERS
///
/// @RESTURLPARAM{document-handle,string,required}
/// The Handle of the Document.
///
/// @RESTQUERYPARAMETERS
///
/// @RESTQUERYPARAM{rev,string,optional}
/// You can conditionally select a document based on a target revision id by
/// using the `rev` URL parameter.
/// 
/// @RESTHEADERPARAMETERS
///
/// @RESTHEADERPARAM{If-Match,string,optional}
/// You can conditionally get a document based on a target revision id by
/// using the `if-match` HTTP header.
/// 
/// @RESTDESCRIPTION
/// Like `GET`, but only returns the header fields and not the body. You
/// can use this call to get the current revision of a document or check if
/// the document was deleted.
///
/// @RESTRETURNCODES
/// 
/// @RESTRETURNCODE{200}
/// is returned if the document was found
///
/// @RESTRETURNCODE{404}
/// is returned if the document or collection was not found
///
/// @RESTRETURNCODE{304}
/// is returned if the "If-None-Match" header is given and the document has
/// same version
///
/// @RESTRETURNCODE{412}
/// is returned if a "If-Match" header or `rev` is given and the found
/// document has a different version
///
////////////////////////////////////////////////////////////////////////////////

function head_api_structure(req, res)  {
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

  resultOk(req, res, actions.HTTP_OK, undefined, headers);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief deletes a document
///
/// @RESTHEADER{DELETE /_api/structures/`document-handle`,deletes a document}
///
/// @RESTURLPARAMETERS
///
/// @RESTURLPARAM{document-handle,string,required}
/// Deletes the document identified by `document-handle`. 
/// 
/// @RESTQUERYPARAMETERS
///
/// @RESTQUERYPARAM{rev,string,optional}
/// You can conditionally delete a document based on a target revision id by
/// using the `rev` URL parameter.
/// 
/// @RESTQUERYPARAM{policy,string,optional}
/// To control the update behavior in case there is a revision mismatch, you
/// can use the `policy` parameter. This is the same as when replacing
/// documents (see replacing documents for more details).
///
/// @RESTQUERYPARAM{waitForSync,boolean,optional}
/// Wait until document has been sync to disk.
///
/// @RESTHEADERPARAMETERS
///
/// @RESTHEADERPARAM{If-Match,string,optional}
/// You can conditionally delete a document based on a target revision id by
/// using the `if-match` HTTP header.
/// 
/// @RESTDESCRIPTION
/// The body of the response contains a JSON object with the information about
/// the handle and the revision.  The attribute `_id` contains the known
/// `document-handle` of the updated document, the attribute `_rev`
/// contains the known document revision.
///
/// If the `waitForSync` parameter is not specified or set to
/// `false`, then the collection's default `waitForSync` behavior is
/// applied. The `waitForSync` URL parameter cannot be used to disable
/// synchronisation for collections that have a default `waitForSync` value
/// of `true`.
///
/// @RESTRETURNCODES
///
/// @RESTRETURNCODE{200}
/// is returned if the document was deleted sucessfully and `waitForSync` was
/// `true`.
///
/// @RESTRETURNCODE{202}
/// is returned if the document was deleted sucessfully and `waitForSync` was
/// `false`.
///
/// @RESTRETURNCODE{404}
/// is returned if the collection or the document was not found.
/// The response body contains an error document in this case.
///
/// @RESTRETURNCODE{412}
/// is returned if a "If-Match" header or `rev` is given and the current
/// document has a different version
///
////////////////////////////////////////////////////////////////////////////////

function delete_api_structure (req, res) {
  
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

  var waitForSync = getWaitForSync(req, collection);

  try {
    collection.remove( doc, true, waitForSync);
    resultOk(req, res, 
      waitForSync ? actions.HTTP_OK : actions.HTTP_ACCEPTED, 
      { "deleted" : true });    
  }
  catch(err) {
    resultError(req, res, actions.HTTP_BAD, 
        arangodb.ERROR_FAILED, 
        err);
    return;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief updates a document
///
/// @RESTHEADER{PATCH /_api/structures/`document-handle`,patches a document}
///
/// @RESTURLPARAMETERS
///
/// @RESTURLPARAM{document-handle,string,required}
/// The Handle of the Document.
///
/// @RESTQUERYPARAMETERS
///
/// @RESTQUERYPARAM{keepNull,string,optional}
/// If the intention is to delete existing attributes with the patch command, 
/// the URL query parameter `keepNull` can be used with a value of `false`.
/// This will modify the behavior of the patch command to remove any attributes
/// from the existing document that are contained in the patch document with an
/// attribute value of `null`.
///
/// @RESTQUERYPARAM{waitForSync,boolean,optional}
/// Wait until document has been sync to disk.
///
/// @RESTQUERYPARAM{rev,string,optional}
/// You can conditionally patch a document based on a target revision id by
/// using the `rev` URL parameter.
/// 
/// @RESTQUERYPARAM{policy,string,optional}
/// To control the update behavior in case there is a revision mismatch, you
/// can use the `policy` parameter.
///
/// @RESTQUERYPARAM{lang,string,optional}
/// Language of the data.
///
/// @RESTQUERYPARAM{format,boolean,optional}
/// False for unformated values (default: true).
///
/// @RESTHEADERPARAMETERS
///
/// @RESTHEADERPARAM{If-Match,string,optional}
/// You can conditionally delete a document based on a target revision id by
/// using the `if-match` HTTP header.
/// 
/// @RESTDESCRIPTION
/// Partially updates the document identified by `document-handle`.
/// The body of the request must contain a JSON document with the attributes
/// to patch (the patch document). All attributes from the patch document will
/// be added to the existing document if they do not yet exist, and overwritten
/// in the existing document if they do exist there.
///
/// Setting an attribute value to `null` in the patch document will cause a
/// value of `null` be saved for the attribute by default. 
///
/// Optionally, the URL parameter `waitForSync` can be used to force
/// synchronisation of the document update operation to disk even in case
/// that the `waitForSync` flag had been disabled for the entire collection.
/// Thus, the `waitForSync` URL parameter can be used to force synchronisation
/// of just specific operations. To use this, set the `waitForSync` parameter
/// to `true`. If the `waitForSync` parameter is not specified or set to
/// `false`, then the collection's default `waitForSync` behavior is
/// applied. The `waitForSync` URL parameter cannot be used to disable
/// synchronisation for collections that have a default `waitForSync` value
/// of `true`.
///
/// The body of the response contains a JSON object with the information about
/// the handle and the revision. The attribute `_id` contains the known
/// `document-handle` of the updated document, the attribute `_rev`
/// contains the new document revision.
///
/// If the document does not exist, then a `HTTP 404` is returned and the
/// body of the response contains an error document.
///
/// You can conditionally update a document based on a target revision id by
/// using either the `rev` URL parameter or the `if-match` HTTP header.
/// To control the update behavior in case there is a revision mismatch, you
/// can use the `policy` parameter. This is the same as when replacing
/// documents (see replacing documents for details).
///
/// @RESTRETURNCODES
///
/// @RESTRETURNCODE{201}
/// is returned if the document was created sucessfully and `waitForSync` was
/// `true`.
///
/// @RESTRETURNCODE{202}
/// is returned if the document was created sucessfully and `waitForSync` was
/// `false`.
///
/// @RESTRETURNCODE{400}
/// is returned if the body does not contain a valid JSON representation of a
/// document.  The response body contains an error document in this case.
///
/// @RESTRETURNCODE{404}
/// is returned if collection or the document was not found
///
/// @RESTRETURNCODE{412}
/// is returned if a "If-Match" header or `rev` is given and the found
/// document has a different version
///
////////////////////////////////////////////////////////////////////////////////

function patch_api_structure (req, res) {
  var body;
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

  body = actions.getJsonBody(req, res);

  if (body === undefined) {
    resultError(req, res, actions.HTTP_BAD, 
        arangodb.ERROR_FAILED, "no body data");
    return;
  }

  try {
    structure = db._collection("_structures").document(collection.name());
    patchDocumentByStructure(req, res, collection, structure, doc, body);
  }
  catch (err) {
    patchDocument(req, res, collection, doc, body);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief replaces a document
///
/// @RESTHEADER{PUT /_api/structures/`document-handle`,replaces a document}
///
/// @RESTURLPARAMETERS
///
/// @RESTURLPARAM{document-handle,string,required}
/// The Handle of the Document.
/// 
/// @RESTQUERYPARAMETERS
///
/// @RESTQUERYPARAM{waitForSync,boolean,optional}
/// Wait until document has been sync to disk.
///
/// @RESTQUERYPARAM{rev,string,optional}
/// You can conditionally replace a document based on a target revision id by
/// using the `rev` URL parameter.
/// 
/// @RESTQUERYPARAM{policy,string,optional}
/// To control the update behavior in case there is a revision mismatch, you
/// can use the `policy` parameter. This is the same as when replacing
/// documents (see replacing documents for more details).
///
/// @RESTQUERYPARAM{lang,string,optional}
/// Language of the data.
///
/// @RESTQUERYPARAM{format,boolean,optional}
/// False for unformated values (default: true).
///
/// @RESTHEADERPARAMETERS
///
/// @RESTHEADERPARAM{If-Match,string,optional}
/// You can conditionally replace a document based on a target revision id by
/// using the `if-match` HTTP header.
/// 
/// @RESTDESCRIPTION
/// Completely updates (i.e. replaces) the document identified by `document-handle`.
/// If the document exists and can be updated, then a `HTTP 201` is returned
/// and the "ETag" header field contains the new revision of the document.
///
/// If the new document passed in the body of the request contains the
/// `document-handle` in the attribute `_id` and the revision in `_rev`,
/// these attributes will be ignored. Only the URI and the "ETag" header are
/// relevant in order to avoid confusion when using proxies.
///
/// Optionally, the URL parameter `waitForSync` can be used to force
/// synchronisation of the document replacement operation to disk even in case
/// that the `waitForSync` flag had been disabled for the entire collection.
/// Thus, the `waitForSync` URL parameter can be used to force synchronisation
/// of just specific operations. To use this, set the `waitForSync` parameter
/// to `true`. If the `waitForSync` parameter is not specified or set to
/// `false`, then the collection's default `waitForSync` behavior is
/// applied. The `waitForSync` URL parameter cannot be used to disable
/// synchronisation for collections that have a default `waitForSync` value
/// of `true`.
///
/// The body of the response contains a JSON object with the information about
/// the handle and the revision.  The attribute `_id` contains the known
/// `document-handle` of the updated document, the attribute `_rev`
/// contains the new document revision.
///
/// If the document does not exist, then a `HTTP 404` is returned and the
/// body of the response contains an error document.
///
/// There are two ways for specifying the targeted document revision id for
/// conditional replacements (i.e. replacements that will only be executed if
/// the revision id found in the database matches the document revision id specified
/// in the request):
/// - specifying the target revision in the `rev` URL query parameter
/// - specifying the target revision in the `if-match` HTTP header
///
/// Specifying a target revision is optional, however, if done, only one of the
/// described mechanisms must be used (either the `rev` URL parameter or the
/// `if-match` HTTP header).
/// Regardless which mechanism is used, the parameter needs to contain the target
/// document revision id as returned in the `_rev` attribute of a document or
/// by an HTTP `etag` header.
///
/// For example, to conditionally replace a document based on a specific revision
/// id, you the following request:
/// 
/// - PUT /_api/document/`document-handle`?rev=`etag`
///
/// If a target revision id is provided in the request (e.g. via the `etag` value
/// in the `rev` URL query parameter above), ArangoDB will check that
/// the revision id of the document found in the database is equal to the target
/// revision id provided in the request. If there is a mismatch between the revision
/// id, then by default a `HTTP 412` conflict is returned and no replacement is
/// performed.
///
/// The conditional update behavior can be overriden with the `policy` URL query parameter:
///
/// - PUT /_api/document/`document-handle`?policy=`policy`
///
/// If `policy` is set to `error`, then the behavior is as before: replacements
/// will fail if the revision id found in the database does not match the target
/// revision id specified in the request.
///
/// If `policy` is set to `last`, then the replacement will succeed, even if the
/// revision id found in the database does not match the target revision id specified
/// in the request. You can use the `last` `policy` to force replacements.
///
/// @RESTRETURNCODES
///
/// @RESTRETURNCODE{201}
/// is returned if the document was created sucessfully and `waitForSync` was
/// `true`.
///
/// @RESTRETURNCODE{202}
/// is returned if the document was created sucessfully and `waitForSync` was
/// `false`.
///
/// @RESTRETURNCODE{400}
/// is returned if the body does not contain a valid JSON representation of a
/// document.  The response body contains an error document in this case.
///
/// @RESTRETURNCODE{404}
/// is returned if collection or the document was not found
///
/// @RESTRETURNCODE{412}
/// is returned if a "If-Match" header or `rev` is given and the found
/// document has a different version
///
////////////////////////////////////////////////////////////////////////////////

function put_api_structure (req, res) {
  var body;
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

  body = actions.getJsonBody(req, res);

  if (body === undefined) {
    resultError(req, res, actions.HTTP_BAD, 
        arangodb.ERROR_FAILED, "no body data");
    return;
  }

  try {
    structure = db._collection("_structures").document(collection.name());
    replaceDocumentByStructure(req, res, collection, structure, doc, body);
  }
  catch (err) {
    replaceDocument(req, res, collection, doc, body);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a document
///
/// @RESTHEADER{POST /_api/structure,creates a document}
///
/// @RESTBODYPARAM{document,json,required}
/// A JSON representation of document.
///
/// @RESTQUERYPARAMETERS
///
/// @RESTQUERYPARAM{collection,string,required}
/// The collection name.
///
/// @RESTQUERYPARAM{createCollection,boolean,optional}
/// If this parameter has a value of `true` or `yes`, then the collection is
/// created if it does not yet exist. Other values will be ignored so the
/// collection must be present for the operation to succeed.
///
/// @RESTQUERYPARAM{waitForSync,boolean,optional}
/// Wait until document has been sync to disk.
///
/// @RESTQUERYPARAM{lang,string,optional}
/// Language of the send data.
///
/// @RESTQUERYPARAM{format,boolean,optional}
/// True, if the document contains formatted values (default: true).
///
/// @RESTDESCRIPTION
/// Creates a new document in the collection named `collection`.  A JSON
/// representation of the document must be passed as the body of the POST
/// request.
///
/// If the document was created successfully, then the "Location" header
/// contains the path to the newly created document. The "ETag" header field
/// contains the revision of the document.
///
/// The body of the response contains a JSON object with the following
/// attributes:
///
/// - `_id` contains the document handle of the newly created document
/// - `_key` contains the document key
/// - `_rev` contains the document revision
///
/// If the collection parameter `waitForSync` is `false`, then the call returns
/// as soon as the document has been accepted. It will not wait, until the
/// documents has been sync to disk.
///
/// Optionally, the URL parameter `waitForSync` can be used to force
/// synchronisation of the document creation operation to disk even in case that
/// the `waitForSync` flag had been disabled for the entire collection.  Thus,
/// the `waitForSync` URL parameter can be used to force synchronisation of just
/// this specific operations. To use this, set the `waitForSync` parameter to
/// `true`. If the `waitForSync` parameter is not specified or set to `false`,
/// then the collection's default `waitForSync` behavior is applied. The
/// `waitForSync` URL parameter cannot be used to disable synchronisation for
/// collections that have a default `waitForSync` value of `true`.
///
/// @RESTRETURNCODES
///
/// @RESTRETURNCODE{201}
/// is returned if the document was created sucessfully and `waitForSync` was
/// `true`.
///
/// @RESTRETURNCODE{202}
/// is returned if the document was created sucessfully and `waitForSync` was
/// `false`.
///
/// @RESTRETURNCODE{400}
/// is returned if the body does not contain a valid JSON representation of a
/// document.  The response body contains an error document in this case.
///
/// @RESTRETURNCODE{404}
/// is returned if the collection specified by `collection` is unknown.  The
/// response body contains an error document in this case.
////////////////////////////////////////////////////////////////////////////////

function post_api_structure (req, res) {
  // POST /_api/structure
  var body;
  var structure;
  var collection;

  var collectionName = req.parameters.collection;
  if (undefined === collectionName) {
    resultError(req, res, actions.HTTP_NOT_FOUND, 
        arangodb.ERROR_ARANGO_COLLECTION_NOT_FOUND, "collection not found");
    return;    
  }

  try {
    collection = db._collection(collectionName);
  }
  catch (err) {
  }
  
  if (null === collection) {
    var createCollection = stringToBoolean(req.parameters.createCollection);
    if (createCollection) {
      try {
        db._create(collectionName);
        collection = db._collection(collectionName);
      }
      catch(err2) {        
        resultError(req, res, actions.HTTP_NOT_FOUND, 
          arangodb.ERROR_ARANGO_COLLECTION_NOT_FOUND, err2);
        return;             
      }
    }    
  } 
  
  if (undefined === collection || null === collection) {
    resultError(req, res, actions.HTTP_NOT_FOUND, 
        arangodb.ERROR_ARANGO_COLLECTION_NOT_FOUND, "collection not found");
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
  catch (err3) {
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
      if (req.requestType === actions.DELETE) {
        delete_api_structure(req, res);
      }
      else if (req.requestType === actions.GET) {
        get_api_structure(req, res);
      }
      else if (req.requestType === actions.HEAD) {
        head_api_structure(req, res);
      }
      else if (req.requestType === actions.PATCH) {
        patch_api_structure(req, res);
      }
      else if (req.requestType === actions.POST) {
        post_api_structure(req, res);
      }
      else if (req.requestType === actions.PUT) {
        put_api_structure(req, res);
      }
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
