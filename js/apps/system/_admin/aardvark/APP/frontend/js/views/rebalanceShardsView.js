/* jshint browser: true */
/* jshint unused: false */
/* global arangoHelper, Backbone, templateEngine, $, window, _ */
(function () {
    'use strict';
    window.RebalanceShardsView = Backbone.View.extend({
        el: '#content',

        template: templateEngine.createTemplate('rebalanceShardsView.ejs'),

        events: {
            'click #rebalanceShards': 'rebalanceShards'
        },

        /*
        initialize: function () {
            this.render(false);
        },
         */

        render: function (navi) {
            console.log("Render function");
            if (window.location.hash === '#rebalanceShards') {

                arangoHelper.buildNodesSubNav('Rebalance Shards');

                console.log("$el ", this.$el);
                console.log(this.template.render({}));
                this.$el.html(this.template.render({}));

                arangoHelper.createTooltips();
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
                        console.log("true");
                        window.setTimeout(function () {
                            self.render(false);
                        }, 3000);
                    }
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