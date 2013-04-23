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

var internal = require("internal");

var fs = require("fs");

var arangodb = require("org/arangodb");

var guessContentType = arangodb.guessContentType;

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
/// @brief normalizes a path
////////////////////////////////////////////////////////////////////////////////

function normalizePath (url) {
  if (url === "") {
    url = "/";
  }
  else if (url[0] !== '/') {
    url = "/" + url;
  }

  return url;
}

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
/// @brief prints an application
////////////////////////////////////////////////////////////////////////////////

ArangoApp.prototype._PRINT = function (context) {
  context.output += '[ArangoApp "' + this._name + '" at "' + this._description.urlPrefix + '"]';
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
/// @brief mounts one function directly
////////////////////////////////////////////////////////////////////////////////

ArangoApp.prototype.mountStaticFunction = function (url, func, methods) {
  var pages;

  if (url === "") {
    url = "/";
  }
  else if (url[0] !== '/') {
    url = "/" + url;
  }

  if (methods === undefined) {
    methods = [ "GET", "HEAD" ];
  }

  pages = {
    type: "StaticFunction",
    key: url,
    url: { match: url },
    action: {
      'function': String(func),
      methods: methods
    }
  };

  this.updateRoute(pages);

  return this;
};

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

  return this;
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

  url = normalizePath(url);

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

  return this;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief mounts a simple action
////////////////////////////////////////////////////////////////////////////////

ArangoApp.prototype.mountAction = function (url, func, methods) {
  var pages;
  var name;

  url = normalizePath(url);

  if (methods === undefined) {
    methods = [ "GET", "HEAD" ];
  }

  if (! (methods instanceof Array)) {
    var error = new arangodb.ArangoError();
    error.errorNum = arangodb.ERROR_BAD_PARAMETER;
    error.errorMessage = "methods must be a list of HTTP methods, i. e. [\"GET\"]";

    throw error;
  }

  pages = {
    type: "Action",
    key: url,
    url: { match: url },
    action: { 
      'do': func,
      methods: methods,
      options: {
        path: url,
        application: this._name
      }
    }
  };

  this.updateRoute(pages);

  return this;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief changes the common prefix
////////////////////////////////////////////////////////////////////////////////

ArangoApp.prototype.setPrefix = function (prefix) {
  if (prefix !== "" && prefix[prefix.length - 1] === '/') {
    prefix = prefix.slice(0, prefix.length - 1);
  }

  if (prefix !== "" && prefix[0] !== '/') {
    prefix = "/" + prefix;
  }

  if (prefix === "/") {
    prefix = "";
  }

  this._description.urlPrefix = prefix;

  return this;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief gets the common prefix
////////////////////////////////////////////////////////////////////////////////

ArangoApp.prototype.prefix = function () {
  return this._description.urlPrefix;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief saves and reloads
////////////////////////////////////////////////////////////////////////////////

ArangoApp.prototype.save = function () {
  var doc;

  doc = this._routing.replace(this._description, this._description);
  this._description = this._routing.document(doc);

  internal.reloadRouting();

  return this;
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
  var re = /.*\/([^\/]*)$/;

  routes = this._description.routes;
  prefix = normalizePath(prefix);

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
    var filename;

    if (fs.isDirectory(file)) {
      continue;
    }

    content = fs.read(file);

    if (content === null) {
      continue;
    }

    filename = re.exec(files[i]);

    if (filename !== null) {
      filename = filename[1];
    }
    else {
      filename = files[i];
    }

    contentType = guessContentType(file);

    collection.save({
      application: this._name,
      prefix: prefix,
      path: subpath,
      content: content,
      contentType: contentType,
      filename: filename
    });

    arangodb.print("imported '" + subpath + "' of type '" + contentType + "'");
  }

  return this;
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
/// @brief loads a module directory into the database
////////////////////////////////////////////////////////////////////////////////

exports.uploadModules = function (prefix, path) {
  var i;
  var files;
  var re = /^(.*)\.js$/;

  prefix = normalizePath(prefix);
  files = fs.listTree(path);

  for (i = 0;  i < files.length;  ++i) {
    var content;
    var mpath;
    var file = path + "/" + files[i];
  
    if (fs.isDirectory(file)) {
      continue;
    }

    mpath = re.exec(files[i]);

    if (mpath === null) {
      arangodb.print("skipping file '" + files[i] + "' of unknown type, expecting .js");
      continue;
    }
    
    mpath = prefix + "/" + mpath[1];

    arangodb.defineModule(mpath, file);

    arangodb.print("imported '" + mpath + "'");
  }

  internal.flushServerModules();
  internal.wait(1);
  internal.reloadRouting();
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
