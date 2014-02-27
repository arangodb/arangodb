/*jslint indent: 2, nomen: true, maxlen: 100, sloppy: true, vars: true, white: true, plusplus: true, newcap: true */
/*global window, $, Backbone, document, arangoCollection,arangoHelper,dashboardView,arangoDatabase*/

(function() {
  "use strict";

  window.ClusterRouter = Backbone.Router.extend({

    routes: {
      "planTest"               : "planTest",
      "planSymmetrical"        : "planSymmetric",
      "planAsymmetrical"       : "planAsymmetric",
      "shards"                 : "showShards"
    },

    initialize: function () {
      this.clusterPlan = new window.ClusterPlan();
      this.clusterPlan.fetch({
        async: false
      });

      //für zum testen
//      this.clusterPlan.set({"plan": "blub"});

      if(this.clusterPlan.get("plan")) {
        this.showCluster();
      } else {
        this.planScenario();
      }

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
        this.planTestView = new window.PlanTestView();
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
    }

  });

}());
