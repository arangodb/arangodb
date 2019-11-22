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

    initialize: function () {
      if (window.App.replaceApp) {
        this._upgrade = true;
      }
    },

    events: {
      'click #installGitHubService': 'installGitHubService',
      'keydown input': 'checkValidators'
    },

    checkValidators: function () {
      if (window.modalView._validators.length !== 1) {
        window.modalView.clearValidators();
        this.setGithubValidators();
      }
    },

    render: function () {
      $(this.el).html(this.template.render({
        services: this.collection,
        upgrade: this._upgrade
      }));

      this.breadcrumb();
      this.setGithubValidators();
      arangoHelper.createTooltips('.modalTooltips');

      return this;
    },

    breadcrumb: function () {
      var self = this;

      if (window.App.naviView) {
        var replaceString = 'New';
        if (this._upgrade) {
          replaceString = 'Replace (' + window.App.replaceAppData.mount + ')';
        }
        $('#subNavigationBar .breadcrumb').html(
          '<a href="#services">Services:</a> ' + replaceString
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
        var mount, info, options, url, version;
        if (this._upgrade) {
          mount = window.App.replaceAppData.mount;
        } else {
          mount = window.arangoHelper.escapeHtml($('#new-app-mount').val());
          if (mount.charAt(0) !== '/') {
            mount = '/' + mount;
          }
        }

        url = window.arangoHelper.escapeHtml($('#repository').val());
        version = window.arangoHelper.escapeHtml($('#tag').val());
        if (version === '') {
          version = 'master';
        }
        try {
          Joi.assert(url, Joi.string().regex(/^[a-zA-Z0-9_-]+\/[a-zA-Z0-9_-]+$/));
        } catch (e) {
          return;
        }
        // send server req through collection

        info = {
          url: url,
          version: version
        };

        options = arangoHelper.getFoxxFlags();
        options.legacy = Boolean($('#github-app-islegacy')[0].checked);
        this.collection.install('git', info, mount, options, this.installCallback.bind(this));
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
