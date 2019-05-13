////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2019 ArangoDB GmbH, Cologne, Germany
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
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Andrey Abramov
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

const internal = require('internal');
const arangosh = require('@arangodb/arangosh');
const errors = require('@arangodb').errors;

////////////////////////////////////////////////////////////////////////////////
/// @brief ArangoAnalyzer instance
////////////////////////////////////////////////////////////////////////////////

function ArangoAnalyzer(data) {
  this._data = data;
}

ArangoAnalyzer.prototype.features = function() {
  return this._data['features'];
};

ArangoAnalyzer.prototype.name = function() {
  return this._data['name'];
};

ArangoAnalyzer.prototype.properties = function() {
  return this._data['properties'];
};

ArangoAnalyzer.prototype.type = function() {
  return this._data['type'];
};

ArangoAnalyzer.prototype._help = function () {
  var help = arangosh.createHelpHeadline('ArangoAnalyzers help') +
    'ArangoAnalyzer constructor:                                        ' + '\n' +
    ' > var analyzer = require("@arangodb/analyzers").analyzer(<name>); ' + '\n' +
    '                                                                   ' + '\n' +
    'Administration Functions:                                          ' + '\n' +
    '  name()                                analyzer name              ' + '\n' +
    '  type()                                analyzer type              ' + '\n' +
    '  properties()                          analyzer properties        ' + '\n' +
    '  features()                            analyzer features          ' + '\n' +
    '  _help()                               this help                  ' + '\n' +
    '                                                                   ' + '\n' +
    'Attributes:                                                        ' + '\n' +
    '  _data                                 server-side definition     ';
  internal.print(help);
};

///////////////////////////////////////////////////////////////////////////////
/// @brief pretty print
///////////////////////////////////////////////////////////////////////////////

ArangoAnalyzer.prototype._PRINT = function(context) {
  var colors = internal.COLORS;
  var useColor = context.useColor;

  context.output += '[ArangoAnalyzer "';
  if (useColor) { context.output += colors.COLOR_STRING; }
  context.output += this.name();
  if (useColor) { context.output += colors.COLOR_RESET; }
  context.output += '" (type ' + this.type() + ')]';
};

exports.ArangoAnalyzer = ArangoAnalyzer;

////////////////////////////////////////////////////////////////////////////////
/// @brief return the base url for analyzers usage
////////////////////////////////////////////////////////////////////////////////

var _baseurl = function(suffix) {
  var url = '/_api/analyzer';

  if (suffix) {
    url += '/' + encodeURIComponent(suffix);
  }

  return url;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief exported members and functions
////////////////////////////////////////////////////////////////////////////////

exports.analyzer = function(name) {
  var db = internal.db;
  var url = _baseurl(name);
  var result = db._connection.GET(url);

  if (result.hasOwnProperty('error')
     && result.hasOwnProperty('error')
     && errors.ERROR_ARANGO_DOCUMENT_NOT_FOUND.code === result.errorNum) {
    return null;
  }

  arangosh.checkRequestResult(result);

  return new ArangoAnalyzer(result);
};

exports.remove = function(name, force) {
  var db = internal.db;
  var url = _baseurl(name);

  if (force !== undefined) {
    url += '?force=true';
  }

  var result = db._connection.DELETE(url);

  arangosh.checkRequestResult(result);
};

exports.save = function(name, type, properties, features) {
  var body = {};

  if (name !== undefined) {
    body['name'] = name;
  }

  if (type !== undefined) {
    body['type'] = type;
  }

  if (properties !== undefined) {
    body['properties'] = properties;
  }

  if (features !== undefined) {
    body['features'] = features;
  }

  var db = internal.db;
  var url = _baseurl();
  var result = db._connection.POST(url, body);

  return arangosh.checkRequestResult(result);
};

exports.toArray = function() {
  var db = internal.db;
  var url = _baseurl();
  var result = db._connection.GET(url);

  arangosh.checkRequestResult(result);

  var list = [];

  for (var i = 0; i < result.result.length; ++i) {
    list.push(new ArangoAnalyzer(result.result[i]));
  }

  return list;
};
