/*jshint browser: true */
/*jshint unused: false */
/*global arangoHelper, Backbone, templateEngine, $, window, _, nv, d3 */
(function () {
  "use strict";

  window.NodesView = Backbone.View.extend({

    el: '#content',
    template: templateEngine.createTemplate("nodesView.ejs"),
    interval: 5000,
    knownServers: [],

    events: {
      "click .pure-table-body .pure-table-row": "navigateToNode"  
    },

    initialize: function (options) {
      var self = this;

      if (window.App.isCluster) {
        this.dbServers = options.dbServers;
        this.coordinators = options.coordinators;
        this.updateServerTime();

        //start polling with interval
        window.setInterval(function() {
          if (window.location.hash === '#nodes') {


            var callback = function(data) {
            };

          }
        }, this.interval);
      }
    },

    navigateToNode: function(elem) {
      var name = $(elem.currentTarget).attr('node');
      window.App.navigate("#node/" + encodeURIComponent(name), {trigger: true});
    },

    render: function () {
      this.$el.html(this.template.render({coords: []}));

      var callback = function() {
        this.continueRender();
      }.bind(this);

      if (!this.initDone) {
        this.waitForCoordinators(callback);
      }
      else {
        callback();
      }
    },

    continueRender: function() {
      var coords = this.coordinators.toJSON();
      this.$el.html(this.template.render({
        coords: coords
      }));
    },

    waitForCoordinators: function(callback) {
      var self = this; 

      window.setTimeout(function() {
        if (self.coordinators.length === 0) {
          self.waitForCoordinators(callback);
        }
        else {
          this.initDone = true;
          callback();
        }
      }, 200);
    },

    updateServerTime: function() {
      this.serverTime = new Date().getTime();
    }

  });
}());
