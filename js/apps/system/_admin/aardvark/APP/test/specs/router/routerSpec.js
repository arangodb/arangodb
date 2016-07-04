/* jshint browser: true */
/* jshint unused: false */
/* global describe, beforeEach, arangoHelper, afterEach, it, console, spyOn, expect*/
/* global $, jasmine, _*/

(function () {
  'use strict';

  describe('The router', function () {
    var jQueryDummy,
      width,
      fakeDB,
      storeDummy,
      naviDummy,
      footerDummy,
      documentDummy,
      documentsDummy,
      sessionDummy,
      foxxDummy,
      logsDummy,
      statisticBarDummy,
      collectionsViewDummy,
      databaseDummy,
      userBarDummy,
      graphManagementView,
      documentViewDummy,
      statisticsDescriptionCollectionDummy,
      statisticsCollectionDummy,
      dashboardDummy,
      documentsViewDummy;

    // Spy on all views that are initialized by startup
    beforeEach(function () {
      // Disable the binding of resize event. Causes tests to occasionally fail
      spyOn($.fn, 'resize');
      window.CreateDummyForObject(window, 'FoxxInstallView');
      naviDummy = {
        id: 'navi',
        render: function () {
          throw 'should be a spy';
        },
        handleResize: function () {
          throw 'should be a spy';
        },
        selectMenuItem: function () {
          throw 'should be a spy';
        },
        handleSelectDatabase: function () {
          throw 'should be a spy';
        }
      };
      footerDummy = {
        id: 'footer',
        render: function () {
          throw 'should be a spy';
        },
        handleResize: function () {
          throw 'should be a spy';
        },
        system: {}
      };
      documentDummy = {id: 'document'};
      documentsDummy = {id: 'documents'};
      storeDummy = {
        id: 'store',
        fetch: function (a) {
          if (a && a.success) {
            a.success();
          }
        }
      };
      logsDummy = {
        all: {
          id: 'logsAll',
          fetch: function (a) {
            a.success();
          }
        },
        1: {
          id: 'logs1',
          fetch: function (a) {
            a.success();
          }
        },
        2: {
          id: 'logs2',
          fetch: function (a) {
            a.success();
          }
        },
        3: {
          id: 'logs3',
          fetch: function (a) {
            a.success();
          }
        },
        4: {
          id: 'logs4',
          fetch: function (a) {
            a.success();
          }
        }
      };

      dashboardDummy = {
        id: 'dashboard',
        resize: function () {
          // This should throw as well however resizinge fails now and then
          return undefined;
        },
        stopUpdating: function () {
          throw 'should be a spy';
        },
        render: function () {
          throw 'should be a spy';
        }
      };
      statisticsDescriptionCollectionDummy = {
        id: 'statisticsDescription',
        fetch: function () {
          throw 'should be a spy';
        }
      };
      statisticsCollectionDummy = {
        id: 'statisticsCollection',
        fetch: function () {
          throw 'should be a spy';
        }
      };
      databaseDummy = {
        id: 'databaseCollection',
        fetch: function () {
          throw 'should be a spy';
        }
      };
      fakeDB = {
        id: 'fakeDB',
        fetch: function () {
          throw 'should be a spy';
        }
      };
      sessionDummy = {
        id: 'sessionDummy'
      };
      foxxDummy = {
        id: 'foxxDummy',
        fetch: function () {
          throw 'should be a spy';
        },
        findWhere: function (a) {
          return a._key;
        }
      };
      statisticBarDummy = {
        id: 'statisticBarDummy',
        render: function () {
          throw 'should be a spy';
        }
      };
      userBarDummy = {
        id: 'userBarDummy',
        render: function () {
          throw 'should be a spy';
        }
      };
      collectionsViewDummy = {
        render: function () {
          throw 'should be a spy';
        }
      };
      documentViewDummy = {
        render: function () {
          throw 'should be a spy';
        },
        setType: function () {
          throw 'should be a spy';
        }
      };
      graphManagementView = {
        render: function () {
          throw 'should be a spy';
        },
        handleResize: function () {
          return undefined;
        }
      };
      documentsViewDummy = {
        render: function () {
          throw 'should be a spy';
        },
        initTable: function () {
          throw 'should be a spy';
        },
        setCollectionId: function () {
          throw 'should be a spy';
        }
      };
      width = 255;
      jQueryDummy = {
        id: 'jquery',
        css: function () {
          throw 'should be a spy';
        },
        width: function () {
          return width;
        },
        resize: function (a) {
          a();
        },
        ajax: function (a, b) {
          return 'Peter';
        }
      };

      spyOn(window, '$').andReturn(jQueryDummy);
      spyOn(window, 'DocumentsView').andReturn(documentsViewDummy);
      spyOn(window, 'GraphManagementView').andReturn(graphManagementView);
      spyOn(window, 'DocumentView').andReturn(documentViewDummy);
      spyOn(window, 'arangoCollections').andReturn(storeDummy);
      spyOn(window, 'ArangoUsers').andReturn(sessionDummy);
      spyOn(window, 'arangoDocuments').andReturn(documentsDummy);
      spyOn(window, 'arangoDocument').andReturn(documentDummy);
      spyOn(window, 'ArangoDatabase').andReturn(databaseDummy);
      spyOn(window, 'FoxxCollection').andReturn(foxxDummy);
      spyOn(window, 'StatisticsDescriptionCollection').andReturn(
        statisticsDescriptionCollectionDummy
      );
      spyOn(window, 'StatisticsCollection').andReturn(statisticsCollectionDummy);
      spyOn(window, 'CollectionsView').andReturn(collectionsViewDummy);
      spyOn(window, 'ArangoLogs').andCallFake(function (opts) {
        if (opts.upto) {
          return logsDummy.all;
        }
        return logsDummy[opts.loglevel];
      });
      spyOn(window, 'FooterView').andReturn(footerDummy);
      spyOn(footerDummy, 'render');
      spyOn(window, 'NavigationView').andReturn(naviDummy);
      spyOn(naviDummy, 'render');
      spyOn(naviDummy, 'selectMenuItem');
      spyOn(window, 'CurrentDatabase').andReturn(fakeDB);
      spyOn(fakeDB, 'fetch').andCallFake(function (options) {
        expect(options.async).toBeFalsy();
      });
      spyOn(window, 'DBSelectionView');
      spyOn(window, 'DashboardView').andReturn(dashboardDummy);
      spyOn(window, 'StatisticBarView').andReturn(statisticBarDummy);
      spyOn(window, 'UserBarView').andReturn(userBarDummy);
      spyOn(window, 'ArangoQueries');

      spyOn(window, 'checkVersion');
    });

    afterEach(function () {
      // Remove all global values the router has created.
      delete window.modalView;
      delete window.foxxInstallView;
      delete window.progressView;
    });

    describe('initialization', function () {
      var r;

      beforeEach(function () {
        spyOn(storeDummy, 'fetch');
        r = new window.Router();
      });

      it('should create the foxx installer', function () {
        expect(window.FoxxInstallView).toHaveBeenCalledWith({
          collection: foxxDummy
        });
        expect(window.foxxInstallView).toBeDefined();
      });

      it('should trigger the version check', function () {
        expect(window.checkVersion).toHaveBeenCalled();
        expect(window.checkVersion.callCount).toEqual(1);
      });

      it('should create a Foxx Collection', function () {
        expect(window.FoxxCollection).toHaveBeenCalled();
      });

      it('should bind a resize event and call the handle Resize Method', function () {
        var e = new window.Router();
        spyOn(e, 'handleResize');
        e.initialize();
        expect(e.handleResize).toHaveBeenCalled();
      });

      it('should not create a documentsView', function () {
        expect(window.DocumentsView).not.toHaveBeenCalled();
      });

      it('should fetch the collectionsStore', function () {
        expect(storeDummy.fetch).toHaveBeenCalled();
      });

      it('should fetch the current db', function () {
        expect(window.CurrentDatabase).toHaveBeenCalled();
        expect(fakeDB.fetch).toHaveBeenCalled();
      });

      it('should handle resize', function () {
        spyOn(dashboardDummy , 'render');
        r.dashboard();
        spyOn(graphManagementView, 'render');
        r.graphManagement();
        spyOn(dashboardDummy , 'resize');
        spyOn(graphManagementView, 'handleResize');
        r.handleResize();
        expect(dashboardDummy.resize).toHaveBeenCalled();
        expect(graphManagementView.handleResize).toHaveBeenCalledWith(width);
      });
    });

    describe('navigation', function () {
      var r,
        simpleNavigationCheck;

      beforeEach(function () {
        r = new window.Router();
        spyOn(r, 'handleResize');
        simpleNavigationCheck = function (url, viewName, navTo, initObject,
          funcList, shouldNotRender, shouldNotCache) {
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
              throw 'should be a spy';
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
            naviDummy.selectMenuItem.reset();
            route();
            if (!r.hasOwnProperty(viewName)) {
              expect(window[viewName]).not.toHaveBeenCalled();
            }
            checkFuncExec();
          }
        };
      });

      it('should offer all necessary routes', function () {
        var available = _.keys(r.routes),
          expected = [
            '',
            'dashboard',
            'collections',
            'new',
            'login',
            'collection/:colid/documents/:pageid',
            'collection/:colid/:docid' ,
            'shell',
            'query',
            'logs',
            'api',
            'databases',
            'application/installed/:key',
            'application/available/:key',
            'applications',
            'graphManagement',
            'userManagement',
            'userProfile' ,
            'testing'
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

      it('should route to documents', function () {
        var colid = 5,
          pageid = 6;

        spyOn(documentsViewDummy, 'render');
        spyOn(documentsViewDummy, 'setCollectionId');
        spyOn(arangoHelper, 'collectionApiType').andReturn(1);
        simpleNavigationCheck(
          {
            url: 'collection/:colid/documents/:pageid',
            params: [colid, pageid]
          },
          'DocumentsView',
          '',
          undefined,
          {
          },
          true
        );
        expect(documentsViewDummy.setCollectionId).toHaveBeenCalledWith(colid, pageid);
      });

      it('should route to graphManagement', function () {
        spyOn(graphManagementView, 'render');
        simpleNavigationCheck(
          {
            url: 'graph'
          },
          'GraphManagementView',
          'graphviewer-menu',
          {
            collection: new window.GraphCollection(),
            collectionCollection: storeDummy
          },
          {
          },
          true
        );
      });

      it('should route to document', function () {
        var colid = 5,
          docid = 6;
        spyOn(arangoHelper, 'collectionApiType').andReturn(5);
        spyOn(documentViewDummy, 'render');
        spyOn(documentViewDummy, 'setType');
        simpleNavigationCheck(
          {
            url: 'collection/:colid/:docid',
            params: [colid, docid]
          },
          'DocumentView',
          '',
          undefined,
          {
          },
          true
        );
        expect(r.documentView.colid).toEqual(colid);
        expect(r.documentView.docid).toEqual(docid);
        expect(arangoHelper.collectionApiType).toHaveBeenCalledWith(colid);
        expect(documentViewDummy.setType).toHaveBeenCalledWith(colid);
      });

      it('should route to databases', function () {
        spyOn(arangoHelper, 'databaseAllowed').andReturn(true);
        simpleNavigationCheck(
          'databases',
          'databaseView',
          'databases-menu',
          {
            users: sessionDummy,
            collection: databaseDummy
          }
        );
      });

      it('should not route to databases and hide database navi', function () {
        spyOn(arangoHelper, 'databaseAllowed').andReturn(false);
        spyOn(r, 'navigate');
        spyOn(jQueryDummy, 'css');

        r.databases();
        expect(r.navigate).toHaveBeenCalledWith('#', {trigger: true});
        expect(naviDummy.selectMenuItem).toHaveBeenCalledWith('dashboard-menu');
        expect(jQueryDummy.css).toHaveBeenCalledWith('display', 'none');
      });

      it('should navigate to the dashboard', function () {
        spyOn(dashboardDummy, 'render');
        simpleNavigationCheck(
          'dashboard',
          'DashboardView',
          'dashboard-menu',
          {
            dygraphConfig: window.dygraphConfig,
            database: databaseDummy
          },
          {
          },
          true
        );
      });

      it('should call handleSelectDatabase in naviview', function () {
        spyOn(naviDummy, 'handleSelectDatabase');
        r.handleSelectDatabase();
        expect(naviDummy.handleSelectDatabase).toHaveBeenCalled();
      });

      it('should navigate to the userManagement', function () {
        simpleNavigationCheck(
          'userManagement',
          'userManagementView',
          'tools-menu',
          {
            collection: r.userCollection
          },
          {
          },
          false
        );
      });

      it('should navigate to the logs', function () {
        simpleNavigationCheck(
          'logs',
          'LogsView',
          'tools-menu',
          {
            logall: logsDummy.all,
            logdebug: logsDummy['4'],
            loginfo: logsDummy['3'],
            logwarning: logsDummy['2'],
            logerror: logsDummy['1']
          },
          {
          },
          false
        );
      });

      it('should navigate to the userProfile', function () {
        simpleNavigationCheck(
          'userProfile',
          'userManagementView',
          'tools-menu',
          {
            collection: r.userCollection
          },
          {
          },
          false
        );
      });

      it('should route to the login screen', function () {
        simpleNavigationCheck(
          'login',
          'loginView',
          '',
          {collection: sessionDummy}
        );
      });

      it('should route to the api tab', function () {
        simpleNavigationCheck(
          'api',
          'ApiView',
          'tools-menu'
        );
      });

      it('should route to the query tab', function () {
        simpleNavigationCheck(
          'query',
          'queryView',
          'query-menu'
        );
      });

      it('should route to the shell tab', function () {
        simpleNavigationCheck(
          'shell',
          'shellView',
          'tools-menu'
        );
      });

      it('should route to the applications tab', function () {
        simpleNavigationCheck(
          'applications',
          'ApplicationsView',
          'applications-menu',
          { collection: foxxDummy},
          {
            reload: undefined
          },
          true
        );
      });
    });

    describe('logsAllowed, checkUser, collections', function () {
      var r, ajaxFunction;

      beforeEach(function () {
        spyOn(jQueryDummy, 'ajax').andCallFake(function () {});
        r = new window.Router();
        spyOn(r, 'handleResize');
      });

      it('checkUser logged in', function () {
        sessionDummy.models = [1, 2];
        expect(r.checkUser()).toEqual(true);
      });
      it('checkUser not logged in', function () {
        spyOn(r, 'navigate');
        sessionDummy.models = [];
        expect(r.checkUser()).toEqual(false);
        expect(r.navigate).toHaveBeenCalledWith('login', {trigger: true});
      });

      it('collections', function () {
        spyOn(collectionsViewDummy, 'render');
        storeDummy.fetch = function (a) {
          a.success();
        };
        r.collections();
        expect(collectionsViewDummy.render).toHaveBeenCalled();
        expect(naviDummy.selectMenuItem).toHaveBeenCalledWith('collections-menu');
      });
    });
  });
}());
