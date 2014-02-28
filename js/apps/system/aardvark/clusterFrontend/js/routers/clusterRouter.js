/*jslint indent: 2, nomen: true, maxlen: 100, sloppy: true, vars: true, white: true, plusplus: true, newcap: true */
/*global window, $, Backbone, document, arangoCollection,arangoHelper,dashboardView,arangoDatabase*/

(function() {
  "use strict";

  window.ClusterRouter = Backbone.Router.extend({

    routes: {
      "planTest"               : "planTest",
      "planSymmetrical"        : "planSymmetric",
      "planAsymmetrical"       : "planAsymmetric",
      "shards"                 : "showShards",
      "showCluster"            : "showCluster",
      "dashboard/:server"      : "dashboard",
      "handleClusterDown"      : "handleClusterDown"
    },

    getNewRoute: function(last) {
      if (last === "statistics") {
        return this.clusterPlan.getCoordinator()
          + "/_admin/"
          + last;
      }
      return this.clusterPlan.getCoordinator()
        + "/_admin/aardvark/cluster/"
        + last;
    },

    updateAllUrls: function() {
      _.each(this.toUpdate, function(u) {
        u.updateUrl();
      });
    },

    registerForUpdate: function(o) {
      this.toUpdate.push(o);
      o.updateUrl();
    },

    initialize: function () {
      this.bind('all', function(trigger, args) {
        var routeData = trigger.split(":");
        if (trigger === "route") {
            console.log(trigger, args);
            if (this.currentRoute === "dashboard") {
                this.dashboardView.stopUpdating();
            }
            this.currentRoute = args;
        }
      });
      this.toUpdate = [];
      this.clusterPlan = new window.ClusterPlan();
      this.clusterPlan.fetch({
        async: false
      });
      this.footerView = new window.FooterView();
      this.footerView.render();
    },

    showCluster: function() {
      if (!this.showClusterView) {
        this.showClusterView = new window.ShowClusterView();
      }
      this.showClusterView.render();
    },

    showShards: function() {
      if (!this.showShardsView) {
        this.showShardsView = new window.ShowShardsView();
      }
      this.showShardsView.render();
    },

    handleResize: function() {
     // Not needed here
    },

    planTest: function() {
      if (!this.planTestView) {
        this.planTestView = new window.PlanTestView(
          {model : this.clusterPlan}
        );
      }
      this.planTestView.render();
    },

    planSymmetric: function() {
      if (!this.planSymmetricView) {
        this.planSymmetricView = new window.PlanSymmetricView();
      }
      this.planSymmetricView.render(true);
    },

    planAsymmetric: function() {
      if (!this.planSymmetricView) {
        this.planSymmetricView = new window.PlanSymmetricView();
      }
      this.planSymmetricView.render(false);
    },

    planScenario: function() {
      if (!this.planScenarioSelector) {
        this.planScenarioSelector = new window.PlanScenarioSelectorView();
      }
      this.planScenarioSelector.render();
    },

    showDownload: function(content) {
      if (!this.downloadView) {
        this.downloadView = new window.DownloadView();
      }
      this.downloadView.render(content);
    },

    handleClusterDown : function() {
      if (!this.clusterDownView) {
        this.clusterDownView = new window.ClusterDownView();
      }
      this.clusterDownView.render();
    },

    dashboard: function(server) {
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
              description: this.statisticsDescription,
              documentStore: new window.arangoDocuments(),
              server : server
          });
      }
      this.dashboardView.render();
    }

  });

}());
