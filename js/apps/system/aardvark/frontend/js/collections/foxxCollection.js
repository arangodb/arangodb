/*jslint indent: 2, nomen: true, maxlen: 100, white: true, plusplus: true */
/*global window, Backbone, $ */
(function() {
  "use strict";
  window.FoxxCollection = Backbone.Collection.extend({
    model: window.Foxx,

    url: "/_admin/aardvark/foxxes",

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
          result = true;
        },
        error: function(data) {
          result = false;
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
    }

  });
}());
