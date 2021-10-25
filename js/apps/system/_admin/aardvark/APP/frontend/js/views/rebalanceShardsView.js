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

    render: function (navi) {
      $("#rebalanceShards *").prop('disabled',true);
      arangoHelper.checkDatabasePermissions("", () => {
        $("#rebalanceShards *").prop('disabled',false);
      });
      if (window.location.hash === '#rebalanceShards') {
        arangoHelper.buildNodesSubNav('Rebalance Shards');
        this.$el.html(this.template.render({}));
        document.getElementById("rebalanceShards").innerHTML += ` <div style="font-size: 10pt; margin-left: 10px; margin-top: 12px; font-weight: 200"> 
                <b>WARNING:</b> Clicking this button will start a 
                shard rebalancing process and schedule up to ${this.maxNumberOfMoveShards} shards move operations, which may have background load </div>`;
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
          if (data === true) {
            window.setTimeout(function () {
              self.render(false);
            }, 3000);
          }
          if (data.result.operations === 0) {
            arangoHelper.arangoNotification('No move shards operation to be scheduled. ');
          } else {
            arangoHelper.arangoNotification('Started rebalance process. Scheduled ' + data.result.operations + ' shards move operations. (Limit is ' + self.maxNumberOfMoveShards + '.)');
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
  })
}());