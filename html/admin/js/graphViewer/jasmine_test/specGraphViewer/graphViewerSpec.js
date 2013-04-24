/*jslint indent: 2, nomen: true, maxlen: 100, white: true  plusplus: true */
/*global beforeEach, afterEach */
/*global describe, it, expect, jasmine */
/*global waitsFor, runs, waits */
/*global window, eb, loadFixtures, document */
/*global $, _, d3*/
/*global helper*/
/*global GraphViewer*/

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


describe("Graph Viewer", function() {
  "use strict";
  var viewer,
  waittime = 200,
  svg,
  docSVG;
  
  beforeEach(function() {
    docSVG = document.createElement("svg");
    document.body.appendChild(docSVG);
    svg = d3.select("svg");
  });
  
  
  afterEach(function() {
    document.body.removeChild(docSVG);
  });
  
  describe('set up process', function() {
    
    var adapterConfig;
    
    beforeEach(function() {
      adapterConfig = {type: "json", path: "../test_data/"};
    });
    
    it('should throw an error if the svg is not given or incorrect', function() {
      expect(
        function() {
          var t = new GraphViewer();
        }
      ).toThrow("SVG has to be given and has to be selected using d3.select");
      expect(
        function() {
          var t = new GraphViewer(docSVG);
        }
      ).toThrow("SVG has to be given and has to be selected using d3.select");
    });
    
    it('should throw an error if the width is not given or incorrect', function() {
      expect(
        function() {
          var t = new GraphViewer(svg);
        }
      ).toThrow("A width greater 0 has to be given");
      expect(
        function() {
          var t = new GraphViewer(svg, -10);
        }
      ).toThrow("A width greater 0 has to be given");
    });
    
    it('should throw an error if the height is not given or incorrect', function() {
      expect(
        function() {
          var t = new GraphViewer(svg, 10);
        }
      ).toThrow("A height greater 0 has to be given");
      expect(
        function() {
          var t = new GraphViewer(svg, 10, -10);
        }
      ).toThrow("A height greater 0 has to be given");
    });
    
    it('should throw an error if the adapterConfig is not given', function() {
      expect(
        function() {
          var t = new GraphViewer(svg, 10, 10);
        }
      ).toThrow("An adapter configuration has to be given");
    });
    
    it('should not throw an error if everything is given', function() {
      expect(
        function() {
          var t = new GraphViewer(svg, 10, 10, adapterConfig);
        }
      ).not.toThrow();
    });
    
    describe('GraphViewer set up correctly', function() {
      
      var viewer;
      
      beforeEach(function() {
        viewer = new GraphViewer(svg, 10, 10, adapterConfig);
      });
      
      it('should offer the nodeShaper', function() {
        expect(viewer.nodeShaper).toBeDefined();
      });
      
      it('should offer the edgeShaper', function() {
        expect(viewer.edgeShaper).toBeDefined();
      });
      
      it('should offer the startFunction', function() {
        expect(viewer.start).toBeDefined();
      });
      
      it('should offer the adapter', function() {
        expect(viewer.adapter).toBeDefined();
      });
      
      it('should offer the layouter', function() {
        expect(viewer.layouter).toBeDefined();
      });
      
      it('should offer the complete config for the event dispatcher', function() {
        expect(viewer.dispatcherConfig).toBeDefined();
        expect(viewer.dispatcherConfig).toEqual({
          expand: {
            edges: [],
            nodes: [],
            startCallback: jasmine.any(Function),
            loadNode: jasmine.any(Function),
            reshapeNodes: jasmine.any(Function)
          },
          drag: {
            layouter: jasmine.any(Object)
          },
          nodeEditor: {
            nodes: [],
            adapter: jasmine.any(Object)
          },
          edgeEditor: {
            edges: [],
            adapter: jasmine.any(Object)
          }
        });
        expect(viewer.dispatcherConfig.expand).toEqual({
          edges: [],
          nodes: [],
          startCallback: jasmine.any(Function),
          loadNode: jasmine.any(Function),
          reshapeNodes: jasmine.any(Function)
        });
        expect(viewer.dispatcherConfig.drag).toEqual({
          layouter: jasmine.any(Object)
        });
        expect(viewer.dispatcherConfig.nodeEditor).toEqual({
          nodes: [],
          adapter: jasmine.any(Object)
        });
        expect(viewer.dispatcherConfig.edgeEditor).toEqual({
          edges: [],
          adapter: jasmine.any(Object)
        });
        
        
      });
      
      it('should offer to load a new graph', function() {
        expect(viewer.loadGraph).toBeDefined();
      });
      
      it('should offer to load a new graph by attribute value', function() {
        expect(viewer.loadGraphWithAttributeValue).toBeDefined();
      });
      
      
      it("should be able to load a root node", function() {
        runs (function() {
          this.addMatchers({
            toBeDisplayed: function() {
              var nodes = this.actual,
              nonDisplayed = [];
              this.message = function(){
                var msg = "Nodes: [";
                _.each(nonDisplayed, function(n) {
                  msg += n + " ";
                });
                msg += "] are not displayed.";
                return msg;
              };
              _.each(nodes, function(n) {
                if ($("svg #" + n)[0] === undefined) {
                  nonDisplayed.push(n);
                }
              });
              return nonDisplayed.length === 0;
            }
          });
          
          viewer.loadGraph(0);
        });
    
        // Give it a second to load
        // Unfortunately there is no handle to check for changes
        waits(waittime);
    
        runs (function() {
          expect([0, 1, 2, 3, 4]).toBeDisplayed();
        });
      });
      
    });
    
  });
  

});