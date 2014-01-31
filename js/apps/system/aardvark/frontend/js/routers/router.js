/*jslint indent: 2, nomen: true, maxlen: 100, sloppy: true, vars: true, white: true, plusplus: true, newcap: true */
/*global window, $, Backbone, document, arangoCollection,arangoHelper,dashboardView,arangoDatabase*/

(function() {
  "use strict";

  window.Router = Backbone.Router.extend({
    routes: {
      ""                                    : "dashboard",
      "dashboard"                           : "dashboard",
      "collection/:colid"                   : "collection",
      "collections"                         : "collections",
      "collectionInfo/:colid"               : "collectionInfo",
      "new"                                 : "newCollection",
      "login"                               : "login",
      "collection/:colid/documents/:pageid" : "documents",
      "collection/:colid/:docid"            : "document",
      "collection/:colid/:docid/source"     : "source",
      "shell"                               : "shell",
      "query"                               : "query",
      "logs"                                : "logs",
      "api"                                 : "api",
      "databases"                           : "databases",
      "application/installed/:key"          : "applicationEdit",
      "application/available/:key"          : "applicationInstall",
      "applications/installed"              : "applicationsInstalled",
      "applications/available"              : "applicationsAvailable",
      "applications"                        : "applications",
      "application/documentation/:key"      : "appDocumentation",
      "graph"                               : "graph",
      "graphManagement"                     : "graphManagement",
      "graphManagement/add"                 : "graphAddNew",
      "graphManagement/delete/:name"        : "graphDelete"
    },

    initialize: function () {
      this.graphs = new window.GraphCollection();

      window.currentDB = new window.CurrentDatabase();
      window.currentDB.fetch({
        async: false
      });

      window.activeSession = new window.ArangoSession();

      window.arangoDatabase = new window.ArangoDatabase();

      window.arangoCollectionsStore = new window.arangoCollections();
      window.arangoDocumentsStore = new window.arangoDocuments();
      window.arangoDocumentStore = new window.arangoDocument();

      window.collectionsView = new window.CollectionsView({
        collection: window.arangoCollectionsStore
      });
      window.arangoCollectionsStore.fetch();
      window.documentsView = new window.DocumentsView();
      window.documentView = new window.DocumentView();
      window.documentSourceView = new window.DocumentSourceView();
      window.arangoLogsStore = new window.ArangoLogs();
      window.arangoLogsStore.fetch({
        success: function () {
          window.logsView = new window.LogsView({
            collection: window.arangoLogsStore
          });
        }
      });

      this.foxxList = new window.FoxxCollection();

      this.footerView = new window.FooterView();
      this.naviView = new window.NavigationView();
      this.footerView.render();
      this.naviView.render();
      this.graphView = new window.GraphView({
        graphs: this.graphs,
        collection: window.arangoCollectionsStore
      });

      var self = this;
      $(window).resize(function() {
        self.handleResize();
      });
      this.handleResize();
    },

    logsAllowed: function () {
      return (window.databaseName === '_system');
    },

    checkSession: function () {
      if (window.activeSession.models.length === 0) {
        this.navigate("login", {trigger: true});
        return false;
      }
      return true;
    },

    login: function () {
      if (!this.loginView) {
        this.loginView = new window.loginView({
          collection: window.activeSession
        });
      }
      this.loginView.render();
      this.naviView.selectMenuItem('');
    },

    collections: function() {
      var naviView = this.naviView;
      window.arangoCollectionsStore.fetch({
        success: function () {
          window.collectionsView.render();
          naviView.selectMenuItem('collections-menu');
        }
      });
    },

    collection: function(colid) {
      if (!this.collectionView) {
        this.collectionView = new window.CollectionView();
      }
      this.collectionView.setColId(colid);
      this.collectionView.render();
      this.naviView.selectMenuItem('collections-menu');
    },
    collectionInfo: function(colid) {
      if (!this.collectionInfoView) {
        this.collectionInfoView = new window.CollectionInfoView();
      }
      this.collectionInfoView.setColId(colid);
      this.collectionInfoView.render();
      this.naviView.selectMenuItem('collections-menu');
    },
    newCollection: function() {
      if (!this.newCollectionView) {
        this.newCollectionView = new window.newCollectionView({});
      }
      this.newCollectionView.render();
      this.naviView.selectMenuItem('collections-menu');
    },

    documents: function(colid, pageid) {
      if (!window.documentsView) {
        window.documentsView.initTable(colid, pageid);
      }
      window.documentsView.collectionID = colid;
      var type = arangoHelper.collectionApiType(colid);
      window.documentsView.colid = colid;
      window.documentsView.pageid = pageid;
      window.documentsView.type = type;
      window.documentsView.render();
      window.arangoDocumentsStore.getDocuments(colid, pageid);
    },

    document: function(colid, docid) {
      if (!window.documentView) {
        window.documentView.initTable();
      }
      window.documentView.colid = colid;
      window.documentView.docid = docid;
      window.documentView.render();
      var type = arangoHelper.collectionApiType(colid);
      window.documentView.type = type;
      window.documentView.typeCheck(type);
    },

    source: function(colid, docid) {
      window.documentSourceView.render();
      window.documentSourceView.colid = colid;
      window.documentSourceView.docid = docid;
      var type = arangoHelper.collectionApiType(colid);
      window.documentSourceView.type = type;
      window.documentSourceView.typeCheck(type);
    },

    shell: function() {
      if (!this.shellView) {
        this.shellView = new window.shellView();
      }
      this.shellView.render();
      this.naviView.selectMenuItem('shell-menu');
    },

    query: function() {
      if (!this.queryView) {
        this.queryView = new window.queryView();
      }
      this.queryView.render();
      this.naviView.selectMenuItem('query-menu');
    },

    api: function() {
      if (!this.apiView) {
        this.apiView = new window.apiView();
      }
      this.apiView.render();
      this.naviView.selectMenuItem('api-menu');
    },

    databases: function() {
      if (arangoHelper.databaseAllowed() === true) {
        if (!this.databaseView) {
          this.databaseView = new window.databaseView({
            collection: arangoDatabase
          });
        }
        this.databaseView.render();
        this.naviView.selectMenuItem('databases-menu');
      }
      else {
        this.navigate("#", {trigger: true});
        this.naviView.selectMenuItem('dashboard-menu');
        $('#databaseNavi').css('display','none');
        $('#databaseNaviSelect').css('display','none');
      }
    },

    logs: function() {
      if (! this.logsAllowed()) {
        this.navigate('', { trigger: true });
        return;
      }

      window.arangoLogsStore.fetch({
        success: function () {
          window.logsView.render();
          $('#logNav a[href="#all"]').tab('show');
          window.logsView.initLogTables();
          window.logsView.drawTable();
          $('#all-switch').click();
        }
      });
      this.naviView.selectMenuItem('logs-menu');
    },

    dashboard: function() {
      this.naviView.selectMenuItem('dashboard-menu');
      if (this.statisticsDescription === undefined) {
        this.statisticsDescription = new window.StatisticsDescription();
        this.statisticsDescription.fetch({
          async:false
        });
      }
      if (this.statistics === undefined) {
        this.statisticsCollection = new window.StatisticsCollection();
      }
      if (this.dashboardView === undefined) {
        this.dashboardView = new dashboardView({
          collection: this.statisticsCollection,
          description: this.statisticsDescription
        });
         }
        this.dashboardView.render();
    },

    graph: function() {
      var self = this;
      window.arangoCollectionsStore.fetch({
        success: function () {
          self.graphView.render();
          self.naviView.selectMenuItem('graphviewer-menu');
        }
      });
    },

    graphManagement: function() {
      if (!this.graphManagementView) {
        this.graphManagementView = new window.GraphManagementView({collection: this.graphs});
      }
      this.graphManagementView.render();
      this.naviView.selectMenuItem('graphviewer-menu');
    },

    graphAddNew: function() {
      if (!this.addNewGraphView) {
        this.addNewGraphView = new window.AddNewGraphView({
          collection: window.arangoCollectionsStore,
          graphs: this.graphs
        });
      }
      this.addNewGraphView.render();
      this.naviView.selectMenuItem('graphviewer-menu');
    },

    graphDelete: function(name) {
      if (!this.deleteGraphView) {
        this.deleteGraphView = new window.DeleteGraphView({
          collection: this.graphs
        });
      }
      this.deleteGraphView.render(name);
      this.naviView.selectMenuItem('graphviewer-menu');
    },

    applications: function() {
      if (this.applicationsView === undefined) {
        this.applicationsView = new window.ApplicationsView({
          collection: this.foxxList
        });
      }
      this.applicationsView.reload();
      this.naviView.selectMenuItem('applications-menu');
    },

    applicationsAvailable: function() {
      if (this.applicationsInstalledView === undefined) {
        this.applicationsInstalledView = new window.FoxxInstalledListView({
          collection: this.foxxList
        });
      }
      this.applicationsInstalledView.reload();
      this.naviView.selectMenuItem('applications-menu');
    },

    applicationsInstalled: function() {
      if (this.applicationsActiveView === undefined) {
        this.applicationsActiveView = new window.FoxxActiveListView({
          collection: this.foxxList
        });
      }
      this.applicationsActiveView.reload();
      this.naviView.selectMenuItem('applications-menu');
    },

    applicationEdit: function(appkey) {
      this.foxxList.fetch({
        async: false
      });
      var editAppView = new window.foxxEditView({model: this.foxxList.findWhere({_key: appkey})});
      editAppView.render();
    },

    applicationInstall: function(appkey) {
      this.foxxList.fetch({
        async: false
      });
      if (!this.installAppView) {
        this.installAppView = new window.foxxMountView({
          collection: this.foxxList
        });
      }
      this.installAppView.render(appkey);
    },

    appDocumentation: function(key) {
      var docuView = new window.AppDocumentationView({key: key});
      docuView.render();
      this.naviView.selectMenuItem('applications-menu');
    },

    handleSelectDatabase: function () {
      this.footerView.handleSelectDatabase();
    },

    handleResize: function () {
      var oldWidth = $('#content').width();
      var containerWidth = $(window).width() - 70;
      /*var spanWidth = 242;*/
      var spanWidth = 243;
      var divider = containerWidth / spanWidth;
      var roundDiv = parseInt(divider, 10);
      var newWidth = roundDiv*spanWidth -2;
      var marginWidth = ((containerWidth+30) - newWidth)/2;
      this.footerView.handleResize(marginWidth);
      this.naviView.handleResize(marginWidth);
      $('#content').width(newWidth)
      .css('margin-left', marginWidth)
      .css('margin-right', marginWidth);
      // $('.footer-right p').css('margin-right', marginWidth + 20);
      // $('.footer-left p').css('margin-left', marginWidth + 20);
      if (newWidth !== oldWidth) {
        this.graphView.handleResize(newWidth);
      }
    }
  });

}());
