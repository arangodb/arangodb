/* jshint browser: true */
/* jshint unused: false */
/* global describe, beforeEach, afterEach, it, */
/* global spyOn, runs, expect, waitsFor, jasmine, jQuery*/
/* global _, $, uiMatchers */

(function () {
  'use strict';

  describe('The Modal view Singleton', function () {
    var testee, div, jQueryDummy;

    beforeEach(function () {
      div = document.createElement('div');
      div.id = 'modalPlaceholder';
      document.body.appendChild(div);
      testee = new window.ModalView();
      uiMatchers.define(this);
    });

    afterEach(function () {
      document.body.removeChild(div);
    });

    describe('buttons', function () {
      var testShow;

      beforeEach(function () {
        testShow = testee.show.bind(testee, 'modalTable.ejs', 'My Modal');
      });

      it('should create all button types', function () {
        expect(testee.buttons.SUCCESS).toEqual('success');
        expect(testee.buttons.NOTIFICATION).toEqual('notification');
        expect(testee.buttons.NEUTRAL).toEqual('neutral');
        expect(testee.buttons.DELETE).toEqual('danger');
        expect(testee.buttons.CLOSE).toEqual('close');
      });

      it('should automatically create the close button', function () {
        testShow();
        var btn = $('.button-close', $(div));
        expect(btn.length).toEqual(1);
        spyOn(testee, 'hide').andCallThrough();
        btn.click();
        expect(testee.hide).toHaveBeenCalled();
      });

      it('should be able to bind a callback to a button', function () {
        var btnObj = {},
          title = 'Save',
          buttons = [],
          cbs = {
            callback: function () {
              testee.hide();
            }
          },
          btn;

        spyOn(cbs, 'callback').andCallThrough();
        btnObj = testee.createSuccessButton(title, cbs.callback);
        buttons.push(btnObj);
        testShow(buttons);
        btn = $('.button-' + btnObj.type, $(div));
        expect(btn.length).toEqual(1);
        expect(btn.text()).toEqual(title);
        btn.click();
        expect(cbs.callback).toHaveBeenCalled();
      });

      it('should be able to create several buttons', function () {
        var btnObj1 = {},
          btnObj2 = {},
          btnObj3 = {},
          title = 'Button_',
          buttons = [],
          cbs = {
            cb1: function () {
              throw 'Should never be invoked';
            },
            cb2: function () {
              throw 'Should never be invoked';
            },
            cb3: function () {
              throw 'Should never be invoked';
            }
          },
          btn;

        spyOn(cbs, 'cb1');
        spyOn(cbs, 'cb2');
        spyOn(cbs, 'cb3');
        btnObj1 = testee.createSuccessButton(title + 1, cbs.cb1);
        btnObj2 = testee.createNeutralButton(title + 2, cbs.cb2);
        btnObj3 = testee.createNotificationButton(title + 3, cbs.cb3);
        buttons.push(btnObj1);
        buttons.push(btnObj2);
        buttons.push(btnObj3);
        testShow(buttons);
        btn = $('button[class*=button-]', $(div));
        // Three defined above + cancel + Yes & No from delete which are always present
        expect(btn.length).toEqual(6);
        // Check click events for all buttons
        btn = $('.button-' + btnObj1.type, $(div));
        expect(btn.length).toEqual(1);
        btn.click();
        expect(cbs.cb1).toHaveBeenCalled();
        btn = $('.button-' + btnObj2.type + ':not(#modal-abort-delete)', $(div));
        expect(btn.length).toEqual(1);
        btn.click();
        expect(cbs.cb2).toHaveBeenCalled();
        btn = $('.button-' + btnObj3.type, $(div));
        expect(btn.length).toEqual(1);
        btn.click();
        expect(cbs.cb3).toHaveBeenCalled();
        testee.hide();
      });

      describe('should create a delete button', function () {
        var cbs;

        beforeEach(function () {
          var btnObj,
            title = 'Delete',
            buttons = [],
            btn;

          cbs = {
            callback: function () {
              testee.hide();
            }
          };
          spyOn(cbs, 'callback').andCallThrough();
          btnObj = testee.createDeleteButton(title, cbs.callback);
          buttons.push(btnObj);
          testShow(buttons);
          btn = $('.button-' + btnObj.type + ':not(#modal-confirm-delete)', $(div));
          expect(btn.length).toEqual(1);
          expect(btn.text()).toEqual(title);
          btn.click();
          expect(cbs.callback).not.toHaveBeenCalled();
        });

        it('with confirm dialog', function () {
          expect($('#modal-delete-confirmation').css('display')).toEqual('block');
          $('#modal-confirm-delete', $(div)).click();
          expect(cbs.callback).toHaveBeenCalled();
        });

        it('should hide confirm dialog', function () {
          expect($('#modal-delete-confirmation').css('display')).toEqual('block');
          $('#modal-abort-delete', $(div)).click();
          expect(cbs.callback).not.toHaveBeenCalled();
          expect($('#modal-delete-confirmation').css('display')).toEqual('none');
        });
      });

      it('should create a custom close button', function () {
        var btnObj = {},
          title = 'Cancel',
          buttons = [],
          cbs = {
            callback: function () {
              return undefined;
            }
          },
          btn;

        spyOn(cbs, 'callback').andCallThrough();
        btnObj = testee.createCloseButton(cbs.callback);
        buttons.push(btnObj);
        testShow(buttons);
        btn = $('.button-close', $(div));
        expect(btn.length).toEqual(1);
        expect(btn.text()).toEqual(title);
        spyOn(testee, 'hide').andCallThrough();
        btn.click();
        expect(testee.hide).toHaveBeenCalled();
        expect(cbs.callback).toHaveBeenCalled();
      });

      it('should create a disabled button', function () {
        var btnObj = {},
          title = 'Disabled',
          buttons = [],
          btn;

        btnObj = testee.createDisabledButton(title);
        buttons.push(btnObj);
        testShow(buttons);
        btn = $('.button-neutral:not(#modal-abort-delete)', $(div));
        expect(btn.length).toEqual(1);
        expect(btn.text()).toEqual(title);
        spyOn(testee, 'hide').andCallThrough();
        btn.click();
        expect(testee.hide).not.toHaveBeenCalled();
      });
    });

    describe('table content', function () {
      var testShow,
        tdSelector,
        readOnlyDef,
        textDef,
        pwDef,
        cbxDef,
        opt1Def,
        opt2Def,
        selectDef,
        select2Def;

      beforeEach(function () {
        testShow = testee.show.bind(testee, 'modalTable.ejs', 'My Modal', null);
        tdSelector = '.modal-body table tr th';
        readOnlyDef = testee.createReadOnlyEntry(
          'readOnly',
          'Readonly text',
          'ReadOnly',
          'ReadOnly Description'
        );
        textDef = testee.createTextEntry(
          'textId',
          'Text',
          'Text value',
          'Text Description',
          'Text Placeholder'
        );
        pwDef = testee.createPasswordEntry(
          'pwdId',
          'Password',
          'secret',
          'Password Description',
          'PWD Placeholder'
        );
        cbxDef = testee.createCheckboxEntry(
          'cbxId',
          'Checkbox',
          'myBox',
          'Checkbox Description',
          true
        );
        opt1Def = testee.createOptionEntry(
          'l1'
        );
        opt2Def = testee.createOptionEntry(
          'l2',
          'v2'
        );
        selectDef = testee.createSelectEntry(
          'selectId',
          'Select',
          'v2',
          'Select Info',
          [opt1Def, opt2Def]
        );
        select2Def = testee.createSelect2Entry(
          'select2Id',
          'Select2',
          ['Select2Tag', 'otherTag'],
          'Select Info',
          'Select your tags'
        );
      });

      it('should render readonly', function () {
        var content = [readOnlyDef],
          fields;

        testShow(content);
        fields = $(tdSelector);
        expect(fields.length).toEqual(3);
        expect($(fields[0]).text().trim()).toEqual(readOnlyDef.label + ':');
        expect($(fields[1]).text().trim()).toEqual(readOnlyDef.value);
      // expect($(fields[2]).text()).toEqual(lbl)
      });

      it('should render text-input', function () {
        var content = [textDef],
          fields,
          input;

        testShow(content);
        fields = $(tdSelector);
        expect($(fields[0]).text()).toEqual(textDef.label + ':');
        input = $('#' + textDef.id);
        expect(input[0]).toEqual($(fields[1]).find(' > input')[0]);
        expect(input[0]).toBeTag('input');
        expect(input.attr('type')).toEqual('text');
        expect(input.attr('placeholder')).toEqual(textDef.placeholder);
        expect(input.val()).toEqual(textDef.value);
      // expect($(fields[2]).text()).toEqual(lbl)
      });

      it('should render password-input', function () {
        var content = [pwDef],
          fields,
          input;

        testShow(content);
        fields = $(tdSelector);
        expect($(fields[0]).text()).toEqual(pwDef.label + ':');
        input = $('#' + pwDef.id);
        expect(input[0]).toEqual($(fields[1]).find(' > input')[0]);
        expect(input[0]).toBeTag('input');
        expect(input.attr('type')).toEqual('password');
        expect(input.attr('placeholder')).toEqual(pwDef.placeholder);
        expect(input.val()).toEqual(pwDef.value);
      // expect($(fields[2]).text()).toEqual(lbl)
      });

      it('should render checkbox', function () {
        var content = [cbxDef],
          fields,
          cbx;

        testShow(content);
        fields = $(tdSelector);
        expect($(fields[0]).text()).toEqual(cbxDef.label + ':');
        cbx = $('#' + cbxDef.id);
        expect(cbx[0]).toEqual($(fields[1]).find(' > input')[0]);
        expect(cbx.attr('type')).toEqual('checkbox');

      // expect($(fields[2]).text()).toEqual(lbl)
      });

      it('should render select', function () {
        var content = [selectDef],
          fields,
          select;

        testShow(content);
        fields = $(tdSelector);
        expect($(fields[0]).text()).toEqual(selectDef.label + ':');
        select = $('#' + selectDef.id);
        expect(select[0]).toEqual($(fields[1]).find(' > select')[0]);
        _.each(select.children(), function (o, i) {
          var opt = selectDef.options[i];
          expect($(o).attr('value')).toEqual(opt.value);
          expect($(o).text()).toEqual(opt.label);
        });
        expect(select.val()).toEqual(selectDef.selected);

      // expect($(fields[2]).text()).toEqual(lbl)
      });

      it('should render a select2', function () {
        var content = [select2Def],
          fields,
          select;

        testShow(content);
        fields = $(tdSelector);
        expect($(fields[0]).text()).toEqual(select2Def.label + ':');
        select = $('#' + select2Def.id);
        expect(_.pluck(select.select2('data'), 'text')).toEqual(select2Def.value);
      // expect($(fields[2]).text()).toEqual(lbl)
      });

      it('should render several types in a mix', function () {
        var content = [
            readOnlyDef,
            textDef,
            pwDef,
            cbxDef,
            selectDef
          ],
          rows;

        testShow(content);
        rows = $('.modal-body table tr');
        expect(rows.length).toEqual(content.length);
        _.each(rows, function (v, k) {
          expect($('th:first-child', $(v)).text()).toEqual(content[k].label + ':');
        });
      });
    });

    it('should delegate more events', function () {
      var myEvents = {
        'my': 'events'
      };
      expect(testee.events).not.toEqual(myEvents);
      spyOn(testee, 'delegateEvents');
      testee.show('modalTable.ejs', 'Delegate Events',
        undefined, undefined, undefined, myEvents);
      expect(testee.events).toEqual(myEvents);
      expect(testee.delegateEvents).toHaveBeenCalled();
    });

    it('should call keyboard bind function', function () {
      spyOn(testee, 'createModalHotkeys');
      spyOn(testee, 'createInitModalHotkeys');
      testee.enabledHotkey = false;
      testee.show('modalTable.ejs', 'Delegate Events',
        undefined, undefined, undefined, undefined);
      expect(testee.createModalHotkeys).toHaveBeenCalled();
      expect(testee.createInitModalHotkeys).toHaveBeenCalled();
    });

    it('should not call keyboard bind function', function () {
      spyOn(testee, 'createModalHotkeys');
      spyOn(testee, 'createInitModalHotkeys');
      testee.enabledHotkey = true;
      testee.show('modalTable.ejs', 'Delegate Events',
        undefined, undefined, undefined, undefined);
      expect(testee.createModalHotkeys).toHaveBeenCalled();
      expect(testee.createInitModalHotkeys).not.toHaveBeenCalled();
    });

    it('should call function bind function for view.el', function () {
      testee.enabledHotkey = false;
      var btnObj = {},
        title = 'Save',
        buttons = [],
        cbs = {
          callback: function () {
            throw 'Should by a spy';
          }
        },
        btn,
        testShow = testee.show.bind(testee, 'modalTable.ejs', 'My Modal'),
        e = jQuery.Event('keydown');
      e.which = 13;
      e.keyCode = 13;

      spyOn(cbs, 'callback');
      btnObj = testee.createSuccessButton(title, cbs.callback);
      buttons.push(btnObj);
      testShow(buttons);
      btn = $('.button-' + btnObj.type, $(div));

      spyOn($.fn, 'click').andCallThrough();
      $(testee.el).trigger(e);

      expect($.fn.click).toHaveBeenCalled();
      expect(cbs.callback).toHaveBeenCalled();
    });

    it('should call function bind function for view.el input', function () {
      var testShow = testee.show.bind(testee, 'modalTable.ejs', 'My Modal'),
        btnObj = {},
        title = 'Save',
        buttons = [],
        textField = [],
        cbs = {
          callback: function () {
            throw 'Should be a spy';
          }
        },
        btn,
        e = jQuery.Event('keydown');
      e.which = 13; // enter key
      e.keyCode = 13; // enter key

      testee.enabledHotkey = false;

      spyOn(cbs, 'callback');
      btnObj = testee.createSuccessButton(title, cbs.callback);
      buttons.push(btnObj);
      textField.push(
        testee.createTextEntry('dontcare', 'ABC', 'asd')
      );

      testShow(buttons, textField);

      btn = $('.button-' + btnObj.type, $(div));

      spyOn($.fn, 'click').andCallThrough();
      $('#dontcare').trigger(e);

      expect($.fn.click).toHaveBeenCalled();
      expect(cbs.callback).toHaveBeenCalled();
    });

    it('should call function bind function for view.el select', function () {
      var testShow = testee.show.bind(testee, 'modalTable.ejs', 'My Modal'),
        btnObj = {},
        title = 'Save',
        buttons = [],
        select = [],
        cbs = {
          callback: function () {
            throw 'Should be a spy';
          }
        },
        btn,
        e = jQuery.Event('keydown');
      e.which = 13; // enter key
      e.keyCode = 13; // enter key

      testee.enabledHotkey = false;
      spyOn(cbs, 'callback');
      btnObj = testee.createSuccessButton(title, cbs.callback);
      buttons.push(btnObj);

      select.push(
        testee.createSelectEntry('dontcare', 'Choose', false, undefined,
          testee.createOptionEntry('option1')
        )
      );

      testShow(buttons, select);

      btn = $('.button-' + btnObj.type, $(div));

      spyOn($.fn, 'click').andCallThrough();
      $('select').trigger(e);

      expect($.fn.click).toHaveBeenCalled();
      expect(cbs.callback).toHaveBeenCalled();
    });

    /* it("test focused button navigation (direction right)", function() {

     testee.enabledHotkey = false
     var btnObj = {},
     title = "Save",
     buttons = [],
     cbs = {
     callback: function() {
     }
     },
     btn,
     testShow = testee.show.bind(testee, "modalTable.ejs", "My Modal")

     spyOn(cbs, "callback").andCallThrough()
     btnObj = testee.createSuccessButton(title, cbs.callback)
     buttons.push(btnObj)
     testShow(buttons)
     btn = $(".button-" + btnObj.type, $(div))

     spyOn($.fn, "next")
     spyOn($.fn, "is").andReturn(true)

     btn.focus()
     testee.navigateThroughButtons('right')
     expect($.fn.next).toHaveBeenCalled()

     console.log(testee)
     });*/

  });
}());
