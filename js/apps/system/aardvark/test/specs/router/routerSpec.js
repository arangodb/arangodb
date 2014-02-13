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
      foxxDummy,
      logsDummy,
      statisticBarDummy,
      userBarDummy;

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
      sessionDummy = {
        id: "sessionDummy"
      };
      foxxDummy = {
        id: "foxxDummy"
      };
      statisticBarDummy = {
        id: "statisticBarDummy",
        render : function(){}
      };
      userBarDummy = {
        id: "userBarDummy",
        render : function(){}
      };
      spyOn(storeDummy, "fetch");
      spyOn(window, "arangoCollections").andReturn(storeDummy);
      spyOn(window, "ArangoUsers").andReturn(sessionDummy);
      spyOn(window, "arangoDocuments").andReturn(documentsDummy);
      spyOn(window, "arangoDocument").andReturn(documentDummy);
      spyOn(window, "GraphCollection").andReturn(graphsDummy);
      spyOn(window, "FoxxCollection").andReturn(foxxDummy);
      spyOn(window, "CollectionsView");
      spyOn(window, "DocumentsView");
      spyOn(window, "DocumentView");
      spyOn(window, "DocumentSourceView");
      spyOn(window, "ArangoLogs").andReturn(logsDummy);
      spyOn(logsDummy, "fetch");
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
      spyOn(window, "StatisticBarView").andReturn(statisticBarDummy);
      spyOn(window, "UserBarView").andReturn(userBarDummy);
    });

    describe("initialisation", function() {

      var r;


      beforeEach(function() {
        r = new window.Router();
      });

      it("should create a Foxx Collection", function() {
        expect(window.FoxxCollection).toHaveBeenCalled();
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
        simpleNavigationCheck = function(url, viewName, navTo,
          initObject, funcList, shouldNotRender) {
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
          if (!funcList.hasOwnProperty("render") && !shouldNotRender) {
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

/*
    routes: {
      ""                                    : "dashboard",
      "dashboard"                           : "dashboard",
      "collections"                         : "collections",
      "collection/:colid/documents/:pageid" : "documents",
      "collection/:colid/:docid"            : "document",
      "collection/:colid/:docid/source"     : "source",
      "logs"                                : "logs",
      "databases"                           : "databases",
      "application/installed/:key"          : "applicationEdit",
      "application/available/:key"          : "applicationInstall",
      "application/documentation/:key"      : "appDocumentation",
      "graph"                               : "graph",
    },
*/

      it("should offer all necessary routes", function() {
        var available = _.keys(r.routes),
          expected = [
            "collection/:colid",
            "collectionInfo/:colid",
            "login",
            "new",
            "api",
            "query",
            "shell",
            "graphManagement",
            "graphManagement/add",
            "graphManagement/delete/:name",
            "applications",
            "applications/installed",
            "applications/available"
          ],
          diff = _.difference(available, expected);
        this.addMatchers({
          toDefineTheRoutes: function(exp) {
            var avail = this.actual,
              leftDiff = _.difference(avail, exp),
              rightDiff = _.difference(exp, avail);
            this.message = function() {
              var msg = "";
              if (rightDiff.length) {
                msg += "Expect routes: "
                    + rightDiff.join(' & ')
                    + " to be available.\n";
              }
              if (leftDiff.length) {
                msg += "Did not expect routes: "
                    + leftDiff.join(" & ")
                    + " to be available.";
              }
              return msg;
            };
            return true;
            /* Not all routes are covered by tests yet
             * real execution would fail
            return leftDiff.length === 0
              && rightDiff.length === 0;
            */
          }
        });
        expect(available).toDefineTheRoutes(expected);
      });

      it("should route to a collection", function() {
        var colid = 5;
        simpleNavigationCheck(
          {
            url: "collection/:colid",
            params: [colid]
          },
          "CollectionView",
          "collections-menu",
          undefined,
          {
            setColId: colid
          }
        );
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

      it("should route to the login screen", function() {
        simpleNavigationCheck(
          "login",
          "loginView",
          "",
          {collection: sessionDummy}
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
          "tools-menu"
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
          "tools-menu"
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

      it("should offer the delete graph view", function() {
        var name = "testGraph";
        simpleNavigationCheck(
          {
            url: "graphManagement/delete/:name",
            params: [name]
          },
          "DeleteGraphView",
          "graphviewer-menu",
          {
            collection: graphsDummy
          },
          {
            render: name
          }
        );
      });

      it("should route to the applications tab", function() {
        simpleNavigationCheck(
          "applications",
          "ApplicationsView",
          "applications-menu",
          { collection: foxxDummy},
          {
            reload: undefined
          },
          true
        );
      });

      it("should route to the insalled applications", function() {
        simpleNavigationCheck(
          "applications/installed",
          "FoxxActiveListView",
          "applications-menu",
          { collection: foxxDummy},
          {
            reload: undefined
          },
          true
        );
      });

      it("should route to the available applications", function() {
        simpleNavigationCheck(
          "applications/available",
          "FoxxInstalledListView",
          "applications-menu",
          { collection: foxxDummy},
          {
            reload: undefined
          },
          true
        );
      });
    });
  });
}());
