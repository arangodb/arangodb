/*jslint indent: 2, nomen: true, maxlen: 100, sloppy: true, vars: true, white: true, plusplus: true, regexp: true, continue: true */
/*global require, exports */

////////////////////////////////////////////////////////////////////////////////
/// @brief deployment tools
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
var fs = require("fs");
var internal = require("internal");

// -----------------------------------------------------------------------------
// --SECTION--                                                         ArangoApp
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoDeployment
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief application
////////////////////////////////////////////////////////////////////////////////

function ArangoApp (routing, description) {
  this._name = description.application;
  this._routing = routing;
  this._description = description;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoDeployment
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief guesses the content type
////////////////////////////////////////////////////////////////////////////////

function guessContentType (filename, content) {
  var re = /.*\.([^\.]*)$/;
  var match = re.exec(filename);
  var extension;

  if (match === null) {
    return "text/plain; charset=utf-8";
  }

  extension = match[1];

  if (extension === "html") {
    return "text/html; charset=utf-8";
  }

  if (extension === "xml") {
    return "application/xml; charset=utf-8";
  }

  if (extension === "json") {
    return "application/xml; charset=utf-8";
  }

  return "text/plain; charset=utf-8";
}

////////////////////////////////////////////////////////////////////////////////
/// @brief normalizes a path
////////////////////////////////////////////////////////////////////////////////

ArangoApp.prototype.normalizePath = function (url) {
  if (url === "") {
    url = "/";
  }
  else if (url[0] !== '/') {
    url = "/" + url;
  }

  return url;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief updates a routing entry
////////////////////////////////////////////////////////////////////////////////

ArangoApp.prototype.updateRoute = function (route) {
  var routes;
  var i;

  routes = this._description.routes;

  for (i = 0;  i < routes.length;  ++i) {
    if (routes[i].type === route.type && routes[i].key === route.key) {
      routes[i] = route;
      return;
    }
  }

  routes.push(route);
};

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoDeployment
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief mounts one page directly
////////////////////////////////////////////////////////////////////////////////

ArangoApp.prototype.mountStaticContent = function (url, content, contentType) {
  var pages;
  var name;

  if (url === "") {
    url = "/";
  }
  else if (url[0] !== '/') {
    url = "/" + url;
  }

  if (typeof content !== "string") {
    content = JSON.stringify(content);

    if (contentType === undefined) {
      contentType = "application/json; charset=utf-8";
    }
  }

  if (contentType === undefined) {
    contentType = "text/html; charset=utf-8";
  }

  pages = {
    type: "StaticContent",
    key: url,
    url: { match: url },
    content: {
      contentType: contentType,
      body: content,
      methods: [ "GET", "HEAD" ]
    }
  };

  this.updateRoute(pages);
};

////////////////////////////////////////////////////////////////////////////////
/// @brief mounts a bunch of static pages
////////////////////////////////////////////////////////////////////////////////

ArangoApp.prototype.mountStaticPages = function (url, collection) {
  var pages;
  var name;

  if (typeof collection !== "string") {
    name = collection.name();
  }
  else {
    name = collection;
  }

  url = this.normalizePath(url);

  pages = {
    type: "StaticPages",
    key: url,
    url: { match: url + "/*" },
    action: { 
      controller: "org/arangodb/actions/staticContentController",
      methods: [ "GET", "HEAD" ],
      options: {
	contentCollection: name,
	prefix: url,
	application: this._name
      }
    }
  };

  this.updateRoute(pages);
};

////////////////////////////////////////////////////////////////////////////////
/// @brief saves and reloads
////////////////////////////////////////////////////////////////////////////////

ArangoApp.prototype.save = function () {
  var doc;

  doc = this._routing.replace(this._description, this._description);
  this._description = this._routing.document(doc);

  internal.reloadRouting();
};

////////////////////////////////////////////////////////////////////////////////
/// @brief uploads content from filesystem
////////////////////////////////////////////////////////////////////////////////

ArangoApp.prototype.uploadStaticPages = function (prefix, path) {
  var doc;
  var files;
  var i;
  var route;
  var routes;
  var collection;
  var error;
  var name;

  routes = this._description.routes;
  prefix = this.normalizePath(prefix);

  for (i = 0;  i < routes.length;  ++i) {
    route = routes[i];

    if (route.type === "StaticPages" && route.key === prefix) {
      break;
    }
  }

  if (routes.length <= i) {
    error = new arangodb.ArangoError();
    error.errorNum = arangodb.ERROR_HTTP_NOT_FOUND;
    error.errorMessage = "cannot find path '" + prefix + "' in routing table";

    throw error;
  }

  files = fs.listTree(path);

  name = route.action.options.contentCollection;
  collection = arangodb.db._collection(name);

  if (collection === null) {
    arangodb.db._createDocumentCollection(name);
    collection = arangodb.db._collection(name);
  }

  collection.ensureHashIndex("application", "prefix", "path");

  collection.removeByExample({ application: this._name, prefix: prefix });

  for (i = 0;  i < files.length;  ++i) {
    var content;
    var contentType;
    var file = path + "/" + files[i];
    var subpath = "/" + files[i];

    if (fs.isDirectory(file)) {
      continue;
    }

    content = internal.read(file);

    if (content === null) {
      continue;
    }

    contentType = guessContentType(file, content);

    collection.save({
      application: this._name,
      prefix: prefix,
      path: subpath,
      content: content,
      contentType: contentType
    });

    internal.print("imported '" + subpath + "' of type '" + contentType + "'");
  }
};

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a new app
////////////////////////////////////////////////////////////////////////////////

exports.createApp = function (name) {
  var routing = arangodb.db._collection("_routing");
  var doc = routing.firstExample({ application: name });

  if (doc !== null) {
    var error = new arangodb.ArangoError();
    error.errorNum = arangodb.ERROR_ARANGO_DUPLICATE_IDENTIFIER;
    error.errorMessage = "application name must be unique";

    throw error;
  }

  doc = routing.save({ application: name,
		       urlPrefix: "",
		       routes: [],
		       middleware: [] });

  return new ArangoApp(routing, routing.document(doc));
};

////////////////////////////////////////////////////////////////////////////////
/// @brief loads an existing app
////////////////////////////////////////////////////////////////////////////////

exports.readApp = function (name) {
  var routing = arangodb.db._collection("_routing");
  var doc = routing.firstExample({ application: name });

  if (doc === null) {
    var error = new arangodb.ArangoError();
    error.errorNum = arangodb.ERROR_ARANGO_DOCUMENT_NOT_FOUND;
    error.errorMessage = "application unknown";

    throw error;
  }

  return new ArangoApp(routing, doc);
};

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
