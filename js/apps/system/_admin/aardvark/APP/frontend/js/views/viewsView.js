/* jshint browser: true */
/* jshint unused: false */
/* global $, Joi, frontendConfig, arangoHelper, _, Backbone, templateEngine, window */
(function () {
  'use strict';

  window.ViewsView = Backbone.View.extend({
    el: '#content',
    readOnly: false,

    template: templateEngine.createTemplate('viewsView.ejs'),

    initialize: function () {
    },

    refreshRate: 10000,

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
      'click .tile-view': 'gotoView',
      'keyup #viewsSearchInput': 'search',
      'click #viewsSearchSubmit': 'search',
      'click #viewsSortDesc': 'sorting'
    },

    checkVisibility: function () {
      if ($('#viewsDropdown').is(':visible')) {
        this.dropdownVisible = true;
      } else {
        this.dropdownVisible = false;
      }
      arangoHelper.setCheckboxStatus('#viewsDropdown');
    },

    checkIfInProgress: function () {
      if (window.location.hash.search('views') > -1) {
        var self = this;

        var callback = function (error, lockedViews) {
          if (error) {
            console.log('Could not check locked views');
          } else {
            if (lockedViews.length > 0) {
              _.each(lockedViews, function (foundView) {
                if ($('#' + foundView.collection)) {
                  // found view html container
                  $('#' + foundView.collection + ' .collection-type-icon').removeClass('fa-clone');
                  $('#' + foundView.collection + ' .collection-type-icon').addClass('fa-spinner').addClass('fa-spin');
                } else {
                  $('#' + foundView.collection + ' .collection-type-icon').addClass('fa-clone');
                  $('#' + foundView.collection + ' .collection-type-icon').removeClass('fa-spinner').removeClass('fa-spin');
                }
              });
            } else {
              // if no view found at all, just reset all to default
              $('.tile .collection-type-icon').addClass('fa-clone').removeClass('fa-spinner').removeClass('fa-spin');
            }

            window.setTimeout(function () {
              self.checkIfInProgress();
            }, self.refreshRate);
          }
        };

        if (!frontendConfig.ldapEnabled) {
          window.arangoHelper.syncAndReturnUnfinishedAardvarkJobs('view', callback);
        }
      }
    },
    sorting: function () {
      if ($('#viewsSortDesc').is(':checked')) {
        this.setSortingDesc(true);
      } else {
        this.setSortingDesc(false);
      }

      this.checkVisibility();
      this.render();
    },

    setSortingDesc: function (yesno) {
      this.sortOptions.desc = yesno;
    },

    search: function () {
      this.setSearchString(arangoHelper.escapeHtml($('#viewsSearchInput').val()));
      this.render();
    },

    toggleSettingsDropdown: function () {
      var self = this;
      // apply sorting to checkboxes
      $('#viewsSortDesc').attr('checked', this.sortOptions.desc);

      $('#viewsToggle').toggleClass('activated');
      $('#viewsDropdown2').slideToggle(200, function () {
        self.checkVisibility();
      });
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
        $('#viewsToggle').addClass('activated');
        $('#viewsDropdown2').show();
      }

      $('#viewsSortDesc').attr('checked', self.sortOptions.desc);
      arangoHelper.setCheckboxStatus('#viewsDropdown');

      var searchInput = $('#viewsSearchInput');
      var strLength = searchInput.val().length;
      searchInput.focus();
      searchInput[0].setSelectionRange(strLength, strLength);

      arangoHelper.checkDatabasePermissions(this.setReadOnly.bind(this));

      self.events['click .graphViewer-icon-button.add'] = self.addRow.bind(self);
      self.events['click .graphViewer-icon-button.delete'] = self.removeRow.bind(self);
    },

    setReadOnly: function () {
      this.readOnly = true;
      $('#createView').parent().parent().addClass('disabled');
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
        var url = 'view/' + encodeURIComponent(name);
        window.App.navigate(url, {trigger: true});
      }
    },

    getViews: function () {
      var self = this;

      this.collection.fetch({
        success: function (data) {
          var res = {
            result: []
          };
          self.collection.each(function (view) {
            res.result.push(view.toJSON());
          });
          self.render(res);
          self.checkIfInProgress();
        },
        error: function (error) {
          console.log(error);
        }
      });
    },

    createView: function (e) {
      if (!this.readOnly) {
        e.preventDefault();
        this.createViewModal();
      }
    },

    createViewModal: function () {
      var buttons = [];
      var tableContent = [];
      var advanced = [];
      let primarySortTableContent = [];
      let storedValuesTableContent = [];
      let advancedTableContent = [];

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

      tableContent.push(
        window.modalView.createSelectEntry(
          'newPrimarySortCompression',
          'Primary Sort Compression',
          'lz4',
          false,
          [
            window.modalView.createOptionEntry('LZ4', 'lz4'),
            window.modalView.createOptionEntry('None', 'none')
          ],
          'width: unset;'
        )
      );

      primarySortTableContent.push(
        window.modalView.createTableEntry(
          'newPrimarySort',
          {
            noLabel: true // Dummy object. Entry will only be created if this is set as a String.
          },
          ['Field', 'Direction'],
          [[
            window.modalView.createTextEntry(
              undefined,
              undefined,
              '',
              false,
              undefined,
              true,
              [
                {
                  rule: Joi.string().required(),
                  msg: 'No field name given.'
                }
              ]
            ),
            window.modalView.createSelectEntry(
              _.uniqueId('direction-'),
              undefined,
              'asc',
              false,
              [
                window.modalView.createOptionEntry('ASC', 'asc'),
                window.modalView.createOptionEntry('DESC', 'desc')
              ]
            )
          ]],
          undefined,
          'padding-left: 0;'
        )
      );

      storedValuesTableContent.push(
        window.modalView.createTableEntry(
          'newStoredValues',
          {
            noLabel: true // Dummy object. Entry will only be created if this is set as a String.
          },
          ['Fields', 'Compression'],
          [[
            window.modalView.createSelect2Entry(
              _.uniqueId('field-'),
              null,
              null,
              null,
              null,
              true,
              false,
              false,
              null
            ),
            window.modalView.createSelectEntry(
              _.uniqueId('compression-'),
              undefined,
              'lz4',
              false,
              [
                window.modalView.createOptionEntry('LZ4', 'lz4'),
                window.modalView.createOptionEntry('None', 'none')
              ]
            )
          ]],
          undefined,
          'margin-top: 10px; padding-left: 0;'
        )
      );

      advancedTableContent.push(
        window.modalView.createTextEntry(
          'newWriteBufferIdle',
          'Write Buffer Idle',
          64,
          false,
          undefined,
          false,
          [
            {
              rule: Joi.number().integer().min(0),
              msg: 'Only non-negative integers allowed.'
            }
          ]
        )
      );

      advancedTableContent.push(
        window.modalView.createTextEntry(
          'newWriteBufferActive',
          'Write Buffer Active',
          '0',
          false,
          undefined,
          false,
          [
            {
              rule: Joi.number().integer().min(0),
              msg: 'Only non-negative integers allowed.'
            }
          ]
        )
      );

      advancedTableContent.push(
        window.modalView.createTextEntry(
          'newWriteBufferSizeMax',
          'Write Buffer Size Max',
          33554432,
          false,
          undefined,
          false,
          [
            {
              rule: Joi.number().integer().min(0),
              msg: 'Only non-negative integers allowed.'
            }
          ]
        )
      );

      advanced.push({
        header: 'Primary Sort',
        content: primarySortTableContent
      });

      advanced.push({
        header: 'Stored Values',
        content: storedValuesTableContent
      });

      advanced.push({
        header: 'Advanced',
        content: advancedTableContent
      });

      buttons.push(
        window.modalView.createSuccessButton('Create', this.submitCreateView.bind(this))
      );

      window.modalView.show('modalTable.ejs', 'Create New View', buttons, tableContent, advanced,
        undefined, this.events);

      // select2 workaround
      $('.select2-search-field input').on('focusout', function (e) {
        if ($('.select2-drop').is(':visible')) {
          if (!$('#select2-search-field input').is(':focus')) {
            window.setTimeout(function () {
              $(e.currentTarget).parent().parent().parent().select2('close');
            }, 200);
          }
        }
      });
    },

    addRow: function (e) {
      e.stopPropagation();

      const row = $(e.currentTarget).closest('table').find('tbody').children().last();
      let foundSelect2 = false;
      if (row.find('.select2-container').length > 0) {
        foundSelect2 = true;
      }

      let newRow;
      if (!foundSelect2) {
        newRow = row.clone(true);
      } else {
        newRow = row.clone(false);

        let firstCell = newRow.find('td:first-child');
        firstCell.html('<div></div>');
      }

      const idParts = newRow.attr('id').split('-');

      idParts[idParts.length - 1] = parseInt(idParts[idParts.length - 1]) + 1;
      const newId = idParts.join('-');

      newRow.attr('id', newId);

      const inputs = newRow.find('input,textarea');
      inputs.val('');
      inputs.attr('id', (idx, id) => id ? `${id.split('-')[0]}-${_.uniqueId()}` : id);

      if (!newRow.find('button.delete').length) {
        const lastCell = newRow.children().last().find('span');
        $(`
            <button style="margin-left: 5px;"
                    class="graphViewer-icon-button gv_internal_remove_line gv-icon-small delete addDelete">
            </button>
        `).appendTo(lastCell);
      }

      newRow.insertAfter(row);
    },

    removeRow: function (e) {
      e.stopPropagation();
      $(e.currentTarget).closest('tr').remove();
    },

    submitCreateView: function () {
      var self = this;
      var name = $('#newName').val();
      var primarySortCompression = $('#newPrimarySortCompression').val();
      var writebufferIdle = parseInt($('#newWriteBufferIdle').val());
      var writebufferActive = parseInt($('#newWriteBufferActive').val());
      var writebufferSizeMax = parseInt($('#newWriteBufferSizeMax').val());

      const primarySort = [];
      $('#newPrimarySort tbody').children().each((idx, tr) => {
        const inputs = $(tr).find('input,select');
        const [field, direction] = inputs.map((idx, input) => input.value);

        if (field) {
          primarySort.push({
            field,
            direction: direction || 'asc'
          });
        }
      });

      const storedValues = [];
      $('#newStoredValues tbody').children().each((idx, tr) => {
        const inputs = $(tr).find('textarea,select');
        const [fields, compression] = inputs.map((idx, input) => input.value);

        if (fields) {
          const fieldsArr = fields.trim().split('\n');

          storedValues.push({
            fields: fieldsArr,
            compression: compression || 'lz4'
          });
        }
      });

      var options = JSON.stringify({
        name: name,
        type: 'arangosearch',
        primarySortCompression,
        writebufferIdle,
        writebufferActive,
        writebufferSizeMax,
        primarySort,
        storedValues
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
          arangoHelper.arangoNotification('View', 'Creation in progress. This may take a while.');
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
