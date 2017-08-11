/* jshint browser: true */
/* global Backbone, $, window, arangoHelper, templateEngine*/
(function () {
  'use strict';

  window.ServiceInstallNewView = Backbone.View.extend({
    el: '#content',
    readOnly: false,

    template: templateEngine.createTemplate('serviceInstallNewView.ejs'),

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
      // if repo not fetched yet, wait
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
        arangoHelper.buildServicesSubNav('New');
      } else {
        window.setTimeout(function () {
          self.breadcrumb();
        }, 100);
      }
    }

  });
}());
