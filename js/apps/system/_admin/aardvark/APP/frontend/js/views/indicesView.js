/* jshint browser: true */
/* jshint unused: false */
/* global _, arangoHelper, Backbone, window, templateEngine, $ */

(function () {
  'use strict';

  window.IndicesView = Backbone.View.extend({
    el: '#content',

    initialize: function (options) {
      this.collectionName = options.collectionName;
      this.model = this.collection;
    },

    template: templateEngine.createTemplate('indicesView.ejs'),

    events: {
    },

    render: function () {
      $(this.el).html(this.template.render({
        model: this.model
      }));

      this.breadcrumb();
      window.arangoHelper.buildCollectionSubNav(this.collectionName, 'Indices');

      this.getIndex();
    },

    breadcrumb: function () {
      $('#subNavigationBar .breadcrumb').html(
        'Collection: ' + this.collectionName
      );
    },

    getIndex: function () {
      var callback = function (error, data) {
        if (error) {
          window.arangoHelper.arangoError('Index', data.errorMessage);
        } else {
          this.renderIndex(data);
        }
      }.bind(this);

      this.model.getIndex(callback);
    },

    createIndex: function () {
      // e.preventDefault()
      var self = this;
      var indexType = $('#newIndexType').val();
      var postParameter = {};
      var fields;
      var unique;
      var sparse;

      switch (indexType) {
        case 'Geo':
          // HANDLE ARRAY building
          fields = $('#newGeoFields').val();
          var geoJson = self.checkboxToValue('#newGeoJson');
          postParameter = {
            type: 'geo',
            fields: self.stringToArray(fields),
            geoJson: geoJson
          };
          break;
        case 'Persistent':
          fields = $('#newPersistentFields').val();
          unique = self.checkboxToValue('#newPersistentUnique');
          sparse = self.checkboxToValue('#newPersistentSparse');
          postParameter = {
            type: 'persistent',
            fields: self.stringToArray(fields),
            unique: unique,
            sparse: sparse
          };
          break;
        case 'Hash':
          fields = $('#newHashFields').val();
          unique = self.checkboxToValue('#newHashUnique');
          sparse = self.checkboxToValue('#newHashSparse');
          postParameter = {
            type: 'hash',
            fields: self.stringToArray(fields),
            unique: unique,
            sparse: sparse
          };
          break;
        case 'Fulltext':
          fields = ($('#newFulltextFields').val());
          var minLength = parseInt($('#newFulltextMinLength').val(), 10) || 0;
          postParameter = {
            type: 'fulltext',
            fields: self.stringToArray(fields),
            minLength: minLength
          };
          break;
        case 'Skiplist':
          fields = $('#newSkiplistFields').val();
          unique = self.checkboxToValue('#newSkiplistUnique');
          sparse = self.checkboxToValue('#newSkiplistSparse');
          postParameter = {
            type: 'skiplist',
            fields: self.stringToArray(fields),
            unique: unique,
            sparse: sparse
          };
          break;
      }

      var callback = function (error, msg) {
        if (error) {
          if (msg) {
            var message = JSON.parse(msg.responseText);
            arangoHelper.arangoError('Document error', message.errorMessage);
          } else {
            arangoHelper.arangoError('Document error', 'Could not create index.');
          }
        }
        // toggle back
        self.toggleNewIndexView();

        // rerender
        self.render();
      };

      this.model.createIndex(postParameter, callback);
    },

    bindIndexEvents: function () {
      this.unbindIndexEvents();
      var self = this;

      $('#indexEditView #addIndex').bind('click', function () {
        self.toggleNewIndexView();

        $('#cancelIndex').unbind('click');
        $('#cancelIndex').bind('click', function () {
          self.toggleNewIndexView();
          self.render();
        });

        $('#createIndex').unbind('click');
        $('#createIndex').bind('click', function () {
          self.createIndex();
        });
      });

      $('#newIndexType').bind('change', function () {
        self.selectIndexType();
      });

      $('.deleteIndex').bind('click', function (e) {
        self.prepDeleteIndex(e);
      });

      $('#infoTab a').bind('click', function (e) {
        $('#indexDeleteModal').remove();
        if ($(e.currentTarget).html() === 'Indices' && !$(e.currentTarget).parent().hasClass('active')) {
          $('#newIndexView').hide();
          $('#indexEditView').show();

          $('#indexHeaderContent #modal-dialog .modal-footer .button-danger').hide();
          $('#indexHeaderContent #modal-dialog .modal-footer .button-success').hide();
          $('#indexHeaderContent #modal-dialog .modal-footer .button-notification').hide();
        }
        if ($(e.currentTarget).html() === 'General' && !$(e.currentTarget).parent().hasClass('active')) {
          $('#indexHeaderContent #modal-dialog .modal-footer .button-danger').show();
          $('#indexHeaderContent #modal-dialog .modal-footer .button-success').show();
          $('#indexHeaderContent #modal-dialog .modal-footer .button-notification').show();
          var elem2 = $('.index-button-bar2')[0];
          // $('#addIndex').detach().appendTo(elem)
          if ($('#cancelIndex').is(':visible')) {
            $('#cancelIndex').detach().appendTo(elem2);
            $('#createIndex').detach().appendTo(elem2);
          }
        }
      });
    },

    prepDeleteIndex: function (e) {
      var self = this;
      this.lastTarget = e;

      this.lastId = $(this.lastTarget.currentTarget).parent().parent().first().children().first().text();
      // window.modalView.hide()

      // delete modal
      $('#content #modal-dialog .modal-footer').after(
        '<div id="indexDeleteModal" style="display:block;" class="alert alert-error modal-delete-confirmation">' +
        '<strong>Really delete?</strong>' +
        '<button id="indexConfirmDelete" class="button-danger pull-right modal-confirm-delete">Yes</button>' +
        '<button id="indexAbortDelete" class="button-neutral pull-right">No</button>' +
        '</div>'
      );
      $('#indexHeaderContent #indexConfirmDelete').unbind('click');
      $('#indexHeaderContent #indexConfirmDelete').bind('click', function () {
        $('#indexHeaderContent #indexDeleteModal').remove();
        self.deleteIndex();
      });

      $('#indexHeaderContent #indexAbortDelete').unbind('click');
      $('#indexHeaderContent #indexAbortDelete').bind('click', function () {
        $('#indexHeaderContent #indexDeleteModal').remove();
      });
    },

    unbindIndexEvents: function () {
      $('#indexHeaderContent #indexEditView #addIndex').unbind('click');
      $('#indexHeaderContent #newIndexType').unbind('change');
      $('#indexHeaderContent #infoTab a').unbind('click');
      $('#indexHeaderContent .deleteIndex').unbind('click');
    },

    deleteIndex: function () {
      var callback = function (error) {
        if (error) {
          arangoHelper.arangoError('Could not delete index');
          $("tr th:contains('" + this.lastId + "')").parent().children().last().html(
            '<span class="deleteIndex icon_arangodb_roundminus"' +
            ' data-original-title="Delete index" title="Delete index"></span>'
          );
          this.model.set('locked', false);
        } else if (!error && error !== undefined) {
          $("tr th:contains('" + this.lastId + "')").parent().remove();
          this.model.set('locked', false);
        }
      }.bind(this);

      this.model.set('locked', true);
      this.model.deleteIndex(this.lastId, callback);

      $("tr th:contains('" + this.lastId + "')").parent().children().last().html(
        '<i class="fa fa-circle-o-notch fa-spin"></i>'
      );
    },
    renderIndex: function (data) {
      this.index = data;

      var cssClass = 'collectionInfoTh modal-text';
      if (this.index) {
        var fieldString = '';
        var actionString = '';

        _.each(this.index.indexes, function (v) {
          if (v.type === 'primary' || v.type === 'edge') {
            actionString = '<span class="icon_arangodb_locked" ' +
              'data-original-title="No action"></span>';
          } else {
            actionString = '<span class="deleteIndex icon_arangodb_roundminus" ' +
              'data-original-title="Delete index" title="Delete index"></span>';
          }

          if (v.fields !== undefined) {
            fieldString = v.fields.join(', ');
          }

          // cut index id
          var position = v.id.indexOf('/');
          var indexId = v.id.substr(position + 1, v.id.length);
          var selectivity = (
          v.hasOwnProperty('selectivityEstimate')
            ? (v.selectivityEstimate * 100).toFixed(2) + '%'
            : 'n/a'
          );
          var sparse = (v.hasOwnProperty('sparse') ? v.sparse : 'n/a');

          $('#collectionEditIndexTable').append(
            '<tr>' +
            '<th class=' + JSON.stringify(cssClass) + '>' + indexId + '</th>' +
            '<th class=' + JSON.stringify(cssClass) + '>' + v.type + '</th>' +
            '<th class=' + JSON.stringify(cssClass) + '>' + v.unique + '</th>' +
            '<th class=' + JSON.stringify(cssClass) + '>' + sparse + '</th>' +
            '<th class=' + JSON.stringify(cssClass) + '>' + selectivity + '</th>' +
            '<th class=' + JSON.stringify(cssClass) + '>' + fieldString + '</th>' +
            '<th class=' + JSON.stringify(cssClass) + '>' + actionString + '</th>' +
            '</tr>'
          );
        });
      }
      this.bindIndexEvents();
    },

    selectIndexType: function () {
      $('.newIndexClass').hide();
      var type = $('#newIndexType').val();
      $('#newIndexType' + type).show();
    },

    resetIndexForms: function () {
      $('#indexHeader input').val('').prop('checked', false);
      $('#newIndexType').val('Geo').prop('selected', true);
      this.selectIndexType();
    },

    toggleNewIndexView: function () {
      var elem = $('.index-button-bar2')[0];

      if ($('#indexEditView').is(':visible')) {
        $('#indexEditView').hide();
        $('#newIndexView').show();
        $('#cancelIndex').detach().appendTo('#indexHeaderContent #modal-dialog .modal-footer');
        $('#createIndex').detach().appendTo('#indexHeaderContent #modal-dialog .modal-footer');
      } else {
        $('#indexEditView').show();
        $('#newIndexView').hide();
        $('#cancelIndex').detach().appendTo(elem);
        $('#createIndex').detach().appendTo(elem);
      }

      arangoHelper.fixTooltips('.icon_arangodb, .arangoicon', 'right');
      this.resetIndexForms();
    },

    stringToArray: function (fieldString) {
      var fields = [];
      fieldString.split(',').forEach(function (field) {
        field = field.replace(/(^\s+|\s+$)/g, '');
        if (field !== '') {
          fields.push(field);
        }
      });
      return fields;
    },

    checkboxToValue: function (id) {
      return $(id).prop('checked');
    }

  });
}());
