var app = app || {};

$(function () {
	'use strict';

	app.EditSuperView = Backbone.View.extend({

    template: new EJS({url: 'templates/editView.ejs'}),
    
    el: "body",

		events: {
      "click #store": "submit",
      "keypress": "editOnEnter",
      "hidden": "hidden"
		},

		initialize: function () {
      this.columns = this.options.columns;
      this.validationObj = {};
      var rules = {},
        messages = {},
        self = this,
        highlight = function(element) {
          $(element).closest('.control-group').removeClass('success').addClass('error');
        },
        success = function(element) {
          element
          .text('OK!').addClass('valid')
          .closest('.control-group').removeClass('error').addClass('success');
        },
        submitHandler = function() {
          self.save();
        };
      _.each(this.columns, function(c) {
        var rule = {},
          msg = {};
        rule.required = true;
        msg.required = "Enter a " + c.name;
        switch (c.type) {
          case "string":
            rule.minlength = 1;
            msg.minlength = $.format("Enter at least {0} characters");
            break;
          case "boolean":
            delete rule.required;
            delete msg.required;
            break;
          case "number":
            rule.number = true;
            msg.number = "Please enter a number";
            break;
          case "email":
            rule.email = true;
            msg.email = "Enter a valid email";
            break;
          case "url":
            rule.url = true;
            msg.url = "Enter a valid url";
            break;
          default:
            rule.minlength = 1;
            msg.minlength = $.format("Enter at least {0} characters");
        }
        
        rules[c.name] = rule;
        messages[c.name] = msg;
      });
      this.validationObj.rules = rules;
      this.validationObj.highlight = highlight;
      this.validationObj.success = success;
      this.validationObj.messages = messages;
      this.validationObj.submitHandler = submitHandler;
      
		},
    
    submit: function(e) {
      $('#document_form').submit();
      e.stopPropagation();
    },
    
    editOnEnter: function(e) {
      if (e.keyCode === 13) {
        this.submit();
      }
    },
    
    hidden: function () {
      $('#document_modal').remove();
    },
    
    parse: function() {
      var obj = {};
      _.each(this.columns, function(c) {
        var name = c.name,
          val = $("#" + name).val();
        switch(c.type) {
          case "string":
          case "url":
          case "email":
            obj[name] = val;
            break;
          case "number":
            obj[name] = parseFloat(val);
            break;
          case "boolean":
            obj[name] = $("#" + name).prop("checked");
            break;
          default:
            obj[name] = val;
        }
      });
      return obj;
    },
    
    
    activateValidation: function () {
      $('#document_form').validate(this.validationObj);
    },
    
    indicateError: function(field, msg) {
      field.attr("style", "background-color:rgba(255, 0, 0, 0.5)");
      alert(msg);
    },
    
    
		superRender: function (data) {
      $(this.el).off();
      $(this.el).append(this.template.render(data));
      this.delegateEvents();
      this.activateValidation();
      $('#document_modal').modal("show");
      
		}
	});
}());
