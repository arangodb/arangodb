/*jslint indent: 2, nomen: true, maxlen: 100, white: true  plusplus: true, browser: true*/
/*global describe, beforeEach, afterEach, it, spyOn, expect*/
/*global $*/

(function() {
  "use strict";

  describe("The navigation bar", function() {

    var div, view, currentDBFake, curName, isSystem;

    beforeEach(function() {
      curName = "_system";
      isSystem = true;
      window.currentDB = window.currentDB || {
        get: function() {}
      };
      spyOn(window.currentDB, "get").andCallFake(function(key) {
        if (key === "name") {
          return curName;
        }
        if (key === "isSystem") {
          return isSystem;
        }
        expect(true).toBeFalsy();
      });
      div = document.createElement("div");
      div.id = "navigationBar";
      document.body.appendChild(div);
    });

    afterEach(function() {
      document.body.removeChild(div);
    });

    describe("in any database", function() {

      beforeEach(function() {
        view = new window.NavigationView();
        view.render();
      });

      it("should offer a dashboard tab", function() {
        var tab = $("#dashboard", $(div));
        expect(tab.length).toEqual(1);
        spyOn(window.App, "navigate");
        tab.click();
        expect(window.App.navigate).toHaveBeenCalledWith("dashboard", {trigger: true});
      });

      it("should offer a collections tab", function() {
        var tab = $("#collections", $(div));
        expect(tab.length).toEqual(1);
        spyOn(window.App, "navigate");
        tab.click();
        expect(window.App.navigate).toHaveBeenCalledWith("collections", {trigger: true});
      });

      it("should offer a applications tab", function() {
        var tab = $("#applications", $(div));
        expect(tab.length).toEqual(1);
        spyOn(window.App, "navigate");
        tab.click();
        expect(window.App.navigate).toHaveBeenCalledWith("applications", {trigger: true});
      });

      it("should offer a graph tab", function() {
        var tab = $("#graph", $(div));
        expect(tab.length).toEqual(1);
        spyOn(window.App, "navigate");
        tab.click();
        expect(window.App.navigate).toHaveBeenCalledWith("graph", {trigger: true});
      });

      it("should offer an aql editor tab", function() {
        var tab = $("#query", $(div));
        expect(tab.length).toEqual(1);
        spyOn(window.App, "navigate");
        tab.click();
        expect(window.App.navigate).toHaveBeenCalledWith("query", {trigger: true});
      });

      it("should offer an api tab", function() {
        var tab = $("#api", $(div));
        expect(tab.length).toEqual(1);
        spyOn(window.App, "navigate");
        tab.click();
        expect(window.App.navigate).toHaveBeenCalledWith("api", {trigger: true});
      });


      it("should offer a graph tab", function() {
        var tab = $("#graph", $(div));
        expect(tab.length).toEqual(1);
        spyOn(window.App, "navigate");
        tab.click();
        expect(window.App.navigate).toHaveBeenCalledWith("graph", {trigger: true});
      });

    });

    describe("in _system database", function() {

      beforeEach(function() {
        view = new window.NavigationView();
        view.render();
      });

      it("should offer a databases tab", function() {
        var tab = $("#databases", $(div));
        expect(tab.length).toEqual(1);
        spyOn(window.App, "navigate");
        tab.click();
        expect(window.App.navigate).toHaveBeenCalledWith("databases", {trigger: true});
      });

      it("should offer a logs tab", function() {
        var tab = $("#logs", $(div));
        expect(tab.length).toEqual(1);
        spyOn(window.App, "navigate");
        tab.click();
        expect(window.App.navigate).toHaveBeenCalledWith("logs", {trigger: true});
      });
    });

    describe("in a not _system database", function() {

      beforeEach(function() {
        curName = "firstDB";
        isSystem = false;
        view = new window.NavigationView();
        view.render();
      });
      
      it("should not offer a databases tab", function() {
        var tab = $("#databases", $(div));
        expect(tab.length).toEqual(0);
      });

      it("should not offer a logs tab", function() {
        var tab = $("#logs", $(div));
        expect(tab.length).toEqual(0);
      });
    });

  });

}());
