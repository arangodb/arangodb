/* jshint browser: true */
/* jshint unused: false */
/* global $, Joi, arangoHelper, _, Backbone, templateEngine, window */
(function () {
  'use strict';

  window.ViewsView = Backbone.View.extend({
    el: '#content',

    template: templateEngine.createTemplate('viewsView.ejs'),

    initialize: function () {
    },

    sortOptions: {
      desc: false
    },

    searchString: '',

    remove: function () {
      this.$el.empty().off(); /* off to unbind the events */
      this.stopListening();
      this.unbind();
      delete this.el;
      return this;
    },

    events: {
      'click #createView': 'createView',
      'click #viewsToggle': 'toggleSettingsDropdown',
      'click .tile': 'gotoView',
      'keyup #viewsSearchInput': 'search',
      'click #viewsSearchSubmit': 'search',
      'click #viewsSortDesc': 'sorting'
    },

    sorting: function () {
      if ($('#viewsSortDesc').is(':checked')) {
        this.setSortingDesc(true);
      } else {
        this.setSortingDesc(false);
      }

      if ($('#viewsDropdown').is(':visible')) {
        this.dropdownVisible = true;
      } else {
        this.dropdownVisible = false;
      }

      this.render();
    },

    setSortingDesc: function (yesno) {
      this.sortOptions.desc = yesno;
    },

    search: function () {
      this.setSearchString($('#viewsSearchInput').val());
      this.render();
    },

    toggleSettingsDropdown: function () {
      // apply sorting to checkboxes
      $('#viewsSortDesc').attr('checked', this.sortOptions.desc);

      $('#viewsToggle').toggleClass('activated');
      $('#viewsDropdown2').slideToggle(200);
    },

    render: function (data) {
      var self = this;

      if (data) {
        self.$el.html(self.template.render({
          views: self.applySorting(data.result),
          searchString: self.getSearchString()
        }));
      } else {
        this.getViews();
        this.$el.html(this.template.render({
          views: [],
          searchString: self.getSearchString()
        }));
      }

      if (self.dropdownVisible === true) {
        $('#viewsSortDesc').attr('checked', self.sortOptions.desc);
        $('#viewsToggle').toggleClass('activated');
        $('#viewsDropdown2').show();
      }

      $('#viewsSortDesc').attr('checked', self.sortOptions.desc);
      arangoHelper.setCheckboxStatus('#viewsDropdown');

      var searchInput = $('#viewsSearchInput');
      var strLength = searchInput.val().length;
      searchInput.focus();
      searchInput[0].setSelectionRange(strLength, strLength);
    },

    setSearchString: function (string) {
      this.searchString = string;
    },

    getSearchString: function () {
      return this.searchString.toLowerCase();
    },

    applySorting: function (data) {
      var self = this;

      // default sorting order
      data = _.sortBy(data, 'name');
      // desc sorting order
      if (this.sortOptions.desc) {
        data = data.reverse();
      }

      var toReturn = [];
      if (this.getSearchString() !== '') {
        _.each(data, function (view, key) {
          if (view && view.name) {
            if (view.name.toLowerCase().indexOf(self.getSearchString()) !== -1) {
              toReturn.push(view);
            }
          }
        });
      } else {
        return data;
      }

      return toReturn;
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
          self.render(data);
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
          if (error.responseJSON && error.responseJSON.errorMessage) {
            arangoHelper.arangoError('Views', error.responseJSON.errorMessage);
          }
        }
      });
    }

  });
}());
