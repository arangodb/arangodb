/*jslint indent: 2, nomen: true, maxlen: 100, vars: true, white: true, plusplus: true */
/*global Backbone, $, window, _ */
/*global templateEngine*/

(function () {
  "use strict";

  window.ModalView = Backbone.View.extend({

    baseTemplate: templateEngine.createTemplate("modalBase.ejs"),
    tableTemplate: templateEngine.createTemplate("modalTable.ejs"),
    el: "#modalPlaceholder",
    contentEl: "#modalContent",

    buttons: {
      SUCCESS: "success",
      NOTIFICATION: "notification",
      DELETE: "danger",
      NEUTRAL: "neutral"
    },
    tables: {
      READONLY: "readonly",
      TEXT: "text",
      PASSWORD: "password",
      SELECT: "select",
      CHECKBOX: "checkbox"
    },
    closeButton: {
      type: "close",
      title: "Cancel"
    },

    initialize: function() {
      Object.freeze(this.buttons);
      Object.freeze(this.tables);
      var self = this;
      this.closeButton.callback = function() {
        self.hide(); 
      };
      //this.hide.bind(this);
    },


    show: function(templateName, title, buttons, tableContent) {
      buttons = buttons || [];
      buttons.push(this.closeButton);
      $(this.el).html(this.baseTemplate.render({
        title: title,
        buttons: buttons
      }));
      _.each(buttons, function(b, i) {
        if (b.disabled || !b.callback) {
          return;
        }
        $("#modalButton" + i).bind("click", b.callback);
      });

      var template = templateEngine.createTemplate(templateName),
        model = {};
      model.content = tableContent || [];
      $(".modal-body").html(template.render(model));
    },

    hide: function() {

    }
  });
}());
