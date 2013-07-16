/*jslint indent: 2, nomen: true, maxlen: 100, white: true  plusplus: true */
/*global it, expect, describe, beforeEach*/
/*global spyOn, jasmine, window*/


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


var describeInterface = function (testee) {
  "use strict";
    
  describe('checking the interface', function() {
    it('should comply to the Adapter Interface', function() {
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
      
      // Add functions to load here:
      expect(testee).toHaveFunction("loadNode", 2);
      expect(testee).toHaveFunction("loadInitialNode", 2);
      expect(testee).toHaveFunction("explore", 2);
//      expect(testee).toHaveFunction("loadNodeFromTreeById", 2);
      expect(testee).toHaveFunction("requestCentralityChildren", 2);
//      expect(testee).toHaveFunction("loadNodeFromTreeByAttributeValue", 3);
      expect(testee).toHaveFunction("createEdge", 2);
      expect(testee).toHaveFunction("deleteEdge", 2);
      expect(testee).toHaveFunction("patchEdge", 3);
      expect(testee).toHaveFunction("createNode", 2);
      expect(testee).toHaveFunction("deleteNode", 2);
      expect(testee).toHaveFunction("patchNode", 3);
      expect(testee).toHaveFunction("setNodeLimit", 2);
      expect(testee).toHaveFunction("setChildLimit", 1);
      expect(testee).toHaveFunction("expandCommunity", 2);
    });
  });
    
    
};

/**
* Expectations on constructor:
* Created with config: {prioList: ["foo", "bar", "baz"]}
* loadNode -> Adds {_id: 1} -{_id:"1-2"}-> {_id: 2}  
* createEdge({source: {_id: 1}, target: {_id: 1}}) -> {_id: "1-2", _from:1, _to:2}
* createNode({}) -> {_id: 1}
*
*/

var describeIntegeration = function(constructor) {
  "use strict";
  
  describe('checking integeration of the abstract adapter', function() {
    
    var mockedAbstract, testee;
    
    beforeEach(function() {
      mockedAbstract = {
        edges: [],
        nodes: [],
        setWidth: function(){},
        setHeight: function(){},
        insertNode: function(data){
          var node = {
            _data: data,
            _id: data._id
          }, i;
          for (i = 0; i < this.nodes.length; i++) {
            if (this.nodes[i]._id === node._id) {
              return this.nodes[i];
            }
          }
          this.nodes.push(node);
          return node;
        },
        insertEdge: function(){},
        removeNode: function(){},
        removeEdge: function(){},
        removeEdgesForNode: function(){},
        expandCommunity: function(){},
        setNodeLimit: function(){},
        setChildLimit: function(){},
        checkSizeOfInserted: function(){},
        checkNodeLimit: function(){},
        explore: function(){},
        changeTo: function(){},
        getPrioList: function(){}
      };
      
      
      spyOn(window, "AbstractAdapter").andCallFake(function(nodes, edges, descendant, config) {
        mockedAbstract.nodes = nodes;
        mockedAbstract.edges = edges;
        return mockedAbstract;
      });
    });
    
    it('should create the AbstractAdapter with correct values', function() {
      testee = constructor();
      expect(window.AbstractAdapter).wasCalledWith(
        [],
        [],
        testee,
        {prioList: ["foo", "bar", "baz"]}
      );
    });
    
    it('should call getPrioList on the abstract', function() {
      spyOn(mockedAbstract, "getPrioList");
      testee = constructor();
      testee.getPrioList();
      expect(mockedAbstract.getPrioList).wasCalled();
    });
    
    it('should call setNodeLimit on the abstract', function() {
      spyOn(mockedAbstract, "setNodeLimit");
      testee = constructor();
      testee.setNodeLimit(5, function(){});
      expect(mockedAbstract.setNodeLimit).wasCalledWith(5, jasmine.any(Function));
    });
    
    it('should propagate changeTo to the abstract', function() {
      spyOn(mockedAbstract, "changeTo");
      testee = constructor();
      testee.changeTo({prioList: ["foo", "bar", "baz"]});
      expect(mockedAbstract.changeTo).wasCalledWith({prioList: ["foo", "bar", "baz"]});
    });
    
    it('should call explore on the abstract', function() {
      spyOn(mockedAbstract, "explore");
      testee = constructor();
      var node = {
        _id: "1"
      };
      testee.explore(node, function(){});
      expect(mockedAbstract.explore).wasCalledWith(node, jasmine.any(Function));
    });
    
    it('should call setChildLimit on the abstract', function() {
      spyOn(mockedAbstract, "setChildLimit");
      testee = constructor();
      testee.setChildLimit(5);
      expect(mockedAbstract.setChildLimit).wasCalledWith(5);
    });
    
    it('should call expandCommunity on the abstract', function() {
      spyOn(mockedAbstract, "expandCommunity");
      var comm = {};
      testee = constructor();
      testee.expandCommunity(comm);
      expect(mockedAbstract.expandCommunity).wasCalledWith(comm);
    });
    
    it('should use the abstract to insert objects from loadNode', function() {
      spyOn(mockedAbstract, "insertNode").andCallThrough();
      spyOn(mockedAbstract, "insertEdge").andCallThrough();
      spyOn(mockedAbstract, "checkSizeOfInserted");
      spyOn(mockedAbstract, "checkNodeLimit");
      testee = constructor();
      testee.loadNode(1);
      expect(mockedAbstract.insertNode).wasCalledWith({_id: 1});
      expect(mockedAbstract.insertNode).wasCalledWith({_id: 2});
      expect(mockedAbstract.insertEdge).wasCalledWith({_id: "1-2", _from: 1, _to: 2});
      expect(mockedAbstract.checkSizeOfInserted).wasCalledWith({2: mockedAbstract.nodes[1]});
      expect(mockedAbstract.checkNodeLimit).wasCalledWith(mockedAbstract.nodes[0]);
    });
    
    it('should insert an edge on createEdge', function() {
      spyOn(mockedAbstract, "insertEdge");
      testee = constructor();
      testee.createEdge({source: {_id: 1}, target: {_id: 2}}, function(){});
      expect(mockedAbstract.insertEdge).wasCalledWith({_id: "1-2", _from: 1, _to: 2});
    });
    
    it('should remove an edge on deleteEdge', function() {
      spyOn(mockedAbstract, "removeEdge");
      testee = constructor();
      var edge = {_id: "1-2", _from: 1, _to: 2};
      testee.deleteEdge(edge);
      expect(mockedAbstract.removeEdge).wasCalledWith(edge);
    });
    
    it('should insert a node on createNode', function() {
      spyOn(mockedAbstract, "insertNode");
      testee = constructor();
      testee.createNode({}, function(){});
      expect(mockedAbstract.insertNode).wasCalledWith({_id: 1});
    });
    
    it('should remove a node on deleteNode', function() {
      spyOn(mockedAbstract, "removeNode");
      spyOn(mockedAbstract, "removeEdgesForNode");
      testee = constructor();
      var node = {_id: 1};
      testee.deleteNode(node);
      expect(mockedAbstract.removeEdgesForNode).wasCalledWith(node);
      expect(mockedAbstract.removeNode).wasCalledWith(node);
    });
        
  });
  
};
