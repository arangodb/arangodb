/*jslint indent: 2, nomen: true, maxlen: 100, sloppy: true, vars: true, white: true, plusplus: true, newcap: true */
/*global window, $, Backbone, templateEngine, plannerTemplateEngine, alert */

(function() {
  "use strict";
  
  window.ClusterDownView = Backbone.View.extend({

    el: "#content",

    template: templateEngine.createTemplate("clusterDown.ejs"),
    modal: templateEngine.createTemplate("waitModal.ejs"),

    events: {
      "click #relaunchCluster"  : "relaunchCluster",
      "click #upgradeCluster"   : "upgradeCluster",
      "click #editPlan"         : "editPlan",
      "click #submitEditPlan"   : "submitEditPlan",
      "click #deletePlan"       : "deletePlan",
      "click #submitDeletePlan" : "submitDeletePlan"
    },

    render: function() {
      var planVersion = window.versionHelper.fromString(
        window.App.clusterPlan.getVersion()
      );
      var currentVersion = window.App.footerView.system.version;
      if (!currentVersion) {
        $.ajax({
          type: "GET",
          cache: false,
          url: "/_api/version",
          contentType: "application/json",
          processData: false,
          async: false,
          success: function(data) {
            currentVersion = data.version;
          }
        });
      }
      currentVersion = window.versionHelper.fromString(
        currentVersion
      );
      var shouldUpgrade = false;
      if (currentVersion.major > planVersion.major
        || (
          currentVersion.major === planVersion.major
          && currentVersion.minor > planVersion.minor
        )) {
        shouldUpgrade = true;
      }
      $(this.el).html(this.template.render({
        canUpgrade: shouldUpgrade
      }));
      $(this.el).append(this.modal.render({}));
    },

    relaunchCluster: function() {
      $('#waitModalLayer').modal('show');
      $('.modal-backdrop.fade.in').addClass('waitModalBackdrop');
      $('#waitModalMessage').html('Please be patient while your cluster will be relaunched');
      $.ajax({
        cache: false,
        type: "GET",
        url: "cluster/relaunch",
        success: function() {
          $('.modal-backdrop.fade.in').removeClass('waitModalBackdrop');
          $('#waitModalLayer').modal('hide');
          window.App.navigate("showCluster", {trigger: true});
        }
      });
    },

    upgradeCluster: function() {
      $('#waitModalLayer').modal('show');
      $('.modal-backdrop.fade.in').addClass('waitModalBackdrop');
      $('#waitModalMessage').html('Please be patient while your cluster will be upgraded');
      $.ajax({
        cache: false,
        type: "GET",
        url: "cluster/upgrade",
        success: function() {
          $('.modal-backdrop.fade.in').removeClass('waitModalBackdrop');
          $('#waitModalLayer').modal('hide');
          window.App.clusterPlan.fetch();
          window.App.navigate("showCluster", {trigger: true});
        }
      });
    },

    editPlan: function() {
      $('#deletePlanModal').modal('hide');
      $('#editPlanModal').modal('show');
    },

    submitEditPlan : function() {
      $('#editPlanModal').modal('hide');
      var plan = window.App.clusterPlan;
      if (plan.isTestSetup()) {
        window.App.navigate("planTest", {trigger : true});
        return;
      }
/*
      if (plan.isSymmetricSetup()) {
        window.App.navigate("planSymmetrical", {trigger : true});
        return;
      }
*/
      window.App.navigate("planAsymmetrical", {trigger : true});
    },

    deletePlan: function() {
      $('#editPlanModal').modal('hide');
      $('#deletePlanModal').modal('show');
    },

    submitDeletePlan : function() {
      $('#deletePlanModal').modal('hide');
      window.App.clusterPlan.destroy();
      window.App.clusterPlan = new window.ClusterPlan();
      window.App.planScenario();
    }


  });

}());
