$(document).ready(function() {

  window.Router = Backbone.Router.extend({

    routes: {
      ""                                    : "collections",
      "collection/:colid"                   : "collection",
      "new"                                 : "newCollection",
      "collection/:colid/documents/:pageid" : "documents",
      "collection/:colid/:docid"            : "document",
      "collection/:colid/:docid/source"     : "source",
      "shell"                               : "shell",
      "dashboard"                           : "dashboard",
      "query"                               : "query",
      "logs"                                : "logs",
      "about"                               : "about"
    },
    initialize: function () {
      window.arangoCollectionsStore = new window.arangoCollections();
      window.arangoDocumentsStore = new window.arangoDocuments();
      window.arangoDocumentStore = new window.arangoDocument();

      window.dashboardView = new window.dashboardView({
        collection: window.arangoCollectionsStore
      });

      window.documentsView = new window.documentsView({
        collection: window.arangoDocuments,
      });
      window.documentView = new window.documentView({
        collection: window.arangoDocument,
      });
      window.documentSourceView = new window.documentSourceView({
        collection: window.arangoDocument,
      });

      window.arangoLogsStore = new window.arangoLogs();
      window.arangoLogsStore.fetch({
        success: function () {
          if (!window.logsView) {
          }
          window.logsView = new window.logsView({
            collection: window.arangoLogsStore
          });
        }
      });
      this.naviView = new window.navigationView();
      this.footerView = new window.footerView();
      this.naviView.render();
      this.footerView.render();
    },
    collections: function() {

      var naviView = this.naviView;

      window.arangoCollectionsStore.fetch({
        success: function () {
          var collectionsView = new window.collectionsView({
            collection: window.arangoCollectionsStore
          });
          collectionsView.render();
          naviView.selectMenuItem('collections-menu');
        }
      });
    },
    collection: function(colid) {
      //TODO: if-statement for every view !
      if (!this.collectionView) {
        this.collectionView = new window.collectionView({
          colId: colid,
          model: arangoCollection
        });
      }
      else {
        this.collectionView.options.colId = colid;
      }
      this.collectionView.render();
    },
    newCollection: function() {
      if (!this.newCollectionView) {
        this.newCollectionView = new window.newCollectionView({});
      }
      this.newCollectionView.render();
    },
    documents: function(colid, pageid) {
      if (!window.documentsView) {
        window.documentsView.initTable(colid, pageid);
      }
      window.documentsView.colid = colid;
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
    about: function() {
      if (!this.aboutView) {
        this.aboutView = new window.aboutView();
      }
      this.aboutView.render();
      this.naviView.selectMenuItem('about-menu');
    },
    logs: function() {
      var self = this;
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
      var self = this;
      window.arangoCollectionsStore.fetch({
        success: function () {
          window.dashboardView.render();
          self.naviView.selectMenuItem('dashboard-menu');
        }
      });
    }

  });

  window.App = new window.Router();
  Backbone.history.start();

});
