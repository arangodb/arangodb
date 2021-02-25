/*jshint globalstrict:false, strict:true */
/*global assertEqual, assertTrue, ARGUMENTS */

////////////////////////////////////////////////////////////////////////////////
/// @brief tests for client-specific functionality
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2010-2016 triagens GmbH, Cologne, Germany
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
/// @author Max Neunhoeffer
/// @author Copyright 2016, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var jsunity = require("jsunity");
var wait = require("internal").wait;
var _ = require("lodash");

////////////////////////////////////////////////////////////////////////////////
/// @brief bogus UUIDs
////////////////////////////////////////////////////////////////////////////////

function guid() {
  function s4() {
    return Math.floor((1 + Math.random()) * 0x10000)
      .toString(16)
      .substring(1);
  }
  return s4() + s4() + '-' + s4() + '-' + s4() + '-' +
    s4() + '-' + s4() + s4() + s4();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief shuffle array elements
////////////////////////////////////////////////////////////////////////////////

function shuffle(a) {
    for (let i = a.length - 1; i > 0; i--) {
        const j = Math.floor(Math.random() * (i + 1));
        [a[i], a[j]] = [a[j], a[i]];
    }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function agencyTestSuite () {
  'use strict';

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief the agency servers
  ////////////////////////////////////////////////////////////////////////////////

  var instanceInfo = JSON.parse(require('internal').env.INSTANCEINFO);
  var agencyServers = instanceInfo.arangods.map(arangod => {
    return arangod.url;
  });

  var agencyLeader = agencyServers[0];
  var request = require("@arangodb/request");

  function agencyConfig() {
    for (let count = 0; count < 60; ++count) {
      let res = request({url: agencyLeader + "/_api/agency/config",
                         method: "GET", followRedirect: true});
      if (res.statusCode === 200) {
        res.bodyParsed = JSON.parse(res.body);
        if (res.bodyParsed.leaderId !== "") {
          return res.bodyParsed;
        }
        require('console').topic("agency=warn", "No leadership ...");
      } else {
        require('console').topic("agency=warn", "Got status " + res.statusCode +
                                 " from agency.");
      }
      require("internal").wait(1.0);   // give the server another second
    }
    require('console').topic("agency=error",
                             "Giving up, agency did not boot successfully.");
    assertEqual("apple", "orange");
    // all is lost because agency did not get up and running in time
  }

  function findAgencyCompactionIntervals() {
    var c = agencyConfig();
    return {
      compactionStepSize: c.configuration["compaction step size"],
      compactionKeepSize: c.configuration["compaction keep size"]
    };
  }

  var compactionConfig = findAgencyCompactionIntervals();
  require("console").topic("agency=info", "Agency compaction configuration: ", compactionConfig);

  function getCompactions(servers) {
    var ret = [];
    servers.forEach(function (url) {
      var compaction = {
        url: url + "/_api/cursor",
        timeout: 240,
        method: "POST",
        headers: {"Content-Type": "application/json"},
        body: JSON.stringify({ query : "FOR c IN compact SORT c._key RETURN c" })};
      var state = {
        url: url + "/_api/agency/state",
        timeout: 240
      };

      ret.push({compactions: JSON.parse(request(compaction).body),
                state: JSON.parse(request(state).body), url: url});
    });
    return ret;
  }

  function accessAgency(api, list, timeout = 60) {
    // We simply try all agency servers in turn until one gives us an HTTP
    // response:
    var res;
    var inquire = false;

    var clientIds = [];
    list.forEach(function (trx) {
      if (Array.isArray(trx) && trx.length === 3 &&
          typeof(trx[0]) === 'object' && typeof(trx[2]) === 'string') {
        clientIds.push(trx[2]);
      }
    });

    var startTime = new Date();
    while (true) {

      if (new Date() - startTime > 600000) {
        assertTrue(false, "Hit global timeout of 10 minutes in accessAgency.");
      }

      if (!inquire) {
        res = request({url: agencyLeader + "/_api/agency/" + api,
                       method: "POST", followRedirect: false,
                       body: JSON.stringify(list),
                       headers: {"Content-Type": "application/json"},
                       timeout: timeout  /* essentially for the huge trx package
                                            running under ASAN in the CI */ });
        require('console').topic("agency=debug", 'Sent out agency request, statusCode:', res.statusCode);
      } else { // inquire. Remove successful commits. For later retries
        res = request({url: agencyLeader + "/_api/agency/inquire",
                       method: "POST", followRedirect: false,
                       body: JSON.stringify(clientIds),
                       headers: {"Content-Type": "application/json"},
                       timeout: timeout
                      });
        require('console').topic("agency=info", 'Sent out agency inquiry, statusCode:', res.statusCode);
      }

      if (res.statusCode === 307) {
        agencyLeader = res.headers.location;
        var l = 0;
        for (var i = 0; i < 3; ++i) {
          l = agencyLeader.indexOf('/', l+1);
        }
        agencyLeader = agencyLeader.substring(0,l);
        if (clientIds.length > 0 && api === 'write') {
          inquire = true;
        }
        require('console').topic("agency=info", 'Redirected to ' + agencyLeader);
        continue;
      } else if (res.statusCode === 503 || res.statusCode === 500) {
        // 503 covers service not available and 500 covers timeout
        require('console').topic("agency=info", 'Got status code', res.statusCode, ', waiting for leader ... ');
        if (clientIds.length > 0 && api === 'write') {
          inquire = true;
        }
        wait(1.0);
        continue;
      }
      if (!inquire) {
        break;  // done, let's report the result, whatever it is
      }
      // In case of inquiry, we probably have done some of the transactions:
      var done = 0;
      res.bodyParsed = JSON.parse(res.body);
      res.bodyParsed.results.forEach(function (index) {
        var noZeroYet = true;
        if (index > 0) {
          done++;
          assertTrue(noZeroYet);
        } else {
          noZeroYet = false;
        }
      });
      require('console').topic("agency=info", 'Inquiry analysis: done=', done, ' body:', res.body);
      if (done === clientIds.length) {
        require('console').topic("agency=info", 'Inquiry analysis, accepting result as good!');
        break;
      } else {
        list = list.slice(done);
        clientIds = clientIds.slice(done);
        inquire = false;
        require('console').topic("agency=info", 'Inquiry analysis: have accepted', done, 'transactions as done, continuing with this list:', JSON.stringify(list));
      }
    }
    try {
      res.bodyParsed = JSON.parse(res.body);
    } catch(e) {
      require("console").error("Exception in body parse:", res.body, JSON.stringify(e), api, list, JSON.stringify(res));
    }
    return res;
  }

  function readAndCheck(list) {
    var res = accessAgency("read", list);
    assertEqual(res.statusCode, 200);
    return res.bodyParsed;
  }

  function writeAndCheck(list, timeout = 60) {
    var res = accessAgency("write", list, timeout);
    assertEqual(res.statusCode, 200);
    return res.bodyParsed;
  }

  function transactAndCheck(list, code) {
    var res = accessAgency("transact", list);
    assertEqual(res.statusCode, code);
    return res.bodyParsed;
  }

  function doCountTransactions(count, start) {
    let i, res;
    let counter = 0;
    let trxs = [];
    for (i = start; i < start + count; ++i) {
      let key = "/key"+i;
      let trx = [{},{},"clientid" + start + counter++];
      trx[0][key] = "value" + i;
      trxs.push(trx);
      if (trxs.length >= 200 || i === start + count - 1) {
        res = accessAgency("write", trxs);
        assertEqual(200, res.statusCode);
        trxs = [];
      }
    }
    trxs = [];
    for (i = 0; i < start + count; ++i) {
      trxs.push(["/key"+i]);
    }
    res = accessAgency("read", trxs);
    assertEqual(200, res.statusCode);
    for (i = 0; i < start + count; ++i) {
      let key = "key"+i;
      let correct = {};
      correct[key] = "value" + i;
      assertEqual(correct, res.bodyParsed[i]);
    }
  }

  function evalComp() {

    var servers = _.clone(agencyServers), llogi;
    var count = 0;

    while (servers.length > 0) {
      var agents = getCompactions(servers), i, old;
      var ready = true;
      for (i = 1; i < agents.length; ++i) {
        if (agents[0].state.log[agents[0].state.log.length-1].index !==
            agents[i].state.log[agents[i].state.log.length-1].index) {
          ready = false;
          break;
        }
      }
      if (!ready) {
        continue;
      }
      agents.forEach( function (agent) {

        var results = agent.compactions.result;         // All compactions
        var llog = agent.state.log[agent.state.log.length-1];   // Last log entry
        llogi = llog.index;                         // Last log index
        var lcomp = results[results.length-1];          // Last compaction entry
        var lcompi = parseInt(lcomp._key);              // Last compaction index
        var stepsize = compactionConfig.compactionStepSize;

        if (lcompi > llogi - stepsize) { // agent has compacted

          var foobar = accessAgency("read", [["foobar"]]).bodyParsed[0].foobar;
          var n = 0;
          var keepsize = compactionConfig.compactionKeepSize;
          var flog = agent.state.log[0];                      // First log entry
          var flogi = flog.index;                         // First log index

          // Expect to find last compaction maximally
          // keep-size away from last RAFT index
          assertTrue(lcompi > llogi - stepsize);

          // log entries before compaction index - compaction keep size
          // are dumped
          if (lcompi > keepsize) {
            assertTrue(flogi === lcompi - keepsize);
          } else {
            assertEqual(flogi, 0);
          }

          if(lcomp.readDB[0].hasOwnProperty("foobar")) {
            // All log entries > last compaction index,
            // which are {"foobar":{"op":"increment"}}
            agent.state.log.forEach( function(log) {
              if (log.index > lcompi) {
                if (log.query.foobar !== undefined) {
                  ++n;
                }
              }
            });

            // Sum of relevant log entries > last compaction index and last
            // compaction's foobar value must match foobar's value in agency
            assertEqual(lcomp.readDB[0].foobar + n, foobar);

          }
          // this agent is fine remove it from agents to be check this time
          // around list
          servers.splice(servers.indexOf(agent.url));

        }
      });
      wait(0.1);
      ++count;
      if (count > 600) {
        return 0;
      }

    }

    return llogi;

  }

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

    setUp : function () {
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

    tearDown : function () {
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief wait until leadership is established
////////////////////////////////////////////////////////////////////////////////

    testStartup : function () {
      assertEqual(readAndCheck([["/"]]), [{}]);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief Test transact interface
////////////////////////////////////////////////////////////////////////////////

    testPoll : function () {
      var cur = accessAgency("write",[[{"/": {"op":"delete"}}]]).
          bodyParsed.results[0];
      var ret, result, log, ci, job, wr;

      ret = request({url: agencyLeader + "/_api/agency/poll",
                         method: "GET", followRedirect: true});
      assertEqual(ret.statusCode, 200);
      ret.bodyParsed = JSON.parse(ret.body);
      assertTrue(ret.bodyParsed.hasOwnProperty("result"));
      result = ret.bodyParsed.result;
      assertTrue(result.hasOwnProperty("readDB"));
      assertEqual(result.readDB, {});
      assertTrue(result.hasOwnProperty("commitIndex"));
      ci = result.commitIndex;
      assertTrue(result.hasOwnProperty("firstIndex"));
      assertEqual(result.firstIndex, 0);

      ret = request({url: agencyLeader + "/_api/agency/poll?index=0",
                     method: "GET", followRedirect: true});
      if (ret.statusCode === 200) {
        ret.bodyParsed = JSON.parse(ret.body);
      }
      assertTrue(ret.bodyParsed.hasOwnProperty("result"));
      result = ret.bodyParsed.result;
      assertTrue(result.hasOwnProperty("readDB"));
      assertEqual(result.readDB, {});
      assertTrue(result.hasOwnProperty("commitIndex"));
      assertEqual(result.commitIndex, ci);
      assertTrue(result.hasOwnProperty("firstIndex"));
      assertEqual(result.firstIndex, 0);

      ret = request({url: agencyLeader + "/_api/agency/poll?index=1",
                     method: "GET", followRedirect: true});
      if (ret.statusCode === 200) {
        ret.bodyParsed = JSON.parse(ret.body);
      }
      assertTrue(ret.bodyParsed.hasOwnProperty("result"));
      result = ret.bodyParsed.result;
      assertTrue(result.hasOwnProperty("log"));
      log = result.log;
      assertTrue(Array.isArray(log));
      assertEqual(result.commitIndex, ci);
      assertEqual(result.firstIndex, 1);

      ret = request({url: agencyLeader + "/_api/agency/poll?index=1",
                     method: "GET", followRedirect: true});
      if (ret.statusCode === 200) {
        ret.bodyParsed = JSON.parse(ret.body);
      }
      assertTrue(ret.bodyParsed.hasOwnProperty("result"));
      result = ret.bodyParsed.result;
      assertTrue(result.hasOwnProperty("log"));
      log = result.log;
      assertTrue(Array.isArray(log));
      assertEqual(result.commitIndex, ci);
      assertEqual(result.firstIndex, 1);

      ret = request({url: agencyLeader + "/_api/agency/poll?index=" + ci,
                     method: "GET", followRedirect: true});
      if (ret.statusCode === 200) {
        ret.bodyParsed = JSON.parse(ret.body);
      }
      assertTrue(ret.bodyParsed.hasOwnProperty("result"));
      result = ret.bodyParsed.result;
      assertTrue(result.hasOwnProperty("log"));
      log = result.log;
      assertEqual(log.length, 1);
      assertTrue(Array.isArray(log));
      assertEqual(result.commitIndex, ci);
      assertEqual(result.firstIndex, ci);


      ret = request({url: agencyLeader + "/_api/agency/poll?index=" + ci,
                     method: "GET", followRedirect: true});
      if (ret.statusCode === 200) {
        ret.bodyParsed = JSON.parse(ret.body);
      }
      assertTrue(ret.bodyParsed.hasOwnProperty("result"));
      result = ret.bodyParsed.result;
      assertTrue(result.hasOwnProperty("log"));
      log = result.log;
      assertEqual(log.length, 1);
      assertTrue(Array.isArray(log));
      assertEqual(result.commitIndex, ci);
      assertEqual(result.firstIndex, ci);

      ret = request({url: agencyLeader + "/_api/agency/poll?index=" + (ci + 1),
                     method: "GET", headers: {"X-arango-async": "store"}});

      assertEqual(ret.statusCode, 202);
      assertTrue(ret.headers.hasOwnProperty("x-arango-async-id"));
      job = ret.headers["x-arango-async-id"];

      wr = accessAgency("write",[[{"/": {"op":"delete"}}]]).
          bodyParsed.results[0];
      let tries = 0;
      while (++tries <60) {
        // jobs are processed asynchronously, so we need to poll here
        // until the job has been processed
        ret = request({url: agencyLeader + "/_api/job/" + job,
                       method: "GET", followRedirect: true});
        if (ret.statusCode !== 204) {
          break;
        }
        wait(0.5);
      }
      assertEqual(ret.statusCode, 200);
      ret = request({url: agencyLeader + "/_api/job/" + job, method: "PUT"});
      assertEqual(ret.statusCode, 200);
      ret.bodyParsed = JSON.parse(ret.body);
      result = ret.bodyParsed.result;

      assertTrue(result.hasOwnProperty("log"));
      assertTrue(result.hasOwnProperty("commitIndex"));
      assertTrue(result.hasOwnProperty("firstIndex"));
      assertEqual(result.firstIndex, ci+1);
      assertEqual(result.commitIndex, ci+1);

      // Multiple writes
      ci = result.commitIndex;
      ret = request({url: agencyLeader + "/_api/agency/poll?index=" + (ci + 1),
                     method: "GET", followRedirect: true, headers: {"X-arango-async": "store"}});
      assertTrue(ret.headers.hasOwnProperty("x-arango-async-id"));
      job = ret.headers["x-arango-async-id"];
      wr = accessAgency("write",[[{"/": {"op":"delete"}}],[{"/": {"op":"delete"}}],[{"/": {"op":"delete"}}]]).
          bodyParsed.results[0];
      ret = request({url: agencyLeader + "/_api/job/" + job,
                     method: "PUT", followRedirect: true, body: {}});
      assertEqual(ret.statusCode, 200);
      ret.bodyParsed = JSON.parse(ret.body);
      result = ret.bodyParsed.result;
      assertEqual(result.firstIndex, ci+1);
      assertEqual(result.commitIndex, ci+3);
      ci = result.commitIndex;
    },
////////////////////////////////////////////////////////////////////////////////
/// @brief Test transact interface
////////////////////////////////////////////////////////////////////////////////

    testTransact : function () {

      var cur = accessAgency("write",[[{"/": {"op":"delete"}}]]).
          bodyParsed.results[0];
      assertEqual(readAndCheck([["/x"]]), [{}]);
      var res = transactAndCheck([["/x"],[{"/x":12}]],200);
      assertEqual(res, [{},++cur]);
      res = transactAndCheck([["/x"],[{"/x":12}]],200);
      assertEqual(res, [{x:12},++cur]);
      res = transactAndCheck([["/x"],[{"/x":12}],["/x"]],200);
      assertEqual(res, [{x:12},++cur,{x:12}]);
      res = transactAndCheck([["/x"],[{"/x":12}],["/x"]],200);
      assertEqual(res, [{x:12},++cur,{x:12}]);
      res = transactAndCheck([["/x"],[{"/x":{"op":"increment"}}],["/x"]],200);
      assertEqual(res, [{x:12},++cur,{x:13}]);
      res = transactAndCheck(
        [["/x"],[{"/x":{"op":"increment"}}],["/x"],[{"/x":{"op":"increment"}}]],
        200);
      assertEqual(res, [{x:13},++cur,{x:14},++cur]);
      res = transactAndCheck(
        [[{"/x":{"op":"increment"}}],[{"/x":{"op":"increment"}}],["/x"]],200);
      assertEqual(res, [++cur,++cur,{x:17}]);
      writeAndCheck([[{"/":{"op":"delete"}}]]);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test to write a single top level key
////////////////////////////////////////////////////////////////////////////////

    testSingleTopLevel : function () {
      assertEqual(readAndCheck([["/x"]]), [{}]);
      writeAndCheck([[{x:12}]]);
      assertEqual(readAndCheck([["/x"]]), [{x:12}]);
      writeAndCheck([[{x:{"op":"delete"}}]]);
      assertEqual(readAndCheck([["/x"]]), [{}]);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test to write a single non-top level key
////////////////////////////////////////////////////////////////////////////////

    testSingleNonTopLevel : function () {
      assertEqual(readAndCheck([["/x/y"]]), [{}]);
      writeAndCheck([[{"x/y":12}]]);
      assertEqual(readAndCheck([["/x/y"]]), [{x:{y:12}}]);
      writeAndCheck([[{"x/y":{"op":"delete"}}]]);
      assertEqual(readAndCheck([["/x"]]), [{x:{}}]);
      writeAndCheck([[{"x":{"op":"delete"}}]]);
      assertEqual(readAndCheck([["/x"]]), [{}]);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test preconditions
////////////////////////////////////////////////////////////////////////////////

    testPrecondition : function () {
      writeAndCheck([[{"/a":12}]]);
      assertEqual(readAndCheck([["/a"]]), [{a:12}]);
      writeAndCheck([[{"/a":13},{"/a":12}]]);
      assertEqual(readAndCheck([["/a"]]), [{a:13}]);
      var res = accessAgency("write", [[{"/a":14},{"/a":12}]]); // fail precond {a:12}
      assertEqual(res.statusCode, 412);
      assertEqual(res.bodyParsed, {"results":[0]});
      writeAndCheck([[{a:{op:"delete"}}]]);
      // fail precond oldEmpty
      res = accessAgency("write",[[{"a":14},{"a":{"oldEmpty":false}}]]);
      assertEqual(res.statusCode, 412);
      assertEqual(res.bodyParsed, {"results":[0]});
      writeAndCheck([[{"a":14},{"a":{"oldEmpty":true}}]]); // precond oldEmpty
      writeAndCheck([[{"a":14},{"a":{"old":14}}]]);        // precond old
      // fail precond old
      res = accessAgency("write",[[{"a":14},{"a":{"old":13}}]]);
      assertEqual(res.statusCode, 412);
      assertEqual(res.bodyParsed, {"results":[0]});
      writeAndCheck([[{"a":14},{"a":{"isArray":false}}]]); // precond isArray
      // fail precond isArray
      res = accessAgency("write",[[{"a":14},{"a":{"isArray":true}}]]);
      assertEqual(res.statusCode, 412);
      assertEqual(res.bodyParsed, {"results":[0]});
      // check object precondition
      res = accessAgency("write",[[{"/a/b/c":{"op":"set","new":12}}]]);
      res = accessAgency("write",[[{"/a/b/c":{"op":"set","new":13}},{"a":{"old":{"b":{"c":12}}}}]]);
      assertEqual(res.statusCode, 200);
      res = accessAgency("write",[[{"/a/b/c":{"op":"set","new":14}},{"/a":{"old":{"b":{"c":12}}}}]]);
      assertEqual(res.statusCode, 412);
      res = accessAgency("write",[[{"/a/b/c":{"op":"set","new":14}},{"/a":{"old":{"b":{"c":13}}}}]]);
      assertEqual(res.statusCode, 200);
      // multiple preconditions
      res = accessAgency("write",[[{"/a":1,"/b":true,"/c":"c"},{"/a":{"oldEmpty":false}}]]);
      assertEqual(readAndCheck([["/a","/b","c"]]), [{a:1,b:true,c:"c"}]);
      res = accessAgency("write",[[{"/a":2},{"/a":{"oldEmpty":false},"/b":{"oldEmpty":true}}]]);
      assertEqual(res.statusCode, 412);
      assertEqual(readAndCheck([["/a"]]), [{a:1}]);
      res = accessAgency("write",[[{"/a":2},{"/a":{"oldEmpty":true},"/b":{"oldEmpty":false}}]]);
      assertEqual(res.statusCode, 412);
      assertEqual(readAndCheck([["/a"]]), [{a:1}]);
      res = accessAgency("write",[[{"/a":2},{"/a":{"oldEmpty":false},"/b":{"oldEmpty":false},"/c":{"oldEmpty":true}}]]);
      assertEqual(res.statusCode, 412);
      assertEqual(readAndCheck([["/a"]]), [{a:1}]);
      res = accessAgency("write",[[{"/a":2},{"/a":{"oldEmpty":false},"/b":{"oldEmpty":false},"/c":{"oldEmpty":false}}]]);
      assertEqual(res.statusCode, 200);
      assertEqual(readAndCheck([["/a"]]), [{a:2}]);
      res = accessAgency("write",[[{"/a":3},{"/a":{"old":2},"/b":{"oldEmpty":false},"/c":{"oldEmpty":false}}]]);
      assertEqual(res.statusCode, 200);
      assertEqual(readAndCheck([["/a"]]), [{a:3}]);
      res = accessAgency("write",[[{"/a":2},{"/a":{"old":2},"/b":{"oldEmpty":false},"/c":{"oldEmpty":false}}]]);
      assertEqual(res.statusCode, 412);
      assertEqual(readAndCheck([["/a"]]), [{a:3}]);
      res = accessAgency("write",[[{"/a":2},{"/a":{"old":3},"/b":{"oldEmpty":false},"/c":{"isArray":true}}]]);
      assertEqual(res.statusCode, 412);
      assertEqual(readAndCheck([["/a"]]), [{a:3}]);
      res = accessAgency("write",[[{"/a":2},{"/a":{"old":3},"/b":{"oldEmpty":false},"/c":{"isArray":false}}]]);
      assertEqual(res.statusCode, 200);
      assertEqual(readAndCheck([["/a"]]), [{a:2}]);
      // in precondition & multiple
      writeAndCheck([[{"a":{"b":{"c":[1,2,3]},"e":[1,2]},"d":false}]]);
      res = accessAgency("write",[[{"/b":2},{"/a/b/c":{"in":3}}]]);
      assertEqual(res.statusCode, 200);
      assertEqual(readAndCheck([["/b"]]), [{b:2}]);
      res = accessAgency("write",[[{"/b":3},{"/a/e":{"in":3}}]]);
      assertEqual(res.statusCode, 412);
      assertEqual(readAndCheck([["/b"]]), [{b:2}]);
      res = accessAgency("write",[[{"/b":3},{"/a/e":{"in":3},"/a/b/c":{"in":3}}]]);
      assertEqual(res.statusCode, 412);
      res = accessAgency("write",[[{"/b":3},{"/a/e":{"in":3},"/a/b/c":{"in":3}}]]);
      assertEqual(res.statusCode, 412);
      res = accessAgency("write",[[{"/b":3},{"/a/b/c":{"in":3},"/a/e":{"in":3}}]]);
      assertEqual(res.statusCode, 412);
      res = accessAgency("write",[[{"/b":3},{"/a/b/c":{"in":3},"/a/e":{"in":2}}]]);
      assertEqual(res.statusCode, 200);
      assertEqual(readAndCheck([["/b"]]), [{b:3}]);
      // Permute order of keys and objects within precondition
      var localObj =
          {"foo" : "bar",
           "baz" : {
             "_id": "5a00203e4b660989b2ae5493", "index": 0,
             "guid": "7a709cc2-1479-4079-a0a3-009cbe5674f4",
             "isActive": true, "balance": "$3,072.23",
             "picture": "http://placehold.it/32x32",
             "age": 21, "eyeColor": "green", "name":
             { "first": "Durham", "last": "Duke" },
             "tags": ["anim","et","id","do","est",1.0,-1024,1024]
           },
           "qux" : ["3.14159265359",3.14159265359]
          };
      var test;
      var localKeys = [];
      for (var i in localObj.baz) {
        localKeys.push(i);
      }
      var permuted;
      res = accessAgency(
        "write",
        [[localObj,
          {"foo":localObj.bar,
           "baz":{"old":localObj.baz},
           "qux":localObj.qux}]]);
      assertEqual(res.statusCode, 412);

      res = writeAndCheck([[localObj]]);
      res = writeAndCheck([[localObj, {"foo":localObj.foo,"baz":{"old":localObj.baz},"qux":localObj.qux}]]);
      res = writeAndCheck(
        [[localObj, {"baz":{"old":localObj.baz},"foo":localObj.foo,"qux":localObj.qux}]]);
      res = writeAndCheck(
        [[localObj, {"baz":{"old":localObj.baz},"qux":localObj.qux,"foo":localObj.foo}]]);
      res = writeAndCheck(
        [[localObj, {"qux":localObj.qux,"baz":{"old":localObj.baz},"foo":localObj.foo}]]);

      for (var j in localKeys) {
        permuted = {};
        shuffle(localKeys);
        for (var k in localKeys) {
          permuted[localKeys[k]] = localObj.baz[localKeys[k]];
        }
        res = writeAndCheck(
          [[localObj, {"baz":{"old":permuted},"foo":localObj.foo,"qux":localObj.qux}]]);
        res = writeAndCheck(
          [[localObj, {"foo":localObj.foo,"qux":localObj.qux,"baz":{"old":permuted}}]]);
        res = writeAndCheck(
          [[localObj, {"qux":localObj.qux,"baz":{"old":permuted},"foo":localObj.foo}]]);
      }

      // Permute order of keys and objects within arrays in preconditions
      writeAndCheck([[{"a":[{"b":12,"c":13}]}]]);
      writeAndCheck([[{"a":[{"b":12,"c":13}]},{"a":[{"b":12,"c":13}]}]]);
      writeAndCheck([[{"a":[{"b":12,"c":13}]},{"a":[{"c":13,"b":12}]}]]);

      localObj = {"b":"Hello world!", "c":3.14159265359, "d":314159265359, "e": -3};
      var localObk = {"b":1, "c":1.0, "d": 100000000001, "e": -1};
      localKeys  = [];
      for (var l in localObj) {
        localKeys.push(l);
      }
      permuted = {};
      var per2 = {};
      writeAndCheck([[ { "a" : [localObj,localObk] } ]]);
      writeAndCheck([[ { "a" : [localObj,localObk] }, {"a" : [localObj,localObk] }]]);
      for (var m = 0; m < 7; m++) {
        permuted = {};
        shuffle(localKeys);
        for (k in localKeys) {
          permuted[localKeys[k]] = localObj[localKeys[k]];
          per2 [localKeys[k]] = localObk[localKeys[k]];
        }
        writeAndCheck([[ { "a" : [localObj,localObk] }, {"a" : [permuted,per2] }]]);
        res = accessAgency("write",
                           [[ { "a" : [localObj,localObk] }, {"a" : [per2,permuted] }]]);
        assertEqual(res.statusCode, 412);
      }

      res = accessAgency("write", [[{"a":12},{"a":{"intersectionEmpty":""}}]]);
      assertEqual(res.statusCode, 412);
      res = accessAgency("write", [[{"a":12},{"a":{"intersectionEmpty":[]}}]]);
      assertEqual(res.statusCode, 200);
      res = accessAgency("write", [[{"a":[12,"Pi",3.14159265359,true,false]},
                                    {"a":{"intersectionEmpty":[]}}]]);
      assertEqual(res.statusCode, 200);
      res = accessAgency("write", [[{"a":[12,"Pi",3.14159265359,true,false]},
                                    {"a":{"intersectionEmpty":[false,"Pi"]}}]]);
      assertEqual(res.statusCode, 412);
      res = accessAgency("write", [[{"a":[12,"Pi",3.14159265359,true,false]},
                                    {"a":{"intersectionEmpty":["Pi",false]}}]]);
      assertEqual(res.statusCode, 412);
      res = accessAgency("write", [[{"a":[12,"Pi",3.14159265359,true,false]},
                                    {"a":{"intersectionEmpty":[false,false,false]}}]]);
      assertEqual(res.statusCode, 412);
      res = accessAgency("write", [[{"a":[12,"Pi",3.14159265359,true,false]},
                                    {"a":{"intersectionEmpty":["pi",3.1415926535]}}]]);
      assertEqual(res.statusCode, 200);
      res = accessAgency("write", [[{"a":[12,"Pi",3.14159265359,true,false]},
                                    {"a":{"instersectionEmpty":[]}}]]);
      assertEqual(res.statusCode, 412);

    },

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief test clientIds
  ////////////////////////////////////////////////////////////////////////////////

    testClientIds : function () {
      var res;
      var cur;

      res = accessAgency("write", [[{"a":12}]]).bodyParsed;
      cur = res.results[0];

      writeAndCheck([[{"/a":12}]]);
      var id = [guid(),guid(),guid(),guid(),guid(),guid(),
                guid(),guid(),guid(),guid(),guid(),guid(),
                guid(),guid(),guid()];
      var query = [{"a":12},{"a":13},{"a":13}];
      var pre = [{},{"a":12},{"a":12}];
      cur += 2;

      var wres = accessAgency("write", [[query[0], pre[0], id[0]]]);
      res = accessAgency("inquire",[id[0]]);
      wres.bodyParsed.inquired = true;
      assertEqual(res.bodyParsed.results, wres.bodyParsed.results);

      wres = accessAgency("write", [[query[1], pre[1], id[0]]]);
      res = accessAgency("inquire",[id[0]]);
      assertEqual(res.bodyParsed.results, wres.bodyParsed.results);
      cur++;

      wres = accessAgency("write",[[query[1], pre[1], id[2]]]);
      assertEqual(wres.statusCode,412);
      res = accessAgency("inquire",[id[2]]);
      assertEqual(res.statusCode,404);
      assertEqual(res.bodyParsed, {"results":[0],"inquired":true});
      assertEqual(res.bodyParsed.results, wres.bodyParsed.results);

      wres = accessAgency("write",[[query[0], pre[0], id[3]],
                                   [query[1], pre[1], id[3]]]);
      assertEqual(wres.statusCode,200);
      cur += 2;
      res = accessAgency("inquire",[id[3]]);
      assertEqual(res.bodyParsed, {"results":[cur],"inquired":true});
      assertEqual(res.bodyParsed.results[0], wres.bodyParsed.results[1]);
      assertEqual(res.statusCode,200);


      wres = accessAgency("write",[[query[0], pre[0], id[4]],
                                   [query[1], pre[1], id[4]],
                                   [query[2], pre[2], id[4]]]);
      assertEqual(wres.statusCode,412);
      cur += 2;
      res = accessAgency("inquire",[id[4]]);
      assertEqual(res.bodyParsed, {"results":[cur],"inquired":true});
      assertEqual(res.bodyParsed.results[0], wres.bodyParsed.results[1]);
      assertEqual(res.statusCode,200);

      wres = accessAgency("write",[[query[0], pre[0], id[5]],
                                   [query[2], pre[2], id[5]],
                                   [query[1], pre[1], id[5]]]);
      assertEqual(wres.statusCode,412);
      cur += 2;
      res = accessAgency("inquire",[id[5]]);
      assertEqual(res.bodyParsed, {"results":[cur],"inquired":true});
      assertEqual(res.bodyParsed.results[0], wres.bodyParsed.results[1]);
      assertEqual(res.statusCode,200);

      wres = accessAgency("write",[[query[2], pre[2], id[6]],
                                   [query[0], pre[0], id[6]],
                                   [query[1], pre[1], id[6]]]);
      assertEqual(wres.statusCode,412);
      cur += 2;
      res = accessAgency("inquire",[id[6]]);
      assertEqual(res.bodyParsed, {"results":[cur],"inquired":true});
      assertEqual(res.bodyParsed.results[0], wres.bodyParsed.results[2]);
      assertEqual(res.statusCode,200);

      wres = accessAgency("write",[[query[2], pre[2], id[7]],
                                  [query[0], pre[0], id[8]],
                                  [query[1], pre[1], id[9]]]);
      assertEqual(wres.statusCode,412);
      cur += 2;
      res = accessAgency("inquire",[id[7],id[8],id[9]]);
      assertEqual(res.statusCode,404);
      assertEqual(res.bodyParsed.results, wres.bodyParsed.results);

    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test document/transaction assignment
////////////////////////////////////////////////////////////////////////////////

    testDocument : function () {
      writeAndCheck([[{"a":{"b":{"c":[1,2,3]},"e":12},"d":false}]]);
      assertEqual(readAndCheck([["a/e"],[ "d","a/b"]]),
                  [{a:{e:12}},{a:{b:{c:[1,2,3]},d:false}}]);
      writeAndCheck([[{"a":{"_id":"576d1b7becb6374e24ed5a04","index":0,"guid":"60ffa50e-0211-4c60-a305-dcc8063ae2a5","isActive":true,"balance":"$1,050.96","picture":"http://placehold.it/32x32","age":30,"eyeColor":"green","name":{"first":"Maura","last":"Rogers"},"company":"GENESYNK","email":"maura.rogers@genesynk.net","phone":"+1(804)424-2766","address":"501RiverStreet,Wollochet,Vermont,6410","about":"Temporsintofficiaipsumidnullalaboreminimlaborisinlaborumincididuntexcepteurdolore.Sunteumagnadolaborumsunteaquisipsumaliquaaliquamagnaminim.Cupidatatadproidentullamconisietofficianisivelitculpaexcepteurqui.Suntautemollitconsecteturnulla.Commodoquisidmagnaestsitelitconsequatdoloreupariaturaliquaetid.","registered":"Friday,November28,20148:01AM","latitude":"-30.093679","longitude":"10.469577","tags":["laborum","proident","est","veniam","sunt"],"range":[0,1,2,3,4,5,6,7,8,9],"friends":[{"id":0,"name":"CarverDurham"},{"id":1,"name":"DanielleMalone"},{"id":2,"name":"ViolaBell"}],"greeting":"Hello,Maura!Youhave9unreadmessages.","favoriteFruit":"banana"}}],[{"!!@#$%^&*)":{"_id":"576d1b7bb2c1af32dd964c22","index":1,"guid":"e6bda5a9-54e3-48ea-afd7-54915fec48c2","isActive":false,"balance":"$2,631.75","picture":"http://placehold.it/32x32","age":40,"eyeColor":"blue","name":{"first":"Jolene","last":"Todd"},"company":"QUANTASIS","email":"jolene.todd@quantasis.us","phone":"+1(954)418-2311","address":"818ButlerStreet,Berwind,Colorado,2490","about":"Commodoesseveniamadestirureutaliquipduistempor.Auteeuametsuntessenisidolorfugiatcupidatatsintnulla.Sitanimincididuntelitculpasunt.","registered":"Thursday,June12,201412:08AM","latitude":"-7.101063","longitude":"4.105685","tags":["ea","est","sunt","proident","pariatur"],"range":[0,1,2,3,4,5,6,7,8,9],"friends":[{"id":0,"name":"SwansonMcpherson"},{"id":1,"name":"YoungTyson"},{"id":2,"name":"HinesSandoval"}],"greeting":"Hello,Jolene!Youhave5unreadmessages.","favoriteFruit":"strawberry"}}],[{"1234567890":{"_id":"576d1b7b79527b6201ed160c","index":2,"guid":"2d2d7a45-f931-4202-853d-563af252ca13","isActive":true,"balance":"$1,446.93","picture":"http://placehold.it/32x32","age":28,"eyeColor":"blue","name":{"first":"Pickett","last":"York"},"company":"ECSTASIA","email":"pickett.york@ecstasia.me","phone":"+1(901)571-3225","address":"556GrovePlace,Stouchsburg,Florida,9119","about":"Idnulladolorincididuntirurepariaturlaborumutmolliteavelitnonveniaminaliquip.Adametirureesseanimindoloreduisproidentdeserunteaconsecteturincididuntconsecteturminim.Ullamcoessedolorelitextemporexcepteurexcepteurlaboreipsumestquispariaturmagna.ExcepteurpariaturexcepteuradlaborissitquieiusmodmagnalaborisincididuntLoremLoremoccaecat.","registered":"Thursday,January28,20165:20PM","latitude":"-56.18036","longitude":"-39.088125","tags":["ad","velit","fugiat","deserunt","sint"],"range":[0,1,2,3,4,5,6,7,8,9],"friends":[{"id":0,"name":"BarryCleveland"},{"id":1,"name":"KiddWare"},{"id":2,"name":"LangBrooks"}],"greeting":"Hello,Pickett!Youhave10unreadmessages.","favoriteFruit":"strawberry"}}],[{"@":{"_id":"576d1b7bc674d071a2bccc05","index":3,"guid":"14b44274-45c2-4fd4-8c86-476a286cb7a2","isActive":true,"balance":"$1,861.79","picture":"http://placehold.it/32x32","age":27,"eyeColor":"brown","name":{"first":"Felecia","last":"Baird"},"company":"SYBIXTEX","email":"felecia.baird@sybixtex.name","phone":"+1(821)498-2971","address":"571HarrisonAvenue,Roulette,Missouri,9284","about":"Adesseofficianisiexercitationexcepteurametconsecteturessequialiquaquicupidatatincididunt.Nostrudullamcoutlaboreipsumduis.ConsequatsuntlaborumadLoremeaametveniamesseoccaecat.","registered":"Monday,December21,20156:50AM","latitude":"0.046813","longitude":"-13.86172","tags":["velit","qui","ut","aliquip","eiusmod"],"range":[0,1,2,3,4,5,6,7,8,9],"friends":[{"id":0,"name":"CeliaLucas"},{"id":1,"name":"HensonKline"},{"id":2,"name":"ElliottWalker"}],"greeting":"Hello,Felecia!Youhave9unreadmessages.","favoriteFruit":"apple"}}],[{"|}{[]αв¢∂єƒgαв¢∂єƒg":{"_id":"576d1b7be4096344db437417","index":4,"guid":"f789235d-b786-459f-9288-0d2f53058d02","isActive":false,"balance":"$2,011.07","picture":"http://placehold.it/32x32","age":28,"eyeColor":"brown","name":{"first":"Haney","last":"Burks"},"company":"SPACEWAX","email":"haney.burks@spacewax.info","phone":"+1(986)587-2735","address":"197OtsegoStreet,Chesterfield,Delaware,5551","about":"Quisirurenostrudcupidatatconsequatfugiatvoluptateproidentvoluptate.Duisnullaadipisicingofficiacillumsuntlaborisdeseruntirure.Laborumconsecteturelitreprehenderitestcillumlaboresintestnisiet.Suntdeseruntexercitationutauteduisaliquaametetquisvelitconsecteturirure.Auteipsumminimoccaecatincididuntaute.Irureenimcupidatatexercitationutad.Minimconsecteturadipisicingcommodoanim.","registered":"Friday,January16,20155:29AM","latitude":"86.036358","longitude":"-1.645066","tags":["occaecat","laboris","ipsum","culpa","est"],"range":[0,1,2,3,4,5,6,7,8,9],"friends":[{"id":0,"name":"SusannePacheco"},{"id":1,"name":"SpearsBerry"},{"id":2,"name":"VelazquezBoyle"}],"greeting":"Hello,Haney!Youhave10unreadmessages.","favoriteFruit":"apple"}}]]);
      assertEqual(readAndCheck([["/!!@#$%^&*)/address"]]),[{"!!@#$%^&*)":{"address": "818ButlerStreet,Berwind,Colorado,2490"}}]);
    },



////////////////////////////////////////////////////////////////////////////////
/// @brief test arrays
////////////////////////////////////////////////////////////////////////////////

    testArrays : function () {
      writeAndCheck([[{"/":[]}]]);
      assertEqual(readAndCheck([["/"]]),[[]]);
      writeAndCheck([[{"/":[1,2,3]}]]);
      assertEqual(readAndCheck([["/"]]),[[1,2,3]]);
      writeAndCheck([[{"/a":[1,2,3]}]]);
      assertEqual(readAndCheck([["/"]]),[{a:[1,2,3]}]);
      writeAndCheck([[{"1":["C","C++","Java","Python"]}]]);
      assertEqual(readAndCheck([["/1"]]),[{1:["C","C++","Java","Python"]}]);
      writeAndCheck([[{"1":["C",2.0,"Java","Python"]}]]);
      assertEqual(readAndCheck([["/1"]]),[{1:["C",2.0,"Java","Python"]}]);
      writeAndCheck([[{"1":["C",2.0,"Java",{"op":"set","new":12,"ttl":7}]}]]);
      assertEqual(readAndCheck([["/1"]]),[{"1":["C",2,"Java",{"op":"set","new":12,"ttl":7}]}]);
      writeAndCheck([[{"1":["C",2.0,"Java",{"op":"set","new":12,"ttl":7,"Array":[12,3]}]}]]);
      assertEqual(readAndCheck([["/1"]]),[{"1":["C",2,"Java",{"op":"set","new":12,"ttl":7,"Array":[12,3]}]}]);
      writeAndCheck([[{"2":[[],[],[],[],[[[[[]]]]]]}]]);
      assertEqual(readAndCheck([["/2"]]),[{"2":[[],[],[],[],[[[[[]]]]]]}]);
      writeAndCheck([[{"2":[[[[[[]]]]],[],[],[],[[]]]}]]);
      assertEqual(readAndCheck([["/2"]]),[{"2":[[[[[[]]]]],[],[],[],[[]]]}]);
      writeAndCheck([[{"2":[[[[[["Hello World"],"Hello World"],1],2.0],"C"],[1],[2],[3],[[1,2],3],4]}]]);
      assertEqual(readAndCheck([["/2"]]),[{"2":[[[[[["Hello World"],"Hello World"],1],2.0],"C"],[1],[2],[3],[[1,2],3],4]}]);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test multiple transaction
////////////////////////////////////////////////////////////////////////////////

    testTransaction : function () {
      writeAndCheck([[{"a":{"b":{"c":[1,2,4]},"e":12},"d":false}],
                     [{"a":{"b":{"c":[1,2,3]}}}]]);
      assertEqual(readAndCheck([["a/e"],[ "d","a/b"]]),
                  [{a:{}},{a:{b:{c:[1,2,3]},d:false}}]);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief Test "new" operator
////////////////////////////////////////////////////////////////////////////////

    testOpSetNew : function () {
      writeAndCheck([[{"a/z":{"op":"set","new":12}}]]);
      assertEqual(readAndCheck([["/a/z"]]), [{"a":{"z":12}}]);
      writeAndCheck([[{"a/y":{"op":"set","new":12, "ttl": 1}}]]);
      assertEqual(readAndCheck([["/a/y"]]), [{"a":{"y":12}}]);
      wait(1.1);
      assertEqual(readAndCheck([["/a/y"]]), [{a:{}}]);
      writeAndCheck([[{"/a/y":{"op":"set","new":12, "ttl": 3}}]]);
      assertEqual(readAndCheck([["a/y"]]), [{"a":{"y":12}}]);
      wait(3.1);
      assertEqual(readAndCheck([["/a/y"]]), [{a:{}}]);
      writeAndCheck([[{"foo/bar":{"op":"set","new":{"baz":12}}}]]);
      assertEqual(readAndCheck([["/foo/bar/baz"]]),
                  [{"foo":{"bar":{"baz":12}}}]);
      assertEqual(readAndCheck([["/foo/bar"]]), [{"foo":{"bar":{"baz":12}}}]);
      assertEqual(readAndCheck([["/foo"]]), [{"foo":{"bar":{"baz":12}}}]);
      writeAndCheck([[{"foo/bar":{"op":"set","new":{"baz":12},"ttl":3}}]]);
      wait(3.1);
      assertEqual(readAndCheck([["/foo"]]), [{"foo":{}}]);
      assertEqual(readAndCheck([["/foo/bar"]]), [{"foo":{}}]);
      assertEqual(readAndCheck([["/foo/bar/baz"]]), [{"foo":{}}]);
      writeAndCheck([[{"a/u":{"op":"set","new":25, "ttl": 3}}]]);
      assertEqual(readAndCheck([["/a/u"]]), [{"a":{"u":25}}]);
      writeAndCheck([[{"a/u":{"op":"set","new":26}}]]);
      assertEqual(readAndCheck([["/a/u"]]), [{"a":{"u":26}}]);
      wait(3.0);  // key should still be there
      assertEqual(readAndCheck([["/a/u"]]), [{"a":{"u":26}}]);
      writeAndCheck([
        [{ "/a/u": { "op":"set", "new":{"z":{"z":{"z":"z"}}}, "ttl":30 }}]]);

      // temporary to make sure we remain with same leader.
      var tmp = agencyLeader;
      var leaderErr = false;

      let res = request({url: agencyLeader + "/_api/agency/stores",
                         method: "GET", followRedirect: true});
      if (res.statusCode === 200) {
        res.bodyParsed = JSON.parse(res.body);
        if (res.bodyParsed.read_db[0].a !== undefined) {
          assertTrue(res.bodyParsed.read_db[1]["/a/u"] >= 0);
        } else {
          leaderErr = true; // not leader
        }
      } else {
        assertTrue(false); // no point in continuing
      }

      // continue ttl test only, if we have not already lost
      // touch with the leader
      if (!leaderErr) {
        writeAndCheck([
          [{ "/a/u": { "op":"set", "new":{"z":{"z":{"z":"z"}}} }}]]);

        res = request({url: agencyLeader + "/_api/agency/stores",
                       method: "GET", followRedirect: true});

        // only, if agency is still led by same guy/girl
        if (agencyLeader === tmp) {
          if (res.statusCode === 200) {
            res.bodyParsed = JSON.parse(res.body);
            console.warn(res.bodyParsed.read_db[0]);
            if (res.bodyParsed.read_db[0].a !== undefined) {
              assertTrue(res.bodyParsed.read_db[1]["/a/u"] === undefined);
            } else {
              leaderErr = true;
            }
          } else {
            assertTrue(false); // no point in continuing
          }
        } else {
          leaderErr = true;
        }
      }

      if (leaderErr) {
        require("console").warn("on the record: status code was " + res.statusCode + " couldn't test proper implementation of TTL at this point. not going to startle the chickens over this however and assume rare leader change within.");
      }
     },

////////////////////////////////////////////////////////////////////////////////
/// @brief Test "push" operator
////////////////////////////////////////////////////////////////////////////////

    testOpPush : function () {
      writeAndCheck([[{"/a/b/c":{"op":"push","new":"max"}}]]);
      assertEqual(readAndCheck([["/a/b/c"]]), [{a:{b:{c:[1,2,3,"max"]}}}]);
      writeAndCheck([[{"/a/euler":{"op":"push","new":2.71828182845904523536}}]]);
      assertEqual(readAndCheck([["/a/euler"]]),
                  [{a:{euler:[2.71828182845904523536]}}]);
      writeAndCheck([[{"/a/euler":{"op":"set","new":2.71828182845904523536}}]]);
      assertEqual(readAndCheck([["/a/euler"]]),
                  [{a:{euler:2.71828182845904523536}}]);
      writeAndCheck([[{"/a/euler":{"op":"push","new":2.71828182845904523536}}]]);
      assertEqual(readAndCheck([["/a/euler"]]),
                  [{a:{euler:[2.71828182845904523536]}}]);

      writeAndCheck([[{"/version":{"op":"set", "new": {"c": ["hello"]}, "ttl":3}}]]);
      assertEqual(readAndCheck([["version"]]), [{version:{c:["hello"]}}]);
      writeAndCheck([[{"/version/c":{"op":"push", "new":"world"}}]]); // int before
      assertEqual(readAndCheck([["version"]]), [{version:{c:["hello","world"]}}]);
      wait(3.1);
      assertEqual(readAndCheck([["version"]]), [{}]);
      writeAndCheck([[{"/version/c":{"op":"push", "new":"hello"}}]]); // int before
      assertEqual(readAndCheck([["version"]]), [{version:{c:["hello"]}}]);

    },

////////////////////////////////////////////////////////////////////////////////
/// @brief Test "remove" operator
////////////////////////////////////////////////////////////////////////////////

    testOpRemove : function () {
      writeAndCheck([[{"/a/euler":{"op":"delete"}}]]);
      assertEqual(readAndCheck([["/a/euler"]]), [{a:{}}]);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief Test "prepend" operator
////////////////////////////////////////////////////////////////////////////////

    testOpPrepend : function () {
      writeAndCheck([[{"/a/b/c":{"op":"prepend","new":3.141592653589793}}]]);
      assertEqual(readAndCheck([["/a/b/c"]]),
                  [{a:{b:{c:[3.141592653589793,1,2,3,"max"]}}}]);
      writeAndCheck(
        [[{"/a/euler":{"op":"prepend","new":2.71828182845904523536}}]]);
      assertEqual(readAndCheck([["/a/euler"]]),
                  [{a:{euler:[2.71828182845904523536]}}]);
      writeAndCheck(
        [[{"/a/euler":{"op":"set","new":2.71828182845904523536}}]]);
      assertEqual(readAndCheck([["/a/euler"]]),
                  [{a:{euler:2.71828182845904523536}}]);
      writeAndCheck(
        [[{"/a/euler":{"op":"prepend","new":2.71828182845904523536}}]]);
      assertEqual(readAndCheck(
        [["/a/euler"]]), [{a:{euler:[2.71828182845904523536]}}]);
      writeAndCheck([[{"/a/euler":{"op":"prepend","new":1.25}}]]);
      assertEqual(readAndCheck([["/a/euler"]]),
                  [{a:{euler:[1.25,2.71828182845904523536]}}]);

      writeAndCheck([[{"/version":{"op":"set", "new": {"c": ["hello"]}, "ttl":3}}]]);
      assertEqual(readAndCheck([["version"]]), [{version:{c:["hello"]}}]);
      writeAndCheck([[{"/version/c":{"op":"prepend", "new":"world"}}]]); // int before
      assertEqual(readAndCheck([["version"]]), [{version:{c:["world","hello"]}}]);
      wait(3.1);
      assertEqual(readAndCheck([["version"]]), [{}]);
      writeAndCheck([[{"/version/c":{"op":"prepend", "new":"hello"}}]]); // int before
      assertEqual(readAndCheck([["version"]]), [{version:{c:["hello"]}}]);

    },

////////////////////////////////////////////////////////////////////////////////
/// @brief Test "shift" operator
////////////////////////////////////////////////////////////////////////////////

    testOpShift : function () {
      writeAndCheck([[{"/a/f":{"op":"shift"}}]]); // none before
      assertEqual(readAndCheck([["/a/f"]]), [{a:{f:[]}}]);
      writeAndCheck([[{"/a/e":{"op":"shift"}}]]); // on empty array
      assertEqual(readAndCheck([["/a/f"]]), [{a:{f:[]}}]);
      writeAndCheck([[{"/a/b/c":{"op":"shift"}}]]); // on existing array
      assertEqual(readAndCheck([["/a/b/c"]]), [{a:{b:{c:[1,2,3,"max"]}}}]);
      writeAndCheck([[{"/a/b/d":{"op":"shift"}}]]); // on existing scalar
      assertEqual(readAndCheck([["/a/b/d"]]), [{a:{b:{d:[]}}}]);

      writeAndCheck([[{"/version":{"op":"set", "new": {"c": ["hello","world"]}, "ttl":3}}]]);
      assertEqual(readAndCheck([["version"]]), [{version:{c:["hello","world"]}}]);
      writeAndCheck([[{"/version/c":{"op":"shift"}}]]); // int before
      assertEqual(readAndCheck([["version"]]), [{version:{c:["world"]}}]);
      wait(3.1);
      assertEqual(readAndCheck([["version"]]), [{}]);
      writeAndCheck([[{"/version/c":{"op":"shift"}}]]); // int before
      assertEqual(readAndCheck([["version"]]), [{version:{c:[]}}]);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief Test "pop" operator
////////////////////////////////////////////////////////////////////////////////

    testOpPop : function () {
      writeAndCheck([[{"/a/f":{"op":"pop"}}]]); // none before
      assertEqual(readAndCheck([["/a/f"]]), [{a:{f:[]}}]);
      writeAndCheck([[{"/a/e":{"op":"pop"}}]]); // on empty array
      assertEqual(readAndCheck([["/a/f"]]), [{a:{f:[]}}]);
      writeAndCheck([[{"/a/b/c":{"op":"pop"}}]]); // on existing array
      assertEqual(readAndCheck([["/a/b/c"]]), [{a:{b:{c:[1,2,3]}}}]);
      writeAndCheck([[{"a/b/d":1}]]); // on existing scalar
      writeAndCheck([[{"/a/b/d":{"op":"pop"}}]]); // on existing scalar
      assertEqual(readAndCheck([["/a/b/d"]]), [{a:{b:{d:[]}}}]);

      writeAndCheck([[{"/version":{"op":"set", "new": {"c": ["hello","world"]}, "ttl":3}}]]);
      assertEqual(readAndCheck([["version"]]), [{version:{c:["hello","world"]}}]);
      writeAndCheck([[{"/version/c":{"op":"pop"}}]]); // int before
      assertEqual(readAndCheck([["version"]]), [{version:{c:["hello"]}}]);
      wait(3.1);
      assertEqual(readAndCheck([["version"]]), [{}]);
      writeAndCheck([[{"/version/c":{"op":"pop"}}]]); // int before
      assertEqual(readAndCheck([["version"]]), [{version:{c:[]}}]);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief Test "pop" operator
////////////////////////////////////////////////////////////////////////////////

    testOpErase : function () {

      writeAndCheck([[{"/version":{"op":"delete"}}]]);

      writeAndCheck([[{"/a":[0,1,2,3,4,5,6,7,8,9]}]]); // none before
      assertEqual(readAndCheck([["/a"]]), [{a:[0,1,2,3,4,5,6,7,8,9]}]);
      writeAndCheck([[{"a":{"op":"erase","val":3}}]]);
      assertEqual(readAndCheck([["/a"]]), [{a:[0,1,2,4,5,6,7,8,9]}]);
      writeAndCheck([[{"a":{"op":"erase","val":3}}]]);
      assertEqual(readAndCheck([["/a"]]), [{a:[0,1,2,4,5,6,7,8,9]}]);
      writeAndCheck([[{"a":{"op":"erase","val":0}}]]);
      assertEqual(readAndCheck([["/a"]]), [{a:[1,2,4,5,6,7,8,9]}]);
      writeAndCheck([[{"a":{"op":"erase","val":1}}]]);
      assertEqual(readAndCheck([["/a"]]), [{a:[2,4,5,6,7,8,9]}]);
      writeAndCheck([[{"a":{"op":"erase","val":2}}]]);
      assertEqual(readAndCheck([["/a"]]), [{a:[4,5,6,7,8,9]}]);
      writeAndCheck([[{"a":{"op":"erase","val":4}}]]);
      assertEqual(readAndCheck([["/a"]]), [{a:[5,6,7,8,9]}]);
      writeAndCheck([[{"a":{"op":"erase","val":5}}]]);
      assertEqual(readAndCheck([["/a"]]), [{a:[6,7,8,9]}]);
      writeAndCheck([[{"a":{"op":"erase","val":9}}]]);
      assertEqual(readAndCheck([["/a"]]), [{a:[6,7,8]}]);
      writeAndCheck([[{"a":{"op":"erase","val":7}}]]);
      assertEqual(readAndCheck([["/a"]]), [{a:[6,8]}]);
      writeAndCheck([[{"a":{"op":"erase","val":6}}],
                     [{"a":{"op":"erase","val":8}}]]);
      assertEqual(readAndCheck([["/a"]]), [{a:[]}]);

      writeAndCheck([[{"/a":[0,1,2,3,4,5,6,7,8,9]}]]); // none before
      assertEqual(readAndCheck([["/a"]]), [{a:[0,1,2,3,4,5,6,7,8,9]}]);
      writeAndCheck([[{"a":{"op":"erase","pos":3}}]]);
      assertEqual(readAndCheck([["/a"]]), [{a:[0,1,2,4,5,6,7,8,9]}]);
      writeAndCheck([[{"a":{"op":"erase","pos":0}}]]);
      assertEqual(readAndCheck([["/a"]]), [{a:[1,2,4,5,6,7,8,9]}]);
      writeAndCheck([[{"a":{"op":"erase","pos":0}}]]);
      assertEqual(readAndCheck([["/a"]]), [{a:[2,4,5,6,7,8,9]}]);
      writeAndCheck([[{"a":{"op":"erase","pos":2}}]]);
      assertEqual(readAndCheck([["/a"]]), [{a:[2,4,6,7,8,9]}]);
      writeAndCheck([[{"a":{"op":"erase","pos":4}}]]);
      assertEqual(readAndCheck([["/a"]]), [{a:[2,4,6,7,9]}]);
      writeAndCheck([[{"a":{"op":"erase","pos":2}}]]);
      assertEqual(readAndCheck([["/a"]]), [{a:[2,4,7,9]}]);
      writeAndCheck([[{"a":{"op":"erase","pos":2}}]]);
      assertEqual(readAndCheck([["/a"]]), [{a:[2,4,9]}]);
      writeAndCheck([[{"a":{"op":"erase","pos":0}}]]);
      assertEqual(readAndCheck([["/a"]]), [{a:[4,9]}]);
      writeAndCheck([[{"a":{"op":"erase","pos":1}}],
                     [{"a":{"op":"erase","pos":0}}]]);
      assertEqual(readAndCheck([["/a"]]), [{a:[]}]);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief Test "pop" operator
////////////////////////////////////////////////////////////////////////////////

    testOpReplace : function () {
      writeAndCheck([[{"/version":{"op":"delete"}}]]); // clear
      writeAndCheck([[{"/a":[0,1,2,3,4,5,6,7,8,9]}]]);
      assertEqual(readAndCheck([["/a"]]), [{a:[0,1,2,3,4,5,6,7,8,9]}]);
      writeAndCheck([[{"a":{"op":"replace","val":3,"new":"three"}}]]);
      assertEqual(readAndCheck([["/a"]]), [{a:[0,1,2,"three",4,5,6,7,8,9]}]);
      writeAndCheck([[{"a":{"op":"replace","val":1,"new":[1]}}]]);
      assertEqual(readAndCheck([["/a"]]), [{a:[0,[1],2,"three",4,5,6,7,8,9]}]);
      writeAndCheck([[{"a":{"op":"replace","val":[1],"new":[1,2,3]}}]]);
      assertEqual(readAndCheck([["/a"]]),
                  [{a:[0,[1,2,3],2,"three",4,5,6,7,8,9]}]);
      writeAndCheck([[{"a":{"op":"replace","val":[1,2,3],"new":[1,2,3]}}]]);
      assertEqual(readAndCheck([["/a"]]),
                  [{a:[0,[1,2,3],2,"three",4,5,6,7,8,9]}]);
      writeAndCheck([[{"a":{"op":"replace","val":4,"new":[1,2,3]}}]]);
      assertEqual(readAndCheck([["/a"]]),
                  [{a:[0,[1,2,3],2,"three",[1,2,3],5,6,7,8,9]}]);
      writeAndCheck([[{"a":{"op":"replace","val":9,"new":[1,2,3]}}]]);
      assertEqual(readAndCheck([["/a"]]),
                  [{a:[0,[1,2,3],2,"three",[1,2,3],5,6,7,8,[1,2,3]]}]);
      writeAndCheck([[{"a":{"op":"replace","val":[1,2,3],"new":{"a":0}}}]]);
      assertEqual(readAndCheck([["/a"]]),
                  [{a:[0,{a:0},2,"three",{a:0},5,6,7,8,{a:0}]}]);
      writeAndCheck([[{"a":{"op":"replace","val":{"a":0},"new":"a"}}]]);
      assertEqual(readAndCheck([["/a"]]),
                  [{a:[0,"a",2,"three","a",5,6,7,8,"a"]}]);
      writeAndCheck([[{"a":{"op":"replace","val":"a","new":"/a"}}]]);
      assertEqual(readAndCheck([["/a"]]),
                  [{a:[0,"/a",2,"three","/a",5,6,7,8,"/a"]}]);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief Test "increment" operator
////////////////////////////////////////////////////////////////////////////////

    testOpIncrement : function () {
      writeAndCheck([[{"/version":{"op":"delete"}}]]);
      writeAndCheck([[{"/version":{"op":"increment"}}]]); // none before
      assertEqual(readAndCheck([["version"]]), [{version:1}]);
      writeAndCheck([[{"/version":{"op":"increment"}}]]); // int before
      assertEqual(readAndCheck([["version"]]), [{version:2}]);
      writeAndCheck([[{"/version":{"op":"set", "new": {"c":12}, "ttl":3}}]]); // int before
      assertEqual(readAndCheck([["version"]]), [{version:{c:12}}]);
      writeAndCheck([[{"/version/c":{"op":"increment"}}]]); // int before
      assertEqual(readAndCheck([["version"]]), [{version:{c:13}}]);
      wait(3.1);
      assertEqual(readAndCheck([["version"]]), [{}]);
      writeAndCheck([[{"/version/c":{"op":"increment"}}]]); // int before
      assertEqual(readAndCheck([["version"]]), [{version:{c:1}}]);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief Test "decrement" operator
////////////////////////////////////////////////////////////////////////////////

    testOpDecrement : function () {
      writeAndCheck([[{"/version":{"op":"delete"}}]]);
      writeAndCheck([[{"/version":{"op":"decrement"}}]]); // none before
      assertEqual(readAndCheck([["version"]]), [{version:-1}]);
      writeAndCheck([[{"/version":{"op":"decrement"}}]]); // int before
      assertEqual(readAndCheck([["version"]]), [{version:-2}]);
      writeAndCheck([[{"/version":{"op":"set", "new": {"c":12}, "ttl":3}}]]); // int before
      assertEqual(readAndCheck([["version"]]), [{version:{c:12}}]);
      writeAndCheck([[{"/version/c":{"op":"decrement"}}]]); // int before
      assertEqual(readAndCheck([["version"]]), [{version:{c:11}}]);
      wait(3.1);
      assertEqual(readAndCheck([["version"]]), [{}]);
      writeAndCheck([[{"/version/c":{"op":"decrement"}}]]); // int before
      assertEqual(readAndCheck([["version"]]), [{version:{c:-1}}]);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief Test "op" keyword in other places than as operator
////////////////////////////////////////////////////////////////////////////////

    testOpInStrangePlaces : function () {
      writeAndCheck([[{"/op":12}]]);
      assertEqual(readAndCheck([["/op"]]), [{op:12}]);
      writeAndCheck([[{"/op":{op:"delete"}}]]);
      writeAndCheck([[{"/op/a/b/c":{"op":"set","new":{"op":13}}}]]);
      assertEqual(readAndCheck([["/op/a/b/c"]]), [{op:{a:{b:{c:{op:13}}}}}]);
      writeAndCheck([[{"/op/a/b/c/op":{"op":"increment"}}]]);
      assertEqual(readAndCheck([["/op/a/b/c"]]), [{op:{a:{b:{c:{op:14}}}}}]);
      writeAndCheck([[{"/op/a/b/c/op":{"op":"decrement"}}]]);
      assertEqual(readAndCheck([["/op/a/b/c"]]), [{op:{a:{b:{c:{op:13}}}}}]);
      writeAndCheck([[{"/op/a/b/c/op":{"op":"pop"}}]]);
      assertEqual(readAndCheck([["/op/a/b/c"]]), [{op:{a:{b:{c:{op:[]}}}}}]);
      writeAndCheck([[{"/op/a/b/c/op":{"op":"increment"}}]]);
      assertEqual(readAndCheck([["/op/a/b/c"]]), [{op:{a:{b:{c:{op:1}}}}}]);
      writeAndCheck([[{"/op/a/b/c/op":{"op":"shift"}}]]);
      assertEqual(readAndCheck([["/op/a/b/c"]]), [{op:{a:{b:{c:{op:[]}}}}}]);
      writeAndCheck([[{"/op/a/b/c/op":{"op":"decrement"}}]]);
      assertEqual(readAndCheck([["/op/a/b/c"]]), [{op:{a:{b:{c:{op:-1}}}}}]);
      writeAndCheck([[{"/op/a/b/c/op":{"op":"push","new":-1}}]]);
      assertEqual(readAndCheck([["/op/a/b/c"]]), [{op:{a:{b:{c:{op:[-1]}}}}}]);
      writeAndCheck([[{"/op/a/b/d":{"op":"set","new":{"ttl":14}}}]]);
      assertEqual(readAndCheck([["/op/a/b/d"]]), [{op:{a:{b:{d:{ttl:14}}}}}]);
      writeAndCheck([[{"/op/a/b/d/ttl":{"op":"increment"}}]]);
      assertEqual(readAndCheck([["/op/a/b/d"]]), [{op:{a:{b:{d:{ttl:15}}}}}]);
      writeAndCheck([[{"/op/a/b/d/ttl":{"op":"decrement"}}]]);
      assertEqual(readAndCheck([["/op/a/b/d"]]), [{op:{a:{b:{d:{ttl:14}}}}}]);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief op delete on top node
////////////////////////////////////////////////////////////////////////////////

    testOperatorsOnRootNode : function () {
      writeAndCheck([[{"/":{"op":"delete"}}]]);
      assertEqual(readAndCheck([["/"]]), [{}]);
      writeAndCheck([[{"/":{"op":"increment"}}]]);
      assertEqual(readAndCheck([["/"]]), [1]);
      writeAndCheck([[{"/":{"op":"delete"}}]]);
      writeAndCheck([[{"/":{"op":"decrement"}}]]);
      assertEqual(readAndCheck([["/"]]), [-1]);
      writeAndCheck([[{"/":{"op":"push","new":"Hello"}}]]);
      assertEqual(readAndCheck([["/"]]), [["Hello"]]);
      writeAndCheck([[{"/":{"op":"delete"}}]]);
      writeAndCheck([[{"/":{"op":"push","new":"Hello"}}]]);
      assertEqual(readAndCheck([["/"]]), [["Hello"]]);
      writeAndCheck([[{"/":{"op":"pop"}}]]);
      assertEqual(readAndCheck([["/"]]), [[]]);
      writeAndCheck([[{"/":{"op":"pop"}}]]);
      assertEqual(readAndCheck([["/"]]), [[]]);
      writeAndCheck([[{"/":{"op":"push","new":"Hello"}}]]);
      assertEqual(readAndCheck([["/"]]), [["Hello"]]);
      writeAndCheck([[{"/":{"op":"shift"}}]]);
      assertEqual(readAndCheck([["/"]]), [[]]);
      writeAndCheck([[{"/":{"op":"shift"}}]]);
      assertEqual(readAndCheck([["/"]]), [[]]);
      writeAndCheck([[{"/":{"op":"prepend","new":"Hello"}}]]);
      assertEqual(readAndCheck([["/"]]), [["Hello"]]);
      writeAndCheck([[{"/":{"op":"shift"}}]]);
      assertEqual(readAndCheck([["/"]]), [[]]);
      writeAndCheck([[{"/":{"op":"pop"}}]]);
      assertEqual(readAndCheck([["/"]]), [[]]);
      writeAndCheck([[{"/":{"op":"delete"}}]]);
      assertEqual(readAndCheck([["/"]]), [{}]);
      writeAndCheck([[{"/":{"op":"delete"}}]]);
      assertEqual(readAndCheck([["/"]]), [{}]);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief Test observe / unobserve
////////////////////////////////////////////////////////////////////////////////

    testObserve : function () {
      var res, before, after, clean;
      var trx = [{"/a":"a"}, {"a":{"oldEmpty":true}}];

      // In the beginning
      res = request({url:agencyLeader+"/_api/agency/stores", method:"GET"});
      assertEqual(200, res.statusCode);
      clean = JSON.parse(res.body);

      // Don't create empty object for observation
      writeAndCheck([[{"/a":{"op":"observe", "url":"https://google.com"}}]]);
      assertEqual(readAndCheck([["/"]]), [{}]);
      res = accessAgency("write",[trx]);
      assertEqual(res.statusCode, 200);
      res = accessAgency("write",[trx]);
      assertEqual(res.statusCode, 412);

      writeAndCheck([[{"/":{"op":"delete"}}]]);
      var c = agencyConfig().term;

      // No duplicate entries in
      res = request({url:agencyLeader+"/_api/agency/stores", method:"GET"});
      assertEqual(200, res.statusCode);
      before = JSON.parse(res.body);
      writeAndCheck([[{"/a":{"op":"observe", "url":"https://google.com"}}]]);
      res = request({url:agencyLeader+"/_api/agency/stores", method:"GET"});
      assertEqual(200, res.statusCode);
      after = JSON.parse(res.body);
      if (!_.isEqual(before, after)) {
        if (agencyConfig().term === c) {
          assertEqual(before, after); //peng
        } else {
          require("console").warn("skipping remaining callback tests this time around");
          return; //
        }
      }

      // Normalization
      res = request({url:agencyLeader+"/_api/agency/stores", method:"GET"});
      assertEqual(200, res.statusCode);
      before = JSON.parse(res.body);
      writeAndCheck([[{"//////a////":{"op":"observe", "url":"https://google.com"}}]]);
      writeAndCheck([[{"a":{"op":"observe", "url":"https://google.com"}}]]);
      writeAndCheck([[{"a/":{"op":"observe", "url":"https://google.com"}}]]);
      writeAndCheck([[{"/a/":{"op":"observe", "url":"https://google.com"}}]]);
      res = request({url:agencyLeader+"/_api/agency/stores", method:"GET"});
      assertEqual(200, res.statusCode);
      after = JSON.parse(res.body);
      if (!_.isEqual(before, after)) {
        if (agencyConfig().term === c) {
          assertEqual(before, after); //peng
        } else {
          require("console").warn("skipping remaining callback tests this time around");
          return; //
        }
      }

      // Unobserve
      res = request({url:agencyLeader+"/_api/agency/stores", method:"GET"});
      assertEqual(200, res.statusCode);
      before = JSON.parse(res.body);
      writeAndCheck([[{"//////a":{"op":"unobserve", "url":"https://google.com"}}]]);
      res = request({url:agencyLeader+"/_api/agency/stores", method:"GET"});
      assertEqual(200, res.statusCode);
      after = JSON.parse(res.body);
      assertEqual(clean, after);
      if (!_.isEqual(clean, after)) {
        if (agencyConfig().term === c) {
          assertEqual(clean, after); //peng
        } else {
          require("console").warn("skipping remaining callback tests this time around");
          return; //
        }
      }

      writeAndCheck([[{"/":{"op":"delete"}}]]);
      assertEqual(readAndCheck([["/"]]), [{}]);

    },

////////////////////////////////////////////////////////////////////////////////
/// @brief Test delete / replace / erase should not create new stuff in agency
////////////////////////////////////////////////////////////////////////////////

    testNotCreate : function () {
      var trx = [{"/a":"a"}, {"a":{"oldEmpty":true}}], res;

      // Don't create empty object for observation
      writeAndCheck([[{"a":{"op":"delete"}}]]);
      assertEqual(readAndCheck([["/"]]), [{}]);
      res = accessAgency("write",[trx]);
      assertEqual(res.statusCode, 200);
      res = accessAgency("write",[trx]);
      assertEqual(res.statusCode, 412);
      writeAndCheck([[{"/":{"op":"delete"}}]]);
      assertEqual(readAndCheck([["/"]]), [{}]);

      // Don't create empty object for observation
      writeAndCheck([[{"a":{"op":"replace", "val":1, "new":2}}]]);
      assertEqual(readAndCheck([["/"]]), [{}]);
      res = accessAgency("write",[trx]);
      assertEqual(res.statusCode, 200);
      res = accessAgency("write",[trx]);
      assertEqual(res.statusCode, 412);
      writeAndCheck([[{"/":{"op":"delete"}}]]);
      assertEqual(readAndCheck([["/"]]), [{}]);

      // Don't create empty object for observation
      writeAndCheck([[{"a":{"op":"erase", "val":1}}]]);
      assertEqual(readAndCheck([["/"]]), [{}]);
      res = accessAgency("write",[trx]);
      assertEqual(res.statusCode, 200);
      res = accessAgency("write",[trx]);
      assertEqual(res.statusCode, 412);
      writeAndCheck([[{"/":{"op":"delete"}}]]);
      assertEqual(readAndCheck([["/"]]), [{}]);

    },

////////////////////////////////////////////////////////////////////////////////
/// @brief Test that order should not matter
////////////////////////////////////////////////////////////////////////////////

    testOrder : function () {
      writeAndCheck([[{"a":{"b":{"c":[1,2,3]},"e":12},"d":false}]]);
      assertEqual(readAndCheck([["a/e"],[ "d","a/b"]]),
                  [{a:{e:12}},{a:{b:{c:[1,2,3]},d:false}}]);
      writeAndCheck([[{"/":{"op":"delete"}}]]);
      writeAndCheck([[{"d":false, "a":{"b":{"c":[1,2,3]},"e":12}}]]);
      assertEqual(readAndCheck([["a/e"],[ "d","a/b"]]),
                  [{a:{e:12}},{a:{b:{c:[1,2,3]},d:false}}]);
      writeAndCheck([[{"d":false, "a":{"e":12,"b":{"c":[1,2,3]}}}]]);
      assertEqual(readAndCheck([["a/e"],[ "d","a/b"]]),
                  [{a:{e:12}},{a:{b:{c:[1,2,3]},d:false}}]);
      writeAndCheck([[{"d":false, "a":{"e":12,"b":{"c":[1,2,3]}}}]]);
      assertEqual(readAndCheck([["a/e"],["a/b","d"]]),
                  [{a:{e:12}},{a:{b:{c:[1,2,3]},d:false}}]);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief Test nasty willful attempt to break
////////////////////////////////////////////////////////////////////////////////

    testOrderEvil : function () {
      writeAndCheck([[{"a":{"b":{"c":[1,2,3]},"e":12},"d":false}]]);
      assertEqual(readAndCheck([["a/e"],[ "d","a/b"]]),
                  [{a:{e:12}},{a:{b:{c:[1,2,3]},d:false}}]);
      writeAndCheck([[{"/":{"op":"delete"}}]]);
      writeAndCheck([[{"d":false, "a":{"b":{"c":[1,2,3]},"e":12}}]]);
      assertEqual(readAndCheck([["a/e"],[ "d","a/b"]]),
                  [{a:{e:12}},{a:{b:{c:[1,2,3]},d:false}}]);
      writeAndCheck([[{"d":false, "a":{"e":12,"b":{"c":[1,2,3]}}}]]);
      assertEqual(readAndCheck([["a/e"],[ "d","a/b"]]),
                  [{a:{e:12}},{a:{b:{c:[1,2,3]},d:false}}]);
      writeAndCheck([[{"d":false, "a":{"e":12,"b":{"c":[1,2,3]}}}]]);
      assertEqual(readAndCheck([["a/e"],["a/b","d"]]),
                  [{a:{e:12}},{a:{b:{c:[1,2,3]},d:false}}]);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief Test nasty willful attempt to break
////////////////////////////////////////////////////////////////////////////////

    testSlashORama : function () {
      writeAndCheck([[{"/":{"op":"delete"}}]]);
      writeAndCheck([[{"//////////////////////a/////////////////////b//":
                       {"b///////c":4}}]]);
      assertEqual(readAndCheck([["/"]]), [{a:{b:{b:{c:4}}}}]);
      writeAndCheck([[{"/":{"op":"delete"}}]]);
      writeAndCheck([[{"////////////////////////": "Hi there!"}]]);
      assertEqual(readAndCheck([["/"]]), ["Hi there!"]);
      writeAndCheck([[{"/":{"op":"delete"}}]]);
      writeAndCheck(
        [[{"/////////////////\\/////a/////////////^&%^&$^&%$////////b\\\n//":
           {"b///////c":4}}]]);
      assertEqual(readAndCheck([["/"]]),
                  [{"\\":{"a":{"^&%^&$^&%$":{"b\\\n":{"b":{"c":4}}}}}}]);
    },

    testKeysBeginningWithSameString: function() {
      var res = accessAgency("write",[[{"/bumms":{"op":"set","new":"fallera"}, "/bummsfallera": {"op":"set","new":"lalalala"}}]]);
      assertEqual(res.statusCode, 200);
      assertEqual(readAndCheck([["/bumms", "/bummsfallera"]]), [{bumms:"fallera", bummsfallera: "lalalala"}]);
    },

    testHiddenAgencyWrite: function() {
      var res = accessAgency("write",[[{".agency": {"op":"set","new":"fallera"}}]]);
      assertEqual(res.statusCode, 403);
    },

    testHiddenAgencyWriteSlash: function() {
      var res = accessAgency("write",[[{"/.agency": {"op":"set","new":"fallera"}}]]);
      assertEqual(res.statusCode, 403);
    },

    testHiddenAgencyWriteDeep: function() {
      var res = accessAgency("write",[[{"/.agency/hans": {"op":"set","new":"fallera"}}]]);
      assertEqual(res.statusCode, 403);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief Compaction
////////////////////////////////////////////////////////////////////////////////

    testLogCompaction: function() {
      // Find current log index and erase all data:
      let cur = accessAgency("write",[[{"/": {"op":"delete"}}]]).
          bodyParsed.results[0];

      let count = compactionConfig.compactionStepSize - 100 - cur;
      require("console").topic("agency=info", "Avoiding log compaction for now with", count,
        "keys, from log entry", cur, "on.");
      doCountTransactions(count, 0);

      // Now trigger one log compaction and check all keys:
      let count2 = compactionConfig.compactionStepSize + 100 - (cur + count);
      require("console").topic("agency=info", "Provoking log compaction for now with", count2,
        "keys, from log entry", cur + count, "on.");
      doCountTransactions(count2, count);

      // All tests so far have not really written many log entries in
      // comparison to the compaction interval (with the default settings),
      let count3 = 2 * compactionConfig.compactionStepSize + 100
        - (cur + count + count2);
      require("console").topic("agency=info", "Provoking second log compaction for now with",
        count3, "keys, from log entry", cur + count + count2, "on.");
      doCountTransactions(count3, count + count2);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief Huge transaction package
////////////////////////////////////////////////////////////////////////////////

    testHugeTransactionPackage : function() {
      writeAndCheck([[{"a":{"op":"delete"}}]]); // cleanup first
      var huge = [];
      for (var i = 0; i < 20000; ++i) {
        huge.push([{"a":{"op":"increment"}}, {}, "huge" + i]);
      }
      writeAndCheck(huge, 600);
      assertEqual(readAndCheck([["a"]]), [{"a":20000}]);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief Huge transaction package, inc/dec
////////////////////////////////////////////////////////////////////////////////

    testTransactionWithIncDec : function() {
      writeAndCheck([[{"a":{"op":"delete"}}]]); // cleanup first
      var trx = [];
      for (var i = 0; i < 100; ++i) {
        trx.push([{"a":{"op":"increment"}}, {}, "inc" + i]);
        trx.push([{"a":{"op":"decrement"}}, {}, "dec" + i]);
      }
      writeAndCheck(trx);
      assertEqual(readAndCheck([["a"]]), [{"a":0}]);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief Transaction, update of same key
////////////////////////////////////////////////////////////////////////////////

    testTransactionUpdateSameKey : function() {
      writeAndCheck([[{"a":{"op":"delete"}}]]); // cleanup first
      var trx = [];
      trx.push([{"a":"foo"}]);
      trx.push([{"a":"bar"}]);
      writeAndCheck(trx);
      assertEqual(readAndCheck([["a"]]), [{"a":"bar"}]);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief Transaction, insert and remove of same key
////////////////////////////////////////////////////////////////////////////////

    testTransactionInsertRemoveSameKey : function() {
      writeAndCheck([[{"a":{"op":"delete"}}]]); // cleanup first
      var trx = [];
      trx.push([{"a":"foo"}]);
      trx.push([{"a":{"op":"delete"}}]);
      writeAndCheck(trx);
      assertEqual(readAndCheck([["/a"]]), [{}]);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief Huge transaction package, all different keys
////////////////////////////////////////////////////////////////////////////////

    testTransactionDifferentKeys : function() {
      writeAndCheck([[{"a":{"op":"delete"}}]]); // cleanup first
      var huge = [], i;
      for (i = 0; i < 100; ++i) {
        huge.push([{["a" + i]:{"op":"increment"}}, {}, "diff" + i]);
      }
      writeAndCheck(huge);
      for (i = 0; i < 100; ++i) {
        assertEqual(readAndCheck([["a" + i]]), [{["a" + i]:1}]);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief Test compaction step/keep
////////////////////////////////////////////////////////////////////////////////

    // Test currently deactivated, it at the very least takes very long,
    // it might be broken in its entirety.
    /*testCompactionStepKeep : function() {

      // prepare transaction package for tests
      var transaction = [], i;
      for (i = 0; i < compactionConfig.compactionStepSize; i++) {
        transaction.push([{"foobar":{"op":"increment"}}]);
      }
      writeAndCheck([[{"/":{"op":"delete"}}]]); // cleanup first
      writeAndCheck([[{"foobar":0}]]); // cleanup first
      var foobar = accessAgency("read", [["foobar"]]).bodyParsed[0].foobar;

      var llogi = evalComp();
      assertTrue(llogi > 0);

      // at this limit we should see keep size to kick in
      var lim = compactionConfig.compactionKeepSize - llogi;

      // 1st package
      writeAndCheck(transaction);
      lim -= transaction.length;
      assertTrue(evalComp()>0);

      writeAndCheck(transaction);
      lim -= transaction.length;
      assertTrue(evalComp()>0);

      while(lim > compactionConfig.compactionStepSize) {
        writeAndCheck(transaction);
        lim -= transaction.length;
      }
      assertTrue(evalComp()>0);

      writeAndCheck(transaction);
      assertTrue(evalComp()>0);

      writeAndCheck(transaction);
      assertTrue(evalComp()>0);

      writeAndCheck(transaction);
      assertTrue(evalComp()>0);

    }
    */
  };
}


////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(agencyTestSuite);

return jsunity.done();
