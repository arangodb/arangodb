/*jshint browser: true */
/*jshint unused: false */
/*global Backbone, window, templateEngine, $ */

(function() {
  "use strict";

  window.TableView = Backbone.View.extend({

    template: templateEngine.createTemplate("tableView.ejs"),
    loading: templateEngine.createTemplate("loadingTableView.ejs"),

    initialize: function() {
      this.rowClickCallback = this.options.rowClick;
    },

    events: {
      "click tbody tr": "rowClick",
      "click .deleteButton": "removeClick",
    },

    rowClick: function(event) {
      if (this.hasOwnProperty("rowClickCallback")) {
        this.rowClickCallback(event); 
      }
    },

    removeClick: function(event) {
      if (this.hasOwnProperty("removeClickCallback")) {
        this.removeClickCallback(event); 
        event.stopPropagation();
      }
    },

    setRowClick: function(callback) {
      this.rowClickCallback = callback;
    },

    setRemoveClick: function(callback) {
      this.removeClickCallback = callback;
    },

    render: function() {
      $(this.el).html(this.template.render({
        docs: this.collection
      }));
    },

    drawLoading: function() {
      $(this.el).html(this.loading.render({}));
    }

  });

}());
