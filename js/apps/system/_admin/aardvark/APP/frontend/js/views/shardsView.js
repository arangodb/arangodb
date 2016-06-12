/*jshint browser: true */
/*jshint unused: false */
/*global arangoHelper, Backbone, templateEngine, $, window, _, nv, d3 */
(function () {
  "use strict";

  window.ShardsView = Backbone.View.extend({

    el: '#content',
    template: templateEngine.createTemplate("shardsView.ejs"),
    interval: 10000,
    knownServers: [],

    events: {
    },

    initialize: function () {
      var self = this;
      clearInterval(this.intervalFunction);

      if (window.App.isCluster) {
        this.updateServerTime();

        //start polling with interval
        this.intervalFunction = window.setInterval(function() {
          if (window.location.hash === '#shards') {
            self.render();
          }
        }, this.interval);
      }
    },

    render: function () {

      var self = this;

      $.ajax({
        type: "GET",
        cache: false,
        url: arangoHelper.databaseUrl("/_admin/cluster/shardDistribution"),
        contentType: "application/json",
        processData: false,
        async: true,
        success: function(data) {
          self.continueRender(data);
        },
        error: function() {
          arangoHelper.arangoError("Cluster", "Could not fetch sharding information.");
        }
      });
    },

    continueRender: function(collections) {
      console.log(collections);
      this.$el.html(this.template.render({
        collections: collections
      }));
    },

    updateServerTime: function() {
      this.serverTime = new Date().getTime();
    }

  });
}());
