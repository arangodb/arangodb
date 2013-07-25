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
        
        it('should offer a function to change the prioList', function() {
          expect(reducer.changePrioList).toBeDefined();
          expect(reducer.changePrioList).toEqual(jasmine.any(Function));
          expect(reducer.changePrioList.length).toEqual(1);
        });
        
        it('should offer a function to get the current prioList', function() {
          expect(reducer.getPrioList).toBeDefined();
          expect(reducer.getPrioList).toEqual(jasmine.any(Function));
          expect(reducer.getPrioList.length).toEqual(0);
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
          _.each(res, function(obj) {
            expect(obj.reason).toEqual({
              type: "single",
              text: "One Node"
            });
            expect(obj.nodes.length).toEqual(1);
          });
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
          _.each(res, function(obj) {
            expect(obj.reason).toEqual({
              type: "similar",
              text: "Similar Nodes",
              example: jasmine.any(Object)
            });
            expect(obj.nodes.length).toEqual(3);
          });
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
          _.each(resArray, function(entry) {
            expect(entry.reason).toEqual({
              type: "similar",
              text: "Similar Nodes",
              example: jasmine.any(Object)
            });
            if (_.isEqual(entry.reason.example, a1)) {
              res1 = entry;
            } else if (_.isEqual(entry.reason.example, b1)) {
              res2 = entry;
            } else if (_.isEqual(entry.reason.example, c1)) {
              res3 = entry;
            } else {
              // Should never be reached
              expect(true).toBeFalsy();
            }
          });
          expect(res1.nodes).toContainAll([a1, a2, a3]);
          expect(res2.nodes).toContainAll([b1, b2, b3]);
          expect(res3.nodes).toContainAll([c1, c2, c3]);
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
      
      it('should be able to get the list', function() {
        expect(reducer.getPrioList()).toEqual(prios);
      });
      
      it('should bucket nodes according to the list', function() {
        buckets = 3;
        var a1, a2 ,a3,
          b1, b2, b3,
          c1, c2, c3,
          r1, r2, r3,
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
        r1 = {
          key: "age",
          value: "1",
          text: "age: 1"
        };
        r2 = {
          key: "age",
          value: "2",
          text: "age: 2"
        };
        r3 = {
          key: "age",
          value: "3",
          text: "age: 3"
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
        _.each(resArray, function(entry) {
          if (_.isEqual(entry.reason, r1)) {
            res1 = entry;
          } else if (_.isEqual(entry.reason, r2)) {
            res2 = entry;
          } else if (_.isEqual(entry.reason, r3)) {
            res3 = entry;
          } else {
            expect(true).toBeFalsy();
          }
        });
        expect(res1.nodes).toContainAll([a1, a2, a3]);
        expect(res2.nodes).toContainAll([b1, b2, b3]);
        expect(res3.nodes).toContainAll([c1, c2, c3]);
      });
       
      it('should bucket following the priorities of the list', function() {
        buckets = 3;
        var a1, a2 ,a3,
          b1, b2, b3,
          c1, c2, c3,
          r1, r2, r3,
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
        r1 = {
          key: "age",
          value: "1",
          text: "age: 1"
        };
        r2 = {
          key: "type",
          value: "person",
          text: "type: person"
        };
        r3 = {
          key: "age",
          value: "3",
          text: "age: 3"
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
        _.each(resArray, function(entry) {
          if (_.isEqual(entry.reason, r1)) {
            res1 = entry;
          } else if (_.isEqual(entry.reason, r2)) {
            res2 = entry;
          } else if (_.isEqual(entry.reason, r3)) {
            res3 = entry;
          } else {
            expect(true).toBeFalsy();
          }
        });
        expect(res1.nodes).toContainAll([a1, a2, a3, c1]);
        expect(res2.nodes).toContainAll([b1, b2, b3]);
        expect(res3.nodes).toContainAll([c2, c3]);
        
      });
      
      it('should be possible to delete the list', function() {
        buckets = 3;
        var a1, a2 ,a3,
          b1, b2, b3,
          c1, c2, c3,
          r1, r2, r3,
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
            foo: "baz"
          }
        };
        a3 = {
          _data: {
            age: 1,
            name: "Charly",
            foo: "tango"
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
            foo: "baz"
          }
        };
        b3 = {
          _data: {
            age: 2,
            name: "Charly",
            foo: "tango"
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
            foo: "baz"
          }
        };
        c3 = {
          _data: {
            age: 3,
            name: "Charly",
            foo: "tango"
          }
        };
        r1 = {
          type: "similar",
          example: a1,
          text: "Similar Nodes"
        };
        r2 = {
          type: "similar",
          example: a2,
          text: "Similar Nodes"
        };
        r3 = {
          type: "similar",
          example: a3,
          text: "Similar Nodes"
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
        
        reducer.changePrioList([]);
        
        resArray = reducer.bucketNodes(nodes, buckets);
        _.each(resArray, function(entry) {
          if (_.isEqual(entry.reason, r1)) {
            res1 = entry;
          } else if (_.isEqual(entry.reason, r2)) {
            res2 = entry;
          } else if (_.isEqual(entry.reason, r3)) {
            res3 = entry;
          } else {
            expect(true).toBeFalsy();
          }
        });
        expect(res1.nodes).toContainAll([a1, b1, c1]);
        expect(res2.nodes).toContainAll([a2, b2, c2]);
        expect(res3.nodes).toContainAll([a3, b3, c3]);
      });
      
      it('should be possible to change the list', function() {
        buckets = 3;
        var a1, a2 ,a3,
          b1, b2, b3,
          c1, c2, c3,
          r1, r2, r3,
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
            foo: "baz"
          }
        };
        a3 = {
          _data: {
            age: 1,
            name: "Charly",
            foo: "tango"
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
            foo: "baz"
          }
        };
        b3 = {
          _data: {
            age: 2,
            name: "Charly",
            foo: "tango"
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
            foo: "baz"
          }
        };
        c3 = {
          _data: {
            age: 3,
            name: "Charly",
            foo: "tango"
          }
        };
        r1 = {
          key: "foo",
          value: "bar",
          text: "foo: bar"
        };
        r2 = {
          key: "foo",
          value: "baz",
          text: "foo: baz"
        };
        r3 = {
          key: "foo",
          value: "tango",
          text: "foo: tango"
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
        
        reducer.changePrioList(["foo"]);
        
        resArray = reducer.bucketNodes(nodes, buckets);
        _.each(resArray, function(entry) {
          if (_.isEqual(entry.reason, r1)) {
            res1 = entry;
          } else if (_.isEqual(entry.reason, r2)) {
            res2 = entry;
          } else if (_.isEqual(entry.reason, r3)) {
            res3 = entry;
          } else {
            expect(true).toBeFalsy();
          }
        });
        expect(res1.nodes).toContainAll([a1, b1, c1]);
        expect(res2.nodes).toContainAll([a2, b2, c2]);
        expect(res3.nodes).toContainAll([a3, b3, c3]);
      });
      
    });
    
  });
  
}());