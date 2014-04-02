/*jslint indent: 2, nomen: true, maxlen: 100, white: true  plusplus: true, browser: true*/
/*global describe, beforeEach, afterEach, it, */
/*global spyOn, runs, expect, waitsFor, jasmine*/
/*global AddNewGraphView, _, $, arangoHelper */

(function() {
  "use strict";

  describe("The Modal view Singleton", function() {
    
    var testee, div;

    beforeEach(function() {
      div = document.createElement("div");
      div.id = "modalPlaceholder";
      document.body.appendChild(div);
      testee = new window.ModalView();
    });

    afterEach(function() {
      document.body.removeChild(div);
    });

    describe("buttons", function() {

      var testShow;

      beforeEach(function() {
        testShow = testee.show.bind(testee, "dummy.ejs", "My Modal");
      });

      it("should create all button types", function() {
        expect(testee.buttons.SUCCESS).toEqual("success");
        expect(testee.buttons.NOTIFICATION).toEqual("notification");
        expect(testee.buttons.NEUTRAL).toEqual("neutral");
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
    });

  });
}());
