/* jshint browser: true */
/* global Backbone, $, window, Joi, _, arangoHelper */
/* global templateEngine */

(function () {
  'use strict';

  var createButtonStub = function (type, title, cb, confirm, style) {
    const button = {
      type: type,
      title: title,
      callback: cb,
      confirm: confirm
    };
    if (style) {
      button.style = style;
    }

    return button;
  };

  var createTextStub = function (type, label, value, info, placeholder, mandatory, joiObj,
    addDelete, addAdd, maxEntrySize, tags, style, cssClass, removeColon) {
    var obj = {
      type: type,
      label: label
    };
    if (style) {
      obj.style = style;
    }
    if (value) {
      obj.value = value;
    }
    if (info !== undefined) {
      obj.info = info;
    }
    if (placeholder !== undefined) {
      obj.placeholder = placeholder;
    }
    if (mandatory !== undefined) {
      obj.mandatory = mandatory;
    }
    if (addDelete !== undefined) {
      obj.addDelete = addDelete;
    }
    if (addAdd !== undefined) {
      obj.addAdd = addAdd;
    }
    if (maxEntrySize !== undefined) {
      obj.maxEntrySize = maxEntrySize;
    }
    if (tags !== undefined) {
      obj.tags = tags;
    }
    if (cssClass !== undefined) {
      obj.cssClass = cssClass;
    }
    if (removeColon) {
      obj.removeColon = true;
    }
    if (joiObj) {
      // returns true if the string contains the match
      obj.validateInput = function () {
        // return regexp.test(el.val())
        return joiObj;
      };
    }
    return obj;
  };

  window.ModalView = Backbone.View.extend({
    _validators: [],
    _validateWatchers: [],
    baseTemplate: templateEngine.createTemplate('modalBase.ejs'),
    tableTemplate: templateEngine.createTemplate('modalTable.ejs'),
    el: '#modalPlaceholder',
    contentEl: '#modalContent',
    hideFooter: false,
    confirm: {
      list: '#modal-delete-confirmation',
      yes: '#modal-confirm-delete',
      no: '#modal-abort-delete'
    },
    enabledHotkey: false,
    enableHotKeys: true,

    buttons: {
      SUCCESS: 'success',
      NOTIFICATION: 'notification',
      DELETE: 'danger',
      NEUTRAL: 'neutral',
      DISABLED: 'disabled',
      CLOSE: 'close'
    },
    tables: {
      READONLY: 'readonly',
      TEXT: 'text',
      BLOB: 'blob',
      PASSWORD: 'password',
      SELECT: 'select',
      SELECT2: 'select2',
      JSONEDITOR: 'jsoneditor',
      CHECKBOX: 'checkbox',
      TABLE: 'table',
      SPACER: 'spacer'
    },

    initialize: function () {
      Object.freeze(this.buttons);
      Object.freeze(this.tables);
    },

    createModalHotkeys: function () {
      // submit modal
      $(this.el).unbind('keydown');
      $(this.el).unbind('return');

      $('#modal-dialog .modal-body .collectionTh > input').unbind('keydown');
      $('#modal-dialog .modal-body .collectionTh > input').unbind('return');
      $('#modal-dialog .modal-body .collectionTh > input', $(this.el)).bind('keydown', 'return', function (e) {
        if (!$('#modal-dialog .modal-footer .button-success').is(':disabled') && e.keyCode === 13) {
          $('#modal-dialog .modal-footer .button-success').click();
        }
      });

      $('#modal-dialog .modal-body .collectionTh > select').unbind('keydown');
      $('#modal-dialog .modal-body .collectionTh > select').unbind('return');
      $('#modal-dialog .modal-body .collectionTh > select', $(this.el)).bind('keydown', 'return', function (e) {
        if (!$('#modal-dialog .modal-footer .button-success').is(':disabled') && e.keyCode === 13) {
          $('#modal-dialog .modal-footer .button-success').click();
        }
      });
    },

    createInitModalHotkeys: function () {
      var self = this;
      // navigate through modal buttons
      // left cursor
      $(this.el).bind('keydown', 'left', function () {
        self.navigateThroughButtons('left');
      });
      // right cursor
      $(this.el).bind('keydown', 'right', function () {
        self.navigateThroughButtons('right');
      });
    },

    navigateThroughButtons: function (direction) {
      var hasFocus = $('.createModalDialog .modal-footer button').is(':focus');
      if (hasFocus === false) {
        if (direction === 'left') {
          $('.createModalDialog .modal-footer button').first().focus();
        } else if (direction === 'right') {
          $('.createModalDialog .modal-footer button').last().focus();
        }
      } else if (hasFocus === true) {
        if (direction === 'left') {
          $(':focus').prev().focus();
        } else if (direction === 'right') {
          $(':focus').next().focus();
        }
      }
    },

    createCloseButton: function (title, cb) {
      var self = this;
      return createButtonStub(this.buttons.CLOSE, title, function () {
        self.hide();
        if (cb) {
          cb();
        }
      });
    },

    createSuccessButton: function (title, cb) {
      return createButtonStub(this.buttons.SUCCESS, title, cb);
    },

    createNotificationButton: function (title, cb) {
      return createButtonStub(this.buttons.NOTIFICATION, title, cb);
    },

    createDeleteButton: function (title, cb, confirm) {
      return createButtonStub(this.buttons.DELETE, title, cb, confirm);
    },

    createNeutralButton: function (title, cb) {
      return createButtonStub(this.buttons.NEUTRAL, title, cb);
    },

    createDisabledButton: function (title, style) {
      var disabledButton = createButtonStub(this.buttons.DISABLED, title, style);
      disabledButton.disabled = true;
      return disabledButton;
    },

    createReadOnlyEntry: function (id, label, value, info, addDelete, addAdd, style) {
      var obj = createTextStub(this.tables.READONLY, label, value, info, undefined, undefined,
        undefined, addDelete, addAdd, undefined, undefined, style);
      obj.id = id;
      return obj;
    },

    createTextEntry: function (id, label, value, info, placeholder, mandatory, regexp, style) {
      var obj = createTextStub(this.tables.TEXT, label, value, info, placeholder, mandatory,
        regexp, undefined, undefined, undefined, undefined, style);
      obj.id = id;
      return obj;
    },

    createBlobEntry: function (id, label, value, info, placeholder, mandatory, regexp, style) {
      var obj = createTextStub(this.tables.BLOB, label, value, info, placeholder, mandatory,
        regexp, undefined, undefined, undefined, undefined, style);
      obj.id = id;
      return obj;
    },

    createSpacerEntry: function (id, label) {
      var obj = createTextStub(this.tables.SPACER, label, undefined, undefined, undefined, undefined,
        undefined, undefined, undefined, undefined, undefined, undefined,
        'labeledSpacer', true);
      obj.id = id;
      return obj;
    },

    createSelect2Entry: function (
      id, label, value, info, placeholder, mandatory, addDelete, addAdd, maxEntrySize, tags, style, cssClass, isEnterpriseOnlyGraph) {
      var obj = createTextStub(this.tables.SELECT2, label, value, info, placeholder,
        mandatory, undefined, addDelete, addAdd, maxEntrySize, tags, style, cssClass);
      obj.id = id;
      if (isEnterpriseOnlyGraph) {
        obj.isEnterpriseOnlyGraph = true;
      }
      return obj;
    },

    createJsonEditor: function (
      id, label, value, info, placeholder, mandatory, addDelete, addAdd, maxEntrySize, tags, style) {
      var obj = createTextStub(this.tables.JSONEDITOR, 'Document body', value, '', placeholder,
        mandatory, undefined, addDelete, addAdd, maxEntrySize, tags, style);
      obj.id = id;
      return obj;
    },

    createPasswordEntry: function (id, label, value, info, placeholder, mandatory, regexp, style) {
      var obj = createTextStub(this.tables.PASSWORD, label, value, info, placeholder, mandatory,
        regexp, undefined, undefined, undefined, undefined, style);
      obj.id = id;
      return obj;
    },

    createCheckboxEntry: function (id, label, value, info, checked, style) {
      var obj = createTextStub(this.tables.CHECKBOX, label, value, info, undefined,
        undefined, undefined, undefined, undefined,
        undefined, undefined, style);
      obj.id = id;
      if (checked) {
        obj.checked = checked;
      }
      if (value) {
        obj.checked = value;
      }

      return obj;
    },

    createSelectEntry: function (id, label, selected, info, options, style) {
      var obj = createTextStub(this.tables.SELECT, label, null, info, undefined,
        undefined, undefined, undefined, undefined,
        undefined, undefined, style);
      obj.id = id;
      if (selected) {
        obj.selected = selected;
      }
      obj.options = options;
      return obj;
    },

    createOptionEntry: function (label, value, style) {
      return {
        label: label,
        value: value || label,
        style
      };
    },

    createTableEntry: function (id, label, head, rows, info, style) {
      return {
        id,
        label,
        info,
        type: this.tables.TABLE,
        head,
        rows,
        style: style + " ; display: table; "
      };
    },

    renameDangerButton: function (buttonID) {
      var buttonText = $(buttonID).text();
      $('#modal-delete-confirmation strong').html('Really ' + buttonText.toLowerCase() + '?');
    },

    handleSelect2Row: function (row) {
      // handle select2
      let options = {
        tags: row.tags || [],
        showSearchBox: false,
        minimumResultsForSearch: -1,
        width: row.width || '336px'
      };

      if (row.isEnterpriseOnlyGraph) {
        options.language = {};
        options.language.noMatches = function () {
          return "Please enter a new and valid collection name.";
        };
      } else {
        options.language = {};
        options.language.noMatches = function () {
          return "No collections found.";
        };
      }

      if (row.maxEntrySize) {
        options.maximumSelectionSize = row.maxEntrySize;
      }

      $('#' + row.id).select2(options);
    },

    show: function (templateName, title, buttons, tableContent, advancedContent,
      extraInfo, events, noConfirm, tabBar, divID) {
      var self = this;
      var lastBtn;
      var confirmMsg;
      var closeButtonFound = false;

      buttons = buttons || [];
      noConfirm = Boolean(noConfirm);
      this.clearValidators();
      if (buttons.length > 0) {
        buttons.forEach(function (b) {
          if (b.type === self.buttons.CLOSE) {
            closeButtonFound = true;
          }
          if (b.type === self.buttons.DELETE) {
            confirmMsg = confirmMsg || b.confirm;
          }
        });
        if (!closeButtonFound) {
          // Insert close as second from right
          lastBtn = buttons.pop();
          buttons.push(self.createCloseButton('Cancel'));
          buttons.push(lastBtn);
        }
      } else {
        buttons.push(self.createCloseButton('Close'));
      }
      if (!divID) {
        $(this.el).html(this.baseTemplate.render({
          title: title,
          buttons: buttons,
          hideFooter: this.hideFooter,
          confirm: confirmMsg,
          tabBar: tabBar
        }));
      } else {
        // render into custom div
        $('#' + divID).html(this.baseTemplate.render({
          title: title,
          buttons: buttons,
          hideFooter: this.hideFooter,
          confirm: confirmMsg,
          tabBar: tabBar
        }));
        // remove not needed modal elements
        $('#' + divID + ' #modal-dialog').removeClass('fade hide modal');
        $('#' + divID + ' .modal-header').remove();
        $('#' + divID + ' .modal-tabbar').remove();
        $('#' + divID + ' .modal-tabbar').remove();
        $('#' + divID + ' .button-close').remove();
        if ($('#' + divID + ' .modal-footer').children().length === 0) {
          $('#' + divID + ' .modal-footer').remove();
        }
      }
      _.each(buttons, function (b, i) {
        if (b.disabled || b.type === 'disabled' || !b.callback) {
          if (divID) {
            $('#' + divID + ' ' + '#modalButton' + i).attr('disabled', true);
          } else {
            $('#modalButton' + i).attr('disabled', true);
          }
          return;
        }
        if (b.type === self.buttons.DELETE && !noConfirm) {
          var string = '#modalButton' + i;
          if (divID) {
            string = '#' + divID + ' #modalButton' + i;
          }
          $(string).bind('click', function () {
            if (divID) {
              $('#' + divID + ' ' + self.confirm.yes).unbind('click');
              $('#' + divID + ' ' + self.confirm.yes).bind('click', b.callback);
              $('#' + divID + ' ' + self.confirm.list).css('display', 'block');
              self.renameDangerButton(string);
            } else {
              $(self.confirm.yes).unbind('click');
              $(self.confirm.yes).bind('click', b.callback);
              $(self.confirm.list).css('display', 'block');
              self.renameDangerButton(string);
            }
          });
          return;
        }
        if (divID) {
          $('#' + divID + ' ' + '#modalButton' + i).bind('click', b.callback);
        } else {
          $('#modalButton' + i).bind('click', b.callback);
        }
      });

      if (divID) {
        $('#' + divID + ' ' + this.confirm.no).bind('click', function () {
          $('#' + divID + ' ' + self.confirm.list).css('display', 'none');
        });
      } else {
        $(this.confirm.no).bind('click', function () {
          $(self.confirm.list).css('display', 'none');
        });
      }

      var template;
      if (typeof templateName === 'string') {
        template = templateEngine.createTemplate(templateName);
        if (divID) {
          $('#' + divID + ' .createModalDialog .modal-body').html(template.render({
            content: tableContent,
            advancedContent: advancedContent,
            info: extraInfo
          }));
        } else {
          $('#modalPlaceholder .createModalDialog .modal-body').html(template.render({
            content: tableContent,
            advancedContent: advancedContent,
            info: extraInfo
          }));
        }
      } else {
        var counter = 0;
        _.each(templateName, function (v) {
          template = templateEngine.createTemplate(v);
          $('.createModalDialog .modal-body .tab-content #' + tabBar[counter]).html(template.render({
            content: tableContent,
            advancedContent: advancedContent,
            info: extraInfo
          }));

          counter++;
        });
      }

      arangoHelper.createTooltips('.createModalDialog .modalTooltips', 'left');

      var completeTableContent = tableContent || [];
      if (advancedContent && advancedContent.content) {
        completeTableContent = completeTableContent.concat(advancedContent.content);
      } else if (Array.isArray(advancedContent)) {
        _.each(advancedContent, function(arrEntry){
          if (arrEntry.content) {
            completeTableContent = completeTableContent.concat(arrEntry.content);
          }
        });
      }

      _.each(completeTableContent, function (row) {
        self.modalBindValidation(row);
        if (row.type === self.tables.SELECT2) {
          self.handleSelect2Row(row);
        }

        if (row.type === self.tables.TABLE) {
          row.rows.forEach(row => {
            _.each(row, self.modalBindValidation, self);

            _.each(row, function(innerRow) {
              if (innerRow.type === self.tables.SELECT2) {
                innerRow.width = "resolve";
                self.handleSelect2Row(innerRow);
              }
            });
          });
        }
      });

      if (events) {
        this.events = events;
        this.delegateEvents();
      }

      let enableAccordion = function (counter) {
        const timeoutDuration = 100;
        if ($(`#accordion${counter}`)) {
          $(`#accordion${counter} .accordion-toggle`).bind('click', function () {
            if ($(`#collapseOne${counter}`).is(':visible')) {
              $(`#collapseOne${counter}`).hide();
              setTimeout(function () {
                $(`#accordion${counter} .accordion-toggle`).addClass('collapsed');
              }, timeoutDuration);
            } else {
              $(`#collapseOne${counter}`).show();
              setTimeout(function () {
                $(`#accordion${counter} .accordion-toggle`).removeClass('collapsed');
              }, timeoutDuration);
            }
          });
          $(`#collapseOne${counter}`).hide();
          setTimeout(function () {
            $(`#accordion${counter} .accordion-toggle`).addClass('collapsed');
          }, timeoutDuration);
        }
      };

      if (advancedContent && Array.isArray(advancedContent)) {
        let c = 2;
        _.each(advancedContent, function(entry) {
          enableAccordion(c);
          c++;
        });
      } else if (advancedContent) {
        enableAccordion(2);
      }

      if (!divID) {
        $('#modal-dialog').modal('show');
      }

      // enable modal hotkeys after rendering is complete
      if (this.enabledHotkey === false) {
        this.createInitModalHotkeys();
        this.enabledHotkey = true;
      }
      if (this.enableHotKeys) {
        this.createModalHotkeys();
      }

      // if input-field is available -> autofocus first one
      var focus;
      if (divID) {
        focus = $('#' + divID + ' ' + '#modal-dialog').find('input');
      } else {
        focus = $('#modal-dialog').find('input');
      }
      if (focus) {
        setTimeout(function () {
          if (divID) {
            focus = $('#' + divID + ' ' + '#modal-dialog');
          } else {
            focus = $('#modal-dialog');
          }
          if (focus.length > 0) {
            focus = focus.find('input');
            if (focus.length > 0) {
              $(focus[0]).focus();
            }
          }
        }, 400);
      }
    },

    modalBindValidation: function (entry) {
      var self = this;
      if (entry.hasOwnProperty('id') &&
        entry.hasOwnProperty('validateInput')) {
        var validCheck = function (e) {
          if (e) {
            var $el = $(e.target);
            var validation = entry.validateInput($el);
            var error = false;
            _.each(validation, function (validator) {
              var value = $el.val();
              if (!validator.rule) {
                validator = { rule: validator };
              }
              if (typeof validator.rule === 'function') {
                try {
                  validator.rule(value);
                } catch (e) {
                  error = validator.msg || e.message;
                }
              } else {
                var result = Joi.validate(value, validator.rule);
                if (result.error) {
                  error = validator.msg || result.error.message;
                }
              }
              if (error) {
                return false;
              }
            });
            if (error) {
              return error;
            }
          }
        };
        var $el = $('#' + entry.id);
        // catch result of validation and act
        $el.on('keyup focusout', function (e) {
          var $el = $(e.target);
          var msg = validCheck(e);
          var errorElement = $el.next()[0];
          if (msg) {
            $el.addClass('invalid-input');
            if (errorElement) {
              // error element available
              $(errorElement).text(msg);
            } else {
              // error element not available
              $el.after('<p class="errorMessage">' + msg + '</p>');
            }
            if ($('#modal-dialog').is(':visible')) {
              $('#modal-dialog .modal-footer .button-success')
                .prop('disabled', true)
                .addClass('disabled');
            } else {
              $('.createModalDialog .modal-footer .button-success')
                .prop('disabled', true)
                .addClass('disabled');
            }
          } else {
            $el.removeClass('invalid-input');
            if (errorElement) {
              $(errorElement).remove();
            }
            self.modalTestAll();
          }
        });
        this._validators.push(validCheck);
        this._validateWatchers.push($el);
      }
    },

    modalTestAll: function () {
      var tests = _.map(this._validators, function (v) {
        return v();
      });
      var invalid = _.any(tests);
      if (invalid) {
        if ($('#modal-dialog').is(':visible')) {
          $('#modal-dialog .modal-footer .button-success')
            .prop('disabled', true)
            .addClass('disabled');
        } else {
          $('.createModalDialog .modal-footer .button-success')
            .prop('disabled', true)
            .addClass('disabled');
        }
      } else {
        if ($('#modal-dialog').is(':visible')) {
          $('#modal-dialog .modal-footer .button-success')
            .prop('disabled', false)
            .removeClass('disabled');
        } else {
          $('.createModalDialog .modal-footer .button-success')
            .prop('disabled', false)
            .removeClass('disabled');
        }
      }
      return !invalid;
    },

    clearValidators: function () {
      this._validators = [];
      _.each(this._validateWatchers, function (w) {
        w.unbind('keyup focusout');
      });
      this._validateWatchers = [];
    },

    hide: function () {
      this.clearValidators();
      $('#modal-dialog').modal('hide');
    }

  });
}());
