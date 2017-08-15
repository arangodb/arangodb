/* jshint browser: true */
/* global Backbone, $, window, Joi, arangoHelper, templateEngine */
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

      this.renderSelects();
      this.breadcrumb();
      this.setNewAppValidators();
      arangoHelper.createTooltips('.modalTooltips');

      return this;
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
              msg: "Can only contain a to z, A to Z, 0-9, '-' and '_'."
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
              msg: 'Has to be non empty.'
            }
          ];
        }
      });
      window.modalView.modalTestAll();
    },

    breadcrumb: function () {
      var self = this;

      if (window.App.naviView) {
        $('#subNavigationBar .breadcrumb').html(
          '<a href="#services">Services:</a> New'
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
