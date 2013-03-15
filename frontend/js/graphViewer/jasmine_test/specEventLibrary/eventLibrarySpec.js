/*jslint indent: 2, nomen: true, maxlen: 100, white: true  plusplus: true */
/*global beforeEach, afterEach */
/*global describe, it, expect */
/*global runs, spyOn, waitsFor */
/*global window, eb, loadFixtures, document */
/*global EventLibrary*/

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
/// @author Copyright 2011-2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////


(function () {
  "use strict";

  describe('Event Library', function () {

    var eventLib;
    
    beforeEach(function() {
      eventLib = new EventLibrary();
    });

    describe('Expand', function() {
      
      var loaded,
      reshaped,
      nodes,
      edges,
      loadedNodes,
      reshapedNodes,
      started,
      loadNodeCallback = function(node) {
        loaded++;
        loadedNodes.push(node);
      },
      reshapeNodeCallback = function(node) {
        reshaped++;
        reshapedNodes.push(node);
      },
      startCallback = function() {
        started++;
      },
      testee;
      
      beforeEach(function() {
        loaded = 0;
        reshaped = 0;
        started = 0;
      
        nodes = [];
        edges = [];
        loadedNodes = [];
        reshapedNodes = [];
      });
      
      it('should expand a collapsed node', function() {
        var node = {
          id: 0,
          _outboundCounter: 0,
          _inboundCounter: 0
        };
        nodes.push(node);
        testee = eventLib.Expand(
          edges,
          nodes,
          startCallback,
          loadNodeCallback,
          reshapeNodeCallback
        );
        testee(node);
        
        expect(node._expanded).toBeTruthy();
        expect(started).toEqual(1);
        expect(reshaped).toEqual(1);
        expect(loaded).toEqual(1);
        expect(reshapedNodes.length).toEqual(1);
        expect(reshapedNodes[0]).toEqual(node);
        expect(loadedNodes.length).toEqual(1);
        expect(loadedNodes[0]).toEqual(node.id);
        
      });
      
      it('should collapse an expanded node', function() {
        var node = {
          id: 0,
          _expanded: true,
          _outboundCounter: 0,
          _inboundCounter: 0
        };
        nodes.push(node);
        testee = eventLib.Expand(
          edges,
          nodes,
          startCallback,
          loadNodeCallback,
          reshapeNodeCallback
        );
        testee(node);
        
        expect(node._expanded).toBeFalsy();
        expect(started).toEqual(1);
        expect(reshaped).toEqual(1);
        expect(loaded).toEqual(0);
        expect(reshapedNodes.length).toEqual(1);
        expect(reshapedNodes[0]).toEqual(node);
      });
      
      it('should collapse a tree', function() {
        var root = {
          id: 0,
          _expanded: true,
          _outboundCounter: 2,
          _inboundCounter: 0
        },
        c1 = {
          id: 1,
          _outboundCounter: 0,
          _inboundCounter: 1
        },
        c2 = {
          id: 2,
          _outboundCounter: 0,
          _inboundCounter: 1
        };
        
        nodes.push(root);
        nodes.push(c1);
        nodes.push(c2);
        edges.push({source: root, target: c1});
        edges.push({source: root, target: c2});
        
        testee = eventLib.Expand(
          edges,
          nodes,
          startCallback,
          loadNodeCallback,
          reshapeNodeCallback
        );
        testee(root);
        
        expect(root._expanded).toBeFalsy();
        expect(started).toEqual(1);
        expect(reshaped).toEqual(1);
        expect(loaded).toEqual(0);
        expect(nodes.length).toEqual(1);
        expect(edges.length).toEqual(0);
        expect(reshapedNodes.length).toEqual(1);
        expect(reshapedNodes[0]).toEqual(root);
      });
      
    });
      
  });

}());