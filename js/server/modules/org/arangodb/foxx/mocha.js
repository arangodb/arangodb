/*jshint globalstrict: true */
/*global global, require, exports */
'use strict';

////////////////////////////////////////////////////////////////////////////////
/// @brief Foxx Mocha integration
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2015 triAGENS GmbH, Cologne, Germany
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
/// @author Alan Plum
/// @author Copyright 2015, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var mocha = require('mocha');
var util = require('util');
var EventEmitter = require('events').EventEmitter;

function Mocha() {
  this.suite = new mocha.Suite('', new mocha.Context());
  this.options = {};
  this.suite.on('pre-require', function (context) {
    require('console').log('########', Date(), 'pre-require', context);
  });
  Object.keys(mocha.interfaces).forEach(function (key) {
    mocha.interfaces[key](this.suite);
  }.bind(this));
  EventEmitter.call(this);
}
util.inherits(Mocha, EventEmitter);

Mocha.prototype.run = function () {
  var runner = new mocha.Runner(this.suite, false);
  var reporter = new mocha.reporters.JSON(runner, this.options);
  runner.run();
  return reporter.stats;
};

Mocha.prototype.loadFiles = function (app) {
  var files = app._manifest.tests ? app._manifest.tests.slice() : [];
  files.forEach(function (file) {
    var context = {};
    this.suite.emit('pre-require', context, file, this);
    this.suite.emit('require', app.loadAppScript(file, {context: context}), file, this);
    this.suite.emit('post-require', global, file, this);
  }.bind(this));
};

exports.run = function (app) {
  var m = new Mocha();
  m.loadFiles(app);
  return m.run();
};