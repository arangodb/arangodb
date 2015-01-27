/*jshint globalstrict: true */
/*global require, module */
'use strict';

////////////////////////////////////////////////////////////////////////////////
/// @brief Foxx logging
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

var util = require('util');
var extend = require('underscore').extend;
var AssertionError = require('assert').AssertionError;
var db = require('org/arangodb').db;

var Console = function (mount, tracing) {
  this._mount = mount;
  this._timers = Object.create(null);
  this._tracing = Boolean(tracing);
  this._logLevel = -999;
  this._logLevels = {TRACE: -2};

  Object.keys(Console.prototype).forEach(function (name) {
    if (typeof this[name] === 'function') {
      this[name] = this[name].bind(this);
    }
  }.bind(this));

  this.debug = this._withLevel('DEBUG', -1);
  this.info = this._withLevel('INFO', 0);
  this.warn = this._withLevel('WARN', 1);
  this.error = this._withLevel('ERROR', 2);

  this.assert._level = 'ERROR';
  this.dir._level = 'INFO';
  this.log._level = 'INFO';
  this.time._level = 'INFO';
  this.trace._level = 'TRACE';
};

extend(Console.prototype, {
  _log: function (level, message, callee) {
    if (!this._logLevels.hasOwnProperty(level)) {
      throw new Error('Unknown log level: ' + level);
    }

    if (this._logLevels[level] < this._logLevel) {
      // Don't log this.
      return;
    }

    var doc = {
      mount: this._mount,
      level: level,
      levelNum: this._logLevels[level],
      created: Date.now(),
      message: message
    };

    if (this._tracing) {
      var e = new Error();
      Error.captureStackTrace(e, callee || this._log);
      doc.stack = e.stack.replace(/\n$/, '').split('\n').slice(2)
      .map(function (line) {
        var tokens = line.replace(/^\s*at\s+/, '').split(':');
        return {
          fileName: tokens.slice(0, tokens.length - 2).join(':'),
          lineNumber: Number(tokens[tokens.length - 2]),
          columnNumber: Number(tokens[tokens.length - 1])
        };
      });
    }

    if (!db._foxxlog) {
      db._create('_foxxlog', {isSystem: true});
      db._foxxlog.ensureSkiplist('mount', 'created');
      db._foxxlog.ensureSkiplist('level', 'created');
      db._foxxlog.ensureSkiplist('levelNum', 'created');
      db._foxxlog.ensureSkiplist('stack[0].fileName');
      db._foxxlog.ensureFulltextIndex('message');
    }

    db._foxxlog.save(doc);
  },

  assert: function (condition, message) {
    if (!condition) {
      this._log(this.assert._level, new AssertionError({
        message: message,
        actual: condition,
        expected: true,
        operator: '==',
        stackStartFunction: this.assert
      }).stack,
      this.assert);
    }
  },

  dir: function (obj) {
    this._log(this.dir._level, util.inspect(obj));
  },

  log: function () {
    this._log(this.log._level, util.format.apply(null, arguments));
  },

  time: function (label) {
    this._timers[label] = Date.now();
  },

  timeEnd: function (label) {
    if (!this._timers.hasOwnProperty(label)) {
      throw new Error('No such label: ' + label);
    }
    this._log(
      this.time._level,
      util.format('%s: %dms', label, Date.now() - this._timers[label]),
      this.time
    );
    delete this._timers[label];
  },

  trace: function (message) {
    var trace = new Error(message);
    trace.name = 'Trace';
    Error.captureStackTrace(trace, this.trace);
    this._log(this.trace._level, trace.stack);
  },

  _withLevel: function (level, weight) {
    level = String(level);
    weight = Number(weight);
    weight = weight === weight ? weight : 999;
    this._logLevels[level] = weight;
    var logWithLevel = function() {
      this._log(level, util.format.apply(null, arguments), logWithLevel);
    }.bind(this);
    return logWithLevel;
  },

  _setLogLevel: function (level) {
    if (typeof level === 'string') {
      if (!this._logLevels.hasOwnProperty(level)) {
        throw new Error('Unknown log level: ' + level);
      }
      level = this._logLevels[level];
    }
    this._logLevel = typeof level === 'number' ? level : -999;
    return this._logLevel;
  },

  _setTracing: function (tracing) {
    this._tracing = Boolean(tracing);
    return this._tracing;
  }
});

module.exports = extend(
  function (mount, tracing) {
    return new Console(mount, tracing);
  },
  {Console: Console}
);