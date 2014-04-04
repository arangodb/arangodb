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
      "click #arangoLogTabbar button" : "setActiveLoglevel",
      "click #logTable_first" : "firstPage",
      "click #logTable_last" : "lastPage"
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
      titles: ["Loglevel", "Date", "Message"],
      rows: []
    },

    convertedRows: null,

    setActiveLoglevel: function(e) {
     $('.arangodb-tabbar').removeClass('arango-active-tab');

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
            rowsArray.push([
              self.convertLogStatus(model.get('level')),
              arangoHelper.formatDT(date),
              model.get('text')]);
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
      $('#'+this.currentLoglevel).addClass('arango-active-tab');
      this.renderPagination();
      return this;
    },

    jumpTo: function(page) {
      var self = this;
      this.activeCollection.setPage(page);
      this.activeCollection.fetch({
        success: function() {
          self.convertModelToJSON();
        }
      });
    },

    firstPage: function() {
      this.jumpTo(1);
    },

    lastPage: function() {
      this.jumpTo(Math.ceil(this.activeCollection.totalAmount/this.activeCollection.pagesize));
    },

    renderPagination: function () {
      var self = this;
      var currentPage = this.activeCollection.getPage();
      var totalPages = Math.ceil(this.activeCollection.totalAmount/this.activeCollection.pagesize);
      var target = $('#logPaginationDiv'),
      options = {
        page: currentPage,
        lastPage: totalPages,
        click: function(i) {
          if (i === 1 && i !== currentPage) {
            self.activeCollection.setPage(1);
          }
          else if (i === totalPages && i !== currentPage) {
            self.activeCollection.setPage(totalPages);
          }
          else if (i !== currentPage) {
            self.activeCollection.setPage(i);
          }
          options.page = i;
          self.activeCollection.fetch({
            success: function() {
              self.convertModelToJSON();
            }
          });
        }
      };
      target.html("");
      target.pagination(options);
      $('#logPaginationDiv').prepend(
        '<ul class="pre-pagi"><li><a id="logTable_first" class="pagination-button">'+
        '<span><i class="fa fa-angle-double-left"/></span></a></li></ul>'
      );
      $('#logPaginationDiv').append(
        '<ul class="las-pagi"><li><a id="logTable_last" class="pagination-button">'+
        '<span><i class="fa fa-angle-double-right"/></span></a></li></ul>'
      );
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
