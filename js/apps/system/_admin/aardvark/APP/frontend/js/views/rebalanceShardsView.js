/* jshint browser: true */
/* jshint unused: false */
/* global arangoHelper, Backbone, templateEngine, $, window, _ */
(function () {
  'use strict';
  window.RebalanceShardsView = Backbone.View.extend({
    el: '#content',

    maxNumberOfMoveShards: undefined,

    template: templateEngine.createTemplate('rebalanceShardsView.ejs'),

    events: {
      'click #rebalanceShardsBtn': 'rebalanceShards'
    },

    initialize: function (options) {
      this.maxNumberOfMoveShards = options.maxNumberOfMoveShards;
    },

    render: function () {
      if (window.location.hash === '#rebalanceShards') {
        arangoHelper.buildNodesSubNav('Rebalance Shards', false);
        this.$el.html(this.template.render({maxNumberOfMoveShards: this.maxNumberOfMoveShards}));

        arangoHelper.createTooltips();
      }
    },

    rebalanceShards: function () {
      var self = this;

      $.ajax({
        type: 'POST',
        cache: false,
        url: arangoHelper.databaseUrl('/_admin/cluster/rebalanceShards'),
        contentType: 'application/json',
        processData: false,
        data: JSON.stringify({}),
        async: true,
        success: function (data) {
          if (data.result.operations === 0) {
            arangoHelper.arangoNotification('No move shards operations were scheduled.');
          } else {
            arangoHelper.arangoNotification('Started rebalance process. Scheduled ' + data.result.operations + ' shards move operation(s).');
          }
        },
        error: function () {
          arangoHelper.arangoError('Could not start rebalance process.');
        }
      });
      window.modalView.hide();
    },
    remove: function () {
      this.$el.empty().off(); /* off to unbind the events */
      this.stopListening();
      this.unbind();
      delete this.el;
      return this;
    },
  });
}());
