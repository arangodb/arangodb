/*jslint indent: 2, nomen: true, maxlen: 100, vars: true, white: true, plusplus: true */
/*global Backbone, templateEngine, $, arangoHelper, window*/

(function() {
  "use strict";
  window.FooterView = Backbone.View.extend({
    el: '.footer',
    system: {},
    isOffline: true,
    firstLogin: true,

    initialize: function () {
      //also server online check
      var self = this;
      this.dbSelectionView = new window.DBSelectionView({
        collection: window.arangoDatabase,
        current: window.currentDB
      });
      window.setInterval(function(){
        self.getVersion();
      }, 15000);
      self.getVersion();
    },

    template: templateEngine.createTemplate("footerView.ejs"),

    getVersion: function () {
      var self = this;

      // always retry this call, because it also checks if the server is online
      $.ajax({
        type: "GET",
        cache: false,
        url: "/_api/version",
        contentType: "application/json",
        processData: false,
        async: true,
        success: function(data) {
          if (self.isOffline === true) {
            self.isOffline = false;
            arangoHelper.removeNotifications();
            if (!self.firstLogin) {
              window.setTimeout(function(){
                arangoHelper.arangoNotification("Server connected");
              }, 1000);
            } else {
              self.firstLogin = false;
            }
            self.system.name = data.server;
            self.system.version = data.version;
            self.render();
          }
        },
        error: function (data) {
          self.isOffline = true;
          arangoHelper.arangoError("Server is offline");
        }
      });

      if (! self.system.hasOwnProperty('database')) {
        $.ajax({
          type: "GET",
          cache: false,
          url: "/_api/database/current",
          contentType: "application/json",
          processData: false,
          async: true,
          success: function(data) {
            var name = data.result.name;
            self.system.database = name;
            window.databaseName = name;

            var timer = window.setInterval(function () {
              var navElement = $('#databaseNavi');

              if (navElement) {
                window.clearTimeout(timer);
                timer = null;

                if (name === '_system') {
                  // show "logs" button
                  $('.logs-menu').css('visibility', 'visible');
                  $('.logs-menu').css('display', 'inline');
                  // show dbs menues
                  $('#databaseNavi').css('display','inline');
                  $('#databaseNaviSelect').css('display','inline');
                }
                else {
                  // hide "logs" button
                  $('.logs-menu').css('visibility', 'hidden');
                  $('.logs-menu').css('display', 'none');
                }
                self.render();
              }
            }, 50);
          }
        });
      }
    },

    renderVersion: function () {
      if (this.system.hasOwnProperty('database') && this.system.hasOwnProperty('name')) {
        $(this.el).html(this.template.render({
          name: this.system.name,
          version: this.system.version,
          database: this.system.database,
          margin: this.resizeMargin
        }));
        this.dbSelectionView.render($("#dbSelect"));
      }
    },

    handleResize: function(newMargin) {
      this.resizeMargin = newMargin;
      this.render();
    },

    handleSelectDatabase: function() {
      this.dbSelectionView.render($("#dbSelect"));
    },

    render: function () {
      if (!this.system.version) {
        this.getVersion();
      }
      $(this.el).html(this.template.render({
        name: this.system.name,
        version: this.system.version,
        database: this.system.database,
        margin: this.resizeMargin
      }));
      this.dbSelectionView.render($("#dbSelect"));
      return this;
    }

  });
}());
