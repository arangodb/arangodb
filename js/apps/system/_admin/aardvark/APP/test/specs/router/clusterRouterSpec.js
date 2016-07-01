/* jshint browser: true */
/* jshint unused: false */
/* global describe, beforeEach, btoa, arangoHelper, afterEach, it, console, spyOn, expect*/
/* global $, jasmine, _*/

(function () {
  'use strict';

  describe('The cluster router', function () {
    var showClusterViewDummy,
      naviDummy,
      statisticsDescriptionCollectionDummy,
      shutdownButtonViewDummy,
      serverDashboardViewDummy,
      planTestViewDummy,
      modalLoginDummy,
      arangoDocumentsDummy,
      planSymmetricViewDummy,
      planScenarioSelectorViewDummy,
      statisticsCollectionDummy,
      clusterPlanDummy,
      clusterDownDummy,
      showShardsDummy,
      footerViewDummy;

    // Spy on all views that are initialized by startup
    beforeEach(function () {
      showClusterViewDummy = {
        id: 'showCluster',
        stopUpdating: function () {
          return undefined;
        },
        render: function () {
          return undefined;
        },
        resize: function () {
          return undefined;
        }
      };
      shutdownButtonViewDummy = {
        id: 'shutdown',
        unrender: function () {
          return undefined;
        },
        render: function () {
          return undefined;
        }

      };
      modalLoginDummy = {
        id: 'modalLogin',
        render: function () {
          return undefined;
        }
      };

      serverDashboardViewDummy = {
        id: 'serverDashboard',
        stopUpdating: function () {
          return undefined;
        },
        resize: function () {
          return undefined;
        },
        render: function () {
          return undefined;
        }

      };

      footerViewDummy = {
        id: 'serverDashboard',
        render: function () {
          return undefined;
        }
      };

      clusterPlanDummy = {
        id: 'serverDashboard',
        returnValue: undefined,
        fetch: function () {
          return undefined;
        },
        get: function () {
          return this.returnValue;
        },
        set: function () {
          return undefined;
        },
        getCoordinator: function () {
          return undefined;
        }
      };

      planSymmetricViewDummy = {
        id: 'planSymmetricView',
        render: function () {
          return undefined;
        }
      };

      showShardsDummy = {
        id: 'showShardsView',
        render: function () {
          return undefined;
        }
      };

      planTestViewDummy = {
        id: 'planTestView',
        render: function () {
          return undefined;
        }
      };

      clusterDownDummy = {
        id: 'clusterDownView',
        render: function () {
          return undefined;
        }
      };
      planScenarioSelectorViewDummy = {
        id: 'planScenarioSelectorView',
        render: function () {
          return undefined;
        }
      };
      naviDummy = {
        id: 'navi',
        render: function () {
          return undefined;
        },
        handleResize: function () {
          return undefined;
        },
        selectMenuItem: function () {
          return undefined;
        },
        handleSelectDatabase: function () {
          return undefined;
        }
      };

      statisticsDescriptionCollectionDummy = {
        id: 'statisticsDescriptionCollection',
        fetch: function () {
          return undefined;
        }
      };
      statisticsCollectionDummy = {
        id: 'statisticsDescriptionCollection'
      };

      arangoDocumentsDummy = {};

      spyOn(window, 'FooterView').andReturn(footerViewDummy);
      spyOn(window, 'ClusterPlan').andReturn(clusterPlanDummy);
      spyOn(window, 'PlanSymmetricView').andReturn(planSymmetricViewDummy);
      spyOn(window, 'PlanScenarioSelectorView').andReturn(planScenarioSelectorViewDummy);
      spyOn(window, 'PlanTestView').andReturn(planTestViewDummy);
      spyOn(window, 'NavigationView').andReturn(naviDummy);
      spyOn(window, 'LoginModalView').andReturn(modalLoginDummy);
      spyOn(window, 'ClusterDownView').andReturn(clusterDownDummy);
      spyOn(window, 'ShowShardsView').andReturn(showShardsDummy);
      spyOn(window, 'StatisticsDescriptionCollection').andReturn(statisticsDescriptionCollectionDummy);
      spyOn(window, 'StatisticsCollection').andReturn(statisticsCollectionDummy);
      spyOn(window, 'ShowClusterView').andReturn(showClusterViewDummy);
      spyOn(window, 'ShutdownButtonView').andReturn(shutdownButtonViewDummy);
      spyOn(window, 'ServerDashboardView').andReturn(serverDashboardViewDummy);
      spyOn(window, 'arangoDocuments').andReturn(arangoDocumentsDummy);
    });

    describe('initialization', function () {
      var r;

      beforeEach(function () {
        spyOn(footerViewDummy, 'render');
        spyOn(clusterPlanDummy, 'fetch');
        r = new window.ClusterRouter();
      });

      it('should create a dyGraph config', function () {
        r.initialize();
        var a = r.dygraphConfig;
        expect(a).not.toBe(undefined);
      });

      it('isCheckingUser should equal false', function () {
        expect(r.isCheckingUser).toEqual(false);
      });

      it('should unrender the shutdownView', function () {
        spyOn(showClusterViewDummy, 'stopUpdating');
        spyOn(shutdownButtonViewDummy, 'unrender');
        spyOn(serverDashboardViewDummy, 'stopUpdating');

        var e = new window.ClusterRouter();
        spyOn(e, 'bind').andCallFake(function (a, func) {
          expect(a).toEqual('all');
          func('route', 'notShowCluster');
        });
        e.showClusterView = showClusterViewDummy;
        e.shutdownView = shutdownButtonViewDummy;
        e.dashboardView = serverDashboardViewDummy;
        e.initialize();
        expect(e.bind).toHaveBeenCalled();
        expect(showClusterViewDummy.stopUpdating).toHaveBeenCalled();
        expect(shutdownButtonViewDummy.unrender).toHaveBeenCalled();
        expect(serverDashboardViewDummy.stopUpdating).toHaveBeenCalled();
      });

      it('isCheckingUser should equal false', function () {
        expect(r.toUpdate).toEqual([]);
      });

      it('clusterPlan should have been called', function () {
        expect(window.ClusterPlan).toHaveBeenCalled();
        expect(clusterPlanDummy.fetch).toHaveBeenCalledWith({
          async: false
        });
      });

      it('footerView should have been called', function () {
        expect(window.FooterView).toHaveBeenCalled();
        expect(footerViewDummy.render).toHaveBeenCalled();
      });

      it('should bind a resize event and call the handle Resize Method', function () {
        var e = new window.ClusterRouter();
        spyOn(e, 'handleResize');
        spyOn(window, '$').andReturn({
          resize: function (a) {
            a();
          }
        });
        e.initialize();
        expect(e.handleResize).toHaveBeenCalled();
      });
    });

    describe('navigation', function () {
      var r,
        simpleNavigationCheck;

      beforeEach(function () {
        r = new window.ClusterRouter();
        simpleNavigationCheck =
          function (url, viewName, navTo,
            initObject, funcList, shouldNotRender, shouldNotCache) {
            var route,
              view = {},
              checkFuncExec = function () {
                _.each(funcList, function (v, f) {
                  if (v !== undefined) {
                    expect(view[f]).toHaveBeenCalledWith(v);
                  } else {
                    expect(view[f]).toHaveBeenCalled();
                  }
                });
            };
            funcList = funcList || {};
            if (_.isObject(url)) {
              route = function () {
                r[r.routes[url.url]].apply(r, url.params);
              };
            } else {
              route = r[r.routes[url]].bind(r);
            }
            if (!funcList.hasOwnProperty('render') && !shouldNotRender) {
              funcList.render = undefined;
            }
            _.each(_.keys(funcList), function (f) {
              view[f] = function () {
                return undefined;
              };
              spyOn(view, f);
            });
            expect(route).toBeDefined();
            try {
              spyOn(window, viewName).andReturn(view);
            } catch (e) {
              if (e.message !== viewName + ' has already been spied upon') {
                throw e;
              }
            }
            route();
            if (initObject) {
              expect(window[viewName]).toHaveBeenCalledWith(initObject);
            } else {
              expect(window[viewName]).toHaveBeenCalled();
            }
            checkFuncExec();
            if (navTo) {
              expect(naviDummy.selectMenuItem).toHaveBeenCalledWith(navTo);
            }
            // Check if the view is loaded from cache
            if (shouldNotCache !== true) {
              window[viewName].reset();
              _.each(_.keys(funcList), function (f) {
                view[f].reset();
              });
              route();
              if (!r.hasOwnProperty(viewName)) {
                expect(window[viewName]).not.toHaveBeenCalled();
              }
              checkFuncExec();
            }
        };
      });

      /*
       routes: {
         ""                       : "initialRoute",
         "planScenario"           : "planScenario",
         "planTest"               : "planTest",
         "planAsymmetrical"       : "planAsymmetric",
         "shards"                 : "showShards",
         "showCluster"            : "showCluster",
         "handleClusterDown"      : "handleClusterDown"
       },
       */

      it('should offer all necessary routes', function () {
        var available = _.keys(r.routes),
          expected = [
            '',
            'planScenario',
            'planTest',
            'planAsymmetrical',
            'shards',
            'showCluster',
            'handleClusterDown'
        ];
        this.addMatchers({
          toDefineTheRoutes: function (exp) {
            var avail = this.actual,
              leftDiff = _.difference(avail, exp),
              rightDiff = _.difference(exp, avail);
            this.message = function () {
              var msg = '';
              if (rightDiff.length) {
                msg += 'Expect routes: '
                  + rightDiff.join(' & ')
                  + ' to be available.\n';
              }
              if (leftDiff.length) {
                msg += 'Did not expect routes: '
                  + leftDiff.join(' & ')
                  + ' to be available.';
              }
              return msg;
            };
            return true;
          }
        });
        expect(available).toDefineTheRoutes(expected);
      });

      it('should route to initial route (planScenario)', function () {
        spyOn(planScenarioSelectorViewDummy, 'render');
        simpleNavigationCheck(
          '',
          'PlanScenarioSelectorView',
          '',
          undefined,
          {

          },
          true
        );
        expect(planScenarioSelectorViewDummy.render).toHaveBeenCalledWith();
      });

      it('should route to plan Test', function () {
        spyOn(planTestViewDummy, 'render');
        simpleNavigationCheck(
          'planTest',
          'PlanTestView',
          '',
          {model: r.clusterPlan},
          {

          },
          true
        );
        expect(planTestViewDummy.render).toHaveBeenCalledWith();
      });

      it('should route to planAsymmetrical', function () {
        spyOn(planSymmetricViewDummy, 'render');
        simpleNavigationCheck(
          'planAsymmetrical',
          'PlanSymmetricView',
          '',
          {model: r.clusterPlan},
          {

          },
          true
        );
        expect(planSymmetricViewDummy.render).toHaveBeenCalledWith(false);
      });

      it('should route to shards', function () {
        spyOn(showShardsDummy, 'render');
        simpleNavigationCheck(
          'shards',
          'ShowShardsView',
          '',
          undefined,
          {

          },
          true
        );
        expect(showShardsDummy.render).toHaveBeenCalledWith();
      });

      it('should route to clusterDown', function () {
        spyOn(clusterDownDummy, 'render');
        simpleNavigationCheck(
          'handleClusterDown',
          'ClusterDownView',
          '',
          undefined,
          {

          },
          true
        );
        expect(clusterDownDummy.render).toHaveBeenCalledWith();
      });

      it('should route to clusterView and verify call of showClusterView', function () {
        spyOn(showClusterViewDummy, 'render');
        simpleNavigationCheck(
          'showCluster',
          'ShowClusterView',
          '',
          {dygraphConfig: r.dygraphConfig},
          {

          },
          true
        );
        expect(showClusterViewDummy.render).toHaveBeenCalledWith();
      });

      it('should route to clusterView and verify call of ShutdownButtonView', function () {
        spyOn(shutdownButtonViewDummy, 'render');
        simpleNavigationCheck(
          'showCluster',
          'ShutdownButtonView',
          '',
          {overview: showClusterViewDummy},
          {

          },
          true
        );
        expect(shutdownButtonViewDummy.render).toHaveBeenCalledWith();
      });

      it('should resize the dashboardView', function () {
        spyOn(serverDashboardViewDummy, 'resize');
        spyOn(showClusterViewDummy, 'resize');
        r.dashboardView = serverDashboardViewDummy;
        r.showClusterView = showClusterViewDummy;
        r.handleResize();
        expect(serverDashboardViewDummy.resize).toHaveBeenCalled();
        expect(showClusterViewDummy.resize).toHaveBeenCalled();
      });

      it('should call dashboardView', function () {
        spyOn(serverDashboardViewDummy, 'render');
        r.dashboardView = serverDashboardViewDummy;
        r.serverToShow = 'fritz';
        r.dashboard();

        expect(serverDashboardViewDummy.render).toHaveBeenCalled();
        expect(window.ServerDashboardView).toHaveBeenCalledWith({
          dygraphConfig: window.dygraphConfig,
          serverToShow: 'fritz'
        });
      });

      it('should call dashboardView but navigate to ' +
        'initialRoute as no server is defined', function () {
          spyOn(statisticsDescriptionCollectionDummy, 'fetch');
          r.serverToShow = undefined;
          r.dashboard();
          expect(statisticsDescriptionCollectionDummy.fetch).not.toHaveBeenCalled();
        });

      it('should updateAllUrls', function () {
        var m1 = {
            updateUrl: function () {
              return undefined;
            }
          }, m2 = {
            updateUrl: function () {
              return undefined;
            }
        };
        spyOn(m1, 'updateUrl');
        spyOn(m2, 'updateUrl');
        r.toUpdate = [m1, m2];
        r.updateAllUrls();
        expect(m1.updateUrl).toHaveBeenCalled();
        expect(m2.updateUrl).toHaveBeenCalled();
      });

      it('should registerForUpdate', function () {
        var a,
          m1 = {
            updateUrl: function () {
              return undefined;
            }
          },
          m2 = {
            updateUrl: function () {
              return undefined;
            }
        };
        spyOn(m1, 'updateUrl');
        spyOn(m2, 'updateUrl');
        r.toUpdate = [];
        r.registerForUpdate(m1);
        r.registerForUpdate(m2);
        a = r.toUpdate;
        expect(a.length).toEqual(2);
        expect(m2.updateUrl).toHaveBeenCalled();
        expect(m1.updateUrl).toHaveBeenCalled();
      });

      it('should addAuth but fail due to missing user', function () {
        var xhr = {
          abort: function () {
            return undefined;
          },
          setRequestHeader: function () {
            return undefined;
          }
        };
        spyOn(clusterPlanDummy, 'get');
        spyOn(r, 'requestAuth');
        spyOn(xhr, 'abort');
        spyOn(xhr, 'setRequestHeader');
        r.isCheckingUser = false;
        r.addAuth(xhr);

        expect(clusterPlanDummy.get).toHaveBeenCalledWith('user');
        expect(xhr.abort).toHaveBeenCalled();
        expect(r.requestAuth).toHaveBeenCalled();
        expect(xhr.setRequestHeader).not.toHaveBeenCalled();
      });

      it('should addAuth', function () {
        var xhr = {
          abort: function () {
            return undefined;
          },
          setRequestHeader: function () {
            return undefined;
          }
        };
        spyOn(clusterPlanDummy, 'get').andReturn({
          name: 'heinz',
          passwd: 'pw'
        });
        spyOn(r, 'requestAuth');
        spyOn(xhr, 'setRequestHeader');
        spyOn(xhr, 'abort');
        r.isCheckingUser = false;
        r.addAuth(xhr);

        expect(clusterPlanDummy.get).toHaveBeenCalledWith('user');
        expect(xhr.abort).not.toHaveBeenCalled();
        expect(r.requestAuth).not.toHaveBeenCalled();
        expect(xhr.setRequestHeader).toHaveBeenCalledWith(
          'Authorization', 'Basic ' + btoa('heinz:pw')
        );
      });

      it('should requestAuth', function () {
        spyOn(modalLoginDummy, 'render');
        spyOn(clusterPlanDummy, 'set');
        r.requestAuth();
        expect(clusterPlanDummy.set).toHaveBeenCalledWith({'user': null});
        expect(window.LoginModalView).toHaveBeenCalled();
        expect(modalLoginDummy.render).toHaveBeenCalled();
      });

      it('should getNewRoute iof last was statistics', function () {
        spyOn(clusterPlanDummy, 'getCoordinator').andReturn('fritz');
        expect(r.getNewRoute('statistics')).toEqual('fritz/_admin/statistics');
        expect(clusterPlanDummy.getCoordinator).toHaveBeenCalled();
      });

      it('should getNewRoute', function () {
        spyOn(clusterPlanDummy, 'getCoordinator').andReturn('fritz');
        expect(r.getNewRoute('else')).toEqual('fritz/_admin/aardvark/cluster/else');
        expect(clusterPlanDummy.getCoordinator).toHaveBeenCalled();
      });
    });
  });
}());
