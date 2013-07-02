/*jslint indent: 2, nomen: true, maxlen: 100, white: true  plusplus: true */
/*global beforeEach, afterEach, jasmine */
/*global runs, waits */
/*global describe, it, expect, spyOn */
/*global window, eb, loadFixtures, document */
/*global $, _, d3*/
/*global helper*/
/*global NodeReducer*/

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

  describe('Node Reducer', function () {
    
    describe('setup process', function() {
      
      it('should throw an error if no nodes are given', function() {
        expect(function() {
          var s = new NodeReducer();
        }).toThrow("Nodes have to be given.");
      });
      
      it('should throw an error if no edges are given', function() {
        expect(function() {
          var s = new NodeReducer([]);
        }).toThrow("Edges have to be given.");
      });
      
      it('should not throw an error if mandatory information is given', function() {
        expect(function() {
          var s = new NodeReducer([], []);
        }).not.toThrow();
      });
      
      it('should create an instance of the modularityJoiner', function() {
        spyOn(window, "ModularityJoiner");
        var n = [],
          e = [],
          s = new NodeReducer(n, e);
        expect(window.ModularityJoiner).wasCalledWith(n, e);
      });
    });
    
    describe('setup correctly', function() {
      
      var reducer,
      nodes,
      edges;

      beforeEach(function () {
        nodes = [];
        edges = [];
        reducer = new NodeReducer(nodes, edges);
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
      
      describe('checking the interface', function() {
        
        it('should offer a function to identify a far away community', function() {
          expect(reducer.getCommunity).toBeDefined();
          expect(reducer.getCommunity).toEqual(jasmine.any(Function));
          expect(reducer.getCommunity.length).toEqual(2);
        });
        
        it('should offer a function for bucket sort of nodes', function() {
          expect(reducer.bucketNodes).toBeDefined();
          expect(reducer.bucketNodes).toEqual(jasmine.any(Function));
          expect(reducer.bucketNodes.length).toEqual(2);
        });
        
      });
      
      describe('checking community identification', function() {
        
        it('should be able to identify an obvious community', function() {
          helper.insertSimpleNodes(nodes, ["0", "1", "2", "3", "4"]);
          edges.push(helper.createSimpleEdge(nodes, 0, 1));
          edges.push(helper.createSimpleEdge(nodes, 0, 2));
          edges.push(helper.createSimpleEdge(nodes, 0, 3));
          edges.push(helper.createSimpleEdge(nodes, 3, 4));
          
          var com = reducer.getCommunity(3, nodes[4]);
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
          
          var com = reducer.getCommunity(6, nodes[4]);
          expect(com).toContainNodes(["0", "1", "2", "3"]);
        });
        
        it('should not return a close group if there is an alternative', function() {
          helper.insertSimpleNodes(nodes, ["0", "1", "2", "3", "4", "5", "6", "7"]);
          helper.insertClique(nodes, edges, [0, 1, 2]);
          edges.push(helper.createSimpleEdge(nodes, 3, 2));
          edges.push(helper.createSimpleEdge(nodes, 3, 4));
          edges.push(helper.createSimpleEdge(nodes, 4, 5));
          edges.push(helper.createSimpleEdge(nodes, 5, 6));
          edges.push(helper.createSimpleEdge(nodes, 5, 7));
          
          var com = reducer.getCommunity(6, nodes[3]);
          expect(com).toContainNodes(["5", "6", "7"]);
        });
        
        it('should also take the best community if no focus is given', function() {
          helper.insertSimpleNodes(nodes, ["0", "1", "2", "3", "4", "5", "6", "7"]);
          helper.insertClique(nodes, edges, [0, 1, 2]);
          edges.push(helper.createSimpleEdge(nodes, 3, 2));
          edges.push(helper.createSimpleEdge(nodes, 3, 4));
          edges.push(helper.createSimpleEdge(nodes, 4, 5));
          edges.push(helper.createSimpleEdge(nodes, 5, 6));
          edges.push(helper.createSimpleEdge(nodes, 5, 7));
          
          var com = reducer.getCommunity(6);
          expect(com).toContainNodes(["0", "1", "2"]);
        });
        
        it('should wait for a running identification to finish before allowing to start a new one', function() {
          
          var firstRun, secondRun;
          
          runs(function() { 
            var i;           
            helper.insertNSimpleNodes(nodes, 1000);
            helper.insertClique(nodes, edges, [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10]);
            for (i = 11; i < 1000; i++) {
              edges.push(helper.createSimpleEdge(nodes, i - 1, i));
            }
            
            
            setTimeout(function() {
              console.log("Start1");
              firstRun = reducer.getCommunity(6);
              console.log("End1");
            }, 10);
            
            setTimeout(function() {
              console.log("Start2");
              expect(function() {
                secondRun = reducer.getCommunity(6);
              }).toThrow("Still running.");
              console.log("End2");
            }, 10);
            
          });
          
          waits(100);
          
          runs(function() {
            expect(firstRun).toContainNodes(["0", "1", "2"]);
            expect(secondRun).toBeUndefined();
          });
          
        });
        
      });
      
      describe('checking bucket sort of nodes', function() {
        var allNodes, buckets;
        
        beforeEach(function() {
          allNodes = [];
          
          this.addMatchers({
            toContainAll: function(objs) {
              var bucket = this.actual,
                passed = true;
              _.each(bucket, function(n) {
                var i;
                for (i = 0; i < objs.length; i++) {
                  if (objs[i] === n) {
                    return;
                  }
                }
                passed = false;
              });
              this.message = function() {
                return JSON.stringify(bucket)
                  + " should contain all of "
                  + JSON.stringify(objs);
              };
              return passed;
            }
          });
        });
        
        it('should not bucket anything if #nodes <= #buckets', function() {
          buckets = 5;
          allNodes.push({a: 1});
          allNodes.push({a: 1});
          allNodes.push({a: 1});
          allNodes.push({a: 1});
          allNodes.push({a: 1});
          var res = reducer.bucketNodes(allNodes, buckets);
          expect(res.length).toEqual(5);
          expect(res[0].length).toEqual(1);
          expect(res[1].length).toEqual(1);
          expect(res[2].length).toEqual(1);
          expect(res[3].length).toEqual(1);
          expect(res[4].length).toEqual(1);
        });
        
        it('should create at most the given amount of buckets', function() {
          buckets = 3;
          allNodes.push({a: 1});
          allNodes.push({b: 2});
          allNodes.push({c: 3});
          allNodes.push({d: 4});
          allNodes.push({e: 5});
          allNodes.push({f: 6});
          
          var res = reducer.bucketNodes(allNodes, buckets);
          expect(res.length).toEqual(3);
        });
        
        it('should uniformly distribute dissimilar nodes', function() {
          buckets = 3;
          allNodes.push({a: 1});
          allNodes.push({b: 2});
          allNodes.push({c: 3});
          allNodes.push({d: 4});
          allNodes.push({e: 5});
          allNodes.push({f: 6});
          allNodes.push({g: 7});
          allNodes.push({h: 8});
          allNodes.push({i: 9});
          
          var res = reducer.bucketNodes(allNodes, buckets);
          expect(res[0].length).toEqual(3);
          expect(res[1].length).toEqual(3);
          expect(res[2].length).toEqual(3);
        });
        
        it('should bucket clearly similar nodes together', function() {
          buckets = 3;
          var a1, a2 ,a3,
            b1, b2, b3,
            c1, c2, c3,
            resArray,
            res1,
            res2,
            res3;
            
          a1 = {a: 1};
          a2 = {a: 1};
          a3 = {a: 1};
          
          b1 = {b: 2};
          b2 = {b: 2};
          b3 = {b: 2};
          
          c1 = {c: 3};
          c2 = {c: 3};
          c3 = {c: 3};
          
          allNodes.push(a1);
          allNodes.push(a2);
          allNodes.push(a3);
          allNodes.push(b1);
          allNodes.push(b2);
          allNodes.push(b3);
          allNodes.push(c1);
          allNodes.push(c2);
          allNodes.push(c3);
          
          resArray = reducer.bucketNodes(allNodes, buckets);
          res1 = resArray[0];
          res2 = resArray[1];
          res3 = resArray[2];
          
          if (res1[0].a !== undefined) {
            expect(res1).toContainAll([a1, a2, a3]);
          } else if (res2[0].a !== undefined) {
            expect(res2).toContainAll([a1, a2, a3]);
          } else {
            expect(res3).toContainAll([a1, a2, a3]);
          }
          
          if (res1[0].b !== undefined) {
            expect(res1).toContainAll([b1, b2, b3]);
          } else if (res2[0].b !== undefined) {
            expect(res2).toContainAll([b1, b2, b3]);
          } else {
            expect(res3).toContainAll([b1, b2, b3]);
          }
          
          if (res1[0].c !== undefined) {
            expect(res1).toContainAll([c1, c2, c3]);
          } else if (res2[0].c !== undefined) {
            expect(res2).toContainAll([c1, c2, c3]);
          } else {
            expect(res3).toContainAll([c1, c2, c3]);
          }
        });
        
      });
      
    });
  });
}());