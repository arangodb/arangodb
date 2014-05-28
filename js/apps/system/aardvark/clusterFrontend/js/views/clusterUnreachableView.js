/*jslint indent: 2, nomen: true, maxlen: 100, sloppy: true, vars: true, white: true, plusplus: true, newcap: true */
/*global window, $, Backbone, templateEngine, plannerTemplateEngine, alert, _ */

(function() {
  "use strict";
  
  window.ClusterUnreachableView = Backbone.View.extend({

    el: "#content",

    template: templateEngine.createTemplate("clusterUnreachable.ejs"),
    modal: templateEngine.createTemplate("waitModal.ejs"),

    events: {
      "click #clusterShutdown": "shutdown"
    },

    initialize: function() {
      this.coordinators = new window.ClusterCoordinators([], {
      });
    },


    retryConnection: function() {
      this.coordinators.checkConnection(function() {
        window.App.showCluster();
      });
    },

    shutdown: function() {
      window.clearTimeout(this.timer);
      window.App.shutdownView.clusterShutdown();
    },

    render: function() {
      var plan = window.App.clusterPlan;
      var list = [];
      if (plan && plan.has("runInfo")) {
        var startServerInfos = _.where(plan.get("runInfo"), {isStartServers: true});
        _.each(
          _.filter(startServerInfos, function(s) {
            return _.contains(s.roles, "Coordinator");
          }), function(s) {
            var name = s.endpoints[0].split("://")[1];
            name = name.split(":")[0];
            list.push(name);
          }
        );
      }
      $(this.el).html(this.template.render({
        coordinators: list
      }));
      $(this.el).append(this.modal.render({}));
      this.timer = window.setTimeout(this.retryConnection.bind(this), 10000);
    }

  });
}());
