/*jshint browser: true */
/*global Backbone, $, window, setTimeout, Joi, _ */
/*global templateEngine*/

(function () {
  "use strict";

  var createButtonStub = function(type, title, cb, confirm) {
    return {
      type: type,
      title: title,
      callback: cb,
      confirm: confirm
    };
  };

  var createTextStub = function(type, label, value, info, placeholder, mandatory, joiObj,
                                addDelete, addAdd, maxEntrySize, tags) {
    var obj = {
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
    if (joiObj){
      // returns true if the string contains the match
      obj.validateInput = function() {
        // return regexp.test(el.val());
        return joiObj;
      };
    }
    return obj;
  };

  window.ModalView = Backbone.View.extend({

    _validators: [],
    _validateWatchers: [],
    baseTemplate: templateEngine.createTemplate("modalBase.ejs"),
    tableTemplate: templateEngine.createTemplate("modalTable.ejs"),
    el: "#modalPlaceholder",
    contentEl: "#modalContent",
    hideFooter: false,
    confirm: {
      list: "#modal-delete-confirmation",
      yes: "#modal-confirm-delete",
      no: "#modal-abort-delete"
    },
    enabledHotkey: false,
    enableHotKeys : true,

    buttons: {
      SUCCESS: "success",
      NOTIFICATION: "notification",
      DELETE: "danger",
      NEUTRAL: "neutral",
      CLOSE: "close"
    },
    tables: {
      READONLY: "readonly",
      TEXT: "text",
      BLOB: "blob",
      PASSWORD: "password",
      SELECT: "select",
      SELECT2: "select2",
      CHECKBOX: "checkbox"
    },

    initialize: function() {
      Object.freeze(this.buttons);
      Object.freeze(this.tables);
    },

    createModalHotkeys: function() {
      //submit modal
      $(this.el).bind('keydown', 'return', function(){
        $('.createModalDialog .modal-footer .button-success').click();
      });
      $("input", $(this.el)).bind('keydown', 'return', function(){
        $('.createModalDialog .modal-footer .button-success').click();
      });
      $("select", $(this.el)).bind('keydown', 'return', function(){
        $('.createModalDialog .modal-footer .button-success').click();
      });
    },

    createInitModalHotkeys: function() {
      var self = this;
      //navigate through modal buttons
      //left cursor
      $(this.el).bind('keydown', 'left', function(){
        self.navigateThroughButtons('left');
      });
      //right cursor
      $(this.el).bind('keydown', 'right', function(){
        self.navigateThroughButtons('right');
      });

    },

    navigateThroughButtons: function(direction) {
      var hasFocus = $('.createModalDialog .modal-footer button').is(':focus');
      if (hasFocus === false) {
        if (direction === 'left') {
          $('.createModalDialog .modal-footer button').first().focus();
        }
        else if (direction === 'right') {
          $('..createModalDialog .modal-footer button').last().focus();
        }
      }
      else if (hasFocus === true) {
        if (direction === 'left') {
          $(':focus').prev().focus();
        }
        else if (direction === 'right') {
          $(':focus').next().focus();
        }
      }

    },

    createCloseButton: function(title, cb) {
      var self = this;
      return createButtonStub(this.buttons.CLOSE, title, function () {
        self.hide();
        if (cb) {
          cb();
        }
      });
    },

    createSuccessButton: function(title, cb) {
      return createButtonStub(this.buttons.SUCCESS, title, cb);
    },

    createNotificationButton: function(title, cb) {
      return createButtonStub(this.buttons.NOTIFICATION, title, cb);
    },

    createDeleteButton: function(title, cb, confirm) {
      return createButtonStub(this.buttons.DELETE, title, cb, confirm);
    },

    createNeutralButton: function(title, cb) {
      return createButtonStub(this.buttons.NEUTRAL, title, cb);
    },

    createDisabledButton: function(title) {
      var disabledButton = createButtonStub(this.buttons.NEUTRAL, title);
      disabledButton.disabled = true;
      return disabledButton;
    },

    createReadOnlyEntry: function(id, label, value, info, addDelete, addAdd) {
      var obj = createTextStub(this.tables.READONLY, label, value, info,undefined, undefined,
        undefined,addDelete, addAdd);
      obj.id = id;
      return obj;
    },

    createTextEntry: function(id, label, value, info, placeholder, mandatory, regexp) {
      var obj = createTextStub(this.tables.TEXT, label, value, info, placeholder, mandatory,
                               regexp);
      obj.id = id;
      return obj;
    },

    createBlobEntry: function(id, label, value, info, placeholder, mandatory, regexp) {
      var obj = createTextStub(this.tables.BLOB, label, value, info, placeholder, mandatory,
                               regexp);
      obj.id = id;
      return obj;
    },

    createSelect2Entry: function(
      id, label, value, info, placeholder, mandatory, addDelete, addAdd, maxEntrySize, tags) {
      var obj = createTextStub(this.tables.SELECT2, label, value, info, placeholder,
        mandatory, undefined, addDelete, addAdd, maxEntrySize, tags);
      obj.id = id;
      return obj;
    },

    createPasswordEntry: function(id, label, value, info, placeholder, mandatory, regexp) {
      var obj = createTextStub(this.tables.PASSWORD, label, value, info, placeholder, mandatory, regexp);
      obj.id = id;
      return obj;
    },

    createCheckboxEntry: function(id, label, value, info, checked) {
      var obj = createTextStub(this.tables.CHECKBOX, label, value, info);
      obj.id = id;
      if (checked) {
        obj.checked = checked;
      }
      return obj;
    },

    createSelectEntry: function(id, label, selected, info, options) {
      var obj = createTextStub(this.tables.SELECT, label, null, info);
      obj.id = id;
      if (selected) {
        obj.selected = selected;
      }
      obj.options = options;
      return obj;
    },

    createOptionEntry: function(label, value) {
      return {
        label: label,
        value: value || label
      };
    },

    show: function(templateName, title, buttons, tableContent, advancedContent, extraInfo, events, noConfirm) {
      var self = this, lastBtn, confirmMsg, closeButtonFound = false;
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
      $(this.el).html(this.baseTemplate.render({
        title: title,
        buttons: buttons,
        hideFooter: this.hideFooter,
        confirm: confirmMsg
      }));
      _.each(buttons, function(b, i) {
        if (b.disabled || !b.callback) {
          return;
        }
        if (b.type === self.buttons.DELETE && !noConfirm) {
          $("#modalButton" + i).bind("click", function() {
            $(self.confirm.yes).unbind("click");
            $(self.confirm.yes).bind("click", b.callback);
            $(self.confirm.list).css("display", "block");
          });
          return;
        }
        $("#modalButton" + i).bind("click", b.callback);
      });
      $(this.confirm.no).bind("click", function() {
        $(self.confirm.list).css("display", "none");
      });

      var template = templateEngine.createTemplate(templateName);
      $(".createModalDialog .modal-body").html(template.render({
        content: tableContent,
        advancedContent: advancedContent,
        info: extraInfo
      }));
      $('.createModalDialog .modalTooltips').tooltip({
        position: {
          my: "left top",
          at: "right+55 top-1"
        }
      });

      var completeTableContent = tableContent || [];
      if (advancedContent && advancedContent.content) {
        completeTableContent = completeTableContent.concat(advancedContent.content);
      }

      _.each(completeTableContent, function(row) {
        self.modalBindValidation(row);
        if (row.type === self.tables.SELECT2) {
          //handle select2
          $('#'+row.id).select2({
            tags: row.tags || [],
            showSearchBox: false,
            minimumResultsForSearch: -1,
            width: "336px",
            maximumSelectionSize: row.maxEntrySize || 8
          });
        }
      });

      if (events) {
        this.events = events;
        this.delegateEvents();
      }

      $("#modal-dialog").modal("show");

      //enable modal hotkeys after rendering is complete
      if (this.enabledHotkey === false) {
        this.createInitModalHotkeys();
        this.enabledHotkey = true;
      }
      if (this.enableHotKeys) {
        this.createModalHotkeys();
      }

      //if input-field is available -> autofocus first one
      var focus = $('#modal-dialog').find('input');
      if (focus) {
        setTimeout(function() {
          var focus = $('#modal-dialog');
          if (focus.length > 0) {
            focus = focus.find('input'); 
              if (focus.length > 0) {
                $(focus[0]).focus();
              }
          }
        }, 800);
      }

    },

    modalBindValidation: function(entry) {
      var self = this;
      if (entry.hasOwnProperty("id")
        && entry.hasOwnProperty("validateInput")) {
        var validCheck = function() {
          var $el = $("#" + entry.id);
          var validation = entry.validateInput($el);
          var error = false;
          _.each(validation, function(validator) {
            var value = $el.val();
            if (!validator.rule) {
              validator = {rule: validator};
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
        };
        var $el = $('#' + entry.id);
        // catch result of validation and act
        $el.on('keyup focusout', function() {
          var msg = validCheck();
          var errorElement = $el.next()[0];
          if (msg) {
            $el.addClass('invalid-input');
            if (errorElement) {
              //error element available
              $(errorElement).text(msg);
            }
            else {
              //error element not available
              $el.after('<p class="errorMessage">' + msg+ '</p>');
            }
            $('.createModalDialog .modal-footer .button-success')
              .prop('disabled', true)
              .addClass('disabled');
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

    modalTestAll: function() {
      var tests = _.map(this._validators, function(v) {
        return v();
      });
      var invalid = _.any(tests);
      if (invalid) {
        $('.createModalDialog .modal-footer .button-success')
          .prop('disabled', true)
          .addClass('disabled');
      } else {
        $('.createModalDialog .modal-footer .button-success')
          .prop('disabled', false)
          .removeClass('disabled');
      }
      return !invalid;
    },

    clearValidators: function() {
      this._validators = [];
      _.each(this._validateWatchers, function(w) {
        w.unbind('keyup focusout');
      });
      this._validateWatchers = [];
    },

    hide: function() {
      this.clearValidators();
      $("#modal-dialog").modal("hide");
    }
  });
}());
