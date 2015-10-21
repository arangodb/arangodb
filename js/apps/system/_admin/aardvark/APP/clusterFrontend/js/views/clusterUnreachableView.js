/*global window, $, Backbone, XMLHttpRequest, templateEngine, plannerTemplateEngine, alert, _ */

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

    retryCounter: 0,

    retryNavigation: function() {
      var self = this;

      if (this.retryCounter === 10) {
        this.retryCounter = 0;

        var xhttp = new XMLHttpRequest(); xhttp.open("GET", window.location.origin + window.location.pathname, false);
        xhttp.send();

        if (xhttp.status === 200) {
          window.location.reload();
        }
        else {
          window.setTimeout(function() {
            self.retryCounter++;
            self.retryNavigation();
          }, 1000);
        }
      }
      else {
        this.retryCounter++;
        window.setTimeout(function() {
          self.retryNavigation();
        }, 1000);
      }
    },

    retryConnection: function() {
        this.coordinators.checkConnection(function() {
          window.App.showCluster();
        });

        this.retryNavigation();
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
