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

      this.addMatchers({
        toBeOfClass: function(name) {
          var el = $(this.actual);
          this.message = function() {
            return "Expected \"" + el.attr("class") + "\" to contain " + name; 
          };
          return el.hasClass(name);
        }
      });

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
        },

        checkDominoLayout = function(tile) {
          var tileElements = tile.children,
              upper = tileElements[0],
              line = tileElements[1],
              lower = tileElements[2];
          expect(tile).toBeOfClass("domino");
          expect(tile).toBeOfClass("server");
          expect(upper).toBeOfClass("domino-upper");
          expect(line).toBeOfClass("domino-line");
          expect(lower).toBeOfClass("domino-lower");
          expect(upper.children[0]).toBeOfClass("domino-header");
          expect(lower.children[0]).toBeOfClass("domino-header");
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

        var checkButtonContent = function(btn, pair, cls) {
          expect(btn).toBeOfClass("btn");
          expect(btn).toBeOfClass("server");
          expect(btn).toBeOfClass("btn-" + cls);
          expect($(btn).text()).toEqual(pair.primary.name);
        };

        beforeEach(function() {
          view.render(true);
        });

        it("should render all pairs", function() {
          expect($("button", $(div)).length).toEqual(4);
        });
        
        it("should render the good pair", function() {
          checkButtonContent(
            document.getElementById(okPair.primary.name),
            okPair,
            "success"
          );
        });
        
        it("should render the single primary", function() {
          checkButtonContent(
            document.getElementById(noBkp.primary.name),
            noBkp,
            "success"
          );
        });
        
        it("should render the dead secondary", function() {
          checkButtonContent(
            document.getElementById(deadBkp.primary.name),
            deadBkp,
            "success"
          );
        });
        
        it("should render the dead primary", function() {
          checkButtonContent(
            document.getElementById(deadPrim.primary.name),
            deadPrim,
            "danger"
          );
        });
        
        checkUserActions();
      });

      describe("maximised version", function() {

        var checkDominoContent = function(tile, pair, btn1, btn2) {
          var upper = tile.children[0],
              lower = tile.children[2];
          expect(upper).toBeOfClass("btn-" + btn1);
          expect($(upper.children[0]).text()).toEqual(pair.primary.name);
          expect($(upper.children[1]).text()).toEqual(pair.primary.url);
          expect(lower).toBeOfClass("btn-" + btn2);
          if (pair.secondary) {
            expect($(lower.children[0]).text()).toEqual(pair.secondary.name);
            expect($(lower.children[1]).text()).toEqual(pair.secondary.url);
          } else {
            expect($(lower.children[0]).text()).toEqual("Not configured");
          }
        };

        beforeEach(function() {
          view.render();
        });

        it("should render all pairs", function() {
          expect($("> div.domino", $(div)).length).toEqual(4);
        });

        it("should render the good pair", function() {
          var tile = getTile(okPair.primary.name);
          checkDominoLayout(tile);
          checkDominoContent(tile, okPair, "success", "success");
        });

        it("should render the single primary", function() {
          var tile = getTile(noBkp.primary.name);
          checkDominoLayout(tile);
          checkDominoContent(tile, noBkp, "success", "inactive");
        });

        it("should render the dead secondary pair", function() {
          var tile = getTile(deadBkp.primary.name);
          checkDominoLayout(tile);
          checkDominoContent(tile, deadBkp, "success", "danger");
        });

        it("should render the dead primary pair", function() {
          var tile = getTile(deadPrim.primary.name);
          checkDominoLayout(tile);
          checkDominoContent(tile, deadPrim, "danger", "success");
        });


        checkUserActions();

      });

    });

  });

}());
