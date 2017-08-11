/* jshint browser: true */
/* global Backbone, $, window, arangoHelper, templateEngine */
(function () {
  'use strict';

  window.ServiceInstallGitHubView = Backbone.View.extend({
    el: '#content',
    readOnly: false,

    template: templateEngine.createTemplate('serviceInstallGitHubView.ejs'),

    remove: function () {
      this.$el.empty().off(); /* off to unbind the events */
      this.stopListening();
      this.unbind();
      delete this.el;
      return this;
    },

    events: {
    },

    initialize: function () {
    },

    render: function () {
      $(this.el).html(this.template.render({
        services: this.collection
      }));

      this.breadcrumb();

      return this;
    },

    breadcrumb: function () {
      var self = this;

      if (window.App.naviView) {
        $('#subNavigationBar .breadcrumb').html(
          'New Service'
        );
        arangoHelper.buildServicesSubNav('GitHub');
      } else {
        window.setTimeout(function () {
          self.breadcrumb();
        }, 100);
      }
    }

  });
}());
