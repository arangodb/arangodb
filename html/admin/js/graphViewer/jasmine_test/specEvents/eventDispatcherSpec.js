/*jslint indent: 2, nomen: true, maxlen: 100, white: true  plusplus: true */
/*global beforeEach, afterEach */
/*global describe, it, expect */
/*global runs, spyOn, waitsFor */
/*global window, document, $, d3 */
/*global EventDispatcher, NodeShaper, EdgeShaper*/

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

  describe('Event Dispatcher', function () {

    var dispatcher,
    nodeShaper,
    edgeShaper,
    svg;
    
    beforeEach(function() {
      svg = document.createElement("svg");
      document.body.appendChild(svg);
      
      nodeShaper = new NodeShaper(d3.select("svg"));
      edgeShaper = new EdgeShaper(d3.select("svg"));
      dispatcher = new EventDispatcher(nodeShaper, edgeShaper);
    });
    
    afterEach(function() {
      document.body.removeChild(svg);
    });
    
    describe('set up process', function() {
      it('should throw an error if nodeShaper is not given', function() {
        expect(
          function() {
            var t = new EventDispatcher();
          }
        ).toThrow("NodeShaper has to be given.");
      });
      
      it('should throw an error if edgeShaper is not given', function() {
        expect(
          function() {
            var t = new EventDispatcher(nodeShaper);
          }
        ).toThrow("EdgeShaper has to be given.");
      });
      
    });
    
    describe('checking objects to bind events to', function() {
      
      
      it('should be able to bind to any DOM-element', function() {
        var called = false,
        callback = function() {
          called = true;
        };
        
        runs(function() {
          var target = $("svg");
          dispatcher.bind(target, "click", callback);
          target.click();
        });
        
        waitsFor(function() {
          return called;
        }, 1000, "The click event should have been triggered.");
        
        runs(function() {
          // Just display that everything had worked
          expect(true).toBeTruthy();
        });
        
      });
      
      it('should be able to bind to all nodes', function() {
        var called;
        
        
        runs(function() {
          var nodes = [{_id: 1}, {_id:2}],
          callback = function() {
            called++;
          };
          called = 0;
          nodeShaper.drawNodes(nodes);
          dispatcher.bind("nodes", "click", callback);
          $("#1").click();
          $("#2").click();
        });
        
        waitsFor(function() {
          return called === 2;
        }, 1000, "The two click events should have been triggered.");
        
        runs(function() {
          // Just display that everything had worked
          expect(true).toBeTruthy();
        });
        
      });
      
      it('should be able to bind to all edges', function() {
        var called;
        
        
        runs(function() {
          var n1 = {_id: 1},
          n2 = {_id: 2},
          n3 = {_id: 3},
          edges = [
            {source: n1, target: n2},
            {source: n2, target: n3}
          ],
          callback = function() {
            called++;
          };
          called = 0;
          edgeShaper.drawEdges(edges);
          dispatcher.bind("edges", "click", callback);
          $("#1-2").click();
          $("#2-3").click();
        });
        
        waitsFor(function() {
          return called === 2;
        }, 1000, "The two click events should have been triggered.");
        
        runs(function() {
          // Just display that everything had worked
          expect(true).toBeTruthy();
        });
        
      });
      
    });
    
    describe('checking triggers for events', function() {
      
    });

    describe('checking different events', function() {
      
    });
  });

}());
