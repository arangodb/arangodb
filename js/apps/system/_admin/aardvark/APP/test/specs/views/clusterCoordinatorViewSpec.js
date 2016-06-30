/* jshint browser: true */
/* jshint unused: false */
/* global describe, beforeEach, afterEach, it, */
/* global spyOn, expect*/
/* global templateEngine, $, uiMatchers*/
(function () {
  'use strict';

  describe('Cluster Coordinator View', function () {
    var view, div;

    beforeEach(function () {
      div = document.createElement('div');
      div.id = 'clusterServers';
      document.body.appendChild(div);
      uiMatchers.define(this);
    });

    afterEach(function () {
      document.body.removeChild(div);
    });

    describe('rendering', function () {
      var charly, carlos, chantalle, coordinators,
        coordCol,
        getTile = function (name) {
          return document.getElementById(name);
        },
        checkTile = function (c, cls) {
          var tile = getTile(c.name),
            inner = tile.children[0];
          expect(tile).toBeOfClass('coordinator');
          expect(tile).toBeOfClass('domino');
          expect(inner).toBeOfClass('btn-' + cls);
          expect(inner).toBeOfClass('domino-inner');
          expect(inner.children[0]).toBeOfClass('domino-header');
          expect($(inner.children[0]).text()).toEqual(c.name);
          expect($(inner.children[1]).text()).toEqual(c.url);
      };

      beforeEach(function () {
        charly = {
          name: 'Charly',
          url: 'tcp://192.168.0.1:1337',
          status: 'ok'
        };
        carlos = {
          name: 'Carlos',
          url: 'tcp://192.168.0.2:1337',
          status: 'critical'
        };
        chantalle = {
          name: 'Chantalle',
          url: 'tcp://192.168.0.5:1337',
          status: 'ok'
        };
        coordinators = [
          charly,
          carlos,
          chantalle
        ];
        coordCol = {
          getList: function () {
            return coordinators;
          }
        };
        view = new window.ClusterCoordinatorView({
          collection: coordCol
        });
        view.render();
      });

      it('should render all coordiniators', function () {
        expect($('> div.coordinator', $(div)).length).toEqual(3);
      });

      it('should render charly', function () {
        checkTile(charly, 'success');
      });

      it('should render carlos', function () {
        checkTile(carlos, 'danger');
      });

      it('should render chantalle', function () {
        checkTile(chantalle, 'success');
      });

      it('should allow an unrender function', function () {
        view.unrender();
        expect($(div).html()).toEqual('');
      });
    });
  });
}());
