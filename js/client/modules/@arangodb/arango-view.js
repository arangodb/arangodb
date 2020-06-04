/*jshint strict: false */

// /////////////////////////////////////////////////////////////////////////////
// @brief ArangoView
// 
// @file
// 
// DISCLAIMER
// 
// Copyright 2013 triagens GmbH, Cologne, Germany
// 
// Licensed under the Apache License, Version 2.0 (the "License")
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
// 
//     http://www.apache.org/licenses/LICENSE-2.0
// 
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
// 
// Copyright holder is triAGENS GmbH, Cologne, Germany
// 
// @author Daniel H. Larkin
// @author Copyright 2012-2013, triAGENS GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

const internal = require('internal');
const arangosh = require('@arangodb/arangosh');
const ArangoError = require('@arangodb').ArangoError;

// //////////////////////////////////////////////////////////////////////////////
// / @brief constructor
// //////////////////////////////////////////////////////////////////////////////

function ArangoView (database, data) {
  this._database = database;
  this._dbName = database._name();
  this._dbPrefix = '/_db/' + encodeURIComponent(database._name());

  if (typeof data === 'string') {
    this._id = null;
    this._name = data;
    this._type = null;
  }
  else if (data !== undefined) {
    this._id = data.id;
    this._name = data.name;
    this._type = data.type;
  } else {
    this._id = null;
    this._name = null;
    this._type = null;
  }
}

exports.ArangoView = ArangoView;

// /////////////////////////////////////////////////////////////////////////////
// @brief pretty print
// /////////////////////////////////////////////////////////////////////////////

ArangoView.prototype._PRINT = function (context) {
  const type = this.type();
  const name = this.name();

  const colors = internal.COLORS;
  const useColor = context.useColor;

  context.output += '[ArangoView ';
  if (useColor) { context.output += colors.COLOR_NUMBER; }
  context.output += this._id;
  if (useColor) { context.output += colors.COLOR_RESET; }
  context.output += ', "';
  if (useColor) { context.output += colors.COLOR_STRING; }
  context.output += name || 'unknown';
  if (useColor) { context.output += colors.COLOR_RESET; }
  context.output += '" (type ' + type + ')]';
};

// /////////////////////////////////////////////////////////////////////////////
// @brief prefix a URL with the database name of the view
// /////////////////////////////////////////////////////////////////////////////

ArangoView.prototype._prefixurl = function (url) {
  if (url.substr(0, 5) === '/_db/') {
    return url;
  }

  if (url[0] === '/') {
    return this._dbPrefix + url;
  }
  return this._dbPrefix + '/' + url;
};

// /////////////////////////////////////////////////////////////////////////////
// @brief return the base url for view usage
// /////////////////////////////////////////////////////////////////////////////

ArangoView.prototype._baseurl = function (suffix) {
  var url = this._database._viewurl(this.name());

  if (suffix) {
    url += '/' + suffix;
  }

  return this._prefixurl(url);
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief print the help for ArangoView
// //////////////////////////////////////////////////////////////////////////////

var helpArangoView = arangosh.createHelpHeadline('ArangoView help') +
  'ArangoView constructor:                                             ' + '\n' +
  ' > view = db._view("myview");                                       ' + '\n' +
  ' > col = db._createView("myview", "type", {[properties]});          ' + '\n' +
  '                                                                          ' + '\n' +
  'Administration Functions:                                                 ' + '\n' +
  '  name()                                view name                   ' + '\n' +
  '  type()                                type of the view            ' + '\n' +
  '  properties()                          show view properties        ' + '\n' +
  '  drop()                                delete a view               ' + '\n' +
  '  _help()                               this help                         ' + '\n' +
  '                                                                          ' + '\n' +
  'Attributes:                                                               ' + '\n' +
  '  _database                             database object                   ' + '\n' +
  '  _id                                   view identifier             ';

ArangoView.prototype._help = function () {
  internal.print(helpArangoView);
};

// /////////////////////////////////////////////////////////////////////////////
// @brief gets the name of a view
// /////////////////////////////////////////////////////////////////////////////

ArangoView.prototype.name = function () {
  if (this._name === null) {
    this.refresh();
  }

  return this._name;
};

// /////////////////////////////////////////////////////////////////////////////
// @brief gets the type of a view
// /////////////////////////////////////////////////////////////////////////////

ArangoView.prototype.type = function () {
  if (this._type === null) {
    this.refresh();
  }

  return this._type;
};

// /////////////////////////////////////////////////////////////////////////////
// @brief gets or sets the properties of a view
// /////////////////////////////////////////////////////////////////////////////

ArangoView.prototype.properties = function (properties, partialUpdate) {
  var requestResult;

  if (properties === undefined) {
    requestResult = this._database._connection.GET(this._baseurl('properties'));
  } else if (partialUpdate === undefined || partialUpdate === true) {
    requestResult = this._database._connection.PATCH(
      this._baseurl('properties'), properties
    );
  } else {
    requestResult = this._database._connection.PUT(
      this._baseurl('properties'), properties
    );
  }

  arangosh.checkRequestResult(requestResult);

  const mask = {
    'code': true,
    'globallyUniqueId': true,
    'id': true,
    'name': true,
    'type': true,
  };
  var result = {};

  // remove masked attributes from result
  for (let attr in requestResult) {
    if (!mask.hasOwnProperty(attr) && requestResult[attr] !== undefined) {
      result[attr] = requestResult[attr];
    }
  }

  return result;
};

// /////////////////////////////////////////////////////////////////////////////
// @brief drops a view
// /////////////////////////////////////////////////////////////////////////////

ArangoView.prototype.drop = function (options) {
  var requestResult;
  if (typeof options === 'object' && options.isSystem) {
    requestResult = this._database._connection.DELETE(this._baseurl() + '?isSystem=true');
  } else {
    requestResult = this._database._connection.DELETE(this._baseurl());
  }

  if (requestResult !== null
    && requestResult !== undefined
    && requestResult.error === true
    && requestResult.errorNum !== internal.errors.ERROR_ARANGO_DATA_SOURCE_NOT_FOUND.code) {
    // check error in case we got anything else but "view not found"
    arangosh.checkRequestResult(requestResult);
  }

  var database = this._database;
  var name;

  for (name in database) {
    if (database.hasOwnProperty(name)) {
      var view = database[name];

      if (view instanceof ArangoView) {
        if (view.name() === this.name()) {
          delete database[name];
        }
      }
    }
  }
};

// /////////////////////////////////////////////////////////////////////////////
// @brief renames a view
// /////////////////////////////////////////////////////////////////////////////

ArangoView.prototype.rename = function (name) {
  var body = { name: name };
  var requestResult = this._database._connection.PUT(this._baseurl('rename'), body);

  arangosh.checkRequestResult(requestResult);

  delete this._database[this._name];
  this._database[name] = this;
  this._name = name;
};
