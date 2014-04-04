/*jslint indent: 2, nomen: true, maxlen: 100, vars: true, white: true, plusplus: true */
/*global require, exports, Backbone, EJS, $, window, templateEngine*/

(function() {
  "use strict";
  window.NewLogsView = Backbone.View.extend({
    el: '#content',
    id: '#logContent',

    initialize: function () {
      this.convertModelToJSON();
    },

    currentLoglevel: "logall",

    events: {
      "click #arangoLogTabbar button" : "setActiveLoglevel"
    },

    template: templateEngine.createTemplate("newLogsView.ejs"),
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
      cols: 3,
      titles: ["Loglevel", "Date", "Message"],
      rows: [
        ["hallo", "welt", "2"],
        ["miasd", "asdst", "12"]
      ]
    },

    convertedRows: null,

    setActiveLoglevel: function(e) {
     $('.arangodb-tabbar').removeClass('arangoActiveTab');

      if (this.currentLoglevel !== e.currentTarget.id) {
        this.currentLoglevel = e.currentTarget.id;
        this.convertModelToJSON();
      }
    },

    convertModelToJSON: function() {
      var self = this;
      var date;
      var rowsArray = [];
      this.activeCollection = this.options[this.currentLoglevel];
      this.activeCollection.fetch({
        success: function() {
          self.activeCollection.each(function(model) {
            date = new Date(model.get('timestamp') * 1000);
            rowsArray.push([model.get('level'), arangoHelper.formatDT(date), model.get('text')]);
          });
          self.tableDescription.rows = rowsArray;
          self.render();
        }
      });
    },

    render: function() {
      $(this.el).html(this.template.render({}));
      $(this.id).html(this.tabbar.render({content: this.tabbarElements}));
      $(this.id).append(this.table.render({content: this.tableDescription}));
      $('#'+this.currentLoglevel).addClass('arangoActiveTab');
      return this;
    },

    renderPagination: function (totalPages, currentPage) {

      var self = this;
      var target = $('#logPaginationDiv'),
      options = {
//        left: 2,
//        right: 2,
        page: currentPage,
        lastPage: totalPages,
        click: function(i) {
          var doSomething = false;
          if (i === 1 && i !== currentPage) {
            self.firstTable();
          }
          else if (i === totalPages && i !== currentPage) {
            self.lastTable();
          }
          else if (i !== currentPage) {
            self.jumpToTable(i);
          }
          options.page = i;
        }
      };
      target.html("");
      target.pagination(options);
      $('#logPaginationDiv').prepend(
        '<ul class="pre-pagi"><li><a id="logTableID_first" class="pagination-button">'+
        '<span><i class="fa fa-angle-double-left"/></span></a></li></ul>'
      );
      $('#logPaginationDiv').append(
        '<ul class="las-pagi"><li><a id="logTableID_last" class="pagination-button">'+
        '<span><i class="fa fa-angle-double-right"/></span></a></li></ul>'
      );
    },

    drawTable: function () {
    },
    clearTable: function () {
    },
    convertLogStatus: function (status) {
      var returnString;
      if (status === 1) {
        returnString = "Error";
      }
      else if (status === 2) {
        returnString = "Warning";
      }
      else if (status === 3) {
        returnString =  "Info";
      }
      else if (status === 4) {
        returnString = "Debug";
      }
      else {
        returnString = "Unknown";
      }
      return returnString;
    }
  });
}());
