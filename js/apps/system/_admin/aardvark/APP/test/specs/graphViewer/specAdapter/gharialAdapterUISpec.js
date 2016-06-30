/* jshint unused: false */
/* global beforeEach, afterEach */
/* global describe, it, expect*/
/* global runs, waitsFor, spyOn */
/* global window, eb, loadFixtures, document */
/* global $, _, d3*/
/* global helper, uiMatchers*/
/* global GharialAdapterControls, GharialAdapter*/

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

  describe('Gharial Adapter UI', function () {
    var adapter, adapterUI, list, prioList;

    beforeEach(function () {
      prioList = [];
      adapter = {
        getPrioList: function () {
          return undefined;
        },
        changeToGraph: function () {
          return undefined;
        },
        changeTo: function () {
          return undefined;
        },
        getGraphs: function (cb) {
          cb(['oldGraph', 'newGraph']);
        },
        getGraphName: function () {
          return undefined;
        },
        getDirection: function () {
          return undefined;
        },
        loadRandomNode: function () {
          return undefined;
        }
      };
      list = document.createElement('ul');
      document.body.appendChild(list);
      list.id = 'control_adapter_list';
      adapterUI = new GharialAdapterControls(list, adapter);
      spyOn(adapter, 'changeTo');
      spyOn(adapter, 'changeToGraph');
      spyOn(adapter, 'getGraphs').andCallThrough();
      spyOn(adapter, 'getPrioList').andCallFake(function () {
        return prioList;
      });
      uiMatchers.define(this);
    });

    afterEach(function () {
      document.body.removeChild(list);
    });

    it('should throw errors if not setup correctly', function () {
      expect(function () {
        var e = new GharialAdapterControls();
        return e;
      }).toThrow('A list element has to be given.');
      expect(function () {
        var e = new GharialAdapterControls(list);
        return e;
      }).toThrow('The GharialAdapter has to be given.');
    });

    it('should check adapter interface consistency', function () {
      var realAdapter = new GharialAdapter([], [], {}, {graphName: 'oldGraph'});
      this.addMatchers({
        toOfferFunction: function (name) {
          this.message = function () {
            return 'Function "' + name + '" is not available on adapter.';
          };
          return _.isFunction(this.actual[name]);
        }
      });
      _.each(
        _.keys(adapter), function (name) {
          expect(realAdapter).toOfferFunction(name);
        }
      );
    });

    describe('change graph control', function () {
      var idPrefix = '#control_adapter_graph',
        callbacks;

      beforeEach(function () {
        runs(function () {
          callbacks = {
            cb: function () {
              return undefined;
            }
          };
          spyOn(callbacks, 'cb');
          adapterUI.addControlChangeGraph(callbacks.cb);
          expect($('#control_adapter_list ' + idPrefix).length).toEqual(1);
          expect($('#control_adapter_list ' + idPrefix)[0]).toConformToListCSS();
          helper.simulateMouseEvent('click', idPrefix.substr(1) + '_button');
          expect($(idPrefix + '_modal').length).toEqual(1);
        });
      });

      afterEach(function () {
        waitsFor(function () {
          return $(idPrefix + '_modal').length === 0;
        }, 2000, 'The modal dialog should disappear.');
      });

      it('should be added to the list', function () {
        runs(function () {
          $(idPrefix + '_graph').prop('selectedIndex', 1);
          helper.simulateMouseEvent('click', idPrefix.substr(1) + '_submit');
          expect(adapter.changeToGraph).toHaveBeenCalledWith(
            'oldGraph',
            false
          );
        });
      });

      it('should change graph and traversal direction to directed', function () {
        runs(function () {
          $(idPrefix + '_graph').prop('selectedIndex', 1);
          $(idPrefix + '_undirected').attr('checked', false);

          helper.simulateMouseEvent('click', idPrefix.substr(1) + '_submit');

          expect(adapter.changeToGraph).toHaveBeenCalledWith(
            'oldGraph',
            false
          );
        });
      });

      it('should change graph and traversal direction to undirected', function () {
        runs(function () {
          $(idPrefix + '_graph').prop('selectedIndex', 1);
          $(idPrefix + '_undirected').attr('checked', true);

          helper.simulateMouseEvent('click', idPrefix.substr(1) + '_submit');

          expect(adapter.changeToGraph).toHaveBeenCalledWith(
            'oldGraph',
            true
          );
        });
      });

      it('should be possible to start with a random vertex', function () {
        runs(function () {
          spyOn(adapter, 'loadRandomNode');
          $(idPrefix + '_graph').prop('selectedIndex', 1);
          $(idPrefix + '_undirected').attr('checked', true);
          $(idPrefix + '_random').attr('checked', true);

          helper.simulateMouseEvent('click', idPrefix.substr(1) + '_submit');

          expect(adapter.loadRandomNode).toHaveBeenCalled();
        });
      });

      it('should be possible to not start with a random vertex', function () {
        runs(function () {
          spyOn(adapter, 'loadRandomNode');
          $(idPrefix + '_graph').prop('selectedIndex', 1);
          $(idPrefix + '_undirected').attr('checked', true);
          $(idPrefix + '_random').attr('checked', false);

          helper.simulateMouseEvent('click', idPrefix.substr(1) + '_submit');

          expect(adapter.loadRandomNode).not.toHaveBeenCalled();
        });
      });

      it('should trigger the callback', function () {
        runs(function () {
          $(idPrefix + '_graph').prop('selectedIndex', 1);
          $(idPrefix + '_undirected').attr('checked', true);
          $(idPrefix + '_random').attr('checked', false);

          helper.simulateMouseEvent('click', idPrefix.substr(1) + '_submit');

          expect(callbacks.cb).toHaveBeenCalled();
        });
      });

      it('should offer the available graphs as sorted lists', function () {
        runs(function () {
          var graphList = document.getElementById(idPrefix.substr(1) + '_graph'),
            graphCollectionOptions = graphList.children;

          expect(adapter.getGraphs).toHaveBeenCalled();

          expect(graphList).toBeTag('select');
          expect(graphCollectionOptions.length).toEqual(2);
          expect(graphCollectionOptions[0]).toBeTag('option');
          expect(graphCollectionOptions[1]).toBeTag('option');

          expect(graphCollectionOptions[0].value).toEqual('newGraph');
          expect(graphCollectionOptions[1].value).toEqual('oldGraph');

          helper.simulateMouseEvent('click', idPrefix.substr(1) + '_submit');
        });
      });
    });

    describe('change priority list control', function () {
      var idPrefix;

      beforeEach(function () {
        idPrefix = '#control_adapter_priority';
        adapterUI.addControlChangePriority();

        expect($('#control_adapter_list ' + idPrefix).length).toEqual(1);
        expect($('#control_adapter_list ' + idPrefix)[0]).toConformToListCSS();
        helper.simulateMouseEvent('click', idPrefix.substr(1) + '_button');
        expect($(idPrefix + '_modal').length).toEqual(1);
      });

      afterEach(function () {
        waitsFor(function () {
          return $(idPrefix + '_modal').length === 0;
        }, 2000, 'The modal dialog should disappear.');
      });

      it('should be added to the list', function () {
        runs(function () {
          $(idPrefix + '_attribute_1').attr('value', 'foo');
          helper.simulateMouseEvent('click', idPrefix.substr(1) + '_submit');
          expect(adapter.changeTo).toHaveBeenCalledWith({
            prioList: ['foo']
          });
        });
      });

      it('should not add empty attributes to priority', function () {
        runs(function () {
          $(idPrefix + '_attribute_1').attr('value', '');
          helper.simulateMouseEvent('click', idPrefix.substr(1) + '_submit');
          expect(adapter.changeTo).toHaveBeenCalledWith({
            prioList: []
          });
        });
      });

      it('should add a new line to priority on demand', function () {
        runs(function () {
          helper.simulateMouseEvent('click', idPrefix.substr(1) + '_attribute_addLine');
          expect($(idPrefix + '_attribute_1').length).toEqual(1);
          expect($(idPrefix + '_attribute_2').length).toEqual(1);
          expect($(idPrefix + '_attribute_addLine').length).toEqual(1);
          $(idPrefix + '_attribute_1').val('foo');
          $(idPrefix + '_attribute_2').val('bar');
          helper.simulateMouseEvent('click', idPrefix.substr(1) + '_submit');
          expect(adapter.changeTo).toHaveBeenCalledWith({
            prioList: ['foo', 'bar']
          });
        });
      });

      it('should add many new lines to priority on demand', function () {
        runs(function () {
          var idPrefixAttr = idPrefix.substr(1) + '_attribute_';
          helper.simulateMouseEvent('click', idPrefixAttr + 'addLine');
          helper.simulateMouseEvent('click', idPrefixAttr + 'addLine');
          helper.simulateMouseEvent('click', idPrefixAttr + 'addLine');
          helper.simulateMouseEvent('click', idPrefixAttr + 'addLine');
          expect($('#' + idPrefixAttr + '1').length).toEqual(1);
          expect($('#' + idPrefixAttr + '2').length).toEqual(1);
          expect($('#' + idPrefixAttr + '3').length).toEqual(1);
          expect($('#' + idPrefixAttr + '4').length).toEqual(1);
          expect($('#' + idPrefixAttr + '5').length).toEqual(1);

          expect($('#' + idPrefixAttr + '1').val()).toEqual('');
          expect($('#' + idPrefixAttr + '2').val()).toEqual('');
          expect($('#' + idPrefixAttr + '3').val()).toEqual('');
          expect($('#' + idPrefixAttr + '4').val()).toEqual('');
          expect($('#' + idPrefixAttr + '5').val()).toEqual('');

          expect($('#' + idPrefixAttr + 'addLine').length).toEqual(1);
          $('#' + idPrefixAttr + '1').val('foo');
          $('#' + idPrefixAttr + '2').val('bar');
          $('#' + idPrefixAttr + '3').val('');
          $('#' + idPrefixAttr + '4').val('baz');
          $('#' + idPrefixAttr + '5').val('foxx');
          helper.simulateMouseEvent('click', idPrefix.substr(1) + '_submit');
          expect(adapter.changeTo).toHaveBeenCalledWith({
            prioList: ['foo', 'bar', 'baz', 'foxx']
          });
        });
      });

      it('should remove all but the first line', function () {
        runs(function () {
          helper.simulateMouseEvent('click', idPrefix.substr(1) + '_attribute_addLine');
          expect($(idPrefix + '_attribute_1_remove').length).toEqual(0);
          expect($(idPrefix + '_attribute_2_remove').length).toEqual(1);
          helper.simulateMouseEvent('click', idPrefix.substr(1) + '_attribute_2_remove');

          expect($(idPrefix + '_attribute_addLine').length).toEqual(1);
          expect($(idPrefix + '_attribute_2_remove').length).toEqual(0);
          expect($(idPrefix + '_attribute_2').length).toEqual(0);

          $(idPrefix + '_attribute_1').attr('value', 'foo');
          helper.simulateMouseEvent('click', idPrefix.substr(1) + '_submit');
          expect(adapter.changeTo).toHaveBeenCalledWith({
            prioList: ['foo']
          });
        });
      });

    /* TO_DO

    it('should load the current prioList from the adapter', function() {

      runs(function() {
        helper.simulateMouseEvent("click", "control_adapter_priority_cancel")
      })

      waitsFor(function() {
        return $("#control_adapter_priority_modal").length === 0
      }, 2000, "The modal dialog should disappear.")

      runs(function() {
        expect($("#control_adapter_priority_cancel").length).toEqual(0)
        prioList.push("foo")
        prioList.push("bar")
        prioList.push("baz")
        helper.simulateMouseEvent("click", "control_adapter_priority")
        expect(adapter.getPrioList).wasCalled()
        var idPrefix = "#control_adapter_priority_attribute_"
        expect($(idPrefix + "1").length).toEqual(1)
        expect($(idPrefix + "2").length).toEqual(1)
        expect($(idPrefix + "3").length).toEqual(1)
        expect($(idPrefix + "4").length).toEqual(0)
        expect($(idPrefix + "addLine").length).toEqual(1)
        expect($(idPrefix + "1_remove").length).toEqual(0)
        expect($(idPrefix + "2_remove").length).toEqual(1)
        expect($(idPrefix + "3_remove").length).toEqual(1)
        expect($(idPrefix + "1").attr("value")).toEqual("foo")
        expect($(idPrefix + "2").attr("value")).toEqual("bar")
        expect($(idPrefix + "3").attr("value")).toEqual("baz")
        expect($("#control_adapter_priority_modal").length).toEqual(1)
        expect($("#control_adapter_priority_cancel").length).toEqual(1)
        helper.simulateMouseEvent("click", "control_adapter_priority_cancel")
      })
    })
    */
    });

    it('should be able to add all controls to the list', function () {
      adapterUI.addAll();
      expect($('#control_adapter_list #control_adapter_graph').length).toEqual(1);
      expect($('#control_adapter_list #control_adapter_priority').length).toEqual(1);
    });
  });
}());
