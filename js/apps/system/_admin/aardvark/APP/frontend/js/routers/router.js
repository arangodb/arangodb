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
      "query": "query",
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
        this.arangoDatabase = new window.ArangoDatabase();
        this.currentDB = new window.CurrentDatabase();

        this.arangoCollectionsStore = new window.arangoCollections();
        this.arangoDocumentStore = new window.arangoDocument();
        arangoHelper.setDocumentStore(this.arangoDocumentStore);

        this.arangoCollectionsStore.fetch();

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

    checkUser: function () {
      if (this.userCollection.whoAmI() === null) {
        this.navigate("login", {trigger: true});
        return false;
      }
      this.initOnce();
      return true;
    },

    logs: function () {
      if (!this.checkUser()) {
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

    applicationDetail: function (mount) {
      if (!this.checkUser()) {
        return;
      }

      if (this.foxxList.length === 0) {
        this.foxxList.fetch({ async: false });
      }
      if (!this.hasOwnProperty('applicationDetailView')) {
        this.applicationDetailView = new window.ApplicationDetailView({
          model: this.foxxList.get(decodeURIComponent(mount))
        });
      }

      this.applicationDetailView.model = this.foxxList.get(decodeURIComponent(mount));
      this.applicationDetailView.render('swagger');
    },

    login: function () {
      if (this.userCollection.whoAmI() !== null) {
        this.navigate("", {trigger: true});
        return false;
      }
      if (!this.loginView) {
        this.loginView = new window.loginView({
          collection: this.userCollection
        });
      }
      this.loginView.render();
    },

    collections: function () {
      if (!this.checkUser()) {
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

    documents: function (colid, pageid) {
      if (!this.checkUser()) {
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

    document: function (colid, docid) {
      if (!this.checkUser()) {
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
      var type = arangoHelper.collectionApiType(colid);
      this.documentView.setType(type);
    },

    shell: function () {
      if (!this.checkUser()) {
        return;
      }
      if (!this.shellView) {
        this.shellView = new window.shellView();
      }
      this.shellView.render();
    },

    query: function () {
      if (!this.checkUser()) {
        return;
      }
      if (!this.queryView) {
        this.queryView = new window.queryView({
          collection: this.queryCollection
        });
      }
      this.queryView.render();
    },
    
    test: function () {
      if (!this.checkUser()) {
        return;
      }
      if (!this.testView) {
        this.testView = new window.testView({
        });
      }
      this.testView.render();
    },

    workMonitor: function () {
      if (!this.checkUser()) {
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

    queryManagement: function () {
      if (!this.checkUser()) {
        return;
      }
      if (!this.queryManagementView) {
        this.queryManagementView = new window.queryManagementView({
          collection: undefined
        });
      }
      this.queryManagementView.render();
    },

    databases: function () {
      if (!this.checkUser()) {
        return;
      }
      if (arangoHelper.databaseAllowed() === true) {
        if (! this.databaseView) {
          this.databaseView = new window.databaseView({
            users: this.userCollection,
            collection: this.arangoDatabase
          });
        }
        this.databaseView.render();
      } else {
        this.navigate("#", {trigger: true});
        $('#databaseNavi').css('display', 'none');
        $('#databaseNaviSelect').css('display', 'none');
      }
    },

    dashboard: function () {
      if (!this.checkUser()) {
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

    graphManagement: function () {
      if (!this.checkUser()) {
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

    showGraph: function (name) {
      if (!this.checkUser()) {
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
      this.graphManagementView.loadGraphViewer(name);
    },

    applications: function () {
      if (!this.checkUser()) {
        return;
      }
      if (this.applicationsView === undefined) {
        this.applicationsView = new window.ApplicationsView({
          collection: this.foxxList
        });
      }
      this.applicationsView.reload();
    },

    handleSelectDatabase: function () {
      if (!this.checkUser()) {
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
    },

    userManagement: function () {
      if (!this.checkUser()) {
        return;
      }
      if (!this.userManagementView) {
        this.userManagementView = new window.userManagementView({
          collection: this.userCollection
        });
      }
      this.userManagementView.render();
    },

    userProfile: function () {
      if (!this.checkUser()) {
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
