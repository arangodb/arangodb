/* jshint browser: true */
/* jshint unused: false */
/* global $, Joi, arangoHelper, Backbone, templateEngine, window */
(function () {
  'use strict';

  window.ViewsView = Backbone.View.extend({
    el: '#content',

    template: templateEngine.createTemplate('viewsView.ejs'),

    initialize: function () {
    },

    remove: function () {
      this.$el.empty().off(); /* off to unbind the events */
      this.stopListening();
      this.unbind();
      delete this.el;
      return this;
    },

    events: {
      'click #createView': 'createView',
      'click .tile': 'gotoView'
    },

    render: function () {
      this.getViews();
      this.$el.html(this.template.render({
        views: []
      }));
    },

    gotoView: function (e) {
      var name = $(e.currentTarget).attr('id');
      if (name) {
        window.location.hash = window.location.hash.substr(0, window.location.hash.length - 1) + '/' + encodeURIComponent(name);
      }
    },

    getViews: function () {
      var self = this;

      $.ajax({
        type: 'GET',
        cache: false,
        url: arangoHelper.databaseUrl('/_api/view'),
        contentType: 'application/json',
        processData: false,
        success: function (data) {
          self.$el.html(self.template.render({
            views: data
          }));
        },
        error: function (error) {
          console.log(error);
        }
      });
    },

    createView: function (e) {
      e.preventDefault();
      this.createViewModal();
    },

    createViewModal: function () {
      var buttons = [];
      var tableContent = [];

      tableContent.push(
        window.modalView.createTextEntry(
          'newName',
          'Name',
          '',
          false,
          'Name',
          true,
          [
            {
              rule: Joi.string().regex(/^[a-zA-Z0-9\-_]*$/),
              msg: 'Only symbols, "_" and "-" are allowed.'
            },
            {
              rule: Joi.string().required(),
              msg: 'No view name given.'
            }
          ]
        )
      );

      tableContent.push(
        window.modalView.createReadOnlyEntry(
          undefined,
          'Type',
          'arangosearch',
          undefined,
          undefined,
          false,
          undefined
        )
      );

      buttons.push(
        window.modalView.createSuccessButton('Create', this.submitCreateView.bind(this))
      );

      window.modalView.show('modalTable.ejs', 'Create New View', buttons, tableContent);
    },

    submitCreateView: function () {
      var self = this;
      var name = $('#newName').val();
      var options = JSON.stringify({
        name: name,
        type: 'arangosearch',
        properties: {}
      });

      $.ajax({
        type: 'POST',
        cache: false,
        url: arangoHelper.databaseUrl('/_api/view'),
        contentType: 'application/json',
        processData: false,
        data: options,
        success: function (data) {
          window.modalView.hide();
          self.getViews();
        },
        error: function (error) {
          console.log(error);
        }
      });
    }

  });
}());
