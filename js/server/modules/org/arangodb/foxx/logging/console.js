/*jshint globalstrict: true */
/*global exports, require, Symbol */
'use strict';

////////////////////////////////////////////////////////////////////////////////
/// @brief Foxx logging console
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

exports.for = function (mount, traceAll) {
  traceAll = Boolean(traceAll);

  var console = {};
  var timers = Object.create(null);

  function log(level, message, trace) {
    var doc = {
      mount: mount,
      level: level,
      created: Date.now(),
      message: message
    };

    if (trace) {
      var e = new Error();
      Error.captureStackTrace(e, log);
      var stack = e.stack.replace(/\n$/, '').split('\n').slice(2);
      var tokens = stack[0].replace(/^\s*at\s+/, '').split(':');
      extend(doc, {
        fileName: tokens.slice(0, tokens.length - 2).join(':'),
        lineNumber: Number(tokens[tokens.length - 2]),
        columnNumber: Number(tokens[tokens.length - 1]),
        stack: stack.join('\n').replace(/(^|\n)\s+/g, '$1')
      });
    }

    // TODO write to collection
    require('console').log(doc);
  }

  console.assert = function (condition, message) {
    if (!condition) {
      log('error', new AssertionError({
        message: message,
        actual: condition,
        expected: true,
        operator: '==',
        stackStartFunction: console.assert
      }).stack, traceAll);
    }
  };

  console.dir = function (obj) {
    log('info', util.inspect(obj), traceAll);
  };

  console.time = function (label) {
    var symbol = Symbol.for(label);
    timers[symbol] = Date.now();
  };

  console.timeEnd = function (label) {
    var symbol = Symbol.for(label);
    if (!timers.hasOwnProperty(symbol)) {
      throw new Error('No such label: ' + label);
    }
    log('info', util.format('%s: %dms', label, Date.now() - timers[symbol]), traceAll);
    delete timers[symbol];
  };

  console.trace = function (message) {
    var trace = new Error(message);
    trace.name = 'Trace';
    Error.captureStackTrace(trace, console.trace);
    log('trace', trace.stack, traceAll);
  };

  console._level = function (level, trace) {
    level = String(level);
    trace = Boolean(trace);
    return function () {
      log(level, util.format.apply(null, arguments), trace);
    };
  };

  console.debug = console._level('debug', traceAll);
  console.info = console._level('info', traceAll);
  console.warn = console._level('warn', traceAll);
  console.error = console._level('error', traceAll);
  console.log = console.info;

  return console;
};