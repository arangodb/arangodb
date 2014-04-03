/*jslint indent: 2, nomen: true, maxlen: 100, white: true  plusplus: true, browser: true*/
/*global describe, beforeEach, afterEach, it, */
/*global spyOn, runs, expect, waitsFor, jasmine*/
/*global _, $, uiMatchers */

(function() {
  "use strict";

  describe("The Modal view Singleton", function() {
    
    var testee, div;

    beforeEach(function() {
      div = document.createElement("div");
      div.id = "modalPlaceholder";
      document.body.appendChild(div);
      testee = new window.ModalView();
      uiMatchers.define(this);
    });

    afterEach(function() {
      document.body.removeChild(div);
    });

    describe("buttons", function() {

      var testShow;

      beforeEach(function() {
        testShow = testee.show.bind(testee, "modalTable.ejs", "My Modal");
      });

      it("should create all button types", function() {
        expect(testee.buttons.SUCCESS).toEqual("success");
        expect(testee.buttons.NOTIFICATION).toEqual("notification");
        expect(testee.buttons.NEUTRAL).toEqual("neutral");
        expect(testee.buttons.DELETE).toEqual("danger");
      });

      it("should automatically create the close button", function() {
        testShow();
        var btn = $(".button-close", $(div));
        expect(btn.length).toEqual(1);
        spyOn(testee, "hide").andCallThrough();
        btn.click();
        expect(testee.hide).toHaveBeenCalled();
      });

      it("should be able to bind a callback to a button", function() {
        var btnObj = {},
          title = "Save",
          buttons = [btnObj],
          cbs = {
            callback: function() {
              testee.hide();
            }
          },
          btn;

        spyOn(cbs, "callback").andCallThrough();
        btnObj.title = title;
        btnObj.type = testee.buttons.SUCCESS;
        btnObj.callback = cbs.callback;
        testShow(buttons);
        btn = $(".button-" + btnObj.type, $(div));
        expect(btn.length).toEqual(1);
        expect(btn.text()).toEqual(title);
        btn.click();
        expect(cbs.callback).toHaveBeenCalled();
      });

      it("should be able to create several buttons", function() {
        var btnObj1 = {},
          btnObj2 = {},
          btnObj3 = {},
          title = "Button_",
          buttons = [btnObj1, btnObj2, btnObj3],
          cbs = {
            cb1: function() {
              throw "Should never be invoked";
            },
            cb2: function() {
              throw "Should never be invoked";
            },
            cb3: function() {
              throw "Should never be invoked";
            }
          },
          btn;

        spyOn(cbs, "cb1");
        btnObj1.title = title + 1;
        btnObj1.type = testee.buttons.SUCCESS;
        btnObj1.callback = cbs.cb1;
        spyOn(cbs, "cb2");
        btnObj2.title = title + 2;
        btnObj2.type = testee.buttons.NEUTRAL;
        btnObj2.callback = cbs.cb2;
        spyOn(cbs, "cb3");
        btnObj3.title = title + 3;
        btnObj3.type = testee.buttons.NOTIFICATION;
        btnObj3.callback = cbs.cb3;
        testShow(buttons);
        btn = $("button[class*=button-]", $(div));
        //Three defined above + cancel
        expect(btn.length).toEqual(4);
        //Check click events for all buttons
        btn = $(".button-" + btnObj1.type, $(div));
        btn.click();
        expect(cbs.cb1).toHaveBeenCalled();
        btn = $(".button-" + btnObj2.type, $(div));
        btn.click();
        expect(cbs.cb2).toHaveBeenCalled();
        btn = $(".button-" + btnObj3.type, $(div));
        btn.click();
        expect(cbs.cb3).toHaveBeenCalled();
        testee.hide();
      });

      describe("should create a delete button", function() {
        var cbs;

        beforeEach(function() {
          var btnObj = {},
            title = "Delete",
            buttons = [btnObj],
            btn;

          cbs = {
            callback: function() {
              testee.hide();
            }
          };
          spyOn(cbs, "callback").andCallThrough();
          btnObj.title = title;
          btnObj.type = testee.buttons.DELETE;
          btnObj.callback = cbs.callback;
          testShow(buttons);
          btn = $(".button-" + btnObj.type, $(div));
          expect(btn.length).toEqual(1);
          expect(btn.text()).toEqual(title);
          btn.click();
          expect(cbs.callback).not.toHaveBeenCalled();
        });

        it("with confirm dialog", function() {
          expect($("#modal-delete-confirmation").css("display")).toEqual("block");
          $("#modal-confirm-delete", $(div)).click();
          expect(cbs.callback).toHaveBeenCalled();
        });

        it("should hide confirm dialog", function() {
          expect($("#modal-delete-confirmation").css("display")).toEqual("block");
          $("#modal-abort-delete", $(div)).click();
          expect(cbs.callback).not.toHaveBeenCalled();
          expect($("#modal-delete-confirmation").css("display")).toEqual("none");
        });

      });

    });

    describe("table content", function() {

      var testShow,
        tdSelector,
        readOnlyDef,
        textDef,
        pwDef,
        cbxDef,
        selectDef;

      beforeEach(function() {
        testShow = testee.show.bind(testee, "modalTable.ejs", "My Modal", null);
        tdSelector = ".modal-body table tr th";
        readOnlyDef = {
          type: testee.tables.READONLY,
          value: "Readonly text",
          label: "ReadOnly",
          info: "ReadOnly Description"
        };
        textDef = {
          type: testee.tables.TEXT,
          value: "Text value",
          label: "Text",
          info: "Text Description",
          id: "textId",
          placeholder: "Text Placeholder"
        };
        pwDef = {
          type: testee.tables.PASSWORD,
          value: "secret",
          label: "Password",
          info: "Password Description",
          id: "pwdId",
          placeholder: "PWD Placeholder"
        };
        cbxDef = {
          type: testee.tables.CHECKBOX,
          value: "myBox",
          label: "Checkbox",
          info: "Checkbox Description",
          checked: true,
          id: "cbxId"
        };
        selectDef = {
          type: testee.tables.SELECT,
          label: "Select",
          id: "selectId",
          selected: "v2",
          options: [
            {
              value: "v1",
              label: "l1"
            },
            {
              value: "v2",
              label: "l2"
            }
          ]
        };
      });

      it("should render readonly", function() {
        var content = [readOnlyDef],
          fields;

        testShow(content);
        fields = $(tdSelector);
        expect(fields.length).toEqual(3);
        expect($(fields[0]).text().trim()).toEqual(readOnlyDef.label + ":");
        expect($(fields[1]).text().trim()).toEqual(readOnlyDef.value);
        // expect($(fields[2]).text()).toEqual(lbl);
      });

      it("should render text-input", function() {
        var content = [textDef],
          fields,
          input;

        testShow(content);
        fields = $(tdSelector);
        expect($(fields[0]).text()).toEqual(textDef.label + ":");
        input = $("#" + textDef.id);
        expect(input[0]).toEqual($(fields[1]).find(" > input")[0]);
        expect(input[0]).toBeTag("input");
        expect(input.attr("type")).toEqual("text");
        expect(input.attr("placeholder")).toEqual(textDef.placeholder);
        expect(input.val()).toEqual(textDef.value);
        // expect($(fields[2]).text()).toEqual(lbl);
      });

      it("should render password-input", function() {
        var content = [pwDef],
          fields,
          input;

        testShow(content);
        fields = $(tdSelector);
        expect($(fields[0]).text()).toEqual(pwDef.label + ":");
        input = $("#" + pwDef.id);
        expect(input[0]).toEqual($(fields[1]).find(" > input")[0]);
        expect(input[0]).toBeTag("input");
        expect(input.attr("type")).toEqual("password");
        expect(input.attr("placeholder")).toEqual(pwDef.placeholder);
        expect(input.val()).toEqual(pwDef.value);
        // expect($(fields[2]).text()).toEqual(lbl);
      });

      it("should render checkbox", function() {
        var content = [cbxDef],
          fields,
          cbx;

        testShow(content);
        fields = $(tdSelector);
        expect($(fields[0]).text()).toEqual(cbxDef.label + ":");
        cbx = $("#" + cbxDef.id);
        expect(cbx[0]).toEqual($(fields[1]).find(" > input")[0]);
        expect(cbx.attr("type")).toEqual("checkbox");

        // expect($(fields[2]).text()).toEqual(lbl);
      });

      it("should render select", function() {
        var content = [selectDef],
          fields,
          select;

        testShow(content);
        fields = $(tdSelector);
        expect($(fields[0]).text()).toEqual(selectDef.label + ":");
        select = $("#" + selectDef.id);
        expect(select[0]).toEqual($(fields[1]).find(" > select")[0]);
        _.each(select.children(), function(o, i) {
          var opt = selectDef.options[i];
          expect($(o).attr("value")).toEqual(opt.value);
          expect($(o).text()).toEqual(opt.label);
        });
        expect(select.val()).toEqual(selectDef.selected);

        // expect($(fields[2]).text()).toEqual(lbl);
      });

      it("should render several types in a mix", function() {
        var content = [
            readOnlyDef,
            textDef,
            pwDef,
            cbxDef,
            selectDef
          ],
          rows;

        rows = $(".modal-body table tr");
        _.each(rows, function(v, k) {
          expect($("td:first-child", $(v)).text()).toEqual(content[k].label + ":");
        });
        

      });

      
    });

  });
}());
