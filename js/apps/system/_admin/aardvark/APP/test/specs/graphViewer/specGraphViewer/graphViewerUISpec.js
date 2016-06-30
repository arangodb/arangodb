/* jshint unused: false */
/* global beforeEach, afterEach */
/* global describe, it, expect, jasmine*/
/* global runs, waitsFor, spyOn, waits */
/* global window, eb, loadFixtures, document */
/* global $, _, d3*/
/* global helper, mocks, JSONAdapter:true, uiMatchers*/
/* global GraphViewerUI, NodeShaper, EdgeShaper*/

// //////////////////////////////////////////////////////////////////////////////
// / @brief Graph functionality
// /
// / @file
// /
// / DISCLAIMER
// /
// / Copyright 2010-2012 triagens GmbH, Cologne, Germany
// /
// / Licensed under the Apache License, Version 2.0 (the "License")
// / you may not use this file except in compliance with the License.
// / You may obtain a copy of the License at
// /
// /     http://www.apache.org/licenses/LICENSE-2.0
// /
// / Unless required by applicable law or agreed to in writing, software
// / distributed under the License is distributed on an "AS IS" BASIS,
// / WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// / See the License for the specific language governing permissions and
// / limitations under the License.
// /
// / Copyright holder is triAGENS GmbH, Cologne, Germany
// /
// / @author Michael Hackstein
// / @author Copyright 2011-2013, triAGENS GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

(function () {
  'use strict';

  describe('Graph Viewer UI', function () {
    var div,
      ui,
      adapterConfig,
      adapterMockCall;

    beforeEach(function () {
      // Mock for jsonAdapter
      window.communicationMock(spyOn);
      var Tmp = JSONAdapter;
      JSONAdapter = function (jsonPath, nodes, edges, width, height) {
        var r = new Tmp(jsonPath, nodes, edges, width, height);
        r.loadInitialNodeByAttributeValue = function (attribute, value, callback) {
          adapterMockCall = {
            attribute: attribute,
            value: value
          };
        };
        r.getCollections = function (callback) {
          callback(['nodes'], ['edges']);
        };
        r.getGraphs = function (callback) {
          callback(['graph']);
        };
        r.getAttributeExamples = function (callback) {
          callback(['name', 'type']);
        };
        r.getPrioList = function () {
          return [];
        };
        return r;
      };
      // Mock for ZoomManager
      if (window.ZoomManager === undefined) {
        window.ZoomManager = {
          getMinimalZoomFactor: function () {
            return 0.16;
          }
        };
      }
      spyOn(window, 'ZoomManager').andCallThrough();
      div = document.createElement('div');
      div.id = 'contentDiv';
      div.style.width = '200px';
      div.style.height = '200px';
      adapterConfig = {type: 'json', path: '../test_data/'};
      document.body.appendChild(div);
      ui = new GraphViewerUI(div, adapterConfig);
      uiMatchers.define(this);
    });

    afterEach(function () {
      document.body.removeChild(div);
    });

    it('should throw an error if no container element is given', function () {
      expect(
        function () {
          var t = new GraphViewerUI();
        }
      ).toThrow('A parent element has to be given.');
    });

    it('should throw an error if the container element has no id', function () {
      expect(
        function () {
          var t = new GraphViewerUI(document.createElement('div'));
        }
      ).toThrow('The parent element needs an unique id.');
    });

    it('should throw an error if the adapter config is not given', function () {
      expect(
        function () {
          var t = new GraphViewerUI(div);
        }
      ).toThrow('An adapter configuration has to be given');
    });

    it('should append a svg to the given parent', function () {
      expect($('#contentDiv svg').length).toEqual(1);
    });

    it('should automatically start the ZoomManager', function () {
      expect(window.ZoomManager).toHaveBeenCalledWith(
        119,
        200,
        jasmine.any(Object),
        jasmine.any(Object),
        jasmine.any(NodeShaper),
        jasmine.any(EdgeShaper),
        {},
        jasmine.any(Function)
      );
    });

    describe('checking the toolbox', function () {
      var toolboxSelector = '#contentDiv #toolbox';

      it('should append the toolbox', function () {
        expect($(toolboxSelector).length).toEqual(1);
      });

      it('should contain the objects from eventDispatcher', function () {
        expect($(toolboxSelector + ' #control_event_drag').length).toEqual(1);
        expect($(toolboxSelector + ' #control_event_edit').length).toEqual(1);
        expect($(toolboxSelector + ' #control_event_expand').length).toEqual(1);
        expect($(toolboxSelector + ' #control_event_delete').length).toEqual(1);
        expect($(toolboxSelector + ' #control_event_connect').length).toEqual(1);
      });

      it('should have the correct layout', function () {
        expect($(toolboxSelector)[0]).toConformToToolboxLayout();
      });
    });

    describe('checking the menubar', function () {
      it('should append the menubar', function () {
        expect($('#contentDiv #menubar').length).toEqual(1);
      });

      it('should contain a field to load a node by attribute', function () {
        var barSelector = '#contentDiv #menubar',
          attrfield = $(barSelector + ' #attribute')[0],
          valfield = $(barSelector + ' #value')[0],
          btn = $(barSelector + ' #loadnode')[0];
        expect($(barSelector + ' #attribute').length).toEqual(1);
        expect($(barSelector + ' #value').length).toEqual(1);
        expect($(barSelector + ' #loadnode').length).toEqual(1);
        expect(attrfield).toBeTag('input');
        expect(attrfield.type).toEqual('text');
        expect(attrfield.placeholder).toEqual('Attribute name');
        expect(valfield).toBeTag('input');
        expect(valfield.type).toEqual('text');
        expect(valfield).toBeOfClass('searchInput');
        expect(valfield.placeholder).toEqual('Attribute value');
        expect(btn).toBeTag('img');
        expect(btn.className).toEqual('gv-search-submit-icon');
      });

      it('should contain a position for the layout buttons', function () {
        var buttonBar = $('#contentDiv #menubar #modifiers');
        expect(buttonBar.length).toEqual(1);
        expect(buttonBar).toBeOfClass('headerButtonBar');
      });

      it('should contain a general configure menu', function () {
        var menuSelector = '#contentDiv #menubar #configuredropdown',
          dropDownSelector = '#configureDropdown .dropdownInner ul';
        expect($(menuSelector).length).toEqual(1);
        expect($('span', menuSelector).attr('title')).toEqual('Configure');
        expect($('span', menuSelector)).toBeOfClass('icon_arangodb_settings2');
        expect($(dropDownSelector + ' #control_adapter_graph').length).toEqual(1);
        expect($(dropDownSelector + ' #control_node_labelandcolourlist').length).toEqual(1);
        expect($(dropDownSelector + ' #control_adapter_priority').length).toEqual(1);
      });

      it('should contain a filter menu', function () {
        var menuSelector = '#contentDiv #menubar #filterdropdown',
          dropDownSelector = '#filterDropdown .dropdownInner ul';
        expect($(menuSelector).length).toEqual(1);
        expect($('span', menuSelector).attr('title')).toEqual('Filter');
        expect($('span', menuSelector)).toBeOfClass('icon_arangodb_filter');
      });

      it('should have the same layout as the web interface', function () {
        var header = div.children[0];
        expect(header).toBeTag('ul');
        expect(header.id).toEqual('menubar');
      });
    });

    describe('checking the node colour mapping list', function () {
      var map;

      beforeEach(function () {
        map = $('#contentDiv #node_colour_list');
      });

      it('should append the list', function () {
        expect(map.length).toEqual(1);
      });

      it('should have the correct css class', function () {
        expect(map).toBeOfClass('gv-colour-list');
      });
    });

    describe('checking to load a graph', function () {
      var waittime = 200;

      beforeEach(function () {
        this.addMatchers({
          toBeDisplayed: function () {
            var nodes = this.actual,
              nonDisplayed = [];
            this.message = function () {
              var msg = 'Nodes: [';
              _.each(nonDisplayed, function (n) {
                msg += n + ' ';
              });
              msg += '] are not displayed.';
              return msg;
            };
            _.each(nodes, function (n) {
              if ($('svg #' + n)[0] === undefined) {
                nonDisplayed.push(n);
              }
            });
            return nonDisplayed.length === 0;
          }
        });
      });

      it('should load the graph by _id', function () {
        runs(function () {
          $('#contentDiv #menubar #value').attr('value', '0');
          helper.simulateMouseEvent('click', 'loadnode');
        });

        waits(waittime);

        runs(function () {
          expect([0, 1, 2, 3, 4]).toBeDisplayed();
        });
      });

      it('should load the graph on pressing enter', function () {
        runs(function () {
          $('#contentDiv #menubar #value').attr('value', '0');
          var e = $.Event('keypress');
          e.keyCode = 13;
          $('#value').trigger(e);
        });

        waits(waittime);

        runs(function () {
          expect([0, 1, 2, 3, 4]).toBeDisplayed();
        });
      });

      it('should load the graph by attribute and value', function () {
        runs(function () {
          adapterMockCall = {};
          $('#contentDiv #menubar #attribute').attr('value', 'name');
          $('#contentDiv #menubar #value').attr('value', '0');
          helper.simulateMouseEvent('click', 'loadnode');
          expect(adapterMockCall).toEqual({
            attribute: 'name',
            value: '0'
          });
        });
      });
    });

    describe('set up with jsonAdapter and click Expand rest default', function () {
      // This waittime is rather optimistic, on a slow machine this has to be increased
      var waittime = 100,

        clickOnNode = function (id) {
          helper.simulateMouseEvent('click', id);
      };

      beforeEach(function () {
        this.addMatchers({
          toBeDisplayed: function () {
            var nodes = this.actual,
              nonDisplayed = [];
            this.message = function () {
              var msg = 'Nodes: [';
              _.each(nonDisplayed, function (n) {
                msg += n + ' ';
              });
              msg += '] are not displayed.';
              return msg;
            };
            _.each(nodes, function (n) {
              if ($('svg #' + n)[0] === undefined) {
                nonDisplayed.push(n);
              }
            });
            return nonDisplayed.length === 0;
          },
          toNotBeDisplayed: function () {
            var nodes = this.actual,
              displayed = [];
            this.message = function () {
              var msg = 'Nodes: [';
              _.each(displayed, function (n) {
                msg += n + ' ';
              });
              msg += '] are still displayed.';
              return msg;
            };
            _.each(nodes, function (n) {
              if ($('svg #' + n)[0] !== undefined) {
                displayed.push(n);
              }
            });
            return displayed.length === 0;
          },
          toBeConnectedTo: function (target) {
            var source = this.actual;
            this.message = function () {
              return source + ' -> ' + target + ' edge, does not exist';
            };
            return $('svg #' + source + '-' + target)[0] !== undefined;
          },
          toNotBeConnectedTo: function (target) {
            var source = this.actual;
            this.message = function () {
              return source + ' -> ' + target + ' edge, does still exist';
            };
            return $('svg #' + source + '-' + target)[0] === undefined;
          }

        });

        runs(function () {
          $('#contentDiv #menubar #value').attr('value', '0');
          helper.simulateMouseEvent('click', 'loadnode');
          helper.simulateMouseEvent('click', 'control_event_expand');
        });

        waits(waittime);
      });

      it('should be able to expand a node', function () {
        runs(function () {
          clickOnNode(1);
        });

        waits(waittime);

        runs(function () {
          expect([0, 1, 2, 3, 4, 5, 6, 7]).toBeDisplayed();
        });
      });

      it('should be able to collapse the root', function () {
        runs(function () {
          clickOnNode(0);
        });

        waits(waittime);

        runs(function () {
          // Load 1 Nodes: Root
          expect([0]).toBeDisplayed();
          expect([1, 2, 3, 4]).toNotBeDisplayed();
        });
      });

      it('should be able to load a different graph', function () {
        runs(function () {
          $('#contentDiv #menubar #value').attr('value', '42');
          helper.simulateMouseEvent('click', 'loadnode');
        });

        waits(waittime);

        runs(function () {
          expect([42, 43, 44, 45]).toBeDisplayed();
          expect([0, 1, 2, 3, 4]).toNotBeDisplayed();
        });
      });

      describe('when a user rapidly expand nodes', function () {
        beforeEach(function () {
          runs(function () {
            clickOnNode(1);
            clickOnNode(2);
            clickOnNode(3);
            clickOnNode(4);
          });

          waits(waittime);

          it('the graph should still be correct', function () {
            expect([0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 12]).toBeDisplayed();
          });
        });
      });

      describe('when a user rapidly expand and collapses nodes', function () {
        beforeEach(function () {
          runs(function () {
            clickOnNode(1);
            clickOnNode(2);
            clickOnNode(3);
            clickOnNode(4);
          });

          // Wait a gentle second for all nodes to expand properly
          waits(waittime);

          runs(function () {
            clickOnNode(1);
            clickOnNode(4);
          });

          waits(waittime);
        });

        it('the graph should still be correct', function () {
          expect([0, 1, 2, 3, 4, 8, 9]).toBeDisplayed();
          expect([5, 6, 7, 12]).toNotBeDisplayed();
        });
      });

      describe('when an undirected circle has been loaded', function () {
        beforeEach(function () {
          runs(function () {
            clickOnNode(2);
            clickOnNode(3);
          });

          waits(waittime);
        });

        it('the basis should be correct', function () {
          expect([0, 1, 2, 3, 4, 8, 9]).toBeDisplayed();
        });

        it('should be able to collapse one node '
          + 'without removing the double referenced one', function () {
            runs(function () {
              clickOnNode(2);
            });

            waits(waittime);

            runs(function () {
              expect([2, 3, 8]).toBeDisplayed();
              expect(2).toNotBeConnectedTo(8);
              expect(3).toBeConnectedTo(8);
            });
          });

        it('should be able to collapse the other node '
          + 'without removing the double referenced one', function () {
            runs(function () {
              clickOnNode(3);
            });

            waits(waittime);

            runs(function () {
              expect([2, 3, 8]).toBeDisplayed();
              expect(3).toNotBeConnectedTo(8);
              expect(2).toBeConnectedTo(8);
            });
          });

        it('should be able to collapse the both nodes '
          + 'and remove the double referenced one', function () {
            runs(function () {
              clickOnNode(3);
              clickOnNode(2);
            });

            waits(waittime);

            runs(function () {
              expect([2, 3]).toBeDisplayed();
              expect([8]).toNotBeDisplayed();
              expect(3).toNotBeConnectedTo(8);
              expect(2).toNotBeConnectedTo(8);
            });
          });
      });

      describe('when a complex graph has been loaded', function () {
        beforeEach(function () {
          runs(function () {
            clickOnNode(1);
            clickOnNode(4);
            clickOnNode(2);
            clickOnNode(3);
          });
          waits(waittime);
        });

        it('should be able to collapse a node '
          + 'referencing a node connected to a subgraph', function () {
            runs(function () {
              clickOnNode(1);
            });

            waits(waittime);

            runs(function () {
              expect([0, 1, 2, 3, 4, 5, 8, 9, 12]).toBeDisplayed();
              expect([6, 7, 10, 11]).toNotBeDisplayed();

              expect(0).toBeConnectedTo(1);
              expect(0).toBeConnectedTo(2);
              expect(0).toBeConnectedTo(3);
              expect(0).toBeConnectedTo(4);
              expect(1).toNotBeConnectedTo(5);

              expect(2).toBeConnectedTo(8);

              expect(3).toBeConnectedTo(8);
              expect(3).toBeConnectedTo(9);

              expect(4).toBeConnectedTo(5);
              expect(4).toBeConnectedTo(12);
            });
          });
      });
    });
  });
}());
