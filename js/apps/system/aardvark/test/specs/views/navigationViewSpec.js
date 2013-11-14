/*jslint indent: 2, nomen: true, maxlen: 100, white: true  plusplus: true, browser: true*/
/*global describe, beforeEach, afterEach, it, spyOn, expect*/

(function() {
  "use strict";

  describe("The navigation bar", function() {

    var div, view, dbSelectionFake;

    beforeEach(function() {
      div = document.createElement("div");
      div.className = "header";
      document.body.appendChild(div);
      dbSelectionFake = {
        render: function() {}
      };
      spyOn(window, "DBSelectionView").andReturn(dbSelectionFake);
      view = new window.NavigationView();
      view.render();
    });

    afterEach(function() {
      document.body.removeChild(div);
    });

    it("should render the database selection view", function() {
      spyOn(dbSelectionFake, "render");
      view.render();
      expect(dbSelectionFake.render).toHaveBeenCalledWith($("#selectDB"));
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

    /*
     * TODO _system only tabs Databases and Logs
     */
    describe("in _system database", function() {


      
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

  });

}());
