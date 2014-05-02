/*jslint indent: 2, nomen: true, maxlen: 100, white: true  plusplus: true, browser: true*/
/*global describe, beforeEach, afterEach, it, spyOn, expect*/
/*global $*/

(function() {
  "use strict";

  describe("The navigation bar", function() {

    var div, view, currentDBFake, curName, isSystem,
      DBSelectionViewDummy, StatisticBarViewDummy, UserBarViewDummy,
      NotificationViewDummy, UserCollectionDummy, oldRouter;

    beforeEach(function() {
      curName = "_system";
      isSystem = true;
      oldRouter = window.App;
      window.App = {navigate : function(){}};

      window.currentDB = window.currentDB || {
        get: function() {}
      };

      DBSelectionViewDummy = {
        id : "DBSelectionViewDummy",
        render : function(){}
      };

      UserBarViewDummy = {
        id : "UserBarViewDummy",
        render : function(){}
      };

      StatisticBarViewDummy = {
        id : "StatisticBarViewDummy",
        render : function(){}
      };

      NotificationViewDummy = {
        id : "NotificationViewDummy",
        render : function(){}
      };

      UserCollectionDummy = {
        id      : "UserCollectionDummy",
        whoAmI  : function () {return "root";}
      };



      spyOn(window, "UserBarView").andReturn(UserBarViewDummy);
      spyOn(window, "StatisticBarView").andReturn(StatisticBarViewDummy);
      spyOn(window, "DBSelectionView").andReturn(DBSelectionViewDummy);
      spyOn(window, "NotificationView").andReturn(NotificationViewDummy);
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
      window.App = oldRouter;
    });

    describe("in any database", function() {

      beforeEach(function() {
        view = new window.NavigationView({userCollection : UserCollectionDummy});
        view.render();
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
        view = new window.NavigationView({userCollection : UserCollectionDummy});
        view.render();
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
        view = new window.NavigationView({userCollection : UserCollectionDummy});
        view.render();
      });
      
      it("should not offer a logs tab", function() {
        var tab = $("#logs", $(div));
        expect(tab.length).toEqual(0);
      });
    });

  });

}());
