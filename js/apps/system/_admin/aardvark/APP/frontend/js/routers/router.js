/* jshint unused: false */
/* global window, $, Backbone, document, d3 */
/* global $, arangoHelper, btoa, _, frontendConfig */

(function () {
  'use strict';

  window.Router = Backbone.Router.extend({
    toUpdate: [],
    dbServers: [],
    isCluster: undefined,
    lastRoute: undefined,

    routes: {
      '': 'cluster',
      'dashboard': 'dashboard',
      'collections': 'collections',
      'new': 'newCollection',
      'login': 'login',
      'collection/:colid/documents/:pageid': 'documents',
      'cIndices/:colname': 'cIndices',
      'cSettings/:colname': 'cSettings',
      'cInfo/:colname': 'cInfo',
      'collection/:colid/:docid': 'document',
      'shell': 'shell',
      'queries': 'query',
      'workMonitor': 'workMonitor',
      'databases': 'databases',
      'settings': 'databases',
      'services': 'applications',
      'service/:mount': 'applicationDetail',
      'graphs': 'graphManagement',
      'graphs/:name': 'showGraph',
      'users': 'userManagement',
      'user/:name': 'userView',
      'user/:name/permission': 'userPermission',
      'userProfile': 'userProfile',
      'cluster': 'cluster',
      'nodes': 'nodes',
      'shards': 'shards',
      'node/:name': 'node',
      'nodeInfo/:id': 'nodeInfo',
      'logs': 'logger',
      'helpus': 'helpUs',
      'graph/:name': 'graph',
      'graph/:name/settings': 'graphSettings',
      'support': 'support'
    },

    execute: function (callback, args) {
      if (this.lastRoute === '#queries') {
        // cleanup input editors
        this.queryView.removeInputEditors();
        // cleanup old canvas elements
        this.queryView.cleanupGraphs();
        // cleanup old ace instances
        this.queryView.removeResults();
      }

      if (this.lastRoute) {
        if (this.lastRoute.substr(0, 11) === '#collection' && this.lastRoute.split('/').length === 3) {
          this.documentView.cleanupEditor();
        }

        if (this.lastRoute === '#dasboard' || window.location.hash.substr(0, 5) === '#node') {
          // dom graph cleanup
          d3.selectAll('svg > *').remove();
        }

        if (this.lastRoute === '#logger') {
          if (this.loggerView.logLevelView) {
            this.loggerView.logLevelView.remove();
          }
          if (this.loggerView.logTopicView) {
            this.loggerView.logTopicView.remove();
          }
        }
      }

      this.lastRoute = window.location.hash;
      // this function executes before every route call
      $('#subNavigationBar .breadcrumb').html('');
      $('#subNavigationBar .bottom').html('');
      $('#loadingScreen').hide();
      $('#content').show();
      if (callback) {
        callback.apply(this, args);
      }

      if (this.graphViewer) {
        if (this.graphViewer.graphSettingsView) {
          this.graphViewer.graphSettingsView.hide();
        }
      }
      if (this.queryView) {
        if (this.queryView.graphViewer) {
          if (this.queryView.graphViewer.graphSettingsView) {
            this.queryView.graphViewer.graphSettingsView.hide();
          }
        }
      }
    },

    listenerFunctions: {},

    listener: function (event) {
      _.each(window.App.listenerFunctions, function (func, key) {
        func(event);
      });
    },

    checkUser: function () {
      var self = this;

      if (window.location.hash === '#login') {
        return;
      }

      var startInit = function () {
        this.initOnce();

        // show hidden by default divs
        $('.bodyWrapper').show();
        $('.navbar').show();
      }.bind(this);

      var callback = function (error, user) {
        if (frontendConfig.authenticationEnabled) {
          self.currentUser = user;
          if (error || user === null) {
            if (window.location.hash !== '#login') {
              this.navigate('login', {trigger: true});
            }
          } else {
            startInit();
          }
        } else {
          startInit();
        }
      }.bind(this);

      if (frontendConfig.authenticationEnabled) {
        this.userCollection.whoAmI(callback);
      } else {
        this.initOnce();

        // show hidden by default divs
        $('.bodyWrapper').show();
        $('.navbar').show();
      }
    },

    waitForInit: function (origin, param1, param2) {
      if (!this.initFinished) {
        setTimeout(function () {
          if (!param1) {
            origin(false);
          }
          if (param1 && !param2) {
            origin(param1, false);
          }
          if (param1 && param2) {
            origin(param1, param2, false);
          }
        }, 350);
      } else {
        if (!param1) {
          origin(true);
        }
        if (param1 && !param2) {
          origin(param1, true);
        }
        if (param1 && param2) {
          origin(param1, param2, true);
        }
      }
    },

    initFinished: false,

    initialize: function () {
      // check frontend config for global conf settings
      if (frontendConfig.isCluster === true) {
        this.isCluster = true;
      }

      document.addEventListener('keyup', this.listener, false);

      // This should be the only global object
      window.modalView = new window.ModalView();

      this.foxxList = new window.FoxxCollection();
      window.foxxInstallView = new window.FoxxInstallView({
        collection: this.foxxList
      });
      window.progressView = new window.ProgressView();

      var self = this;

      this.userCollection = new window.ArangoUsers();

      this.initOnce = function () {
        this.initOnce = function () {};

        var callback = function (error, isCoordinator) {
          self = this;
          if (isCoordinator === true) {
            self.coordinatorCollection.fetch({
              success: function () {
                self.fetchDBS();
              }
            });
          }
          if (error) {
            console.log(error);
          }
        }.bind(this);

        window.isCoordinator(callback);

        if (frontendConfig.isCluster === false) {
          this.initFinished = true;
        }

        this.arangoDatabase = new window.ArangoDatabase();
        this.currentDB = new window.CurrentDatabase();

        this.arangoCollectionsStore = new window.ArangoCollections();
        this.arangoDocumentStore = new window.ArangoDocument();

        // Cluster
        this.coordinatorCollection = new window.ClusterCoordinators();

        arangoHelper.setDocumentStore(this.arangoDocumentStore);

        this.arangoCollectionsStore.fetch({
          cache: false
        });

        window.spotlightView = new window.SpotlightView({
          collection: this.arangoCollectionsStore
        });

        this.footerView = new window.FooterView({
          collection: self.coordinatorCollection
        });
        this.notificationList = new window.NotificationCollection();

        this.currentDB.fetch({
          cache: false,
          success: function () {
            self.naviView = new window.NavigationView({
              database: self.arangoDatabase,
              currentDB: self.currentDB,
              notificationCollection: self.notificationList,
              userCollection: self.userCollection,
              isCluster: self.isCluster
            });
            self.naviView.render();
          }
        });

        this.queryCollection = new window.ArangoQueries();

        this.footerView.render();

        window.checkVersion();

        this.userConfig = new window.UserConfig();
        this.userConfig.fetch();

        this.documentsView = new window.DocumentsView({
          collection: new window.ArangoDocuments(),
          documentStore: this.arangoDocumentStore,
          collectionsStore: this.arangoCollectionsStore
        });

        arangoHelper.initSigma();
      }.bind(this);

      $(window).resize(function () {
        self.handleResize();
      });

      $(window).scroll(function () {
        // self.handleScroll()
      });
    },

    handleScroll: function () {
      if ($(window).scrollTop() > 50) {
        $('.navbar > .secondary').css('top', $(window).scrollTop());
        $('.navbar > .secondary').css('position', 'absolute');
        $('.navbar > .secondary').css('z-index', '10');
        $('.navbar > .secondary').css('width', $(window).width());
      } else {
        $('.navbar > .secondary').css('top', '0');
        $('.navbar > .secondary').css('position', 'relative');
        $('.navbar > .secondary').css('width', '');
      }
    },

    cluster: function (initialized) {
      this.checkUser();
      if (!initialized) {
        this.waitForInit(this.cluster.bind(this));
        return;
      }
      if (this.isCluster === false || this.isCluster === undefined) {
        if (this.currentDB.get('name') === '_system') {
          this.routes[''] = 'dashboard';
          this.navigate('#dashboard', {trigger: true});
        } else {
          this.routes[''] = 'collections';
          this.navigate('#collections', {trigger: true});
        }
        return;
      }

      if (!this.clusterView) {
        this.clusterView = new window.ClusterView({
          coordinators: this.coordinatorCollection,
          dbServers: this.dbServers
        });
      }
      this.clusterView.render();
    },

    node: function (id, initialized) {
      this.checkUser();
      if (!initialized || this.isCluster === undefined) {
        this.waitForInit(this.node.bind(this), id);
        return;
      }
      if (this.isCluster === false) {
        this.routes[''] = 'dashboard';
        this.navigate('#dashboard', {trigger: true});
        return;
      }

      if (this.nodeView) {
        this.nodeView.remove();
      }
      this.nodeView = new window.NodeView({
        coordid: id,
        coordinators: this.coordinatorCollection,
        dbServers: this.dbServers
      });
      this.nodeView.render();
    },

    nodeInfo: function (id, initialized) {
      this.checkUser();
      if (!initialized || this.isCluster === undefined) {
        this.waitForInit(this.nodeInfo.bind(this), id);
        return;
      }
      if (this.isCluster === false) {
        this.routes[''] = 'dashboard';
        this.navigate('#dashboard', {trigger: true});
        return;
      }

      if (this.nodeInfoView) {
        this.nodeInfoView.remove();
      }
      this.nodeInfoView = new window.NodeInfoView({
        nodeId: id,
        coordinators: this.coordinatorCollection,
        dbServers: this.dbServers[0]
      });
      this.nodeInfoView.render();
    },

    shards: function (initialized) {
      this.checkUser();
      if (!initialized || this.isCluster === undefined) {
        this.waitForInit(this.shards.bind(this));
        return;
      }
      if (this.isCluster === false) {
        this.routes[''] = 'dashboard';
        this.navigate('#dashboard', {trigger: true});
        return;
      }
      if (!this.shardsView) {
        this.shardsView = new window.ShardsView({
          dbServers: this.dbServers
        });
      }
      this.shardsView.render();
    },

    nodes: function (initialized) {
      this.checkUser();
      if (!initialized || this.isCluster === undefined) {
        this.waitForInit(this.nodes.bind(this));
        return;
      }
      if (this.isCluster === false) {
        this.routes[''] = 'dashboard';
        this.navigate('#dashboard', {trigger: true});
        return;
      }
      if (this.nodesView) {
        this.nodesView.remove();
      }
      this.nodesView = new window.NodesView({
      });
      this.nodesView.render();
    },

    cNodes: function (initialized) {
      this.checkUser();
      if (!initialized || this.isCluster === undefined) {
        this.waitForInit(this.cNodes.bind(this));
        return;
      }
      if (this.isCluster === false) {
        this.routes[''] = 'dashboard';
        this.navigate('#dashboard', {trigger: true});
        return;
      }
      this.nodesView = new window.NodesView({
        coordinators: this.coordinatorCollection,
        dbServers: this.dbServers[0],
        toRender: 'coordinator'
      });
      this.nodesView.render();
    },

    dNodes: function (initialized) {
      this.checkUser();
      if (!initialized || this.isCluster === undefined) {
        this.waitForInit(this.dNodes.bind(this));
        return;
      }
      if (this.isCluster === false) {
        this.routes[''] = 'dashboard';
        this.navigate('#dashboard', {trigger: true});
        return;
      }
      if (this.dbServers.length === 0) {
        this.navigate('#cNodes', {trigger: true});
        return;
      }

      this.nodesView = new window.NodesView({
        coordinators: this.coordinatorCollection,
        dbServers: this.dbServers[0],
        toRender: 'dbserver'
      });
      this.nodesView.render();
    },

    sNodes: function (initialized) {
      this.checkUser();
      if (!initialized || this.isCluster === undefined) {
        this.waitForInit(this.sNodes.bind(this));
        return;
      }
      if (this.isCluster === false) {
        this.routes[''] = 'dashboard';
        this.navigate('#dashboard', {trigger: true});
        return;
      }

      this.scaleView = new window.ScaleView({
        coordinators: this.coordinatorCollection,
        dbServers: this.dbServers[0]
      });
      this.scaleView.render();
    },

    addAuth: function (xhr) {
      var u = this.clusterPlan.get('user');
      if (!u) {
        xhr.abort();
        if (!this.isCheckingUser) {
          this.requestAuth();
        }
        return;
      }
      var user = u.name;
      var pass = u.passwd;
      var token = user.concat(':', pass);
      xhr.setRequestHeader('Authorization', 'Basic ' + btoa(token));
    },

    logger: function (name, initialized) {
      this.checkUser();
      if (!initialized) {
        this.waitForInit(this.logger.bind(this), name);
        return;
      }
      if (!this.loggerView) {
        var co = new window.ArangoLogs(
          {upto: true, loglevel: 4}
        );
        this.loggerView = new window.LoggerView({
          collection: co
        });
      }
      this.loggerView.render();
    },

    applicationDetail: function (mount, initialized) {
      this.checkUser();
      if (!initialized) {
        this.waitForInit(this.applicationDetail.bind(this), mount);
        return;
      }
      var callback = function () {
        if (this.hasOwnProperty('applicationDetailView')) {
          this.applicationDetailView.remove();
        }
        this.applicationDetailView = new window.ApplicationDetailView({
          model: this.foxxList.get(decodeURIComponent(mount))
        });

        this.applicationDetailView.model = this.foxxList.get(decodeURIComponent(mount));
        this.applicationDetailView.render('swagger');
      }.bind(this);

      if (this.foxxList.length === 0) {
        this.foxxList.fetch({
          cache: false,
          success: function () {
            callback();
          }
        });
      } else {
        callback();
      }
    },

    login: function () {
      var callback = function (error, user) {
        if (!this.loginView) {
          this.loginView = new window.LoginView({
            collection: this.userCollection
          });
        }
        if (error || user === null) {
          this.loginView.render();
        } else {
          this.loginView.render(true);
        }
      }.bind(this);

      this.userCollection.whoAmI(callback);
    },

    collections: function (initialized) {
      this.checkUser();
      if (!initialized) {
        this.waitForInit(this.collections.bind(this));
        return;
      }
      var self = this;
      if (this.collectionsView) {
        this.collectionsView.remove();
      }
      this.collectionsView = new window.CollectionsView({
        collection: this.arangoCollectionsStore
      });
      this.arangoCollectionsStore.fetch({
        cache: false,
        success: function () {
          self.collectionsView.render();
        }
      });
    },

    cIndices: function (colname, initialized) {
      var self = this;

      this.checkUser();
      if (!initialized) {
        this.waitForInit(this.cIndices.bind(this), colname);
        return;
      }
      this.arangoCollectionsStore.fetch({
        cache: false,
        success: function () {
          self.indicesView = new window.IndicesView({
            collectionName: colname,
            collection: self.arangoCollectionsStore.findWhere({
              name: colname
            })
          });
          self.indicesView.render();
        }
      });
    },

    cSettings: function (colname, initialized) {
      var self = this;

      this.checkUser();
      if (!initialized) {
        this.waitForInit(this.cSettings.bind(this), colname);
        return;
      }
      this.arangoCollectionsStore.fetch({
        cache: false,
        success: function () {
          self.settingsView = new window.SettingsView({
            collectionName: colname,
            collection: self.arangoCollectionsStore.findWhere({
              name: colname
            })
          });
          self.settingsView.render();
        }
      });
    },

    cInfo: function (colname, initialized) {
      var self = this;

      this.checkUser();
      if (!initialized) {
        this.waitForInit(this.cInfo.bind(this), colname);
        return;
      }
      this.arangoCollectionsStore.fetch({
        cache: false,
        success: function () {
          self.infoView = new window.InfoView({
            collectionName: colname,
            collection: self.arangoCollectionsStore.findWhere({
              name: colname
            })
          });
          self.infoView.render();
        }
      });
    },

    documents: function (colid, pageid, initialized) {
      this.checkUser();
      if (!initialized) {
        this.waitForInit(this.documents.bind(this), colid, pageid);
        return;
      }
      if (this.documentsView) {
        this.documentsView.unbindEvents();
      }
      if (!this.documentsView) {
        this.documentsView = new window.DocumentsView({
          collection: new window.ArangoDocuments(),
          documentStore: this.arangoDocumentStore,
          collectionsStore: this.arangoCollectionsStore
        });
      }
      this.documentsView.setCollectionId(colid, pageid);
      this.documentsView.render();
      this.documentsView.delegateEvents();
    },

    document: function (colid, docid, initialized) {
      this.checkUser();
      if (!initialized) {
        this.waitForInit(this.document.bind(this), colid, docid);
        return;
      }
      var mode;
      if (this.documentView) {
        if (this.documentView.defaultMode) {
          mode = this.documentView.defaultMode;
        }
        this.documentView.remove();
      }
      this.documentView = new window.DocumentView({
        collection: this.arangoDocumentStore
      });
      this.documentView.colid = colid;
      this.documentView.defaultMode = mode;

      var doc = window.location.hash.split('/')[2];
      var test = (doc.split('%').length - 1) % 3;

      if (decodeURI(doc) !== doc && test !== 0) {
        doc = decodeURIComponent(doc);
      }
      this.documentView.docid = doc;

      this.documentView.render();

      var callback = function (error, type) {
        if (!error) {
          this.documentView.setType();
        } else {
          console.log('Error', 'Could not fetch collection type');
        }
      }.bind(this);

      arangoHelper.collectionApiType(colid, null, callback);
    },

    query: function (initialized) {
      this.checkUser();
      if (!initialized) {
        this.waitForInit(this.query.bind(this));
        return;
      }
      if (!this.queryView) {
        this.queryView = new window.QueryView({
          collection: this.queryCollection
        });
      }
      this.queryView.render();
    },

    graph: function (name, initialized) {
      this.checkUser();
      if (!initialized) {
        this.waitForInit(this.graph.bind(this), name);
        return;
      }

      // TODO better manage mechanism for both gv's
      if (this.graphViewer) {
        if (this.graphViewer.graphSettingsView) {
          this.graphViewer.graphSettingsView.remove();
        }
        this.graphViewer.killCurrentGraph();
        this.graphViewer.unbind();
        this.graphViewer.remove();
      }

      this.graphViewer = new window.GraphViewer({
        name: name,
        documentStore: this.arangoDocumentStore,
        collection: new window.GraphCollection(),
        userConfig: this.userConfig
      });
      this.graphViewer.render();
    },

    graphSettings: function (name, initialized) {
      this.checkUser();
      if (!initialized) {
        this.waitForInit(this.graphSettings.bind(this), name);
        return;
      }
      if (this.graphSettingsView) {
        this.graphSettingsView.remove();
      }
      this.graphSettingsView = new window.GraphSettingsView({
        name: name,
        userConfig: this.userConfig
      });
      this.graphSettingsView.render();
    },

    helpUs: function (initialized) {
      this.checkUser();
      if (!initialized) {
        this.waitForInit(this.helpUs.bind(this));
        return;
      }
      if (!this.testView) {
        this.helpUsView = new window.HelpUsView({
        });
      }
      this.helpUsView.render();
    },

    support: function (initialized) {
      this.checkUser();
      if (!initialized) {
        this.waitForInit(this.support.bind(this));
        return;
      }
      if (!this.testView) {
        this.supportView = new window.SupportView({
        });
      }
      this.supportView.render();
    },

    workMonitor: function (initialized) {
      this.checkUser();
      if (!initialized) {
        this.waitForInit(this.workMonitor.bind(this));
        return;
      }
      if (!this.workMonitorCollection) {
        this.workMonitorCollection = new window.WorkMonitorCollection();
      }
      if (!this.workMonitorView) {
        this.workMonitorView = new window.WorkMonitorView({
          collection: this.workMonitorCollection
        });
      }
      this.workMonitorView.render();
    },

    queryManagement: function (initialized) {
      this.checkUser();
      if (!initialized) {
        this.waitForInit(this.queryManagement.bind(this));
        return;
      }
      if (!this.queryManagementView) {
        this.queryManagementView = new window.QueryManagementView({
          collection: undefined
        });
      }
      this.queryManagementView.render();
    },

    databases: function (initialized) {
      this.checkUser();
      if (!initialized) {
        this.waitForInit(this.databases.bind(this));
        return;
      }

      var callback = function (error) {
        if (error) {
          arangoHelper.arangoError('DB', 'Could not get list of allowed databases');
          this.navigate('#', {trigger: true});
          $('#databaseNavi').css('display', 'none');
          $('#databaseNaviSelect').css('display', 'none');
        } else {
          if (this.databaseView) {
            // cleanup events and view
            this.databaseView.remove();
          }
          this.databaseView = new window.DatabaseView({
            users: this.userCollection,
            collection: this.arangoDatabase
          });
          this.databaseView.render();
        }
      }.bind(this);

      arangoHelper.databaseAllowed(callback);
    },

    dashboard: function (initialized) {
      this.checkUser();
      if (!initialized) {
        this.waitForInit(this.dashboard.bind(this));
        return;
      }

      if (this.dashboardView === undefined) {
        this.dashboardView = new window.DashboardView({
          dygraphConfig: window.dygraphConfig,
          database: this.arangoDatabase
        });
      }
      this.dashboardView.render();
    },

    graphManagement: function (initialized) {
      this.checkUser();
      if (!initialized) {
        this.waitForInit(this.graphManagement.bind(this));
        return;
      }
      if (this.graphManagementView) {
        this.graphManagementView.undelegateEvents();
      }
      this.graphManagementView =
        new window.GraphManagementView(
          {
            collection: new window.GraphCollection(),
            collectionCollection: this.arangoCollectionsStore
          }
      );
      this.graphManagementView.render();
    },

    showGraph: function (name, initialized) {
      this.checkUser();
      if (!initialized) {
        this.waitForInit(this.showGraph.bind(this), name);
        return;
      }
      if (!this.graphManagementView) {
        this.graphManagementView =
          new window.GraphManagementView(
            {
              collection: new window.GraphCollection(),
              collectionCollection: this.arangoCollectionsStore
            }
        );
        this.graphManagementView.render(name, true);
      } else {
        this.graphManagementView.loadGraphViewer(name);
      }
    },

    applications: function (initialized) {
      this.checkUser();
      if (!initialized) {
        this.waitForInit(this.applications.bind(this));
        return;
      }
      if (this.applicationsView === undefined) {
        this.applicationsView = new window.ApplicationsView({
          collection: this.foxxList
        });
      }
      this.applicationsView.reload();
    },

    handleSelectDatabase: function (initialized) {
      this.checkUser();
      if (!initialized) {
        this.waitForInit(this.handleSelectDatabase.bind(this));
        return;
      }
      this.naviView.handleSelectDatabase();
    },

    handleResize: function () {
      if (this.dashboardView) {
        this.dashboardView.resize();
      }
      if (this.graphManagementView && Backbone.history.getFragment() === 'graphs') {
        this.graphManagementView.handleResize($('#content').width());
      }
      if (this.queryView && Backbone.history.getFragment() === 'queries') {
        this.queryView.resize();
      }
      if (this.naviView) {
        this.naviView.resize();
      }
      if (this.graphViewer && Backbone.history.getFragment().indexOf('graph') > -1) {
        this.graphViewer.resize();
      }
      if (this.documentsView && Backbone.history.getFragment().indexOf('documents') > -1) {
        this.documentsView.resize();
      }
      if (this.documentView && Backbone.history.getFragment().indexOf('collection') > -1) {
        this.documentView.resize();
      }
    },

    userPermission: function (name, initialized) {
      this.checkUser();
      if (initialized || initialized === null) {
        if (this.userPermissionView) {
          this.userPermissionView.remove();
        }

        this.userPermissionView = new window.UserPermissionView({
          collection: this.userCollection,
          databases: this.arangoDatabase,
          username: name
        });

        this.userPermissionView.render();
      } else if (initialized === false) {
        this.waitForInit(this.userPermissionView.bind(this), name);
        return;
      }
    },

    userView: function (name, initialized) {
      this.checkUser();
      if (initialized || initialized === null) {
        this.userView = new window.UserView({
          collection: this.userCollection,
          username: name
        });
        this.userView.render();
      } else if (initialized === false) {
        this.waitForInit(this.userView.bind(this), name);
      }
    },

    userManagement: function (initialized) {
      this.checkUser();
      if (!initialized) {
        this.waitForInit(this.userManagement.bind(this));
        return;
      }
      if (this.userManagementView) {
        this.userManagementView.remove();
      }

      this.userManagementView = new window.UserManagementView({
        collection: this.userCollection
      });
      this.userManagementView.render();
    },

    userProfile: function (initialized) {
      this.checkUser();
      if (!initialized) {
        this.waitForInit(this.userProfile.bind(this));
        return;
      }
      if (!this.userManagementView) {
        this.userManagementView = new window.UserManagementView({
          collection: this.userCollection
        });
      }
      this.userManagementView.render(true);
    },

    fetchDBS: function (callback) {
      var self = this;
      var cb = false;

      this.coordinatorCollection.each(function (coordinator) {
        self.dbServers.push(
          new window.ClusterServers([], {
            host: coordinator.get('address')
          })
        );
      });

      this.initFinished = true;

      _.each(this.dbServers, function (dbservers) {
        dbservers.fetch({
          success: function () {
            if (cb === false) {
              if (callback) {
                callback();
                cb = true;
              }
            }
          }
        });
      });
    },

    getNewRoute: function (host) {
      return 'http://' + host;
    },

    registerForUpdate: function (o) {
      this.toUpdate.push(o);
      o.updateUrl();
    }

  });
}());
