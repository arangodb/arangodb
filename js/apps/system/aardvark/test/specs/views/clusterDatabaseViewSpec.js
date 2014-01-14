/*jslint indent: 2, nomen: true, maxlen: 100, white: true  plusplus: true, browser: true*/
/*global describe, beforeEach, afterEach, it, */
/*global spyOn, expect*/
/*global templateEngine, $*/
(function() {
  "use strict";

  describe("Cluster Database View", function() {
    var view, div, colView;

    beforeEach(function() {
      div = document.createElement("div");
      div.id = "clusterDatabases"; 
      document.body.appendChild(div);
      colView = {
        render: function(){}
      };
      spyOn(window, "ClusterCollectionView").andReturn(colView);
    });

    afterEach(function() {
      document.body.removeChild(div);
    });

    describe("initialisation", function() {

      it("should create a Cluster Collection View", function() {
        view = new window.ClusterDatabaseView();
        expect(window.ClusterCollectionView).toHaveBeenCalled();
      });

    });

    describe("rendering", function() {

      beforeEach(function() {
        spyOn(colView, "render");
        view = new window.ClusterDatabaseView();
      });

      it("should not render the Server view", function() {
        view.render();
        expect(colView.render).not.toHaveBeenCalled();
      });
    });

    describe("user actions", function() {
      var db;

      beforeEach(function() {
        spyOn(colView, "render");
        view = new window.ClusterDatabaseView();
        view.render();
      });

      it("should be able to navigate to _system", function() {
        db = "_system";
        $("#" + db).click();
        expect(colView.render).toHaveBeenCalledWith(db);
      });

      it("should be able to navigate to myDatabase", function() {
        db = "myDatabase";
        $("#" + db).click();
        expect(colView.render).toHaveBeenCalledWith(db);
      });

      it("should be able to navigate to otherDatabase", function() {
        db = "otherDatabase";
        $("#" + db).click();
        expect(colView.render).toHaveBeenCalledWith(db);
      });

    });

  });

}());

