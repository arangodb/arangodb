/* jshint browser: true */
/* jshint unused: false */
/* global window, Backbone, $, arangoHelper */
(function () {
  'use strict';
  window.FoxxCollection = Backbone.Collection.extend({
    model: window.Foxx,

    sortOptions: {
      desc: false
    },

    url: arangoHelper.databaseUrl('/_admin/aardvark/foxxes'),

    comparator: function (item, item2) {
      var a, b;
      if (this.sortOptions.desc === true) {
        a = item.get('mount');
        b = item2.get('mount');
        return a < b ? 1 : a > b ? -1 : 0;
      }
      a = item.get('mount');
      b = item2.get('mount');
      return a > b ? 1 : a < b ? -1 : 0;
    },

    setSortingDesc: function (val) {
      this.sortOptions.desc = val;
    },

    install: function (mode, info, mount, options, callback) {
      var url = arangoHelper.databaseUrl('/_admin/aardvark/foxxes/' + mode + '?mount=' + encodeURIComponent(mount));
      if (options.legacy) {
        url += '&legacy=true';
      }
      if (options.setup === true) {
        url += '&setup=true';
      } else if (options.setup === false) {
        url += '&setup=false';
      }
      if (options.teardown === true) {
        url += '&teardown=true';
      } else if (options.teardown === false) {
        url += '&teardown=false';
      }
      if (options.replace === true) {
        url += '&replace=true';
      } else if (options.replace === false) {
        url += '&upgrade=true';
      }
      $.ajax({
        cache: false,
        type: 'PUT',
        url: url,
        data: JSON.stringify(info),
        contentType: 'application/json',
        processData: false,
        success: function (data) {
          callback(data);
        },
        error: function (err) {
          callback(err);
        }
      });
    },

  });
}());
