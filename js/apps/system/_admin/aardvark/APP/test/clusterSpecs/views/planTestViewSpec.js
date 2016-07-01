/* jshint browser: true */
/* jshint unused: false */
/* global describe, beforeEach, afterEach, it, */
/* global spyOn, expect, runs, waitsFor*/
/* global templateEngine, $, uiMatchers*/
(function () {
  'use strict';

  describe('Cluster Plan Single Machine View', function () {
    var view, div;

    beforeEach(function () {
      div = document.createElement('div');
      div.id = 'content';
      document.body.appendChild(div);
      view = new window.PlanTestView();
      window.App = {
        navigate: function () {
          throw 'Should be a spy';
        },
        clusterPlan: {
          get: function () {
            throw 'Should be a spy';
          },
          save: function () {
            throw 'Should be a spy';
          }
        },
        updateAllUrls: function () {
          throw 'Should be a spy';
        }
      };
      spyOn(window.App, 'navigate');
    });

    afterEach(function () {
      document.body.removeChild(div);
      delete window.App;
    });

    describe('no stored plan', function () {
      beforeEach(function () {
        spyOn(window.App.clusterPlan, 'get').andReturn(undefined);
      });

      it('should render default values', function () {
        spyOn(view.template, 'render');
        view.render();
        expect(window.App.clusterPlan.get).toHaveBeenCalledWith('config');
        expect(view.template.render).toHaveBeenCalledWith({
          dbs: 3,
          coords: 2,
          hostname: window.location.hostname,
          port: window.location.port
        });
      });

      it('should cancel', function () {
        view.cancel();
        expect(window.App.navigate).toHaveBeenCalledWith('planScenario',
          {trigger: true});
      });
    });

    describe('with a stored plan', function () {
      var dbs, coords, loc, port;

      beforeEach(function () {
        dbs = 5;
        coords = 3;
        loc = 'example.com';
        port = '1337';
        spyOn(window.App.clusterPlan, 'get').andReturn({
          numberOfDBservers: dbs,
          numberOfCoordinators: coords,
          dispatchers: {
            d1: {
              endpoint: 'http:://' + loc + ':' + port
            }
          }
        });
      });

      it('should render values', function () {
        spyOn(view.template, 'render');
        view.render();
        expect(view.template.render).toHaveBeenCalledWith({
          dbs: dbs,
          coords: coords,
          hostname: loc,
          port: port
        });
      });

      it('should cancel', function () {
        view.cancel();
        expect(window.App.navigate).toHaveBeenCalledWith('handleClusterDown',
          {trigger: true});
      });
    });

    describe('startPlan', function () {
      it('should startPlan without host', function () {
        spyOn(window.App.clusterPlan, 'save');
        var jquerydummy = {
            modal: function () {},
            addClass: function () {},
            html: function () {},
            val: function () {}
          }, jquerydummySpecific = {
            val: function () {}
        };
        spyOn(window, '$').andCallFake(function (a) {
          if (a === '#host') {
            return jquerydummySpecific;
          }
          return jquerydummy;
        });
        spyOn(jquerydummy, 'modal');
        spyOn(jquerydummy, 'addClass');
        spyOn(jquerydummy, 'html');
        spyOn(jquerydummy, 'val').andReturn(1);
        spyOn(jquerydummySpecific, 'val').andReturn(undefined);

        view.startPlan();
        expect(window.$).toHaveBeenCalledWith('#waitModalLayer');
        expect(window.$).toHaveBeenCalledWith('.modal-backdrop.fade.in');
        expect(window.$).toHaveBeenCalledWith('#waitModalMessage');
        expect(window.$).toHaveBeenCalledWith('#host');
        expect(window.$).toHaveBeenCalledWith('#port');
        expect(window.$).toHaveBeenCalledWith('#coordinators');
        expect(window.$).toHaveBeenCalledWith('#dbs');

        expect(jquerydummy.modal).toHaveBeenCalledWith('show');
        expect(jquerydummy.addClass).toHaveBeenCalledWith('waitModalBackdrop');
        expect(jquerydummy.html).toHaveBeenCalledWith(
          'Please be patient while your cluster is being launched'
        );
        expect(jquerydummy.val).toHaveBeenCalled();
        expect(jquerydummy.val).toHaveBeenCalled();
        expect(jquerydummy.val).toHaveBeenCalled();
        expect(jquerydummySpecific.val).toHaveBeenCalled();

        expect(window.App.clusterPlan.save).not.toHaveBeenCalled();
      });

      it('should startPlan without port', function () {
        spyOn(window.App.clusterPlan, 'save');
        var jquerydummy = {
            modal: function () {},
            addClass: function () {},
            html: function () {},
            val: function () {}
          }, jquerydummySpecific = {
            val: function () {}
        };
        spyOn(window, '$').andCallFake(function (a) {
          if (a === '#port') {
            return jquerydummySpecific;
          }
          return jquerydummy;
        });
        spyOn(jquerydummy, 'modal');
        spyOn(jquerydummy, 'addClass');
        spyOn(jquerydummy, 'html');
        spyOn(jquerydummy, 'val').andReturn(1);
        spyOn(jquerydummySpecific, 'val').andReturn(undefined);

        view.startPlan();
        expect(window.$).toHaveBeenCalledWith('#waitModalLayer');
        expect(window.$).toHaveBeenCalledWith('.modal-backdrop.fade.in');
        expect(window.$).toHaveBeenCalledWith('#waitModalMessage');
        expect(window.$).toHaveBeenCalledWith('#host');
        expect(window.$).toHaveBeenCalledWith('#port');
        expect(window.$).toHaveBeenCalledWith('#coordinators');
        expect(window.$).toHaveBeenCalledWith('#dbs');

        expect(jquerydummy.modal).toHaveBeenCalledWith('show');
        expect(jquerydummy.addClass).toHaveBeenCalledWith('waitModalBackdrop');
        expect(jquerydummy.html).toHaveBeenCalledWith(
          'Please be patient while your cluster is being launched'
        );
        expect(jquerydummy.val).toHaveBeenCalled();
        expect(jquerydummy.val).toHaveBeenCalled();
        expect(jquerydummy.val).toHaveBeenCalled();
        expect(jquerydummySpecific.val).toHaveBeenCalled();

        expect(window.App.clusterPlan.save).not.toHaveBeenCalled();
      });

      it('should startPlan without coordinators', function () {
        spyOn(window.App.clusterPlan, 'save');
        var jquerydummy = {
            modal: function () {},
            addClass: function () {},
            html: function () {},
            val: function () {}
          }, jquerydummySpecific = {
            val: function () {}
        };
        spyOn(window, '$').andCallFake(function (a) {
          if (a === '#coordinators') {
            return jquerydummySpecific;
          }
          return jquerydummy;
        });
        spyOn(jquerydummy, 'modal');
        spyOn(jquerydummy, 'addClass');
        spyOn(jquerydummy, 'html');
        spyOn(jquerydummy, 'val').andReturn(1);
        spyOn(jquerydummySpecific, 'val').andReturn(undefined);

        view.startPlan();
        expect(window.$).toHaveBeenCalledWith('#waitModalLayer');
        expect(window.$).toHaveBeenCalledWith('.modal-backdrop.fade.in');
        expect(window.$).toHaveBeenCalledWith('#waitModalMessage');
        expect(window.$).toHaveBeenCalledWith('#host');
        expect(window.$).toHaveBeenCalledWith('#port');
        expect(window.$).toHaveBeenCalledWith('#coordinators');
        expect(window.$).toHaveBeenCalledWith('#dbs');

        expect(jquerydummy.modal).toHaveBeenCalledWith('show');
        expect(jquerydummy.addClass).toHaveBeenCalledWith('waitModalBackdrop');
        expect(jquerydummy.html).toHaveBeenCalledWith(
          'Please be patient while your cluster is being launched'
        );
        expect(jquerydummy.val).toHaveBeenCalled();
        expect(jquerydummy.val).toHaveBeenCalled();
        expect(jquerydummy.val).toHaveBeenCalled();
        expect(jquerydummySpecific.val).toHaveBeenCalled();

        expect(window.App.clusterPlan.save).not.toHaveBeenCalled();
      });

      it('should startPlan without dbs', function () {
        spyOn(window.App.clusterPlan, 'save');
        var jquerydummy = {
            modal: function () {},
            addClass: function () {},
            html: function () {},
            val: function () {}
          }, jquerydummySpecific = {
            val: function () {}
        };
        spyOn(window, '$').andCallFake(function (a) {
          if (a === '#dbs') {
            return jquerydummySpecific;
          }
          return jquerydummy;
        });
        spyOn(jquerydummy, 'modal');
        spyOn(jquerydummy, 'addClass');
        spyOn(jquerydummy, 'html');
        spyOn(jquerydummy, 'val').andReturn(1);
        spyOn(jquerydummySpecific, 'val').andReturn(undefined);

        view.startPlan();
        expect(window.$).toHaveBeenCalledWith('#waitModalLayer');
        expect(window.$).toHaveBeenCalledWith('.modal-backdrop.fade.in');
        expect(window.$).toHaveBeenCalledWith('#waitModalMessage');
        expect(window.$).toHaveBeenCalledWith('#host');
        expect(window.$).toHaveBeenCalledWith('#port');
        expect(window.$).toHaveBeenCalledWith('#coordinators');
        expect(window.$).toHaveBeenCalledWith('#dbs');

        expect(jquerydummy.modal).toHaveBeenCalledWith('show');
        expect(jquerydummy.addClass).toHaveBeenCalledWith('waitModalBackdrop');
        expect(jquerydummy.html).toHaveBeenCalledWith(
          'Please be patient while your cluster is being launched'
        );
        expect(jquerydummy.val).toHaveBeenCalled();
        expect(jquerydummy.val).toHaveBeenCalled();
        expect(jquerydummy.val).toHaveBeenCalled();
        expect(jquerydummySpecific.val).toHaveBeenCalled();

        expect(window.App.clusterPlan.save).not.toHaveBeenCalled();
      });

      it('should startPlan with success', function () {
        var jquerydummy = {
            modal: function () {},
            addClass: function () {},
            html: function () {},
            val: function () {},
            removeClass: function () {}
          }, jquerydummyHost = {
            val: function () {
              return '123.456.789';
            }
          }, jquerydummyPort = {
            val: function () {
              return '10';
            }
          }, jquerydummyCoordinators = {
            val: function () {
              return 4;
            }
          }, jquerydummyDBServer = {
            val: function () {
              return 3;
            }
        };
        spyOn(window, '$').andCallFake(function (a) {
          if (a === '#host') {
            return jquerydummyHost;
          }
          if (a === '#port') {
            return jquerydummyPort;
          }
          if (a === '#coordinators') {
            return jquerydummyCoordinators;
          }
          if (a === '#dbs') {
            return jquerydummyDBServer;
          }
          return jquerydummy;
        });
        spyOn(jquerydummy, 'modal');
        spyOn(jquerydummy, 'addClass');
        spyOn(jquerydummy, 'removeClass');
        spyOn(jquerydummy, 'html');
        spyOn(jquerydummy, 'val').andReturn(1);
        spyOn(jquerydummyHost, 'val').andCallThrough();
        spyOn(jquerydummyPort, 'val').andCallThrough();
        spyOn(jquerydummyCoordinators, 'val').andCallThrough();
        spyOn(jquerydummyDBServer, 'val').andCallThrough();

        spyOn(window.App.clusterPlan, 'save').andCallFake(function (a, b) {
          expect(a).toEqual({
            type: 'testSetup',
            dispatchers: '123.456.789' + ':' + '10',
            numberDBServers: 3,
            numberCoordinators: 4
          });

          b.success();
        });

        spyOn(window.App, 'updateAllUrls');

        view.startPlan();
        expect(window.$).toHaveBeenCalledWith('#waitModalLayer');
        expect(window.$).toHaveBeenCalledWith('.modal-backdrop.fade.in');
        expect(window.$).toHaveBeenCalledWith('#waitModalMessage');
        expect(window.$).toHaveBeenCalledWith('#host');
        expect(window.$).toHaveBeenCalledWith('#port');
        expect(window.$).toHaveBeenCalledWith('#coordinators');
        expect(window.$).toHaveBeenCalledWith('#dbs');

        expect(window.App.updateAllUrls).toHaveBeenCalled();
        expect(window.App.navigate).toHaveBeenCalledWith('showCluster',
          {trigger: true});

        expect(jquerydummy.modal).toHaveBeenCalledWith('show');
        expect(jquerydummy.addClass).toHaveBeenCalledWith('waitModalBackdrop');
        expect(jquerydummy.removeClass).toHaveBeenCalledWith('waitModalBackdrop');
        expect(jquerydummy.html).toHaveBeenCalledWith(
          'Please be patient while your cluster is being launched'
        );
        expect(jquerydummyHost.val).toHaveBeenCalled();
        expect(jquerydummyPort.val).toHaveBeenCalled();
        expect(jquerydummyCoordinators.val).toHaveBeenCalled();
        expect(jquerydummyDBServer.val).toHaveBeenCalled();
      });

      it('should startPlan with error', function () {
        var jquerydummy = {
            modal: function () {},
            addClass: function () {},
            html: function () {},
            val: function () {},
            removeClass: function () {}
          }, jquerydummyHost = {
            val: function () {
              return '123.456.789';
            }
          }, jquerydummyPort = {
            val: function () {
              return '10';
            }
          }, jquerydummyCoordinators = {
            val: function () {
              return 4;
            }
          }, jquerydummyDBServer = {
            val: function () {
              return 3;
            }
        };
        spyOn(window, '$').andCallFake(function (a) {
          if (a === '#host') {
            return jquerydummyHost;
          }
          if (a === '#port') {
            return jquerydummyPort;
          }
          if (a === '#coordinators') {
            return jquerydummyCoordinators;
          }
          if (a === '#dbs') {
            return jquerydummyDBServer;
          }
          return jquerydummy;
        });
        spyOn(jquerydummy, 'modal');
        spyOn(jquerydummy, 'addClass');
        spyOn(jquerydummy, 'removeClass');
        spyOn(jquerydummy, 'html');
        spyOn(jquerydummy, 'val').andReturn(1);
        spyOn(jquerydummyHost, 'val').andCallThrough();
        spyOn(jquerydummyPort, 'val').andCallThrough();
        spyOn(jquerydummyCoordinators, 'val').andCallThrough();
        spyOn(jquerydummyDBServer, 'val').andCallThrough();

        spyOn(window.App.clusterPlan, 'save').andCallFake(function (a, b) {
          expect(a).toEqual({
            type: 'testSetup',
            dispatchers: '123.456.789' + ':' + '10',
            numberDBServers: 3,
            numberCoordinators: 4
          });

          b.error({}, {statusText: 'bla'});
        });

        spyOn(window.App, 'updateAllUrls');

        view.startPlan();
        expect(window.$).toHaveBeenCalledWith('#waitModalLayer');
        expect(window.$).toHaveBeenCalledWith('.modal-backdrop.fade.in');
        expect(window.$).toHaveBeenCalledWith('#waitModalMessage');
        expect(window.$).toHaveBeenCalledWith('#host');
        expect(window.$).toHaveBeenCalledWith('#port');
        expect(window.$).toHaveBeenCalledWith('#coordinators');
        expect(window.$).toHaveBeenCalledWith('#dbs');

        expect(window.App.updateAllUrls).not.toHaveBeenCalled();
        expect(window.App.navigate).not.toHaveBeenCalledWith('showCluster',
          {trigger: true});

        expect(jquerydummy.modal).toHaveBeenCalledWith('show');
        expect(jquerydummy.addClass).toHaveBeenCalledWith('waitModalBackdrop');
        expect(jquerydummy.removeClass).toHaveBeenCalledWith('waitModalBackdrop');
        expect(jquerydummy.html).toHaveBeenCalledWith(
          'Please be patient while your cluster is being launched'
        );
        expect(jquerydummyHost.val).toHaveBeenCalled();
        expect(jquerydummyPort.val).toHaveBeenCalled();
        expect(jquerydummyCoordinators.val).toHaveBeenCalled();
        expect(jquerydummyDBServer.val).toHaveBeenCalled();
      });
    });
  });
}());
