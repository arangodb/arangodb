/*jslint indent: 2, nomen: true, maxlen: 100, white: true  plusplus: true */
/*global beforeEach, afterEach */
/*global describe, it, expect */
/*global helper*/
/*global CommunityNode*/

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

  describe('Community Node', function() {
    
    var nodes;
    
    beforeEach(function () {
      nodes = [];
      helper.insertNSimpleNodes(nodes, 30);
    });
    
    describe('checking the interface', function() {
      
      var testee;
      
      beforeEach(function() {
        testee = new CommunityNode(nodes.slice(3, 13));
        this.addMatchers({
          toHaveFunction: function(func, argCounter) {
            var obj = this.actual;
            this.message = function(){
              return testee.constructor.name
                + " should react to function "
                + func
                + " with at least "
                + argCounter
                + " arguments.";
            };
            if (typeof(obj[func]) !== "function") {
              return false;
            }
            if (obj[func].length < argCounter) {
              return false;
            }
            return true;
          }
        });
      });
      
      it('should offer a function to getAll included nodes', function() {
        expect(testee).toHaveFunction("getNodes", 0);
      });
      
      it('should offer a function to check if a node is included', function() {
        expect(testee).toHaveFunction("hasNode", 1);
      });
      
      it('should offer a function to get a node if it is included', function() {
        expect(testee).toHaveFunction("getNode", 1);
      });
      
      it('should offer a function to insert an additional node', function() {
        expect(testee).toHaveFunction("insertNode", 1);
      });
      
      it('should offer a function to insert an additional edge', function() {
        expect(testee).toHaveFunction("insertEdge", 1);
      });
      
      it('should offer a function to dissolve the community', function() {
        expect(testee).toHaveFunction("dissolve", 0);
      });
    });
    
    it('should create a communityNode containing the given nodes', function() {
      var c = new CommunityNode(nodes.slice(3, 13));
      expect(c.getNodes()).toEqual(nodes.slice(3, 13));
    });
    
    it('should be able to insert a new node', function() {
      var c = new CommunityNode(nodes.slice(3, 13)),
        n = {
        _id: "foxx",
        _inboundCounter: 0,
        _outboundCounter: 0,
        position: {
          x: 1,
          y: 1,
          z: 1
        }
      };
      c.insertNode(n);
      expect(c.getNodes()).toEqual(nodes.slice(3, 13).concat([n]));
    });
    
    it('should be able to check if a node is included', function() {
      var n = {
        _id: "foxx",
        _inboundCounter: 0,
        _outboundCounter: 0,
        position: {
          x: 1,
          y: 1,
          z: 1
        }
      },
      c = new CommunityNode([n]);
      
      expect(c.hasNode("foxx")).toBeTruthy();
      expect(c.hasNode("1")).toBeFalsy();
    });
    
    it('should only acknowledge included nodes', function() {
      var c = new CommunityNode(nodes.slice(3, 13)),
        i;
      for (i = 0; i < nodes.length; i++) {
        if (2 < i && i < 13) {
          expect(c.hasNode(nodes[i]._id)).toBeTruthy();
        } else {
          expect(c.hasNode(nodes[i]._id)).toBeFalsy();
        }
      }
    });
    
    it('should return nodes by id', function() {
      var n = {
        _id: "foxx",
        _inboundCounter: 0,
        _outboundCounter: 0,
        position: {
          x: 1,
          y: 1,
          z: 1
        }
      },
      c = new CommunityNode([n]);
      expect(c.getNode("foxx")).toEqual(n);
    });
    
    it('should return undefined if node is not contained', function() {
      var n = {
        _id: "foxx",
        _inboundCounter: 0,
        _outboundCounter: 0,
        position: {
          x: 1,
          y: 1,
          z: 1
        }
      },
      c = new CommunityNode([n]);
      expect(c.getNode("nofoxx")).toBeUndefined();
    });
    
    it('should initially contain the required attributes for shaping', function() {
      var x = 42,
        y = 23,
        n = {
          _id: "foxx",
          _inboundCounter: 0,
          _outboundCounter: 0,
          position: {
            x: x,
            y: y,
            z: 1
          }
        },
        initNodes = [n].concat(nodes.slice(3, 13)),
        c = new CommunityNode(initNodes);
      expect(c.x).toBeDefined();
      expect(c.x).toEqual(x);
      expect(c.y).toBeDefined();
      expect(c.y).toEqual(y);
      expect(c._size).toEqual(11);
      expect(c._id).toMatch(/^\*community_\d{1,7}$/);
    });
    
    it('should be able to resolve the community', function() {
      var c = new CommunityNode(nodes.slice(3, 13)),
        e1 = {
          _id: "3-4",
          _from: "3",
          _to: "4"
        },
        e2 = {
          _id: "5-7",
          _from: "5",
          _to: "7"
        };
        c.insertEdge(e1);
        c.insertEdge(e2);
      expect(c.dissolve()).toEqual({
        nodes: nodes.slice(3, 13),
        edges: [e1, e2]
      });
    });
  });

}());
