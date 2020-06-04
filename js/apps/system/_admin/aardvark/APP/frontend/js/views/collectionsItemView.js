/* jshint browser: true */
/* jshint unused: false */
/* global window, frontendConfig, Backbone, _, $, templateEngine, arangoHelper, Joi */

(function () {
  'use strict';

  window.CollectionListItemView = Backbone.View.extend({
    tagName: 'div',
    className: 'tile pure-u-1-1 pure-u-sm-1-2 pure-u-md-1-3 pure-u-lg-1-4 pure-u-xl-1-6',
    template: templateEngine.createTemplate('collectionsItemView.ejs'),

    initialize: function (options) {
      this.collectionsView = options.collectionsView;
    },

    events: {
      'click .iconSet.icon_arangodb_settings2': 'createEditPropertiesModal',
      'click .pull-left': 'noop',
      'click .icon_arangodb_settings2': 'editProperties',
      'click .spanInfo': 'showProperties',
      'click': 'selectCollection'
    },

    render: function () {
      if (this.model.get('locked') || this.model.get('status') === 'corrupted') {
        $(this.el).addClass('locked');
        $(this.el).addClass(this.model.get('lockType'));
      } else {
        $(this.el).removeClass('locked');
      }
      if (this.model.get('status') === 'loading' || this.model.get('status') === 'unloading') {
        $(this.el).addClass('locked');
      }
      $(this.el).html(this.template.render({
        model: this.model
      }));
      $(this.el).attr('id', 'collection_' + this.model.get('name'));

      return this;
    },

    editProperties: function (event) {
      if (this.model.get('locked')) {
        return 0;
      }
      event.stopPropagation();
      this.createEditPropertiesModal();
    },

    showProperties: function (event) {
      if (this.model.get('locked')) {
        return 0;
      }
      event.stopPropagation();
      this.createInfoModal();
    },

    selectCollection: function (event) {
      // check if event was fired from disabled button
      if ($(event.target).hasClass('disabled')) {
        return 0;
      }
      if (this.model.get('locked')) {
        return 0;
      }
      if (this.model.get('status') === 'loading') {
        return 0;
      }
      if (this.model.get('status') === 'corrupted') {
        return 0;
      }

      if (this.model.get('status') === 'unloaded') {
        this.loadCollection();
      } else {
        window.App.navigate(
          'collection/' + encodeURIComponent(this.model.get('name')) + '/documents/1', {trigger: true}
        );
      }
    },

    noop: function (event) {
      event.stopPropagation();
    },

    unloadCollection: function () {
      var unloadCollectionCallback = function (error) {
        if (error) {
          arangoHelper.arangoError('Collection error', this.model.get('name') + ' could not be unloaded.');
        } else if (error === undefined) {
          this.model.set('status', 'unloading');
          this.render();
        } else {
          if (window.location.hash === '#collections') {
            this.model.set('status', 'unloaded');
            this.render();
          } else {
            arangoHelper.arangoNotification('Collection ' + this.model.get('name') + ' unloaded.');
          }
        }
      }.bind(this);

      this.model.unloadCollection(unloadCollectionCallback);
      window.modalView.hide();
    },

    loadCollection: function () {
      var loadCollectionCallback = function (error) {
        if (error) {
          arangoHelper.arangoError('Collection error', this.model.get('name') + ' could not be loaded.');
        } else if (error === undefined) {
          this.model.set('status', 'loading');
          this.render();
        } else {
          if (window.location.hash === '#collections') {
            this.model.set('status', 'loaded');
            this.render();
          } else {
            arangoHelper.arangoNotification('Collection ' + this.model.get('name') + ' loaded.');
          }
        }
      }.bind(this);

      this.model.loadCollection(loadCollectionCallback);
      window.modalView.hide();
    },

    truncateCollection: function () {
      this.model.truncateCollection();
      window.modalView.hide();
    },

    warumupCollection: function () {
      this.model.warumupCollection();
      window.modalView.hide();
    },

    deleteCollection: function () {
      this.model.destroy(
        {
          error: function (_, data) {
            arangoHelper.arangoError('Could not drop collection: ' + data.responseJSON.errorMessage);
          },
          success: function () {
            window.modalView.hide();
          }
        }
      );
      this.collectionsView.render();
    },

    saveModifiedCollection: function () {
      var callback = function (error, isCoordinator) {
        if (error) {
          arangoHelper.arangoError('Error', 'Could not get coordinator info');
        } else {
          var newname;
          if (isCoordinator) {
            newname = this.model.get('name');
          } else {
            newname = $('#change-collection-name').val();
          }
          var status = this.model.get('status');

          if (status === 'loaded') {
            var callbackChange = function (error) {
              if (error) {
                arangoHelper.arangoError('Collection error: ' + error.responseText);
              } else {
                this.collectionsView.render();
                window.modalView.hide();
              }
            }.bind(this);

            var callbackRename = function (error) {
              if (error) {
                arangoHelper.arangoError('Collection error: ' + error.responseText);
              } else {
                var wfs = $('#change-collection-sync').val();
                var replicationFactor;
                if (frontendConfig.isCluster) {
                  replicationFactor = $('#change-replication-factor').val();
                }

                this.model.changeCollection(wfs, replicationFactor, callbackChange);
              }
            }.bind(this);

            if (frontendConfig.isCluster === false) {
              this.model.renameCollection(newname, callbackRename);
            } else {
              callbackRename();
            }
          } else if (status === 'unloaded') {
            if (this.model.get('name') !== newname) {
              var callbackRename2 = function (error, data) {
                if (error) {
                  arangoHelper.arangoError('Collection error: ' + data.responseText);
                } else {
                  this.collectionsView.render();
                  window.modalView.hide();
                }
              }.bind(this);

              if (frontendConfig.isCluster === false) {
                this.model.renameCollection(newname, callbackRename2);
              } else {
                callbackRename2();
              }
            } else {
              window.modalView.hide();
            }
          }
        }
      }.bind(this);

      window.isCoordinator(callback);
    },

    createEditPropertiesModal: function () {
      var callback = function (error, isCoordinator) {
        if (error) {
          arangoHelper.arangoError('Error', 'Could not get coordinator info');
        } else {
          var collectionIsLoaded = false;

          if (this.model.get('status') === 'loaded') {
            collectionIsLoaded = true;
          }

          var buttons = [];
          var tableContent = [];

          if (!isCoordinator) {
            tableContent.push(
              window.modalView.createTextEntry(
                'change-collection-name',
                'Name',
                this.model.get('name'),
                false,
                '',
                true,
                [
                  {
                    rule: Joi.string().regex(/^[a-zA-Z]/),
                    msg: 'Collection name must always start with a letter.'
                  },
                  {
                    rule: Joi.string().regex(/^[a-zA-Z0-9\-_]*$/),
                    msg: 'Only Symbols "_" and "-" are allowed.'
                  },
                  {
                    rule: Joi.string().required(),
                    msg: 'No collection name given.'
                  }
                ]
              )
            );
          }

          var after = function () {
            tableContent.push(
              window.modalView.createReadOnlyEntry(
                'change-collection-id', 'ID', this.model.get('id'), ''
              )
            );
            tableContent.push(
              window.modalView.createReadOnlyEntry(
                'change-collection-type', 'Type', this.model.get('type'), ''
              )
            );
            tableContent.push(
              window.modalView.createReadOnlyEntry(
                'change-collection-status', 'Status', this.model.get('status'), ''
              )
            );
            buttons.push(
              window.modalView.createDeleteButton(
                'Delete',
                this.deleteCollection.bind(this)
              )
            );
            buttons.push(
              window.modalView.createDeleteButton(
                'Truncate',
                this.truncateCollection.bind(this)
              )
            );

            if (frontendConfig.engine === 'rocksdb') {
              buttons.push(
                window.modalView.createDeleteButton(
                  'Load Indexes into Memory',
                  this.warumupCollection.bind(this)
                )
              );
            }
            if (collectionIsLoaded) {
              buttons.push(
                window.modalView.createNotificationButton(
                  'Unload',
                  this.unloadCollection.bind(this)
                )
              );
            } else {
              buttons.push(
                window.modalView.createNotificationButton(
                  'Load',
                  this.loadCollection.bind(this)
                )
              );
            }

            buttons.push(
              window.modalView.createSuccessButton(
                'Save',
                this.saveModifiedCollection.bind(this)
              )
            );

            var tabBar = ['General', 'Indexes'];
            var templates = ['modalTable.ejs', 'indicesView.ejs'];

            window.modalView.show(
              templates,
              'Modify Collection',
              buttons,
              tableContent, null, null,
              this.events, null,
              tabBar
            );
            if (this.model.get('status') === 'loaded') {
              this.getIndex();
            } else {
              $($('#infoTab').children()[1]).remove();
            }
          }.bind(this);

          if (collectionIsLoaded) {
            var callback2 = function (error, data) {
              if (error) {
                arangoHelper.arangoError('Collection', 'Could not fetch properties');
              } else {
                var wfs = data.waitForSync;

                // prevent "unexpected sync method error"
                tableContent.push(
                  window.modalView.createSelectEntry(
                    'change-collection-sync',
                    'Wait for sync',
                    wfs,
                    'Synchronize to disk before returning from a create or update of a document.',
                    [{value: false, label: 'No'}, {value: true, label: 'Yes'}])
                );
              }
              after();
            };

            this.model.getProperties(callback2);
          } else {
            after();
          }
        }
      }.bind(this);
      window.isCoordinator(callback);
    },

    bindIndexEvents: function () {
      this.unbindIndexEvents();
      var self = this;

      $('#indexEditView #addIndex').bind('click', function () {
        self.toggleNewIndexView();

        $('#cancelIndex').unbind('click');
        $('#cancelIndex').bind('click', function () {
          self.toggleNewIndexView();
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

          $('#modal-dialog .modal-footer .button-danger').hide();
          $('#modal-dialog .modal-footer .button-success').hide();
          $('#modal-dialog .modal-footer .button-notification').hide();
        // $('#addIndex').detach().appendTo('#modal-dialog .modal-footer')
        }
        if ($(e.currentTarget).html() === 'General' && !$(e.currentTarget).parent().hasClass('active')) {
          $('#modal-dialog .modal-footer .button-danger').show();
          $('#modal-dialog .modal-footer .button-success').show();
          $('#modal-dialog .modal-footer .button-notification').show();
          var elem2 = $('.index-button-bar2')[0];
          // $('#addIndex').detach().appendTo(elem)
          if ($('#cancelIndex').is(':visible')) {
            $('#cancelIndex').detach().appendTo(elem2);
            $('#createIndex').detach().appendTo(elem2);
          }
        }
      });
    },

    unbindIndexEvents: function () {
      $('#indexEditView #addIndex').unbind('click');
      $('#newIndexType').unbind('change');
      $('#infoTab a').unbind('click');
      $('.deleteIndex').unbind('click');
    /*
    //$('#documentsToolbar ul').unbind('click')
    this.markFilterToggle()
    this.changeEditMode(false)
    "click #documentsToolbar ul"    : "resetIndexForms"
    */
    },

    createInfoModal: function () {
      var callbackRev = function (error, revision, figures) {
        if (error) {
          arangoHelper.arangoError('Figures', 'Could not get revision.');
        } else {
          var buttons = [];
          var tableContent = {
            figures: figures,
            revision: revision,
            model: this.model
          };
          window.modalView.show(
            'modalCollectionInfo.ejs',
            'Collection: ' + (this.model.get('name').length > 64 ? this.model.get('name').substr(0, 64) + "..." : this.model.get('name')),
            buttons,
            tableContent
          );
        }
      }.bind(this);

      var callback = function (error, data) {
        if (error) {
          arangoHelper.arangoError('Figures', 'Could not get figures.');
        } else {
          var figures = data;
          this.model.getRevision(callbackRev, figures);
        }
      }.bind(this);

      this.model.getFigures(callback);
    },
    // index functions
    resetIndexForms: function () {
      $('#indexHeader input').val('').prop('checked', false);
      $('#newIndexType').val('Geo').prop('selected', true);
      this.selectIndexType();
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
        case 'Ttl':
          fields = $('#newTtlFields').val();
          var expireAfter = parseInt($('#newTtlExpireAfter').val(), 10) || 0;
          postParameter = {
            type: 'ttl',
            fields: self.stringToArray(fields),
            expireAfter: expireAfter
          };
          break;
        case 'Geo':
          // HANDLE ARRAY building
          fields = $('#newGeoFields').val();
          var geoJson = self.checkboxToValue('#newGeoJson');
          var constraint = self.checkboxToValue('#newGeoConstraint');
          var ignoreNull = self.checkboxToValue('#newGeoIgnoreNull');
          postParameter = {
            type: 'geo',
            fields: self.stringToArray(fields),
            geoJson: geoJson,
            constraint: constraint,
            ignoreNull: ignoreNull
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
        self.refreshCollectionsView();
      };

      window.modalView.hide();
      self.model.createIndex(postParameter, callback);
    },

    lastTarget: null,

    prepDeleteIndex: function (e) {
      var self = this;
      this.lastTarget = e;

      this.lastId = $(this.lastTarget.currentTarget).parent().parent().first().children().first().text();
      // window.modalView.hide()

      // delete modal
      $('#modal-dialog .modal-footer').after(
        '<div id="indexDeleteModal" style="display:block;" class="alert alert-error modal-delete-confirmation">' +
        '<strong>Really delete?</strong>' +
        '<button id="indexConfirmDelete" class="button-danger pull-right modal-confirm-delete">Yes</button>' +
        '<button id="indexAbortDelete" class="button-neutral pull-right">No</button>' +
        '</div>'
      );
      $('#indexConfirmDelete').unbind('click');
      $('#indexConfirmDelete').bind('click', function () {
        $('#indexDeleteModal').remove();
        self.deleteIndex();
      });

      $('#indexAbortDelete').unbind('click');
      $('#indexAbortDelete').bind('click', function () {
        $('#indexDeleteModal').remove();
      });
    },

    refreshCollectionsView: function () {
      window.App.arangoCollectionsStore.fetch({
        success: function () {
          window.App.collectionsView.render();
        }
      });
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
          this.refreshCollectionsView();
        } else if (!error && error !== undefined) {
          $("tr th:contains('" + this.lastId + "')").parent().remove();
          this.model.set('locked', false);
          this.refreshCollectionsView();
        }
        this.refreshCollectionsView();
      }.bind(this);

      this.model.set('locked', true);
      this.model.deleteIndex(this.lastId, callback);

      $("tr th:contains('" + this.lastId + "')").parent().children().last().html(
        '<i class="fa fa-circle-o-notch fa-spin"></i>'
      );
    },

    selectIndexType: function () {
      $('.newIndexClass').hide();
      var type = $('#newIndexType').val();
      $('#newIndexType' + type).show();
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

    toggleNewIndexView: function () {
      var elem = $('.index-button-bar2')[0];

      if ($('#indexEditView').is(':visible')) {
        $('#indexEditView').hide();
        $('#newIndexView').show();
        $('#cancelIndex').detach().appendTo('#modal-dialog .modal-footer');
        $('#createIndex').detach().appendTo('#modal-dialog .modal-footer');
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
