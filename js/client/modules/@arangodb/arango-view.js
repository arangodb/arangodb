/*jshint strict: false */

// //////////////////////////////////////////////////////////////////////////////
// / @brief ArangoView
// /
// / @file
// /
// / DISCLAIMER
// /
// / Copyright 2013 triagens GmbH, Cologne, Germany
// /
// / Licensed under the Apache License, Version 2.0 (the "License")
// / you may not use this file except in compliance with the License.
// / You may obtain a copy of the License at
// /
// /     http://www.apache.org/licenses/LICENSE-2.0
// /
// / Unless required by applicable law or agreed to in writing, software
// / distributed under the License is distributed on an "AS IS" BASIS,
// / WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// / See the License for the specific language governing permissions and
// / limitations under the License.
// /
// / Copyright holder is triAGENS GmbH, Cologne, Germany
// /
// / @author Daniel H. Larkin
// / @author Copyright 2012-2013, triAGENS GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

var internal = require('internal');
var arangosh = require('@arangodb/arangosh');

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

var ArangoError = require('@arangodb').ArangoError;

// //////////////////////////////////////////////////////////////////////////////
// / @brief append the waitForSync parameter to a URL
// //////////////////////////////////////////////////////////////////////////////

ArangoView.prototype._appendSyncParameter = function (url, waitForSync) {
  if (waitForSync) {
    if (url.indexOf('?') === -1) {
      url += '?';
    }else {
      url += '&';
    }
    url += 'waitForSync=true';
  }
  return url;
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief append some boolean parameter to a URL
// //////////////////////////////////////////////////////////////////////////////

ArangoView.prototype._appendBoolParameter = function (url, name, val) {
  if (url.indexOf('?') === -1) {
    url += '?';
  }else {
    url += '&';
  }
  url += name + (val ? '=true' : '=false');
  return url;
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief prefix a URL with the database name of the view
// //////////////////////////////////////////////////////////////////////////////

ArangoView.prototype._prefixurl = function (url) {
  if (url.substr(0, 5) === '/_db/') {
    return url;
  }

  if (url[0] === '/') {
    return this._dbPrefix + url;
  }
  return this._dbPrefix + '/' + url;
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief return the base url for view usage
// //////////////////////////////////////////////////////////////////////////////

ArangoView.prototype._baseurl = function (suffix) {
  var url = this._database._viewurl(this.name());

  if (suffix) {
    url += '/' + suffix;
  }

  return this._prefixurl(url);
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief converts into an array
// //////////////////////////////////////////////////////////////////////////////

ArangoView.prototype.toArray = function () {
  return this.all().toArray();
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

// //////////////////////////////////////////////////////////////////////////////
// / @brief gets the name of a view
// //////////////////////////////////////////////////////////////////////////////

ArangoView.prototype.name = function () {
  if (this._name === null) {
    this.refresh();
  }

  return this._name;
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief gets the type of a view
// //////////////////////////////////////////////////////////////////////////////

ArangoView.prototype.type = function () {
  if (this._type === null) {
    this.refresh();
  }

  return this._type;
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief gets or sets the properties of a view
// //////////////////////////////////////////////////////////////////////////////

ArangoView.prototype.properties = function (properties, partialUpdate) {
  var attributes = {
    'locale': true,
    'links': true,
    'commit': true,
  };
  var a;

  var requestResult;

  if (properties === undefined) {
    requestResult = this._database._connection.GET(this._baseurl('properties'));
  } else if (partialUpdate === undefined || partialUpdate === true) {
    requestResult = this._database._connection.PATCH(
      this._baseurl('properties'), JSON.stringify(properties)
    );
  } else {
    requestResult = this._database._connection.PUT(
      this._baseurl('properties'), JSON.stringify(properties)
    );
  }

  arangosh.checkRequestResult(requestResult);

  var result = { };
  for (a in attributes) {
    if (attributes.hasOwnProperty(a) &&
      requestResult.hasOwnProperty(a) &&
      requestResult[a] !== undefined) {
      result[a] = requestResult[a];
    }
  }

  return result;
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief drops a view
// //////////////////////////////////////////////////////////////////////////////

ArangoView.prototype.drop = function () {
  var requestResult = this._database._connection.DELETE(this._baseurl());

  if (requestResult !== null
    && requestResult.error === true
    && requestResult.errorNum !== internal.errors.ERROR_ARANGO_DATA_SOURCE_NOT_FOUND.code) {
    // check error in case we got anything else but "view not found"
    arangosh.checkRequestResult(requestResult);
  } else {
    this._database._unregisterView(this._name);
  }
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief renames a view
// //////////////////////////////////////////////////////////////////////////////

ArangoView.prototype.rename = function (name) {
  var body = { name: name };
  var requestResult = this._database._connection.PUT(this._baseurl('rename'), JSON.stringify(body));

  arangosh.checkRequestResult(requestResult);

  this._database._renameView(this._name, name);
  this._name = name;
};
