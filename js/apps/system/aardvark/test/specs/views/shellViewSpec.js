/*jslint indent: 2, nomen: true, maxlen: 100, white: true  plusplus: true, browser: true*/
/*global describe, beforeEach, afterEach, it, spyOn, expect*/
/*global arangoHelper*/

/*
(function() {
  "use strict";

  describe("The shell view", function() {

    var view, div;

    beforeEach(function() {
      div = document.createElement("div");
      div.id = "content";
      document.body.appendChild(div);

      view = new window.shellView({
      });

      window.height = function() {
        return 400;
      }
      spyOn(window, "height");
      view.render();
    });

    afterEach(function() {
      document.body.removeChild(div);
    });

    it("assert the basics", function () {
      expect(view.resizing).toEqual(false);
    });

    /*
    it("shortcut Ctrl+Z", function () {
      spyOn(view, "resize").andCallFake({
      });
      var press = jQuery.Event("keypress");
      press.ctrlKey = true;
      press.which = 90;
      //spyOn(window, "jqconsole");
      spyOn(jqconsole, "AbortPrompt");
      $("#replShell").focus();
      $("#replShell").trigger(press);
    });*/


 // });
//}()); 
