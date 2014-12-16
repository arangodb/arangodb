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

    installFoxxFromGithub: function (url, name, version) {

      var result, options = {
        url: url,
        name: name,
        version: version
      };

      $.ajax({
        cache: false,
        type: "POST",
        async: false, // sequential calls!
        url: "/_admin/aardvark/foxxes/gitinstall",
        data: JSON.stringify(options),
        contentType: "application/json",
        processData: false,
        success: function(data) {
          result = data;
        },
        error: function(data) {
          result = {
            error: true 
          };
        }
      });
      return result;
    },

    mountInfo: function (key) {
      var result;
      $.ajax({
        cache: false,
        type: "GET",
        async: false, // sequential calls!
        url: "/_admin/aardvark/foxxes/mountinfo/"+key,
        contentType: "application/json",
        processData: false,
        success: function(data) {
          result = data;
        },
        error: function(data) {
          result = data;
        }
      });

      return result;
    },

    gitInfo: function (key) {
      var result;
      $.ajax({
        cache: false,
        type: "GET",
        async: false, // sequential calls!
        url: "/_admin/aardvark/foxxes/gitinfo/"+key,
        contentType: "application/json",
        processData: false,
        success: function(data) {
          result = data;
        },
        error: function(data) {
          result = data;
        }
      });

      return result;
    },

    purgeFoxx: function (key) {
      var msg, url = "/_admin/aardvark/foxxes/purge/"+key;
      $.ajax({
        async: false,
        type: "DELETE",
        url: url,
        contentType: "application/json",
        dataType: "json",
        processData: false,
        success: function(data) {
          msg = true;
        },
        error: function(data) {
          msg = false;
        }
      });
      return msg;
    },

    purgeAllFoxxes: function (key) {
      var msg, url = "/_admin/aardvark/foxxes/purgeall/"+key;
      $.ajax({
        async: false,
        type: "DELETE",
        url: url,
        contentType: "application/json",
        dataType: "json",
        processData: false,
        success: function(data) {
          msg = true;
        },
        error: function(data) {
          msg = false;
        }
      });
      return msg;
    }

  });
}());
