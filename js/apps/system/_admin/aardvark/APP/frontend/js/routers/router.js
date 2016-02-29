/*jshint unused: false */
/*global window, $, Backbone, document, arangoCollectionModel*/
/*global arangoHelper,dashboardView,arangoDatabase, _*/

(function () {
  "use strict";

  window.Router = Backbone.Router.extend({
    routes: {
      "": "dashboard",
      "dashboard": "dashboard",
      "collections": "collections",
      "new": "newCollection",
      "login": "login",
      "collection/:colid/documents/:pageid": "documents",
      "collection/:colid/:docid": "document",
      "shell": "shell",
      "query": "query2",
      "query2": "query",
      "queryManagement": "queryManagement",
      "workMonitor": "workMonitor",
      "databases": "databases",
      "applications": "applications",
      "applications/:mount": "applicationDetail",
      "graph": "graphManagement",
      "graph/:name": "showGraph",
      "userManagement": "userManagement",
      "userProfile": "userProfile",
      "logs": "logs",
      "test": "test"
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
        this.initFinished = true;
        this.arangoDatabase = new window.ArangoDatabase();
        this.currentDB = new window.CurrentDatabase();

        this.arangoCollectionsStore = new window.arangoCollections();
        this.arangoDocumentStore = new window.arangoDocument();
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
              userCollection: self.userCollection
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

    },

    logs: function (initialized) {
      this.checkUser();
      if (!initialized) {
        this.waitForInit(this.logs.bind(this));
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
      this.documentView.docid = docid;
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
    }
  });

}());
