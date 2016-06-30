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
      view = new window.PlanSymmetricView();
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
      delete window.App;
    });

    it('assert the basics', function () {
      expect(view.events).toEqual({
        'click #startSymmetricPlan': 'startPlan',
        'click .add': 'addEntry',
        'click .delete': 'removeEntry',
        'click #cancel': 'cancel',
        'click #test-all-connections': 'checkAllConnections',
        'focusout .host': 'autoCheckConnections',
        'focusout .port': 'autoCheckConnections',
        'focusout .user': 'autoCheckConnections',
        'focusout .passwd': 'autoCheckConnections'
      });
    });

    it('should cancel with a plan', function () {
      spyOn(window.App.clusterPlan, 'get').andReturn(1);
      view.cancel();
      expect(window.App.navigate).toHaveBeenCalledWith('handleClusterDown',
        {trigger: true});
    });

    it('should cancel without a plan', function () {
      spyOn(window.App.clusterPlan, 'get').andReturn(undefined);
      view.cancel();
      expect(window.App.navigate).toHaveBeenCalledWith('planScenario',
        {trigger: true});
    });

    it('should start plan with success', function () {
      var jquerydummy = {
          modal: function () {},
          addClass: function () {},
          html: function () {},
          val: function () {},
          removeClass: function () {},
          prop: function () {},
          each: function (a) {
            a(1, 'd');
          }
        }, jquerydummyHost = {
          val: function () {
            return '123.456.789';
          }
        }, jquerydummyPort = {
          val: function () {
            return '10';
          }
        }, jquerydummyPw = {
          val: function () {
            return 'password';
          }
        }, jquerydummyUser = {
          val: function () {
            return 'user';
          }
      };
      spyOn(window, '$').andCallFake(function (a, b) {
        if (a === '.host' && b === 'd') {
          return jquerydummyHost;
        }
        if (a === '.port' && b === 'd') {
          return jquerydummyPort;
        }
        if (a === '.passwd' && b === 'd') {
          return jquerydummyPw;
        }
        if (a === '.user' && b === 'd') {
          return jquerydummyUser;
        }
        return jquerydummy;
      });
      spyOn(jquerydummy, 'modal');
      spyOn(jquerydummy, 'addClass');
      spyOn(jquerydummy, 'removeClass');
      spyOn(jquerydummy, 'html');
      spyOn(jquerydummy, 'prop').andReturn(true);
      spyOn(jquerydummy, 'each').andCallThrough();
      spyOn(jquerydummy, 'val').andReturn(1);
      spyOn(jquerydummyHost, 'val').andCallThrough();
      spyOn(jquerydummyPort, 'val').andCallThrough();
      spyOn(jquerydummyUser, 'val').andCallThrough();
      spyOn(jquerydummyPw, 'val').andCallThrough();

      spyOn(window.App.clusterPlan, 'save').andCallFake(function (a, b) {
        expect(a).toEqual({dispatchers: [
            {
              host: '123.456.789:10',
              isDBServer: true,
              isCoordinator: true,
              username: 'user',
              passwd: 'password'
            }
          ],
          useSSLonDBservers: true,
          useSSLonCoordinators: true,
        type: 'symmetricalSetup' }
        );

        b.success();
      });
      spyOn(window.App, 'updateAllUrls');
      view.isSymmetric = true;
      view.startPlan();
      expect(window.App.navigate).toHaveBeenCalledWith('showCluster',
        {trigger: true});

      expect(window.$).toHaveBeenCalledWith('.useSSLonDBservers');
      expect(window.$).toHaveBeenCalledWith('.useSSLonCoordinators');
      expect(window.$).toHaveBeenCalledWith('.dispatcher');
      expect(window.$).toHaveBeenCalledWith('#waitModalLayer');
      expect(window.$).toHaveBeenCalledWith('.modal-backdrop.fade.in');

      expect(window.App.updateAllUrls).toHaveBeenCalled();
      expect(window.App.navigate).toHaveBeenCalledWith('showCluster',
        {trigger: true});

      expect(jquerydummy.prop).toHaveBeenCalledWith('checked');
      expect(jquerydummy.modal).toHaveBeenCalledWith('show');
      expect(jquerydummy.addClass).toHaveBeenCalledWith('waitModalBackdrop');
      expect(jquerydummy.removeClass).toHaveBeenCalledWith('waitModalBackdrop');
      expect(jquerydummy.html).toHaveBeenCalledWith(
        'Please be patient while your cluster is being launched'
      );
      expect(jquerydummyHost.val).toHaveBeenCalled();
      expect(jquerydummyPort.val).toHaveBeenCalled();
      expect(jquerydummyPw.val).toHaveBeenCalled();
      expect(jquerydummyUser.val).toHaveBeenCalled();
    });

    it('should start asymmetrical plan with error', function () {
      var jquerydummy = {
          modal: function () {},
          addClass: function () {},
          html: function () {},
          val: function () {},
          removeClass: function () {},
          prop: function () {},
          each: function (a) {
            a(1, 'd');
          }
        }, jquerydummyHost = {
          val: function () {
            return '123.456.789';
          }
        }, jquerydummyPort = {
          val: function () {
            return '10';
          }
        }, jquerydummyPw = {
          val: function () {
            return 'password';
          }
        }, jquerydummyUser = {
          val: function () {
            return 'user';
          }
      };
      spyOn(window, '$').andCallFake(function (a, b) {
        if (a === '.host' && b === 'd') {
          return jquerydummyHost;
        }
        if (a === '.port' && b === 'd') {
          return jquerydummyPort;
        }
        if (a === '.passwd' && b === 'd') {
          return jquerydummyPw;
        }
        if (a === '.user' && b === 'd') {
          return jquerydummyUser;
        }
        return jquerydummy;
      });
      spyOn(jquerydummy, 'modal');
      spyOn(jquerydummy, 'addClass');
      spyOn(jquerydummy, 'removeClass');
      spyOn(jquerydummy, 'html');
      spyOn(jquerydummy, 'prop').andReturn(true);
      spyOn(jquerydummy, 'each').andCallThrough();
      spyOn(jquerydummy, 'val').andReturn(1);
      spyOn(jquerydummyHost, 'val').andCallThrough();
      spyOn(jquerydummyPort, 'val').andCallThrough();
      spyOn(jquerydummyUser, 'val').andCallThrough();
      spyOn(jquerydummyPw, 'val').andCallThrough();

      spyOn(window.App.clusterPlan, 'save').andCallFake(function (a, b) {
        expect(a).toEqual({dispatchers: [
            {
              host: '123.456.789:10',
              isDBServer: true,
              isCoordinator: true,
              username: 'user',
              passwd: 'password'
            }
          ],
          useSSLonDBservers: true,
          useSSLonCoordinators: true,
        type: 'asymmetricalSetup' }
        );

        b.error({}, {statusText: 'bla'});
      });
      spyOn(window.App, 'updateAllUrls');
      view.isSymmetric = false;
      view.startPlan();
      expect(window.$).toHaveBeenCalledWith('.useSSLonDBservers');
      expect(window.$).toHaveBeenCalledWith('.useSSLonCoordinators');
      expect(window.$).toHaveBeenCalledWith('.dispatcher');
      expect(window.$).toHaveBeenCalledWith('#waitModalLayer');
      expect(window.$).toHaveBeenCalledWith('.modal-backdrop.fade.in');

      expect(window.App.updateAllUrls).not.toHaveBeenCalled();
      expect(window.App.navigate).not.toHaveBeenCalled();

      expect(jquerydummy.prop).toHaveBeenCalledWith('checked');
      expect(jquerydummy.modal).toHaveBeenCalledWith('show');
      expect(jquerydummy.addClass).toHaveBeenCalledWith('waitModalBackdrop');
      expect(jquerydummy.removeClass).toHaveBeenCalledWith('waitModalBackdrop');
      expect(jquerydummy.html).toHaveBeenCalledWith(
        'Please be patient while your cluster is being launched'
      );
      expect(jquerydummyHost.val).toHaveBeenCalled();
      expect(jquerydummyPort.val).toHaveBeenCalled();
      expect(jquerydummyPw.val).toHaveBeenCalled();
      expect(jquerydummyUser.val).toHaveBeenCalled();
    });

    it('should start asymmetrical plan with missing dbserver', function () {
      var jquerydummy = {
          modal: function () {},
          addClass: function () {},
          html: function () {},
          val: function () {},
          removeClass: function () {},
          prop: function () {},
          each: function (a) {
            a(1, 'd');
          }
        }, jquerydummyHost = {
          val: function () {
            return undefined;
          }
        }, jquerydummyPort = {
          val: function () {
            return '10';
          }
        }, jquerydummyPw = {
          val: function () {
            return 'password';
          }
        }, jquerydummyUser = {
          val: function () {
            return 'user';
          }
      };
      spyOn(window, '$').andCallFake(function (a, b) {
        if (a === '.host' && b === 'd') {
          return jquerydummyHost;
        }
        if (a === '.port' && b === 'd') {
          return jquerydummyPort;
        }
        if (a === '.passwd' && b === 'd') {
          return jquerydummyPw;
        }
        if (a === '.user' && b === 'd') {
          return jquerydummyUser;
        }
        return jquerydummy;
      });
      spyOn(jquerydummy, 'modal');
      spyOn(jquerydummy, 'addClass');
      spyOn(jquerydummy, 'removeClass');
      spyOn(jquerydummy, 'html');
      spyOn(jquerydummy, 'prop').andReturn(true);
      spyOn(jquerydummy, 'each').andCallThrough();
      spyOn(jquerydummy, 'val').andReturn(1);
      spyOn(jquerydummyHost, 'val').andCallThrough();
      spyOn(jquerydummyPort, 'val').andCallThrough();
      spyOn(jquerydummyUser, 'val').andCallThrough();
      spyOn(jquerydummyPw, 'val').andCallThrough();

      spyOn(window.App.clusterPlan, 'save').andCallFake(function (a, b) {
        expect(a).toEqual({dispatchers: [
            {
              host: '123.456.789:10',
              isDBServer: true,
              isCoordinator: true,
              username: 'user',
              passwd: 'password'
            }
          ],
          useSSLonDBservers: true,
          useSSLonCoordinators: true,
        type: 'asymmetricalSetup' }
        );

        b.error({}, {statusText: 'bla'});
      });
      spyOn(window.App, 'updateAllUrls');
      view.isSymmetric = false;
      view.startPlan();
      expect(window.$).toHaveBeenCalledWith('.useSSLonDBservers');
      expect(window.$).toHaveBeenCalledWith('.useSSLonCoordinators');
      expect(window.$).toHaveBeenCalledWith('.dispatcher');
      // expect(window.$).toHaveBeenCalledWith("#waitModalLayer")
      // expect(window.$).toHaveBeenCalledWith(".modal-backdrop.fade.in")

      expect(window.App.updateAllUrls).not.toHaveBeenCalled();
      expect(window.App.navigate).not.toHaveBeenCalled();

      expect(jquerydummy.prop).toHaveBeenCalledWith('checked');
      // expect(jquerydummy.modal).toHaveBeenCalledWith("show")
      // expect(jquerydummy.addClass).toHaveBeenCalledWith("waitModalBackdrop")
      // expect(jquerydummy.removeClass).toHaveBeenCalledWith("waitModalBackdrop")
      // expect(jquerydummy.html).toHaveBeenCalledWith(
      //                "Please be patient while your cluster is being launched"
      //          )
      expect(jquerydummyHost.val).toHaveBeenCalled();
      expect(jquerydummyPort.val).toHaveBeenCalled();
      expect(jquerydummyPw.val).toHaveBeenCalled();
      expect(jquerydummyUser.val).toHaveBeenCalled();
    });

    it('should start asymmetrical plan with missing coordinator', function () {
      var jquerydummy = {
          modal: function () {},
          addClass: function () {},
          html: function () {},
          val: function () {},
          removeClass: function () {},
          prop: function () {},
          each: function (a) {
            a(1, 'd');
          }
        }, jquerydummyHost = {
          val: function () {
            return '123.456.789';
          }
        }, jquerydummyPort = {
          val: function () {
            return '10';
          }
        }, jquerydummyPw = {
          val: function () {
            return 'password';
          }
        }, jquerydummyUser = {
          val: function () {
            return 'user';
          }
      };
      spyOn(window, '$').andCallFake(function (a, b) {
        if (a === '.host' && b === 'd') {
          return jquerydummyHost;
        }
        if (a === '.port' && b === 'd') {
          return jquerydummyPort;
        }
        if (a === '.passwd' && b === 'd') {
          return jquerydummyPw;
        }
        if (a === '.user' && b === 'd') {
          return jquerydummyUser;
        }
        if (a === '.isCoordinator' && b === 'd') {
          return {
            prop: function () {
              return false;
            }

          };
        }
        return jquerydummy;
      });
      spyOn(jquerydummy, 'modal');
      spyOn(jquerydummy, 'addClass');
      spyOn(jquerydummy, 'removeClass');
      spyOn(jquerydummy, 'html');
      spyOn(jquerydummy, 'prop').andReturn(true);
      spyOn(jquerydummy, 'each').andCallThrough();
      spyOn(jquerydummy, 'val').andReturn(1);
      spyOn(jquerydummyHost, 'val').andCallThrough();
      spyOn(jquerydummyPort, 'val').andCallThrough();
      spyOn(jquerydummyUser, 'val').andCallThrough();
      spyOn(jquerydummyPw, 'val').andCallThrough();

      spyOn(window.App.clusterPlan, 'save').andCallFake(function (a, b) {
        expect(a).toEqual({dispatchers: [
            {
              host: '123.456.789:10',
              isDBServer: true,
              isCoordinator: true,
              username: 'user',
              passwd: 'password'
            }
          ],
          useSSLonDBservers: true,
          useSSLonCoordinators: true,
        type: 'asymmetricalSetup' }
        );

        b.error({}, {statusText: 'bla'});
      });
      spyOn(window.App, 'updateAllUrls');
      view.isSymmetric = false;
      view.startPlan();
      expect(window.$).toHaveBeenCalledWith('.useSSLonDBservers');
      expect(window.$).toHaveBeenCalledWith('.useSSLonCoordinators');
      expect(window.$).toHaveBeenCalledWith('.dispatcher');
      // expect(window.$).toHaveBeenCalledWith("#waitModalLayer")
      // expect(window.$).toHaveBeenCalledWith(".modal-backdrop.fade.in")

      expect(window.App.updateAllUrls).not.toHaveBeenCalled();
      expect(window.App.navigate).not.toHaveBeenCalled();

      expect(jquerydummy.prop).toHaveBeenCalledWith('checked');
      // expect(jquerydummy.modal).toHaveBeenCalledWith("show")
      // expect(jquerydummy.addClass).toHaveBeenCalledWith("waitModalBackdrop")
      // expect(jquerydummy.removeClass).toHaveBeenCalledWith("waitModalBackdrop")
      // expect(jquerydummy.html).toHaveBeenCalledWith(
      //                "Please be patient while your cluster is being launched"
      //          )
      expect(jquerydummyHost.val).toHaveBeenCalled();
      expect(jquerydummyPort.val).toHaveBeenCalled();
      expect(jquerydummyPw.val).toHaveBeenCalled();
      expect(jquerydummyUser.val).toHaveBeenCalled();
    });

    it('should start symmetrical plan with missing disaptcher', function () {
      var jquerydummy = {
          modal: function () {},
          addClass: function () {},
          html: function () {},
          val: function () {},
          removeClass: function () {},
          prop: function () {},
          each: function (a) {
            a(1, 'd');
          }
        }, jquerydummyHost = {
          val: function () {
            return undefined;
          }
        }, jquerydummyPort = {
          val: function () {
            return '10';
          }
        }, jquerydummyPw = {
          val: function () {
            return 'password';
          }
        }, jquerydummyUser = {
          val: function () {
            return 'user';
          }
      };
      spyOn(window, '$').andCallFake(function (a, b) {
        if (a === '.host' && b === 'd') {
          return jquerydummyHost;
        }
        if (a === '.port' && b === 'd') {
          return jquerydummyPort;
        }
        if (a === '.passwd' && b === 'd') {
          return jquerydummyPw;
        }
        if (a === '.user' && b === 'd') {
          return jquerydummyUser;
        }
        return jquerydummy;
      });
      spyOn(jquerydummy, 'modal');
      spyOn(jquerydummy, 'addClass');
      spyOn(jquerydummy, 'removeClass');
      spyOn(jquerydummy, 'html');
      spyOn(jquerydummy, 'prop').andReturn(true);
      spyOn(jquerydummy, 'each').andCallThrough();
      spyOn(jquerydummy, 'val').andReturn(1);
      spyOn(jquerydummyHost, 'val').andCallThrough();
      spyOn(jquerydummyPort, 'val').andCallThrough();
      spyOn(jquerydummyUser, 'val').andCallThrough();
      spyOn(jquerydummyPw, 'val').andCallThrough();

      spyOn(window.App.clusterPlan, 'save').andCallFake(function (a, b) {
        expect(a).toEqual({dispatchers: [
            {
              host: '123.456.789:10',
              isDBServer: true,
              isCoordinator: true,
              username: 'user',
              passwd: 'password'
            }
          ],
          useSSLonDBservers: true,
          useSSLonCoordinators: true,
        type: 'symmetricalSetup' }
        );

        b.success();
      });
      spyOn(window.App, 'updateAllUrls');
      view.isSymmetric = true;
      view.startPlan();
      expect(window.App.navigate).not.toHaveBeenCalled();

      expect(window.$).toHaveBeenCalledWith('.useSSLonDBservers');
      expect(window.$).toHaveBeenCalledWith('.useSSLonCoordinators');
      expect(window.$).toHaveBeenCalledWith('.dispatcher');

      expect(window.App.updateAllUrls).not.toHaveBeenCalled();

      expect(jquerydummy.prop).toHaveBeenCalledWith('checked');
      expect(jquerydummyHost.val).toHaveBeenCalled();
      expect(jquerydummyPort.val).toHaveBeenCalled();
      expect(jquerydummyPw.val).toHaveBeenCalled();
      expect(jquerydummyUser.val).toHaveBeenCalled();
    });

    it('should addEntry', function () {
      var jquerydummy = {
        attr: function () {},
        removeClass: function () {},
        addClass: function () {},
        val: function () {},
        append: function () {}
      };

      spyOn(window, '$').andCallFake(function (a) {
        return jquerydummy;
      });
      spyOn(jquerydummy, 'attr');
      spyOn(jquerydummy, 'removeClass');
      spyOn(jquerydummy, 'addClass');
      spyOn(jquerydummy, 'append');
      spyOn(jquerydummy, 'val').andReturn('credentials');

      spyOn(view.entryTemplate, 'render').andReturn('template');
      view.isSymmetric = false;
      view.addEntry();

      expect(view.entryTemplate.render).toHaveBeenCalledWith({
        isSymmetric: false,
        isFirst: false,
        isCoordinator: true,
        isDBServer: true,
        host: '',
        port: '',
        user: 'credentials',
        passwd: 'credentials'
      });

      expect(window.$).toHaveBeenCalledWith('#startSymmetricPlan');
      expect(window.$).toHaveBeenCalledWith('#server_list');
      expect(window.$).toHaveBeenCalledWith('#server_list ' +
        'div.control-group.dispatcher:last .user');
      expect(window.$).toHaveBeenCalledWith('#server_list ' +
        'div.control-group.dispatcher:last .passwd');

      expect(jquerydummy.attr).toHaveBeenCalledWith('disabled', 'disabled');
      expect(jquerydummy.removeClass).toHaveBeenCalledWith('button-success');
      expect(jquerydummy.addClass).toHaveBeenCalledWith('button-neutral');
      expect(jquerydummy.append).toHaveBeenCalledWith('template');
    });

    it('should removeEntry', function () {
      var jquerydummy = {
        closest: function () {
          return jquerydummy;
        },
        remove: function () {}
      };

      spyOn(window, '$').andCallFake(function (a) {
        return jquerydummy;
      });
      spyOn(jquerydummy, 'closest').andCallThrough();
      spyOn(jquerydummy, 'remove');

      spyOn(view, 'checkAllConnections');

      view.removeEntry({currentTarget: 1});

      expect(window.$).toHaveBeenCalledWith(1);

      expect(jquerydummy.closest).toHaveBeenCalledWith('.control-group');
      expect(jquerydummy.remove).toHaveBeenCalled();
      expect(view.checkAllConnections).toHaveBeenCalled();
    });

    it('should render symmetric view with no config', function () {
      var jquerydummy = {
        html: function () {},
        append: function () {}
      };

      spyOn(window, '$').andCallFake(function (a) {
        return jquerydummy;
      });
      spyOn(jquerydummy, 'html');
      spyOn(jquerydummy, 'append');

      spyOn(window.App.clusterPlan, 'get');

      spyOn(view.template, 'render').andReturn('template');
      spyOn(view.modal, 'render').andReturn('modal');
      spyOn(view.entryTemplate, 'render').andReturn('entryTemplate');
      spyOn(view, 'disableLaunchButton');

      view.render(true);

      expect(view.template.render).toHaveBeenCalledWith({
        isSymmetric: true,
        params: {},
        useSSLonDBservers: false,
        useSSLonCoordinators: false
      });
      expect(view.entryTemplate.render).toHaveBeenCalledWith({
        isSymmetric: true,
        isFirst: true,
        isCoordinator: true,
        isDBServer: true,
        host: '',
        port: '',
        user: '',
        passwd: ''
      });

      expect(jquerydummy.html).toHaveBeenCalledWith('template');
      expect(jquerydummy.append).toHaveBeenCalledWith('entryTemplate');
      expect(jquerydummy.append).toHaveBeenCalledWith('modal');
      expect(window.App.clusterPlan.get).toHaveBeenCalledWith('config');
      expect(view.disableLaunchButton).toHaveBeenCalled();
    });

    it('should render symmetric view with config', function () {
      var jquerydummy = {
        html: function () {},
        append: function () {}
      };

      spyOn(window, '$').andCallFake(function (a) {
        return jquerydummy;
      });
      spyOn(jquerydummy, 'html');
      spyOn(jquerydummy, 'append');

      spyOn(window.App.clusterPlan, 'get').andReturn({
        useSSLonDBservers: true,
        useSSLonCoordinators: true,

        dispatchers: [
          {
            allowDBservers: undefined,
            allowCoordinators: undefined,
            endpoint: 'http://123.456.789:11',
            username: 'uu',
            passwd: 'passwd'

          },
          {
            allowDBservers: false,
            allowCoordinators: false,
            endpoint: 'http://123.456.799:12',
            username: 'uu2',
            passwd: 'passwd2'
          }
        ]
      });

      spyOn(view.template, 'render').andReturn('template');
      spyOn(view.modal, 'render').andReturn('modal');
      spyOn(view.entryTemplate, 'render').andReturn('entryTemplate');
      spyOn(view, 'disableLaunchButton');

      view.render(true);

      expect(view.template.render).toHaveBeenCalledWith({
        isSymmetric: true,
        params: {},
        useSSLonDBservers: true,
        useSSLonCoordinators: true
      });
      expect(view.entryTemplate.render).toHaveBeenCalledWith({
        isSymmetric: true,
        isFirst: true,
        host: '123.456.789',
        port: '11',
        isCoordinator: true,
        isDBServer: true,
        user: 'uu',
        passwd: 'passwd'
      });
      expect(view.entryTemplate.render).toHaveBeenCalledWith({
        isSymmetric: true,
        isFirst: false,
        host: '123.456.799',
        port: '12',
        isCoordinator: false,
        isDBServer: false,
        user: 'uu2',
        passwd: 'passwd2'
      });

      expect(jquerydummy.html).toHaveBeenCalledWith('template');
      expect(jquerydummy.append).toHaveBeenCalledWith('entryTemplate');
      expect(jquerydummy.append).toHaveBeenCalledWith('modal');
      expect(window.App.clusterPlan.get).toHaveBeenCalledWith('config');
      expect(view.disableLaunchButton).toHaveBeenCalled();
    });

    it('should autoCheckConnections', function () {
      var jquerydummy = {
        parent: function () {},
        children: function () {
          return jquerydummy;
        },
        val: function () {}
      };

      spyOn(window, '$').andCallFake(function (a) {
        return jquerydummy;
      });
      spyOn(jquerydummy, 'parent');
      spyOn(jquerydummy, 'val').andReturn('sometext');
      spyOn(jquerydummy, 'children').andCallThrough();

      spyOn(view, 'checkAllConnections');
      view.autoCheckConnections({currentTarget: 'abcde'});
      expect(view.checkAllConnections).toHaveBeenCalled();
      expect(window.$).toHaveBeenCalledWith('abcde');
      expect(jquerydummy.parent).toHaveBeenCalled();
      expect(jquerydummy.children).toHaveBeenCalledWith('.host');
      expect(jquerydummy.children).toHaveBeenCalledWith('.port');
      expect(jquerydummy.children).toHaveBeenCalledWith('.user');
      expect(jquerydummy.children).toHaveBeenCalledWith('.passwd');

      // expect(view.disableLaunchButton).toHaveBeenCalled()

    });

    it('should not autoCheckConnections as host and port are empty', function () {
      var jquerydummy = {
        parent: function () {},
        children: function () {
          return jquerydummy;
        },
        val: function () {}
      };

      spyOn(window, '$').andCallFake(function (a) {
        return jquerydummy;
      });
      spyOn(jquerydummy, 'parent');
      spyOn(jquerydummy, 'val').andReturn('');
      spyOn(jquerydummy, 'children').andCallThrough();

      spyOn(view, 'checkAllConnections');
      view.autoCheckConnections({currentTarget: 'abcde'});
      expect(view.checkAllConnections).not.toHaveBeenCalled();
      expect(window.$).toHaveBeenCalledWith('abcde');
      expect(jquerydummy.parent).toHaveBeenCalled();
      expect(jquerydummy.children).toHaveBeenCalledWith('.host');
      expect(jquerydummy.children).toHaveBeenCalledWith('.port');
      expect(jquerydummy.children).toHaveBeenCalledWith('.user');
      expect(jquerydummy.children).toHaveBeenCalledWith('.passwd');
    });

    it('should checkConnection with succes', function () {
      spyOn($, 'ajax').andCallFake(function (opt) {
        expect(opt.async).toEqual(true);
        expect(opt.cache).toEqual(false);
        expect(opt.type).toEqual('GET');
        expect(opt.xhrFields).toEqual({
          withCredentials: true
        });
        expect(opt.url).toEqual('http://' + 'host' + ':' + 'port' + '/_api/version');
        opt.success();
      });
      view.connectionValidationKey = 'connectionValidationKey';
      spyOn(view, 'checkDispatcherArray');
      view.checkConnection('host',
        'port',
        'user',
        'passwd',
        'target',
        1,
        ['d1', 'd2'],
        'connectionValidationKey'
      );
      expect(view.checkDispatcherArray).toHaveBeenCalledWith(
        ['d1', true], 'connectionValidationKey'
      );
    });

    it('should checkConnection with error', function () {
      spyOn($, 'ajax').andCallFake(function (opt) {
        expect(opt.async).toEqual(true);
        expect(opt.cache).toEqual(false);
        expect(opt.type).toEqual('GET');
        expect(opt.xhrFields).toEqual({
          withCredentials: true
        });
        expect(opt.url).toEqual('http://' + 'host' + ':' + 'port' + '/_api/version');
        opt.error();
      });
      view.connectionValidationKey = 'connectionValidationKey';
      spyOn(view, 'checkDispatcherArray');
      view.checkConnection('host',
        'port',
        'user',
        'passwd',
        'target',
        1,
        ['d1', 'd2'],
        'connectionValidationKey'
      );
      expect(view.checkDispatcherArray).not.toHaveBeenCalled();
    });

    it('should checkConnection with beforeSend', function () {
      var xhr = {
        setRequestHeader: function () {}
      };
      spyOn($, 'ajax').andCallFake(function (opt) {
        expect(opt.async).toEqual(true);
        expect(opt.cache).toEqual(false);
        expect(opt.type).toEqual('GET');
        expect(opt.xhrFields).toEqual({
          withCredentials: true
        });
        expect(opt.url).toEqual('http://' + 'host' + ':' + 'port' + '/_api/version');
        opt.beforeSend(xhr);
      });
      view.connectionValidationKey = 'connectionValidationKey';
      spyOn(view, 'checkDispatcherArray');

      spyOn(xhr, 'setRequestHeader');
      view.checkConnection('host',
        'port',
        'user',
        'passwd',
        'target',
        1,
        ['d1', 'd2'],
        'connectionValidationKey'
      );
      expect(view.checkDispatcherArray).not.toHaveBeenCalled();
      expect(xhr.setRequestHeader).toHaveBeenCalled();
      expect(xhr.setRequestHeader).toHaveBeenCalledWith(
        'X-Omit-Www-Authenticate', 'content'
      );
    });

    it('should checkConnection with exception', function () {
      spyOn($, 'ajax').andThrow('error');
      spyOn(view, 'disableLaunchButton');
      view.checkConnection('host',
        'port',
        'user',
        'passwd',
        'target',
        1,
        ['d1', 'd2'],
        'connectionValidationKey'
      );
      expect(view.disableLaunchButton).toHaveBeenCalled();
    });

    it('should checkDispatcherArray', function () {
      spyOn(view, 'enableLaunchButton').andCallThrough();
      var jquerydummy = {
        attr: function () {},
        removeClass: function () {},
        addClass: function () {}
      };

      spyOn(window, '$').andCallFake(function (a) {
        return jquerydummy;
      });
      spyOn(jquerydummy, 'attr');
      spyOn(jquerydummy, 'removeClass');
      spyOn(jquerydummy, 'addClass');
      view.connectionValidationKey = 'connectionValidationKey';
      view.checkDispatcherArray(
        ['d1', 'd2'],
        'connectionValidationKey'
      );

      expect(window.$).toHaveBeenCalledWith('#startSymmetricPlan');
      expect(jquerydummy.attr).toHaveBeenCalledWith('disabled', false);
      expect(jquerydummy.removeClass).toHaveBeenCalledWith('button-neutral');
      expect(jquerydummy.addClass).toHaveBeenCalledWith('button-success');

      expect(view.enableLaunchButton).toHaveBeenCalled();
    });

    it('should checkDispatcherArray with bad key', function () {
      spyOn(view, 'enableLaunchButton');
      view.connectionValidationKey = 'badKey';
      view.checkDispatcherArray(
        ['d1', 'd2'],
        'connectionValidationKey'
      );
      expect(view.enableLaunchButton).not.toHaveBeenCalled();
    });

    it('should checkAllConnections', function () {
      spyOn(view, 'disableLaunchButton');

      var jquerydummy = {
          length: 4,
          each: function () {}
        }, jquerydummyHost = {
          val: function () {
            return 'hostname';
          }
        }, jquerydummyPort = {
          val: function () {
            return '10';
          }
        }, jquerydummyPw = {
          val: function () {
            return 'password';
          }
        }, jquerydummyUser = {
          val: function () {
            return 'user';
          }
      };
      spyOn(window, '$').andCallFake(function (a, b) {
        if (a === '.host' && b === 'd') {
          return jquerydummyHost;
        }
        if (a === '.port' && b === 'd') {
          return jquerydummyPort;
        }
        if (a === '.passwd' && b === 'd') {
          return jquerydummyPw;
        }
        if (a === '.user' && b === 'd') {
          return jquerydummyUser;
        }
        return jquerydummy;
      });
      spyOn(jquerydummyHost, 'val').andCallThrough();
      spyOn(jquerydummyPort, 'val').andCallThrough();
      spyOn(jquerydummyUser, 'val').andCallThrough();
      spyOn(jquerydummyPw, 'val').andCallThrough();

      spyOn(jquerydummy, 'each').andCallFake(function (a) {
        a(1, 'd');
      });

      spyOn(view, 'checkConnection');
      view.checkAllConnections();

      expect(view.checkConnection).toHaveBeenCalledWith(
        'hostname',
        '10',
        'user',
        'password',
        undefined,
        1,
        [false, false, false, false],
        view.connectionValidationKey);
      expect(window.$).toHaveBeenCalledWith('.dispatcher');
      expect(view.disableLaunchButton).toHaveBeenCalled();
    });
    /*

            checkAllConnections: function() {
                this.connectionValidationKey = Math.random()
                this.disableLaunchButton()
                var numOfDispatcher = $('.dispatcher').length,
                    dispatcherArray = [],
                    idx
                for (idx = 0; idx < numOfDispatcher; idx++) {
                    dispatcherArray.push(false)
                }

                //Object mit #dispatcher + random key
                var self = this
                $('.dispatcher').each(
                    function(i, dispatcher) {
                        var target = $('.controls', dispatcher)[0]
                        var host = $('.host', dispatcher).val()
                        var port = $('.port', dispatcher).val()
                        var user = $('.user', dispatcher).val()
                        var passwd = $('.passwd', dispatcher).val()
                        self.checkConnection(
                            host,
                            port,
                            user,
                            passwd,
                            target,
                            i,
                            dispatcherArray,
                            self.connectionValidationKey
                        )
                    }
                )
            },

*/

  });
}());
