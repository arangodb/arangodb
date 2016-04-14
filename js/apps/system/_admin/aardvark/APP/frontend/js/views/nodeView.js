/*jshint browser: true */
/*jshint unused: false */
/*global arangoHelper, Backbone, templateEngine, $, window, _, nv, d3 */
(function () {
  "use strict";

  window.NodeView = Backbone.View.extend({

    el: '#content',
    template: templateEngine.createTemplate("nodeView.ejs"),
    interval: 5000,
    dashboards: [],

    events: {
    },

    initialize: function (options) {

      if (window.App.isCluster) {
        this.coordinators = options.coordinators;
        this.coordname = options.coordname;
        this.updateServerTime();

        //start polling with interval
        window.setInterval(function() {
          if (window.location.hash.indexOf('#node/') === 0) {

            var callback = function(data) {
            };

          }
        }, this.interval);
      }
    },

    breadcrumb: function(name) {
      console.log("yes");
      $('#subNavigationBar .breadcrumb').html("Node: " + name);
    },

    render: function () {
      console.log(1);
      this.$el.html(this.template.render({coords: []}));

      var callback = function() {
        this.continueRender();
        this.breadcrumb(this.coordname);
      }.bind(this);

      if (!this.initDone) {
        this.waitForCoordinators(callback);
      }
      else {
        this.coordname = window.location.hash.split('/')[1];
        this.coordinator = this.coordinators.findWhere({name: this.coordname});
        callback();
      }
    },

    continueRender: function() {
      this.dashboards[this.coordinator.get('name')] = new window.DashboardView({
        dygraphConfig: window.dygraphConfig,
        database: window.App.arangoDatabase,
        serverToShow: {
          raw: this.coordinator.get('address'),
          isDBServer: false,
          endpoint: this.coordinator.get('protocol') + "://" + this.coordinator.get('address'),
          target: this.coordinator.get('name')
        }
      });
      this.dashboards[this.coordinator.get('name')].render();
    },

    waitForCoordinators: function(callback) {
      var self = this; 

      window.setTimeout(function() {
        if (self.coordinators.length === 0) {
          self.waitForCoordinators(callback);
        }
        else {
          self.coordinator = self.coordinators.findWhere({name: self.coordname});
          self.initDone = true;
          callback();
        }
      }, 200);
    },

    updateServerTime: function() {
      this.serverTime = new Date().getTime();
    }

  });
}());
