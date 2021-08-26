/* jshint browser: true */
/* global Backbone, $, window, Joi, _, arangoHelper */
/* global templateEngine */

(function () {
  'use strict';

  const createButtonStub = function (type, title, cb, confirm) {
    return {
      type: type,
      title: title,
      callback: cb,
      confirm: confirm
    };
  };

  const createTextStub = function (type, label, value, info, placeholder, mandatory, joiObj,
    addDelete, addAdd, maxEntrySize, tags) {
    const obj = {
      type: type,
      label: label
    };
    if (value !== undefined) {
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
      TABLE: 'table'
    },

    initialize: function () {
      Object.freeze(this.buttons);
      Object.freeze(this.tables);
    },

    createModalHotkeys: function () {
      // submit modal
      $(this.el).off('keydown');
      $(this.el).off('return');

      let element = $('#modal-dialog .modal-body .collectionTh > input');
      const successButton = $('#modal-dialog .modal-footer .button-success');
      element.off('keydown');
      element.off('return');
      $('#modal-dialog .modal-body .collectionTh > input', $(this.el)).on('keydown', null, 'return',
        function (e) {
          if (!successButton.is(':disabled') && e.keyCode === 13) {
            $('#modal-dialog .modal-footer .button-success').trigger('click');
          }
        });

      element = $('#modal-dialog .modal-body .collectionTh > select');
      element.off('keydown');
      element.off('return');
      $('#modal-dialog .modal-body .collectionTh > select', $(this.el)).on('keydown', null,
        'return', function (e) {
          if (!successButton.is(':disabled') && e.keyCode === 13) {
            $('#modal-dialog .modal-footer .button-success').trigger('click');
          }
        });
    },

    createInitModalHotkeys: function () {
      const self = this;
      // navigate through modal buttons
      // left cursor
      $(this.el).on('keydown', null, 'left', function () {
        self.navigateThroughButtons('left');
      });
      // right cursor
      $(this.el).on('keydown', null, 'right', function () {
        self.navigateThroughButtons('right');
      });
    },

    navigateThroughButtons: function (direction) {
      const button = $('.createModalDialog .modal-footer button');
      const hasFocus = button.is(':focus');
      if (hasFocus === false) {
        if (direction === 'left') {
          button.first().trigger('focus');
        } else if (direction === 'right') {
          button.last().trigger('focus');
        }
      } else if (hasFocus === true) {
        if (direction === 'left') {
          $(':focus').prev().trigger('focus');
        } else if (direction === 'right') {
          $(':focus').next().trigger('focus');
        }
      }
    },

    createCloseButton: function (title, cb) {
      const self = this;
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

    createDisabledButton: function (title) {
      const disabledButton = createButtonStub(this.buttons.DISABLED, title);
      disabledButton.disabled = true;
      return disabledButton;
    },

    createReadOnlyEntry: function (id, label, value, info, addDelete, addAdd) {
      const obj = createTextStub(this.tables.READONLY, label, value, info, undefined, undefined,
        undefined, addDelete, addAdd);
      obj.id = id;
      return obj;
    },

    createTextEntry: function (id, label, value, info, placeholder, mandatory, regexp) {
      const obj = createTextStub(this.tables.TEXT, label, value, info, placeholder, mandatory,
        regexp);
      obj.id = id;
      return obj;
    },

    createBlobEntry: function (id, label, value, info, placeholder, mandatory, regexp) {
      const obj = createTextStub(this.tables.BLOB, label, value, info, placeholder, mandatory,
        regexp);
      obj.id = id;
      return obj;
    },

    createSelect2Entry: function (
      id, label, value, info, placeholder, mandatory, addDelete, addAdd, maxEntrySize, tags) {
      const obj = createTextStub(this.tables.SELECT2, label, value, info, placeholder,
        mandatory, undefined, addDelete, addAdd, maxEntrySize, tags);
      obj.id = id;
      return obj;
    },

    createJsonEditor: function (
      id, label, value, info, placeholder, mandatory, addDelete, addAdd, maxEntrySize, tags) {
      const obj = createTextStub(this.tables.JSONEDITOR, 'Document body', value, '', placeholder,
        mandatory, undefined, addDelete, addAdd, maxEntrySize, tags);
      obj.id = id;
      return obj;
    },

    createPasswordEntry: function (id, label, value, info, placeholder, mandatory, regexp) {
      const obj = createTextStub(this.tables.PASSWORD, label, value, info, placeholder, mandatory,
        regexp);
      obj.id = id;
      return obj;
    },

    createCheckboxEntry: function (id, label, value, info, checked) {
      const obj = createTextStub(this.tables.CHECKBOX, label, value, info);
      obj.id = id;
      if (checked) {
        obj.checked = checked;
      }
      if (value) {
        obj.checked = value;
      }

      return obj;
    },

    createSelectEntry: function (id, label, selected, info, options) {
      const obj = createTextStub(this.tables.SELECT, label, null, info);
      obj.id = id;
      if (selected) {
        obj.selected = selected;
      }
      obj.options = options;
      return obj;
    },

    createOptionEntry: function (label, value) {
      return {
        label: label,
        value: value || label
      };
    },

    createTableEntry: function (id, cols, options, head = (new Array(cols)).fill(''),
      rows = []) {
      return {
        type: this.tables.TABLE,
        cols,
        head,
        rows,
        options
      };
    },

    renameDangerButton: function (buttonID) {
      const buttonText = $(buttonID).text();
      $('#modal-delete-confirmation strong').html('Really ' + buttonText.toLowerCase() + '?');
    },

    show: function (templateName, title, buttons, tableContent, advancedContent,
      extraInfo, events, noConfirm, tabBar, divID) {
      let element;
      const self = this;
      let lastBtn;
      let confirmMsg;
      let closeButtonFound = false;

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

        element = $('#' + divID + ' .modal-tabbar');
        element.remove();
        element.remove();
        $('#' + divID + ' .button-close').remove();

        element = $('#' + divID + ' .modal-footer');
        if (element.children().length === 0) {
          element.remove();
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
          let string = '#modalButton' + i;
          if (divID) {
            string = '#' + divID + ' #modalButton' + i;
          }
          $(string).on('click', function () {
            if (divID) {
              const element = $('#' + divID + ' ' + self.confirm.yes);
              element.off('click');
              element.on('click', b.callback);
              $('#' + divID + ' ' + self.confirm.list).css('display', 'block');
              self.renameDangerButton(string);
            } else {
              $(self.confirm.yes).off('click');
              $(self.confirm.yes).on('click', b.callback);
              $(self.confirm.list).css('display', 'block');
              self.renameDangerButton(string);
            }
          });
          return;
        }
        if (divID) {
          $('#' + divID + ' ' + '#modalButton' + i).on('click', b.callback);
        } else {
          $('#modalButton' + i).on('click', b.callback);
        }
      });

      if (divID) {
        $('#' + divID + ' ' + this.confirm.no).on('click', function () {
          $('#' + divID + ' ' + self.confirm.list).css('display', 'none');
        });
      } else {
        $(this.confirm.no).on('click', function () {
          $(self.confirm.list).css('display', 'none');
        });
      }

      let template;
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
        let counter = 0;
        _.each(templateName, function (v) {
          template = templateEngine.createTemplate(v);
          $('.createModalDialog .modal-body .tab-content #' + tabBar[counter]).html(
            template.render({
              content: tableContent,
              advancedContent: advancedContent,
              info: extraInfo
            }));

          counter++;
        });
      }

      arangoHelper.createTooltips('.createModalDialog .modalTooltips', 'left');

      let completeTableContent = tableContent || [];
      if (advancedContent && advancedContent.content) {
        completeTableContent = completeTableContent.concat(advancedContent.content);
      }

      _.each(completeTableContent, function (row) {
        self.modalBindValidation(row);
        if (row.type === self.tables.SELECT2) {
          // handle select2

          const options = {
            tags: row.tags || [],
            showSearchBox: false,
            minimumResultsForSearch: -1,
            width: '336px'
          };

          if (row.maxEntrySize) {
            options.maximumSelectionSize = row.maxEntrySize;
          }

          $('#' + row.id).select2(options);
        }
      });

      if (events) {
        this.events = events;
        this.delegateEvents();
      }

      if ($('#accordion2')) {
        const element = $('#collapseOne');
        $('#accordion2 .accordion-toggle').on('click', function () {
          if (element.is(':visible')) {
            element.hide();
            setTimeout(function () {
              $('.accordion-toggle').addClass('collapsed');
            }, 100);
          } else {
            element.show();
            setTimeout(function () {
              $('.accordion-toggle').removeClass('collapsed');
            }, 100);
          }
        });
        element.hide();
        setTimeout(function () {
          $('.accordion-toggle').addClass('collapsed');
        }, 100);
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
      let focus;
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
              $(focus[0]).trigger('focus');
            }
          }
        }, 400);
      }
    },

    modalBindValidation: function (entry) {
      const self = this;
      if (entry.hasOwnProperty('id') &&
        entry.hasOwnProperty('validateInput')) {
        const validCheck = function () {
          const $el = $('#' + entry.id);
          const validation = entry.validateInput($el);
          let error = false;
          _.each(validation, function (validator) {
            const value = $el.val();
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
              const result = Joi.validate(value, validator.rule);
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
        };
        const $el = $('#' + entry.id);
        // catch result of validation and act
        $el.on('keyup focusout', function () {
          const msg = validCheck();
          const errorElement = $el.next()[0];
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
              $('#modal-dialog .modal-footer .button-success').
                prop('disabled', true).
                addClass('disabled');
            } else {
              $('.createModalDialog .modal-footer .button-success').
                prop('disabled', true).
                addClass('disabled');
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
      const tests = _.map(this._validators, function (v) {
        return v();
      });
      const invalid = _.any(tests);
      if (invalid) {
        if ($('#modal-dialog').is(':visible')) {
          $('#modal-dialog .modal-footer .button-success').
            prop('disabled', true).
            addClass('disabled');
        } else {
          $('.createModalDialog .modal-footer .button-success').
            prop('disabled', true).
            addClass('disabled');
        }
      } else {
        if ($('#modal-dialog').is(':visible')) {
          $('#modal-dialog .modal-footer .button-success').
            prop('disabled', false).
            removeClass('disabled');
        } else {
          $('.createModalDialog .modal-footer .button-success').
            prop('disabled', false).
            removeClass('disabled');
        }
      }
      return !invalid;
    },

    clearValidators: function () {
      this._validators = [];
      _.each(this._validateWatchers, function (w) {
        w.off('keyup focusout');
      });
      this._validateWatchers = [];
    },

    hide: function () {
      this.clearValidators();
      $('#modal-dialog').modal('hide');
    }

  });
}());
