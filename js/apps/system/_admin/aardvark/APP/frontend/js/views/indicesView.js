/* jshint browser: true */
/* jshint unused: false */
/* global _, arangoHelper, Backbone, window, templateEngine, $ */

(function () {
  'use strict';

  window.IndicesView = Backbone.View.extend({
    el: '#content',

    initialize: function (options) {
      var self = this;
      this.collectionName = options.collectionName;
      this.model = this.collection;

      // rerender
      self.interval = window.setInterval(function () {
        if (window.location.hash.indexOf('cIndices/' + self.collectionName) !== -1 && window.VISIBLE) {
          if ($('#collectionEditIndexTable').is(':visible') && !$('#indexDeleteModal').is(':visible')) {
            self.rerender();
          }
        }
      }, self.refreshRate);
    },

    interval: null,
    refreshRate: 10000,

    template: templateEngine.createTemplate('indicesView.ejs'),

    events: {
    },

    remove: function () {
      if (this.interval) {
        window.clearInterval(this.interval);
      }
      this.$el.empty().off(); /* off to unbind the events */
      this.stopListening();
      this.unbind();
      delete this.el;
      return this;
    },

    render: function () {
      var self = this;

      var continueFunction = function (data) {
        /* filter out index aliases */
        var aliases = data.supports.aliases;
        if (!aliases) {
          aliases = {};
        } else {
          aliases = aliases.indexes;
        }
        $(self.el).html(self.template.render({
          model: self.model,
          supported: data.supports.indexes.filter(function (type) {
            return !aliases.hasOwnProperty(type);
          })
        }));

        self.breadcrumb();
        window.arangoHelper.buildCollectionSubNav(self.collectionName, 'Indexes');

        self.getIndex();

        // check permissions and adjust views
        arangoHelper.checkCollectionPermissions(self.collectionName, self.changeViewToReadOnly);
      };

      if (!this.engineData) {
        $.ajax({
          type: 'GET',
          cache: false,
          url: arangoHelper.databaseUrl('/_api/engine'),
          contentType: 'application/json',
          processData: false,
          success: function (data) {
            self.engineData = data;
            continueFunction(data);
          },
          error: function () {
            arangoHelper.arangoNotification('Index', 'Could not fetch index information.');
          }
        });
      } else {
        continueFunction(this.engineData);
      }
    },

    rerender: function () {
      this.getIndex(true);
    },

    changeViewToReadOnly: function () {
      $('.breadcrumb').html($('.breadcrumb').html() + ' (read-only)');
      // this method disables all write-based functions
      $('#addIndex').addClass('disabled');
      $('#addIndex').css('color', 'rgba(0,0,0,.5)');
      $('#addIndex').css('cursor', 'not-allowed');
    },

    breadcrumb: function () {
      $('#subNavigationBar .breadcrumb').html(
        'Collection: ' + (this.collectionName.length > 64 ? this.collectionName.substr(0, 64) + "..." : this.collectionName)
      );
    },

    getIndex: function (rerender) {
      var callback = function (error, data, id) {
        if (error) {
          window.arangoHelper.arangoError('Index', data.errorMessage);
        } else {
          this.renderIndex(data, id, rerender);
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
      var deduplicate;
      var background;
      var name;

      switch (indexType) {
        case 'Ttl':
          fields = $('#newTtlFields').val();
          var expireAfter = parseInt($('#newTtlExpireAfter').val(), 10) || 0;
          background = self.checkboxToValue('#newTtlBackground');
          name = $('#newTtlName').val();
          postParameter = {
            type: 'ttl',
            fields: self.stringToArray(fields),
            expireAfter: expireAfter,
            inBackground: background,
            name: name
          };
          break;
        case 'Geo':
          // HANDLE ARRAY building
          fields = $('#newGeoFields').val();
          background = self.checkboxToValue('#newGeoBackground');
          var geoJson = self.checkboxToValue('#newGeoJson');
          name = $('#newGeoName').val();
          postParameter = {
            type: 'geo',
            fields: self.stringToArray(fields),
            geoJson: geoJson,
            inBackground: background,
            name: name
          };
          break;
        case 'Persistent':
          fields = $('#newPersistentFields').val();
          unique = self.checkboxToValue('#newPersistentUnique');
          sparse = self.checkboxToValue('#newPersistentSparse');
          deduplicate = self.checkboxToValue('#newPersistentDeduplicate');
          background = self.checkboxToValue('#newPersistentBackground');
          name = $('#newPersistentName').val();
          postParameter = {
            type: 'persistent',
            fields: self.stringToArray(fields),
            unique: unique,
            sparse: sparse,
            deduplicate: deduplicate,
            inBackground: background,
            name: name
          };
          break;
        case 'Hash':
          fields = $('#newHashFields').val();
          unique = self.checkboxToValue('#newHashUnique');
          sparse = self.checkboxToValue('#newHashSparse');
          deduplicate = self.checkboxToValue('#newHashDeduplicate');
          background = self.checkboxToValue('#newHashBackground');
          name = $('#newHashName').val();
          postParameter = {
            type: 'hash',
            fields: self.stringToArray(fields),
            unique: unique,
            sparse: sparse,
            deduplicate: deduplicate,
            inBackground: background,
            name: name
          };
          break;
        case 'Fulltext':
          fields = $('#newFulltextFields').val();
          var minLength = parseInt($('#newFulltextMinLength').val(), 10) || 0;
          background = self.checkboxToValue('#newFulltextBackground');
          name = $('#newFulltextName').val();
          postParameter = {
            type: 'fulltext',
            fields: self.stringToArray(fields),
            minLength: minLength,
            inBackground: background,
            name: name
          };
          break;
        case 'Skiplist':
          fields = $('#newSkiplistFields').val();
          unique = self.checkboxToValue('#newSkiplistUnique');
          sparse = self.checkboxToValue('#newSkiplistSparse');
          deduplicate = self.checkboxToValue('#newSkiplistDeduplicate');
          background = self.checkboxToValue('#newSkiplistBackground');
          name = $('#newSkiplistName').val();
          postParameter = {
            type: 'skiplist',
            fields: self.stringToArray(fields),
            unique: unique,
            sparse: sparse,
            deduplicate: deduplicate,
            inBackground: background,
            name: name
          };
          break;
      }

      var callback = function (error, msg) {
        if (error) {
          if (msg) {
            var message = JSON.parse(msg.responseText);
            arangoHelper.arangoError('Index error', message.errorMessage);
          } else {
            arangoHelper.arangoError('Index error', 'Could not create index.');
          }
        } else {
          arangoHelper.arangoNotification('Index', 'Creation in progress. This may take a while.');
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
        if ($(e.currentTarget).html() === 'Indexes' && !$(e.currentTarget).parent().hasClass('active')) {
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

      if ($('#indexDeleteModal').length) {
        return;
      }

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
    renderIndex: function (data, id, rerender) {
      this.index = data;

      // get pending jobs
      var checkState = function (error, data) {
        if (error) {
          arangoHelper.arangoError('Jobs', 'Could not read pending jobs.');
        } else {
          var readJob = function (error, data, job) {
            if (error) {
              if (data.responseJSON.code === 404) {
                // delete non existing aardvark job
                arangoHelper.deleteAardvarkJob(job);
              } else if (data.responseJSON.code === 400) {
                // index job failed -> print error
                arangoHelper.arangoError('Index creation failed', data.responseJSON.errorMessage);
                // delete non existing aardvark job
                arangoHelper.deleteAardvarkJob(job);
              } else if (data.responseJSON.code === 204) {
                // job is still in quere or pending
                arangoHelper.arangoMessage('Index', 'There is at least one new index in the queue or in the process of being created.');
              }
            } else {
              arangoHelper.deleteAardvarkJob(job);
            }
          };

          _.each(data, function (job) {
            if (job.collection === id) {
              $.ajax({
                type: 'PUT',
                cache: false,
                url: arangoHelper.databaseUrl('/_api/job/' + job.id),
                contentType: 'application/json',
                success: function (data, a, b) {
                  readJob(false, data, job.id);
                },
                error: function (data) {
                  readJob(true, data, job.id);
                }
              });
            }
          });
        }
      };

      arangoHelper.getAardvarkJobs(checkState);

      var cssClass = 'collectionInfoTh modal-text';
      if (this.index) {
        var fieldString = '';
        var actionString = '';

        if (rerender) {
          $('#collectionEditIndexTable tbody').empty();
        }

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
          var extras = [];
          ["deduplicate", "expireAfter", "minLength", "geoJson"].forEach(function(k) {
            if (v.hasOwnProperty(k)) {
              extras.push(k + ": " + v[k]);
            }
          });

          $('#collectionEditIndexTable').append(
            '<tr>' +
            '<th class=' + JSON.stringify(cssClass) + '>' + arangoHelper.escapeHtml(indexId) + '</th>' +
            '<th class=' + JSON.stringify(cssClass) + '>' + arangoHelper.escapeHtml(v.type) + '</th>' +
            '<th class=' + JSON.stringify(cssClass) + '>' + arangoHelper.escapeHtml(v.unique) + '</th>' +
            '<th class=' + JSON.stringify(cssClass) + '>' + arangoHelper.escapeHtml(sparse) + '</th>' +
            '<th class=' + JSON.stringify(cssClass) + '>' + arangoHelper.escapeHtml(extras.join(", ")) + '</th>' +
            '<th class=' + JSON.stringify(cssClass) + '>' + arangoHelper.escapeHtml(selectivity) + '</th>' +
            '<th class=' + JSON.stringify(cssClass) + '>' + arangoHelper.escapeHtml(fieldString) + '</th>' +
            '<th class=' + JSON.stringify(cssClass) + '>' + arangoHelper.escapeHtml(v.name) + '</th>' +
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
      if (type === null) {
        type = $('#newIndexType').children().first().attr('value');
        $('#newIndexType').val(arangoHelper.escapeHtml(type));
      }
      $('#newIndexType' + type).show();
    },

    resetIndexForms: function () {
      $('#indexHeader input').val('').prop('checked', false);
      $('#newIndexType').val('unknown').prop('selected', true);
      this.selectIndexType();
    },

    toggleNewIndexView: function () {
      if (!$('#addIndex').hasClass('disabled')) {
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

        this.resetIndexForms();
      }
      arangoHelper.createTooltips('.index-tooltip');
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
