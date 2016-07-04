/* jshint unused: false */
/* global beforeEach, afterEach */
/* global describe, it, expect, jasmine*/
/* global runs, waitsFor, spyOn */
/* global window, eb, loadFixtures, document */
/* global $, _, d3*/
/* global helper, uiMatchers*/
/* global NodeShaper, NodeShaperControls*/

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

  describe('Node Shaper UI', function () {
    var svg, shaper, shaperUI, list;

    beforeEach(function () {
      svg = document.createElement('svg');
      document.body.appendChild(svg);
      shaper = new NodeShaper(d3.select('svg'));
      list = document.createElement('ul');
      document.body.appendChild(list);
      list.id = 'control_node_list';
      shaperUI = new NodeShaperControls(list, shaper);
      spyOn(shaper, 'changeTo');
      spyOn(shaper, 'getColourMapping').andCallFake(function () {
        return {
          blue: {
            list: ['bl', 'ue'],
            front: 'white'
          },
          green: {
            list: ['gr', 'een'],
            front: 'black'
          }
        };
      });
      spyOn(shaper, 'setColourMappingListener');
      uiMatchers.define(this);
    });

    afterEach(function () {
      document.body.removeChild(svg);
      document.body.removeChild(list);
    });

    it('should throw errors if not setup correctly', function () {
      expect(function () {
        var c = new NodeShaperControls();
      }).toThrow('A list element has to be given.');
      expect(function () {
        var c = new NodeShaperControls(list);
      }).toThrow('The NodeShaper has to be given.');
    });

    it('should be able to add a shape none control to the list', function () {
      runs(function () {
        shaperUI.addControlOpticShapeNone();

        expect($('#control_node_list #control_node_none').length).toEqual(1);
        expect($('#control_node_list #control_node_none')[0]).toConformToListCSS();

        helper.simulateMouseEvent('click', 'control_node_none_button');

        expect(shaper.changeTo).toHaveBeenCalledWith({
          shape: {
            type: NodeShaper.shapes.NONE
          }
        });
      });
    });

    it('should be able to add a shape circle control to the list', function () {
      runs(function () {
        shaperUI.addControlOpticShapeCircle();

        expect($('#control_node_list #control_node_circle').length).toEqual(1);
        expect($('#control_node_list #control_node_circle')[0]).toConformToListCSS();
        helper.simulateMouseEvent('click', 'control_node_circle_button');
        expect($('#control_node_circle_modal').length).toEqual(1);
        $('#control_node_circle_radius').attr('value', 42);
        helper.simulateMouseEvent('click', 'control_node_circle_submit');

        expect(shaper.changeTo).toHaveBeenCalledWith({
          shape: {
            type: NodeShaper.shapes.CIRCLE,
            radius: '42'
          }
        });
      });

      waitsFor(function () {
        return $('#control_node_circle_modal').length === 0;
      }, 2000, 'The modal dialog should disappear.');
    });

    it('should be able to add a shape rect control to the list', function () {
      runs(function () {
        shaperUI.addControlOpticShapeRect();

        expect($('#control_node_list #control_node_rect').length).toEqual(1);
        expect($('#control_node_list #control_node_rect')[0]).toConformToListCSS();

        helper.simulateMouseEvent('click', 'control_node_rect_button');
        $('#control_node_rect_width').attr('value', 42);
        $('#control_node_rect_height').attr('value', 12);
        helper.simulateMouseEvent('click', 'control_node_rect_submit');

        expect(shaper.changeTo).toHaveBeenCalledWith({
          shape: {
            type: NodeShaper.shapes.RECT,
            width: '42',
            height: '12'
          }
        });
      });

      waitsFor(function () {
        return $('#control_node_rect_modal').length === 0;
      }, 2000, 'The modal dialog should disappear.');
    });

    it('should be able to add a switch label control to the list', function () {
      runs(function () {
        shaperUI.addControlOpticLabel();

        expect($('#control_node_list #control_node_label').length).toEqual(1);
        expect($('#control_node_list #control_node_label')[0]).toConformToListCSS();

        helper.simulateMouseEvent('click', 'control_node_label_button');
        $('#control_node_label_key').attr('value', 'theAnswer');
        helper.simulateMouseEvent('click', 'control_node_label_submit');

        expect(shaper.changeTo).toHaveBeenCalledWith({
          label: 'theAnswer'
        });
      });

      waitsFor(function () {
        return $('#control_node_label_modal').length === 0;
      }, 2000, 'The modal dialog should disappear.');
    });

    it('should be able to add a switch single colour control to the list', function () {
      runs(function () {
        shaperUI.addControlOpticSingleColour();

        expect($('#control_node_list #control_node_singlecolour').length).toEqual(1);
        expect($('#control_node_list #control_node_singlecolour')[0]).toConformToListCSS();

        helper.simulateMouseEvent('click', 'control_node_singlecolour_button');
        $('#control_node_singlecolour_fill').attr('value', '#123456');
        $('#control_node_singlecolour_stroke').attr('value', '#654321');
        helper.simulateMouseEvent('click', 'control_node_singlecolour_submit');

        expect(shaper.changeTo).toHaveBeenCalledWith({
          color: {
            type: 'single',
            fill: '#123456',
            stroke: '#654321'
          }
        });
      });

      waitsFor(function () {
        return $('#control_node_singlecolour_modal').length === 0;
      }, 2000, 'The modal dialog should disappear.');
    });

    it('should be able to add a switch colour on attribute control to the list', function () {
      runs(function () {
        shaperUI.addControlOpticAttributeColour();

        expect($('#control_node_list #control_node_attributecolour').length).toEqual(1);
        expect($('#control_node_list #control_node_attributecolour')[0]).toConformToListCSS();

        helper.simulateMouseEvent('click', 'control_node_attributecolour_button');
        $('#control_node_attributecolour_key').attr('value', 'label');
        helper.simulateMouseEvent('click', 'control_node_attributecolour_submit');

        expect(shaper.changeTo).toHaveBeenCalledWith({
          color: {
            type: 'attribute',
            key: 'label'
          }
        });
      });

      waitsFor(function () {
        return $('#control_node_attributecolour_modal').length === 0;
      }, 2000, 'The modal dialog should disappear.');
    });

    it('should be able to add a switch colour on expand status control to the list', function () {
      runs(function () {
        shaperUI.addControlOpticExpandColour();

        expect($('#control_node_list #control_node_expandcolour').length).toEqual(1);
        expect($('#control_node_list #control_node_expandcolour')[0]).toConformToListCSS();

        helper.simulateMouseEvent('click', 'control_node_expandcolour_button');
        $('#control_node_expandcolour_expanded').attr('value', '#123456');
        $('#control_node_expandcolour_collapsed').attr('value', '#654321');
        helper.simulateMouseEvent('click', 'control_node_expandcolour_submit');

        expect(shaper.changeTo).toHaveBeenCalledWith({
          color: {
            type: 'expand',
            expanded: '#123456',
            collapsed: '#654321'
          }
        });
      });

      waitsFor(function () {
        return $('#control_node_expandcolour_modal').length === 0;
      }, 2000, 'The modal dialog should disappear.');
    });

    it('should be able to add all optic controls to the list', function () {
      shaperUI.addAllOptics();

      expect($('#control_node_list #control_node_none').length).toEqual(1);
      expect($('#control_node_list #control_node_circle').length).toEqual(1);
      expect($('#control_node_list #control_node_rect').length).toEqual(1);
      expect($('#control_node_list #control_node_label').length).toEqual(1);
      expect($('#control_node_list #control_node_singlecolour').length).toEqual(1);
      expect($('#control_node_list #control_node_attributecolour').length).toEqual(1);
      expect($('#control_node_list #control_node_expandcolour').length).toEqual(1);
    });

    it('should be able to add all action controls to the list', function () {
      shaperUI.addAllActions();
    });

    it('should be able to add all controls to the list', function () {
      shaperUI.addAll();

      expect($('#control_node_list #control_node_none').length).toEqual(1);
      expect($('#control_node_list #control_node_circle').length).toEqual(1);
      expect($('#control_node_list #control_node_rect').length).toEqual(1);
      expect($('#control_node_list #control_node_label').length).toEqual(1);
      expect($('#control_node_list #control_node_singlecolour').length).toEqual(1);
      expect($('#control_node_list #control_node_attributecolour').length).toEqual(1);
      expect($('#control_node_list #control_node_expandcolour').length).toEqual(1);
    });

    it('should be able to create a div containing the color <-> label mapping', function () {
      var div = shaperUI.createColourMappingList(),
        list = div.firstChild,
        blue = list.children[0],
        green = list.children[1],
        blue1 = blue.children[0],
        blue2 = blue.children[1],
        green1 = green.children[0],
        green2 = green.children[1];

      expect(shaper.getColourMapping).wasCalled();
      expect(shaper.setColourMappingListener).wasCalledWith(jasmine.any(Function));
      expect(div).toBeTag('div');
      expect($(div).attr('id')).toEqual('node_colour_list');
      expect(list).toBeTag('ul');
      expect(blue).toBeTag('ul');
      expect(blue.children.length).toEqual(2);
      expect(blue.style.backgroundColor).toEqual('blue');

      expect(blue1).toBeTag('li');
      expect($(blue1).text()).toEqual('bl');
      expect(blue2).toBeTag('li');
      expect($(blue2).text()).toEqual('ue');

      expect(green).toBeTag('ul');
      expect(green.children.length).toEqual(2);
      expect(green.style.backgroundColor).toEqual('green');

      expect(green1).toBeTag('li');
      expect($(green1).text()).toEqual('gr');
      expect(green2).toBeTag('li');
      expect($(green2).text()).toEqual('een');
    });

    it('should not create the colour list twice', function () {
      shaperUI.createColourMappingList();
      spyOn(document, 'createElement');
      shaperUI.createColourMappingList();
      expect(document.createElement).not.toHaveBeenCalled();
    });

    it('should be able to submit the controls with return', function () {
      runs(function () {
        shaperUI.addControlOpticExpandColour();

        expect($('#control_node_list #control_node_expandcolour').length).toEqual(1);
        expect($('#control_node_list #control_node_expandcolour')[0]).toConformToListCSS();

        helper.simulateMouseEvent('click', 'control_node_expandcolour_button');
        $('#control_node_expandcolour_expanded').attr('value', '#123456');
        $('#control_node_expandcolour_collapsed').attr('value', '#654321');

        helper.simulateReturnEvent();

        expect(shaper.changeTo).toHaveBeenCalledWith({
          color: {
            type: 'expand',
            expanded: '#123456',
            collapsed: '#654321'
          }
        });
      });

      waitsFor(function () {
        return $('#control_node_expandcolour_modal').length === 0;
      }, 2000, 'The modal dialog should disappear.');
    });

    describe('checking to change colour and label at once with a list', function () {
      var listId, idPrefix, idOnly, modalId, colAttrPrefix, lblAttrPrefix,
        submitId, checkBoxSelector, addColourLineId, addLabelLineId;

      beforeEach(function () {
        listId = '#control_node_list';
        idPrefix = '#control_node_labelandcolourlist';
        lblAttrPrefix = idPrefix + '_label_';
        colAttrPrefix = idPrefix + '_colour_';
        modalId = idPrefix + '_modal';
        idOnly = idPrefix.substr(1);
        submitId = idOnly + '_submit';
        addLabelLineId = lblAttrPrefix.substr(1) + 'addLine';
        addColourLineId = colAttrPrefix.substr(1) + 'addLine';
        checkBoxSelector = "input[type='radio'][name='colour']";
        shaperUI.addControlOpticLabelAndColourList();
        expect($(idPrefix, $(listId)).length).toEqual(1);
        expect($(idPrefix, $(listId))[0]).toConformToListCSS();
        helper.simulateMouseEvent('click', idOnly + '_button');
        expect($(modalId).length).toEqual(1);
      });

      afterEach(function () {
        waitsFor(function () {
          return $(modalId).length === 0;
        }, 2000, 'The modal dialog should disappear.');
      });

      it('should be added to the list', function () {
        runs(function () {
          var val = 'foo';
          $(lblAttrPrefix + '1').attr('value', val);
          helper.simulateMouseEvent('click', submitId);
          expect(shaper.changeTo).toHaveBeenCalledWith({
            label: [val],
            color: {
              type: 'attribute',
              key: [val]
            }
          });
        });
      });

      it('should not add empty attributes for label', function () {
        runs(function () {
          $(lblAttrPrefix + '1').attr('value', '');
          helper.simulateMouseEvent('click', submitId);
          expect(shaper.changeTo).toHaveBeenCalledWith({
            label: [],
            color: {
              type: 'attribute',
              key: []
            }
          });
        });
      });

      it('should add a new line to both on demand', function () {
        runs(function () {
          var lbl1 = 'lbl1', lbl2 = 'lbl2', col1 = 'col1', col2 = 'col2';
          helper.simulateMouseEvent('click', addLabelLineId);
          expect($(lblAttrPrefix + '1').length).toEqual(1);
          expect($(lblAttrPrefix + '2').length).toEqual(1);
          expect($('#' + addLabelLineId).length).toEqual(1);
          $(lblAttrPrefix + '1').val(lbl1);
          $(lblAttrPrefix + '2').val(lbl2);
          $(checkBoxSelector).prop('checked', true);
          helper.simulateMouseEvent('click', addColourLineId);
          expect($(colAttrPrefix + '1').length).toEqual(1);
          expect($(colAttrPrefix + '2').length).toEqual(1);
          expect($('#' + addColourLineId).length).toEqual(1);
          $(colAttrPrefix + '1').val(col1);
          $(colAttrPrefix + '2').val(col2);
          helper.simulateMouseEvent('click', submitId);
          expect(shaper.changeTo).toHaveBeenCalledWith({
            label: [lbl1, lbl2],
            color: {
              type: 'attribute',
              key: [col1, col2]
            }
          });
        });
      });

      it('should add many new lines to label on demand', function () {
        runs(function () {
          var lbl1 = 'foo', lbl2 = 'baz', lbl3 = 'bar', lbl4 = 'foxx';
          helper.simulateMouseEvent('click', addLabelLineId);
          helper.simulateMouseEvent('click', addLabelLineId);
          helper.simulateMouseEvent('click', addLabelLineId);
          helper.simulateMouseEvent('click', addLabelLineId);
          expect($(lblAttrPrefix + '1').length).toEqual(1);
          expect($(lblAttrPrefix + '2').length).toEqual(1);
          expect($(lblAttrPrefix + '3').length).toEqual(1);
          expect($(lblAttrPrefix + '4').length).toEqual(1);
          expect($(lblAttrPrefix + '5').length).toEqual(1);

          expect($(lblAttrPrefix + '1').val()).toEqual('');
          expect($(lblAttrPrefix + '2').val()).toEqual('');
          expect($(lblAttrPrefix + '3').val()).toEqual('');
          expect($(lblAttrPrefix + '4').val()).toEqual('');
          expect($(lblAttrPrefix + '5').val()).toEqual('');

          expect($('#' + addLabelLineId).length).toEqual(1);
          $(lblAttrPrefix + '1').val(lbl1);
          $(lblAttrPrefix + '2').val(lbl2);
          $(lblAttrPrefix + '3').val('');
          $(lblAttrPrefix + '4').val(lbl3);
          $(lblAttrPrefix + '5').val(lbl4);
          helper.simulateMouseEvent('click', submitId);
          expect(shaper.changeTo).toHaveBeenCalledWith({
            label: [lbl1, lbl2, lbl3, lbl4],
            color: {
              type: 'attribute',
              key: [lbl1, lbl2, lbl3, lbl4]
            }
          });
        });
      });

      it('should remove all but the first line', function () {
        runs(function () {
          var lbl1 = 'foo';
          helper.simulateMouseEvent('click', addLabelLineId);
          expect($(lblAttrPrefix + '1_remove').length).toEqual(0);
          expect($(lblAttrPrefix + '2_remove').length).toEqual(1);
          helper.simulateMouseEvent('click', lblAttrPrefix.substr(1) + '2_remove');

          expect($('#' + addLabelLineId).length).toEqual(1);
          expect($(lblAttrPrefix + '2_remove').length).toEqual(0);
          expect($(lblAttrPrefix + '2').length).toEqual(0);

          $(lblAttrPrefix + '1').attr('value', lbl1);
          helper.simulateMouseEvent('click', submitId);
          expect(shaper.changeTo).toHaveBeenCalledWith({
            label: [lbl1],
            color: {
              type: 'attribute',
              key: [lbl1]
            }
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

    describe('checking to change colour and label at once', function () {
      it('should be able to add the control and create the mapping list', function () {
        runs(function () {
          shaperUI.addControlOpticLabelAndColour();
          spyOn(shaperUI, 'createColourMappingList').andCallThrough();

          expect($('#control_node_list #control_node_labelandcolour').length).toEqual(1);
          expect($('#control_node_list #control_node_labelandcolour')[0]).toConformToListCSS();

          helper.simulateMouseEvent('click', 'control_node_labelandcolour_button');
          $('#control_node_labelandcolour_label-attribute').attr('value', 'label');
          helper.simulateMouseEvent('click', 'control_node_labelandcolour_submit');

          expect(shaper.changeTo).toHaveBeenCalledWith({
            label: 'label',
            color: {
              type: 'attribute',
              key: 'label'
            }
          });
          expect(shaperUI.createColourMappingList).wasCalled();
          expect(shaper.setColourMappingListener).wasCalledWith(jasmine.any(Function));
        });

        waitsFor(function () {
          return $('#control_node_attributecolour_modal').length === 0;
        }, 2000, 'The modal dialog should disappear.');

        runs(function () {
          expect(true).toBeTruthy();
        });
      });
    });
  });
}());
