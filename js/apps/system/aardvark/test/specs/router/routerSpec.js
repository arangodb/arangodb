/*jslint indent: 2, nomen: true, maxlen: 100, white: true  plusplus: true, browser: true*/
/*global describe, beforeEach, afterEach, it, spyOn, expect*/
/*global $, jasmine*/

(function() {
  "use strict";

  describe("The router", function() {
  
    var 
      jQueryDummy,
      fakeDB,
      storeDummy,
      naviDummy,
      footerDummy,
      documentDummy,
      documentsDummy,
      databaseDummy,
      sessionDummy,
      graphDummy,
      logsDummy;

    // Spy on all views that are initialized by startup
    beforeEach(function() {
      naviDummy = {
        render: function(){},
        handleResize: function(){}
      };
      footerDummy = {
        render: function(){},
        handleResize: function(){}
      };
      documentDummy = {};
      documentsDummy = {};
      graphDummy = {
        handleResize: function() {}
      };
      storeDummy = {fetch: function(){}};
      logsDummy = {fetch: function() {}};
      fakeDB = {fetch: function() {}};
      jQueryDummy = {
        resize: function() {},
        width: function() {},
        height: function() {},
        css: function() {}
      };
      spyOn(storeDummy, "fetch");
      spyOn(window, "arangoCollections").andReturn(storeDummy);
      spyOn(window, "ArangoSession").andReturn(sessionDummy);
      spyOn(window, "ArangoDatabase").andReturn(databaseDummy);
      spyOn(window, "arangoDocuments").andReturn(documentsDummy);
      spyOn(window, "arangoDocument").andReturn(documentDummy);
      spyOn(window, "CollectionsView");
      spyOn(window, "CollectionView");
      spyOn(window, "CollectionInfoView");
      spyOn(window, "DocumentsView");
      spyOn(window, "DocumentView");
      spyOn(window, "DocumentSourceView");
      spyOn(window, "ArangoLogs").andReturn(logsDummy);
      spyOn(logsDummy, "fetch"); //TO_DO
      /*
        success: function () {
          window.logsView = new window.logsView({
            collection: window.arangoLogsStore
          });
        }
      */
      spyOn(window, "FooterView").andReturn(footerDummy);
      spyOn(footerDummy, "render");
      spyOn(window, "NavigationView").andReturn(naviDummy);
      spyOn(naviDummy, "render");
      spyOn(window, "GraphView").andReturn(graphDummy);
      spyOn(window, "$").andReturn(jQueryDummy);
      spyOn(jQueryDummy, "resize").andCallFake(function() {
        return jQueryDummy;
      });
      spyOn(jQueryDummy, "width").andCallFake(function() {
        return jQueryDummy;
      });
      spyOn(jQueryDummy, "height").andCallFake(function() {
        return jQueryDummy;
      });
      spyOn(jQueryDummy, "css").andCallFake(function() {
        return jQueryDummy;
      });
      spyOn(window, "CurrentDatabase").andReturn(fakeDB);
      spyOn(fakeDB, "fetch").andCallFake(function(options) {
        expect(options.async).toBeTruthy(); 
      });
    });

    describe("Initialisation", function() {

      var r;

      beforeEach(function() {
        r = new window.Router();
      });
      
      it("should bind a resize event", function() {
        expect($(window).resize).toHaveBeenCalledWith(jasmine.any(Function));    
      });

      it("should create a graph view", function() {
        expect(window.GraphView).toHaveBeenCalledWith({
          collection: storeDummy
        });
      });

      it("should create a documentView", function() {
        expect(window.DocumentView).toHaveBeenCalledWith();
      });

      it("should create a documentSourceView", function() {
        expect(window.DocumentSourceView).toHaveBeenCalledWith();
      });

      it("should create a documentsView", function() {
        expect(window.DocumentsView).toHaveBeenCalledWith();
      });

      it("should create collectionInfoView", function() {
        expect(window.CollectionInfoView).toHaveBeenCalledWith();
      });

      it("should create collectionView", function() {
        expect(window.CollectionView).toHaveBeenCalledWith();
      });

      it("should create collectionsView", function() {
        expect(window.CollectionsView).toHaveBeenCalledWith({
          collection: storeDummy
        });
      });

      it("should fetch the collectionsStore", function() {
        expect(storeDummy.fetch).toHaveBeenCalled();
      });

      it("should fetch the current db", function() {
        expect(window.CurrentDatabase).toHaveBeenCalled();
        expect(fakeDB.fetch).toHaveBeenCalled();
      });
    });
  });
}());
