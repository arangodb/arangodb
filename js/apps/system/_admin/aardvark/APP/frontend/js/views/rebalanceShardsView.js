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
            'click #rebalanceShards': 'rebalanceShards'
        },

        initialize: function (options) {
            this.maxNumberOfMoveShards = options.maxNumberOfMoveShards;
            console.log(options);
        },

        render: function (navi) {

            if (window.location.hash === '#rebalanceShards' && window.App.isCluster) {
                arangoHelper.buildNodesSubNav('Rebalance Shards');

                console.log("$el ", this.$el);
                console.log(this.template.render({}));
                this.$el.html(this.template.render({}));
                document.getElementById("rebalanceShards").innerHTML += ` <div style="font-size: 10pt; margin-left: 10px; margin-top: 12px; font-weight: 200"> 
                <b>WARNING:</b> Clicking this button will start a 
                shard rebalancing process amd schedule up to ${this.maxNumberOfMoveShards}, which may have background load </div>`;
;               arangoHelper.createTooltips();
            }
        },

        rebalanceShards: function () {
            // display the number of move shards jobs it has scheduled
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
                    // arangoHelper.arangoNotification('Started rebalance process. Scheduled ' + operations + ' of up to ' + numShards + 'move shards');
                    arangoHelper.arangoNotification('Started rebalance process.');
                    console.log(data);
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