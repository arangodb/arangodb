/* global templateEngine */

(function () {
    'use strict';
    window.SkeletonLoader = Backbone.View.extend({
        el: '#content',
        readOnly: false,
        template: templateEngine.createTemplate('loading.ejs'),

        initialize: function () {
            this.render();
        },
        render: function () {
            var self = this;
            var template = self.template.render();
            self.$el.html($(template));
            return self;
        }
        });  
  }());
