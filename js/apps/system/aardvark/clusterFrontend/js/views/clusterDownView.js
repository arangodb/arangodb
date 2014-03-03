/*jslint indent: 2, nomen: true, maxlen: 100, sloppy: true, vars: true, white: true, plusplus: true, newcap: true */
/*global window, $, Backbone, plannerTemplateEngine, alert */

(function() {
  "use strict";
  
  window.ClusterDownView = Backbone.View.extend({

    el: "#content",

    template: templateEngine.createTemplate("clusterDown.ejs"),
    modal: templateEngine.createTemplate("waitModal.ejs"),

    events: {
      "click #relaunchCluster"  : "relaunchCluster",
      "click #editPlan"         : "editPlan",
      "click #deletePlan"       : "deletePlan"
    },

    render: function() {
      $(this.el).html(this.template.render({}));
      $(this.el).append(this.modal.render({}));
    },

    relaunchCluster: function() {
      $('#waitModalLayer').modal('show');
      $('.modal-backdrop.fade.in').addClass('waitModalBackdrop');
      $('#waitModalMessage').html('Please be patient while your cluster will be relaunched');
      var result = false;
      $.ajax({
        cache: false,
        type: "GET",
        url: "cluster/relaunch",
        success: function(data) {
          $('.modal-backdrop.fade.in').removeClass('waitModalBackdrop');
          $('#waitModalLayer').modal('hide');
          window.App.navigate("showCluster", {trigger: true});
        }
      });
    },
    editPlan: function() {
      var plan = window.App.clusterPlan;
      if (plan.isTestSetup()) {
        window.App.navigate("planTest", {trigger : true});
        return;
      }
      if (plan.isSymmetricSetup()) {
        window.App.navigate("planSymmetrical", {trigger : true});
        return;
      }
      window.App.navigate("planAsymmetrical", {trigger : true});
    },

    deletePlan: function() {
      window.App.clusterPlan.destroy();
      window.App.clusterPlan = new window.ClusterPlan();
      window.App.planScenario();
    }

  });

}());
