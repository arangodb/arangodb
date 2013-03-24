/*jslint indent: 2, nomen: true, maxlen: 100, sloppy: true, vars: true, white: true, plusplus: true */
/*global require, exports */

////////////////////////////////////////////////////////////////////////////////
/// @brief ArangoAal
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
/// @author Jan Steemann
/// @author Copyright 2012-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var arangosh = require("org/arangodb/arangosh");
var fs = require("fs");
var internal = require("internal");
var db = internal.db;

// -----------------------------------------------------------------------------
// --SECTION--                                                         ArangoAal
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoAal
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief build a github repository URL
////////////////////////////////////////////////////////////////////////////////

function buildGithubUrl (repositoryName) {
  return 'https://github.com/' + repositoryName + '/archive/master.zip';
}

////////////////////////////////////////////////////////////////////////////////
/// @brief validate the source type specified by the user
////////////////////////////////////////////////////////////////////////////////
  
function validateSource (origin) {
  var type, location;
  
  if (origin === undefined || origin === null) {
    throw "no application name specified";
  }

  if (origin && origin.location && origin.type) {
    type     = origin.type;
    location = origin.location;
  }
  else {
    throw "invalid application declaration";
  }

  return { location: location, type: type };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief process files in a zip file
////////////////////////////////////////////////////////////////////////////////

function processZip (source) {
  var location = source.location;

  if (! fs.exists(location) || ! fs.isFile(location)) {
    throw "could not find file";
  }

  source.filename = source.location;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief process files in a directory
////////////////////////////////////////////////////////////////////////////////

function processDirectory (source) {
  var location = source.location;

  if (! fs.exists(location) || ! fs.isDirectory(location)) {
    throw "could not find directory";
  }
  
  var files = fs.listTree(location);
  files.shift();
  if (files.length === 0) {
    throw "directory is empty";
  }

  var tempFile = fs.getTempFile("downloads", false); 
  source.filename = tempFile;
  source.removeFile = true;
    
  internal.zipFile(tempFile, files);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief process files from a github repository 
////////////////////////////////////////////////////////////////////////////////

function processGithubRepository (source) {
  var url = buildGithubUrl(source.location);

  var tempFile = fs.getTempFile("downloads", false); 

  try {
    var result = internal.download(url, "get", tempFile);
    if (result.code >= 200 && result.code <= 299) {
      source.filename = tempFile;
      source.removeFile = true;
    }
  }
  catch (err) {
    throw "could not download from repository";
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief process a source declaration
////////////////////////////////////////////////////////////////////////////////

function processSource (src) {
  if (src.type === "zip") {
    processZip(src);
  }
  else if (src.type === "directory") {
    processDirectory(src);
  }
  else if (src.type === "github") {
    processGithubRepository(src);
  }
  else {
    throw "unknown application type";
  }

  // upload file to the server 
  var response = internal.arango.SEND_FILE("/_api/upload", src.filename);

  if (src.removeFile && src.filename !== '') {
    try {
      fs.remove(src.filename);
    }
    catch (err2) {
    }
  } 

  if (! response.filename) {
    throw "could not upload application to arangodb";
  }

  return response.filename;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoAal
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief get a module
////////////////////////////////////////////////////////////////////////////////

exports.get = function (name, version, origin) {
  var source = validateSource(origin);

  var serverFile = processSource(source);

  var body = {
    name: name,
    version: version,
    filename: serverFile
  };

  var requestResult = db._connection.POST("/_api/aal", JSON.stringify(body));

  arangosh.checkRequestResult(requestResult);

  return requestResult.result;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief uninstall a module
////////////////////////////////////////////////////////////////////////////////

exports.uninstall = function (name, version) {
  if (name === undefined || name === null) {
    throw "no application name specified";
  }

  if (version === undefined || version === null) {
    version = "";
  }

  var requestResult = db._connection.DELETE("/_api/aal/" + 
    encodeURIComponent(name) + "/" + encodeURIComponent(version)); 

  arangosh.checkRequestResult(requestResult);

  return requestResult.result;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief mount a module
////////////////////////////////////////////////////////////////////////////////

exports.mount = function (name, version, path, dbPrefix) {
  if (name === undefined || name === null) {
    throw "no application name specified";
  }

  if (version === undefined || version === null) {
    version = "";
  }

  if (path === undefined || path === null) {
    throw "no application path specified";
  }

  if (dbPrefix === undefined || dbPrefix === null) {
    dbPrefix = "";
  }

  var body = {
    name: name,
    version: version,
    path: path,
    dbPrefix: dbPrefix
  };
  
  var requestResult = db._connection.PUT("/_api/aal",
    JSON.stringify(body));

  arangosh.checkRequestResult(requestResult);

  return requestResult.result;
};

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @}\\|/\\*jslint"
// End:
