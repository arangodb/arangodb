/*jslint indent: 2, nomen: true, maxlen: 100, vars: true, white: true, plusplus: true */
/*global Backbone, $, window, EJS, arangoHelper, _, templateEngine*/

(function() {
  "use strict";

  window.FoxxActiveView = Backbone.View.extend({
    tagName: 'div',
    className: "tile",
    template: templateEngine.createTemplate("foxxActiveView.ejs"),

    events: {
      'click .icon_arangodb_info' : 'showDocu',
      'click'                      : 'editFoxx'
    },

    initialize: function(){
      this._show = true;
      _.bindAll(this, 'render');
    },

    toggle: function(type, shouldShow) {
      if (this.model.get("development")) {
        if ("devel" === type) {
          this._show = shouldShow;
        }
      } else {
        if ("active" === type && true === this.model.get("active")) {
          this._show = shouldShow;
        }
        if ("inactive" === type && false === this.model.get("active")) {
          this._show = shouldShow;
        }
      }
      if (this._show) {
        $(this.el).show();
      } else {
        $(this.el).hide();
      }
    },

    editFoxx: function(event) {
      event.stopPropagation();
      window.App.navigate(
        "application/installed/" + encodeURIComponent(this.model.get("_key")),
        {
          trigger: true
        }
      );
    },

    showDocu: function(event) {
      event.stopPropagation();
      window.App.navigate(
        "application/documentation/" + encodeURIComponent(this.model.get("_key")),
        {
          trigger: true
        }
      );
    },

    render: function(){
      if (this._show) {
        $(this.el).html(this.template.render(this.model));
      }

      return $(this.el);
    }
  });
}());
