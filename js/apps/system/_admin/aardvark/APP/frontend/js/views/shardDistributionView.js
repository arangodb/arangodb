/* jshint browser: true */
/* jshint unused: false */
/* global arangoHelper, Backbone, templateEngine, $, window, _ */
(function () {
  'use strict';

  window.ShardDistributionView = Backbone.View.extend({
    el: '#content',
    hash: '#distribution',
    template: templateEngine.createTemplate('shardDistributionView.ejs'),
    interval: 10000,

    events: {
    },

    initialize: function (options) {
      var self = this;
      clearInterval(this.intervalFunction);

      if (window.App.isCluster) {
        // start polling with interval
        this.intervalFunction = window.setInterval(function () {
          if (window.location.hash === self.hash) {
            // self.render(false); //TODO
          }
        }, this.interval);
      }
    },

    remove: function () {
      clearInterval(this.intervalFunction);
      this.$el.empty().off(); /* off to unbind the events */
      this.stopListening();
      this.unbind();
      delete this.el;
      return this;
    },

    render: function (navi) {
      if (window.location.hash === this.hash) {
        // pre-render without data (placeholders)
        this.$el.html(this.template.render({}));
        var self = this;

        $.ajax({
          type: 'GET',
          cache: false,
          url: arangoHelper.databaseUrl('/_admin/cluster/shardStatistics'),
          contentType: 'application/json',
          processData: false,
          async: true,
          success: function (data) {
            self.rerenderValues(data.result);
          },
          error: function () {
            arangoHelper.arangoError('Distribution', 'Could not fetch "shardStatistics"');
          }
        });

        if (navi !== false) {
          arangoHelper.buildClusterSubNav('Distribution');
        }
      }
    },

    rerenderValues: function (data) {
      console.log(data);
      arangoHelper.renderStatisticsBoxValue('#clusterDatabases', data.databases);
      arangoHelper.renderStatisticsBoxValue('#clusterCollections', data.collections);
      arangoHelper.renderStatisticsBoxValue('#clusterDBServers', data.servers);
      arangoHelper.renderStatisticsBoxValue('#clusterLeaders', data.leaders);
      arangoHelper.renderStatisticsBoxValue('#clusterRealLeaders', data.realLeaders);
      arangoHelper.renderStatisticsBoxValue('#clusterFollowers', data.followers);
      arangoHelper.renderStatisticsBoxValue('#clusterShards', data.shards);
    },

    continueRender: function (data, error) {
      this.$el.html(this.template.render({}));
    }
  });
}());
