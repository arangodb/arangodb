/*jslint indent: 2, nomen: true, maxlen: 100, white: true  plusplus: true, browser: true*/
/*global describe, beforeEach, afterEach, it, spyOn, expect*/
/*global $, jasmine, _*/

(function() {
  "use strict";

  describe("The router", function() {
  
    var 
      jQueryDummy,
      fakeDB,
      graphDummy,
      storeDummy,
      naviDummy,
      footerDummy,
      documentDummy,
      documentsDummy,
      sessionDummy,
      graphsDummy,
      logsDummy;

    // Spy on all views that are initialized by startup
    beforeEach(function() {
      naviDummy = {
        id: "navi",
        render: function(){},
        handleResize: function(){},
        selectMenuItem: function(){}
      };
      footerDummy = {
        id: "footer",
        render: function(){},
        handleResize: function(){}
      };
      documentDummy = {id: "document"};
      documentsDummy = {id: "documents"};
      graphDummy = {
        id: "graph",
        handleResize: function() {}
      };
      storeDummy = {
        id: "store",
        fetch: function(){}
      };
      graphsDummy = {id: "graphs"};
      logsDummy = {
        id: "logs",
        fetch: function() {}
      };
      fakeDB = {
        id: "fakeDB",
        fetch: function() {}
      };
      jQueryDummy = {
        id: "jquery",
        resize: function() {},
        width: function() {},
        height: function() {},
        css: function() {}
      };
      spyOn(storeDummy, "fetch");
      spyOn(window, "arangoCollections").andReturn(storeDummy);
      spyOn(window, "ArangoSession").andReturn(sessionDummy);
      spyOn(window, "arangoDocuments").andReturn(documentsDummy);
      spyOn(window, "arangoDocument").andReturn(documentDummy);
      spyOn(window, "GraphCollection").andReturn(graphsDummy);
      spyOn(window, "CollectionsView");
      spyOn(window, "CollectionView");
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
      spyOn(naviDummy, "selectMenuItem");
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
        expect(options.async).toBeFalsy(); 
      });
      spyOn(window, "DBSelectionView");
    });

    describe("initialisation", function() {

      var r;

      beforeEach(function() {
        r = new window.Router();
      });
      
      it("should bind a resize event", function() {
        expect($(window).resize).toHaveBeenCalledWith(jasmine.any(Function));    
      });

      it("should create a graph view", function() {
        expect(window.GraphView).toHaveBeenCalledWith({
          collection: storeDummy,
          graphs: graphsDummy
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

    describe("navigation", function () {
      
      var r,
        simpleNavigationCheck;

      beforeEach(function() {
        r = new window.Router();
        simpleNavigationCheck = function(url, viewName, navTo, initObject, funcList) {
          var route,
            view = {},
            checkFuncExec = function() {
              _.each(funcList, function(v, f) {
                if (v !== undefined) {
                  expect(view[f]).toHaveBeenCalledWith(v);
                } else {
                  expect(view[f]).toHaveBeenCalled();
                }
              });
            };
          funcList = funcList || {};
          if (_.isObject(url)) {
            route = function() {
              r[r.routes[url.url]].apply(r, url.params);
            };
          } else {
            route = r[r.routes[url]].bind(r);
          }
          if (!funcList.hasOwnProperty("render")) {
            funcList.render = undefined;
          }
          _.each(_.keys(funcList), function(f) {
            view[f] = function(){};
            spyOn(view, f);
          });
          expect(route).toBeDefined();
          spyOn(window, viewName).andReturn(view);
          route();
          if (initObject) {
            expect(window[viewName]).toHaveBeenCalledWith(initObject);
          } else {
            expect(window[viewName]).toHaveBeenCalled();
          }
          checkFuncExec();
          expect(naviDummy.selectMenuItem).toHaveBeenCalledWith(navTo);
          // Check if the view is loaded from cache
          window[viewName].reset();
          _.each(_.keys(funcList), function(f) {
            view[f].reset();
          });
          naviDummy.selectMenuItem.reset();
          route();
          expect(window[viewName]).not.toHaveBeenCalled();
          checkFuncExec();
        };
      });

      it("should route to collection info", function() {
        var colid = 5;
        simpleNavigationCheck(
          {
            url: "collectionInfo/:colid",
            params: [colid]
          },
          "CollectionInfoView",
          "collections-menu",
          undefined,
          {
            setColId: colid
          }
        );
      });
      
      it("should route to the graph management tab", function() {
        simpleNavigationCheck(
          "new",
          "newCollectionView",
          "collections-menu",
          {}
        );
      });

      it("should route to the api tab", function() {
        simpleNavigationCheck(
          "api",
          "apiView",
          "api-menu"
        );
      });

      it("should route to the query tab", function() {
        simpleNavigationCheck(
          "query",
          "queryView",
          "query-menu"
        );
      });

      it("should route to the shell tab", function() {
        simpleNavigationCheck(
          "shell",
          "shellView",
          "shell-menu"
        );
      });

      it("should route to the graph management tab", function() {
        simpleNavigationCheck(
          "graphManagement",
          "GraphManagementView",
          "graphviewer-menu",
          { collection: graphsDummy}
        );
      });

      it("should offer the add new graph view", function() {
        simpleNavigationCheck(
          "graphManagement/add",
          "AddNewGraphView",
          "graphviewer-menu",
          {
            collection: storeDummy,
            graphs: graphsDummy
          }
        );
      });

    });
  });
}());
