/* jshint browser: true */
/* global Backbone, $, window, arangoHelper, templateEngine, Joi */
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
      this.setGithubValidators();
      arangoHelper.createTooltips('.modalTooltips');

      return this;
    },

    breadcrumb: function () {
      var self = this;

      if (window.App.naviView) {
        $('#subNavigationBar .breadcrumb').html(
          '<a href="#services">Services:</a> New'
        );
        arangoHelper.buildServicesSubNav('GitHub');
      } else {
        window.setTimeout(function () {
          self.breadcrumb();
        }, 100);
      }
    },

    setGithubValidators: function () {
      window.modalView.modalBindValidation({
        id: 'repository',
        validateInput: function () {
          return [
            {
              rule: Joi.string().required().regex(/^[a-zA-Z0-9_-]+\/[a-zA-Z0-9_-]+$/),
              msg: 'No valid Github account and repository.'
            }
          ];
        }
      });
      window.modalView.modalTestAll();
    }

  });
}());
