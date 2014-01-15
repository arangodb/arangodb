/*jslint indent: 2, nomen: true, maxlen: 100, white: true  plusplus: true, browser: true*/
/*global describe, beforeEach, afterEach, it, */
/*global spyOn, expect*/
/*global templateEngine, $, _*/
(function() {
  "use strict";

  describe("Cluster Server View", function() {
    var view, div, dbView;

    beforeEach(function() {
      div = document.createElement("div");
      div.id = "clusterServers"; 
      document.body.appendChild(div);
      dbView = {
        render: function(){}
      };
      spyOn(window, "ClusterDatabaseView").andReturn(dbView);
    });

    afterEach(function() {
      document.body.removeChild(div);
    });

    describe("initialisation", function() {

      it("should create a Cluster Database View", function() {
        view = new window.ClusterServerView();
        expect(window.ClusterDatabaseView).toHaveBeenCalled();
      });

    });

    describe("rendering", function() {

      var okPair, noBkp, deadBkp, deadPrim, deadPair, fakeServers,
        getTile = function(name) {
          return document.getElementById(name);
        },

        checkUserActions = function() {
          describe("user actions", function() {
            var info;

            it("should be able to navigate to each server", function() {
              spyOn(view, "render");
              _.each(fakeServers, function(o) {
                dbView.render.reset();
                view.render.reset();
                var name = o.primary.name;
                info = {
                  name: name
                };
                $(getTile(name)).click();
                expect(dbView.render).toHaveBeenCalledWith(info);
                expect(view.render).toHaveBeenCalledWith(true);
              });
            });

          });
        };

      beforeEach(function() {
        spyOn(dbView, "render");
        view = new window.ClusterServerView();
        okPair = {
          primary: {
            name: "Pavel",
            url: "tcp://192.168.0.1:1337",
            status: "ok"
          },
          secondary: {
            name: "Sally",
            url: "tcp://192.168.1.1:1337",
            status: "ok"
          }
        };
        noBkp = {
          primary: {
            name: "Pancho",
            url: "tcp://192.168.0.2:1337",
            status: "ok"
          }
        };
        deadBkp = {
          primary: {
            name: "Pepe",
            url: "tcp://192.168.0.3:1337",
            status: "ok"
          },
          secondary: {
            name: "Sam",
            url: "tcp://192.168.1.3:1337",
            status: "critical"
          }
        };
        deadPrim = {
          primary: {
            name: "Pedro",
            url: "tcp://192.168.0.4:1337",
            status: "critical"
          },
          secondary: {
            name: "Sabrina",
            url: "tcp://192.168.1.4:1337",
            status: "ok"
          }
        };
        deadPair = {
          primary: {
            name: "Pablo",
            url: "tcp://192.168.0.5:1337",
            status: "critical"
          },
          secondary: {
            name: "Sandy",
            url: "tcp://192.168.1.5:1337",
            status: "critical"
          }
        };
        fakeServers = [
          okPair,
          noBkp,
          deadBkp,
          deadPrim
        ];
        view.fakeData = fakeServers;
        view.render();
      });

      it("should not render the Database view", function() {
        expect(dbView.render).not.toHaveBeenCalled();
      });
      
      describe("minified version", function() {

        beforeEach(function() {
          view.render(true);
        });

        it("should render all pairs", function() {
          expect($("button", $(div)).length).toEqual(4);
        });
        
        // TODO
        
        checkUserActions();
      });

      describe("maximised version", function() {

        beforeEach(function() {
          view.render();
        });

        it("should render all pairs", function() {
          expect($("> div.domino", $(div)).length).toEqual(4);
        });

        it("should render the good pair", function() {
          var tile = getTile(okPair.primary.name);
        });

        checkUserActions();

      });

    });

  });

}());
