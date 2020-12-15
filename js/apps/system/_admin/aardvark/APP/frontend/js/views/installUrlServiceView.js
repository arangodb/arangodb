/* jshint browser: true */
/* global Backbone, $, window, arangoHelper, templateEngine, Joi */
(function () {
  'use strict';

  window.ServiceInstallUrlView = Backbone.View.extend({
    el: '#content',
    readOnly: false,

    template: templateEngine.createTemplate('serviceInstallUrlView.ejs'),

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
      'click #installUrlService': 'installUrlService'
    },

    render: function () {
      $(this.el).html(this.template.render({
        services: this.collection,
        upgrade: this._upgrade
      }));

      this.breadcrumb();
      this.setUrlValidators();
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
        arangoHelper.buildServicesSubNav('Remote');
      } else {
        window.setTimeout(function () {
          self.breadcrumb();
        }, 100);
      }
    },

    installUrlService: function () {
      arangoHelper.createMountPointModal(this.installFoxxFromUrl.bind(this));
    },

    installFoxxFromUrl: function () {
      if (window.modalView.modalTestAll()) {
        var mount, info, options, url;
        if (this._upgrade) {
          mount = window.App.replaceAppData.mount;
        } else {
          mount = window.arangoHelper.escapeHtml($('#new-app-mount').val());
          if (mount.charAt(0) !== '/') {
            mount = '/' + mount;
          }
        }
        url = window.arangoHelper.escapeHtml($('#repository').val());

        info = {
          url: url
        };

        // send server req through collection
        options = arangoHelper.getFoxxFlags();
        this.collection.install('url', info, mount, options, this.installCallback.bind(this));
      }
    },

    setUrlValidators: function () {
      window.modalView.modalBindValidation({
        id: 'repository',
        validateInput: function () {
          return [
            {
              rule: Joi.string().required().regex(/^(http)|^(https)/),
              msg: 'No valid http or https url.'
            }
          ];
        }
      });
      window.modalView.modalTestAll();
    },

    installCallback: function (result) {
      window.App.navigate('#services', {trigger: true});
      window.App.applicationsView.installCallback(result);
    }

  });
}());
