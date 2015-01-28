/*jshint browser: true */
/*jshint unused: false */
/*global window, Backbone, alert, $ */
(function() {
  "use strict";
  window.FoxxCollection = Backbone.Collection.extend({
    model: window.Foxx,

    sortOptions: {
      desc: false
    },

    url: "/_admin/aardvark/foxxes",

    comparator: function(item, item2) {
      var a, b;
      if (this.sortOptions.desc === true) {
        a = item.get('name');
        b = item2.get('name');
        return a < b ? 1 : a > b ? -1 : 0;
      }
      a = item.get('name');
      b = item2.get('name');
      return a > b ? 1 : a < b ? -1 : 0;
    },

    setSortingDesc: function(val) {
      this.sortOptions.desc = val;
    },

    // Install Foxx from github repo
    // info is expected to contain: "url" and "version"
    installFromGithub: function (info, mount, callback) {
      $.ajax({
        cache: false,
        type: "PUT",
        url: "/_admin/aardvark/foxxes/" + mount + "/git",
        data: JSON.stringify(info),
        contentType: "application/json",
        processData: false,
        success: function(data) {
          callback(data);
        },
        error: function(err) {
          callback(err);
        }
      });
    },

    installFromZip: function(files, data, callback) {
      var self = this;
      $.ajax({
        type: "POST",
        async: false,
        url: "/_admin/aardvark/foxxes/inspect",
        data: JSON.stringify(data),
        contentType: "application/json"
      }).done(function(res) {
        $.ajax({
          type: "POST",
          async: false,
          url: '/_admin/foxx/fetch',
          data: JSON.stringify({
            name: res.name,
            version: res.version,
            filename: res.filename
          }),
          processData: false
        }).done(function (result) {
          callback(result);
        }).fail(function (err) {
          callback(err);
        });
      }).fail(function(err) {
        callback(err);
      });
      self.hideImportModal();
    }

  });
}());
