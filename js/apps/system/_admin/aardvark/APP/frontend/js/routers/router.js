/*jshint unused: false */
/*global window, $, Backbone, document, arangoCollectionModel*/
/*global arangoHelper, btoa, dashboardView, arangoDatabase, _*/

(function () {
  "use strict";

  window.Router = Backbone.Router.extend({

    toUpdate: [],
    dbServers: [],
    isCluster: undefined,

    routes: {
      "": "cluster",
      "dashboard": "dashboard",
      "collections": "collections",
      "new": "newCollection",
      "login": "login",
      "collection/:colid/documents/:pageid": "documents",
      "collection/:colid/:docid": "document",
      "shell": "shell",
      "queries": "query2",
      "query2": "query",
      "workMonitor": "workMonitor",
      "databases": "databases",
      "services": "applications",
      "service/:mount": "applicationDetail",
      "graphs": "graphManagement",
      "graphs/:name": "showGraph",
      "users": "userManagement",
      "userProfile": "userProfile",
      "cluster": "cluster",
      "nodes": "nodes",
      "node/:name": "node",
      "logs": "logs",
      "test": "test"
    },

    execute: function(callback, args, name) {
      $('#subNavigationBar .breadcrumb').html('');
      if (callback) {
        callback.apply(this, args);
      }
    },

    checkUser: function () {
      var callback = function(error, user) {
        if (error || user === null) {
          this.navigate("login", {trigger: true});
        }
        else {
          this.initOnce();
        }
      }.bind(this);

      this.userCollection.whoAmI(callback); 
    },

    waitForInit: function(origin, param1, param2) {
      if (!this.initFinished) {
        setTimeout(function() {
          if (!param1) {
            origin(false);
          }
          if (param1 && !param2) {
            origin(param1, false);
          }
          if (param1 && param2) {
            origin(param1, param2, false);
          }
        }, 100);
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
        this.initOnce = function() {};

        var callback = function(error, isCoordinator) {
          self = this;
          if (isCoordinator) {
            self.isCluster = true;

            self.coordinatorCollection.fetch({
              success: function() {
                self.fetchDBS();
              }
            });
          }
          else {
            self.isCluster = false;
          }
        }.bind(this);

        window.isCoordinator(callback);

        this.initFinished = true;
        this.arangoDatabase = new window.ArangoDatabase();
        this.currentDB = new window.CurrentDatabase();

        this.arangoCollectionsStore = new window.arangoCollections();
        this.arangoDocumentStore = new window.arangoDocument();

        //Cluster 
        this.coordinatorCollection = new window.ClusterCoordinators();

        arangoHelper.setDocumentStore(this.arangoDocumentStore);

        this.arangoCollectionsStore.fetch();

        window.spotlightView = new window.SpotlightView({
          collection: this.arangoCollectionsStore 
        });

        this.footerView = new window.FooterView();
        this.notificationList = new window.NotificationCollection();

        this.currentDB.fetch({
          success: function() {
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
      }.bind(this);


      $(window).resize(function () {
        self.handleResize();
      });

      $(window).scroll(function () {
        //self.handleScroll();
      });

    },

    handleScroll: function() {
      if ($(window).scrollTop() > 50) {
        $('.navbar > .secondary').css('top', $(window).scrollTop());
        $('.navbar > .secondary').css('position', 'absolute');
        $('.navbar > .secondary').css('z-index', '10');
        $('.navbar > .secondary').css('width', $(window).width());
      }
      else {
        $('.navbar > .secondary').css('top', '0');
        $('.navbar > .secondary').css('position', 'relative');
        $('.navbar > .secondary').css('width', '');
      }
    },

    cluster: function (initialized) {
      this.checkUser();
      if (!initialized || this.isCluster === undefined) {
        this.waitForInit(this.cluster.bind(this));
        return;
      }
      if (this.isCluster === false) {
        this.routes[""] = 'dashboard';
        this.navigate("#dashboard", {trigger: true});
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

    node: function (name, initialized) {
      this.checkUser();
      if (!initialized || this.isCluster === undefined) {
        this.waitForInit(this.node.bind(this), name);
        return;
      }
      if (this.isCluster === false) {
        this.routes[""] = 'dashboard';
        this.navigate("#dashboard", {trigger: true});
        return;
      }

      if (!this.nodeView) {
        this.nodeView = new window.NodeView({
          coordname: name,
          coordinators: this.coordinatorCollection,
        });
      }
      this.nodeView.render();
    },

    nodeLogs: function (initialized) {

    },

    nodes: function (initialized) {
      this.checkUser();
      if (!initialized || this.isCluster === undefined) {
        this.waitForInit(this.nodes.bind(this));
        return;
      }
      if (this.isCluster === false) {
        this.routes[""] = 'dashboard';
        this.navigate("#dashboard", {trigger: true});
        return;
      }

      if (!this.nodesView) {
        this.nodesView = new window.NodesView({
          coordinators: this.coordinatorCollection,
          dbServers: this.dbServers
        });
      }
      this.nodesView.render();
    },

    addAuth: function (xhr) {
      var u = this.clusterPlan.get("user");
      if (!u) {
        xhr.abort();
        if (!this.isCheckingUser) {
          this.requestAuth();
        }
        return;
      }
      var user = u.name;
      var pass = u.passwd;
      var token = user.concat(":", pass);
      xhr.setRequestHeader('Authorization', "Basic " + btoa(token));
    },

    logs: function (name, initialized) {
      this.checkUser();
      if (!initialized) {
        this.waitForInit(this.logs.bind(this), name);
        return;
      }
      if (!this.logsView) {
        var newLogsAllCollection = new window.ArangoLogs(
          {upto: true, loglevel: 4}
        ),
        newLogsDebugCollection = new window.ArangoLogs(
          {loglevel: 4}
        ),
        newLogsInfoCollection = new window.ArangoLogs(
          {loglevel: 3}
        ),
        newLogsWarningCollection = new window.ArangoLogs(
          {loglevel: 2}
        ),
        newLogsErrorCollection = new window.ArangoLogs(
          {loglevel: 1}
        );
        this.logsView = new window.LogsView({
          logall: newLogsAllCollection,
          logdebug: newLogsDebugCollection,
          loginfo: newLogsInfoCollection,
          logwarning: newLogsWarningCollection,
          logerror: newLogsErrorCollection
        });
      }
      this.logsView.render();
    },

    applicationDetail: function (mount, initialized) {
      this.checkUser();
      if (!initialized) {
        this.waitForInit(this.applicationDetail.bind(this), mount);
        return;
      }
      var callback = function() {
        if (!this.hasOwnProperty('applicationDetailView')) {
          this.applicationDetailView = new window.ApplicationDetailView({
            model: this.foxxList.get(decodeURIComponent(mount))
          });
        }

        this.applicationDetailView.model = this.foxxList.get(decodeURIComponent(mount));
        this.applicationDetailView.render('swagger');
      }.bind(this);

      if (this.foxxList.length === 0) {
        this.foxxList.fetch({
          success: function() {
            callback();
          }
        });
      }
      else {
        callback();
      }
    },

    login: function (initialized) {
      var callback = function(error, user) {
        if (error || user === null) {
          if (!this.loginView) {
            this.loginView = new window.loginView({
              collection: this.userCollection
            });
          }
          this.loginView.render();
        }
        else {
          this.navigate("", {trigger: true});
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
      if (!this.collectionsView) {
        this.collectionsView = new window.CollectionsView({
          collection: this.arangoCollectionsStore
        });
      }
      this.arangoCollectionsStore.fetch({
        success: function () {
          self.collectionsView.render();
        }
      });
    },

    documents: function (colid, pageid, initialized) {
      this.checkUser();
      if (!initialized) {
        this.waitForInit(this.documents.bind(this), colid, pageid);
        return;
      }
      if (!this.documentsView) {
        this.documentsView = new window.DocumentsView({
          collection: new window.arangoDocuments(),
          documentStore: this.arangoDocumentStore,
          collectionsStore: this.arangoCollectionsStore
        });
      }
      this.documentsView.setCollectionId(colid, pageid);
      this.documentsView.render();

    },

    document: function (colid, docid, initialized) {
      this.checkUser();
      if (!initialized) {
        this.waitForInit(this.document.bind(this), colid, docid);
        return;
      }
      if (!this.documentView) {
        this.documentView = new window.DocumentView({
          collection: this.arangoDocumentStore
        });
      }
      this.documentView.colid = colid;

      var doc = window.location.hash.split("/")[2];
      var test = (doc.split("%").length - 1) % 3;

      if (decodeURI(doc) !== doc && test !== 0) {
        doc = decodeURIComponent(doc);
      }
      this.documentView.docid = doc;

      this.documentView.render();

      var callback = function(error, type) {
        if (!error) {
          this.documentView.setType(type);
        }
        else {
          console.log("Error", "Could not fetch collection type");
        }
      }.bind(this);

      arangoHelper.collectionApiType(colid, null, callback);
    },

    shell: function (initialized) {
      this.checkUser();
      if (!initialized) {
        this.waitForInit(this.shell.bind(this));
        return;
      }
      if (!this.shellView) {
        this.shellView = new window.shellView();
      }
      this.shellView.render();
    },

    query: function (initialized) {
      this.checkUser();
      if (!initialized) {
        this.waitForInit(this.query.bind(this));
        return;
      }
      if (!this.queryView) {
        this.queryView = new window.queryView({
          collection: this.queryCollection
        });
      }
      this.queryView.render();
    },

    query2: function (initialized) {
      this.checkUser();
      if (!initialized) {
        this.waitForInit(this.query2.bind(this));
        return;
      }
      if (!this.queryView2) {
        this.queryView2 = new window.queryView2({
          collection: this.queryCollection
        });
      }
      this.queryView2.render();
    },
    
    test: function (initialized) {
      this.checkUser();
      if (!initialized) {
        this.waitForInit(this.test.bind(this));
        return;
      }
      if (!this.testView) {
        this.testView = new window.testView({
        });
      }
      this.testView.render();
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
        this.workMonitorView = new window.workMonitorView({
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
        this.queryManagementView = new window.queryManagementView({
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

      var callback = function(error) {
        if (error) {
          arangoHelper.arangoError("DB","Could not get list of allowed databases");
          this.navigate("#", {trigger: true});
          $('#databaseNavi').css('display', 'none');
          $('#databaseNaviSelect').css('display', 'none');
        }
        else {
          if (! this.databaseView) {
            this.databaseView = new window.databaseView({
              users: this.userCollection,
              collection: this.arangoDatabase
            });
          }
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
      if (!this.graphManagementView) {
        this.graphManagementView =
        new window.GraphManagementView(
          {
            collection: new window.GraphCollection(),
            collectionCollection: this.arangoCollectionsStore
          }
        );
      }
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
      }
      else {
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
      if (this.graphManagementView) {
        this.graphManagementView.handleResize($("#content").width());
      }
      if (this.queryView) {
        this.queryView.resize();
      }
      if (this.queryView2) {
        this.queryView2.resize();
      }
    },

    userManagement: function (initialized) {
      this.checkUser();
      if (!initialized) {
        this.waitForInit(this.userManagement.bind(this));
        return;
      }
      if (!this.userManagementView) {
        this.userManagementView = new window.userManagementView({
          collection: this.userCollection
        });
      }
      this.userManagementView.render();
    },

    userProfile: function (initialized) {
      this.checkUser();
      if (!initialized) {
        this.waitForInit(this.userProfile.bind(this));
        return;
      }
      if (!this.userManagementView) {
        this.userManagementView = new window.userManagementView({
          collection: this.userCollection
        });
      }
      this.userManagementView.render(true);
    },
    
    fetchDBS: function() {
      var self = this;

      this.coordinatorCollection.each(function(coordinator) {
        self.dbServers.push(
          new window.ClusterServers([], {
            host: coordinator.get('address') 
          })
        );
      });           
      _.each(this.dbServers, function(dbservers) {
        dbservers.fetch();
      });
    },

    getNewRoute: function(host) {
      return "http://" + host;
    },

    registerForUpdate: function(o) {
      this.toUpdate.push(o);
      o.updateUrl();
    }

  });

}());
