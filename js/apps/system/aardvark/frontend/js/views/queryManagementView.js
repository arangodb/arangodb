/*jshint browser: true */
/*jshint unused: false */
/*global require, exports, Backbone, EJS, $, setTimeout, localStorage, ace, Storage, window, _ */
/*global _, arangoHelper, templateEngine, jQuery, Joi*/

(function () {
  "use strict";
  window.queryManagementView = Backbone.View.extend({
    el: '#content',

    id: '#queryManagementContent',

    templateActive: templateEngine.createTemplate("queryManagementViewActive.ejs"),
    templateSlow: templateEngine.createTemplate("queryManagementViewSlow.ejs"),
    table: templateEngine.createTemplate("arangoTable.ejs"),
    tabbar: templateEngine.createTemplate("arangoTabbar.ejs"),

    initialize: function () {
      this.activeCollection = new window.QueryManagementActive();
      this.slowCollection = new window.QueryManagementSlow();
      this.convertModelToJSON(true);
    },

    events: {
      "click #arangoQueryManagementTabbar button" : "switchTab",
      "click #deleteSlowQueryHistory" : "deleteSlowQueryHistoryModal"
    },

    tabbarElements: {
      id: "arangoQueryManagementTabbar",
      titles: [
        ["Active Queries", "activequeries"],
        ["Slow Queries", "slowqueries"]
      ]
    },

    tableDescription: {
      id: "arangoQueryManagementTable",
      titles: ["ID", "Content", "Started", "Runtime"],
      rows: []
    },

    switchTab: function(e) {
      if (e.currentTarget.id === 'activequeries') {
        this.convertModelToJSON(true);
      }
      else if (e.currentTarget.id === 'slowqueries') {
        this.convertModelToJSON(false);
      }
    },

    deleteSlowQueryHistoryModal: function() {
      var buttons = [], tableContent = [];

      tableContent.push(
        window.modalView.createReadOnlyEntry(
          undefined,
          "Slow Query Log",
          'Do you want to delete the slow query log entries?',
          undefined,
          undefined,
          false,
          undefined
        )
      );

      buttons.push(
        window.modalView.createDeleteButton('Delete', this.deleteSlowQueryHistory.bind(this))
      );

      window.modalView.show(
        'modalTable.ejs',
        'Delete Slow Query Log',
        buttons,
        tableContent
      );
    },

    deleteSlowQueryHistory: function() {
      this.collection.deleteSlowQueryHistory(this.slowQueryCallback.bind(this));
      window.modalView.hide();
    },

    slowQueryCallback: function() {
      this.convertModelToJSON(false);
      this.renderSlow();
    },

    render: function() {
      this.renderActive();
    },

    renderActive: function() {
      this.$el.html(this.templateActive.render({}));
      $(this.id).html(this.tabbar.render({content: this.tabbarElements}));
      $(this.id).append(this.table.render({content: this.tableDescription}));
      $('#activequeries').addClass("arango-active-tab");
    },

    renderSlow: function() {
      this.$el.html(this.templateSlow.render({}));
      $(this.id).html(this.tabbar.render({content: this.tabbarElements}));
      $(this.id).append(this.table.render({content: this.tableDescription}));
      $('#slowqueries').addClass("arango-active-tab");
    },

    convertModelToJSON: function (active) {
      var self = this;
      var rowsArray = [];

      if (active === true) {
        this.collection = this.activeCollection;
      }
      else {
        this.collection = this.slowCollection;
      }

      this.collection.fetch({
        success: function () {
          self.collection.each(function (model) {
            rowsArray.push([
              model.get('id'),
              model.get('query'),
              model.get('runTime'),
              model.get('started')
            ]);
          });

          var message = "No running queries.";
          if (!active) {
            message = "No slow queries.";
          }

          if (rowsArray.length === 0) {
            rowsArray.push([
              message,
              "",
              "",
              ""
            ]);
          }

          self.tableDescription.rows = rowsArray;

          if (active) {
            self.renderActive();
          }
          else {
            self.renderSlow();
          }
        }
      });

    }

  });
}());
