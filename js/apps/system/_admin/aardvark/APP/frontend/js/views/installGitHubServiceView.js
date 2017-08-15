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
      'click #installGitHubService': 'installGitHubService'
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

    installGitHubService: function () {
      arangoHelper.createMountPointModal(this.installFoxxFromGithub.bind(this));
    },

    installFoxxFromGithub: function () {
      if (window.modalView.modalTestAll()) {
        var url, version, mount, flag, isLegacy;
        if (this._upgrade) {
          mount = this.mount;
          flag = $('#new-app-teardown').prop('checked');
        } else {
          mount = window.arangoHelper.escapeHtml($('#new-app-mount').val());
        }
        url = window.arangoHelper.escapeHtml($('#repository').val());
        version = window.arangoHelper.escapeHtml($('#tag').val());

        if (version === '') {
          version = 'master';
        }
        var info = {
          url: window.arangoHelper.escapeHtml($('#repository').val()),
          version: window.arangoHelper.escapeHtml($('#tag').val())
        };

        try {
          Joi.assert(url, Joi.string().regex(/^[a-zA-Z0-9_-]+\/[a-zA-Z0-9_-]+$/));
        } catch (e) {
          return;
        }
        // send server req through collection
        isLegacy = Boolean($('#github-app-islegacy').prop('checked'));
        this.collection.installFromGithub(info, mount, this.installCallback.bind(this), isLegacy, flag);
      }
    },

    installCallback: function (result) {
      window.App.navigate('#services', {trigger: true});
      window.App.applicationsView.installCallback(result);
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
