/*jshint globalstrict:false, strict:false */

////////////////////////////////////////////////////////////////////////////////
/// @brief tests for Foxx console
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2015 triagens GmbH, Cologne, Germany
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

var jsunity = require('jsunity');
var expect = require('expect.js');
var util = require('util');
var Console = require('@arangodb/foxx/legacy/console').Console;
var db = require('@arangodb').db;
var qb = require('aqb');

var mountPath = '##TEST##';

function ls() {
  'use strict';
  if (!db._foxxlog) {
    return [];
  }
  return db._query(
    qb.for('entry').in('_foxxlog')
    .filter(qb.eq('entry.mount', qb.str(mountPath)))
    .sort('entry.time', 'ASC')
    .return('entry')
  ).toArray();
}

function rmrf() {
  'use strict';
  if (!db._foxxlog) {
    return [];
  }
  return db._query(
    qb.for('entry').in('_foxxlog')
    .filter(qb.eq('entry.mount', qb.str(mountPath)))
    .remove('entry').in('_foxxlog')
  ).toArray();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function ConsoleTestSuite () {
  'use strict';
  var console;

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

    setUp: function () {
      console = new Console(mountPath);
      rmrf();
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

    tearDown: function () {
      rmrf();
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tests
////////////////////////////////////////////////////////////////////////////////

    testConsoleLogLogsMessage: function () {
      rmrf();
      console.log('hi');
      var logs = ls();
      expect(logs.length).to.be(1);
      expect(logs[0]).to.have.property('level', 'INFO');
      expect(logs[0]).to.have.property('levelNum', console._logLevels.INFO);
      expect(logs[0]).to.have.property('message', 'hi');
    },

    testConsoleLogUsesFormat: function () {
      rmrf();
      console.log('%s prevails', 'the goddess');
      console.log('%s plus', 1, 2);
      var logs = ls();
      expect(logs[0]).to.have.property('message', 'the goddess prevails');
      expect(logs[1]).to.have.property('message', '1 plus 2');
    },

    testConsoleLogLogsTime: function () {
      rmrf();
      var min = Date.now();
      console.log('hi');
      var max = Date.now();
      var logs = ls();
      expect(logs[0]).to.have.property('time');
      expect(logs[0].time).to.be.a('number');
      expect(logs[0].time).not.to.be.lessThan(min);
      expect(logs[0].time).not.to.be.greaterThan(max);
    },
    testConsoleTimeLogsDuration: function () {
      rmrf();
      var start = Date.now();
      console.time('hi');
      var min = Date.now();
      while (Date.now() - start < 3) {var a = true; a=false;} // make sure a measurable amount of time passes
      console.timeEnd('hi');
      var max = Date.now();
      expect(max).to.be.greaterThan(min); // sanity checking
      var logs = ls();
      var match = logs[0].message.match(/^([^:]+):\s+(\d+)ms$/);
      expect(match).to.be.ok();
      expect(match[1]).to.equal('hi');
      var elapsed = Number(match[2]);
      expect(elapsed).not.to.be.lessThan(min - start);
      expect(elapsed).not.to.be.greaterThan(max - start);
    },
    testConsoleTimeThrowsForInvalidLabel: function () {
      expect(function () {
        console.timeEnd('this is a label that does not exist');
      }).to.throwError();
    },

    testConsoleDirUsesInspect: function () {
      var args = [
        'lol',
        {_PRINT: function (ctx) {ctx.output += 'hello';}}
      ];
      rmrf();
      args.forEach(function (arg) {
        console.dir(arg);
      });
      var logs = ls();
      args.forEach(function (arg, i) {
        expect(logs[i]).to.have.property('message', util.inspect(arg));
      });
    },

    testConsoleAssertDoesNotThrow: function () {
      rmrf();
      console.setAssertThrows(false);
      expect(function () {
        console.assert(false, 'potato');
      }).not.to.throwError();
      var logs = ls();
      expect(logs.length).to.equal(1);
      expect(logs[0]).to.have.property('level', 'ERROR');
      expect(logs[0]).to.have.property('levelNum', console._logLevels.ERROR);
      expect(logs[0].message).to.match(/AssertionError: potato/);
    },

    testConsoleAssertThrows: function () {
      rmrf();
      console.setAssertThrows(true);
      expect(function () {
        console.assert(false, 'potato');
      }).to.throwError(function (e) {
        expect(e.name).to.be('AssertionError');
        expect(e.message).to.be('potato');
      });
      var logs = ls();
      expect(logs.length).to.equal(1);
      expect(logs[0].message).to.match(/AssertionError: potato/);
    },

    testConsoleLogLevelPreventsLogging: function () {
      rmrf();
      console._logLevels.POTATO = -999;
      console.setLogLevel(-2);
      console.log.level = 'POTATO';
      console.log('potato');
      console.log.level = 'INFO';
      delete console._logLevels.POTATO;
      var logs = ls();
      expect(logs).to.be.empty();
    },
    testConsoleTracingAddsInfo: function () {
      rmrf();
      console.setTracing(false);
      console.log('without tracing');
      console.setTracing(true);
      console.log('with tracing');
      console.setTracing(false);
      var logs = ls();
      expect(logs[0]).not.to.have.property('stack');
      expect(logs[1]).to.have.property('stack');
      expect(logs[1].stack).to.be.an('array');
      expect(logs[1].stack[0]).to.have.property('fileName');
      expect(logs[1].stack[0]).to.have.property('lineNumber');
      expect(logs[1].stack[0]).to.have.property('columnNumber');
    },
    testCustomLogLevels: function () {
      rmrf();
      var log = console.custom('BATMAN', 9000);
      log('potato');
      var logs = ls();
      expect(logs.length).to.equal(1);
      expect(logs[0]).to.have.property('level', 'BATMAN');
      expect(logs[0]).to.have.property('levelNum', 9000);
      expect(logs[0]).to.have.property('message', 'potato');
    },

    testTrace: function () {
      rmrf();
      console.trace('wat');
      var logs = ls();
      expect(logs.length).to.equal(1);
      expect(logs[0]).to.have.property('level', 'TRACE');
      expect(logs[0]).to.have.property('levelNum', console._logLevels.TRACE);
      expect(logs[0].message).to.match(/^Trace: wat\n\s+at/);
    },

    testLogsListWithDefaults: function () {
      rmrf();
      console.log('sup');
      console.log('banana');
      var logs = console.logs.list();
      expect(logs.length).to.be(2);
      expect(logs[0]).to.have.property('message', 'sup');
      expect(logs[1]).to.have.property('message', 'banana');
    },

    testLogsListWithSortDESC: function () {
      rmrf();
      console.log('sup');
      console.log('banana');
      var logs = console.logs.list({sort: 'DESC'});
      expect(logs.length).to.be(2);
      expect(logs[0]).to.have.property('message', 'banana');
      expect(logs[1]).to.have.property('message', 'sup');
    },

    testLogsListWithLimit: function () {
      rmrf();
      console.log('sup');
      console.log('banana');
      var logs = console.logs.list({limit: 1});
      expect(logs.length).to.be(1);
      expect(logs[0]).to.have.property('message', 'sup');
    },

    testLogsListWithLimitAndOffset: function () {
      rmrf();
      console.log('sup');
      console.log('banana');
      var logs = console.logs.list({limit: 1, offset: 1});
      expect(logs.length).to.be(1);
      expect(logs[0]).to.have.property('message', 'banana');
    },

    testLogsListWithMinLevel: function () {
      rmrf();
      var logs;
      console.debug('lol');
      console.error('hey');
      logs = console.logs.list({minLevel: 'DEBUG'});
      expect(logs.length).to.be(2);
      logs = console.logs.list({minLevel: console._logLevels.DEBUG + 1});
      expect(logs.length).to.be(1);
      expect(logs[0]).to.have.property('message', 'hey');
      logs = console.logs.list({minLevel: console._logLevels.ERROR + 1});
      expect(logs.length).to.be(0);
    },

    testLogsListWithLevel: function () {
      rmrf();
      var logs;
      console.debug('lol');
      console.error('hey');
      logs = console.logs.list({level: 'DEBUG'});
      expect(logs.length).to.be(1);
      expect(logs[0]).to.have.property('message', 'lol');
    },

    testLogsSearchByFileNameFailsWithoutTracing: function () {
      console.setTracing(false);
      expect(function () {
        console.logs.searchByFileName('lol');
      }).to.throwError(function (e) {
        expect(e.message).to.match(/tracing/i);
      });
    },

    testLogsSearchByFileName: function () {
      rmrf();
      console.setTracing(true);
      db._foxxlog.save({
        mount: mountPath,
        time: Date.now(),
        stack: [
          {fileName: 'abcdef'}
        ]
      });
      expect(console.logs.searchByFileName('bcd')).to.have.property('length', 1);
      expect(console.logs.searchByFileName('ab')).to.have.property('length', 1);
      expect(console.logs.searchByFileName('ef')).to.have.property('length', 1);
      expect(console.logs.searchByFileName('fail')).to.have.property('length', 0);
    },

    testLogsSearchByMessage: function () {
      rmrf();
      console.log('abcdef');
      expect(console.logs.searchByMessage('bcd')).to.have.property('length', 1);
      expect(console.logs.searchByMessage('ab')).to.have.property('length', 1);
      expect(console.logs.searchByMessage('ef')).to.have.property('length', 1);
      expect(console.logs.searchByMessage('fail')).to.have.property('length', 0);
    }
  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(ConsoleTestSuite);

return jsunity.done();

