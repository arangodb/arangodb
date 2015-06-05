/*jshint globalstrict: true */
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

var qb = require('aqb');
var util = require('util');
var extend = require('underscore').extend;
var ErrorStackParser = require('error-stack-parser');
var AssertionError = require('assert').AssertionError;
var exists = require('org/arangodb/is').existy;
var db = require('org/arangodb').db;

function ConsoleLogs(console) {
  this._console = console;
  this.defaultMaxAge = 2 * 60 * 60 * 1000;
}

extend(ConsoleLogs.prototype, {
  _query: function (cfg) {
    if (!db._foxxlog) {
      return [];
    }

    if (!cfg.opts) {
      cfg.opts = {};
    }

    var query = qb.for('entry').in('_foxxlog');

    query = query.filter(qb.eq('entry.mount', qb.str(this._console._mount)));

    query = query.filter(qb.gte(
      'entry.time',
      exists(cfg.opts.startTime)
      ? qb.num(cfg.opts.startTime)
      : Date.now() - this.defaultMaxAge
    ));

    if (exists(cfg.opts.endTime)) {
      query = query.filter(qb.lte('entry.time', qb.num(cfg.opts.endTime)));
    }

    if (exists(cfg.opts.level)) {
      query = query.filter(
        typeof cfg.opts.level === 'number'
        ? qb.eq('entry.levelNum', qb.num(cfg.opts.level))
        : qb.eq('entry.level', qb.str(cfg.opts.level))
      );
    }

    if (exists(cfg.opts.minLevel)) {
      var levelNum = cfg.opts.minLevel;
      if (typeof levelNum !== 'number') {
        if (!this._console._logLevels.hasOwnProperty(levelNum)) {
          throw new Error('Unknown log level: ' + levelNum);
        }
        levelNum = this._console._logLevels[levelNum];
      }
      query = query.filter(qb.gte('entry.levelNum', qb.num(levelNum)));
    }

    if (exists(cfg.fileName)) {
      query = query.filter(qb.LIKE('entry.stack[*].fileName', qb.str('%' + cfg.fileName + '%'), true));
    }

    if (exists(cfg.message)) {
      query = query.filter(qb.LIKE('entry.message', qb.str('%' + cfg.message + '%'), true));
    }

    query = query.sort('entry.time', 'ASC');

    if (exists(cfg.opts.limit)) {
      if (exists(cfg.opts.offset)) {
        query = query.limit(cfg.opts.offset, cfg.opts.limit);
      } else {
        query = query.limit(cfg.opts.limit);
      }
    }

    query = query.return('entry');

    var result = db._query(query).toArray();

    if (cfg.opts.sort && cfg.opts.sort.toUpperCase() === 'DESC') {
      return result.reverse();
    }

    return result;
  },
  list: function (opts) {
    return this._query({opts: opts});
  },
  searchByMessage: function (message, opts) {
    return this._query({message: message, opts: opts});
  },
  searchByFileName: function (fileName, opts) {
    if (!this._console._tracing) {
      throw new Error('Tracing must be enabled in order to search by filename.');
    }
    return this._query({fileName: fileName, opts: opts});
  }
});

function Console(mount, tracing) {
  this._mount = mount;
  this._timers = Object.create(null);
  this._tracing = Boolean(tracing);
  this._logLevel = -999;
  this._logLevels = {TRACE: -2};
  this._assertThrows = false;
  this.logs = new ConsoleLogs(this);

  Object.keys(Console.prototype).forEach(function (name) {
    if (typeof this[name] === 'function') {
      this[name] = this[name].bind(this);
    }
  }.bind(this));

  this.debug = this.custom('DEBUG', -1);
  this.info = this.custom('INFO', 0);
  this.warn = this.custom('WARN', 1);
  this.error = this.custom('ERROR', 2);

  this.assert.level = 'ERROR';
  this.dir.level = 'INFO';
  this.log.level = 'INFO';
  this.time.level = 'INFO';
  this.trace.level = 'TRACE';
}

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
      time: Date.now(),
      message: message
    };

    if (this._tracing) {
      var e = new Error();
      Error.captureStackTrace(e, callee || this._log);
      e.stack = e.stack.replace(/\n+$/, '');
      doc.stack = ErrorStackParser.parse(e).slice(1);
    }

    if (!db._foxxlog) {
      db._create('_foxxlog', {isSystem: true});
      db._foxxlog.ensureSkiplist('mount');
      db._foxxlog.ensureSkiplist('mount', 'created');
      db._foxxlog.ensureSkiplist('mount', 'created', 'levelNum');
    }

    db._foxxlog.save(doc);
  },

  assert: function (condition, message) {
    if (!condition) {
      var err = new AssertionError({
        message: message,
        actual: condition,
        expected: true,
        operator: '==',
        stackStartFunction: this.assert
      });
      this._log(this.assert.level, err.stack, this.assert);
      if (this._assertThrows) {
        throw err;
      }
    }
  },

  dir: function (obj) {
    this._log(this.dir.level, util.inspect(obj));
  },

  log: function () {
    this._log(this.log.level, util.format.apply(null, arguments));
  },

  time: function (label) {
    this._timers[label] = Date.now();
  },

  timeEnd: function (label) {
    if (!Object.prototype.hasOwnProperty.call(this._timers, label)) {
      throw new Error('No such label: ' + label);
    }
    this._log(
      this.time.level,
      util.format('%s: %dms', label, Date.now() - this._timers[label]),
      this.time
    );
    delete this._timers[label];
  },

  trace: function (message) {
    var trace = new Error(message);
    trace.name = 'Trace';
    Error.captureStackTrace(trace, this.trace);
    this._log(this.trace.level, trace.stack);
  },

  custom: function (level, weight) {
    level = String(level);
    weight = Number(weight);
    weight = weight === weight ? weight : 999;
    this._logLevels[level] = weight;
    var logWithLevel = function() {
      this._log(level, util.format.apply(null, arguments), logWithLevel);
    }.bind(this);
    return logWithLevel;
  },

  setLogLevel: function (level) {
    if (typeof level === 'string') {
      if (!this._logLevels.hasOwnProperty(level)) {
        throw new Error('Unknown log level: ' + level);
      }
      level = this._logLevels[level];
    }
    this._logLevel = typeof level === 'number' ? level : -999;
    return this._logLevel;
  },

  setTracing: function (tracing) {
    this._tracing = Boolean(tracing);
    return this._tracing;
  },

  setAssertThrows: function (assertThrows) {
    this._assertThrows = Boolean(assertThrows);
    return this._assertThrows;
  }
});

module.exports = extend(
  function (mount, tracing) {
    return new Console(mount, tracing);
  },
  {Console: Console}
);