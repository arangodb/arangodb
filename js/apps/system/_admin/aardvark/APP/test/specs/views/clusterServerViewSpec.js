/* jshint browser: true */
/* jshint unused: false */
/* global describe, beforeEach, afterEach, it, */
/* global spyOn, expect*/
/* global templateEngine, $, _, uiMatchers*/
(function () {
  'use strict';

  describe('Cluster Server View', function () {
    var view, div, dbView, dbCol;

    beforeEach(function () {
      div = document.createElement('div');
      div.id = 'clusterServers';
      document.body.appendChild(div);
      dbView = {
        render: function () {},
        unrender: function () {}
      };
      dbCol = {
        __id: 'testId'
      };
      spyOn(window, 'ClusterDatabaseView').andReturn(dbView);
      spyOn(window, 'ClusterDatabases').andReturn(dbCol);
      uiMatchers.define(this);
    });

    afterEach(function () {
      document.body.removeChild(div);
    });

    describe('initialization', function () {
      it('should create a Cluster Database View', function () {
        view = new window.ClusterServerView();
        expect(window.ClusterDatabaseView).toHaveBeenCalledWith({
          collection: dbCol
        });
      });
    });

    describe('rendering', function () {
      var okPair, noBkp, deadBkp, deadPrim, deadPair,
        fakeServers, dbServers,
        getTile = function (name) {
          return document.getElementById(name);
        },

        checkUserActions = function () {
          describe('user actions', function () {
            var info;

            it('should be able to navigate to each server', function () {
              spyOn(view, 'render');
              _.each(fakeServers, function (o) {
                dbView.render.reset();
                view.render.reset();
                var name = o.primary.name;
                $(getTile(name)).click();
                expect(dbView.render).toHaveBeenCalledWith(name);
                expect(view.render).toHaveBeenCalledWith(true);
              });
            });
          });
        },

        checkDominoLayout = function (tile) {
          var tileElements = tile.children,
            upper = tileElements[0],
            line = tileElements[1],
            lower = tileElements[2];
          expect(tile).toBeOfClass('domino');
          expect(tile).toBeOfClass('server');
          expect(upper).toBeOfClass('domino-upper');
          expect(line).toBeOfClass('domino-line');
          expect(lower).toBeOfClass('domino-lower');
          expect(upper.children[0]).toBeOfClass('domino-header');
          expect(lower.children[0]).toBeOfClass('domino-header');
      };

      beforeEach(function () {
        spyOn(dbView, 'render');
        spyOn(dbView, 'unrender');
        okPair = {
          primary: {
            name: 'Pavel',
            address: 'tcp://192.168.0.1:1337',
            status: 'ok'
          },
          secondary: {
            name: 'Sally',
            address: 'tcp://192.168.1.1:1337',
            status: 'ok'
          }
        };
        noBkp = {
          primary: {
            name: 'Pancho',
            address: 'tcp://192.168.0.2:1337',
            status: 'ok'
          }
        };
        deadBkp = {
          primary: {
            name: 'Pepe',
            address: 'tcp://192.168.0.3:1337',
            status: 'ok'
          },
          secondary: {
            name: 'Sam',
            address: 'tcp://192.168.1.3:1337',
            status: 'critical'
          }
        };
        deadPrim = {
          primary: {
            name: 'Pedro',
            address: 'tcp://192.168.0.4:1337',
            status: 'critical'
          },
          secondary: {
            name: 'Sabrina',
            address: 'tcp://192.168.1.4:1337',
            status: 'ok'
          }
        };
        deadPair = {
          primary: {
            name: 'Pablo',
            address: 'tcp://192.168.0.5:1337',
            status: 'critical'
          },
          secondary: {
            name: 'Sandy',
            address: 'tcp://192.168.1.5:1337',
            status: 'critical'
          }
        };
        fakeServers = [
          okPair,
          noBkp,
          deadBkp,
          deadPrim
        ];
        dbServers = {
          getList: function () {
            return fakeServers;
          }
        };
        view = new window.ClusterServerView({
          collection: dbServers
        });
        view.render();
      });

      it('should not render the Database view', function () {
        expect(dbView.render).not.toHaveBeenCalled();
        expect(dbView.unrender).toHaveBeenCalled();
      });

      it('should offer an unrender function', function () {
        dbView.unrender.reset();
        view.unrender();
        expect($(div).html()).toEqual('');
        expect(dbView.unrender).toHaveBeenCalled();
      });

      describe('minified version', function () {
        var checkButtonContent = function (btn, pair, cls) {
          expect(btn).toBeOfClass('btn');
          expect(btn).toBeOfClass('server');
          expect(btn).toBeOfClass('btn-' + cls);
          expect($(btn).text()).toEqual(pair.primary.name);
        };

        beforeEach(function () {
          view.render(true);
        });

        it('should render all pairs', function () {
          expect($('button', $(div)).length).toEqual(4);
        });

        it('should render the good pair', function () {
          checkButtonContent(
            document.getElementById(okPair.primary.name),
            okPair,
            'success'
          );
        });

        it('should render the single primary', function () {
          checkButtonContent(
            document.getElementById(noBkp.primary.name),
            noBkp,
            'success'
          );
        });

        it('should render the dead secondary', function () {
          checkButtonContent(
            document.getElementById(deadBkp.primary.name),
            deadBkp,
            'success'
          );
        });

        it('should render the dead primary', function () {
          checkButtonContent(
            document.getElementById(deadPrim.primary.name),
            deadPrim,
            'danger'
          );
        });

        checkUserActions();
      });

      describe('maximised version', function () {
        var checkDominoContent = function (tile, pair, btn1, btn2) {
          var upper = tile.children[0],
            lower = tile.children[2];
          expect(upper).toBeOfClass('btn-' + btn1);
          expect($(upper.children[0]).text()).toEqual(pair.primary.name);
          expect($(upper.children[1]).text()).toEqual(pair.primary.address);
          expect(lower).toBeOfClass('btn-' + btn2);
          if (pair.secondary) {
            expect($(lower.children[0]).text()).toEqual(pair.secondary.name);
            expect($(lower.children[1]).text()).toEqual(pair.secondary.address);
          } else {
            expect($(lower.children[0]).text()).toEqual('Not configured');
          }
        };

        beforeEach(function () {
          view.render();
        });

        it('should render all pairs', function () {
          expect($('> div.domino', $(div)).length).toEqual(4);
        });

        it('should render the good pair', function () {
          var tile = getTile(okPair.primary.name);
          checkDominoLayout(tile);
          checkDominoContent(tile, okPair, 'success', 'success');
        });

        it('should render the single primary', function () {
          var tile = getTile(noBkp.primary.name);
          checkDominoLayout(tile);
          checkDominoContent(tile, noBkp, 'success', 'inactive');
        });

        it('should render the dead secondary pair', function () {
          var tile = getTile(deadBkp.primary.name);
          checkDominoLayout(tile);
          checkDominoContent(tile, deadBkp, 'success', 'danger');
        });

        it('should render the dead primary pair', function () {
          var tile = getTile(deadPrim.primary.name);
          checkDominoLayout(tile);
          checkDominoContent(tile, deadPrim, 'danger', 'success');
        });

        checkUserActions();
      });
    });
  });
}());
