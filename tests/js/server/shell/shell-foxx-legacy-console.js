/* jshint globalstrict:false, strict:false */

// //////////////////////////////////////////////////////////////////////////////
// / @brief tests for Foxx console
// /
// / @file
// /
// / DISCLAIMER
// /
// / Copyright 2015 triagens GmbH, Cologne, Germany
// /
// / Licensed under the Apache License, Version 2.0 (the "License");
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
// / @author Alan Plum
// / @author Copyright 2015, triAGENS GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

var jsunity = require('jsunity');
var expect = require('chai').expect;
var util = require('util');
var Console = require('@arangodb/foxx/legacy/console').Console;
var db = require('@arangodb').db;
var qb = require('aqb');
var internal = require('internal');
var AssertionError = require('assert').AssertionError;
var mountPath = '##TEST##';

require("@arangodb/test-helper").waitForFoxxInitialized();

var foxxlogCol = '_foxxlog';
function clear () {
  'use strict';
  if (!db._foxxlog) {
    return [];
  }
  return db._query(
    qb.for('entry').in(foxxlogCol)
    .filter(qb.eq('entry.mount', qb.str(mountPath)))
    .remove('entry').in(foxxlogCol)
  ).toArray();
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief test suite
// //////////////////////////////////////////////////////////////////////////////

function ConsoleTestSuite () {
  'use strict';
  let console;

  return {

// //////////////////////////////////////////////////////////////////////////////
// / @brief set up
// //////////////////////////////////////////////////////////////////////////////

    setUp: function () {
      console = new Console(mountPath);
      clear();
    },

// //////////////////////////////////////////////////////////////////////////////
// / @brief tear down
// //////////////////////////////////////////////////////////////////////////////

    tearDown: function () {
      clear();
    },

// //////////////////////////////////////////////////////////////////////////////
// / @brief tests
// //////////////////////////////////////////////////////////////////////////////

    testConsoleLogLogsMessage: function () {
      clear();
      console.log('hi');
      const logs = console.logs.list();
      expect(logs.length).to.be.equal(1);
      expect(logs[0]).to.have.property('level', 'INFO');
      expect(logs[0]).to.have.property('levelNum', console._logLevels.INFO);
      expect(logs[0]).to.have.property('message', 'hi');
    },

    testConsoleLogUsesFormat: function () {
      clear();
      console.log('%s plus', 1, 2);
      const logs = console.logs.list();
      expect(logs[0]).to.have.property('message', '1 plus 2'); // FIXME
    },

    testConsoleLogLogsTime: function () {
      clear();
      const min = Date.now();
      console.log('hi');
      const max = Date.now();
      const logs = console.logs.list();
      expect(logs[0]).to.have.property('time');
      expect(logs[0].time).to.be.a('number');
      expect(logs[0].time).not.to.be.lessThan(min);
      expect(logs[0].time).not.to.be.greaterThan(max);
    },
    testConsoleTimeLogsDuration: function () {
      clear();
      const start = Date.now();
      console.time('hi');
      const min = Date.now();
      internal.sleep(0.5);
      const max = Date.now();
      console.timeEnd('hi');
      const end = Date.now();
      expect(max).to.be.greaterThan(min); 
      const logs = console.logs.list();
      const match = logs[0].message.match(/^([^:]+):\s+(\d+)ms$/);
      expect(match).to.be.ok;
      expect(match[1]).to.equal('hi');
      const elapsed = Number(match[2]);
      expect(elapsed).not.to.be.lessThan(max - min);
      expect(elapsed).not.to.be.greaterThan(end - start);
    },
    testConsoleTimeThrowsForInvalidLabel: function () {
      expect(function () {
        console.timeEnd('this is a label that does not exist');
      }).to.throw(Error);
    },

    testConsoleDirUsesInspect: function () {
      const args = [
        'lol',
        {
          _PRINT: function (ctx) {
            ctx.output += 'hello';
          }
        }
      ];
      args.forEach(function (arg) {
        clear();
        console.dir(arg);
        const log = console.logs.list()[0];
        expect(log).to.have.property('message', util.inspect(arg));
      });
    },

    testConsoleAssertDoesNotThrow: function () {
      clear();
      console.setAssertThrows(false);
      expect(function () {
        console.assert(false, 'potato');
      }).not.to.throw(Error);
      const logs = console.logs.list();
      expect(logs.length).to.equal(1);
      expect(logs[0]).to.have.property('level', 'ERROR');
      expect(logs[0]).to.have.property('levelNum', console._logLevels.ERROR);
      expect(logs[0].message).to.match(/AssertionError: potato/);
    },

    testConsoleAssertThrows: function () {
      clear();
      console.setAssertThrows(true);
      expect(function () {
        console.assert(false, 'potato');
      }).to.throw(AssertionError).with.property('message', 'potato');
      const logs = console.logs.list();
      expect(logs.length).to.equal(1);
      expect(logs[0].message).to.match(/AssertionError: potato/);
    },

    testConsoleLogLevelPreventsLogging: function () {
      clear();
      console._logLevels.POTATO = -999;
      console.setLogLevel(-2);
      console.log.level = 'POTATO';
      console.log('potato');
      console.log.level = 'INFO';
      delete console._logLevels.POTATO;
      const logs = console.logs.list();
      expect(logs).to.be.empty;
    },
    testConsoleTracingAddsInfo: function () {
      clear();
      console.setTracing(false);
      console.log('without tracing');
      let logs = console.logs.list();
      expect(logs.length).to.equal(1);
      expect(logs[0]).not.to.have.property('stack');
      clear();
      console.setTracing(true);
      console.log('with tracing');
      console.setTracing(false);
      logs = console.logs.list();
      expect(logs.length).to.equal(1);
      expect(logs[0]).to.have.property('stack');
      expect(logs[0].stack).to.be.an('array');
      expect(logs[0].stack[0]).to.have.property('fileName');
      expect(logs[0].stack[0]).to.have.property('lineNumber');
      expect(logs[0].stack[0]).to.have.property('columnNumber');
    },
    testCustomLogLevels: function () {
      clear();
      const log = console.custom('BATMAN', 9000);
      log('potato');
      const logs = console.logs.list();
      expect(logs.length).to.equal(1);
      expect(logs[0]).to.have.property('level', 'BATMAN');
      expect(logs[0]).to.have.property('levelNum', 9000);
      expect(logs[0]).to.have.property('message', 'potato');
    },

    testTrace: function () {
      clear();
      console.trace('wat');
      const logs = console.logs.list();
      expect(logs.length).to.equal(1);
      expect(logs[0]).to.have.property('level', 'TRACE');
      expect(logs[0]).to.have.property('levelNum', console._logLevels.TRACE);
      expect(logs[0].message).to.match(/^Trace: wat\n\s+at/);
    },

    testLogsListWithMinLevel: function () {
      clear();
      let logs;
      console.debug('lol');
      console.error('hey');
      logs = console.logs.list({minLevel: 'DEBUG'});
      expect(logs.length).to.be.equal(2);
      logs = console.logs.list({minLevel: console._logLevels.DEBUG + 1});
      expect(logs.length).to.be.equal(1);
      expect(logs[0]).to.have.property('message', 'hey');
      logs = console.logs.list({minLevel: console._logLevels.ERROR + 1});
      expect(logs.length).to.be.equal(0);
    },

    testLogsListWithLevel: function () {
      clear();
      let logs;
      console.debug('lol');
      console.error('hey');
      logs = console.logs.list({level: 'DEBUG'});
      expect(logs.length).to.be.equal(1);
      expect(logs[0]).to.have.property('message', 'lol');
    },

    testLogsSearchByFileNameFailsWithoutTracing: function () {
      console.setTracing(false);
      expect(function () {
        console.logs.searchByFileName('lol');
      }).to.throw(Error).with.property('message', 'Tracing must be enabled in order to search by filename.');
    },

    testLogsSearchByFileName: function () {
      clear();
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
      clear();
      console.log('abcdef');
      expect(console.logs.searchByMessage('bcd')).to.have.property('length', 1);
      expect(console.logs.searchByMessage('ab')).to.have.property('length', 1);
      expect(console.logs.searchByMessage('ef')).to.have.property('length', 1);
      expect(console.logs.searchByMessage('fail')).to.have.property('length', 0);
    }

  };
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief executes the test suite
// //////////////////////////////////////////////////////////////////////////////

jsunity.run(ConsoleTestSuite);

let rc = jsunity.done();

db._useDatabase('_system');
try {
  db._drop(foxxlogCol, {
    isSystem: true
  });
} catch (x) {
}

return rc;
