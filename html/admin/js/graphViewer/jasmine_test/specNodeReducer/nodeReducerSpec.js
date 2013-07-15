/*jslint indent: 2, nomen: true, maxlen: 100, white: true  plusplus: true */
/*global beforeEach, afterEach, jasmine */
/*global runs, waits, waitsFor */
/*global describe, it, expect, spyOn */
/*global window, eb, loadFixtures, document */
/*global $, _, d3*/
/*global helper*/
/*global NodeReducer, WebWorkerWrapper*/
/*global console*/

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

    });
    
    describe('setup correctly', function() {
      
      var reducer,
      nodes,
      edges;

      beforeEach(function () {
        nodes = [];
        edges = [];
        reducer = new NodeReducer(nodes, edges);
      });
      
      describe('checking the interface', function() {
        
        it('should offer a function for bucket sort of nodes', function() {
          expect(reducer.bucketNodes).toBeDefined();
          expect(reducer.bucketNodes).toEqual(jasmine.any(Function));
          expect(reducer.bucketNodes.length).toEqual(2);
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
          allNodes.push({
            _data: {a: 1}
          });
          allNodes.push({
            _data: {a: 1}
          });
          allNodes.push({
            _data: {a: 1}
          });
          allNodes.push({
            _data: {a: 1}
          });
          allNodes.push({
            _data: {a: 1}
          });
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
          allNodes.push({
            _data: {a: 1}
          });
          allNodes.push({
            _data: {b: 2}
          });
          allNodes.push({
            _data: {c: 3}
          });
          allNodes.push({
            _data: {d: 4}
          });
          allNodes.push({
            _data: {e: 5}
          });
          allNodes.push({
            _data: {f: 6}
          });
          
          var res = reducer.bucketNodes(allNodes, buckets);
          expect(res.length).toEqual(3);
        });
        
        it('should uniformly distribute dissimilar nodes', function() {
          buckets = 3;
          allNodes.push({
            _data:{a: 1}
          });
          allNodes.push({
            _data:{b: 2}
          });
          allNodes.push({
            _data:{c: 3}
          });
          allNodes.push({
            _data:{d: 4}
          });
          allNodes.push({
            _data:{e: 5}
          });
          allNodes.push({
            _data:{f: 6}
          });
          allNodes.push({
            _data:{g: 7}
          });
          allNodes.push({
            _data:{h: 8}
          });
          allNodes.push({
            _data:{i: 9}
          });
          
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
            
          a1 = {
            _data: {a: 1}
          };
          a2 = {
            _data: {a: 1}
          };
          a3 = {
            _data: {a: 1}
          };
          
          b1 = {
            _data: {b: 2}
          };
          b2 = {
            _data: {b: 2}
          };
          b3 = {
            _data: {b: 2}
          };
          
          c1 = {
            _data: {c: 3}
          };
          c2 = {
            _data: {c: 3}
          };
          c3 = {
            _data: {c: 3}
          };
          
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
          
          if (res1[0]._data.a !== undefined) {
            expect(res1).toContainAll([a1, a2, a3]);
          } else if (res2[0]._data.a !== undefined) {
            expect(res2).toContainAll([a1, a2, a3]);
          } else {
            expect(res3).toContainAll([a1, a2, a3]);
          }
          
          if (res1[0]._data.b !== undefined) {
            expect(res1).toContainAll([b1, b2, b3]);
          } else if (res2[0]._data.b !== undefined) {
            expect(res2).toContainAll([b1, b2, b3]);
          } else {
            expect(res3).toContainAll([b1, b2, b3]);
          }
          
          if (res1[0]._data.c !== undefined) {
            expect(res1).toContainAll([c1, c2, c3]);
          } else if (res2[0]._data.c !== undefined) {
            expect(res2).toContainAll([c1, c2, c3]);
          } else {
            expect(res3).toContainAll([c1, c2, c3]);
          }
        });
        
      });
      
    });
    
    describe('setup with a priority list', function() {
      
      var reducer,
      nodes,
      buckets,
      prios;

      beforeEach(function () {
        nodes = [];
        prios = ["age", "type"];
        reducer = new NodeReducer([], [], prios);
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
      
      it('should bucket nodes according to the list', function() {
        buckets = 3;
        var a1, a2 ,a3,
          b1, b2, b3,
          c1, c2, c3,
          resArray,
          res1,
          res2,
          res3;
          
        a1 = {
          _data: {
            age: 1,
            name: "Alice",
            foo: "bar"
          }
        };
        a2 = {
          _data: {
            age: 1,
            name: "Bob",
            foo: "bar"
          }
        };
        a3 = {
          _data: {
            age: 1,
            name: "Charly",
            foo: "bar"
          }
        };
        
        b1 = {
          _data: {
            age: 2,
            name: "Alice",
            foo: "bar"
          }
        };
        b2 = {
          _data: {
            age: 2,
            name: "Bob",
            foo: "bar"
          }
        };
        b3 = {
          _data: {
            age: 2,
            name: "Charly",
            foo: "bar"
          }
        };
        
        c1 = {
          _data: {
            age: 3,
            name: "Alice",
            foo: "bar"
          }
        };
        c2 = {
          _data: {
            age: 3,
            name: "Bob",
            foo: "bar"
          }
        };
        c3 = {
          _data: {
            age: 3,
            name: "Charly",
            foo: "bar"
          }
        };
        
        nodes.push(a1);
        nodes.push(b1);
        nodes.push(c1);
        
        nodes.push(a2);
        nodes.push(b2);
        nodes.push(c2);
        
        nodes.push(a3);
        nodes.push(b3);
        nodes.push(c3);
        
        resArray = reducer.bucketNodes(nodes, buckets);
        res1 = resArray[0];
        res2 = resArray[1];
        res3 = resArray[2];
        
        if (res1[0]._data.age === 1) {
          expect(res1).toContainAll([a1, a2, a3]);
        } else if (res2[0]._data.age === 1) {
          expect(res2).toContainAll([a1, a2, a3]);
        } else {
          expect(res3).toContainAll([a1, a2, a3]);
        }
        
        if (res1[0]._data.age === 2) {
          expect(res1).toContainAll([b1, b2, b3]);
        } else if (res2[0]._data.age === 2) {
          expect(res2).toContainAll([b1, b2, b3]);
        } else {
          expect(res3).toContainAll([b1, b2, b3]);
        }
        
        if (res1[0]._data.age === 3) {
          expect(res1).toContainAll([c1, c2, c3]);
        } else if (res2[0]._data.age === 3) {
          expect(res2).toContainAll([c1, c2, c3]);
        } else {
          expect(res3).toContainAll([c1, c2, c3]);
        }
        
      });
      
      
      it('should bucket following the priorities of the list', function() {
        buckets = 3;
        var a1, a2 ,a3,
          b1, b2, b3,
          c1, c2, c3,
          resArray,
          res1,
          res2,
          res3;
          
        a1 = {
          _data: {
            age: 1,
            name: "Alice",
            foo: "bar"
          }
        };
        a2 = {
          _data: {
            age: 1,
            name: "Bob",
            foo: "bar"
          }
        };
        a3 = {
          _data: {
            age: 1,
            name: "Charly",
            foo: "bar"
          }
        };
        
        b1 = {
          _data: {
            type: "person",
            name: "Alice",
            foo: "bar"
          }
        };
        b2 = {
          _data: {
            type: "person",
            name: "Bob",
            foo: "bar"
          }
        };
        b3 = {
          _data: {
            type: "person",
            name: "Charly",
            foo: "bar"
          }
        };
        
        c1 = {
          _data: {
            age: 1,
            type: "person",
            name: "Alice",
            foo: "bar"
          }
        };
        c2 = {
          _data: {
            age: 3,
            type: "person",
            name: "Bob",
            foo: "bar"
          }
        };
        c3 = {
          _data: {
            age: 3,
            name: "Charly",
            foo: "bar"
          }
        };
        
        nodes.push(a1);
        nodes.push(b1);
        nodes.push(c1);
        
        nodes.push(a2);
        nodes.push(b2);
        nodes.push(c2);
        
        nodes.push(a3);
        nodes.push(b3);
        nodes.push(c3);
        
        resArray = reducer.bucketNodes(nodes, buckets);
        res1 = resArray[0];
        res2 = resArray[1];
        res3 = resArray[2];
        
        if (res1[0]._data.age === 1) {
          expect(res1).toContainAll([a1, a2, a3, c1]);
        } else if (res2[0]._data.age === 1) {
          expect(res2).toContainAll([a1, a2, a3, c1]);
        } else {
          expect(res3).toContainAll([a1, a2, a3, c1]);
        }
        
        if (res1[0]._data.age === undefined
          && res1[0]._data.type
          && res1[0]._data.type === "person") {
          expect(res1).toContainAll([b1, b2, b3]);
        } else if (res2[0]._data.age === undefined
          && res2[0]._data.type
          && res2[0]._data.type === "person") {
          expect(res2).toContainAll([b1, b2, b3]);
        } else {
          expect(res3).toContainAll([b1, b2, b3]);
        }
        
        if (res1[0]._data.age === 3) {
          expect(res1).toContainAll([c2, c3]);
        } else if (res2[0]._data.age === 3) {
          expect(res2).toContainAll([c2, c3]);
        } else {
          expect(res3).toContainAll([c2, c3]);
        }
        
      });
      
    });
    
  });
  
}());