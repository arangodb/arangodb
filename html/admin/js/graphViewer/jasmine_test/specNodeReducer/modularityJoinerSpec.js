/*jslint indent: 2, nomen: true, maxlen: 100, white: true  plusplus: true */
/*global beforeEach, afterEach, jasmine */
/*global runs, waitsFor */
/*global describe, it, expect, spyOn */
/*global window, eb, loadFixtures, document, console*/
/*global $, _, d3*/
/*global helper*/
/*global ModularityJoiner, WebWorkerWrapper*/

////////////////////////////////////////////////////////////////////////////////
/// @brief Graph functionality
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2010-2012 triagens GmbH, Cologne, Germany
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
/// @author Michael Hackstein
/// @author Copyright 2011-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////


(function () {
  "use strict";

  describe('Modularity Joiner', function () {
    
    beforeEach(function() {
      this.addMatchers({
        toContainNodes: function(ns) {
          var com = this.actual,
          check = true;
          this.message = function() {
            return "Expected " + com + " to contain " + ns; 
          };
          if (com.length !== ns.length) {
            return false;
          }
          _.each(ns, function(n) {
            if(!_.contains(com, n)) {
              check = false;
            }
          });
          return check;
        }
      });
    });
    
    describe('setup process', function() {
      
      it('should not throw an error if mandatory information is given', function() {
        expect(function() {
          var s = new ModularityJoiner();
        }).not.toThrow();
      });
      
      it('should be possible to create it as a worker', function() {
        
        var called, created, error;
        
        
        runs(function() {
          error = "";
          created = false;
          called = false;
          var n = [],
            e = [],
            cb = function(d) {
              var data = d.data;
              called = true;
              if (data.cmd === "construct") {
                created = data.result;
                error = data.error;
              }
            },
            w = new WebWorkerWrapper(ModularityJoiner, cb);
        });
        
        waitsFor(function() {
          return called;
        });
        
        runs(function() {
          expect(created).toBeTruthy();
          expect(error).toBeUndefined();
        });
      });
      
    });
    
    describe('setup correctly', function() {
      
      var joiner,
      nodes,
      edges,
      testNetFour;

      beforeEach(function () {
        nodes = [];
        edges = [];
        joiner = new ModularityJoiner();
        testNetFour = function() {
          helper.insertSimpleNodes(nodes, ["0", "1", "2", "3"]);
          edges.push(helper.createSimpleEdge(nodes, 0, 1));
          edges.push(helper.createSimpleEdge(nodes, 0, 3));
          edges.push(helper.createSimpleEdge(nodes, 1, 2));
          edges.push(helper.createSimpleEdge(nodes, 2, 1));
          edges.push(helper.createSimpleEdge(nodes, 2, 3));
          edges.push(helper.createSimpleEdge(nodes, 3, 0));
          edges.push(helper.createSimpleEdge(nodes, 3, 1));
          edges.push(helper.createSimpleEdge(nodes, 3, 2));
          joiner.insertEdge("0", "1");
          joiner.insertEdge("0", "3");
          joiner.insertEdge("1", "2");
          joiner.insertEdge("2", "1");
          joiner.insertEdge("2", "3");
          joiner.insertEdge("3", "0");
          joiner.insertEdge("3", "1");
          joiner.insertEdge("3", "2");
        };
      });

      
      describe('getters', function() {
        
        beforeEach(function() {
          this.addMatchers({
            toBeGetter: function() {
              var func = joiner[this.actual];
              if (!func) {
                this.message = function() {
                  return "Expected " + this.actual + " to be defined.";
                };
                return false;
              }
              if ("function" !== typeof func) {
                this.message = function() {
                  return "Expected " + this.actual + " to be a function.";
                };
                return false;
              }
              if (func.length !== 0) {
                this.message = function() {
                  return "Expected " + this.actual + " to be a getter function.";
                };
                return false;
              }
              return true;
            }
          });
        });
        
        it('should offer the adjacency matrix', function() {
          expect("getAdjacencyMatrix").toBeGetter();
        });
        
        it('should offer the heap', function() {
          expect("getHeap").toBeGetter();
        });
        
        it('should offer the delta qs', function() {
          expect("getDQ").toBeGetter();
        });
        
        it('should offer the degrees', function() {
          expect("getDegrees").toBeGetter();
        });
        
        it('should offer the best join', function() {
          expect("getBest").toBeGetter();
        });
        
        it('should offer the community list', function() {
          expect("getCommunities").toBeGetter();
        });
        
      });
      
      describe('checking the interface', function() {
        
        it('should offer a function to insert an edge', function() {
          expect(joiner.insertEdge).toBeDefined();
          expect(joiner.insertEdge).toEqual(jasmine.any(Function));
          expect(joiner.insertEdge.length).toEqual(2);
        });
        
        it('should offer a function to delete an edge', function() {
          expect(joiner.deleteEdge).toBeDefined();
          expect(joiner.deleteEdge).toEqual(jasmine.any(Function));
          expect(joiner.deleteEdge.length).toEqual(2);
        });
        
        it('should offer a setup function', function() {
          expect(joiner.setup).toBeDefined();
          expect(joiner.setup).toEqual(jasmine.any(Function));
          expect(joiner.setup.length).toEqual(0);
        });
        
        it('should offer a function to join two communities', function() {
          expect(joiner.joinCommunity).toBeDefined();
          expect(joiner.joinCommunity).toEqual(jasmine.any(Function));
          expect(joiner.joinCommunity.length).toEqual(1);
        });
        
        it('should offer a function to identify a far away community', function() {
          expect(joiner.getCommunity).toBeDefined();
          expect(joiner.getCommunity).toEqual(jasmine.any(Function));
          expect(joiner.getCommunity.length).toEqual(2);
        });
        
      });
      
      describe('after setup', function() {
        
        beforeEach(function() {
          testNetFour();
          joiner.setup();
        });
        
        describe('the adjacency matrix', function() {
          
          it('should be created', function() {
            expect(joiner.getAdjacencyMatrix()).toEqual({
              "0": {
                "1": 1,
                "3": 1
              },
              "1": {
                "2": 1
              },
              "2": {
                "1": 1,
                "3": 1
              },
              "3": {
                "0": 1,
                "1": 1,
                "2": 1
              }
            });
          });
          
          it('should react to insert edge', function() {
            joiner.insertEdge("a", "b");
            
            expect(joiner.getAdjacencyMatrix()).toEqual({
              "0": {
                "1": 1,
                "3": 1
              },
              "1": {
                "2": 1
              },
              "2": {
                "1": 1,
                "3": 1
              },
              "3": {
                "0": 1,
                "1": 1,
                "2": 1
              },
              "a": {
                "b": 1
              }
            });
          });
          
          it('should react to delete edge', function() {
            joiner.deleteEdge("0", "1");
            
            expect(joiner.getAdjacencyMatrix()).toEqual({
              "0": {
                "3": 1
              },
              "1": {
                "2": 1
              },
              "2": {
                "1": 1,
                "3": 1
              },
              "3": {
                "0": 1,
                "1": 1,
                "2": 1
              }
            });
          });
          
          it('should remove empty lines on delete', function() {
            joiner.deleteEdge("1", "2");
            
            expect(joiner.getAdjacencyMatrix()).toEqual({
              "0": {
                "1": 1,
                "3": 1
              },
              "2": {
                "1": 1,
                "3": 1
              },
              "3": {
                "0": 1,
                "1": 1,
                "2": 1
              }
            });
          });
          
          it('should only remove one of the edges on delete', function() {
            joiner.insertEdge("1", "2");
            joiner.insertEdge("1", "2");
            
            joiner.deleteEdge("1", "2");
            expect(joiner.getAdjacencyMatrix()).toEqual({
              "0": {
                "1": 1,
                "3": 1
              },
              "1": {
                "2": 2
              },
              "2": {
                "1": 1,
                "3": 1
              },
              "3": {
                "0": 1,
                "1": 1,
                "2": 1
              }
            });
          });
          
        });
        
        describe('the degrees', function() {
        
          var m, one, two, three, initDeg;
            
        
          beforeEach(function() {
            m = edges.length;
            one = 1 / m;
            two = 2 / m;
            three = 3 / m;
            initDeg = {
              "0": {
                _in: one,
                _out: two
              },
              "1": {
                _in: three,
                _out: one
              },
              "2": {
                _in: two,
                _out: two
              },
              "3": {
                _in: two,
                _out: three
              }
            };
          });
        
          it('should initialy be populated', function() {
            expect(joiner.getDegrees()).toEqual(initDeg);
          });
          
          
          it('should be updated after a joining step', function() {
            var toJoin = joiner.getBest(),
              expected = {};
            //Make sure we join the right ones:
            expect(toJoin.sID).toEqual("0");
            expect(toJoin.lID).toEqual("3");
            
            expected["0"] = {
              _in: initDeg["0"]._in + initDeg["3"]._in,
              _out: initDeg["0"]._out + initDeg["3"]._out
            };
            expected["1"] = initDeg["1"];
            expected["2"] = initDeg["2"];
            
            joiner.joinCommunity(toJoin);
            
            expect(joiner.getDegrees()).toEqual(expected);
          });
          
        });
        
        describe('the deltaQ', function() {
        
          var m, zero, one, two, three, initDQ,
            cleanDQ = function(dq) {
              _.each(dq, function(list, s) {
                _.each(list, function(v, t) {
                  if (v < 0) {
                    delete list[t];
                  }
                });
                if (_.isEmpty(list)) {
                  delete dq[s];
                }
              });
            };
        
          beforeEach(function() {          
            m = edges.length;
            zero = {
              _in: 1/m,
              _out: 2/m
            };
            one = {
              _in: 3/m,
              _out: 1/m
            };
            two = {
              _in: 2/m,
              _out: 2/m
            };
            three = {
              _in: 2/m,
              _out: 3/m
            };
            initDQ = {
              "0": {
                "1": 1/m - zero._in * one._out - zero._out * one._in,
                "2": - zero._in * two._out - zero._out * two._in,
                "3": 2/m - zero._in * three._out - zero._out * three._in
              },
              "1": {
                "2": 2/m - one._in * two._out - one._out * two._in,
                "3": 1/m - one._in * three._out - one._out * three._in
              },
              "2": {
                "3": 2/m - two._in * three._out - two._out * three._in
              }
            };
            cleanDQ(initDQ);
          });
          
          it('should initialy be populated', function() {
            expect(joiner.getDQ()).toEqual(initDQ);
          });
          
          it('should be updated after a joining step', function() {
            var toJoin = joiner.getBest(),
              expected = {};
            //Make sure we join the right ones:
            expect(toJoin.sID).toEqual("0");
            expect(toJoin.lID).toEqual("3");
            
            expected["0"] = {};
            if (initDQ["0"]["1"] && initDQ["1"]["3"]) {
              expected["0"]["1"] = initDQ["0"]["1"] + initDQ["1"]["3"];
            }
            if (initDQ["0"]["2"] && initDQ["2"]["3"]) {
              expected["0"]["2"] = initDQ["0"]["2"] + initDQ["2"]["3"];
            }
            expected["1"] = {};
            if (initDQ["1"]["2"]) {
              expected["1"]["2"] = initDQ["1"]["2"];
            }
            cleanDQ(expected);
            joiner.joinCommunity(toJoin);
            expect(joiner.getDQ()).toEqual(expected);
          });
        
          it('should be ordered', function() {
            this.addMatchers({
              toBeOrdered: function() {
                var dQ = this.actual,
                  notFailed = true,
                  msg = "The pointers: ";
                _.each(dQ, function(list, pointer) {
                  _.each(list, function(val, target) {
                    if (target < pointer) {
                      notFailed = false;
                      msg += pointer + " -> " + target + ", ";
                    }
                  });
                });
                this.message = function() {
                  return msg + "are not correct";
                };
                return notFailed;
              }
            });
            
            
            
            var firstID = nodes.length;
            helper.insertSimpleNodes(nodes, ["9", "20", "10", "99", "12"]);
            
            edges.push(helper.createSimpleEdge(nodes, 0, firstID));
            joiner.insertEdge("0", "9");
            edges.push(helper.createSimpleEdge(nodes, firstID, firstID + 1));
            joiner.insertEdge("9", "20");
            edges.push(helper.createSimpleEdge(nodes, firstID + 1, firstID + 2));
            joiner.insertEdge("20", "10");
            edges.push(helper.createSimpleEdge(nodes, firstID + 2, firstID + 3));
            joiner.insertEdge("10", "99");
            edges.push(helper.createSimpleEdge(nodes, firstID + 3, firstID + 4));
            joiner.insertEdge("99", "12");
            edges.push(helper.createSimpleEdge(nodes, firstID + 4, firstID));
            joiner.insertEdge("12", "9");
            edges.push(helper.createSimpleEdge(nodes, firstID + 3, firstID));
            joiner.insertEdge("99", "9");
            
            
            joiner.setup();
            expect(joiner.getDQ()).toBeOrdered();
          });
        
        });
        
        describe('the heap', function() {
        
          var m, zero, one, two, three;
        
          beforeEach(function() {
            m = edges.length;
            zero = {
              _in: 1/m,
              _out: 2/m
            };
            one = {
              _in: 3/m,
              _out: 1/m
            };
            two = {
              _in: 2/m,
              _out: 2/m
            };
            three = {
              _in: 2/m,
              _out: 3/m
            };
          });
          
          it('should initialy by populated', function() {
            expect(joiner.getHeap()).toEqual({
              "0": "3",
              "1": "2",
              "2": "3"
            });
          });
          
          it('should return the largest value', function() {
            expect(joiner.getBest()).toEqual({
              sID: "0",
              lID: "3",
              val: 2/m - zero._in * three._out - zero._out * three._in
            });
          });
          
          it('should be updated after a join step', function() {
            var toJoin = joiner.getBest(),
              expected = {};
            //Make sure we join the right ones:
            expect(toJoin.sID).toEqual("0");
            expect(toJoin.lID).toEqual("3");

            joiner.joinCommunity(toJoin);
            expect(joiner.getHeap()).toEqual({
              "1": "2"
            });
          });
          
          it('should return the largest value after a join step', function() {
            var toJoin = joiner.getBest(),
              expected = {};
            //Make sure we join the right ones:
            expect(toJoin.sID).toEqual("0");
            expect(toJoin.lID).toEqual("3");

            joiner.joinCommunity(toJoin);
            
            expect(joiner.getBest()).toEqual({
              sID: "1",
              lID: "2",
              val: 2/m - one._in * two._out - one._out * two._in
            });
          });
          
          it('should return null for best if the value is 0 or worse', function() {
            var toJoin = joiner.getBest();
            //Make sure we join the right ones:
            expect(toJoin.sID).toEqual("0");
            expect(toJoin.lID).toEqual("3");
            joiner.joinCommunity(toJoin);
            
            toJoin = joiner.getBest();
            //Make sure we join the right ones:
            expect(toJoin.sID).toEqual("1");
            expect(toJoin.lID).toEqual("2");
            joiner.joinCommunity(toJoin);
            expect(joiner.getBest()).toBeNull();
          });
          
        });
        
        describe('communities', function() {
        
          it('should be able to get the communities', function() {
            // No communities yet. Should not return single nodes.
            expect(joiner.getCommunities()).toEqual({});
          });
        
          it('should be able to join two communities', function() {
            var toJoin = joiner.getBest(),
              joinVal = toJoin.val,
              lowId = toJoin.sID,
              highId = toJoin.lID,
              expected = {},
              m = edges.length,
              zero = {
                _in: 1/m,
                _out: 2/m
              },
              one = {
                _in: 3/m,
                _out: 1/m
              },
              two = {
                _in: 2/m,
                _out: 2/m
              },
              three = {
                _in: 2/m,
                _out: 3/m
              };
            expected[lowId] = {
              nodes: [
                lowId,
                highId
              ],
              q: joinVal
            };
            expect(toJoin).toEqual({
              sID: "0",
              lID: "3",
              val: 2/m - zero._in * three._out - zero._out * three._in
            });
            joiner.joinCommunity(toJoin);
            expect(joiner.getCommunities()).toEqual(expected);
          });
        
        });
        
        describe('checking multiple executions', function() {
          
          it('should be able to recompute the joining', function() {
            var best = joiner.getBest(),
              first,
              firstStringified,
              second,
              secondStringified;
            best = joiner.getBest();
            while (best !== null) {
              joiner.joinCommunity(best);
              best = joiner.getBest();
            }
            first = joiner.getCommunities();
            firstStringified = JSON.stringify(first);
            joiner.setup();
            best = joiner.getBest();
            while (best !== null) {
              joiner.joinCommunity(best);
              best = joiner.getBest();
            }
            second = joiner.getCommunities();
            secondStringified = JSON.stringify(second);
            expect(JSON.stringify(first)).toEqual(firstStringified);
            expect(secondStringified).toEqual(firstStringified);
          });
          
        });
        
        describe('checking massively insertion/deletion of edges', function() {
          
          it('should be possible to keep a consistent adj. matrix', function() {

            joiner.deleteEdge("0", "1");
            joiner.deleteEdge("0", "3");
            joiner.deleteEdge("1", "2");
            joiner.deleteEdge("2", "1");
            joiner.deleteEdge("2", "3");
            joiner.deleteEdge("3", "0");
            joiner.deleteEdge("3", "1");
            joiner.deleteEdge("3", "2");
            
            expect(joiner.getAdjacencyMatrix()).toEqual({});
          });
          
        });
        
      });
      
      describe('checking direct community identification', function() {
        
        it('should be able to identify an obvious community', function() {
          helper.insertSimpleNodes(nodes, ["0", "1", "2", "3", "4"]);
          edges.push(helper.createSimpleEdge(nodes, 0, 1));
          edges.push(helper.createSimpleEdge(nodes, 0, 2));
          edges.push(helper.createSimpleEdge(nodes, 0, 3));
          edges.push(helper.createSimpleEdge(nodes, 3, 4));
        
          _.each(edges, function(e) {
            joiner.insertEdge(e.source._id, e.target._id);
          });
        
          var com = joiner.getCommunity(3, nodes[4]._id);
          expect(com).toContainNodes(["0", "1", "2"]);
        });
      
        it('should prefer cliques as a community over an equal sized other group', function() {
          helper.insertSimpleNodes(nodes, ["0", "1", "2", "3", "4", "5", "6", "7", "8"]);
          helper.insertClique(nodes, edges, [0, 1, 2, 3]);
          edges.push(helper.createSimpleEdge(nodes, 4, 3));
          edges.push(helper.createSimpleEdge(nodes, 4, 5));
          edges.push(helper.createSimpleEdge(nodes, 5, 6));
          edges.push(helper.createSimpleEdge(nodes, 5, 7));
          edges.push(helper.createSimpleEdge(nodes, 5, 8));
        
          _.each(edges, function(e) {
            joiner.insertEdge(e.source._id, e.target._id);
          });
        
          var com = joiner.getCommunity(6, nodes[4]._id);
          expect(com).toContainNodes(["0", "1", "2", "3"]);
        });
        
        it('should not return a close group if there is an alternative', function() {
          helper.insertSimpleNodes(nodes, ["0", "1", "2", "3", "4", "5", "6", "7", "8"]);
          helper.insertClique(nodes, edges, [0, 1, 2]);
          helper.insertClique(nodes, edges, [3, 4, 5]);
          helper.insertClique(nodes, edges, [6, 7, 8]);
          edges.push(helper.createSimpleEdge(nodes, 3, 2));
          edges.push(helper.createSimpleEdge(nodes, 5, 6));
        
          _.each(edges, function(e) {
            joiner.insertEdge(e.source._id, e.target._id);
          });
          var com = joiner.getCommunity(6, nodes[3]._id);
          expect(com).toContainNodes(["6", "7", "8"]);
        });
        /*
        it('should also take the best community if no focus is given', function() {
          helper.insertSimpleNodes(nodes, ["0", "1", "2", "3", "4", "5", "6", "7"]);
          helper.insertClique(nodes, edges, [0, 1, 2]);
          edges.push(helper.createSimpleEdge(nodes, 3, 2));
          edges.push(helper.createSimpleEdge(nodes, 3, 4));
          edges.push(helper.createSimpleEdge(nodes, 4, 5));
          edges.push(helper.createSimpleEdge(nodes, 5, 6));
          edges.push(helper.createSimpleEdge(nodes, 5, 7));
        
          _.each(edges, function(e) {
            joiner.insertEdge(e.source._id, e.target._id);
          });
        
          var com = joiner.getCommunity(6);
          expect(com).toContainNodes(["0", "1", "2"]);
        });
        */
      });
      
      describe('checking the zachary karate club', function() {
        
        beforeEach(function() {
          // This is the Zachary Karate Club Network
          helper.insertSimpleNodes(nodes, [
            "0", // Just Temporary node, as the orig is counting from 1 instead of 0
            "1","2","3","4","5","6","7","8","9","10",
            "11","12","13","14","15","16","17","18","19","20",
            "21","22","23","24","25","26","27","28","29","30",
            "31","32","33","34"
          ]);
          edges.push(helper.createSimpleEdge(nodes, 2, 1));
          edges.push(helper.createSimpleEdge(nodes, 3, 1));
          edges.push(helper.createSimpleEdge(nodes, 3, 2));
          edges.push(helper.createSimpleEdge(nodes, 4, 1));
          edges.push(helper.createSimpleEdge(nodes, 4, 2));
          edges.push(helper.createSimpleEdge(nodes, 4, 3));
          edges.push(helper.createSimpleEdge(nodes, 5, 1));
          edges.push(helper.createSimpleEdge(nodes, 6, 1));
          edges.push(helper.createSimpleEdge(nodes, 7, 1));
          edges.push(helper.createSimpleEdge(nodes, 7, 5));
          edges.push(helper.createSimpleEdge(nodes, 7, 6));
          edges.push(helper.createSimpleEdge(nodes, 8, 1));
          edges.push(helper.createSimpleEdge(nodes, 8, 2));
          edges.push(helper.createSimpleEdge(nodes, 8, 3));
          edges.push(helper.createSimpleEdge(nodes, 8, 4));
          edges.push(helper.createSimpleEdge(nodes, 9, 1));
          edges.push(helper.createSimpleEdge(nodes, 9, 3));
          edges.push(helper.createSimpleEdge(nodes, 10, 3));
          edges.push(helper.createSimpleEdge(nodes, 11, 1));
          edges.push(helper.createSimpleEdge(nodes, 11, 5));
          edges.push(helper.createSimpleEdge(nodes, 11, 6));
          edges.push(helper.createSimpleEdge(nodes, 12, 1));
          edges.push(helper.createSimpleEdge(nodes, 13, 1));
          edges.push(helper.createSimpleEdge(nodes, 13, 4));
          edges.push(helper.createSimpleEdge(nodes, 14, 1));
          edges.push(helper.createSimpleEdge(nodes, 14, 2));
          edges.push(helper.createSimpleEdge(nodes, 14, 3));
          edges.push(helper.createSimpleEdge(nodes, 14, 4));
          edges.push(helper.createSimpleEdge(nodes, 17, 6));
          edges.push(helper.createSimpleEdge(nodes, 17, 7));
          edges.push(helper.createSimpleEdge(nodes, 18, 1));
          edges.push(helper.createSimpleEdge(nodes, 18, 2));
          edges.push(helper.createSimpleEdge(nodes, 20, 1));
          edges.push(helper.createSimpleEdge(nodes, 20, 2));
          edges.push(helper.createSimpleEdge(nodes, 22, 1));
          edges.push(helper.createSimpleEdge(nodes, 22, 2));
          edges.push(helper.createSimpleEdge(nodes, 26, 24));
          edges.push(helper.createSimpleEdge(nodes, 26, 25));
          edges.push(helper.createSimpleEdge(nodes, 28, 3));
          edges.push(helper.createSimpleEdge(nodes, 28, 24));
          edges.push(helper.createSimpleEdge(nodes, 28, 25));
          edges.push(helper.createSimpleEdge(nodes, 29, 3));
          edges.push(helper.createSimpleEdge(nodes, 30, 24));
          edges.push(helper.createSimpleEdge(nodes, 30, 27));
          edges.push(helper.createSimpleEdge(nodes, 31, 2));
          edges.push(helper.createSimpleEdge(nodes, 31, 9));
          edges.push(helper.createSimpleEdge(nodes, 32, 1));
          edges.push(helper.createSimpleEdge(nodes, 32, 25));
          edges.push(helper.createSimpleEdge(nodes, 32, 26));
          edges.push(helper.createSimpleEdge(nodes, 32, 29));
          edges.push(helper.createSimpleEdge(nodes, 33, 3));
          edges.push(helper.createSimpleEdge(nodes, 33, 9));
          edges.push(helper.createSimpleEdge(nodes, 33, 15));
          edges.push(helper.createSimpleEdge(nodes, 33, 16));
          edges.push(helper.createSimpleEdge(nodes, 33, 19));
          edges.push(helper.createSimpleEdge(nodes, 33, 21));
          edges.push(helper.createSimpleEdge(nodes, 33, 23));
          edges.push(helper.createSimpleEdge(nodes, 33, 24));
          edges.push(helper.createSimpleEdge(nodes, 33, 30));
          edges.push(helper.createSimpleEdge(nodes, 33, 31));
          edges.push(helper.createSimpleEdge(nodes, 33, 32));
          edges.push(helper.createSimpleEdge(nodes, 34, 9));
          edges.push(helper.createSimpleEdge(nodes, 34, 10));
          edges.push(helper.createSimpleEdge(nodes, 34, 14));
          edges.push(helper.createSimpleEdge(nodes, 34, 15));
          edges.push(helper.createSimpleEdge(nodes, 34, 16));
          edges.push(helper.createSimpleEdge(nodes, 34, 19));
          edges.push(helper.createSimpleEdge(nodes, 34, 20));
          edges.push(helper.createSimpleEdge(nodes, 34, 21));
          edges.push(helper.createSimpleEdge(nodes, 34, 23));
          edges.push(helper.createSimpleEdge(nodes, 34, 24));
          edges.push(helper.createSimpleEdge(nodes, 34, 27));
          edges.push(helper.createSimpleEdge(nodes, 34, 28));
          edges.push(helper.createSimpleEdge(nodes, 34, 29));
          edges.push(helper.createSimpleEdge(nodes, 34, 30));
          edges.push(helper.createSimpleEdge(nodes, 34, 31));
          edges.push(helper.createSimpleEdge(nodes, 34, 32));
          edges.push(helper.createSimpleEdge(nodes, 34, 33));
          nodes.shift(1); //Remove the temporary node;
          
          _.each(edges, function(e) {
            joiner.insertEdge(e.source._id, e.target._id);
          });
          
          joiner.setup();
          
        });
        
        it('should never have duplicates in communities', function() {
          this.addMatchers({
            toNotContainDuplicates: function() {
              var comms = this.actual,
                duplicate = [],
                found = {},
                failed = false;
              _.each(comms, function (v) {
                _.each(v.nodes, function (i) {
                  if (found[i]) {
                    failed = true;
                    duplicate.push(i);
                  } else {
                    found[i] = true;
                  }
                });
              });
              this.message = function() {
                var outComms = comms;
                _.each(outComms, function(o) {
                  o.nodes = _.filter(o.nodes, function(n) {
                    return _.contains(duplicate, n);
                  });
                });
                return "Found duplicate nodes [" + duplicate
                  + "] in communities: " + JSON.stringify(outComms);
              };
              return !failed;
            }
          });
          var best = joiner.getBest();
          while (best !== null) {
            joiner.joinCommunity(best);
            best = joiner.getBest();
            expect(joiner.getCommunities()).toNotContainDuplicates();
          }
        });

        it('should be able to find communities', function() {
          /////////////////////////////////////////////////////
          /// correct acc to: NMCM09                         //
          /// Red:                                           //
          /// 21,23,24,19,27,16,30,33,34,29,15,9,31          //
          /// White:                                         //
          /// 25, 26, 28, 32                                 //
          /// Green:                                         //
          /// 5, 6, 7, 11, 17                                //
          /// Blue:                                          //
          /// 1, 2, 3, 4, 10, 14, 20, 22, 18, 13, 12         //
          /////////////////////////////////////////////////////
          
          this.addMatchers({
            toContainKarateClubCommunities: function() {
              var c1 = [
                  "10", "15", "16", "19", "21", "23", "27",
                  "30", "31", "33", "34", "9"
                ].sort().join(),
                c2 = ["1", "12", "13", "14", "18", "2", "20", "22", "3", "4", "8"].sort().join(),
                c3 = ["11", "17", "5", "6", "7"].sort().join(),
                c4 = ["24", "25", "26", "28", "29", "32"].sort().join(),
                comms = this.actual,
                failed = false,
                msg = "Found incorrect: ";
              _.each(comms, function(o) {
                var check = o.nodes.sort().join();
                switch (check) {
                case c1:
                  c1 = "";
                  break;
                case c2:
                  c2 = "";
                  break;
                case c3:
                  c3 = "";
                  break;
                case c4:
                  c4 = "";
                  break;
                default:
                  msg += "[" + check + "] ";  
                  failed = true;
                }
                return;
              });
              this.message = function() {
                var notFound = "";
                if (c1 !== "") {
                  notFound += "[" + c1 + "] ";
                }
                if (c2 !== "") {
                  notFound += "[" + c2 + "] ";
                }
                if (c3 !== "") {
                  notFound += "[" + c3 + "] ";
                }
                if (c4 !== "") {
                  notFound += "[" + c4 + "] ";
                }
                return msg + " and did not find: " + notFound;
              };
              return !failed;
            }
          });
          var best = joiner.getBest();
          while (best !== null) {
            joiner.joinCommunity(best);
            best = joiner.getBest();
          }
          expect(joiner.getCommunities()).toContainKarateClubCommunities();
        });
        
      });
      
      describe('checking consistency after join', function() {
        
        beforeEach(function() {
          this.addMatchers({
            toBeOrdered: function() {
              var dQ = this.actual,
                notFailed = true,
                msg = "In dQ the pointers: ";
              _.each(dQ, function(list, pointer) {
                _.each(list, function(val, target) {
                  if (target < pointer) {
                    notFailed = false;
                    msg += pointer + " -> " + target + ", ";
                  }
                });
              });
              this.message = function() {
                return msg + "are not correct";
              };
              return notFailed;
            },
            
            toFulfillHeapConstraint: function(joined) {
              var heap = this.actual.getHeap(),
                high = joined.lID;
              this.message = function() {
                return "The heap " + _.keys(heap) + " should not contain " + high;
              };
              return _.isUndefined(heap[high]);
            },
            toFulfillDQConstraint: function(joined) {
              var dQ = this.actual.getDQ(),
                high = joined.lID;
              this.message = function() {
                return "The delta Q " + _.keys(dQ) + " should not contain " + high;
              };
              return _.isUndefined(dQ[high]);
            },
            toBeDefinedInHeapIffInDQ: function(testee) {
              var id = this.actual,
                dQ = testee.getDQ(),
                heap = testee.getHeap();
              if (_.isUndefined(dQ[id])) {
                this.message = id + " is defined on heap but not on dQ";
                return _.isUndefined(heap[id]);
              }
              this.message = id + " is defined on dQ but not on heap";
              return !_.isUndefined(heap[id]);
            },
            toFulfillCommunityInclusionConstraint: function(joined) {
              var comms = this.actual.getCommunities(),
                low = joined.sID,
                high = joined.lID;
              if (_.isUndefined(comms[low])) {
                this.message = function() {
                  return "The lower ID " + low + " is no pointer to a community";
                };
                return false;
              }
              this.message = function() {
                return "The community " + comms[low].nodes + " should contain " + high;
              };
              return _.contains(comms[low].nodes, high);
            },
            
            toFulfillCommunityPointerConstraint: function() {
              var comms = this.actual.getCommunities(),
                notFailed = true,
                msg = "In communities the pointers: ";
              _.each(comms, function(list, pointer) {
                var ns = list.nodes;
                if (ns[0] !== pointer) {
                  notFailed = false;
                  msg += pointer + " -first-> " + ns[0] + ", ";
                }
                _.each(ns, function(id) {
                  if (id < pointer) {
                    notFailed = false;
                    msg += pointer + " -> " + id + ", ";
                  }
                });
              });
              this.message = function () {
                return msg + "are not correct";
              };
              return notFailed;
            },
            
            toNotContainAnyJoinedNode: function() {
              var comms = this.actual.getCommunities(),
                dQ = this.actual.getDQ(),
                forbidden = [],
                msg = "Nodes: ",
                notFailed = true;
              _.each(comms, function(list) {
                var reducedList = list.nodes.slice();
                reducedList.shift(1);
                forbidden = forbidden.concat(reducedList);
              });
              _.each(forbidden, function (id) {
                if (!_.isUndefined(dQ[id])) {
                  notFailed = false;
                  msg += id + ", ";
                }
              });
              this.message = function() {
                return msg + "should not be contained in dQ";
              };
              return notFailed;
                
            },
            
            toBeConsistent: function(joined) {
              var testee = this.actual;
              expect(testee).toFulfillDQConstraint(joined);
              expect(testee).toFulfillHeapConstraint(joined);
              expect(joined.sID).toBeDefinedInHeapIffInDQ(testee);
              expect(testee).toFulfillCommunityInclusionConstraint(joined);
              expect(testee).toNotContainAnyJoinedNode();
              expect(testee).toFulfillCommunityPointerConstraint();
              
              return true;
            },
            
            toContainALowerAndAHigherID: function() {
              var toJoin = this.actual;
              this.message = function() {
                return toJoin.sID + " shold be lower than " + toJoin.lID;
              };
              return toJoin.sID < toJoin.lID;
            }
            
          });
        });
        
        it('for a larger network', function() {
          var i, best,
            step = 0,
            nodeCount = 20;
          helper.insertNSimpleNodes(nodes, nodeCount);
          helper.insertClique(nodes, edges, [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10]);
          for (i = 11; i < nodeCount; i++) {
            edges.push(helper.createSimpleEdge(nodes, i - 1, i));
            edges.push(helper.createSimpleEdge(nodes, i, i - 2));
          }
          joiner.setup();
          best = joiner.getBest();
          while (best !== null) {
            expect(best).toContainALowerAndAHigherID();
            joiner.joinCommunity(best);
            expect(joiner).toBeConsistent(best, step);
            best = joiner.getBest();
            step++;
          }
        });
        
      });
      /*
      describe('checking large networks', function() {
        
        it('should be able to handle 1000 nodes', function() {
          var start = (new Date()).getTime(),
            i, 
            best, 
            nodeCount = 1000,
            diff;  
          helper.insertNSimpleNodes(nodes, nodeCount);
          helper.insertClique(nodes, edges, [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10]);
          for (i = 11; i < nodeCount; i++) {
            edges.push(helper.createSimpleEdge(nodes, i - 1, i));
            edges.push(helper.createSimpleEdge(nodes, i, i - 2));
          }
          _.each(edges, function(e) {
            joiner.insertEdge(e.source._id, e.target._id);
          });
          diff = (new Date()).getTime() - start;
          console.log("Runtime Fill:", diff, "ms");
          start = (new Date()).getTime();
          joiner.setup();
          diff = (new Date()).getTime() - start;
          console.log("Runtime Setup:", diff, "ms");
          start = (new Date()).getTime();
          best = joiner.getBest();
          while (best !== null) {
            joiner.joinCommunity(best);
            best = joiner.getBest();
          }
          diff = (new Date()).getTime() - start;
          console.log("Runtime Compute:", diff, "ms");          
        });
        
        // This is the max. number of nodes for the admin UI
        // However the format is realy unlikely in the adminUI
        
        it('should be able to handle 3000 nodes', function() {
          var start = (new Date()).getTime(),
            i, best, nodeCount = 3000, diff;      
          helper.insertNSimpleNodes(nodes, nodeCount);
          helper.insertClique(nodes, edges, [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10]);
          for (i = 11; i < nodeCount; i++) {
            edges.push(helper.createSimpleEdge(nodes, i - 1, i));
            edges.push(helper.createSimpleEdge(nodes, i, i - 2));
          }
          _.each(edges, function(e) {
            joiner.insertEdge(e.source._id, e.target._id);
          });
          diff = (new Date()).getTime() - start;
          console.log("Runtime Fill:", diff, "ms");
          start = (new Date()).getTime();
          joiner.setup();
          diff = (new Date()).getTime() - start;
          console.log("Runtime Setup:", diff, "ms");
          start = (new Date()).getTime();
          best = joiner.getBest();
          while (best !== null) {
            joiner.joinCommunity(best);
            best = joiner.getBest();
          }
          diff = (new Date()).getTime() - start;
          
          console.log("Runtime Compute:", diff, "ms");          
        });
        
        // This is what we expect in the Admin UI
        it('should be able to handle few large satelites', function() {
          var start = (new Date()).getTime(),
            i,
            best,
            diff,
            s0 = helper.insertSatelite(nodes, edges, 0, 500),
            s1 = helper.insertSatelite(nodes, edges, 1, 300),
            s2 = helper.insertSatelite(nodes, edges, 2, 313),
            s3 = helper.insertSatelite(nodes, edges, 3, 461),
            s4 = helper.insertSatelite(nodes, edges, 4, 251),
            s5 = helper.insertSatelite(nodes, edges, 5, 576),
            s6 = helper.insertSatelite(nodes, edges, 6, 126),
            s7 = helper.insertSatelite(nodes, edges, 7, 231),
            s8 = helper.insertSatelite(nodes, edges, 8, 50),
            s9 = helper.insertSatelite(nodes, edges, 9, 70),
            s10 = helper.insertSatelite(nodes, edges, 10, 111);
          edges.push(helper.createSimpleEdge(nodes, s0, s1));
          edges.push(helper.createSimpleEdge(nodes, s0, s2));
          edges.push(helper.createSimpleEdge(nodes, s0, s3));
          edges.push(helper.createSimpleEdge(nodes, s0, s4));
          edges.push(helper.createSimpleEdge(nodes, s1, s0));
          edges.push(helper.createSimpleEdge(nodes, s1, s5));
          edges.push(helper.createSimpleEdge(nodes, s1, s6));
          edges.push(helper.createSimpleEdge(nodes, s2, s1));
          edges.push(helper.createSimpleEdge(nodes, s3, s7));
          edges.push(helper.createSimpleEdge(nodes, s3, s8));
          edges.push(helper.createSimpleEdge(nodes, s4, s0));
          edges.push(helper.createSimpleEdge(nodes, s4, s8));
          edges.push(helper.createSimpleEdge(nodes, s4, s9));
          edges.push(helper.createSimpleEdge(nodes, s5, s2));
          edges.push(helper.createSimpleEdge(nodes, s5, s6));
          edges.push(helper.createSimpleEdge(nodes, s5, s10));
          edges.push(helper.createSimpleEdge(nodes, s6, s5));
          edges.push(helper.createSimpleEdge(nodes, s7, s5));
          edges.push(helper.createSimpleEdge(nodes, s7, s6));
          edges.push(helper.createSimpleEdge(nodes, s8, s0));
          edges.push(helper.createSimpleEdge(nodes, s8, s2));
          edges.push(helper.createSimpleEdge(nodes, s8, s4));
          edges.push(helper.createSimpleEdge(nodes, s8, s6));
          edges.push(helper.createSimpleEdge(nodes, s9, s8));
          edges.push(helper.createSimpleEdge(nodes, s9, s10));
          edges.push(helper.createSimpleEdge(nodes, s10, s1));
          edges.push(helper.createSimpleEdge(nodes, s10, s9));
          _.each(edges, function(e) {
            joiner.insertEdge(e.source._id, e.target._id);
          });

          diff = (new Date()).getTime() - start;
          console.log("Runtime Fill:", diff, "ms");
          start = (new Date()).getTime();
          joiner.setup();
          diff = (new Date()).getTime() - start;
          console.log("Runtime Setup:", diff, "ms");
          start = (new Date()).getTime();
          best = joiner.getBest();
          while (best !== null) {
            joiner.joinCommunity(best);
            best = joiner.getBest();
          }
          diff = (new Date()).getTime() - start;
          
          console.log("Runtime Compute:", diff, "ms");      
        });
        
        it('should be able to handle many small satelites', function() {
          var start = (new Date()).getTime(),
          i,
          best,
          diff,
          // This test network has been randomly generated.
          // Each Satelite center has 20-100 children
          // And has an edge to 3 - 13 centers

          s0 = helper.insertSatelite(nodes, edges, 0, 22),
          s1 = helper.insertSatelite(nodes, edges, 1, 72),
          s2 = helper.insertSatelite(nodes, edges, 2, 76),
          s3 = helper.insertSatelite(nodes, edges, 3, 68),
          s4 = helper.insertSatelite(nodes, edges, 4, 64),
          s5 = helper.insertSatelite(nodes, edges, 5, 46),
          s6 = helper.insertSatelite(nodes, edges, 6, 24),
          s7 = helper.insertSatelite(nodes, edges, 7, 98),
          s8 = helper.insertSatelite(nodes, edges, 8, 84),
          s9 = helper.insertSatelite(nodes, edges, 9, 93),
          s10 = helper.insertSatelite(nodes, edges, 10, 79),
          s11 = helper.insertSatelite(nodes, edges, 11, 58),
          s12 = helper.insertSatelite(nodes, edges, 12, 98),
          s13 = helper.insertSatelite(nodes, edges, 13, 64),
          s14 = helper.insertSatelite(nodes, edges, 14, 62),
          s15 = helper.insertSatelite(nodes, edges, 15, 56),
          s16 = helper.insertSatelite(nodes, edges, 16, 63),
          s17 = helper.insertSatelite(nodes, edges, 17, 83),
          s18 = helper.insertSatelite(nodes, edges, 18, 98),
          s19 = helper.insertSatelite(nodes, edges, 19, 29),
          s20 = helper.insertSatelite(nodes, edges, 20, 50),
          s21 = helper.insertSatelite(nodes, edges, 21, 62),
          s22 = helper.insertSatelite(nodes, edges, 22, 20),
          s23 = helper.insertSatelite(nodes, edges, 23, 30),
          s24 = helper.insertSatelite(nodes, edges, 24, 50),
          s25 = helper.insertSatelite(nodes, edges, 25, 55),
          s26 = helper.insertSatelite(nodes, edges, 26, 41),
          s27 = helper.insertSatelite(nodes, edges, 27, 60),
          s28 = helper.insertSatelite(nodes, edges, 28, 62),
          s29 = helper.insertSatelite(nodes, edges, 29, 91),
          s30 = helper.insertSatelite(nodes, edges, 30, 59),
          s31 = helper.insertSatelite(nodes, edges, 31, 48),
          s32 = helper.insertSatelite(nodes, edges, 32, 76),
          s33 = helper.insertSatelite(nodes, edges, 33, 88),
          s34 = helper.insertSatelite(nodes, edges, 34, 92),
          s35 = helper.insertSatelite(nodes, edges, 35, 87),
          s36 = helper.insertSatelite(nodes, edges, 36, 65),
          s37 = helper.insertSatelite(nodes, edges, 37, 80),
          s38 = helper.insertSatelite(nodes, edges, 38, 28),
          s39 = helper.insertSatelite(nodes, edges, 39, 33),
          s40 = helper.insertSatelite(nodes, edges, 40, 86),
          s41 = helper.insertSatelite(nodes, edges, 41, 81),
          s42 = helper.insertSatelite(nodes, edges, 42, 26),
          s43 = helper.insertSatelite(nodes, edges, 43, 57),
          s44 = helper.insertSatelite(nodes, edges, 44, 61),
          s45 = helper.insertSatelite(nodes, edges, 45, 47),
          s46 = helper.insertSatelite(nodes, edges, 46, 47),
          s47 = helper.insertSatelite(nodes, edges, 47, 33);
          edges.push(helper.createSimpleEdge(nodes, s0, s44));
          edges.push(helper.createSimpleEdge(nodes, s0, s47));
          edges.push(helper.createSimpleEdge(nodes, s0, s3));
          edges.push(helper.createSimpleEdge(nodes, s0, s39));
          edges.push(helper.createSimpleEdge(nodes, s0, s8));
          edges.push(helper.createSimpleEdge(nodes, s0, s21));
          edges.push(helper.createSimpleEdge(nodes, s1, s36));
          edges.push(helper.createSimpleEdge(nodes, s1, s43));
          edges.push(helper.createSimpleEdge(nodes, s1, s43));
          edges.push(helper.createSimpleEdge(nodes, s1, s40));
          edges.push(helper.createSimpleEdge(nodes, s1, s39));
          edges.push(helper.createSimpleEdge(nodes, s1, s44));
          edges.push(helper.createSimpleEdge(nodes, s2, s32));
          edges.push(helper.createSimpleEdge(nodes, s2, s10));
          edges.push(helper.createSimpleEdge(nodes, s2, s26));
          edges.push(helper.createSimpleEdge(nodes, s2, s39));
          edges.push(helper.createSimpleEdge(nodes, s2, s43));
          edges.push(helper.createSimpleEdge(nodes, s2, s42));
          edges.push(helper.createSimpleEdge(nodes, s2, s16));
          edges.push(helper.createSimpleEdge(nodes, s2, s7));
          edges.push(helper.createSimpleEdge(nodes, s2, s3));
          edges.push(helper.createSimpleEdge(nodes, s3, s8));
          edges.push(helper.createSimpleEdge(nodes, s3, s33));
          edges.push(helper.createSimpleEdge(nodes, s3, s9));
          edges.push(helper.createSimpleEdge(nodes, s3, s18));
          edges.push(helper.createSimpleEdge(nodes, s3, s18));
          edges.push(helper.createSimpleEdge(nodes, s3, s6));
          edges.push(helper.createSimpleEdge(nodes, s3, s1));
          edges.push(helper.createSimpleEdge(nodes, s4, s47));
          edges.push(helper.createSimpleEdge(nodes, s4, s13));
          edges.push(helper.createSimpleEdge(nodes, s4, s9));
          edges.push(helper.createSimpleEdge(nodes, s4, s17));
          edges.push(helper.createSimpleEdge(nodes, s4, s18));
          edges.push(helper.createSimpleEdge(nodes, s4, s44));
          edges.push(helper.createSimpleEdge(nodes, s4, s7));
          edges.push(helper.createSimpleEdge(nodes, s5, s47));
          edges.push(helper.createSimpleEdge(nodes, s5, s27));
          edges.push(helper.createSimpleEdge(nodes, s5, s12));
          edges.push(helper.createSimpleEdge(nodes, s5, s35));
          edges.push(helper.createSimpleEdge(nodes, s5, s33));
          edges.push(helper.createSimpleEdge(nodes, s5, s2));
          edges.push(helper.createSimpleEdge(nodes, s5, s38));
          edges.push(helper.createSimpleEdge(nodes, s5, s17));
          edges.push(helper.createSimpleEdge(nodes, s5, s18));
          edges.push(helper.createSimpleEdge(nodes, s5, s2));
          edges.push(helper.createSimpleEdge(nodes, s5, s27));
          edges.push(helper.createSimpleEdge(nodes, s5, s0));
          edges.push(helper.createSimpleEdge(nodes, s6, s25));
          edges.push(helper.createSimpleEdge(nodes, s6, s32));
          edges.push(helper.createSimpleEdge(nodes, s6, s12));
          edges.push(helper.createSimpleEdge(nodes, s6, s47));
          edges.push(helper.createSimpleEdge(nodes, s6, s3));
          edges.push(helper.createSimpleEdge(nodes, s7, s11));
          edges.push(helper.createSimpleEdge(nodes, s7, s35));
          edges.push(helper.createSimpleEdge(nodes, s7, s28));
          edges.push(helper.createSimpleEdge(nodes, s7, s46));
          edges.push(helper.createSimpleEdge(nodes, s7, s37));
          edges.push(helper.createSimpleEdge(nodes, s7, s15));
          edges.push(helper.createSimpleEdge(nodes, s7, s31));
          edges.push(helper.createSimpleEdge(nodes, s7, s3));
          edges.push(helper.createSimpleEdge(nodes, s8, s41));
          edges.push(helper.createSimpleEdge(nodes, s8, s30));
          edges.push(helper.createSimpleEdge(nodes, s8, s20));
          edges.push(helper.createSimpleEdge(nodes, s8, s2));
          edges.push(helper.createSimpleEdge(nodes, s8, s40));
          edges.push(helper.createSimpleEdge(nodes, s9, s21));
          edges.push(helper.createSimpleEdge(nodes, s9, s40));
          edges.push(helper.createSimpleEdge(nodes, s9, s42));
          edges.push(helper.createSimpleEdge(nodes, s9, s46));
          edges.push(helper.createSimpleEdge(nodes, s9, s23));
          edges.push(helper.createSimpleEdge(nodes, s9, s8));
          edges.push(helper.createSimpleEdge(nodes, s9, s17));
          edges.push(helper.createSimpleEdge(nodes, s10, s43));
          edges.push(helper.createSimpleEdge(nodes, s10, s18));
          edges.push(helper.createSimpleEdge(nodes, s10, s6));
          edges.push(helper.createSimpleEdge(nodes, s11, s18));
          edges.push(helper.createSimpleEdge(nodes, s11, s31));
          edges.push(helper.createSimpleEdge(nodes, s11, s43));
          edges.push(helper.createSimpleEdge(nodes, s11, s28));
          edges.push(helper.createSimpleEdge(nodes, s11, s5));
          edges.push(helper.createSimpleEdge(nodes, s11, s37));
          edges.push(helper.createSimpleEdge(nodes, s12, s14));
          edges.push(helper.createSimpleEdge(nodes, s12, s6));
          edges.push(helper.createSimpleEdge(nodes, s12, s35));
          edges.push(helper.createSimpleEdge(nodes, s12, s6));
          edges.push(helper.createSimpleEdge(nodes, s12, s8));
          edges.push(helper.createSimpleEdge(nodes, s12, s36));
          edges.push(helper.createSimpleEdge(nodes, s13, s28));
          edges.push(helper.createSimpleEdge(nodes, s13, s38));
          edges.push(helper.createSimpleEdge(nodes, s13, s41));
          edges.push(helper.createSimpleEdge(nodes, s14, s32));
          edges.push(helper.createSimpleEdge(nodes, s14, s19));
          edges.push(helper.createSimpleEdge(nodes, s14, s29));
          edges.push(helper.createSimpleEdge(nodes, s14, s21));
          edges.push(helper.createSimpleEdge(nodes, s14, s17));
          edges.push(helper.createSimpleEdge(nodes, s14, s31));
          edges.push(helper.createSimpleEdge(nodes, s15, s0));
          edges.push(helper.createSimpleEdge(nodes, s15, s28));
          edges.push(helper.createSimpleEdge(nodes, s15, s22));
          edges.push(helper.createSimpleEdge(nodes, s15, s25));
          edges.push(helper.createSimpleEdge(nodes, s15, s11));
          edges.push(helper.createSimpleEdge(nodes, s15, s39));
          edges.push(helper.createSimpleEdge(nodes, s16, s37));
          edges.push(helper.createSimpleEdge(nodes, s16, s41));
          edges.push(helper.createSimpleEdge(nodes, s16, s37));
          edges.push(helper.createSimpleEdge(nodes, s16, s22));
          edges.push(helper.createSimpleEdge(nodes, s16, s11));
          edges.push(helper.createSimpleEdge(nodes, s16, s45));
          edges.push(helper.createSimpleEdge(nodes, s16, s38));
          edges.push(helper.createSimpleEdge(nodes, s17, s28));
          edges.push(helper.createSimpleEdge(nodes, s17, s11));
          edges.push(helper.createSimpleEdge(nodes, s17, s10));
          edges.push(helper.createSimpleEdge(nodes, s17, s13));
          edges.push(helper.createSimpleEdge(nodes, s17, s0));
          edges.push(helper.createSimpleEdge(nodes, s17, s11));
          edges.push(helper.createSimpleEdge(nodes, s17, s1));
          edges.push(helper.createSimpleEdge(nodes, s17, s25));
          edges.push(helper.createSimpleEdge(nodes, s18, s39));
          edges.push(helper.createSimpleEdge(nodes, s18, s24));
          edges.push(helper.createSimpleEdge(nodes, s18, s41));
          edges.push(helper.createSimpleEdge(nodes, s18, s14));
          edges.push(helper.createSimpleEdge(nodes, s18, s31));
          edges.push(helper.createSimpleEdge(nodes, s19, s43));
          edges.push(helper.createSimpleEdge(nodes, s19, s44));
          edges.push(helper.createSimpleEdge(nodes, s19, s23));
          edges.push(helper.createSimpleEdge(nodes, s19, s40));
          edges.push(helper.createSimpleEdge(nodes, s19, s0));
          edges.push(helper.createSimpleEdge(nodes, s19, s5));
          edges.push(helper.createSimpleEdge(nodes, s20, s13));
          edges.push(helper.createSimpleEdge(nodes, s20, s34));
          edges.push(helper.createSimpleEdge(nodes, s20, s46));
          edges.push(helper.createSimpleEdge(nodes, s20, s39));
          edges.push(helper.createSimpleEdge(nodes, s20, s14));
          edges.push(helper.createSimpleEdge(nodes, s20, s12));
          edges.push(helper.createSimpleEdge(nodes, s20, s34));
          edges.push(helper.createSimpleEdge(nodes, s20, s37));
          edges.push(helper.createSimpleEdge(nodes, s20, s39));
          edges.push(helper.createSimpleEdge(nodes, s21, s2));
          edges.push(helper.createSimpleEdge(nodes, s21, s10));
          edges.push(helper.createSimpleEdge(nodes, s21, s28));
          edges.push(helper.createSimpleEdge(nodes, s21, s7));
          edges.push(helper.createSimpleEdge(nodes, s21, s44));
          edges.push(helper.createSimpleEdge(nodes, s21, s13));
          edges.push(helper.createSimpleEdge(nodes, s21, s37));
          edges.push(helper.createSimpleEdge(nodes, s22, s12));
          edges.push(helper.createSimpleEdge(nodes, s22, s12));
          edges.push(helper.createSimpleEdge(nodes, s22, s5));
          edges.push(helper.createSimpleEdge(nodes, s22, s8));
          edges.push(helper.createSimpleEdge(nodes, s22, s42));
          edges.push(helper.createSimpleEdge(nodes, s22, s26));
          edges.push(helper.createSimpleEdge(nodes, s22, s29));
          edges.push(helper.createSimpleEdge(nodes, s22, s1));
          edges.push(helper.createSimpleEdge(nodes, s22, s0));
          edges.push(helper.createSimpleEdge(nodes, s22, s19));
          edges.push(helper.createSimpleEdge(nodes, s23, s47));
          edges.push(helper.createSimpleEdge(nodes, s23, s20));
          edges.push(helper.createSimpleEdge(nodes, s23, s13));
          edges.push(helper.createSimpleEdge(nodes, s23, s36));
          edges.push(helper.createSimpleEdge(nodes, s24, s19));
          edges.push(helper.createSimpleEdge(nodes, s24, s10));
          edges.push(helper.createSimpleEdge(nodes, s24, s32));
          edges.push(helper.createSimpleEdge(nodes, s24, s42));
          edges.push(helper.createSimpleEdge(nodes, s24, s11));
          edges.push(helper.createSimpleEdge(nodes, s24, s32));
          edges.push(helper.createSimpleEdge(nodes, s24, s1));
          edges.push(helper.createSimpleEdge(nodes, s24, s29));
          edges.push(helper.createSimpleEdge(nodes, s24, s34));
          edges.push(helper.createSimpleEdge(nodes, s25, s45));
          edges.push(helper.createSimpleEdge(nodes, s25, s34));
          edges.push(helper.createSimpleEdge(nodes, s25, s9));
          edges.push(helper.createSimpleEdge(nodes, s25, s41));
          edges.push(helper.createSimpleEdge(nodes, s26, s37));
          edges.push(helper.createSimpleEdge(nodes, s26, s28));
          edges.push(helper.createSimpleEdge(nodes, s26, s34));
          edges.push(helper.createSimpleEdge(nodes, s26, s43));
          edges.push(helper.createSimpleEdge(nodes, s26, s13));
          edges.push(helper.createSimpleEdge(nodes, s26, s6));
          edges.push(helper.createSimpleEdge(nodes, s27, s47));
          edges.push(helper.createSimpleEdge(nodes, s27, s29));
          edges.push(helper.createSimpleEdge(nodes, s27, s36));
          edges.push(helper.createSimpleEdge(nodes, s27, s36));
          edges.push(helper.createSimpleEdge(nodes, s27, s45));
          edges.push(helper.createSimpleEdge(nodes, s27, s15));
          edges.push(helper.createSimpleEdge(nodes, s28, s30));
          edges.push(helper.createSimpleEdge(nodes, s28, s5));
          edges.push(helper.createSimpleEdge(nodes, s28, s27));
          edges.push(helper.createSimpleEdge(nodes, s28, s33));
          edges.push(helper.createSimpleEdge(nodes, s28, s4));
          edges.push(helper.createSimpleEdge(nodes, s28, s44));
          edges.push(helper.createSimpleEdge(nodes, s28, s24));
          edges.push(helper.createSimpleEdge(nodes, s28, s25));
          edges.push(helper.createSimpleEdge(nodes, s29, s18));
          edges.push(helper.createSimpleEdge(nodes, s29, s3));
          edges.push(helper.createSimpleEdge(nodes, s29, s10));
          edges.push(helper.createSimpleEdge(nodes, s29, s38));
          edges.push(helper.createSimpleEdge(nodes, s29, s5));
          edges.push(helper.createSimpleEdge(nodes, s29, s0));
          edges.push(helper.createSimpleEdge(nodes, s29, s25));
          edges.push(helper.createSimpleEdge(nodes, s29, s46));
          edges.push(helper.createSimpleEdge(nodes, s30, s46));
          edges.push(helper.createSimpleEdge(nodes, s30, s7));
          edges.push(helper.createSimpleEdge(nodes, s30, s2));
          edges.push(helper.createSimpleEdge(nodes, s30, s22));
          edges.push(helper.createSimpleEdge(nodes, s30, s27));
          edges.push(helper.createSimpleEdge(nodes, s30, s34));
          edges.push(helper.createSimpleEdge(nodes, s30, s39));
          edges.push(helper.createSimpleEdge(nodes, s30, s45));
          edges.push(helper.createSimpleEdge(nodes, s31, s33));
          edges.push(helper.createSimpleEdge(nodes, s31, s46));
          edges.push(helper.createSimpleEdge(nodes, s31, s30));
          edges.push(helper.createSimpleEdge(nodes, s31, s5));
          edges.push(helper.createSimpleEdge(nodes, s31, s2));
          edges.push(helper.createSimpleEdge(nodes, s31, s25));
          edges.push(helper.createSimpleEdge(nodes, s31, s18));
          edges.push(helper.createSimpleEdge(nodes, s31, s27));
          edges.push(helper.createSimpleEdge(nodes, s31, s25));
          edges.push(helper.createSimpleEdge(nodes, s32, s30));
          edges.push(helper.createSimpleEdge(nodes, s32, s26));
          edges.push(helper.createSimpleEdge(nodes, s32, s1));
          edges.push(helper.createSimpleEdge(nodes, s32, s21));
          edges.push(helper.createSimpleEdge(nodes, s32, s38));
          edges.push(helper.createSimpleEdge(nodes, s32, s38));
          edges.push(helper.createSimpleEdge(nodes, s32, s23));
          edges.push(helper.createSimpleEdge(nodes, s33, s36));
          edges.push(helper.createSimpleEdge(nodes, s33, s40));
          edges.push(helper.createSimpleEdge(nodes, s33, s6));
          edges.push(helper.createSimpleEdge(nodes, s34, s19));
          edges.push(helper.createSimpleEdge(nodes, s34, s29));
          edges.push(helper.createSimpleEdge(nodes, s34, s2));
          edges.push(helper.createSimpleEdge(nodes, s34, s0));
          edges.push(helper.createSimpleEdge(nodes, s34, s14));
          edges.push(helper.createSimpleEdge(nodes, s34, s0));
          edges.push(helper.createSimpleEdge(nodes, s34, s1));
          edges.push(helper.createSimpleEdge(nodes, s34, s19));
          edges.push(helper.createSimpleEdge(nodes, s34, s42));
          edges.push(helper.createSimpleEdge(nodes, s34, s39));
          edges.push(helper.createSimpleEdge(nodes, s34, s15));
          edges.push(helper.createSimpleEdge(nodes, s34, s17));
          edges.push(helper.createSimpleEdge(nodes, s35, s13));
          edges.push(helper.createSimpleEdge(nodes, s35, s31));
          edges.push(helper.createSimpleEdge(nodes, s35, s30));
          edges.push(helper.createSimpleEdge(nodes, s35, s22));
          edges.push(helper.createSimpleEdge(nodes, s35, s47));
          edges.push(helper.createSimpleEdge(nodes, s35, s25));
          edges.push(helper.createSimpleEdge(nodes, s36, s44));
          edges.push(helper.createSimpleEdge(nodes, s36, s25));
          edges.push(helper.createSimpleEdge(nodes, s36, s19));
          edges.push(helper.createSimpleEdge(nodes, s36, s9));
          edges.push(helper.createSimpleEdge(nodes, s36, s11));
          edges.push(helper.createSimpleEdge(nodes, s36, s11));
          edges.push(helper.createSimpleEdge(nodes, s36, s6));
          edges.push(helper.createSimpleEdge(nodes, s36, s14));
          edges.push(helper.createSimpleEdge(nodes, s36, s28));
          edges.push(helper.createSimpleEdge(nodes, s36, s31));
          edges.push(helper.createSimpleEdge(nodes, s37, s40));
          edges.push(helper.createSimpleEdge(nodes, s37, s4));
          edges.push(helper.createSimpleEdge(nodes, s37, s45));
          edges.push(helper.createSimpleEdge(nodes, s37, s11));
          edges.push(helper.createSimpleEdge(nodes, s37, s39));
          edges.push(helper.createSimpleEdge(nodes, s37, s30));
          edges.push(helper.createSimpleEdge(nodes, s37, s31));
          edges.push(helper.createSimpleEdge(nodes, s37, s9));
          edges.push(helper.createSimpleEdge(nodes, s37, s35));
          edges.push(helper.createSimpleEdge(nodes, s37, s45));
          edges.push(helper.createSimpleEdge(nodes, s37, s7));
          edges.push(helper.createSimpleEdge(nodes, s37, s32));
          edges.push(helper.createSimpleEdge(nodes, s38, s36));
          edges.push(helper.createSimpleEdge(nodes, s38, s45));
          edges.push(helper.createSimpleEdge(nodes, s38, s5));
          edges.push(helper.createSimpleEdge(nodes, s38, s1));
          edges.push(helper.createSimpleEdge(nodes, s39, s37));
          edges.push(helper.createSimpleEdge(nodes, s39, s32));
          edges.push(helper.createSimpleEdge(nodes, s39, s31));
          edges.push(helper.createSimpleEdge(nodes, s39, s13));
          edges.push(helper.createSimpleEdge(nodes, s39, s20));
          edges.push(helper.createSimpleEdge(nodes, s39, s25));
          edges.push(helper.createSimpleEdge(nodes, s39, s7));
          edges.push(helper.createSimpleEdge(nodes, s39, s20));
          edges.push(helper.createSimpleEdge(nodes, s39, s27));
          edges.push(helper.createSimpleEdge(nodes, s39, s5));
          edges.push(helper.createSimpleEdge(nodes, s39, s17));
          edges.push(helper.createSimpleEdge(nodes, s39, s8));
          edges.push(helper.createSimpleEdge(nodes, s40, s8));
          edges.push(helper.createSimpleEdge(nodes, s40, s12));
          edges.push(helper.createSimpleEdge(nodes, s40, s31));
          edges.push(helper.createSimpleEdge(nodes, s40, s39));
          edges.push(helper.createSimpleEdge(nodes, s40, s31));
          edges.push(helper.createSimpleEdge(nodes, s40, s9));
          edges.push(helper.createSimpleEdge(nodes, s41, s45));
          edges.push(helper.createSimpleEdge(nodes, s41, s6));
          edges.push(helper.createSimpleEdge(nodes, s41, s36));
          edges.push(helper.createSimpleEdge(nodes, s41, s12));
          edges.push(helper.createSimpleEdge(nodes, s41, s26));
          edges.push(helper.createSimpleEdge(nodes, s41, s6));
          edges.push(helper.createSimpleEdge(nodes, s41, s21));
          edges.push(helper.createSimpleEdge(nodes, s41, s33));
          edges.push(helper.createSimpleEdge(nodes, s42, s25));
          edges.push(helper.createSimpleEdge(nodes, s42, s28));
          edges.push(helper.createSimpleEdge(nodes, s42, s46));
          edges.push(helper.createSimpleEdge(nodes, s42, s34));
          edges.push(helper.createSimpleEdge(nodes, s42, s41));
          edges.push(helper.createSimpleEdge(nodes, s42, s32));
          edges.push(helper.createSimpleEdge(nodes, s42, s9));
          edges.push(helper.createSimpleEdge(nodes, s43, s12));
          edges.push(helper.createSimpleEdge(nodes, s43, s29));
          edges.push(helper.createSimpleEdge(nodes, s43, s2));
          edges.push(helper.createSimpleEdge(nodes, s43, s14));
          edges.push(helper.createSimpleEdge(nodes, s43, s1));
          edges.push(helper.createSimpleEdge(nodes, s43, s13));
          edges.push(helper.createSimpleEdge(nodes, s43, s28));
          edges.push(helper.createSimpleEdge(nodes, s43, s47));
          edges.push(helper.createSimpleEdge(nodes, s43, s22));
          edges.push(helper.createSimpleEdge(nodes, s43, s2));
          edges.push(helper.createSimpleEdge(nodes, s43, s5));
          edges.push(helper.createSimpleEdge(nodes, s44, s3));
          edges.push(helper.createSimpleEdge(nodes, s44, s1));
          edges.push(helper.createSimpleEdge(nodes, s44, s18));
          edges.push(helper.createSimpleEdge(nodes, s44, s37));
          edges.push(helper.createSimpleEdge(nodes, s44, s0));
          edges.push(helper.createSimpleEdge(nodes, s44, s4));
          edges.push(helper.createSimpleEdge(nodes, s44, s18));
          edges.push(helper.createSimpleEdge(nodes, s44, s7));
          edges.push(helper.createSimpleEdge(nodes, s44, s9));
          edges.push(helper.createSimpleEdge(nodes, s44, s38));
          edges.push(helper.createSimpleEdge(nodes, s44, s15));
          edges.push(helper.createSimpleEdge(nodes, s45, s35));
          edges.push(helper.createSimpleEdge(nodes, s45, s34));
          edges.push(helper.createSimpleEdge(nodes, s45, s5));
          edges.push(helper.createSimpleEdge(nodes, s46, s7));
          edges.push(helper.createSimpleEdge(nodes, s46, s39));
          edges.push(helper.createSimpleEdge(nodes, s46, s21));
          edges.push(helper.createSimpleEdge(nodes, s46, s47));
          edges.push(helper.createSimpleEdge(nodes, s46, s1));
          edges.push(helper.createSimpleEdge(nodes, s46, s19));
          edges.push(helper.createSimpleEdge(nodes, s46, s11));
          edges.push(helper.createSimpleEdge(nodes, s47, s9));
          edges.push(helper.createSimpleEdge(nodes, s47, s10));
          edges.push(helper.createSimpleEdge(nodes, s47, s46));
          edges.push(helper.createSimpleEdge(nodes, s47, s13));
          edges.push(helper.createSimpleEdge(nodes, s47, s21));
          edges.push(helper.createSimpleEdge(nodes, s47, s19));
          edges.push(helper.createSimpleEdge(nodes, s47, s8));
          edges.push(helper.createSimpleEdge(nodes, s47, s39));
          edges.push(helper.createSimpleEdge(nodes, s47, s15));
          edges.push(helper.createSimpleEdge(nodes, s47, s34));
          edges.push(helper.createSimpleEdge(nodes, s47, s30));
          edges.push(helper.createSimpleEdge(nodes, s47, s45));

          _.each(edges, function(e) {
            joiner.insertEdge(e.source._id, e.target._id);
          });

          diff = (new Date()).getTime() - start;
          console.log("Runtime Fill:", diff, "ms");
          start = (new Date()).getTime();
          joiner.setup();
          diff = (new Date()).getTime() - start;
          console.log("Runtime Setup:", diff, "ms");
          start = (new Date()).getTime();
          best = joiner.getBest();
          while (best !== null) {
            joiner.joinCommunity(best);
            best = joiner.getBest();
          }
          diff = (new Date()).getTime() - start;
          
          console.log("Runtime Compute:", diff, "ms");
                
        });
        
      });
      */
    
      /*
      it('should be able to handle 10000 nodes', function() {
        var start = (new Date()).getTime();
        var i, best, nodeCount = 10000;      
        helper.insertNSimpleNodes(nodes, nodeCount);
        helper.insertClique(nodes, edges, [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10]);
        for (i = 11; i < nodeCount; i++) {
          edges.push(helper.createSimpleEdge(nodes, i - 1, i));
          edges.push(helper.createSimpleEdge(nodes, i, i - 2));
        }
        var diff = (new Date()).getTime() - start;
        console.log("Runtime Fill:", diff, "ms");
        start = (new Date()).getTime();
        joiner.setup();
        diff = (new Date()).getTime() - start;
        console.log("Runtime Setup:", diff, "ms");
        start = (new Date()).getTime();
        best = joiner.getBest();
        var step = 0;
        while (best !== null) {
          joiner.joinCommunity(best);
          best = joiner.getBest();
          step++;
        }
        diff = (new Date()).getTime() - start;
        console.log("Runtime Compute:", diff, "ms");          
      });
      */
    });
    
    describe('configured as a worker', function() {
      
      var joiner,
        nodes,
        edges,
        testNetFour,
        called,
        result,
        error;
      
      beforeEach(function () {
        
        runs(function() {
          called = false;
          result = "";
          error = "";
          nodes = [];
          edges = [];
          testNetFour = function() {
            helper.insertSimpleNodes(nodes, ["0", "1", "2", "3"]);
            edges.push(helper.createSimpleEdge(nodes, 0, 1));
            edges.push(helper.createSimpleEdge(nodes, 0, 3));
            edges.push(helper.createSimpleEdge(nodes, 1, 2));
            edges.push(helper.createSimpleEdge(nodes, 2, 1));
            edges.push(helper.createSimpleEdge(nodes, 2, 3));
            edges.push(helper.createSimpleEdge(nodes, 3, 0));
            edges.push(helper.createSimpleEdge(nodes, 3, 1));
            edges.push(helper.createSimpleEdge(nodes, 3, 2));
            _.each(edges, function(e) {
              joiner.call("insertEdge", e.source._id, e.target._id);
            });
          };
          var callback = function(d) {
            if (d.data.cmd === "insertEdge") {
              return;
            }
            called = true;
            if (d.data.cmd !== "construct") {
              result = d.data.result;
              error = d.data.error;
            }
          };
          joiner = new WebWorkerWrapper(ModularityJoiner, callback);
        });
        
        waitsFor(function() {
          return called;
        }, 1000);
        
        runs(function() {
          called = false;
        });
      });
      
      it('should be able to identify an obvious community', function() {
        
        runs(function() {
          helper.insertSimpleNodes(nodes, ["0", "1", "2", "3", "4"]);
          edges.push(helper.createSimpleEdge(nodes, 0, 1));
          edges.push(helper.createSimpleEdge(nodes, 0, 2));
          edges.push(helper.createSimpleEdge(nodes, 0, 3));
          edges.push(helper.createSimpleEdge(nodes, 3, 4));
      
          _.each(edges, function(e) {
            joiner.call("insertEdge", e.source._id, e.target._id);
          });
          joiner.call("getCommunity", 3, nodes[4]._id);
          
        });
        
        waitsFor(function() {
          return called;
        });
        
        runs(function() {
          expect(result).toContainNodes(["0", "1", "2"]);
          expect(error).toBeUndefined();
        });
      });
      
      it('should prefer cliques as a community over an equal sized other group', function() {
        
        runs(function() {
          helper.insertSimpleNodes(nodes, ["0", "1", "2", "3", "4", "5", "6", "7", "8"]);
          helper.insertClique(nodes, edges, [0, 1, 2, 3]);
          edges.push(helper.createSimpleEdge(nodes, 4, 3));
          edges.push(helper.createSimpleEdge(nodes, 4, 5));
          edges.push(helper.createSimpleEdge(nodes, 5, 6));
          edges.push(helper.createSimpleEdge(nodes, 5, 7));
          edges.push(helper.createSimpleEdge(nodes, 5, 8));
      
          _.each(edges, function(e) {
            joiner.call("insertEdge", e.source._id, e.target._id);
          });
          
          joiner.call("getCommunity", 6, nodes[4]._id);
        });
        
        waitsFor(function() {
          return called;
        });
        
        runs(function() {
          expect(result).toContainNodes(["0", "1", "2", "3"]);
          expect(error).toBeUndefined();
        });
        
      });
    
      it('should not return a close group if there is an alternative', function() {
        
        runs(function() {
          helper.insertSimpleNodes(nodes, ["0", "1", "2", "3", "4", "5", "6", "7", "8"]);
          helper.insertClique(nodes, edges, [0, 1, 2]);
          helper.insertClique(nodes, edges, [3, 4, 5]);
          helper.insertClique(nodes, edges, [6, 7, 8]);
          edges.push(helper.createSimpleEdge(nodes, 3, 2));
          edges.push(helper.createSimpleEdge(nodes, 5, 6));
      
          _.each(edges, function(e) {
            joiner.call("insertEdge", e.source._id, e.target._id);
          });
          
          joiner.call("getCommunity", 6, nodes[3]._id);
        });
        
        waitsFor(function() {
          return called;
        });
        
        runs(function() {
          expect(result).toContainNodes(["6", "7", "8"]);
          expect(error).toBeUndefined();
        });
      
      });
      
      it('should also take the best community if no focus is given', function() {
        runs(function() {
          helper.insertSimpleNodes(nodes, ["0", "1", "2", "3", "4", "5", "6", "7"]);
          helper.insertClique(nodes, edges, [0, 1, 2]);
          edges.push(helper.createSimpleEdge(nodes, 3, 2));
          edges.push(helper.createSimpleEdge(nodes, 3, 4));
          edges.push(helper.createSimpleEdge(nodes, 4, 5));
          edges.push(helper.createSimpleEdge(nodes, 5, 6));
          edges.push(helper.createSimpleEdge(nodes, 5, 7));
      
          _.each(edges, function(e) {
            joiner.call("insertEdge", e.source._id, e.target._id);
          });
          
          joiner.call("getCommunity", 6);
        });
      
        waitsFor(function() {
          return called;
        });
        
        runs(function() {
          expect(result).toContainNodes(["0", "1", "2"]);
          expect(error).toBeUndefined();
        });
      });
      
    });
  });
}());