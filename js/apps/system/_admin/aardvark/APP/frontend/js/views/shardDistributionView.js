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
      console.log('started');
      if (window.location.hash === this.hash) {
        console.log(':-)');
        var self = this;

        $.ajax({
          type: 'GET',
          cache: false,
          url: arangoHelper.databaseUrl('/_admin/cluster/shardDistribution'),
          contentType: 'application/json',
          processData: false,
          async: true,
          success: function (data) {
            self.continueRender(data.results, false);
          },
          error: function (data) {
            self.continueRender(data, true);
          }
        });

        if (navi !== false) {
          arangoHelper.buildClusterSubNav('Distribution');
        }
      }
    },

    continueRender: function (data, error) {
      console.log(data);
      this.$el.html(this.template.render({}));
    }
  });
}());
