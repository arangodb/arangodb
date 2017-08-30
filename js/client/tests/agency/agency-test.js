/*jshint globalstrict:false, strict:true */
/*global assertEqual, ARGUMENTS */

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

  function findAgencyCompactionIntervals() {
    let res = request({url: agencyLeader + "/_api/agency/config",
                       method: "GET", followRedirect: true});
    assertEqual(res.statusCode, 200);
    res.bodyParsed = JSON.parse(res.body);
    return {
      compactionStepSize: res.bodyParsed.configuration["compaction step size"],
      compactionKeepSize: res.bodyParsed.configuration["compaction keep size"]
    };
  }

  var compactionConfig = findAgencyCompactionIntervals();
  require("console").warn("Agency compaction configuration: ", compactionConfig);

  function accessAgency(api, list) {
    // We simply try all agency servers in turn until one gives us an HTTP
    // response:
    var res;
    while (true) {
      res = request({url: agencyLeader + "/_api/agency/" + api,
                     method: "POST", followRedirect: false,
                     body: JSON.stringify(list),
                     headers: {"Content-Type": "application/json"},
                     timeout: 240  /* essentially for the huge trx package
                                      running under ASAN in the CI */ });
      if(res.statusCode === 307) {
        agencyLeader = res.headers.location;
        var l = 0;
        for (var i = 0; i < 3; ++i) {
          l = agencyLeader.indexOf('/', l+1);
        }
        agencyLeader = agencyLeader.substring(0,l);
        require('console').warn('Redirected to ' + agencyLeader);
      } else if (res.statusCode !== 503) {
        break;
      } else {
        require('console').warn('Waiting for leader ... ');
        wait(1.0);
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

  function writeAndCheck(list) {
    var res = accessAgency("write", list);
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
    let trxs = [];
    for (i = start; i < start + count; ++i) {
      let key = "/key"+i;
      let trx = [{}];
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
      res = accessAgency("write",[[{"/a/b/c":{"op":"set","new":13}},{"a":{"b":{"c":12}}}]]);
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
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test clientIds
////////////////////////////////////////////////////////////////////////////////

    testClientIds : function () {
      var res;

      var id = [guid(),guid(),guid(),guid(),guid(),guid(),
                guid(),guid(),guid(),guid(),guid(),guid(),
                guid(),guid(),guid()];
      var query = [{"a":12},{"a":13},{"a":13}];
      var pre = [{},{"a":12},{"a":12}];

      writeAndCheck([[query[0], pre[0], id[0]]]);
      res = accessAgency("inquire",[id[0]]).bodyParsed;
      assertEqual(res.length, 1);
      assertEqual(res[0].length, 1);
      assertEqual(res[0][0].query, query[0]);

      writeAndCheck([[query[1], pre[1], id[0]]]);
      res = accessAgency("inquire",[id[0]]).bodyParsed;
      assertEqual(res.length, 1);
      assertEqual(res[0].length, 2);
      assertEqual(res[0][0].query, query[0]);
      assertEqual(res[0][1].query, query[1]);

      res = accessAgency("write",[[query[1], pre[1], id[2]]]);
      assertEqual(res.statusCode,412);
      res = accessAgency("inquire",[id[2]]).bodyParsed;
      assertEqual(res[0].length, 0);

      res = accessAgency("write",[[query[0], pre[0], id[3]],
                                  [query[1], pre[1], id[3]]]);
      assertEqual(res.statusCode,200);
      res = accessAgency("inquire",[id[3]]).bodyParsed;
      assertEqual(res.length, 1);
      assertEqual(res[0][0].query, query[0]);
      assertEqual(res[0][1].query, query[1]);
      
      res = accessAgency("write",[[query[0], pre[0], id[4]],
                                  [query[1], pre[1], id[4]],
                                  [query[2], pre[2], id[4]]]);
      assertEqual(res.statusCode,412);
      res = accessAgency("inquire",[id[4]]).bodyParsed;
      assertEqual(res.length, 1);
      assertEqual(res[0].length, 2);
      assertEqual(res[0][0].query, query[0]);
      assertEqual(res[0][1].query, query[1]);
      
      res = accessAgency("write",[[query[0], pre[0], id[5]],
                                  [query[2], pre[2], id[5]],
                                  [query[1], pre[1], id[5]]]);
      assertEqual(res.statusCode,412);

      res = accessAgency("inquire",[id[5]]).bodyParsed;
      assertEqual(res.length, 1);
      assertEqual(res[0].length, 2);
      assertEqual(res[0][0].query, query[0]);
      assertEqual(res[0][1].query, query[1]);
      
      res = accessAgency("write",[[query[2], pre[2], id[6]],
                                  [query[0], pre[0], id[6]],
                                  [query[1], pre[1], id[6]]]);
      assertEqual(res.statusCode,412);
      res = accessAgency("inquire",[id[6]]).bodyParsed;
      assertEqual(res.length, 1);
      assertEqual(res[0].length, 2);
      assertEqual(res[0][0].query, query[0]);
      assertEqual(res[0][1].query, query[1]);
      
      res = accessAgency("write",[[query[2], pre[2], id[7]],
                                  [query[0], pre[0], id[8]],
                                  [query[1], pre[1], id[9]]]);
      assertEqual(res.statusCode,412);
      res = accessAgency("inquire",[id[7],id[8],id[9]]).bodyParsed;
      assertEqual(res.length, 3);
      assertEqual(res[0].length, 0);
      assertEqual(res[1].length, 1);
      assertEqual(res[1][0].query, query[0]);
      assertEqual(res[2].length, 1);
      assertEqual(res[2][0].query, query[1]);

      res = accessAgency("inquire",[id[9],id[7],id[8]]).bodyParsed;
      assertEqual(res.length, 3);
      assertEqual(res[0].length, 1);
      assertEqual(res[0][0].query, query[1]);
      assertEqual(res[1].length, 0);
      assertEqual(res[2].length, 1);
      assertEqual(res[2][0].query, query[0]);

      res = accessAgency("inquire",[id[8],id[9],id[7]]).bodyParsed;
      assertEqual(res.length, 3);
      assertEqual(res[0].length, 1);
      assertEqual(res[0][0].query, query[0]);
      assertEqual(res[1].length, 1);
      assertEqual(res[1][0].query, query[1]);
      assertEqual(res[2].length, 0);

      res = accessAgency("inquire",[id[7],id[9],id[8]]).bodyParsed;
      assertEqual(res.length, 3);
      assertEqual(res[0].length, 0);
      assertEqual(res[1].length, 1);
      assertEqual(res[1][0].query, query[1]);
      assertEqual(res[2].length, 1);
      assertEqual(res[2][0].query, query[0]);

      res = accessAgency("inquire",[id[8],id[7],id[9]]).bodyParsed;
      assertEqual(res.length, 3);
      assertEqual(res[0].length, 1);
      assertEqual(res[0][0].query, query[0]);
      assertEqual(res[1].length, 0);
      assertEqual(res[2].length, 1);
      assertEqual(res[2][0].query, query[1]);

      res = accessAgency("inquire",[id[7],id[8],id[9]]).bodyParsed;
      assertEqual(res.length, 3);
      assertEqual(res[0].length, 0);
      assertEqual(res[1].length, 1);
      assertEqual(res[1][0].query, query[0]);
      assertEqual(res[2].length, 1);
      assertEqual(res[2][0].query, query[1]);
      
      res = accessAgency("inquire",[id[7],id[8],id[9]]).bodyParsed;
      assertEqual(res.length, 3);
      assertEqual(res[0].length, 0);
      assertEqual(res[1].length, 1);
      assertEqual(res[1][0].query, query[0]);
      assertEqual(res[2].length, 1);
      assertEqual(res[2][0].query, query[1]);
      
      res = accessAgency("write",[[query[2], pre[2], id[10]],
                                  [query[0], pre[0], id[11]],
                                  [query[1], pre[1], id[12]],
                                  [query[0], pre[0], id[12]]]);

      res = accessAgency("inquire",[id[10],id[11],id[12]]).bodyParsed;
      assertEqual(res.length, 3);
      assertEqual(res[0].length, 0);
      assertEqual(res[1].length, 1);
      assertEqual(res[1][0].query, query[0]);
      assertEqual(res[2].length, 2);
      assertEqual(res[2][0].query, query[1]);
      assertEqual(res[2][1].query, query[0]);
      
      res = accessAgency("transact",[[query[0], pre[0], id[13]],
                                     [query[2], pre[2], id[13]],
                                     [query[1], pre[1], id[13]],
                                     ["a"]]);

      
      assertEqual(res.statusCode,412);
      assertEqual(res.bodyParsed.length, 4);
      assertEqual(res.bodyParsed[0] > 0, true);
      assertEqual(res.bodyParsed[1] > 0, true);
      assertEqual(res.bodyParsed[2], {a : 13});
      assertEqual(res.bodyParsed[3], query[1]);

      res = accessAgency("inquire",[id[13]]).bodyParsed;
      assertEqual(res.length, 1);
      assertEqual(res[0].length, 2);
      assertEqual(res[0][0].query, query[0]);
      assertEqual(res[0][1].query, query[2]);
      
    },

    
////////////////////////////////////////////////////////////////////////////////
/// @brief test document/transaction assignment
////////////////////////////////////////////////////////////////////////////////

    testDocument : function () {
      writeAndCheck([[{"a":{"b":{"c":[1,2,3]},"e":12},"d":false}]]);
      assertEqual(readAndCheck([["a/e"],[ "d","a/b"]]),
                  [{a:{e:12}},{a:{b:{c:[1,2,3]},d:false}}]);
      writeAndCheck(  [[{"a":{"_id":"576d1b7becb6374e24ed5a04","index":0,"guid":"60ffa50e-0211-4c60-a305-dcc8063ae2a5","isActive":true,"balance":"$1,050.96","picture":"http://placehold.it/32x32","age":30,"eyeColor":"green","name":{"first":"Maura","last":"Rogers"},"company":"GENESYNK","email":"maura.rogers@genesynk.net","phone":"+1(804)424-2766","address":"501RiverStreet,Wollochet,Vermont,6410","about":"Temporsintofficiaipsumidnullalaboreminimlaborisinlaborumincididuntexcepteurdolore.Sunteumagnadolaborumsunteaquisipsumaliquaaliquamagnaminim.Cupidatatadproidentullamconisietofficianisivelitculpaexcepteurqui.Suntautemollitconsecteturnulla.Commodoquisidmagnaestsitelitconsequatdoloreupariaturaliquaetid.","registered":"Friday,November28,20148:01AM","latitude":"-30.093679","longitude":"10.469577","tags":["laborum","proident","est","veniam","sunt"],"range":[0,1,2,3,4,5,6,7,8,9],"friends":[{"id":0,"name":"CarverDurham"},{"id":1,"name":"DanielleMalone"},{"id":2,"name":"ViolaBell"}],"greeting":"Hello,Maura!Youhave9unreadmessages.","favoriteFruit":"banana"}}],[{"!!@#$%^&*)":{"_id":"576d1b7bb2c1af32dd964c22","index":1,"guid":"e6bda5a9-54e3-48ea-afd7-54915fec48c2","isActive":false,"balance":"$2,631.75","picture":"http://placehold.it/32x32","age":40,"eyeColor":"blue","name":{"first":"Jolene","last":"Todd"},"company":"QUANTASIS","email":"jolene.todd@quantasis.us","phone":"+1(954)418-2311","address":"818ButlerStreet,Berwind,Colorado,2490","about":"Commodoesseveniamadestirureutaliquipduistempor.Auteeuametsuntessenisidolorfugiatcupidatatsintnulla.Sitanimincididuntelitculpasunt.","registered":"Thursday,June12,201412:08AM","latitude":"-7.101063","longitude":"4.105685","tags":["ea","est","sunt","proident","pariatur"],"range":[0,1,2,3,4,5,6,7,8,9],"friends":[{"id":0,"name":"SwansonMcpherson"},{"id":1,"name":"YoungTyson"},{"id":2,"name":"HinesSandoval"}],"greeting":"Hello,Jolene!Youhave5unreadmessages.","favoriteFruit":"strawberry"}}],[{"1234567890":{"_id":"576d1b7b79527b6201ed160c","index":2,"guid":"2d2d7a45-f931-4202-853d-563af252ca13","isActive":true,"balance":"$1,446.93","picture":"http://placehold.it/32x32","age":28,"eyeColor":"blue","name":{"first":"Pickett","last":"York"},"company":"ECSTASIA","email":"pickett.york@ecstasia.me","phone":"+1(901)571-3225","address":"556GrovePlace,Stouchsburg,Florida,9119","about":"Idnulladolorincididuntirurepariaturlaborumutmolliteavelitnonveniaminaliquip.Adametirureesseanimindoloreduisproidentdeserunteaconsecteturincididuntconsecteturminim.Ullamcoessedolorelitextemporexcepteurexcepteurlaboreipsumestquispariaturmagna.ExcepteurpariaturexcepteuradlaborissitquieiusmodmagnalaborisincididuntLoremLoremoccaecat.","registered":"Thursday,January28,20165:20PM","latitude":"-56.18036","longitude":"-39.088125","tags":["ad","velit","fugiat","deserunt","sint"],"range":[0,1,2,3,4,5,6,7,8,9],"friends":[{"id":0,"name":"BarryCleveland"},{"id":1,"name":"KiddWare"},{"id":2,"name":"LangBrooks"}],"greeting":"Hello,Pickett!Youhave10unreadmessages.","favoriteFruit":"strawberry"}}],[{"":{"_id":"576d1b7bc674d071a2bccc05","index":3,"guid":"14b44274-45c2-4fd4-8c86-476a286cb7a2","isActive":true,"balance":"$1,861.79","picture":"http://placehold.it/32x32","age":27,"eyeColor":"brown","name":{"first":"Felecia","last":"Baird"},"company":"SYBIXTEX","email":"felecia.baird@sybixtex.name","phone":"+1(821)498-2971","address":"571HarrisonAvenue,Roulette,Missouri,9284","about":"Adesseofficianisiexercitationexcepteurametconsecteturessequialiquaquicupidatatincididunt.Nostrudullamcoutlaboreipsumduis.ConsequatsuntlaborumadLoremeaametveniamesseoccaecat.","registered":"Monday,December21,20156:50AM","latitude":"0.046813","longitude":"-13.86172","tags":["velit","qui","ut","aliquip","eiusmod"],"range":[0,1,2,3,4,5,6,7,8,9],"friends":[{"id":0,"name":"CeliaLucas"},{"id":1,"name":"HensonKline"},{"id":2,"name":"ElliottWalker"}],"greeting":"Hello,Felecia!Youhave9unreadmessages.","favoriteFruit":"apple"}}],[{"|}{[]αв¢∂єƒgαв¢∂єƒg":{"_id":"576d1b7be4096344db437417","index":4,"guid":"f789235d-b786-459f-9288-0d2f53058d02","isActive":false,"balance":"$2,011.07","picture":"http://placehold.it/32x32","age":28,"eyeColor":"brown","name":{"first":"Haney","last":"Burks"},"company":"SPACEWAX","email":"haney.burks@spacewax.info","phone":"+1(986)587-2735","address":"197OtsegoStreet,Chesterfield,Delaware,5551","about":"Quisirurenostrudcupidatatconsequatfugiatvoluptateproidentvoluptate.Duisnullaadipisicingofficiacillumsuntlaborisdeseruntirure.Laborumconsecteturelitreprehenderitestcillumlaboresintestnisiet.Suntdeseruntexercitationutauteduisaliquaametetquisvelitconsecteturirure.Auteipsumminimoccaecatincididuntaute.Irureenimcupidatatexercitationutad.Minimconsecteturadipisicingcommodoanim.","registered":"Friday,January16,20155:29AM","latitude":"86.036358","longitude":"-1.645066","tags":["occaecat","laboris","ipsum","culpa","est"],"range":[0,1,2,3,4,5,6,7,8,9],"friends":[{"id":0,"name":"SusannePacheco"},{"id":1,"name":"SpearsBerry"},{"id":2,"name":"VelazquezBoyle"}],"greeting":"Hello,Haney!Youhave10unreadmessages.","favoriteFruit":"apple"}}]]);
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
                  [{a:{e:12}},{a:{b:{c:[1,2,3]},d:false}}]);
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
      writeAndCheck([[{"/a/y":{"op":"set","new":12, "ttl": 1}}]]);
      writeAndCheck([[{"/a/y":{"op":"set","new":12}}]]);
      assertEqual(readAndCheck([["a/y"]]), [{"a":{"y":12}}]);
      wait(1.1);
      assertEqual(readAndCheck([["/a/y"]]), [{"a":{"y":12}}]);
      writeAndCheck([[{"foo/bar":{"op":"set","new":{"baz":12}}}]]);
      assertEqual(readAndCheck([["/foo/bar/baz"]]),
                  [{"foo":{"bar":{"baz":12}}}]);
      assertEqual(readAndCheck([["/foo/bar"]]), [{"foo":{"bar":{"baz":12}}}]);
      assertEqual(readAndCheck([["/foo"]]), [{"foo":{"bar":{"baz":12}}}]);
      writeAndCheck([[{"foo/bar":{"op":"set","new":{"baz":12},"ttl":1}}]]);
      wait(1.1);
      assertEqual(readAndCheck([["/foo"]]), [{"foo":{}}]);
      assertEqual(readAndCheck([["/foo/bar"]]), [{"foo":{}}]);
      assertEqual(readAndCheck([["/foo/bar/baz"]]), [{"foo":{}}]);
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
      assertEqual(res.statusCode, 200);
    }, 
    
    testHiddenAgencyWriteSlash: function() {
      var res = accessAgency("write",[[{"/.agency": {"op":"set","new":"fallera"}}]]);
      assertEqual(res.statusCode, 200);
    },
    
    testHiddenAgencyWriteDeep: function() {
      var res = accessAgency("write",[[{"/.agency/hans": {"op":"set","new":"fallera"}}]]);
      assertEqual(res.statusCode, 200);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief Compaction
////////////////////////////////////////////////////////////////////////////////    
    
    testLogCompaction: function() {
      // Find current log index and erase all data:
      let cur = accessAgency("write",[[{"/": {"op":"delete"}}]]).
          bodyParsed.results[0];

      let count = compactionConfig.compactionStepSize - 100 - cur;
      require("console").warn("Avoiding log compaction for now with", count,
        "keys, from log entry", cur, "on.");
      doCountTransactions(count, 0);

      // Now trigger one log compaction and check all keys:
      let count2 = compactionConfig.compactionStepSize + 100 - (cur + count);
      require("console").warn("Provoking log compaction for now with", count2,
        "keys, from log entry", cur + count, "on.");
      doCountTransactions(count2, count);

      // All tests so far have not really written many log entries in 
      // comparison to the compaction interval (with the default settings),
      let count3 = 2 * compactionConfig.compactionStepSize + 100 
        - (cur + count + count2);
      require("console").warn("Provoking second log compaction for now with", 
        count3, "keys, from log entry", cur + count + count2, "on.");
      doCountTransactions(count3, count + count2);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief Huge transaction package
////////////////////////////////////////////////////////////////////////////////

    testHugeTransactionPackage : function() {
      var huge = [];
      for (var i = 0; i < 20000; ++i) {
        huge.push([{"a":{"op":"increment"}}]);
      }
      writeAndCheck(huge);
      assertEqual(readAndCheck([["a"]]), [{"a":20000}]);
    }

  };
}


////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(agencyTestSuite);

return jsunity.done();

