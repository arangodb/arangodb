/* jshint browser: true */
/* global Backbone, $, _, window, Joi, arangoHelper, templateEngine */
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
      'click #installNewService': 'installNewService',
      'keydown input': 'checkValidators'
    },

    checkValidators: function () {
      if (window.modalView._validators.length !== 4) {
        window.modalView.clearValidators();
        this.setNewAppValidators();
      }
    },

    initialize: function () {
      if (window.App.replaceApp) {
        this._upgrade = true;
      }
    },

    render: function () {
      // if repo not fetched yet, wait
      $(this.el).html(this.template.render({
        services: this.collection,
        upgrade: this._upgrade
      }));

      this.renderSelects();
      this.breadcrumb();
      this.setNewAppValidators();
      arangoHelper.createTooltips('.modalTooltips');

      return this;
    },

    installNewService: function () {
      arangoHelper.createMountPointModal(this.generateNewFoxxApp.bind(this));
    },

    generateNewFoxxApp: function () {
      if (window.modalView.modalTestAll()) {
        var mount, info, options;
        if (this._upgrade) {
          mount = window.App.replaceAppData.mount;
        } else {
          mount = window.arangoHelper.escapeHtml($('#new-app-mount').val());
          if (mount.charAt(0) !== '/') {
            mount = '/' + mount;
          }
        }

        info = {
          name: window.arangoHelper.escapeHtml($('#new-app-name').val()),
          documentCollections: _.map($('#new-app-document-collections').select2('data'), function (d) {
            return window.arangoHelper.escapeHtml(d.text);
          }),
          edgeCollections: _.map($('#new-app-edge-collections').select2('data'), function (d) {
            return window.arangoHelper.escapeHtml(d.text);
          }),
          //        authenticated: window.arangoHelper.escapeHtml($("#new-app-name").val()),
          author: window.arangoHelper.escapeHtml($('#new-app-author').val()),
          license: window.arangoHelper.escapeHtml($('#new-app-license').val()),
          description: window.arangoHelper.escapeHtml($('#new-app-description').val())
        };

        options = arangoHelper.getFoxxFlags();
        this.collection.install('generate', info, mount, options, this.installCallback.bind(this));
      }
      window.modalView.hide();
    },

    checkValidation: function () {
      window.modalView.modalTestAll();
    },

    installCallback: function (result) {
      window.App.navigate('#services', {trigger: true});
      window.App.applicationsView.installCallback(result);
    },

    renderSelects: function () {
      $('#new-app-document-collections').select2({
        tags: [],
        showSearchBox: false,
        minimumResultsForSearch: -1,
        width: '336px'
      });
      $('#new-app-edge-collections').select2({
        tags: [],
        showSearchBox: false,
        minimumResultsForSearch: -1,
        width: '336px'
      });
    },

    setNewAppValidators: function () {
      window.modalView.modalBindValidation({
        id: 'new-app-author',
        validateInput: function () {
          return [
            {
              rule: Joi.string().required().min(1),
              msg: 'Has to be non empty.'
            }
          ];
        }
      });
      window.modalView.modalBindValidation({
        id: 'new-app-name',
        validateInput: function () {
          return [
            {
              rule: Joi.string().required().regex(/^[a-zA-Z\-_][a-zA-Z0-9\-_]*$/),
              msg: "Can only contain a to z, A to Z, 0-9, '-' and '_'. Cannot start with a number."
            }
          ];
        }
      });

      window.modalView.modalBindValidation({
        id: 'new-app-description',
        validateInput: function () {
          return [
            {
              rule: Joi.string().required().min(1),
              msg: 'Has to be non empty.'
            }
          ];
        }
      });

      window.modalView.modalBindValidation({
        id: 'new-app-license',
        validateInput: function () {
          return [
            {
              rule: Joi.string().required().regex(/^[a-zA-Z0-9 .,;-]+$/),
              msg: "Can only contain a to z, A to Z, 0-9, '-', '.', ',' and ';'."
            }
          ];
        }
      });
      window.modalView.modalTestAll();
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
        arangoHelper.buildServicesSubNav('New');
      } else {
        window.setTimeout(function () {
          self.breadcrumb();
        }, 100);
      }
    }

  });
}());
