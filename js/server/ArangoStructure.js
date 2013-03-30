/*jslint indent: 2,
         nomen: true,
         maxlen: 100,
         sloppy: true,
         vars: true,
         white: true,
         plusplus: true */
/*global require */

////////////////////////////////////////////////////////////////////////////////
/// @brief ArangoStructure
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2011-2012 triagens GmbH, Cologne, Germany
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
/// @author Copyright 2011-2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

(function () {
  var internal = require("internal");
  var console = require("console");

  var ArangoDatabase = internal.ArangoDatabase;
  var ArangoCollection = internal.ArangoCollection;
  var ArangoError = internal.ArangoError;

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup V8Shell
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

  function ArangoStructure (collection, lang) {
    this._id = collection._id;
    this._collection = collection;
    this._language = lang;
  }

  internal.ArangoStructure = ArangoStructure;

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup V8Shell
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief prints a collection
////////////////////////////////////////////////////////////////////////////////

  ArangoStructure.prototype._PRINT = function (context) {
    var status = "unknown";
    var type = "unknown";

    switch (this.status()) {
      case ArangoCollection.STATUS_NEW_BORN: status = "new born"; break;
      case ArangoCollection.STATUS_UNLOADED: status = "unloaded"; break;
      case ArangoCollection.STATUS_UNLOADING: status = "unloading"; break;
      case ArangoCollection.STATUS_LOADED: status = "loaded"; break;
      case ArangoCollection.STATUS_CORRUPTED: status = "corrupted"; break;
      case ArangoCollection.STATUS_DELETED: status = "deleted"; break;
    }

    switch (this.type()) {
      case ArangoCollection.TYPE_DOCUMENT: type = "document"; break;
      case ArangoCollection.TYPE_EDGE:     type = "edge"; break;
    }

    context.output += "[ArangoStructure " + this._id + ", \"", this.name() 
                    + "\" (type " + type + ", status " + status +"lang "
                    + this._language + ")]";
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief parses a single attribute
////////////////////////////////////////////////////////////////////////////////

  function parse (lang, info, value) {
    var error;
    var module;

    try {
      module = require(info.module);
    }
    catch (err) {
      error = new ArangoError();
      error.errorNum = internal.errors.ERROR_NOT_IMPLEMENTED.code;
      error.errorMessage = String(err);

      throw error;
    }

    return module[info['do']](value, info, lang);
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief validates a single attribute
////////////////////////////////////////////////////////////////////////////////

  function validate (lang, info, value) {
    var error;
    var module;

    try {
      module = require(info.module);
    }
    catch (err) {
      error = new ArangoError();
      error.errorNum = internal.errors.ERROR_NOT_IMPLEMENTED.code;
      error.errorMessage = String(err);

      throw error;
    }

    module[info['do']](value, info, lang);
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief parses and validates a single attribute
////////////////////////////////////////////////////////////////////////////////

  function parseAndValidate (lang, info, value) {
    var type;

    // parse the input
    if (info.hasOwnProperty("parser")) {
      if (info.parser.hasOwnProperty(lang)) {
        value = parse(lang, info.parser[lang], value);
      }
      else if (info.parser.hasOwnProperty('default')) {
        value = parse(lang, info.parser['default'], value);
      }
    }
    else if (info.hasOwnProperty("type")) {
      type = info.type;

      if (type === "number") {
        value = parseFloat(value);
      }
      else if (type === "string") {
        value = String(value);
      }
      else if (type === "boolean") {
        if (value === 1 || value === "1" || (/^true$/i).test(value)) {
          value = true;
        }
        else {
         value = false;
        }
      }
    }

    // validate the input
    if (info.hasOwnProperty("validator")) {
      if (info.validator.hasOwnProperty(lang)) {
        validate(lang, info.validator[lang], value);
      }
      else if (info.validator.hasOwnProperty('default')) {
        validate(lang, info.validator['default'], value);
      }
    }

    // and return
    return value;
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief parses and validates the structure information recursively
////////////////////////////////////////////////////////////////////////////////

  function parseStructureInformationRecursive (lang, info, path, key, value) {
    var k;
    var result;

    if (info.attributes.hasOwnProperty(path)) {
      value = parseAndValidate(lang, info.attributes[path], value);
    }

    if (value instanceof Array) {
      return value;
    }

    if (value instanceof Object) {
      result = {};

      for (k in value) {
        if (value.hasOwnProperty(k)) {
          result[k] = parseStructureInformationRecursive(lang, info, path + "." + k, k, value[k]);
        }
      }

      return result;
    }

    return value;
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief parses and validates the structure information
////////////////////////////////////////////////////////////////////////////////

  function parseStructureInformation (lang, name, data) {
    var info;
    var key;
    var validated;

    info = internal.db._collection("_structures").firstExample({ collection: name });

    if (info === null) {
      return data;
    }

    validated = {};

    for (key in data) {
      if (data.hasOwnProperty(key)) {
        validated[key] = parseStructureInformationRecursive(lang, info, key, key, data[key]);
      }
    }

    return validated;
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief formats a single attribute
////////////////////////////////////////////////////////////////////////////////

  function format (lang, info, value) {
    var error;
    var module;

    try {
      module = require(info.module);
    }
    catch (err) {
      error = new ArangoError();
      error.errorNum = internal.errors.ERROR_NOT_IMPLEMENTED.code;
      error.errorMessage = String(err);

      throw error;
    }

    return module[info['do']](value, info, lang);
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief formats a single attribute
////////////////////////////////////////////////////////////////////////////////

  function formatValue (lang, info, value) {
    var type;

    // parse the input
    if (info.hasOwnProperty("formatter")) {
      if (info.formatter.hasOwnProperty(lang)) {
        value = format(lang, info.formatter[lang], value);
      }
      else if (info.formatter.hasOwnProperty('default')) {
        value = format(lang, info.formatter['default'], value);
      }
    }

    // and return
    return value;
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief formats the structure information recursively
////////////////////////////////////////////////////////////////////////////////

  function formatStructureInformationRecursive (lang, info, path, key, value) {
    var k;
    var result;

    if (info.attributes.hasOwnProperty(path)) {
      value = formatValue(lang, info.attributes[path], value);
    }

    if (value instanceof Array) {
      return value;
    }

    if (value instanceof Object) {
      result = {};

      for (k in value) {
        if (value.hasOwnProperty(k)) {
          result[k] = formatStructureInformationRecursive(lang, info, path + "." + k, k, value[k]);
        }
      }

      return result;
    }

    return value;
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief formats the structure information
////////////////////////////////////////////////////////////////////////////////

  function formatStructureInformation (lang, name, data) {
    var info;
    var key;
    var formatted;

    info = internal.db._collection("_structures").firstExample({ collection: name });

    if (info === null) {
      return data;
    }

    formatted = {};

    for (key in data) {
      if (data.hasOwnProperty(key)) {
        formatted[key] = formatStructureInformationRecursive(lang, info, key, key, data[key]);
      }
    }

    return formatted;
  }

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup V8Shell
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief name of a structure
////////////////////////////////////////////////////////////////////////////////

  ArangoStructure.prototype.name = function () {
    return this._collection.name();
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief status of a structure
////////////////////////////////////////////////////////////////////////////////

  ArangoStructure.prototype.status = function () {
    return this._collection.status();
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief type of a structure
////////////////////////////////////////////////////////////////////////////////

  ArangoStructure.prototype.type = function () {
    return this._collection.type();
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief saves a document
////////////////////////////////////////////////////////////////////////////////

  ArangoStructure.prototype.save = function (data, waitForSync) {
    data = parseStructureInformation(this._language, this._collection.name(), data);

    return this._collection.save(data, waitForSync);
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief replaces a document
////////////////////////////////////////////////////////////////////////////////

  ArangoStructure.prototype.replace = function (document, data, overwrite, waitForSync) {
    data = parseStructureInformation(this._language, this._collection.name(), data);

    return this._collection.save(document, data, overwrite, waitForSync);
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief deletes a document
////////////////////////////////////////////////////////////////////////////////

  ArangoStructure.prototype.remove = function (document, overwrite, waitForSync) {
    return this._collection.remove(document, overwrite, waitForSync);
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief reads a document
////////////////////////////////////////////////////////////////////////////////

  ArangoStructure.prototype.document = function (handle) {
    var data;

    data = this._collection.document(handle);

    return formatStructureInformation(this._language, this._collection.name(), data);
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief database accessor
////////////////////////////////////////////////////////////////////////////////

  ArangoDatabase.prototype._structure = function (name, lang) {
    var collection;

    collection = internal.db._collection(name);

    if (collection === null) {
      return null;
    }

    if (lang === undefined || lang === null) {
      lang = "en";
    }

    return new ArangoStructure(collection, lang);
  };

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

}());

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @\\}\\)"
// End:
