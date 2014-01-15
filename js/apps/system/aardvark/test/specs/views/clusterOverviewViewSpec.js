/*jslint indent: 2, nomen: true, maxlen: 100, white: true  plusplus: true, browser: true*/
/*global describe, beforeEach, afterEach, it, */
/*global spyOn, expect*/
/*global templateEngine, $, _*/
(function() {
  "use strict";

  describe("Cluster Overview View", function() {
    var view, div, serverView;

    beforeEach(function() {
      div = document.createElement("div");
      div.id = "clusterOverview"; 
      document.body.appendChild(div);
      serverView = {
        render: function(){}
      };
      spyOn(window, "ClusterServerView").andReturn(serverView);
      this.addMatchers({

        toBeTag: function(name) {
          var el = this.actual;
          this.message = function() {
            return "Expected " + el.tagName.toLowerCase() + " to be a " + name; 
          };
          return el.tagName.toLowerCase() === name;
        },
        
        toBeOfClass: function(name) {
          var el = $(this.actual);
          this.message = function() {
            return "Expected \"" + el.attr("class") + "\" to contain " + name; 
          };
          return el.hasClass(name);
        },

        toNotHaveClass: function(name) {
          var el = $(this.actual);
          this.message = function() {
            return "Expected \"" + el.attr("class") + "\" to not contain " + name; 
          };
          return !el.hasClass(name);
        }

      });
    });

    afterEach(function() {
      document.body.removeChild(div);
    });

    describe("initialisation", function() {

      it("should create a Cluster Server View", function() {
        view = new window.ClusterOverviewView();
        expect(window.ClusterServerView).toHaveBeenCalled();
      });

    });

    describe("rendering", function() {

      var checkUserActions = function() {
        describe("user actions", function() {
          var info;

          beforeEach(function() {
            serverView.render.reset();
            view.render();
            spyOn(view, "render");
          });

          it("should be able to navigate to db servers", function() {
            info = {
              type: "dbservers"
            };
            $("#dbserver").click();
            expect(serverView.render).toHaveBeenCalledWith(info);
            expect(view.render).toHaveBeenCalledWith(true);
          });

          it("should be able to navigate to primary servers", function() {
            info = {
              type: "coordinator"
            };
            $("#coordinator").click();
            expect(serverView.render).toHaveBeenCalledWith(info);
            expect(view.render).toHaveBeenCalledWith(true);
          });

        });
      },

      checkShowStatus = function (btn, status) {
        var classNames = {
          ok: "btn-success",
          warning: "btn-warning",
          critical: "btn-danger"
        };
        expect(btn).toBeOfClass(classNames[status]);
        delete classNames[status];
        _.each(classNames, function(v) {
          expect(btn).toNotHaveClass(v);
        });
      };

      beforeEach(function() {
        spyOn(serverView, "render");
        view = new window.ClusterOverviewView();
        // Fake Data Injection to be removed
        view.fakeData = {
          dbservers: {
            plan: 3,
            having: 3,
            status: "warning"
          },
          coordinators: {
            plan: 3,
            having: 2,
            status: "critical"
          }
        };
      });

      describe("minified version", function() {

        beforeEach(function() {
          view.render(true);
        });

        it("should not render the Server view", function() {
          expect(serverView.render).not.toHaveBeenCalled();
        });

        it("should render in minified version", function() {
          expect($(div).hasClass("clusterColumnMax")).toBeFalsy();
        });
  
        describe("dbservers" , function() {

          var getButton = function() {
            return document.getElementById("dbserver");
          };

          it("should render the amounts", function() {
            view.fakeData.dbservers.plan = 5;
            view.fakeData.dbservers.having = 4;
            view.render(true);
            var btn = getButton(),
                span = btn.firstChild,
                txt = $(span).text();
            expect(btn).toBeDefined();
            expect(span).toBeDefined();
            expect(span).toBeOfClass("arangoicon");
            expect(span).toBeOfClass("icon_arangodb_database1");
            expect(span).toBeOfClass("cluster_icon_small");
            expect(txt.trim()).toEqual("4/5");
          });

          it("should render the status ok", function() {
            var status = "ok";
            view.fakeData.dbservers.status = status;
            view.render(true);
            checkShowStatus(getButton(), status);
          });

          it("should render the status warning", function() {
            var status = "warning";
            view.fakeData.dbservers.status = status;
            view.render(true);
            checkShowStatus(getButton(), status);
          });

          it("should render the status critical", function() {
            var status = "critical";
            view.fakeData.dbservers.status = status;
            view.render(true);
            checkShowStatus(getButton(), status);
          });

        });

        describe("coordinators" , function() {

          var getButton = function() {
            return document.getElementById("coordinator");
          };

          it("should render the amounts", function() {
            view.fakeData.coordinators.plan = 5;
            view.fakeData.coordinators.having = 4;
            view.render(true);
            var btn = getButton(),
                span = btn.firstChild,
                txt = $(span).text();
            expect(btn).toBeDefined();
            expect(span).toBeDefined();
            expect(span).toBeOfClass("arangoicon");
            expect(span).toBeOfClass("icon_arangodb_compass");
            expect(span).toBeOfClass("cluster_icon_small");
            expect(txt.trim()).toEqual("4/5");
          });

          it("should render the status ok", function() {
            var status = "ok";
            view.fakeData.coordinators.status = status;
            view.render(true);
            checkShowStatus(getButton(), status);
          });

          it("should render the status warning", function() {
            var status = "warning";
            view.fakeData.coordinators.status = status;
            view.render(true);
            checkShowStatus(getButton(), status);
          });

          it("should render the status critical", function() {
            var status = "critical";
            view.fakeData.coordinators.status = status;
            view.render(true);
            checkShowStatus(getButton(), status);
          });

        });

        checkUserActions();

      });

      describe("maximised version", function() {

        beforeEach(function() {
          view.render();
        });

        it("should not render the Server view", function() {
          expect(serverView.render).not.toHaveBeenCalled();
        });

        it("should increase the size of the column to maximum", function() {
          expect($(div).hasClass("clusterColumnMax")).toBeTruthy();
        });

        describe("dbservers" , function() {

          var getTile = function() {
            return document.getElementById("dbserver");
          };

          it("should render the tile", function() {
            view.fakeData.dbservers.plan = 5;
            view.fakeData.dbservers.having = 4;
            view.render();
            var tile = getTile(),
                spans = $("> span", $(tile)),
                header = spans[0],
                icon = spans[1],
                footer = $("> div", $(tile)),
                htxt = $(header).text(),
                ftxt = $(footer).text();
            expect(tile).toBeDefined();
            expect(tile).toBeOfClass("tile");
            expect(tile).toBeOfClass("tile-left");
            expect(icon).toBeDefined();
            expect(icon).toBeOfClass("arangoicon");
            expect(icon).toBeOfClass("icon_arangodb_database1");
            expect(icon).toBeOfClass("cluster_icon_large");
            expect(htxt.trim()).toEqual("Data Servers");
            expect(ftxt.trim()).toEqual("4/5");
          });

          it("should render the status ok", function() {
            var status = "ok";
            view.fakeData.dbservers.status = status;
            view.render();
            checkShowStatus(getTile(), status);
          });

          it("should render the status warning", function() {
            var status = "warning";
            view.fakeData.dbservers.status = status;
            view.render();
            checkShowStatus(getTile(), status);
          });

          it("should render the status critical", function() {
            var status = "critical";
            view.fakeData.dbservers.status = status;
            view.render();
            checkShowStatus(getTile(), status);
          });

        });

        describe("coordinators" , function() {

          var getTile = function() {
            return document.getElementById("coordinator");
          };

          it("should render the tile", function() {
            view.fakeData.coordinators.plan = 5;
            view.fakeData.coordinators.having = 4;
            view.render();
            var tile = getTile(),
                spans = $("> span", $(tile)),
                header = spans[0],
                icon = spans[1],
                footer = $("> div", $(tile)),
                htxt = $(header).text(),
                ftxt = $(footer).text();
            expect(tile).toBeDefined();
            expect(tile).toBeOfClass("tile");
            expect(tile).toBeOfClass("tile-right");
            expect(icon).toBeDefined();
            expect(icon).toBeOfClass("arangoicon");
            expect(icon).toBeOfClass("icon_arangodb_compass");
            expect(icon).toBeOfClass("cluster_icon_large");
            expect(htxt.trim()).toEqual("Coordinators");
            expect(ftxt.trim()).toEqual("4/5");
          });

          it("should render the status ok", function() {
            var status = "ok";
            view.fakeData.coordinators.status = status;
            view.render();
            checkShowStatus(getTile(), status);
          });

          it("should render the status warning", function() {
            var status = "warning";
            view.fakeData.coordinators.status = status;
            view.render();
            checkShowStatus(getTile(), status);
          });

          it("should render the status critical", function() {
            var status = "critical";
            view.fakeData.coordinators.status = status;
            view.render();
            checkShowStatus(getTile(), status);
          });

        });

        checkUserActions();

      });

    });

  });

}());

