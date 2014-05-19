/*jslint indent: 2, nomen: true, maxlen: 100, vars: true, white: true, plusplus: true */
/*global Backbone, arangoHelper, $, window, templateEngine*/

(function () {
  "use strict";

  window.LogsView = window.PaginationView.extend({

    el: '#content',
    id: '#logContent',
    paginationDiv: "#logPaginationDiv",
    idPrefix: "logTable",
    fetchedAmount: false,

    initialize: function () {
      this.convertModelToJSON();
    },

    currentLoglevel: "logall",

    events: {
      "click #arangoLogTabbar button": "setActiveLoglevel",
      "click #logTable_first": "firstPage",
      "click #logTable_last": "lastPage"
    },

    template: templateEngine.createTemplate("logsView.ejs"),
    tabbar: templateEngine.createTemplate("arangoTabbar.ejs"),
    table: templateEngine.createTemplate("arangoTable.ejs"),

    tabbarElements: {
      id: "arangoLogTabbar",
      titles: [
        ["Debug", "logdebug"],
        ["Warning", "logwarning"],
        ["Error", "logerror"],
        ["Info", "loginfo"],
        ["All", "logall"]
      ]
    },

    tableDescription: {
      id: "arangoLogTable",
      titles: ["Loglevel", "Date", "Message"],
      rows: []
    },

    convertedRows: null,

    setActiveLoglevel: function (e) {
      $('.arangodb-tabbar').removeClass('arango-active-tab');

      if (this.currentLoglevel !== e.currentTarget.id) {
        this.currentLoglevel = e.currentTarget.id;
        this.convertModelToJSON();
      }
    },

    initTotalAmount: function() {
      var self = this;
      this.collection = this.options[this.currentLoglevel];
      this.collection.fetch({
        data: $.param({ test: true}),
        success: function () {
          self.convertModelToJSON();
        }
      });
      this.fetchedAmount = true;
    },

    invertArray: function (array) {
      var rtnArr = [], counter = 0, i;
      for (i = array.length-1; i >= 0; i--) {
        rtnArr[counter] = array[i];
        counter++;
      }
      return rtnArr;
    },

    convertModelToJSON: function () {
      if (!this.fetchedAmount) {
        this.initTotalAmount();
        return;
      }

      var self = this;
      var date;
      var rowsArray = [];
      this.collection = this.options[this.currentLoglevel];
      this.collection.fetch({
        success: function () {
          self.collection.each(function (model) {
            date = new Date(model.get('timestamp') * 1000);
            rowsArray.push([
              model.getLogStatus(),
              arangoHelper.formatDT(date),
              model.get('text')]);
          });
          self.tableDescription.rows = self.invertArray(rowsArray);
          //invert order
          self.render();
        }
      });
    },

    render: function () {
      $(this.el).html(this.template.render({}));
      $(this.id).html(this.tabbar.render({content: this.tabbarElements}));
      $(this.id).append(this.table.render({content: this.tableDescription}));
      $('#' + this.currentLoglevel).addClass('arango-active-tab');
      this.renderPagination();
      return this;
    },

    rerender: function () {
      this.convertModelToJSON();
    }
  });
}());

